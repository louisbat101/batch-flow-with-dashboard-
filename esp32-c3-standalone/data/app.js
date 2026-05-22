// Global state
let currentProducts = [];
let isEditing = false;
let editingId = null;

// DOM elements
const elements = {
    connectionStatus: document.getElementById('connection-status'),
    systemStatus: document.getElementById('system-status'),
    flowReading: document.getElementById('flow-reading'),
    hallAPulses: document.getElementById('hall-a-pulses'),
    hallBPulses: document.getElementById('hall-b-pulses'),
    totalPulses: document.getElementById('total-pulses'),
    valve1Status: document.getElementById('valve1-status'),
    valve2Status: document.getElementById('valve2-status'),
    batchState: document.getElementById('batch-state'),
    calibrationValue: document.getElementById('calibration-value'),
    progressContainer: document.getElementById('progress-container'),
    progressFill: document.getElementById('progress-fill'),
    progressText: document.getElementById('progress-text'),
    emergencyStop: document.getElementById('emergency-stop'),
    manualValve: document.getElementById('manual-valve'),
    manualLitres: document.getElementById('manual-litres'),
    startManual: document.getElementById('start-manual'),
    productsList: document.getElementById('products-list'),
    addProductBtn: document.getElementById('add-product-btn'),
    productModal: document.getElementById('product-modal'),
    productForm: document.getElementById('product-form'),
    modalTitle: document.getElementById('modal-title'),
    closeModal: document.querySelector('.close'),
    cancelProduct: document.getElementById('cancel-product'),
    messages: document.getElementById('messages')
};

// API wrapper
async function apiCall(endpoint, options = {}) {
    const defaultOptions = {
        headers: {
            'Content-Type': 'application/json'
        }
    };
    
    const response = await fetch(endpoint, { ...defaultOptions, ...options });
    
    if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    
    return response.json();
}

// Show message
function showMessage(text, type = 'info', duration = 3000) {
    const message = document.createElement('div');
    message.className = `message ${type}`;
    message.textContent = text;
    
    elements.messages.appendChild(message);
    
    setTimeout(() => {
        if (message.parentNode) {
            message.parentNode.removeChild(message);
        }
    }, duration);
}

// Update connection status
function updateConnectionStatus(connected) {
    elements.connectionStatus.textContent = connected ? 'Connected' : 'Disconnected';
    elements.connectionStatus.className = connected ? 'connected' : 'disconnected';
}

// Update system status display
function updateSystemStatus(status) {
    const state = status.state || 'unknown';
    const batching = status.batching || false;
    
    elements.systemStatus.textContent = batching ? 'BATCHING' : 'READY';
    elements.systemStatus.className = batching ? 'batching' : '';
    
    // Update readings
    elements.flowReading.textContent = `${(status.flow_litres || 0).toFixed(3)} L`;
    elements.hallAPulses.textContent = status.flow_pulses_a || 0;
    elements.hallBPulses.textContent = status.flow_pulses_b || 0;
    elements.totalPulses.textContent = status.flow_total_pulses || 0;
    elements.calibrationValue.textContent = `${(status.calibration || 450).toFixed(1)} p/L`;
    
    // Update valve status
    updateValveStatus(elements.valve1Status, status.valve1_open);
    updateValveStatus(elements.valve2Status, status.valve2_open);
    
    // Update batch state
    elements.batchState.textContent = state.toUpperCase();
    
    // Update progress bar
    if (batching && status.progress_percent !== undefined) {
        elements.progressContainer.style.display = 'block';
        elements.progressFill.style.width = `${Math.min(100, Math.max(0, status.progress_percent))}%`;
        elements.progressText.textContent = `${status.progress_percent.toFixed(1)}%`;
    } else {
        elements.progressContainer.style.display = 'none';
    }
    
    // Enable/disable controls based on state
    elements.startManual.disabled = batching;
    elements.manualValve.disabled = batching;
    elements.manualLitres.disabled = batching;
}

// Update valve status display
function updateValveStatus(element, isOpen) {
    element.textContent = isOpen ? 'OPEN' : 'CLOSED';
    element.className = `valve-status ${isOpen ? 'open' : 'closed'}`;
}

// Load status from API
async function loadStatus() {
    try {
        const status = await apiCall('/api/status');
        updateConnectionStatus(true);
        updateSystemStatus(status);
    } catch (error) {
        console.error('Failed to load status:', error);
        updateConnectionStatus(false);
    }
}

// Load products from API
async function loadProducts() {
    try {
        const data = await apiCall('/api/products');
        currentProducts = data.products || [];
        renderProducts();
    } catch (error) {
        console.error('Failed to load products:', error);
        showMessage('Failed to load products', 'error');
    }
}

// Render products list
function renderProducts() {
    elements.productsList.innerHTML = '';
    
    if (currentProducts.length === 0) {
        elements.productsList.innerHTML = '<p style="text-align: center; color: #666; padding: 2rem;">No products configured</p>';
        return;
    }
    
    currentProducts.forEach((product, index) => {
        const productDiv = document.createElement('div');
        productDiv.className = 'product-item';
        
        productDiv.innerHTML = `
            <div class="product-header">
                <span class="product-name">${escapeHtml(product.name)}</span>
            </div>
            <div class="product-details">
                Target: ${product.target}L | Valve: ${product.valve} | Calibration: ${product.calibration} p/L
            </div>
            <div class="product-actions">
                <button class="btn btn-success btn-sm" onclick="startProductBatch(${product.id})">▶️ Start</button>
                <button class="btn btn-secondary btn-sm" onclick="editProduct(${product.id})">✏️ Edit</button>
                <button class="btn btn-danger btn-sm" onclick="deleteProduct(${product.id})">🗑️ Delete</button>
            </div>
        `;
        
        elements.productsList.appendChild(productDiv);
    });
}

