#!/usr/bin/env python3
"""Update DashboardServer.java with valve timing, persistence, and batch history"""

import re

with open('app/src/main/java/com/batchloader/app/DashboardServer.java', 'r') as f:
    content = f.read()

def replace_once(old, new, label):
    """Replace first occurrence, assert it works"""
    if old not in content:
        print(f"  MISSING: {label}")
        return False
    count = content.count(old)
    if count > 1:
        print(f"  WARNING: {label} - found {count} occurrences, using first")
    content = content.replace(old, new, 1)
    print(f"  OK: {label}")
    return True

# ── 1. Add imports ──
replace_once(
    'import java.util.concurrent.ConcurrentHashMap;\n\nimport fi.iki.elonen.NanoHTTPD;',
    'import java.util.concurrent.ConcurrentHashMap;\nimport java.io.*;\nimport java.nio.file.*;\n\nimport fi.iki.elonen.NanoHTTPD;',
    "Add file I/O imports"
)

# ── 2. Add persistence fields after modbus ──
replace_once(
    '    private ModbusListener modbus;',
    '''    private ModbusListener modbus;
    private String dataDir;
    private static final String PRODUCTS_FILE = "products.json";
    private static final String BATCH_HISTORY_FILE = "batch_history.json";
    private List<Map<String,Object>> batchHistory = new ArrayList<>();
    
    private File getDataFile(String name) {
        File dir = new File(dataDir);
        if (!dir.exists()) dir.mkdirs();
        return new File(dir, name);
    }
    
    private void saveProductsToDisk() {
        try {
            StringBuilder j = new StringBuilder();
            j.append("{\\"products\\":[");
            for (int i = 0; i < products.size(); i++) {
                if (i > 0) j.append(",");
                Map<String,Object> p = products.get(i);
                j.append("{\\"id\\":").append(p.get("id"))
                 .append(",\\"name\\":\\"").append(escapeJson((String)p.getOrDefault("name",""))).append("\\"")
                 .append(",\\"pulsesPerLiter\\":").append(p.get("pulsesPerLiter"))
                 .append(",\\"valveTime\\":").append(p.get("valveTime"))
                 .append(",\\"calStatus\\":\\"").append(escapeJson((String)p.getOrDefault("calStatus","Not Calibrated"))).append("\\"")
                 .append(",\\"calDate\\":\\"").append(escapeJson((String)p.getOrDefault("calDate",""))).append("\\"")
                 .append(",\\"valveOffsetGallons\\":").append(p.getOrDefault("valveOffsetGallons",0.0))
                 .append(",\\"autoLearnEnabled\\":").append(p.getOrDefault("autoLearnEnabled",true))
                 .append(",\\"averageError\\":").append(p.getOrDefault("averageError",0.0))
                 .append(",\\"lastAutoLearnDate\\":\\"").append(escapeJson((String)p.getOrDefault("lastAutoLearnDate",""))).append("\\"")
                 .append(",\\"learningSamples\\":").append(p.getOrDefault("learningSamples",0))
                 .append("}");
            }
            j.append("]}");
            File f = getDataFile(PRODUCTS_FILE);
            try (FileWriter w = new FileWriter(f)) {
                w.write(j.toString());
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    
    private void loadProductsFromDisk() {
        File f = getDataFile(PRODUCTS_FILE);
        if (!f.exists()) return;
        try {
            String json = new String(java.nio.file.Files.readAllBytes(f.toPath()));
            if (json.isEmpty() || !json.contains("products")) return;
            int start = json.indexOf("[");
            int end = json.lastIndexOf("]");
            if (start < 0 || end < 0) return;
            String arr = json.substring(start + 1, end);
            if (arr.trim().isEmpty()) return;
            products.clear();
            nextProductId = 1;
            String[] parts = arr.split("\\\\},\\\\{");
            for (String part : parts) {
                Map<String,Object> p = new HashMap<>();
                String clean = part.replace("{", "").replace("}", "").trim();
                if (clean.isEmpty()) continue;
                String[] fields = clean.split(",");
                for (String field : fields) {
                    field = field.trim();
                    int colon = field.indexOf(":");
                    if (colon < 0) continue;
                    String key = field.substring(0, colon).trim().replace("\\"", "");
                    String val = field.substring(colon + 1).trim().replace("\\"", "");
                    if (key.equals("id")) p.put("id", Integer.parseInt(val));
                    else if (key.equals("name")) p.put("name", val);
                    else if (key.equals("pulsesPerLiter") || key.equals("pulsesPerGallon")) p.put("pulsesPerLiter", Double.parseDouble(val));
                    else if (key.equals("valveTime")) p.put("valveTime", Double.parseDouble(val));
                    else if (key.equals("calStatus")) p.put("calStatus", val);
                    else if (key.equals("calDate")) p.put("calDate", val);
                    else if (key.equals("valveOffsetGallons")) p.put("valveOffsetGallons", Double.parseDouble(val));
                    else if (key.equals("autoLearnEnabled")) p.put("autoLearnEnabled", Boolean.parseBoolean(val));
                    else if (key.equals("averageError")) p.put("averageError", Double.parseDouble(val));
                    else if (key.equals("lastAutoLearnDate")) p.put("lastAutoLearnDate", val);
                    else if (key.equals("learningSamples")) p.put("learningSamples", Integer.parseInt(val));
                }
                if (p.containsKey("id")) {
                    products.add(p);
                    int id = (int)p.get("id");
                    if (id >= nextProductId) nextProductId = id + 1;
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    
    private void saveBatchHistoryToDisk() {
        try {
            StringBuilder j = new StringBuilder();
            j.append("{\\"history\\":[");
            for (int i = 0; i < batchHistory.size(); i++) {
                if (i > 0) j.append(",");
                Map<String,Object> b = batchHistory.get(i);
                j.append("{\\"productName\\":\\"").append(escapeJson((String)b.getOrDefault("productName",""))).append("\\"")
                 .append(",\\"stationAddr\\":").append(b.getOrDefault("stationAddr",0))
                 .append(",\\"requestedGallons\\":").append(b.getOrDefault("requestedGallons",0.0))
                 .append(",\\"deliveredGallons\\":").append(b.getOrDefault("deliveredGallons",0.0))
                 .append(",\\"error\\":").append(b.getOrDefault("error",0.0))
                 .append(",\\"valveOffset\\":").append(b.getOrDefault("valveOffset",0.0))
                 .append(",\\"timestamp\\":\\"").append(escapeJson((String)b.getOrDefault("timestamp",""))).append("\\"")
                 .append(",\\"productId\\":").append(b.getOrDefault("productId",0))
                 .append("}");
            }
            j.append("]}");
            File f = getDataFile(BATCH_HISTORY_FILE);
            try (FileWriter w = new FileWriter(f)) {
                w.write(j.toString());
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    
    private void loadBatchHistoryFromDisk() {
        File f = getDataFile(BATCH_HISTORY_FILE);
        if (!f.exists()) return;
        try {
            String json = new String(java.nio.file.Files.readAllBytes(f.toPath()));
            if (json.isEmpty() || !json.contains("history")) return;
            int start = json.indexOf("[");
            int end = json.lastIndexOf("]");
            if (start < 0 || end < 0) return;
            String arr = json.substring(start + 1, end);
            if (arr.trim().isEmpty()) return;
            batchHistory.clear();
            String[] parts = arr.split("\\\\},\\\\{");
            for (String part : parts) {
                Map<String,Object> b = new HashMap<>();
                String clean = part.replace("{", "").replace("}", "").trim();
                if (clean.isEmpty()) continue;
                String[] fields = clean.split(",");
                for (String field : fields) {
                    field = field.trim();
                    int colon = field.indexOf(":");
                    if (colon < 0) continue;
                    String key = field.substring(0, colon).trim().replace("\\"", "");
                    String val = field.substring(colon + 1).trim().replace("\\"", "");
                    if (key.equals("productName")) b.put("productName", val);
                    else if (key.equals("stationAddr")) b.put("stationAddr", Integer.parseInt(val));
                    else if (key.equals("requestedGallons")) b.put("requestedGallons", Double.parseDouble(val));
                    else if (key.equals("deliveredGallons")) b.put("deliveredGallons", Double.parseDouble(val));
                    else if (key.equals("error")) b.put("error", Double.parseDouble(val));
                    else if (key.equals("valveOffset")) b.put("valveOffset", Double.parseDouble(val));
                    else if (key.equals("timestamp")) b.put("timestamp", val);
                    else if (key.equals("productId")) b.put("productId", Integer.parseInt(val));
                }
                if (b.containsKey("productName")) batchHistory.add(b);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }''',
    "Add persistence fields"
)

