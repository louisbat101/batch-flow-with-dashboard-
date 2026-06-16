package com.batchloader.app;

import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * Serial Port Scanner – can scan RS-485 ports at various baud rates
 * and report any data received (from C3 or slave boards).
 */
public class SerialPortScanner {

    private static final String TAG = "SerialScanner";

    private static final String[] KNOWN_PORTS = {
        "/dev/ttysWK0",  // Built-in RS-485 (test first)
        "/dev/ttyUSB0",  // USB-RS485 dongle
        "/dev/ttyS3",
        "/dev/ttysWK1", "/dev/ttysWK2", "/dev/ttysWK3",
        "/dev/ttyS1", "/dev/ttyS7", "/dev/ttyS8", "/dev/ttyS9",
        "/dev/ttyUSB1",
        "/dev/ttyACM0", "/dev/ttyACM1"
    };

    public static final int[] BAUD_RATES = {9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200};

    /**
     * Quick scan at 9600.
     */
    public static String scan() {
        return scanWithBaud(9600, false);
    }

    /**
     * Scan all known ports at a specific baud rate, optionally reading for data.
     */
    public static String scanWithBaud(int baud, boolean readData) {
        StringBuilder result = new StringBuilder();
        result.append("{\"ports\":[");

        boolean first = true;
        for (String portPath : KNOWN_PORTS) {
            File f = new File(portPath);
            if (f.exists()) {
                if (!first) result.append(",");
                first = false;

                result.append("{\"path\":\"").append(escapeJson(portPath)).append("\"");
                result.append(",\"readable\":").append(f.canRead());
                result.append(",\"writable\":").append(f.canWrite());

                String testResult = tryOpen(portPath, baud, readData);
                result.append(",\"test\":\"").append(escapeJson(testResult)).append("\"");

                result.append("}");
            }
        }

        result.append("]}");
        return result.toString();
    }

    private static String tryOpen(String portPath, int baud, boolean readData) {
        FileInputStream fis = null;
        FileOutputStream fos = null;

        try {
            fis = new FileInputStream(portPath);
            fos = new FileOutputStream(portPath);

            Process p = Runtime.getRuntime().exec(new String[]{
                "stty", "-F", portPath,
                String.valueOf(baud),
                "cs8", "-cstopb", "-parenb", "raw", "-echo"
            });
            int exit = p.waitFor();
            if (exit != 0) return "stty exit=" + exit;

            if (readData) {
                // Send FC04 poll to address 1
                byte[] poll = buildFC04(1);
                fos.write(poll);
                fos.flush();

                long start = System.currentTimeMillis();
                List<Byte> received = new ArrayList<>();
                while (System.currentTimeMillis() - start < 500) {
                    if (fis.available() > 0) {
                        byte[] buf = new byte[32];
                        int n = fis.read(buf);
                        for (int i = 0; i < n; i++) received.add(buf[i]);
                    } else {
                        try { Thread.sleep(10); } catch (InterruptedException e) { break; }
                    }
                }

                if (received.isEmpty()) return "no data @ " + baud;

                StringBuilder hex = new StringBuilder();
                hex.append(received.size()).append(" bytes: ");
                for (byte b : received) hex.append(String.format("%02X ", b & 0xFF));
                return hex.toString().trim();
            } else {
                return "ok@" + baud;
            }
        } catch (Exception e) {
            return "err";
        } finally {
            try { if (fis != null) fis.close(); } catch (Exception ignored) {}
            try { if (fos != null) fos.close(); } catch (Exception ignored) {}
        }
    }

    /**
     * Full scan: try each baud rate on each port and poll addresses 1-10.
     */
    public static String fullBaudScan() {
        StringBuilder result = new StringBuilder();
        result.append("{\"scan\":[");

        boolean firstPort = true;
        for (String portPath : KNOWN_PORTS) {
            File f = new File(portPath);
            if (!f.exists()) continue;

            if (!firstPort) result.append(",");
            firstPort = false;

            result.append("{\"port\":\"").append(escapeJson(portPath)).append("\",\"rates\":[");

            boolean firstRate = true;
            for (int baud : BAUD_RATES) {
                if (!firstRate) result.append(",");
                firstRate = false;

                result.append("{\"baud\":").append(baud).append(",\"addrs\":[");

                boolean firstAddr = true;
                for (int addr = 1; addr <= 10; addr++) {
                    String response = tryPollAddress(portPath, baud, addr);
                    if (!response.isEmpty()) {
                        if (!firstAddr) result.append(",");
                        firstAddr = false;
                        result.append("{\"a\":").append(addr).append(",\"d\":\"");
                        result.append(escapeJson(response)).append("\"}");
                    }
                }

                result.append("]}");
            }
            result.append("]}");
        }

        result.append("]}");
        return result.toString();
    }

