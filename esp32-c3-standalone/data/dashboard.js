/* ══════════════════════════════════════════════════════
   Master Dashboard – Real-time Board Status
   ══════════════════════════════════════════════════════ */

var API = '';   // same origin – ESP32 serves this page

// Mock data structure for boards — will be replaced by API data
var boards = [
  { id: 1, product: '—', status: 'OFFLINE', state: 'offline', indicator: 'offline', name: 'Board 1' },
  { id: 2, product: '—', status: 'OFFLINE', state: 'offline', indicator: 'offline', name: 'Board 2' },
  { id: 3, product: '—', status: 'OFFLINE', state: 'offline', indicator: 'offline', name: 'Board 3' },
  { id: 4, product: '—', status: 'OFFLINE', state: 'offline', indicator: 'offline', name: 'Board 4' }
];

// Load data structure
var loadData = [];  // Start empty - user adds loads

var products = [
  { id: 1, name: 'Acid' },
  { id: 2, name: 'Caustic' },
  { id: 3, name: 'Rinse Water' },
  { id: 4, name: 'Additive' }
];

var statsCounts = {
  totalBoards: 4,
  onlineCount: 0,
  dispensingCount: 0,
  alarmsCount: 0
};

var currentEditingBoard = null;

function init() {
  updateDateTime();
  renderBoardsGrid();       // Show clean offline state immediately
  renderProductsTable();
  renderLoadTable();
  updateStats();
  fetchVersion();
  
  // Refresh board status from API every 2 seconds
  setInterval(function() {
    updateDateTime();
    refreshBoardStatus();
  }, 2000);
}

function fetchVersion() {
  fetch(API + '/api/version')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      var ver = data.version || 'unknown';
      var sidebarEl = document.getElementById('fw-version');
      if (sidebarEl) sidebarEl.textContent = 'Teensy FW: v' + ver;
      var settingsEl = document.getElementById('fw-version-sidebar');
      if (settingsEl) settingsEl.textContent = 'v' + ver;
    })
    .catch(function(e) {
      console.warn('Could not fetch version:', e);
    });
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
  // Fetch real board status from ESP32 API
  try {
    var r = new XMLHttpRequest();
    r.open('GET', API + '/api/boards/status', true);
    r.timeout = 3000;
    r.onload = function() {
      if (r.status === 200) {
        try {
          var stations = JSON.parse(r.responseText);
          
          // Update boards array from stations
          boards = stations.map(function(station) {
            return {
              id: station.address,
              name: station.name || ('Station ' + station.address),
              product: 'Not Assigned', // Will be updated when products are assigned
              status: station.online ? 'ONLINE' : 'OFFLINE',
              state: station.online ? 'idle' : 'offline',
              indicator: station.online ? 'online' : 'offline'
            };
          });
          
          // Update stats
          var onlineCount = stations.filter(function(s) { return s.online; }).length;
          statsCounts.totalBoards = stations.length;
          statsCounts.onlineCount = onlineCount;
          
          renderBoardsGrid();
          updateStats();
        } catch(e) { 
          console.error('Parse error:', e); 
        }
      }
    };
    r.onerror = function() { 
      console.warn('API call failed'); 
    };
    r.send();
  } catch(e) { 
    console.error('Request error:', e); 
  }
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
  
  // Fetch real board status and products
  fetchBoardsAndRenderLoad();
}

function fetchBoardsAndRenderLoad() {
  // Fetch stations
  var xhr1 = new XMLHttpRequest();
  xhr1.open('GET', API + '/api/boards/status', true);
  xhr1.onload = function() {
    if (xhr1.status === 200) {
      try {
        var stations = JSON.parse(xhr1.responseText);
        
        // Fetch products
        var xhr2 = new XMLHttpRequest();
        xhr2.open('GET', API + '/api/products', true);
        xhr2.onload = function() {
          if (xhr2.status === 200) {
            try {
              var response = JSON.parse(xhr2.responseText);
              var productsData = response.products || [];
              renderLoadTableWithData(stations, productsData);
            } catch(e) {
              console.error('Failed to parse products:', e);
            }
          }
        };
        xhr2.send();
        
      } catch(e) {
        console.error('Failed to parse stations:', e);
      }
    }
  };
  xhr1.send();
}