# ── 3. Update constructor signature and body ──
replace_once(
    '    public DashboardServer(ModbusListener modbusListener) {',
    '    public DashboardServer(ModbusListener modbusListener, String dataDir) {'
)
replace_once(
    '        super(PORT);\n        this.modbus = modbusListener;',
    '        super(PORT);\n        this.modbus = modbusListener;\n        this.dataDir = dataDir != null ? dataDir : System.getProperty("java.io.tmpdir");\n        loadProductsFromDisk();\n        loadBatchHistoryFromDisk();'
)

# ── 4. Update getProductsJson to include new fields ──
old_pj = '             .append(",\\"valveTime\\":").append(p.get("valveTime"))\n             .append(",\\"calStatus\\":\\"").append(escapeJson((String)p.getOrDefault("calStatus","Not Calibrated"))).append("\\"")'
new_pj = '             .append(",\\"valveTime\\":").append(p.get("valveTime"))\n             .append(",\\"calStatus\\":\\"").append(escapeJson((String)p.getOrDefault("calStatus","Not Calibrated"))).append("\\"")\n             .append(",\\"calDate\\":\\"").append(escapeJson((String)p.getOrDefault("calDate",""))).append("\\"")\n             .append(",\\"valveOffsetGallons\\":").append(p.getOrDefault("valveOffsetGallons",0.0))\n             .append(",\\"autoLearnEnabled\\":").append(p.getOrDefault("autoLearnEnabled",true))\n             .append(",\\"averageError\\":").append(p.getOrDefault("averageError",0.0))\n             .append(",\\"lastAutoLearnDate\\":\\"").append(escapeJson((String)p.getOrDefault("lastAutoLearnDate",""))).append("\\"")\n             .append(",\\"learningSamples\\":").append(p.getOrDefault("learningSamples",0))'
replace_once(old_pj, new_pj, "Update getProductsJson fields")

