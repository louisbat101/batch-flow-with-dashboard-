/* ══════════════════════════════════════════════════════
   Master Dashboard – Real-time Board Status
   ══════════════════════════════════════════════════════ */

var API = '';   // same origin – ESP32 serves this page

// Mock data structure for boards
var boards = [
  { id: 1, product: 'Acid (Product 1)', status: 'ONLINE', state: 'idle', indicator: 'online', name: 'Board 1' },
  { id: 2, product: 'Caustic (Product 2)', status: 'Dispensing', state: 'dispensing', indicator: 'dispensing', name: 'Board 2' },
  { id: 3, product: 'Rinse Water (Product 3)', status: 'OFFLINE', state: 'offline', indicator: 'offline', name: 'Board 3' },
  { id: 4, product: 'Additive (Product 4)', status: 'ONLINE', state: 'idle', indicator: 'online', name: 'Board 4' }
];

// Load data structure
var loadData = [
  { address: 1, stationName: 'Station 1', product: 'Acid', amount: 5.00 },
  { address: 2, stationName: 'Station 2 - Caustic', product: 'Caustic', amount: 15.00 },
  { address: 3, stationName: 'Station 3 - Rinse Water', product: 'Rinse Water', amount: 25.00 },
  { address: 4, stationName: 'Station 4 - Additive', product: 'Additive', amount: 5.00 }
];

var products = [
  { id: 1, name: 'Acid' },
  { id: 2, name: 'Caustic' },
  { id: 3, name: 'Rinse Water' },
  { id: 4, name: 'Additive' }
];

var statsCounts = {
  totalBoards: 4,
  onlineCount: 1,
  dispensingCount: 1,
  alarmsCount: 1
};

var currentEditingBoard = null;

function init() {
  updateDateTime();
  renderBoardsGrid();
  updateStats();
  
  // Refresh every 2 seconds
  setInterval(function() {
    updateDateTime();
    refreshBoardStatus();
  }, 2000);
}

function updateDateTime() {
  var now = new Date();
  var months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
  var days = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
  var dayName = days[now.getDay()];
  var month = months[now.getMonth()];
  var date = now.getDate();
  var year = now.getFullYear();
  var hours = String(now.getHours()).padStart(2, '0');
  var mins = String(now.getMinutes()).padStart(2, '0');
  var ampm = now.getHours() >= 12 ? 'PM' : 'AM';
  
  // Convert to 12-hour format
  var displayHours = now.getHours() % 12 || 12;
  
  var dateStr = month + ' ' + date + ', ' + year + ' ' + displayHours + ':' + mins + ' ' + ampm;
  
  var el = document.getElementById('current-date-time');
  if (el) el.textContent = dateStr;
}

function updateStats() {
  document.getElementById('stat-total-boards').textContent = statsCounts.totalBoards;
  document.getElementById('stat-online-count').textContent = statsCounts.onlineCount;
  document.getElementById('stat-dispensing-count').textContent = statsCounts.dispensingCount;
  document.getElementById('stat-alarms-count').textContent = statsCounts.alarmsCount;
}

function renderBoardsGrid() {
  var grid = document.getElementById('boards-grid');
  if (!grid) return;
  
  grid.innerHTML = '';
  
  boards.forEach(function(board) {
    var card = document.createElement('div');
    card.className = 'board-card';
    
    var statusClass = board.indicator;
    var statusText = board.status;
    
    card.innerHTML = 
      '<div class="board-number">' + board.id + '</div>' +
      '<div class="board-product">' + board.product + '</div>' +
      '<div class="board-info">' +
        '<span class="board-status-badge ' + statusClass + '">' + statusText + '</span>' +
        '<div class="board-indicator ' + statusClass + '"></div>' +
      '</div>';
    
    card.onclick = function() {
      openRenameModal(board);
    };
    
    grid.appendChild(card);
  });
}