function renderLoadTableWithData(stations, productsData) {
  var tbody = document.getElementById('load-table-body');
  if (!tbody) return;
  
  tbody.innerHTML = '';
  
  // Initialize loadData from stations if empty
  if (loadData.length === 0) {
    stations.forEach(function(station) {
      loadData.push({
        address: station.address,
        stationName: station.name || ('Station ' + station.address),
        product: productsData.length > 0 ? productsData[0].name : '',
        productId: productsData.length > 0 ? productsData[0].id : 0,
        amount: 0
      });
    });
  }
  
  loadData.forEach(function(load, index) {
    var tr = document.createElement('tr');
    
    // Build product dropdown with IDs
    var productOptions = '<option value="">-- Select Product --</option>';
    productsData.forEach(function(p) {
      var selected = (p.id === load.productId) ? ' selected' : '';
      productOptions += '<option value="' + p.id + '"' + selected + '>' + p.name + '</option>';
    });
    
    // Station status indicator
    var station = stations.find(function(s) { return s.address === load.address; });
    var statusBadge = station && station.online 
      ? '<span class="status-online">●</span>' 
      : '<span class="status-offline">●</span>';
    
    tr.innerHTML = 
      '<td class="load-address-cell">' + load.address + '</td>' +
      '<td>' + statusBadge + ' ' + (load.stationName || '') + '</td>' +
      '<td><select onchange="updateLoadProduct(' + index + ', this.value, ' + JSON.stringify(productsData).replace(/"/g, '&quot;') + ')">' + productOptions + '</select></td>' +
      '<td><input type="number" step="0.1" min="0" value="' + (load.amount || 0) + '" onchange="updateLoadAmount(' + index + ', this.value)" placeholder="0.0" /></td>' +
      '<td><button class="btn-remove" onclick="removeLoad(' + index + ')">✕</button></td>';
    
    tbody.appendChild(tr);
  });
}

function updateLoadStationName(index, value) {
  if (loadData[index]) {
    loadData[index].stationName = value;
  }
}

function updateLoadProduct(index, productId, productsData) {
  if (loadData[index]) {
    var pid = parseInt(productId);
    loadData[index].productId = pid;
    
    // Find product name from ID
    var product = productsData.find(function(p) { return p.id === pid; });
    if (product) {
      loadData[index].product = product.name;
    }
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
  
  // Call the actual API to rename station
  var xhr = new XMLHttpRequest();
  xhr.open('POST', API + '/api/boards/rename', true);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.onload = function() {
    if (xhr.status === 200) {
      // Update local board data
      for (var i = 0; i < boards.length; i++) {
        if (boards[i].id === currentEditingBoard.id) {
          boards[i].name = newName;
          break;
        }
      }
      
      renderBoardsGrid();
      if (document.getElementById('page-boards').classList.contains('active')) {
        renderBoardsManagement();
      }
      
      closeRenameModal();
      alert('Station renamed successfully to: ' + newName);
      
      // Refresh board status to get updated names
      refreshBoardStatus();
    } else {
      alert('Failed to rename station: ' + xhr.responseText);
    }
  };
  xhr.onerror = function() {
    alert('Error connecting to server');
  };
  
  xhr.send(JSON.stringify({
    address: currentEditingBoard.id,
    name: newName
  }));
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

var runStations = [];  // Populated from API on renderRunPage

function renderRunPage() {
  var container = document.getElementById('run-stations-container');
  if (!container) return;
  
  // Fetch real boards from API
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API + '/api/boards/status', true);
  xhr.onload = function() {
    if (xhr.status === 200) {
      try {
        var stations = JSON.parse(xhr.responseText);
        
        // Filter to only online stations
        var onlineStations = stations.filter(function(s) { return s.online; });
        
        if (loadData.length === 0) {
          container.innerHTML = '<div class="no-boards-msg">No loads configured. Please go to Load page to set up dispensing.</div>';
          return;
        }
        
        // Match with loadData to get configured loads
        var html = '';
        loadData.forEach(function(load) {
          // Find matching station
          var station = stations.find(function(s) { return s.address === load.address; });
          
          if (!station) {
            console.warn('Station ' + load.address + ' not found in API response');
            return;
          }
          
          // Skip if no product selected or amount is zero
          if (!load.productId || load.amount <= 0) {
            return;
          }
          
          html += '<div class="run-station-card">';
          html += '  <div class="run-station-number">' + station.address + '</div>';
          html += '  <div class="run-station-info">';
          html += '    <div class="run-station-header">';
          html += '      <div class="run-station-name">' + (station.name || 'Station ' + station.address) + '</div>';
          html += '    </div>';
          html += '    <div class="run-station-product">Product: ' + (load.product || 'Not Selected') + '</div>';
          html += '    <div class="run-station-amounts">';
          html += '      <div class="run-amount-item">';
          html += '        <span class="run-amount-label">Target</span>';
          html += '        <span class="run-amount-value">' + load.amount.toFixed(2) + ' L</span>';
          html += '      </div>';
          html += '      <div class="run-amount-item">';
          html += '        <span class="run-amount-label">Dispensed</span>';
          html += '        <span class="run-amount-value">' + (station.dispensedAmount || 0).toFixed(2) + ' L</span>';
          html += '      </div>';
          html += '      <div class="run-amount-item">';
          html += '        <span class="run-amount-label">Time to Done</span>';
          html += '        <span class="run-amount-value">--:--</span>';
          html += '      </div>';
          html += '    </div>';
          html += '    <div class="run-progress-bar">';
          var progress = station.targetAmount > 0 ? (station.dispensedAmount / station.targetAmount * 100) : 0;
          html += '      <div class="run-progress-fill" style="width: ' + progress + '%">' + Math.round(progress) + '%</div>';
          html += '    </div>';
          html += '    <div class="run-station-time">' + (station.dispensing ? 'Running...' : 'Ready') + '</div>';
          html += '  </div>';
          html += '  <div class="run-station-actions">';
          if (station.dispensing) {
            html += '    <button class="btn btn-secondary" onclick="stopStation(' + station.address + ')">STOP</button>';
          } else {
            html += '    <button class="btn btn-success" onclick="startStation(' + station.address + ')">RUN</button>';
          }
          html += '  </div>';
          html += '</div>';
        });
        
        if (html === '') {
          container.innerHTML = '<div class="no-boards-msg">No loads with products configured. Please go to Load page to set up dispensing.</div>';
        } else {
          container.innerHTML = html;
        }
        
      } catch(e) {
        console.error('Failed to parse boards:', e);
        container.innerHTML = '<div class="error-msg">Error: ' + e.message + '</div>';
      }
    } else {
      container.innerHTML = '<div class="error-msg">API Error: ' + xhr.status + '</div>';
    }
  };
  xhr.onerror = function() {
    container.innerHTML = '<div class="error-msg">Connection error - cannot reach API</div>';
  };
  xhr.ontimeout = function() {
    container.innerHTML = '<div class="error-msg">API timeout</div>';
  };
  xhr.timeout = 5000;
  xhr.send();
}

function startStation(address) {
  // Find the load for this address to get the target litres
  var load = loadData.find(function(l) { return l.address == address; });
  var litres = 0;
  if (load && load.amount > 0) {
    litres = load.amount;
  } else {
    alert('No load configured for Station ' + address);
    return;
  }
  
  fetch(API + '/api/run/start', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ address: address, litres: litres })
  })
  .then(function(r) { return r.json(); })
  .then(function(data) {
    console.log('Start response:', data);
    renderRunPage();  // Refresh display
  })
  .catch(function(e) {
    alert('Error starting station: ' + e);
  });
}