# ── 5. Update addDefaultProducts ──
replace_once(
    'p1.put("valveTime",0.250);\n        products.add(p1);',
    'p1.put("valveTime",0.250);\n        p1.put("valveOffsetGallons",0.20);\n        p1.put("autoLearnEnabled",true);\n        p1.put("averageError",0.0);\n        p1.put("lastAutoLearnDate","");\n        p1.put("learningSamples",0);\n        products.add(p1);'
)
replace_once(
    'p2.put("valveTime",0.300);\n        products.add(p2);',
    'p2.put("valveTime",0.300);\n        p2.put("valveOffsetGallons",0.20);\n        p2.put("autoLearnEnabled",true);\n        p2.put("averageError",0.0);\n        p2.put("lastAutoLearnDate","");\n        p2.put("learningSamples",0);\n        products.add(p2);'
)
replace_once(
    'p3.put("valveTime",0.200);\n        products.add(p3);',
    'p3.put("valveTime",0.200);\n        p3.put("valveOffsetGallons",0.20);\n        p3.put("autoLearnEnabled",true);\n        p3.put("averageError",0.0);\n        p3.put("lastAutoLearnDate","");\n        p3.put("learningSamples",0);\n        products.add(p3);'
)
replace_once(
    'p4.put("valveTime",0.150);\n        products.add(p4);',
    'p4.put("valveTime",0.150);\n        p4.put("valveOffsetGallons",0.20);\n        p4.put("autoLearnEnabled",true);\n        p4.put("averageError",0.0);\n        p4.put("lastAutoLearnDate","");\n        p4.put("learningSamples",0);\n        products.add(p4);'
)

