package com.batchloader.app;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

/**
 * Listens to RS-485 traffic on the tablet's built-in serial port.
 * Parses Modbus FC04 responses (the slave's reply to Teensy polls).
 */
public class ModbusListener extends Thread {

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

    // Known built-in RS-485 ports on various tablets
    private static final String[] KNOWN_PORTS = {
        "/dev/ttyS1", "/dev/ttyS2", "/dev/ttyS3", "/dev/ttyS4",
        "/dev/ttyMT1", "/dev/ttyMT2",
        "/dev/ttyHSL0", "/dev/ttyHSL1",
        "/dev/ttyUSB0", "/dev/ttyUSB1"
    };

    // Buffers for reading Modbus frames from each slave (1-4)
    private byte[][] frameBuffers = new byte[5][32];
    private int[] framePositions = new int[5];
    private long[] lastByteTimes = new long[5];

    public ModbusListener(Listener listener) {
        this.listener = listener;
        // Find the RS-485 port
        this.serialPort = findSerialPort();
    }

    private String findSerialPort() {
        for (String port : KNOWN_PORTS) {
            File f = new File(port);
            if (f.exists() && f.canRead()) {
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

    @Override
    public void run() {
        if (serialPort == null) {
            if (listener != null) listener.onError("No RS-485 port found");
            return;
        }

        running = true;
        if (listener != null) listener.onStatus("Opening " + serialPort + " @ 9600 baud...");

        // Clear buffers
        for (int i = 0; i < 5; i++) {
            framePositions[i] = 0;
            lastByteTimes[i] = 0;
        }

        // Open the serial port
        try (FileInputStream serialIn = new FileInputStream(serialPort)) {
            // Configure port to 9600 8N1 via shell (requires root on some tablets)
            try {
                Runtime.getRuntime().exec(new String[]{
                    "stty", "-F", serialPort, "9600", "cs8", "-cstopb", "-parenb"
                }).waitFor();
            } catch (Exception ignored) {}

            if (listener != null) listener.onStatus("Listening on RS-485...");

            byte[] buffer = new byte[256];
            int pos = 0;

            while (running) {
                int bytesRead = serialIn.read(buffer, 0, buffer.length);
                if (bytesRead > 0) {
                    for (int i = 0; i < bytesRead; i++) {
                        processByte(buffer[i] & 0xFF);
                    }
                }

                // Check for stale frames (nothing received for 100ms → discard partial)
                long now = System.currentTimeMillis();
                for (int a = 1; a <= 4; a++) {
                    if (framePositions[a] > 0 && (now - lastByteTimes[a]) > 200) {
                        framePositions[a] = 0;
                    }
                }
            }

        } catch (IOException e) {
            if (listener != null) listener.onError("RS-485 error: " + e.getMessage());
        }
    }

    private void processByte(int b) {
        // Determine which slave address this could belong to.
        // Modbus FC04 frames:
        //   Request:  [addr][0x04][start_hi][start_lo][qty_hi][qty_lo][CRC_lo][CRC_hi]
        //   Response: [addr][0x04][byteCount][data...][CRC_lo][CRC_hi]
        //
        // The Teensy polls addresses 1-4. The response starts with the slave address.
        // Since frames come sequentially (poll addr 1, response, poll addr 2, response...),
        // we can use the address byte to identify which slave.

        // Check if this looks like a start of a Modbus response frame
        // (byte 1 should be 0x04 for FC04 response)
        if (framePositions[0] == 0) {
            // First byte - is it a valid slave address?
            if (b >= 1 && b <= 4) {
                framePositions[0] = b; // Store address temporarily
                return;
            }
            return;
        }

        int addr = framePositions[0];
        
        if (framePositions[addr] == 0) {
            // Second byte - should be 0x04 for FC04 response
            if (b == 0x04) {
                framePositions[addr] = 1;
                frameBuffers[addr][0] = (byte)addr;
                frameBuffers[addr][1] = (byte)b;
            } else {
                framePositions[0] = 0; // Not a valid FC04 response
            }
            return;
        }

        // Collect remaining bytes
        int pos = framePositions[addr];
        if (pos < 32) {
            frameBuffers[addr][pos] = (byte)b;
            framePositions[addr]++;
            lastByteTimes[addr] = System.currentTimeMillis();

            // Minimum FC04 response: addr(1) + FC04(1) + byteCount(1) + data(8) + CRC(2) = 13 bytes
            if (pos >= 12) { // pos is 0-indexed, so pos=12 means we have 13 bytes
                int totalLen = framePositions[addr];
                if (totalLen >= 13) {
                    int byteCount = frameBuffers[addr][2] & 0xFF;
                    int expectedLen = 3 + byteCount + 2; // header + data + CRC

                    if (totalLen == expectedLen) {
                        // Validate CRC
                        if (validateCRC(frameBuffers[addr], totalLen)) {
                            parseResponse(addr);
                        }
                        framePositions[addr] = 0;
                        framePositions[0] = 0;
                    } else if (totalLen > expectedLen) {
                        // Overshoot - discard
                        framePositions[addr] = 0;
                        framePositions[0] = 0;
                    }
                }
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
        byte[] frame = frameBuffers[addr];
        int byteCount = frame[2] & 0xFF;
        
        if (byteCount < 8) return; // Need at least 4 registers

        int regStatus    = (frame[3] & 0xFF) << 8 | (frame[4] & 0xFF);
        int regFlowrate  = (frame[5] & 0xFF) << 8 | (frame[6] & 0xFF);
        int regDispensed = (frame[7] & 0xFF) << 8 | (frame[8] & 0xFF);
        int regTarget    = (frame[9] & 0xFF) << 8 | (frame[10] & 0xFF);

        BoardData data = new BoardData(addr);
        data.online = (regStatus >= 0 && regStatus <= 3);
        data.dispensing = (regStatus == 2);
        data.targetLiters = regTarget / 10.0f;
        data.dispensedLiters = regDispensed / 10.0f;
        data.pulseCount = regFlowrate;

        if (listener != null) {
            listener.onBoardUpdated(addr, data);
        }
    }

    public void shutdown() {
        running = false;
        interrupt();
    }
}