function stopStation(address) {
  fetch(API + '/api/run/stop', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ address: address })
  })
  .then(function(r) { return r.json(); })
  .then(function(data) {
    console.log('Stop response:', data);
    renderRunPage();
  })
  .catch(function(e) {
    console.error('Error stopping station:', e);
  });
}

function fieldCalibrate(stationId) {
  var station = runStations.find(function(s) { return s.id === stationId; });
  if (!station) return;
  
  // Find the product for this station
  var product = products.find(function(p) { return p.name === station.product; });
  if (!product) {
    alert('Product not found');
    return;
  }
  
  // Data from the completed dispense
  var targetVolume = station.target;      // Liters
  var actualPulses = station.actualPulses || 0;  // Pulses measured during dispense
  
  if (!actualPulses || actualPulses <= 0) {
    alert('No flowmeter data recorded. Cannot calibrate.');
    return;
  }
  
  // Calculate actual pulses per liter
  var actualPPL = actualPulses / targetVolume;
  var calibrationDiff = ((actualPPL - product.pulsesPerLiter) / product.pulsesPerLiter) * 100;
  
  var message = 'Field Calibration Results:\n\n' +
    'Old Pulses/Liter: ' + product.pulsesPerLiter.toFixed(1) + '\n' +
    'Measured Pulses: ' + actualPulses + '\n' +
    'Target Volume: ' + targetVolume.toFixed(2) + ' L\n' +
    'New Pulses/Liter: ' + actualPPL.toFixed(1) + '\n' +
    'Difference: ' + calibrationDiff.toFixed(1) + '%\n\n' +
    'Calculating optimal valve closing time...\n\n';
  
  // Estimate flowrate (assuming steady flow)
  // Assume it took ~10 seconds to dispense (rough estimate, in real system this comes from timestamps)
  var estimatedTime = 10;  // seconds
  var flowrateInPulsesPerSec = actualPulses / estimatedTime;
  
  // New valve closing time = (last 10% of target volume) / flowrate
  // This gives buffer time for valve to close before hitting exact target
  var bufferPulses = actualPPL * (targetVolume * 0.1);  // 10% of target as buffer
  var newValveTime = bufferPulses / flowrateInPulsesPerSec;
  
  message += 'Old Valve Close Time: ' + product.valveTime.toFixed(3) + ' sec\n' +
    'Estimated Flowrate: ' + flowrateInPulsesPerSec.toFixed(1) + ' pulses/sec\n' +
    'Recommended New Valve Close Time: ' + newValveTime.toFixed(3) + ' sec\n\n' +
    'Apply this calibration?';
  
  if (confirm(message)) {
    // Update product with new values
    product.pulsesPerLiter = actualPPL;
    product.valveTime = newValveTime;
    
    alert('✓ Product calibration updated!\n\n' +
      'New PPL: ' + product.pulsesPerLiter.toFixed(1) + '\n' +
      'New Valve Close Time: ' + product.valveTime.toFixed(3) + ' sec\n\n' +
      'Changes saved to Products page.');
    
    renderProductsTable();
    
    // In real system: PUT /api/products/{id} to save to ESP32
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
  // Fetch products from API
  fetch(API + '/api/products')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      var apiProducts = data.products || data || [];
      // Map API format (ppl/closeTime) to internal format
      products = apiProducts.map(function(p, idx) {
        return {
          id: p.id !== undefined ? p.id : idx,
          name: p.name || 'Product ' + (idx + 1),
          pulsesPerLiter: p.ppl || p.pulsesPerLiter || 450.0,
          valveTime: p.closeTime || p.valveTime || 0.25,
          calibrationStatus: p.calStatus || 'Not Calibrated',
          calibrationDate: p.calDate || null
        };
      });
      renderProductsTable();
      renderBoardConfigs();
      renderCalibrationHistory();
    })
    .catch(function(e) {
      console.error('Failed to load products:', e);
      renderProductsTable();
    });
}