function refreshBoardStatus() {
  // In a real system, this would fetch from ESP32 API
  // For now, we simulate with the mock data
  // You would call: GET /api/boards to get real data
  
  // Uncomment for real API call:
  /*
  try {
    var r = new XMLHttpRequest();
    r.open('GET', API + '/api/boards', true);
    r.timeout = 3000;
    r.onload = function() {
      try {
        var data = JSON.parse(r.responseText);
        boards = data.boards || boards;
        statsCounts = data.stats || statsCounts;
        renderBoardsGrid();
        updateStats();
      } catch(e) { console.error('Parse error:', e); }
    };
    r.onerror = function() { console.warn('API call failed'); };
    r.send();
  } catch(e) { console.error('Request error:', e); }
  */
}

// ── Page Navigation ────────────────────────────────────
function switchPage(pageName) {
  // Hide all pages
  var pages = document.querySelectorAll('.page');
  pages.forEach(function(page) {
    page.classList.remove('active');
  });
  
  // Show selected page
  var pageEl = document.getElementById('page-' + pageName);
  if (pageEl) {
    pageEl.classList.add('active');
  }
  
  // Update nav items
  var navItems = document.querySelectorAll('.nav-item');
  navItems.forEach(function(item) {
    item.classList.remove('active');
  });
  
  var activeNav = document.querySelector('.nav-item[onclick*="' + pageName + '"]');
  if (activeNav) {
    activeNav.classList.add('active');
  }
  
  // Render page-specific content
  if (pageName === 'products') {
    renderProductsPage();
  }
  
  if (pageName === 'boards') {
    renderBoardsManagement();
  }
  
  if (pageName === 'load') {
    renderLoadTable();
  }
  
  if (pageName === 'run') {
    renderRunPage();
  }
}

function renderLoadTable() {
  var tbody = document.getElementById('load-table-body');
  if (!tbody) return;
  
  tbody.innerHTML = '';
  
  loadData.forEach(function(load, index) {
    var tr = document.createElement('tr');
    
    var productOptions = products.map(function(p) {
      return '<option value="' + p.name + '"' + (p.name === load.product ? ' selected' : '') + '>' + p.name + '</option>';
    }).join('');
    
    tr.innerHTML = 
      '<td class="load-address-cell">' + load.address + '</td>' +
      '<td><input type="text" value="' + (load.stationName || '') + '" onchange="updateLoadStationName(' + index + ', this.value)" /></td>' +
      '<td><select onchange="updateLoadProduct(' + index + ', this.value)">' + productOptions + '</select></td>' +
      '<td><input type="number" step="0.1" value="' + load.amount.toFixed(2) + '" onchange="updateLoadAmount(' + index + ', this.value)" /></td>' +
      '<td><button class="btn-remove" onclick="removeLoad(' + index + ')">✕</button></td>';
    
    tbody.appendChild(tr);
  });
}

function updateLoadStationName(index, value) {
  if (loadData[index]) {
    loadData[index].stationName = value;
  }
}

function updateLoadProduct(index, value) {
  if (loadData[index]) {
    loadData[index].product = value;
  }
}

function updateLoadAmount(index, value) {
  if (loadData[index]) {
    loadData[index].amount = parseFloat(value) || 0;
  }
}

function removeLoad(index) {
  loadData.splice(index, 1);
  renderLoadTable();
}

function clearAllLoads() {
  if (confirm('Are you sure you want to clear all loads?')) {
    loadData = [];
    renderLoadTable();
  }
}

function goToRunPage() {
  switchPage('run');
}