# ── 6. Update handleSaveProducts ──
old_hsp = '    private Response handleSaveProducts(String rawJson) {\n        return jsonResponse("{\\"status\\":\\"ok\\",\\"message\\":\\"Products saved\\"}");\n    }'
new_hsp = '''    private Response handleSaveProducts(String rawJson) {
        try {
            if (rawJson == null || rawJson.isEmpty()) {
                return jsonResponse("{\\"status\\":\\"error\\",\\"message\\":\\"Empty body\\"}");
            }
            int arrStart = rawJson.indexOf("[");
            int arrEnd = rawJson.lastIndexOf("]");
            if (arrStart >= 0 && arrEnd > arrStart) {
                String arr = rawJson.substring(arrStart + 1, arrEnd);
                products.clear();
                nextProductId = 1;
                if (!arr.trim().isEmpty()) {
                    String[] parts = arr.split("\\\\},\\\\{");
                    for (String part : parts) {
                        Map<String,Object> p = new HashMap<>();
                        String clean = part.replace("{", "").replace("}", "").trim();
                        if (clean.isEmpty()) continue;
                        String[] fields = clean.split(",");
                        for (String field : fields) {
                            field = field.trim();
                            int colon = field.indexOf(":");
                            if (colon < 0) continue;
                            String key = field.substring(0, colon).trim().replace("\\"", "");
                            String val = field.substring(colon + 1).trim().replace("\\"", "");
                            if (key.equals("id")) { int id = Integer.parseInt(val); p.put("id", id); if (id >= nextProductId) nextProductId = id + 1; }
                            else if (key.equals("name")) p.put("name", val);
                            else if (key.equals("pulsesPerLiter")) p.put("pulsesPerLiter", Double.parseDouble(val));
                            else if (key.equals("valveTime")) p.put("valveTime", Double.parseDouble(val));
                            else if (key.equals("calStatus")) p.put("calStatus", val);
                            else if (key.equals("calDate")) p.put("calDate", val);
                            else if (key.equals("valveOffsetGallons")) p.put("valveOffsetGallons", Double.parseDouble(val));
                            else if (key.equals("autoLearnEnabled")) { 
                                try { p.put("autoLearnEnabled", Boolean.parseBoolean(val)); } catch (Exception ex) { p.put("autoLearnEnabled", true); }
                            }
                            else if (key.equals("averageError")) p.put("averageError", Double.parseDouble(val));
                            else if (key.equals("lastAutoLearnDate")) p.put("lastAutoLearnDate", val);
                            else if (key.equals("learningSamples")) p.put("learningSamples", Integer.parseInt(val));
                        }
                        if (p.containsKey("id")) products.add(p);
                    }
                }
            }
            saveProductsToDisk();
            return jsonResponse("{\\"status\\":\\"ok\\",\\"message\\":\\"Products saved (" + products.size() + " products)\\"}");
        } catch (Exception e) {
            return jsonResponse("{\\"status\\":\\"error\\",\\"message\\":\\"" + escapeJson(e.getMessage()) + "\\"}");
        }
    }'''
replace_once(old_hsp, new_hsp, "Update handleSaveProducts")

# ── 7. Add batch history and valve timing API endpoints after /api/products ──
old_api = '''        if ("/api/products".equals(uri)) {
            if ("GET".equals(method)) {
                return jsonResponse(getProductsJson());
            }
            if ("POST".equals(method) && body != null) {
                return handleSaveProducts(body.get("raw"));
            }
        }'''