function renderProductsTable() {
  var tbody = document.getElementById('products-table-body');
  if (!tbody) return;
  
  var unitLabel = currentUnit === 'liters' ? 'Liter' : 'Gallon';
  var html = '';
  
  if (products.length === 0) {
    html = '<tr><td colspan="6" style="text-align:center; padding:20px; color:#888;">No products configured. Click "+ Add Product" to create one.</td></tr>';
  } else {
    products.forEach(function(product, index) {
      // Determine calibration status
      var calStatus = product.calibrationStatus || 'Not Calibrated';
      var calBadgeClass = 'cal-badge-not';
      if (calStatus === 'Calibrated') calBadgeClass = 'cal-badge-ok';
      else if (calStatus === 'Factory') calBadgeClass = 'cal-badge-factory';
      
      html += '<tr>';
      html += '  <td>' + (index + 1) + '</td>';
      html += '  <td>' + product.name + '</td>';
      html += '  <td>' + product.pulsesPerLiter.toFixed(1) + '</td>';
      html += '  <td>' + product.valveTime.toFixed(3) + '</td>';
      html += '  <td><span class="cal-badge ' + calBadgeClass + '">' + calStatus + '</span></td>';
      html += '  <td class="product-actions">';
      html += '    <button class="btn btn-primary btn-sm" onclick="editProduct(' + product.id + ')">✏️</button>';
      html += '    <button class="btn btn-cal btn-sm" onclick="openCalibrationModal(' + product.id + ')">Cal</button>';
      html += '    <button class="btn btn-danger btn-sm" onclick="deleteProduct(' + product.id + ')">🗑️</button>';
      html += '  </td>';
      html += '</tr>';
    });
  }
  
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
  } else {
    // Add new
    var newId = products.length > 0 ? Math.max.apply(Math, products.map(function(p) { return p.id; })) + 1 : 1;
    products.push({
      id: newId,
      name: name,
      pulsesPerLiter: pulses,
      valveTime: valveTime
    });
  }
  
  // Persist to backend
  fetch(API + '/api/products', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(products)
  })
  .then(function(r) { return r.json(); })
  .then(function(data) {
    console.log('Products saved:', data);
  })
  .catch(function(e) {
    console.error('Failed to save products:', e);
  });
  
  renderProductsTable();
  closeProductModal();
}

function deleteProduct(productId) {
  if (!confirm('Delete this product? This cannot be undone.')) return;
  
  products = products.filter(function(p) { return p.id !== productId; });
  
  // Persist to backend
  fetch(API + '/api/products', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(products)
  })
  .then(function(r) { return r.json(); })
  .catch(function(e) { console.error('Failed to save after delete:', e); });
  
  renderProductsTable();
}

function closeProductModal() {
  document.getElementById('product-modal').style.display = 'none';
  editingProductId = null;
}

// ════════════════════════════════════════════════════════
// LOAD PAGE FUNCTIONS
// ════════════════════════════════════════════════════════

function openAddLoadModal() {
  var modal = document.getElementById('add-load-modal');
  if (!modal) return;
  
  // Populate station dropdown
  var stationSelect = document.getElementById('load-station-select');
  stationSelect.innerHTML = '<option value="">-- Select Station --</option>';
  for (var i = 1; i <= 10; i++) {
    var opt = document.createElement('option');
    opt.value = i;
    opt.textContent = 'Station ' + i;
    stationSelect.appendChild(opt);
  }
  
  // Populate product dropdown
  var productSelect = document.getElementById('load-product-select');
  productSelect.innerHTML = '<option value="">-- Select Product --</option>';
  products.forEach(function(p) {
    var opt = document.createElement('option');
    opt.value = p.id;
    opt.textContent = p.name;
    productSelect.appendChild(opt);
  });
  
  // Clear amount
  document.getElementById('load-amount-input').value = '';
  
  modal.style.display = 'flex';
}

function closeAddLoadModal() {
  var modal = document.getElementById('add-load-modal');
  if (modal) modal.style.display = 'none';
}

function saveNewLoad() {
  var stationId = document.getElementById('load-station-select').value;
  var productId = document.getElementById('load-product-select').value;
  var amount = parseFloat(document.getElementById('load-amount-input').value);
  
  if (!stationId || !productId || !amount || amount <= 0) {
    alert('Please fill in all fields with valid values');
    return;
  }
  
  var product = products.find(function(p) { return p.id == productId; });
  if (!product) return;
  
  // Check if entry already exists
  var existingIndex = loadData.findIndex(function(l) { return l.address == stationId; });
  
  if (existingIndex >= 0) {
    // Update existing
    loadData[existingIndex].product = product.name;
    loadData[existingIndex].productId = parseInt(productId);
    loadData[existingIndex].amount = amount;
  } else {
    // Add new
    loadData.push({
      address: parseInt(stationId),
      stationName: 'Station ' + stationId,
      product: product.name,
      productId: parseInt(productId),
      amount: amount
    });
  }
  
  renderLoadTable();
  closeAddLoadModal();
}

function renderLoadTable() {
  var tbody = document.getElementById('load-table-body');
  if (!tbody) return;
  
  tbody.innerHTML = '';
  loadData.forEach(function(load) {
    var row = document.createElement('tr');
    row.innerHTML = '<td>' + load.address + '</td>' +
                    '<td>' + load.stationName + '</td>' +
                    '<td>' + load.product + '</td>' +
                    '<td>' + load.amount.toFixed(2) + ' L</td>' +
                    '<td><button class="btn btn-sm btn-danger" onclick="removeLoad(' + load.address + ')">Remove</button></td>';
    tbody.appendChild(row);
  });
}