// Escape HTML to prevent XSS
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Start batch with product
async function startProductBatch(productId) {
    if (!confirm('Start batch with this product?')) return;
    
    try {
        await apiCall('/api/batch/start', {
            method: 'POST',
            body: JSON.stringify({ product_id: productId })
        });
        showMessage('Batch started successfully', 'success');
    } catch (error) {
        console.error('Failed to start batch:', error);
        showMessage('Failed to start batch', 'error');
    }
}

// Start manual batch
async function startManualBatch() {
    const valve = parseInt(elements.manualValve.value);
    const litres = parseFloat(elements.manualLitres.value);
    
    if (!valve || !litres || litres <= 0) {
        showMessage('Please enter valid valve and litres', 'warning');
        return;
    }
    
    if (!confirm(`Start manual batch: ${litres}L on Valve ${valve}?`)) return;
    
    try {
        await apiCall('/api/batch/manual', {
            method: 'POST',
            body: JSON.stringify({ valve, litres })
        });
        showMessage('Manual batch started successfully', 'success');
    } catch (error) {
        console.error('Failed to start manual batch:', error);
        showMessage('Failed to start manual batch', 'error');
    }
}

// Stop batch (emergency stop)
async function stopBatch() {
    if (!confirm('Emergency stop all batching?')) return;
    
    try {
        await apiCall('/api/batch/stop', { method: 'POST' });
        showMessage('Batch stopped', 'warning');
    } catch (error) {
        console.error('Failed to stop batch:', error);
        showMessage('Failed to stop batch', 'error');
    }
}

// Show add product modal
function showAddProductModal() {
    isEditing = false;
    editingId = null;
    elements.modalTitle.textContent = 'Add Product';
    elements.productForm.reset();
    
    // Set defaults
    document.getElementById('product-calibration').value = 450;
    document.getElementById('product-closetime').value = 500;
    
    elements.productModal.style.display = 'block';
}

// Show edit product modal
function editProduct(productId) {
    const product = currentProducts.find(p => p.id === productId);
    if (!product) return;
    
    isEditing = true;
    editingId = productId;
    elements.modalTitle.textContent = 'Edit Product';
    
    // Populate form
    document.getElementById('product-name').value = product.name;
    document.getElementById('product-target').value = product.target;
    document.getElementById('product-valve').value = product.valve;
    document.getElementById('product-calibration').value = product.calibration;
    document.getElementById('product-closetime').value = product.closeTime;
    
    elements.productModal.style.display = 'block';
}

// Hide product modal
function hideProductModal() {
    elements.productModal.style.display = 'none';
    isEditing = false;
    editingId = null;
}

// Save product
async function saveProduct(event) {
    event.preventDefault();
    
    const formData = new FormData(elements.productForm);
    const productData = {
        name: document.getElementById('product-name').value,
        target: parseFloat(document.getElementById('product-target').value),
        valve: parseInt(document.getElementById('product-valve').value),
        calibration: parseFloat(document.getElementById('product-calibration').value),
        closeTime: parseInt(document.getElementById('product-closetime').value)
    };
    
    // Validation
    if (!productData.name.trim()) {
        showMessage('Product name is required', 'warning');
        return;
    }
    
    if (productData.target <= 0 || productData.target > 100) {
        showMessage('Target must be between 0.1 and 100 litres', 'warning');
        return;
    }
    
    try {
        if (isEditing) {
            await apiCall(`/api/products?id=${editingId}`, {
                method: 'PUT',
                body: JSON.stringify(productData)
            });
            showMessage('Product updated successfully', 'success');
        } else {
            await apiCall('/api/products', {
                method: 'POST',
                body: JSON.stringify(productData)
            });
            showMessage('Product added successfully', 'success');
        }
        
        hideProductModal();
        await loadProducts();
    } catch (error) {
        console.error('Failed to save product:', error);
        showMessage('Failed to save product', 'error');
    }
}

// Delete product
async function deleteProduct(productId) {
    const product = currentProducts.find(p => p.id === productId);
    if (!product || !confirm(`Delete product "${product.name}"?`)) return;
    
    try {
        await apiCall(`/api/products?id=${productId}`, { method: 'DELETE' });
        showMessage('Product deleted successfully', 'success');
        await loadProducts();
    } catch (error) {
        console.error('Failed to delete product:', error);
        showMessage('Failed to delete product', 'error');
    }
}

// Initialize application
function init() {
    // Event listeners
    elements.emergencyStop.addEventListener('click', stopBatch);
    elements.startManual.addEventListener('click', startManualBatch);
    elements.addProductBtn.addEventListener('click', showAddProductModal);
    elements.productForm.addEventListener('submit', saveProduct);
    elements.closeModal.addEventListener('click', hideProductModal);
    elements.cancelProduct.addEventListener('click', hideProductModal);
    
    // Close modal when clicking outside
    window.addEventListener('click', (event) => {
        if (event.target === elements.productModal) {
            hideProductModal();
        }
    });
    
    // Initial data load
    loadStatus();
    loadProducts();
    
    // Start polling
    setInterval(loadStatus, 1000); // Update status every second
    setInterval(loadProducts, 30000); // Reload products every 30 seconds
}

// Start when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}