function renderBoardsManagement() {
  var list = document.getElementById('boards-management-list');
  if (!list) return;
  
  list.innerHTML = '';
  
  boards.forEach(function(board) {
    var card = document.createElement('div');
    card.className = 'board-management-card';
    
    var statusClass = board.indicator;
    
    card.innerHTML =
      '<div class="bm-header">' +
        '<div class="bm-id">' + board.id + '</div>' +
      '</div>' +
      '<div class="bm-product">' + board.product + '</div>' +
      '<div class="bm-status ' + statusClass + '">' + board.status + '</div>' +
      '<div class="bm-actions">' +
        '<button class="bm-btn" onclick="openRenameModal(' + JSON.stringify(board).replace(/"/g, '&quot;') + ')">✏️ Rename</button>' +
        '<button class="bm-btn" onclick="deleteBoard(' + board.id + ')">🗑️ Delete</button>' +
      '</div>';
    
    list.appendChild(card);
  });
}

// ── Modal Functions ────────────────────────────────────
function openRenameModal(board) {
  currentEditingBoard = board;
  
  document.getElementById('modal-board-id').textContent = board.id;
  document.getElementById('modal-board-product').textContent = board.product;
  document.getElementById('modal-board-status').textContent = board.status;
  document.getElementById('modal-board-name').value = board.name || '';
  
  var modal = document.getElementById('rename-modal');
  modal.classList.add('show');
  modal.style.display = 'flex';
}

function closeRenameModal() {
  var modal = document.getElementById('rename-modal');
  modal.classList.remove('show');
  modal.style.display = 'none';
  currentEditingBoard = null;
}

function saveRenameBoard() {
  if (!currentEditingBoard) return;
  
  var newName = document.getElementById('modal-board-name').value.trim();
  
  if (!newName) {
    alert('Please enter a board name');
    return;
  }
  
  // Find and update the board
  for (var i = 0; i < boards.length; i++) {
    if (boards[i].id === currentEditingBoard.id) {
      boards[i].name = newName;
      break;
    }
  }
  
  // In real system, would send to API:
  // POST /api/boards/{id}/rename with {name: newName}
  
  renderBoardsGrid();
  if (document.getElementById('page-boards').classList.contains('active')) {
    renderBoardsManagement();
  }
  
  closeRenameModal();
  alert('Board renamed to: ' + newName);
}

function deleteBoard(boardId) {
  if (!confirm('Are you sure you want to delete this board?')) {
    return;
  }
  
  // Remove from array
  boards = boards.filter(function(b) { return b.id !== boardId; });
  statsCounts.totalBoards--;
  
  // In real system, would send to API:
  // DELETE /api/boards/{id}
  
  renderBoardsGrid();
  if (document.getElementById('page-boards').classList.contains('active')) {
    renderBoardsManagement();
  }
  updateStats();
  alert('Board deleted');
}

// ═══════════════════════════════════════════════════════════
// RUN PAGE
// ═══════════════════════════════════════════════════════════

var runStations = [
  { id: 1, name: 'Station 1 - Acid', product: 'Acid', target: 10.00, dispensed: 6.25, progress: 62, timeToDone: '01:25', running: true, done: false },
  { id: 2, name: 'Station 2 - Caustic', product: 'Caustic', target: 15.00, dispensed: 15.00, progress: 100, timeToDone: 'Done', running: false, done: true },
  { id: 3, name: 'Station 3 - Rinse Water', product: 'Rinse Water', target: 20.00, dispensed: 0.00, progress: 0, timeToDone: '--:--', running: false, done: false },
  { id: 4, name: 'Station 4 - Additive', product: 'Additive', target: 5.00, dispensed: 2.10, progress: 42, timeToDone: '02:10', running: true, done: false }
];

function renderRunPage() {
  var container = document.getElementById('run-stations-container');
  if (!container) return;
  
  var html = '';
  runStations.forEach(function(station) {
    html += '<div class="run-station-card">';
    html += '  <div class="run-station-number">' + station.id + '</div>';
    html += '  <div class="run-station-info">';
    html += '    <div class="run-station-header">';
    html += '      <div class="run-station-name">' + station.name + '</div>';
    html += '    </div>';
    html += '    <div class="run-station-product">Product: ' + station.product + '</div>';
    html += '    <div class="run-station-amounts">';
    html += '      <div class="run-amount-item">';
    html += '        <span class="run-amount-label">Target</span>';
    html += '        <span class="run-amount-value">' + station.target.toFixed(2) + ' L</span>';
    html += '      </div>';
    html += '      <div class="run-amount-item">';
    html += '        <span class="run-amount-label">Dispensed</span>';
    html += '        <span class="run-amount-value">' + station.dispensed.toFixed(2) + ' L</span>';
    html += '      </div>';
    html += '      <div class="run-amount-item">';
    html += '        <span class="run-amount-label">Time to Done</span>';
    html += '        <span class="run-amount-value">' + station.timeToDone + '</span>';
    html += '      </div>';
    html += '    </div>';
    html += '    <div class="run-progress-bar">';
    var fillClass = station.done ? 'run-progress-fill complete' : 'run-progress-fill';
    html += '      <div class="' + fillClass + '" style="width: ' + station.progress + '%">';
    html += station.progress + '%';
    html += '      </div>';
    html += '    </div>';
    html += '    <div class="run-station-time">' + (station.done ? 'Done' : (station.running ? 'Running...' : 'Ready')) + '</div>';
    html += '  </div>';
    html += '  <div class="run-station-actions">';
    if (station.done) {
      html += '    <span class="run-station-done">✓</span>';
    } else {
      var btnClass = station.running ? 'btn btn-secondary' : 'btn btn-success';
      var btnText = station.running ? 'Pause' : 'RUN';
      html += '    <button class="' + btnClass + '" onclick="toggleStation(' + station.id + ')">' + btnText + '</button>';
    }
    html += '  </div>';
    html += '</div>';
  });
  
  container.innerHTML = html;
}

function toggleStation(stationId) {
  var station = runStations.find(function(s) { return s.id === stationId; });
  if (station) {
    station.running = !station.running;
    renderRunPage();
    
    // In real system: POST /api/run/start_station or /api/run/stop_station
  }
}

function loadInSequence() {
  alert('Load in sequence - will run stations one after another');
  // In real system: POST /api/run/sequence with mode=sequential
}

function loadOneByOne() {
  alert('Load one by one - run stations simultaneously');
  // In real system: POST /api/run/sequence with mode=parallel
}

function stopAllStations() {
  if (confirm('Stop all running stations?')) {
    runStations.forEach(function(station) {
      station.running = false;
    });
    renderRunPage();
    // In real system: POST /api/run/stop
  }
}

// ═══════════════════════════════════════════════════════════
// PRODUCTS PAGE
// ═══════════════════════════════════════════════════════════

var currentUnit = 'liters';
var products = [
  { id: 1, name: 'Acid', pulsesPerLiter: 450.0, valveTime: 0.250 },
  { id: 2, name: 'Caustic', pulsesPerLiter: 375.0, valveTime: 0.300 },
  { id: 3, name: 'Rinse Water', pulsesPerLiter: 500.0, valveTime: 0.200 },
  { id: 4, name: 'Additive', pulsesPerLiter: 1000.0, valveTime: 0.150 }
];

var boardConfigs = [
  { address: 1, stationName: 'Station 1 - Acid', product: 'Acid', color: '#4f8cff' },
  { address: 2, stationName: 'Station 2 - Caustic', product: 'Caustic', color: '#36d399' },
  { address: 3, stationName: 'Station 3 - Rinse Water', product: 'Rinse Water', color: '#f59e0b' },
  { address: 4, stationName: 'Station 4 - Additive', product: 'Additive', color: '#ec4899' }
];

var editingProductId = null;

function renderProductsPage() {
  renderProductsTable();
  renderBoardConfigs();
}

function renderProductsTable() {
  var tbody = document.getElementById('products-table-body');
  if (!tbody) return;
  
  var unitLabel = currentUnit === 'liters' ? 'Liter' : 'Gallon';
  var html = '';
  
  products.forEach(function(product, index) {
    html += '<tr>';
    html += '  <td>' + (index + 1) + '</td>';
    html += '  <td>' + product.name + '</td>';
    html += '  <td>' + product.pulsesPerLiter.toFixed(1) + '</td>';
    html += '  <td>' + product.valveTime.toFixed(3) + ' sec</td>';
    html += '  <td class="product-actions">';
    html += '    <button class="btn btn-primary btn-sm" onclick="editProduct(' + product.id + ')">✏️</button>';
    html += '    <button class="btn btn-danger btn-sm" onclick="deleteProduct(' + product.id + ')">🗑️</button>';
    html += '  </td>';
    html += '</tr>';
  });
  
  tbody.innerHTML = html;
}

function renderBoardConfigs() {
  var container = document.getElementById('board-config-list');
  if (!container) return;
  
  var html = '';
  boardConfigs.forEach(function(board) {
    html += '<div class="board-config-card">';
    html += '  <div class="board-config-info">';
    html += '    <div class="board-config-name">' + board.stationName + '</div>';
    html += '    <div class="board-config-details">';
    html += '      <div class="board-config-detail"><strong>Address:</strong> ' + board.address + '</div>';
    html += '      <div class="board-config-detail"><strong>Product:</strong> ' + board.product + '</div>';
    html += '    </div>';
    html += '  </div>';
    html += '  <div class="board-config-color" style="background-color: ' + board.color + '"></div>';
    html += '</div>';
  });
  
  container.innerHTML = html;
}

function changeUnit(unit) {
  currentUnit = unit;
  document.getElementById('unit-label').textContent = unit === 'liters' ? 'Liter' : 'Gallon';
  renderProductsTable();
  // In real system: POST /api/settings/unit
}

function openAddProductModal() {
  editingProductId = null;
  document.getElementById('product-modal-title').textContent = 'Add Product';
  document.getElementById('product-name').value = '';
  document.getElementById('product-pulses').value = '';
  document.getElementById('product-valve-time').value = '';
  document.getElementById('product-modal').style.display = 'flex';
}

function editProduct(productId) {
  var product = products.find(function(p) { return p.id === productId; });
  if (!product) return;
  
  editingProductId = productId;
  document.getElementById('product-modal-title').textContent = 'Edit Product';
  document.getElementById('product-name').value = product.name;
  document.getElementById('product-pulses').value = product.pulsesPerLiter;
  document.getElementById('product-valve-time').value = product.valveTime;
  document.getElementById('product-modal').style.display = 'flex';
}

function saveProduct() {
  var name = document.getElementById('product-name').value.trim();
  var pulses = parseFloat(document.getElementById('product-pulses').value);
  var valveTime = parseFloat(document.getElementById('product-valve-time').value);
  
  if (!name || !pulses || !valveTime) {
    alert('Please fill in all fields');
    return;
  }
  
  if (editingProductId) {
    // Edit existing
    var product = products.find(function(p) { return p.id === editingProductId; });
    if (product) {
      product.name = name;
      product.pulsesPerLiter = pulses;
      product.valveTime = valveTime;
    }
    // In real system: PUT /api/products/{id}
  } else {
    // Add new
    var newId = products.length > 0 ? Math.max.apply(Math, products.map(function(p) { return p.id; })) + 1 : 1;
    products.push({
      id: newId,
      name: name,
      pulsesPerLiter: pulses,
      valveTime: valveTime
    });
    // In real system: POST /api/products
  }
  
  renderProductsTable();
  closeProductModal();
}

function deleteProduct(productId) {
  if (!confirm('Delete this product? This cannot be undone.')) return;
  
  products = products.filter(function(p) { return p.id !== productId; });
  renderProductsTable();
  // In real system: DELETE /api/products/{id}
}

function closeProductModal() {
  document.getElementById('product-modal').style.display = 'none';
  editingProductId = null;
}

// Initialize on page load
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