function removeLoad(address) {
  loadData = loadData.filter(function(l) { return l.address != address; });
  renderLoadTable();
}

function clearAllLoads() {
  if (!confirm('Clear all loads? This cannot be undone.')) return;
  loadData = [];
  renderLoadTable();
}

function goToRunPage() {
  if (loadData.length === 0) {
    alert('Add at least one load before proceeding to Run page');
    return;
  }
  switchPage('run');
}

// ════════════════════════════════════════════════════════
// RUN PAGE FUNCTIONS
// ════════════════════════════════════════════════════════

function loadInSequence() {
  alert('Load in sequence started');
  // TODO: Implement sequence loading
}

function loadOneByOne() {
  alert('Load one by one started');
  // TODO: Implement one by one loading
}

function stopAllStations() {
  if (!confirm('Stop all stations?')) return;
  alert('All stations stopped');
  // TODO: Implement stop all
}

// ═══════════════════════════════════════════════════════════
// PRODUCT CALIBRATION SYSTEM
// ═══════════════════════════════════════════════════════════

var calibrationState = {
  productId: null,
  currentStep: 1,
  targetVolume: 5.0,
  recordedPulses: 0,
  oldPPL: 0,
  oldValveTime: 0,
  newPPL: 0,
  newValveTime: 0,
  stationAddress: null,
  isDispensing: false,
  dispenseTimer: null,
  startPulses: 0
};

// ── Calibration History Store ───────────────────────────
var calibrationHistory = [];

// ── Open Calibration Modal for a product ────────────────
function openCalibrationModal(productId) {
  var product = products.find(function(p) { return p.id === productId; });
  if (!product) {
    alert('Product not found');
    return;
  }
  
  calibrationState.productId = productId;
  calibrationState.currentStep = 1;
  calibrationState.oldPPL = product.pulsesPerLiter;
  calibrationState.oldValveTime = product.valveTime;
  calibrationState.targetVolume = 5.0;
  calibrationState.recordedPulses = 0;
  calibrationState.newPPL = 0;
  calibrationState.newValveTime = 0;
  calibrationState.isDispensing = false;
  
  // Update product info in modal
  document.getElementById('cal-product-name').textContent = product.name;
  document.getElementById('cal-current-ppl').textContent = product.pulsesPerLiter.toFixed(1);
  document.getElementById('cal-current-vct').textContent = product.valveTime.toFixed(3);
  
  var unitLabel = currentUnit === 'liters' ? 'Liter' : 'Gallon';
  document.getElementById('cal-current-unit').textContent = unitLabel.toLowerCase();
  document.getElementById('cal-unit-label').textContent = unitLabel + 's';
  document.getElementById('cal-measure-unit').textContent = unitLabel === 'Liter' ? 'L' : 'gal';
  
  document.getElementById('cal-target-volume').value = product.pulsesPerLiter > 0 ? '5.0' : '5.0';
  calibrationState.targetVolume = 5.0;
  
  // Get an available station for this product
  fetchStationsForCalibration(product.name);
  
  // Reset all checkboxes
  document.getElementById('cal-check-tank').checked = false;
  document.getElementById('cal-check-container').checked = false;
  document.getElementById('cal-check-vessel').checked = false;
  document.getElementById('cal-check-valve').checked = false;
  document.getElementById('cal-operator-name').value = '';
  document.getElementById('cal-notes').value = '';
  document.getElementById('cal-actual-volume').value = '';
  
  // Reset to step 1
  showCalStep(1);
  
  // Show modal
  var modal = document.getElementById('calibration-modal');
  modal.style.display = 'flex';
}

function closeCalibrationModal() {
  // Stop dispensing if running
  if (calibrationState.isDispensing) {
    stopCalDispense();
  }
  
  var modal = document.getElementById('calibration-modal');
  modal.style.display = 'none';
  calibrationState.productId = null;
}

// ── Fetch stations for calibration dropdown ────────────
function fetchStationsForCalibration(productName) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API + '/api/boards/status', true);
  xhr.timeout = 5000;
  xhr.onload = function() {
    if (xhr.status === 200) {
      try {
        var response = JSON.parse(xhr.responseText);
        var stations = response.boards || response;
        var onlineStations = stations.filter(function(s) { return s.online; });
        
        if (onlineStations.length > 0) {
          calibrationState.stationAddress = onlineStations[0].address;
          document.getElementById('cal-dispense-station').textContent = 
            (onlineStations[0].name || 'Station ' + onlineStations[0].address);
        } else {
          document.getElementById('cal-dispense-station').textContent = 'No stations online';
        }
      } catch(e) {
        console.error('Failed to parse stations:', e);
        document.getElementById('cal-dispense-station').textContent = 'Error fetching stations';
      }
    }
  };
  xhr.onerror = function() {
    document.getElementById('cal-dispense-station').textContent = 'Connection error';
  };
  xhr.send();
}

// ── Show/Hide Steps ────────────────────────────────────
function showCalStep(step) {
  for (var i = 1; i <= 4; i++) {
    var content = document.getElementById('cal-content-' + i);
    if (content) content.style.display = (i === step) ? 'block' : 'none';
    
    var stepEl = document.getElementById('cal-step-' + i);
    if (stepEl) {
      if (i < step) {
        stepEl.className = 'cal-step cal-step-done';
        stepEl.innerHTML = '✓ ' + stepEl.getAttribute('data-step') + '. ' + getStepName(i);
      } else if (i === step) {
        stepEl.className = 'cal-step cal-step-active';
      } else {
        stepEl.className = 'cal-step';
      }
    }
  }
  calibrationState.currentStep = step;
}

