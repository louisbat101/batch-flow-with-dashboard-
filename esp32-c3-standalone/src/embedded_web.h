#ifndef EMBEDDED_WEB_H
#define EMBEDDED_WEB_H

// Embedded HTML content
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Batch Flow Controller</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
            color: #333;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 16px;
            overflow: hidden;
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1);
        }

        .header {
            background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
            color: white;
            padding: 30px;
            text-align: center;
            position: relative;
        }

        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            font-weight: 700;
        }

        .status-indicators {
            display: flex;
            justify-content: center;
            gap: 20px;
            margin-top: 20px;
        }

        .indicator {
            display: flex;
            align-items: center;
            gap: 8px;
            background: rgba(255, 255, 255, 0.2);
            padding: 10px 15px;
            border-radius: 25px;
            backdrop-filter: blur(10px);
        }

        .indicator-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #ff4444;
            animation: pulse 2s infinite;
        }

        .indicator-dot.connected {
            background: #44ff44;
        }

        @keyframes pulse {
            0% { opacity: 1; }
            50% { opacity: 0.5; }
            100% { opacity: 1; }
        }

        .main-content {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 30px;
            padding: 30px;
        }

        .panel {
            background: #f8fafc;
            border-radius: 12px;
            padding: 25px;
            border: 1px solid #e2e8f0;
            transition: transform 0.2s ease, box-shadow 0.2s ease;
        }

        .panel:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(0, 0, 0, 0.1);
        }

        .panel h2 {
            color: #2d3748;
            margin-bottom: 20px;
            font-size: 1.4em;
            display: flex;
            align-items: center;
            gap: 10px;
        }

        .current-batch {
            grid-column: 1 / -1;
            background: linear-gradient(135deg, #a8edea 0%, #fed6e3 100%);
            border: none;
        }

        .batch-info {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }

        .info-card {
            background: rgba(255, 255, 255, 0.8);
            padding: 20px;
            border-radius: 10px;
            text-align: center;
        }

        .info-card h3 {
            font-size: 0.9em;
            color: #666;
            margin-bottom: 5px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .info-card .value {
            font-size: 2em;
            font-weight: bold;
            color: #2d3748;
        }

        .info-card .unit {
            font-size: 0.8em;
            color: #666;
            margin-left: 5px;
        }

        .form-group {
            margin-bottom: 20px;
        }

        .form-group label {
            display: block;
            margin-bottom: 8px;
            font-weight: 600;
            color: #374151;
        }

        .form-group input, .form-group select {
            width: 100%;
            padding: 12px;
            border: 2px solid #e5e7eb;
            border-radius: 8px;
            font-size: 16px;
            transition: border-color 0.2s ease, box-shadow 0.2s ease;
        }

        .form-group input:focus, .form-group select:focus {
            outline: none;
            border-color: #4f46e5;
            box-shadow: 0 0 0 3px rgba(79, 70, 229, 0.1);
        }

        .btn {
            padding: 12px 24px;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s ease;
            text-decoration: none;
            display: inline-block;
            text-align: center;
        }

        .btn-primary {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }

        .btn-primary:hover {
            transform: translateY(-2px);
            box-shadow: 0 8px 20px rgba(102, 126, 234, 0.3);
        }

        .btn-danger {
            background: linear-gradient(135deg, #ff6b6b 0%, #ee5a52 100%);
            color: white;
        }

        .btn-success {
            background: linear-gradient(135deg, #51cf66 0%, #40c057 100%);
            color: white;
        }

        .btn-warning {
            background: linear-gradient(135deg, #ffd43b 0%, #fab005 100%);
            color: white;
        }

        .progress-container {
            background: #e2e8f0;
            border-radius: 10px;
            overflow: hidden;
            margin: 20px 0;
        }

        .progress-bar {
            height: 24px;
            background: linear-gradient(90deg, #4facfe 0%, #00f2fe 100%);
            width: 0%;
            transition: width 0.3s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
            font-size: 0.9em;
        }

        .status-message {
            padding: 15px;
            border-radius: 8px;
            margin: 20px 0;
            font-weight: 500;
        }

        .status-running {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }

        .status-complete {
            background: #cce7ff;
            color: #004085;
            border: 1px solid #b8daff;
        }

        .status-error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }

        .product-list {
            max-height: 300px;
            overflow-y: auto;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
        }

        .product-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 15px;
            border-bottom: 1px solid #e5e7eb;
            transition: background-color 0.2s ease;
        }

        .product-item:hover {
            background-color: #f9fafb;
        }

        .product-item:last-child {
            border-bottom: none;
        }

        .product-info {
            flex-grow: 1;
        }

        .product-name {
            font-weight: 600;
            color: #1f2937;
        }

        .product-details {
            font-size: 0.9em;
            color: #6b7280;
            margin-top: 2px;
        }

        .product-actions {
            display: flex;
            gap: 10px;
        }

        .btn-small {
            padding: 8px 12px;
            font-size: 14px;
        }

        @media (max-width: 768px) {
            .main-content {
                grid-template-columns: 1fr;
                gap: 20px;
                padding: 20px;
            }
            
            .batch-info {
                grid-template-columns: 1fr 1fr;
            }
            
            .status-indicators {
                flex-direction: column;
                align-items: center;
            }
            
            .header h1 {
                font-size: 2em;
            }
        }

        .hidden {
            display: none !important;
        }

        .calibration-section {
            background: #fff7ed;
            border: 1px solid #fed7aa;
            border-radius: 8px;
            padding: 20px;
            margin-top: 20px;
        }

        .calibration-section h3 {
            color: #9a3412;
            margin-bottom: 15px;
        }

        .calibration-inputs {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 15px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🌊 Batch Flow Controller</h1>
            <p>ESP32-C3 Dual Hall Sensor System</p>
            <div class="status-indicators">
                <div class="indicator">
                    <div class="indicator-dot" id="powerStatus"></div>
                    <span>Power</span>
                </div>
                <div class="indicator">
                    <div class="indicator-dot" id="rs485Status"></div>
                    <span>RS485</span>
                </div>
                <div class="indicator">
                    <div class="indicator-dot" id="flowStatus"></div>
                    <span>Flow Sensor</span>
                </div>
            </div>
        </div>

        <div class="main-content">
            <!-- Current Batch Panel -->
            <div class="panel current-batch">
                <h2>🎯 Current Batch Status</h2>
                <div class="batch-info">
                    <div class="info-card">
                        <h3>Product</h3>
                        <div class="value" id="currentProduct">-</div>
                    </div>
                    <div class="info-card">
                        <h3>Target Volume</h3>
                        <div class="value" id="targetVolume">0<span class="unit">L</span></div>
                    </div>
                    <div class="info-card">
                        <h3>Current Volume</h3>
                        <div class="value" id="currentVolume">0.0<span class="unit">L</span></div>
                    </div>
                    <div class="info-card">
                        <h3>Flow Rate</h3>
                        <div class="value" id="flowRate">0.0<span class="unit">L/min</span></div>
                    </div>
                </div>
                
                <div class="progress-container">
                    <div class="progress-bar" id="progressBar">0%</div>
                </div>
                
                <div id="statusMessage" class="status-message hidden"></div>
                
                <div style="text-align: center; margin-top: 20px;">
                    <button class="btn btn-success" id="startBatchBtn">Start Batch</button>
                    <button class="btn btn-danger" id="stopBatchBtn">Stop Batch</button>
                    <button class="btn btn-warning" id="testValveBtn">Test Valve</button>
                </div>
            </div>

            <!-- Batch Control Panel -->
            <div class="panel">
                <h2>⚙️ Batch Control</h2>
                <div class="form-group">
                    <label for="productSelect">Select Product:</label>
                    <select id="productSelect">
                        <option value="">-- Select Product --</option>
                    </select>
                </div>
                <div class="form-group">
                    <label for="customVolume">Custom Volume (L):</label>
                    <input type="number" id="customVolume" min="0.1" max="1000" step="0.1" placeholder="Enter volume">
                </div>
                <button class="btn btn-primary" id="startCustomBtn">Start Custom Batch</button>
            </div>

            <!-- Product Management Panel -->
            <div class="panel">
                <h2>📦 Product Management</h2>
                <div class="form-group">
                    <label for="productName">Product Name:</label>
                    <input type="text" id="productName" placeholder="Enter product name">
                </div>
                <div class="form-group">
                    <label for="productVolume">Default Volume (L):</label>
                    <input type="number" id="productVolume" min="0.1" max="1000" step="0.1" placeholder="Enter volume">
                </div>
                <button class="btn btn-primary" id="addProductBtn">Add Product</button>
                
                <div class="product-list" id="productList">
                    <!-- Products will be loaded here -->
                </div>
            </div>

            <!-- System Info & Calibration Panel -->
            <div class="panel">
                <h2>🔧 System Information</h2>
                <div class="info-card">
                    <h3>System Status</h3>
                    <div style="text-align: left; margin-top: 10px;">
                        <p><strong>Firmware:</strong> v1.0.0</p>
                        <p><strong>Uptime:</strong> <span id="uptime">0</span> seconds</p>
                        <p><strong>Free Memory:</strong> <span id="freeMemory">0</span> bytes</p>
                        <p><strong>Hall A Pulses:</strong> <span id="hallAPulses">0</span></p>
                        <p><strong>Hall B Pulses:</strong> <span id="hallBPulses">0</span></p>
                    </div>
                </div>

                <div class="calibration-section">
                    <h3>🎛️ Flow Sensor Calibration</h3>
                    <div class="calibration-inputs">
                        <div>
                            <label>Pulses per Liter:</label>
                            <input type="number" id="pulsesPerLiter" value="450" min="1" max="10000">
                        </div>
                        <div>
                            <label>Sensor A Weight:</label>
                            <input type="number" id="sensorAWeight" value="0.6" min="0.1" max="1.0" step="0.1">
                        </div>
                    </div>
                    <button class="btn btn-warning btn-small" id="updateCalibrationBtn">Update Calibration</button>
                </div>
            </div>
        </div>
    </div>

    <script>
        // Global state
        let currentBatch = null;
        let products = [];
        let statusInterval = null;

        // Initialize the application
        document.addEventListener('DOMContentLoaded', function() {
            initializeApp();
            startStatusPolling();
            bindEvents();
        });

        function initializeApp() {
            updatePowerStatus();
            loadProducts();
            updateSystemInfo();
        }

        function startStatusPolling() {
            // Poll status every 1 second
            statusInterval = setInterval(updateStatus, 1000);
            
            // Update system info every 5 seconds
            setInterval(updateSystemInfo, 5000);
        }

        function bindEvents() {
            document.getElementById('startBatchBtn').addEventListener('click', startSelectedBatch);
            document.getElementById('stopBatchBtn').addEventListener('click', stopBatch);
            document.getElementById('testValveBtn').addEventListener('click', testValve);
            document.getElementById('startCustomBtn').addEventListener('click', startCustomBatch);
            document.getElementById('addProductBtn').addEventListener('click', addProduct);
            document.getElementById('updateCalibrationBtn').addEventListener('click', updateCalibration);
        }

        function updatePowerStatus() {
            // Power is always on if we can access the page
            document.getElementById('powerStatus').classList.add('connected');
        }

        async function updateStatus() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();
                
                // Update status indicators
                updateStatusIndicator('rs485Status', data.valveConnected);
                updateStatusIndicator('flowStatus', data.flowRate > 0);
                
                // Update batch info
                document.getElementById('currentVolume').innerHTML = `${data.currentVolume.toFixed(2)}<span class="unit">L</span>`;
                document.getElementById('flowRate').innerHTML = `${data.flowRate.toFixed(2)}<span class="unit">L/min</span>`;
                
                // Update batch status
                if (data.batchActive) {
                    currentBatch = data;
                    updateBatchDisplay(data);
                } else {
                    currentBatch = null;
                    clearBatchDisplay();
                }
                
            } catch (error) {
                console.error('Failed to update status:', error);
                // Update indicators to show disconnected state
                updateStatusIndicator('rs485Status', false);
                updateStatusIndicator('flowStatus', false);
            }
        }

        async function updateSystemInfo() {
            try {
                const response = await fetch('/api/info');
                const data = await response.json();
                
                document.getElementById('uptime').textContent = data.uptime;
                document.getElementById('freeMemory').textContent = data.freeMemory;
                document.getElementById('hallAPulses').textContent = data.hallAPulses;
                document.getElementById('hallBPulses').textContent = data.hallBPulses;
                
            } catch (error) {
                console.error('Failed to update system info:', error);
            }
        }

        function updateStatusIndicator(elementId, connected) {
            const indicator = document.getElementById(elementId);
            if (connected) {
                indicator.classList.add('connected');
            } else {
                indicator.classList.remove('connected');
            }
        }

        function updateBatchDisplay(data) {
            document.getElementById('currentProduct').textContent = data.productName || 'Custom';
            document.getElementById('targetVolume').innerHTML = `${data.targetVolume}<span class="unit">L</span>`;
            
            const progress = Math.min((data.currentVolume / data.targetVolume) * 100, 100);
            const progressBar = document.getElementById('progressBar');
            progressBar.style.width = `${progress}%`;
            progressBar.textContent = `${progress.toFixed(1)}%`;
            
            const statusMessage = document.getElementById('statusMessage');
            statusMessage.className = 'status-message';
            
            if (data.batchComplete) {
                statusMessage.classList.add('status-complete');
                statusMessage.textContent = `Batch completed! Final volume: ${data.currentVolume.toFixed(2)}L`;
            } else if (data.error) {
                statusMessage.classList.add('status-error');
                statusMessage.textContent = `Error: ${data.error}`;
            } else {
                statusMessage.classList.add('status-running');
                statusMessage.textContent = `Batch in progress... ${(data.targetVolume - data.currentVolume).toFixed(2)}L remaining`;
            }
            
            statusMessage.classList.remove('hidden');
        }

        function clearBatchDisplay() {
            document.getElementById('currentProduct').textContent = '-';
            document.getElementById('targetVolume').innerHTML = '0<span class="unit">L</span>';
            document.getElementById('progressBar').style.width = '0%';
            document.getElementById('progressBar').textContent = '0%';
            document.getElementById('statusMessage').classList.add('hidden');
        }

        async function loadProducts() {
            try {
                const response = await fetch('/api/products');
                products = await response.json();
                updateProductSelect();
                updateProductList();
            } catch (error) {
                console.error('Failed to load products:', error);
            }
        }

        function updateProductSelect() {
            const select = document.getElementById('productSelect');
            select.innerHTML = '<option value="">-- Select Product --</option>';
            
            products.forEach(product => {
                const option = document.createElement('option');
                option.value = product.name;
                option.textContent = `${product.name} (${product.volume}L)`;
                select.appendChild(option);
            });
        }

        function updateProductList() {
            const list = document.getElementById('productList');
            list.innerHTML = '';
            
            if (products.length === 0) {
                list.innerHTML = '<div class="product-item"><span>No products configured</span></div>';
                return;
            }
            
            products.forEach(product => {
                const item = document.createElement('div');
                item.className = 'product-item';
                item.innerHTML = `
                    <div class="product-info">
                        <div class="product-name">${product.name}</div>
                        <div class="product-details">${product.volume}L default volume</div>
                    </div>
                    <div class="product-actions">
                        <button class="btn btn-primary btn-small" onclick="startProductBatch('${product.name}')">Start</button>
                        <button class="btn btn-danger btn-small" onclick="deleteProduct('${product.name}')">Delete</button>
                    </div>
                `;
                list.appendChild(item);
            });
        }

        async function startSelectedBatch() {
            const selectedProduct = document.getElementById('productSelect').value;
            if (!selectedProduct) {
                alert('Please select a product first');
                return;
            }
            
            await startProductBatch(selectedProduct);
        }

        async function startProductBatch(productName) {
            try {
                const response = await fetch('/api/batch/start', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ productName })
                });
                
                if (response.ok) {
                    showMessage('Batch started successfully!', 'success');
                } else {
                    const error = await response.text();
                    showMessage(`Failed to start batch: ${error}`, 'error');
                }
            } catch (error) {
                showMessage('Network error starting batch', 'error');
            }
        }

        async function startCustomBatch() {
            const volume = parseFloat(document.getElementById('customVolume').value);
            if (!volume || volume <= 0) {
                alert('Please enter a valid volume');
                return;
            }
            
            try {
                const response = await fetch('/api/batch/start', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ volume })
                });
                
                if (response.ok) {
                    document.getElementById('customVolume').value = '';
                    showMessage('Custom batch started successfully!', 'success');
                } else {
                    const error = await response.text();
                    showMessage(`Failed to start batch: ${error}`, 'error');
                }
            } catch (error) {
                showMessage('Network error starting batch', 'error');
            }
        }

        async function stopBatch() {
            try {
                const response = await fetch('/api/batch/stop', { method: 'POST' });
                if (response.ok) {
                    showMessage('Batch stopped', 'warning');
                } else {
                    showMessage('Failed to stop batch', 'error');
                }
            } catch (error) {
                showMessage('Network error stopping batch', 'error');
            }
        }

        async function testValve() {
            try {
                const response = await fetch('/api/valve/test', { method: 'POST' });
                if (response.ok) {
                    showMessage('Valve test completed', 'success');
                } else {
                    showMessage('Valve test failed', 'error');
                }
            } catch (error) {
                showMessage('Network error testing valve', 'error');
            }
        }

        async function addProduct() {
            const name = document.getElementById('productName').value.trim();
            const volume = parseFloat(document.getElementById('productVolume').value);
            
            if (!name || !volume || volume <= 0) {
                alert('Please enter valid product name and volume');
                return;
            }
            
            try {
                const response = await fetch('/api/products', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name, volume })
                });
                
                if (response.ok) {
                    document.getElementById('productName').value = '';
                    document.getElementById('productVolume').value = '';
                    await loadProducts();
                    showMessage('Product added successfully!', 'success');
                } else {
                    const error = await response.text();
                    showMessage(`Failed to add product: ${error}`, 'error');
                }
            } catch (error) {
                showMessage('Network error adding product', 'error');
            }
        }

        async function deleteProduct(productName) {
            if (!confirm(`Are you sure you want to delete product "${productName}"?`)) {
                return;
            }
            
            try {
                const response = await fetch(`/api/products/${encodeURIComponent(productName)}`, {
                    method: 'DELETE'
                });
                
                if (response.ok) {
                    await loadProducts();
                    showMessage('Product deleted successfully!', 'success');
                } else {
                    showMessage('Failed to delete product', 'error');
                }
            } catch (error) {
                showMessage('Network error deleting product', 'error');
            }
        }

        async function updateCalibration() {
            const pulsesPerLiter = parseInt(document.getElementById('pulsesPerLiter').value);
            const sensorAWeight = parseFloat(document.getElementById('sensorAWeight').value);
            
            if (!pulsesPerLiter || !sensorAWeight) {
                alert('Please enter valid calibration values');
                return;
            }
            
            try {
                const response = await fetch('/api/calibration', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ pulsesPerLiter, sensorAWeight })
                });
                
                if (response.ok) {
                    showMessage('Calibration updated successfully!', 'success');
                } else {
                    showMessage('Failed to update calibration', 'error');
                }
            } catch (error) {
                showMessage('Network error updating calibration', 'error');
            }
        }

        function showMessage(message, type) {
            // Create a temporary notification
            const notification = document.createElement('div');
            notification.style.cssText = `
                position: fixed;
                top: 20px;
                right: 20px;
                padding: 15px 20px;
                border-radius: 8px;
                color: white;
                font-weight: 500;
                z-index: 1000;
                animation: slideIn 0.3s ease-out;
                max-width: 300px;
            `;
            
            if (type === 'success') {
                notification.style.background = 'linear-gradient(135deg, #51cf66 0%, #40c057 100%)';
            } else if (type === 'error') {
                notification.style.background = 'linear-gradient(135deg, #ff6b6b 0%, #ee5a52 100%)';
            } else {
                notification.style.background = 'linear-gradient(135deg, #ffd43b 0%, #fab005 100%)';
                notification.style.color = '#333';
            }
            
            notification.textContent = message;
            document.body.appendChild(notification);
            
            // Remove after 3 seconds
            setTimeout(() => {
                notification.remove();
            }, 3000);
        }

        // Add CSS animation
        const style = document.createElement('style');
        style.textContent = `
            @keyframes slideIn {
                from { transform: translateX(100%); opacity: 0; }
                to { transform: translateX(0); opacity: 1; }
            }
        `;
        document.head.appendChild(style);
    </script>
</body>
</html>
)rawliteral";

#endif