new_api = '''        if ("/api/products".equals(uri)) {
            if ("GET".equals(method)) {
                return jsonResponse(getProductsJson());
            }
            if ("POST".equals(method) && body != null) {
                return handleSaveProducts(body.get("raw"));
            }
        }

        // ── API: Batch History ────────────────────────────
        if ("/api/batch/history".equals(uri)) {
            if ("GET".equals(method)) {
                return jsonResponse(getBatchHistoryJson());
            }
            if ("POST".equals(method) && body != null) {
                return handleSaveBatch(body.get("raw"));
            }
        }

        // ── API: Valve Timing Settings (per product) ──────
        if ("/api/valvetiming".equals(uri) && "POST".equals(method) && body != null) {
            return handleValveTimingUpdate(body.get("raw"));
        }

        // ── API: Reset Learning History ──────────────────
        if ("/api/valvetiming/reset".equals(uri) && "POST".equals(method) && body != null) {
            return handleResetLearning(body.get("raw"));
        }'''

replace_once(old_api, new_api, "Add batch/valve API endpoints")

# ── 8. Add new helper methods after escapeJson ──
old_esc = '''    private String escapeJson(String s) {
        if (s == null) return "";
        return s.replace("\\\\", "\\\\\\\\").replace("\\"", "\\\\\\"").replace("\\\\n", "\\\\\\\\n").replace("\\\\r", "\\\\\\\\r").replace("\\\\t", "\\\\\\\\t");
    }'''