function getStepName(step) {
  var names = ['', 'Prepare', 'Dispense', 'Measure', 'Confirm'];
  return names[step] || '';
}

function goToCalStep(step) {
  showCalStep(step);
}

// ── Check if ready to dispense ─────────────────────────
function checkCalReady() {
  var tank = document.getElementById('cal-check-tank').checked;
  var container = document.getElementById('cal-check-container').checked;
  var vessel = document.getElementById('cal-check-vessel').checked;
  var valve = document.getElementById('cal-check-valve').checked;
  var target = parseFloat(document.getElementById('cal-target-volume').value);
  
  var allChecked = tank && container && vessel && valve;
  var validTarget = target && target > 0;
  
  document.getElementById('cal-btn-gotodispense').disabled = !(allChecked && validTarget);
  
  if (validTarget) {
    calibrationState.targetVolume = target;
    document.getElementById('cal-dispense-target').textContent = target.toFixed(1);
  }
}

// ── Start Calibration Dispense ─────────────────────────
function startCalDispense() {
  if (!calibrationState.stationAddress) {
    alert('No station available for dispensing');
    return;
  }
  
  var target = calibrationState.targetVolume;
  
  // Disable start button, show stop
  document.getElementById('cal-btn-start-dispense').style.display = 'none';
  document.getElementById('cal-btn-stop-dispense').style.display = 'inline-block';
  document.getElementById('cal-dispense-status-text').textContent = 'Dispensing...';
  document.getElementById('cal-dispense-progress').style.display = 'block';
  
  calibrationState.isDispensing = true;
  calibrationState.startPulses = 0; // Will get from API
  
  // Call API to start dispensing
  var xhr = new XMLHttpRequest();
  xhr.open('POST', API + '/api/run/start', true);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.onload = function() {
    if (xhr.status === 200) {
      console.log('Dispense started successfully');
      // Start polling for progress
      pollDispenseProgress();
    } else {
      alert('Failed to start dispense: ' + xhr.responseText);
      stopCalDispense();
    }
  };
  xhr.onerror = function() {
    alert('Network error starting dispense');
    stopCalDispense();
  };
  xhr.send(JSON.stringify({
    address: calibrationState.stationAddress,
    litres: target
  }));
}

function pollDispenseProgress() {
  if (!calibrationState.isDispensing) return;
  
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API + '/api/status', true);
  xhr.timeout = 2000;
  xhr.onload = function() {
    if (xhr.status === 200) {
      try {
        var status = JSON.parse(xhr.responseText);
        
        // Find our station's data
        var stationData = null;
        if (status.boards) {
          stationData = status.boards.find(function(b) { return b.address === calibrationState.stationAddress; });
        } else if (status['line1'] || status['line2']) {
          // Handle the batching status format
          stationData = status;
        }
        
        if (stationData) {
          var dispensed = stationData.dispensedAmount || stationData.loaded || 0;
          var pulses = stationData.pulseCount || 0;
          var target = calibrationState.targetVolume;
          var progress = Math.min(100, (dispensed / target) * 100);
          
          // Update progress bar
          document.getElementById('cal-dispense-progress-fill').style.width = progress + '%';
          document.getElementById('cal-dispense-progress-fill').textContent = Math.round(progress) + '%';
          
          if (progress >= 100 || (stationData.state === 'done' || stationData.state === 'DONE')) {
            // Dispensing complete
            calibrationState.recordedPulses = pulses;
            onDispenseComplete();
            return;
          }
        }
        
        // Continue polling
        if (calibrationState.isDispensing) {
          calibrationState.dispenseTimer = setTimeout(pollDispenseProgress, 500);
        }
      } catch(e) {
        console.error('Poll error:', e);
        if (calibrationState.isDispensing) {
          calibrationState.dispenseTimer = setTimeout(pollDispenseProgress, 1000);
        }
      }
    } else {
      if (calibrationState.isDispensing) {
        calibrationState.dispenseTimer = setTimeout(pollDispenseProgress, 1000);
      }
    }
  };
  xhr.onerror = function() {
    if (calibrationState.isDispensing) {
      calibrationState.dispenseTimer = setTimeout(pollDispenseProgress, 1000);
    }
  };
  xhr.send();
}

function onDispenseComplete() {
  calibrationState.isDispensing = false;
  
  document.getElementById('cal-btn-start-dispense').style.display = 'none';
  document.getElementById('cal-btn-stop-dispense').style.display = 'none';
  document.getElementById('cal-dispense-status-text').textContent = '✓ Complete';
  document.getElementById('cal-dispense-progress-fill').style.width = '100%';
  document.getElementById('cal-dispense-progress-fill').textContent = '100%';
  
  document.getElementById('cal-btn-gotomeasure').style.display = 'inline-block';
  document.getElementById('cal-pulses-display').textContent = calibrationState.recordedPulses + ' pulses';
}