    private static String tryPollAddress(String portPath, int baud, int addr) {
        FileInputStream fis = null;
        FileOutputStream fos = null;

        try {
            fis = new FileInputStream(portPath);
            fos = new FileOutputStream(portPath);

            Process p = Runtime.getRuntime().exec(new String[]{
                "stty", "-F", portPath,
                String.valueOf(baud),
                "cs8", "-cstopb", "-parenb", "raw", "-echo"
            });
            p.waitFor();

            byte[] poll = buildFC04(addr);
            fos.write(poll);
            fos.flush();

            long start = System.currentTimeMillis();
            List<Byte> received = new ArrayList<>();
            while (System.currentTimeMillis() - start < 300) {
                if (fis.available() > 0) {
                    byte[] buf = new byte[64];
                    int n = fis.read(buf);
                    for (int i = 0; i < n; i++) received.add(buf[i]);
                    if (received.size() > 0) break;
                } else {
                    try { Thread.sleep(5); } catch (InterruptedException e) { break; }
                }
            }

            if (received.isEmpty()) return "";

            StringBuilder hex = new StringBuilder();
            for (int i = 0; i < received.size(); i++) {
                hex.append(String.format("%02X", received.get(i)));
                if (i < received.size() - 1) hex.append(" ");
            }

            if (received.size() >= 5) {
                int rxAddr = received.get(0) & 0xFF;
                int rxFunc = received.get(1) & 0xFF;
                if (rxAddr == addr && rxFunc == 0x04) hex.append(" OK");
                else if (rxAddr == addr && (rxFunc & 0x80) != 0) hex.append(" EX");
            }

            return hex.toString();
        } catch (Exception e) {
            return "";
        } finally {
            try { if (fis != null) fis.close(); } catch (Exception ignored) {}
            try { if (fos != null) fos.close(); } catch (Exception ignored) {}
        }
    }

    // ── Listen mode ──────────────────────────────────────────────

    private static volatile boolean listening = false;

    /**
     * Start listening on a port at a baud rate – outputs raw hex to Logcat.
     */
    public static void startListening(final String portPath, final int baud) {
        if (listening) return;
        listening = true;

        new Thread(() -> {
            FileInputStream fis = null;
            try {
                // Open and configure
                FileOutputStream fos = new FileOutputStream(portPath);
                fis = new FileInputStream(portPath);

                Process p = Runtime.getRuntime().exec(new String[]{
                    "stty", "-F", portPath,
                    String.valueOf(baud),
                    "cs8", "-cstopb", "-parenb", "raw", "-echo"
                });
                p.waitFor();
                fos.close();

                byte[] buf = new byte[1024];
                int totalBytes = 0;
                long startTime = System.currentTimeMillis();

                while (listening) {
                    if (fis.available() > 0) {
                        int n = fis.read(buf);
                        if (n > 0) {
                            totalBytes += n;
                            StringBuilder hex = new StringBuilder();
                            for (int i = 0; i < n; i++) {
                                hex.append(String.format("%02X ", buf[i] & 0xFF));
                            }
                            Log.d(TAG, "[" + portPath + "@" + baud + "] " + hex.toString().trim());
                        }
                    } else {
                        // Every second, show a heartbeat if no data
                        long elapsed = System.currentTimeMillis() - startTime;
                        if (elapsed > 1000 && totalBytes == 0) {
                            Log.d(TAG, "[" + portPath + "@" + baud + "] listening... (no data yet)");
                            startTime = System.currentTimeMillis();
                        }
                        try { Thread.sleep(100); } catch (InterruptedException e) { break; }
                    }
                }
                Log.d(TAG, "[" + portPath + "@" + baud + "] stopped (total " + totalBytes + " bytes)");
            } catch (Exception e) {
                Log.e(TAG, "Listen error on " + portPath + "@" + baud + ": " + e.getMessage());
            } finally {
                try { if (fis != null) fis.close(); } catch (Exception ignored) {}
            }
        }).start();
    }

    public static void stopListening() {
        listening = false;
    }

    public static boolean isListening() {
        return listening;
    }

    // ── Utilities ────────────────────────────────────────────────

    private static byte[] buildFC04(int addr) {
        byte[] cmd = new byte[8];
        cmd[0] = (byte) addr;
        cmd[1] = 0x04;
        cmd[2] = 0x00;
        cmd[3] = 0x00;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        int crc = calcCRC16(cmd, 6);
        cmd[6] = (byte) (crc & 0xFF);
        cmd[7] = (byte) ((crc >> 8) & 0xFF);
        return cmd;
    }

    private static int calcCRC16(byte[] data, int len) {
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

    private static String escapeJson(String s) {
        if (s == null) return "";
        return s.replace("\\", "\\\\")
                .replace("\"", "\\\"")
                .replace("\n", "\\n")
                .replace("\r", "\\r")
                .replace("\t", "\\t");
    }
}

