package com.batchloader.app;

import android.util.Log;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.reflect.Field;
import java.util.Arrays;

/**
 * Modbus RTU Master on the tablet's built-in RS-485 port.
 * Actively polls station boards (slaves 1-4) for FC04 status,
 * and sends FC05 commands to open/close valves.
 */
public class ModbusListener extends Thread {

    private static final String TAG = "ModbusMaster";
    
    // Native method to enable RS-485 half-duplex mode on a serial port
    private static native int enableRs485(String port);
    
    static {
        try {
            System.loadLibrary("rs485helper");
            Log.d(TAG, "Native rs485helper library loaded");
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "rs485helper native library not available: " + e.getMessage());
        }
    }

    public static class BoardData {
        public int address;
        public boolean online;
        public boolean dispensing;
        public float targetLiters;
        public float dispensedLiters;
        public int pulseCount;

        BoardData(int addr) {
            this.address = addr;
            this.online = false;
            this.dispensing = false;
            this.targetLiters = 0;
            this.dispensedLiters = 0;
            this.pulseCount = 0;
        }
    }

    public interface Listener {
        void onBoardUpdated(int address, BoardData data);
        void onStatus(String msg);
        void onError(String msg);
    }

    private volatile boolean running = false;
    private Listener listener;
    private String serialPort;
    private FileOutputStream serialOut;
    private FileInputStream serialIn;


        // Known RS-485 ports in priority order
    // USB-RS485 dongle (CH340) — confirmed working
    // Native Rockchip UARTs (ttyS*) need TIOCSRS485 ioctl which Android blocks
    private static final String[] KNOWN_PORTS = {
        "/dev/ttyUSB0",   // USB-RS485 dongle — confirmed working
        "/dev/ttyUSB1",
        "/dev/ttyS7",     // Native rk3568 UART with RS-485 support (ioctl blocked)
        "/dev/ttyS3",
        "/dev/ttyS1", "/dev/ttyS4", "/dev/ttyS8",
        "/dev/ttysWK0",   // WK2xxx SPI-UART
        "/dev/ttysWK1", "/dev/ttysWK2", "/dev/ttysWK3",
        "/dev/ttyS9",
        "/dev/ttyMT1", "/dev/ttyMT2",
        "/dev/ttyHSL0", "/dev/ttyHSL1",
        "/dev/ttyACM0", "/dev/ttyACM1"
    };

    // Poll timing
    private static final int POLL_INTERVAL_MS = 350;   // ms between polls
    private static final int RESPONSE_TIMEOUT_MS = 200; // wait for response

    // Response buffer
    private byte[] responseBuffer = new byte[32];
    private int responsePos = 0;
    private long responseStartTime = 0;
    private int expectedAddr = 0;
    private boolean waitingForResponse = false;
    
    // Last known board data (for reporting when boards go offline)
    private BoardData[] lastData = new BoardData[5];
    private long[] lastResponseTimes = new long[5];
    private static final long OFFLINE_TIMEOUT_MS = 3000; // 3s no response = offline

    // ── Transmit / Receive buffer for poll/write commands ──────
    private final Object txLock = new Object();

    public ModbusListener(Listener listener) {
        this.listener = listener;
        for (int i = 1; i <= 4; i++) {
            lastData[i] = new BoardData(i);
            lastResponseTimes[i] = 0;
        }
        this.serialPort = findSerialPort();
    }

    private String findSerialPort() {
        for (String port : KNOWN_PORTS) {
            File f = new File(port);
            if (f.exists()) {
                return port;
            }
        }
        return null;
    }

    public String getPortName() {
        return serialPort != null ? serialPort : "none";
    }

    public boolean hasPort() {
        return serialPort != null;
    }

    // ── Public API to send Modbus commands ─────────────────────
    
    /**
     * Send FC05 to open or close a valve on a specific station board.
     * @param address  slave address (1-4)
     * @param open     true = open valve, false = close valve
     */
    public void setValve(int address, boolean open) {
        synchronized (txLock) {
            if (serialOut == null) return;
            
            // FC05: [addr][0x05][coil_hi][coil_lo][value_hi][value_lo][CRC_lo][CRC_hi]
            byte[] cmd = new byte[8];
            cmd[0] = (byte) address;
            cmd[1] = 0x05; // function code: write single coil
            cmd[2] = 0x00; // coil address high (coil 0 = valve)
            cmd[3] = 0x00; // coil address low
            cmd[4] = (byte) (open ? 0xFF : 0x00); // 0xFF00 = ON, 0x0000 = OFF
            cmd[5] = 0x00;
            
            int crc = calcCRC16(cmd, 6);
            cmd[6] = (byte) (crc & 0xFF);
            cmd[7] = (byte) ((crc >> 8) & 0xFF);
            
            try {
                serialOut.write(cmd);
                serialOut.flush();
                if (listener != null) {
                    listener.onStatus("FC05 valve " + (open ? "OPEN" : "CLOSE") + " → addr " + address);
                }
            } catch (IOException e) {
                if (listener != null) listener.onError("FC05 write error: " + e.getMessage());
            }
        }
    }

    @Override
    public void run() {
        if (serialPort == null) {
            Log.e(TAG, "No serial port found");
            if (listener != null) listener.onError("No RS-485 port found");
            return;
        }

        running = true;
        Log.d(TAG, "Opening " + serialPort + " @ 9600 8N1");
        if (listener != null) listener.onStatus("Opening " + serialPort + " @ 9600 baud...");

        try {
            // Open both streams for read/write
            serialIn = new FileInputStream(serialPort);
            serialOut = new FileOutputStream(serialPort);
            Log.d(TAG, "Port opened successfully");

            // Try to enable RS-485 half-duplex mode via native helper
            try {
                int result = enableRs485(serialPort);
                if (result == 0) {
                    Log.d(TAG, "RS-485 half-duplex mode ENABLED on " + serialPort);
                } else {
                    Log.w(TAG, "RS-485 ioctl failed with result " + result + " on " + serialPort + 
                          " (may not support half-duplex)");
                }
            } catch (Exception e) {
                Log.w(TAG, "RS-485 enable failed (will try without half-duplex): " + e.getMessage());
            }

            // Configure port to 9600 8N1 via shell
            try {
                String[] cmd = {"stty", "-F", serialPort, "9600", "cs8", "-cstopb", "-parenb", "raw", "-echo"};
                Log.d(TAG, "Running: " + String.join(" ", cmd));
                Process p = Runtime.getRuntime().exec(cmd);
                int exitCode = p.waitFor();
                Log.d(TAG, "stty exit code: " + exitCode);
                // Read stderr to see if there were errors
                java.io.InputStream err = p.getErrorStream();
                java.util.Scanner s = new java.util.Scanner(err).useDelimiter("\\A");
                String stderr = s.hasNext() ? s.next() : "";
                if (!stderr.isEmpty()) Log.w(TAG, "stty stderr: " + stderr);
            } catch (Exception e) {
                Log.w(TAG, "stty failed: " + e.getMessage());
            }

            Log.d(TAG, "Modbus Master active on RS-485");
            if (listener != null) listener.onStatus("Modbus Master active on RS-485");

            int pollAddr = 1;
            int pollCount = 0;
            
            while (running) {
                // Send FC04 poll request to current address
                Log.v(TAG, "Polling addr " + pollAddr + " (poll #" + pollCount + ")");
                sendFC04Poll(pollAddr);
                
                // Wait for and read response
                if (waitForResponse(pollAddr)) {
                    Log.d(TAG, "Got response from addr " + pollAddr);
                } else {
                    Log.v(TAG, "No response from addr " + pollAddr + " (timeout)");
                    markOffline(pollAddr);
                }
                
                // Move to next board
                pollAddr = (pollAddr % 4) + 1;
                pollCount++;
                
                // Check all boards for stale status
                checkStaleBoards();
            }

        } catch (IOException e) {
            Log.e(TAG, "RS-485 IO error: " + e.getMessage());
            e.printStackTrace();
            if (listener != null) listener.onError("RS-485 error: " + e.getMessage());
        } finally {
            try { if (serialIn != null) serialIn.close(); } catch (Exception ignored) {}
            try { if (serialOut != null) serialOut.close(); } catch (Exception ignored) {}
            Log.d(TAG, "Modbus thread ended");
        }
    }

    private void sendFC04Poll(int addr) {
        synchronized (txLock) {
            if (serialOut == null) return;
            
            // FC04 read input registers: [addr][0x04][start_hi][start_lo][qty_hi][qty_lo][CRC_lo][CRC_hi]
            // Read 4 registers starting at 0 (status, flowrate, dispensed, target)
            byte[] cmd = new byte[8];
            cmd[0] = (byte) addr;
            cmd[1] = 0x04;
            cmd[2] = 0x00; // start register high
            cmd[3] = 0x00; // start register low
            cmd[4] = 0x00; // quantity high (4 registers)
            cmd[5] = 0x04; // quantity low
            
            int crc = calcCRC16(cmd, 6);
            cmd[6] = (byte) (crc & 0xFF);
            cmd[7] = (byte) ((crc >> 8) & 0xFF);
            
            try {
                serialOut.write(cmd);
                serialOut.flush();
            } catch (IOException e) {
                if (listener != null) listener.onError("FC04 write error: " + e.getMessage());
            }
        }
        
        // Prepare to receive response
        responsePos = 0;
        expectedAddr = addr;
        waitingForResponse = true;
        responseStartTime = System.currentTimeMillis();
    }

    private boolean waitForResponse(int addr) {
        // Read bytes for up to RESPONSE_TIMEOUT_MS
        while (running && waitingForResponse) {
            long elapsed = System.currentTimeMillis() - responseStartTime;
            if (elapsed > RESPONSE_TIMEOUT_MS) {
                waitingForResponse = false;
                return false;
            }
            
            try {
                int remaining = (int)(RESPONSE_TIMEOUT_MS - elapsed);
                if (remaining <= 0) break;
                
                if (serialIn.available() > 0) {
                    byte[] buf = new byte[32];
                    int read = serialIn.read(buf, 0, Math.min(buf.length, remaining > 0 ? 32 : 0));
                    
                    if (read > 0) {
                        for (int i = 0; i < read; i++) {
                            processRxByte(buf[i] & 0xFF);
                        }
                        // If we completed parsing, done waiting
                        if (!waitingForResponse) return true;
                    }
                } else {
                    // Brief sleep to avoid busy-waiting
                    try { Thread.sleep(5); } catch (InterruptedException ie) { break; }
                }
            } catch (IOException e) {
                waitingForResponse = false;
                return false;
            }
        }
        waitingForResponse = false;
        return false;
    }

    private void processRxByte(int b) {
        if (!waitingForResponse) return;
        
        if (responsePos == 0) {
            // First byte: slave address – must match expected
            if (b == expectedAddr) {
                responseBuffer[responsePos++] = (byte) b;
            } else {
                // Not for us, discard
                responsePos = 0;
            }
            return;
        }
        
        if (responsePos == 1) {
            // Second byte: function code – must be 0x04
            if (b == 0x04) {
                responseBuffer[responsePos++] = (byte) b;
            } else {
                responsePos = 0; // Wrong function code
                waitingForResponse = false; // Stop waiting
            }
            return;
        }

        // Collect remaining bytes
        if (responsePos < responseBuffer.length) {
            responseBuffer[responsePos++] = (byte) b;
            
            // Check if we have enough bytes to determine expected length
            if (responsePos >= 3) {
                int byteCount = responseBuffer[2] & 0xFF;
                int expectedLen = 3 + byteCount + 2; // addr(1) + FC(1) + byteCount(1) + data(N) + CRC(2)
                
                if (responsePos == expectedLen) {
                    waitingForResponse = false;
                    // Validate CRC and parse
                    if (validateCRC(responseBuffer, responsePos)) {
                        parseResponse(expectedAddr);
                    }
                } else if (responsePos > expectedLen) {
                    waitingForResponse = false; // Overshoot – discard
                }
            }
        } else {
            waitingForResponse = false; // Buffer full – discard
        }
    }

    private void markOffline(int addr) {
        if (addr >= 1 && addr <= 4) {
            BoardData data = new BoardData(addr);
            data.online = false;
            if (listener != null) {
                listener.onBoardUpdated(addr, data);
            }
        }
    }

    private void checkStaleBoards() {
        long now = System.currentTimeMillis();
        for (int a = 1; a <= 4; a++) {
            if (lastResponseTimes[a] > 0 && (now - lastResponseTimes[a]) > OFFLINE_TIMEOUT_MS) {
                lastResponseTimes[a] = 0;
                markOffline(a);
            }
        }
    }

    private boolean validateCRC(byte[] frame, int len) {
        if (len < 3) return false;
        int crcPos = len - 2;
        int rxCRC = (frame[crcPos + 1] & 0xFF) << 8 | (frame[crcPos] & 0xFF);
        int calcCRC = calcCRC16(frame, crcPos);
        return rxCRC == calcCRC;
    }

    private int calcCRC16(byte[] data, int len) {
        int crc = 0xFFFF;
        for (int i = 0; i < len; i++) {
            crc ^= (data[i] & 0xFF);
            for (int j = 0; j < 8; j++) {
                if ((crc & 1) != 0) {
                    crc = (crc >> 1) ^ 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }

    private void parseResponse(int addr) {
        byte[] frame = responseBuffer;
        int byteCount = frame[2] & 0xFF;
        
        if (byteCount < 8) return; // Need at least 4 registers

        int regStatus    = (frame[3] & 0xFF) << 8 | (frame[4] & 0xFF);
        int regFlowrate  = (frame[5] & 0xFF) << 8 | (frame[6] & 0xFF);
        int regDispensed = (frame[7] & 0xFF) << 8 | (frame[8] & 0xFF);
        int regTarget    = (frame[9] & 0xFF) << 8 | (frame[10] & 0xFF);

        BoardData data = new BoardData(addr);
        data.online = true;
        data.dispensing = (regStatus == 2);
        data.targetLiters = regTarget / 10.0f;
        data.dispensedLiters = regDispensed / 10.0f;
        data.pulseCount = regFlowrate;

        lastData[addr] = data;
        lastResponseTimes[addr] = System.currentTimeMillis();

        if (listener != null) {
            listener.onBoardUpdated(addr, data);
        }
    }

    /**
     * Get a copy of the last known data for a board.
     */
    public BoardData getLastData(int address) {
        if (address >= 1 && address <= 4) {
            return lastData[address];
        }
        return null;
    }

    public void shutdown() {
        running = false;
        interrupt();
    }
}