function stopCalDispense() {
  calibrationState.isDispensing = false;
  
  if (calibrationState.dispenseTimer) {
    clearTimeout(calibrationState.dispenseTimer);
    calibrationState.dispenseTimer = null;
  }
  
  // Stop all running
  var xhr = new XMLHttpRequest();
  xhr.open('POST', API + '/api/run/stop', true);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.send(JSON.stringify({}));
  
  document.getElementById('cal-btn-start-dispense').style.display = 'inline-block';
  document.getElementById('cal-btn-stop-dispense').style.display = 'none';
  document.getElementById('cal-dispense-status-text').textContent = 'Stopped';
}

// ── Step 3: Calculate new PPG ──────────────────────────
function calculateCalibration() {
  var actualVolume = parseFloat(document.getElementById('cal-actual-volume').value);
  var operator = document.getElementById('cal-operator-name').value.trim();
  
  if (!actualVolume || actualVolume <= 0) {
    alert('Please enter the actual volume measured');
    return false;
  }
  
  var pulses = calibrationState.recordedPulses;
  if (!pulses || pulses <= 0) {
    alert('No pulses recorded. Please complete the dispensing step first.');
    return false;
  }
  
  // Calculate new pulses per unit
  if (currentUnit === 'liters') {
    calibrationState.newPPL = pulses / actualVolume;
  } else {
    // Convert gallons to liters internally
    calibrationState.newPPL = pulses / (actualVolume * 3.78541);
  }
  
  // Calculate recommended valve close time based on flow rate
  // Estimate flow rate from pulses / time (approx 10 sec typical fill)
  var estimatedTimeSec = 10; 
  // In real system this would come from actual timing
  var flowRatePulsesPerSec = pulses / estimatedTimeSec;
  var bufferPulses = calibrationState.newPPL * (calibrationState.targetVolume * 0.05);
  calibrationState.newValveTime = Math.max(0.050, Math.min(2.0, bufferPulses / flowRatePulsesPerSec));
  
  // Display results
  document.getElementById('cal-result-old-ppl').textContent = calibrationState.oldPPL.toFixed(1);
  document.getElementById('cal-result-actual-vol').textContent = actualVolume.toFixed(3) + ' ' + 
    (currentUnit === 'liters' ? 'L' : 'gal');
  document.getElementById('cal-result-pulses').textContent = pulses;
  
  var unitLabel = currentUnit === 'liters' ? 'L' : 'gal';
  document.getElementById('cal-result-new-ppl').textContent = calibrationState.newPPL.toFixed(1) + ' pulses/' + unitLabel;
  
  var change = ((calibrationState.newPPL - calibrationState.oldPPL) / calibrationState.oldPPL) * 100;
  var changeStr = (change >= 0 ? '+' : '') + change.toFixed(1) + '%';
  document.getElementById('cal-result-change').textContent = changeStr;
  document.getElementById('cal-result-change').className = 'cal-result-value' + 
    (Math.abs(change) > 5 ? ' cal-warning' : '');
  
  document.getElementById('cal-result-new-vct').textContent = calibrationState.newValveTime.toFixed(3) + ' sec';
  
  return true;
}

// ── Move to Confirm Step (after calculation) ───────────
function goToCalStep(step) {
  if (step === 4) {
    // Calculate before proceeding
    if (!calculateCalibration()) {
      return;
    }
    document.getElementById('cal-btn-gotoconfirm').disabled = false;
  }
  showCalStep(step);
}

function checkConfirmReady() {
  // Called when operator name or actual volume changes
  var actualVol = parseFloat(document.getElementById('cal-actual-volume').value);
  document.getElementById('cal-btn-gotoconfirm').disabled = !(actualVol && actualVol > 0);
}

// Wire up the onchange for actual volume
document.addEventListener('DOMContentLoaded', function() {
  var actualVolInput = document.getElementById('cal-actual-volume');
  if (actualVolInput) {
    actualVolInput.addEventListener('input', checkConfirmReady);
  }
});