new_helpers = '''    private String escapeJson(String s) {
        if (s == null) return "";
        return s.replace("\\\\", "\\\\\\\\").replace("\\"", "\\\\\\"").replace("\\\\n", "\\\\\\\\n").replace("\\\\r", "\\\\\\\\r").replace("\\\\t", "\\\\\\\\t");
    }

    private String getBatchHistoryJson() {
        StringBuilder j = new StringBuilder();
        j.append("{\\"history\\":[");
        for (int i = 0; i < batchHistory.size(); i++) {
            if (i > 0) j.append(",");
            Map<String,Object> b = batchHistory.get(i);
            j.append("{\\"productName\\":\\"").append(escapeJson((String)b.getOrDefault("productName",""))).append("\\"")
             .append(",\\"stationAddr\\":").append(b.getOrDefault("stationAddr",0))
             .append(",\\"requestedGallons\\":").append(b.getOrDefault("requestedGallons",0.0))
             .append(",\\"deliveredGallons\\":").append(b.getOrDefault("deliveredGallons",0.0))
             .append(",\\"error\\":").append(b.getOrDefault("error",0.0))
             .append(",\\"valveOffset\\":").append(b.getOrDefault("valveOffset",0.0))
             .append(",\\"timestamp\\":\\"").append(escapeJson((String)b.getOrDefault("timestamp",""))).append("\\"")
             .append(",\\"productId\\":").append(b.getOrDefault("productId",0))
             .append("}");
        }
        j.append("]}");
        return j.toString();
    }

    private Response handleSaveBatch(String rawJson) {
        try {
            if (rawJson == null || rawJson.isEmpty()) {
                return jsonResponse("{\\"status\\":\\"error\\",\\"message\\":\\"Empty body\\"}");
            }
            Map<String,Object> entry = new HashMap<>();
            String clean = rawJson.replace("{", "").replace("}", "");
            String[] fields = clean.split(",");
            for (String field : fields) {
                field = field.trim();
                int colon = field.indexOf(":");
                if (colon < 0) continue;
                String key = field.substring(0, colon).trim().replace("\\"", "");
                String val = field.substring(colon + 1).trim().replace("\\"", "");
                if (key.equals("productName")) entry.put("productName", val);
                else if (key.equals("stationAddr")) entry.put("stationAddr", Integer.parseInt(val));
                else if (key.equals("requestedGallons")) entry.put("requestedGallons", Double.parseDouble(val));
                else if (key.equals("deliveredGallons")) entry.put("deliveredGallons", Double.parseDouble(val));
                else if (key.equals("error")) entry.put("error", Double.parseDouble(val));
                else if (key.equals("valveOffset")) entry.put("valveOffset", Double.parseDouble(val));
                else if (key.equals("timestamp")) entry.put("timestamp", val);
                else if (key.equals("productId")) entry.put("productId", Integer.parseInt(val));
            }
            if (entry.containsKey("productName")) {
                batchHistory.add(entry);
                while (batchHistory.size() > 200) batchHistory.remove(0);
                saveBatchHistoryToDisk();
                return jsonResponse("{\\"status\\":\\"ok\\",\\"message\\":\\"Batch recorded\\"}");
            }
            return jsonResponse("{\\"status\\":\\"error\\",\\"message\\":\\"Missing fields\\"}");
        } catch (Exception e) {
            return jsonResponse("{\\"status\\":\\"error\\",\\"message\\":\\"" + escapeJson(e.getMessage()) + "\\"}");
        }
    }

    private Response handleValveTimingUpdate(String rawJson) {
        try {
            if (rawJson == null || rawJson.isEmpty()) {
                return jsonResponse("{\\"status\\":\\"error\\",\\"message\\":\\"Empty body\\"}");
            }
            int productId = 0;
            double valveOffset = 0;
            boolean autoLearn = true;
            String clean = rawJson.replace("{", "").replace("}", "");
            String[] fields = clean.split(",");
            for (String field : fields) {
                field = field.trim();
                int colon = field.indexOf(":");
                if (colon < 0) continue;
                String key = field.substring(0, colon).trim().replace("\\"", "");
                String val = field.substring(colon + 1).trim().replace("\\"", "");
                if (key.equals("productId")) productId = Integer.parseInt(val);
                else if (key.equals("valveOffsetGallons")) valveOffset = Double.parseDouble(val);
                else if (key.equals("autoLearnEnabled")) autoLearn = Boolean.parseBoolean(val);
            }
            for (Map<String,Object> p : products) {
                if ((int)p.get("id") == productId) {
                    p.put("valveOffsetGallons", valveOffset);
                    p.put("autoLearnEnabled", autoLearn);
                    saveProductsToDisk();
                    return jsonResponse("{\\"status\\":\\"ok\\",\\"message\\":\\"Valve timing updated\\"}");
                }
            }
            return jsonResponse("{\\"status\\":\\"error\\",\\"message\\":\\"Product not found\\"}");
        } catch (Exception e) {
            return jsonResponse("{\\"status\\":\\"error\\",\\"message\\":\\"" + escapeJson(e.getMessage()) + "\\"}");
        }
    }

    private Response handleResetLearning(String rawJson) {
        try {
            int productId = 0;
            String clean = rawJson.replace("{", "").replace("}", "");
            String[] fields = clean.split(",");
            for (String field : fields) {
                field = field.trim();
                int colon = field.indexOf(":");
                if (colon < 0) continue;
                String key = field.substring(0, colon).trim().replace("\\"", "");
                String val = field.substring(colon + 1).trim().replace("\\"", "");
                if (key.equals("productId")) productId = Integer.parseInt(val);
            }
            for (Map<String,Object> p : products) {
                if ((int)p.get("id") == productId) {
                    p.put("averageError", 0.0);
                    p.put("learningSamples", 0);
                    p.put("lastAutoLearnDate", "");
                    saveProductsToDisk();
                    return jsonResponse("{\\"status\\":\\"ok\\",\\"message\\":\\"Learning history reset\\"}");
                }
            }
            return jsonResponse("{\\"status\\":\\"error\\",\\"message\\":\\"Product not found\\"}");
        } catch (Exception e) {
            return jsonResponse("{\\"status\\":\\"error\\",\\"message\\":\\"" + escapeJson(e.getMessage()) + "\\"}");
        }
    }'''

replace_once(old_esc, new_helpers, "Add helper methods")

with open('app/src/main/java/com/batchloader/app/DashboardServer.java', 'w') as f:
    f.write(content)

print("\n✅ Backend updated successfully!")