// ── Confirm and Save Calibration ───────────────────────
function confirmCalibration() {
  if (!calibrationState.productId && calibrationState.productId !== 0) return;
  
  var product = products.find(function(p) { return p.id === calibrationState.productId; });
  if (!product) return;
  
  var notes = document.getElementById('cal-notes').value.trim();
  var operator = document.getElementById('cal-operator-name').value.trim() || 'Operator';
  
  // Create calibration record
  var calRecord = {
    date: new Date(),
    productName: product.name,
    productId: calibrationState.productId,
    oldPPL: calibrationState.oldPPL,
    newPPL: calibrationState.newPPL,
    oldVCT: calibrationState.oldValveTime,
    newVCT: calibrationState.newValveTime,
    operator: operator,
    notes: notes,
    targetVolume: calibrationState.targetVolume,
    actualVolume: parseFloat(document.getElementById('cal-actual-volume').value),
    pulses: calibrationState.recordedPulses
  };
  
  // Save to local history
  calibrationHistory.push(calRecord);
  
  // Update product
  product.pulsesPerLiter = calibrationState.newPPL;
  product.valveTime = calibrationState.newValveTime;
  product.calibrationDate = new Date();
  product.calibrationStatus = 'Calibrated';
  
  // Persist to backend API
  var productData = {
    name: product.name,
    ppl: calibrationState.newPPL,
    ppg: calibrationState.newPPL * 0.264172,
    closeTime: calibrationState.newValveTime
  };
  
  // Update all products array and save
  products.forEach(function(p, idx) {
    if (p.id === calibrationState.productId) {
      products[idx].pulsesPerLiter = calibrationState.newPPL;
      products[idx].valveTime = calibrationState.newValveTime;
    }
  });
  
  // PUT to update the product
  var xhr = new XMLHttpRequest();
  xhr.open('PUT', API + '/api/products?id=' + calibrationState.productId, true);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.onload = function() {
    if (xhr.status === 200) {
      console.log('Product calibration saved');
    } else {
      console.warn('Failed to save product calibration:', xhr.responseText);
    }
  };
  xhr.send(JSON.stringify(productData));
  
  // Also save calibration record to backend
  var calData = {
    productId: calibrationState.productId,
    productName: product.name,
    oldPPL: calibrationState.oldPPL,
    newPPL: calibrationState.newPPL,
    oldVCT: calibrationState.oldValveTime,
    newVCT: calibrationState.newValveTime,
    operator: operator,
    notes: notes,
    targetVolume: calibrationState.targetVolume,
    actualVolume: calRecord.actualVolume,
    pulses: calibrationState.recordedPulses
  };
  
  fetch(API + '/api/calibration/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(calData)
  }).catch(function(e) {
    console.warn('Failed to save calibration record:', e);
  });
  
  // Update table display
  renderProductsTable();
  renderCalibrationHistory();
  
  // Close modal
  closeCalibrationModal();
  
  alert('✓ Calibration complete!\n\n' +
    product.name + '\n' +
    'PPL: ' + calibrationState.oldPPL.toFixed(1) + ' → ' + calibrationState.newPPL.toFixed(1) + '\n' +
    'Valve Close: ' + calibrationState.oldValveTime.toFixed(3) + ' → ' + calibrationState.newValveTime.toFixed(3) + ' sec\n\n' +
    'Changes saved to system.');
}

// ═══════════════════════════════════════════════════════════
// CALIBRATION HISTORY RENDERING
// ═══════════════════════════════════════════════════════════

function renderCalibrationHistory() {
  var container = document.getElementById('calibration-history-list');
  if (!container) return;
  
  if (calibrationHistory.length === 0) {
    container.innerHTML = '<p class="no-boards-msg">No calibration records yet. Run a calibration to record history here.</p>';
    return;
  }
  
  var html = '<table class="products-table"><thead><tr>' +
    '<th>Date/Time</th><th>Product</th><th>Old PPG</th><th>New PPG</th><th>Change</th><th>Operator</th>' +
    '</tr></thead><tbody>';
  
  // Show most recent first
  var history = calibrationHistory.slice().reverse();
  history.forEach(function(rec) {
    var dateStr = '';
    if (rec.date) {
      var d = new Date(rec.date);
      dateStr = d.toLocaleDateString() + ' ' + d.toLocaleTimeString();
    }
    
    var change = ((rec.newPPL - rec.oldPPL) / rec.oldPPL) * 100;
    var changeStr = (change >= 0 ? '+' : '') + change.toFixed(1) + '%';
    
    html += '<tr>';
    html += '<td style="font-size:12px;">' + dateStr + '</td>';
    html += '<td>' + rec.productName + '</td>';
    html += '<td>' + rec.oldPPL.toFixed(1) + '</td>';
    html += '<td><strong>' + rec.newPPL.toFixed(1) + '</strong></td>';
    html += '<td class="' + (Math.abs(change) > 5 ? 'over' : '') + '">' + changeStr + '</td>';
    html += '<td>' + rec.operator + '</td>';
    html += '</tr>';
  });
  
  html += '</tbody></table>';
  container.innerHTML = html;
}

// ── Update Products Table to Show Cal Status ──────────
function renderProductsTable() {
  var tbody = document.getElementById('products-table-body');
  if (!tbody) return;
  
  var unitLabel = currentUnit === 'liters' ? 'Liter' : 'Gallon';
  var html = '';
  
  if (products.length === 0) {
    html = '<tr><td colspan="6" style="text-align:center; padding:20px; color:#888;">No products configured. Click "+ Add Product" to create one.</td></tr>';
  } else {
    products.forEach(function(product, index) {
      // Determine calibration status
      var calStatus = product.calibrationStatus || 'Not Calibrated';
      var calBadgeClass = 'cal-badge-not';
      if (calStatus === 'Calibrated') calBadgeClass = 'cal-badge-ok';
      else if (calStatus === 'Factory') calBadgeClass = 'cal-badge-factory';
      
      html += '<tr>';
      html += '  <td>' + (index + 1) + '</td>';
      html += '  <td>' + product.name + '</td>';
      html += '  <td>' + product.pulsesPerLiter.toFixed(1) + '</td>';
      html += '  <td>' + product.valveTime.toFixed(3) + '</td>';
      html += '  <td><span class="cal-badge ' + calBadgeClass + '">' + calStatus + '</span></td>';
      html += '  <td class="product-actions">';
      html += '    <button class="btn btn-primary btn-sm" onclick="editProduct(' + product.id + ')">✏️</button>';
      html += '    <button class="btn btn-cal btn-sm" onclick="openCalibrationModal(' + product.id + ')">🎯 Cal</button>';
      html += '    <button class="btn btn-danger btn-sm" onclick="deleteProduct(' + product.id + ')">🗑️</button>';
      html += '  </td>';
      html += '</tr>';
    });
  }
  
  tbody.innerHTML = html;
}

// Initialize on page load
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
