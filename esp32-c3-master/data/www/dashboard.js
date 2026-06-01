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
  
  // Refresh every 5 seconds (was 2s - too aggressive for ESP32)
  setInterval(function() {
    updateDateTime();
    refreshBoardStatus();
  }, 5000);
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
  
  // Only show ONLINE boards
  var onlineBoards = boards.filter(function(b) { return b.status === 'ONLINE'; });
  
  if (onlineBoards.length === 0) {
    grid.innerHTML = '<div class="no-boards-msg">No boards detected</div>';
    return;
  }
  
  onlineBoards.forEach(function(board) {
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
    r.timeout = 2000;  // Reduced from 3000ms - fail faster to avoid blocking ESP32
    r.onload = function() {
      if (r.status === 200) {
        try {
          var response = JSON.parse(r.responseText);
          var stations = response.boards || response;  // Handle {"boards": [...]} format
          
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
        var response = JSON.parse(xhr1.responseText);
        var stations = response.boards || response;  // Handle {"boards": [...]} format
        
        // Fetch products
        var xhr2 = new XMLHttpRequest();
        xhr2.open('GET', API + '/api/products', true);
        xhr2.onload = function() {
          if (xhr2.status === 200) {
            try {
              var response2 = JSON.parse(xhr2.responseText);
              var productsData = response2.products || response2;  // Handle {"products": [...]} format
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
  
  // Filter to only show ONLINE stations
  var onlineStations = stations.filter(function(s) { return s.online; });
  
  if (onlineStations.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5" style="text-align:center; color: var(--muted); padding: 20px;">No boards detected. Please check connections.</td></tr>';
    return;
  }
  
  // Only show loads that have been manually added
  if (loadData.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5" style="text-align:center; color: var(--muted); padding: 20px;">No loads configured. Click "+ Add Load" to create a load.</td></tr>';
    return;
  }
  
  // Filter loadData to only include online stations
  var filteredLoadData = loadData.filter(function(load) {
    return onlineStations.some(function(s) { return s.address === load.address; });
  });
  
  if (filteredLoadData.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5" style="text-align:center; color: var(--muted); padding: 20px;">No loads for detected stations. Click "+ Add Load" to create a load.</td></tr>';
    return;
  }
  
  filteredLoadData.forEach(function(load, index) {
    var tr = document.createElement('tr');
    
    // Build product dropdown with indices (products don't have IDs, use array index)
    var productOptions = '<option value="">-- Select Product --</option>';
    productsData.forEach(function(p, pIndex) {
      var selected = (pIndex === load.productId) ? ' selected' : '';
      productOptions += '<option value="' + pIndex + '"' + selected + '>' + p.name + '</option>';
    });
    
    // Station status indicator (all filtered stations are online)
    var statusBadge = '<span class="status-online">●</span>';
    
    tr.innerHTML = 
      '<td class="load-address-cell">' + load.address + '</td>' +
      '<td>' + statusBadge + ' ' + (load.stationName || '') + '</td>' +
      '<td><select onchange="updateLoadProduct(' + index + ', this.value, ' + JSON.stringify(productsData).replace(/"/g, '&quot;') + ')">' + productOptions + '</select></td>' +
      '<td><input type="number" step="0.1" min="0" value="' + (load.amount || 0) + '" onchange="updateLoadAmount(' + index + ', this.value)" placeholder="0.0" /></td>' +
      '<td><button class="btn-remove" onclick="removeLoad(' + index + ')">✕</button></td>';
    
    tbody.appendChild(tr);
  });
  
  // Update loadData with filtered data
  loadData = filteredLoadData;
}

function updateLoadStationName(index, value) {
  if (loadData[index]) {
    loadData[index].stationName = value;
  }
}

function updateLoadProduct(index, productIndex, productsData) {
  if (loadData[index]) {
    var pIndex = parseInt(productIndex);
    loadData[index].productId = pIndex;
    
    // Find product name from index
    var product = productsData[pIndex];
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

// ── Add Load Modal Functions ────────────────────────────
function openAddLoadModal() {
  // Fetch stations and products
  var xhr1 = new XMLHttpRequest();
  xhr1.open('GET', API + '/api/boards/status', true);
  xhr1.onload = function() {
    if (xhr1.status === 200) {
      try {
        var response = JSON.parse(xhr1.responseText);
        var stations = response.boards || response;
        var onlineStations = stations.filter(function(s) { return s.online; });
        
        // Populate station dropdown
        var stationSelect = document.getElementById('load-station-select');
        stationSelect.innerHTML = '<option value="">-- Select Station --</option>';
        onlineStations.forEach(function(station) {
          var option = document.createElement('option');
          option.value = station.address;
          option.textContent = (station.name || 'Station ' + station.address) + ' (Address ' + station.address + ')';
          stationSelect.appendChild(option);
        });
        
        // Fetch products
        var xhr2 = new XMLHttpRequest();
        xhr2.open('GET', API + '/api/products', true);
        xhr2.onload = function() {
          console.log('Products API status:', xhr2.status);
          console.log('Products API response:', xhr2.responseText);
          if (xhr2.status === 200) {
            try {
              var response = JSON.parse(xhr2.responseText);
              console.log('Parsed response:', response);
              var products = response.products || response;  // Handle {"products": [...]} format
              console.log('Products array:', products);
              console.log('Products count:', products.length);
              
              var productSelect = document.getElementById('load-product-select');
              productSelect.innerHTML = '<option value="">-- Select Product --</option>';
              products.forEach(function(product, index) {
                console.log('Adding product ' + index + ':', product.name);
                var option = document.createElement('option');
                option.value = index;  // Use index as ID since products don't have IDs
                option.textContent = product.name;
                productSelect.appendChild(option);
              });
              console.log('Product dropdown populated with', products.length, 'items');
            } catch(e) {
              console.error('Failed to parse products:', e);
            }
          } else {
            console.error('Products API failed with status:', xhr2.status);
          }
        };
        xhr2.onerror = function() {
          console.error('Products API network error');
        };
        xhr2.send();
        
      } catch(e) {
        console.error('Failed to parse stations:', e);
      }
    }
  };
  xhr1.send();
  
  // Clear form
  document.getElementById('load-amount-input').value = '';
  
  // Show modal
  var modal = document.getElementById('add-load-modal');
  modal.style.display = 'flex';
}

function closeAddLoadModal() {
  var modal = document.getElementById('add-load-modal');
  modal.style.display = 'none';
}

function saveNewLoad() {
  var stationAddress = parseInt(document.getElementById('load-station-select').value);
  var productIndex = parseInt(document.getElementById('load-product-select').value);
  var amount = parseFloat(document.getElementById('load-amount-input').value);
  
  if (!stationAddress) {
    alert('Please select a station');
    return;
  }
  
  if (isNaN(productIndex)) {
    alert('Please select a product');
    return;
  }
  
  if (!amount || amount <= 0) {
    alert('Please enter a valid amount');
    return;
  }
  
  // Fetch station name and product name
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API + '/api/boards/status', true);
  xhr.onload = function() {
    if (xhr.status === 200) {
      try {
        var response = JSON.parse(xhr.responseText);
        var stations = response.boards || response;
        var station = stations.find(function(s) { return s.address === stationAddress; });
        
        // Fetch product details
        var xhr2 = new XMLHttpRequest();
        xhr2.open('GET', API + '/api/products', true);
        xhr2.onload = function() {
          if (xhr2.status === 200) {
            try {
              var response2 = JSON.parse(xhr2.responseText);
              var products = response2.products || response2;
              var product = products[productIndex];  // Use index to get product
              
              // Check if load already exists for this station
              var existingIndex = loadData.findIndex(function(l) { return l.address === stationAddress; });
              
              if (existingIndex >= 0) {
                // Update existing load
                loadData[existingIndex].productId = productIndex;
                loadData[existingIndex].product = product ? product.name : '';
                loadData[existingIndex].amount = amount;
              } else {
                // Add new load
                loadData.push({
                  address: stationAddress,
                  stationName: station ? (station.name || 'Station ' + stationAddress) : 'Station ' + stationAddress,
                  productId: productIndex,
                  product: product ? product.name : '',
                  amount: amount
                });
              }
              
              closeAddLoadModal();
              renderLoadTable();
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
  xhr.send();
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

var runStations = [
  { id: 1, name: 'Station 1 - Acid', product: 'Acid', target: 10.00, dispensed: 6.25, progress: 62, timeToDone: '01:25', running: true, done: false },
  { id: 2, name: 'Station 2 - Caustic', product: 'Caustic', target: 15.00, dispensed: 15.00, progress: 100, timeToDone: 'Done', running: false, done: true },
  { id: 3, name: 'Station 3 - Rinse Water', product: 'Rinse Water', target: 20.00, dispensed: 0.00, progress: 0, timeToDone: '--:--', running: false, done: false },
  { id: 4, name: 'Station 4 - Additive', product: 'Additive', target: 5.00, dispensed: 2.10, progress: 42, timeToDone: '02:10', running: true, done: false }
];

function renderRunPage() {
  var container = document.getElementById('run-stations-container');
  if (!container) return;
  
  // Fetch real boards from API
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API + '/api/boards/status', true);
  xhr.onload = function() {
    if (xhr.status === 200) {
      try {
        var response = JSON.parse(xhr.responseText);
        var stations = response.boards || response;  // Handle {"boards": [...]} format
        
        // Only show ONLINE stations that have loads configured
        var onlineStations = stations.filter(function(s) { return s.online; });
        
        if (onlineStations.length === 0) {
          container.innerHTML = '<div class="no-boards-msg">No boards detected. Please check connections.</div>';
          return;
        }
        
        // Match with loadData to get configured loads
        var html = '';
        onlineStations.forEach(function(station) {
          // Find matching load data
          var load = loadData.find(function(l) { return l.address === station.address; });
          
          // Skip if no load configured or no product selected
          if (!load || !load.productId || load.amount <= 0) {
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
          html += '        <span class="run-amount-value">0.00 L</span>';
          html += '      </div>';
          html += '      <div class="run-amount-item">';
          html += '        <span class="run-amount-label">Time to Done</span>';
          html += '        <span class="run-amount-value">--:--</span>';
          html += '      </div>';
          html += '    </div>';
          html += '    <div class="run-progress-bar">';
          html += '      <div class="run-progress-fill" style="width: 0%">0%</div>';
          html += '    </div>';
          html += '    <div class="run-station-time">Ready</div>';
          html += '  </div>';
          html += '  <div class="run-station-actions">';
          html += '    <button class="btn btn-success" onclick="startStation(' + station.address + ')">RUN</button>';
          html += '  </div>';
          html += '</div>';
        });
        
        if (html === '') {
          container.innerHTML = '<div class="no-boards-msg">No loads configured. Please go to Load page to set up dispensing.</div>';
        } else {
          container.innerHTML = html;
        }
        
      } catch(e) {
        console.error('Failed to parse boards:', e);
        container.innerHTML = '<div class="error-msg">Error loading boards</div>';
      }
    }
  };
  xhr.onerror = function() {
    container.innerHTML = '<div class="error-msg">Connection error</div>';
  };
  xhr.send();
}

function startStation(address) {
  alert('Starting station ' + address + ' - API integration pending');
  // In real system: POST /api/run/start with {address: address}
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
var products = [];  // Will be loaded from ESP32 API

var boardConfigs = [
  { address: 1, stationName: 'Station 1 - Acid', product: 'Acid', color: '#4f8cff' },
  { address: 2, stationName: 'Station 2 - Caustic', product: 'Caustic', color: '#36d399' },
  { address: 3, stationName: 'Station 3 - Rinse Water', product: 'Rinse Water', color: '#f59e0b' },
  { address: 4, stationName: 'Station 4 - Additive', product: 'Additive', color: '#ec4899' }
];

var editingProductId = null;

function fetchProductsData() {
  console.log('Fetching products from API...');
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API + '/api/products', true);
  xhr.onload = function() {
    console.log('Products API status:', xhr.status);
    console.log('Products API response:', xhr.responseText);
    if (xhr.status === 200) {
      try {
        var response = JSON.parse(xhr.responseText);
        var apiProducts = response.products || response;
        console.log('Loaded', apiProducts.length, 'products from ESP32');
        
        // Convert API format to internal format (add IDs since backend doesn't provide them)
        products = apiProducts.map(function(p, index) {
          return {
            id: index,  // Use array index as ID
            name: p.name,
            pulsesPerLiter: p.ppl,
            valveTime: p.closeTime
          };
        });
        
        renderProductsTable();
      } catch(e) {
        console.error('Failed to parse products:', e);
      }
    } else {
      console.error('Failed to fetch products, status:', xhr.status);
    }
  };
  xhr.onerror = function() {
    console.error('Network error fetching products');
  };
  xhr.send();
}

function renderProductsPage() {
  fetchProductsData();  // Load products from ESP32 first
  renderBoardConfigs();
}

function renderProductsTable() {
  var tbody = document.getElementById('products-table-body');
  if (!tbody) return;
  
  var unitLabel = currentUnit === 'liters' ? 'Liter' : 'Gallon';
  var html = '';
  
  if (products.length === 0) {
    html = '<tr><td colspan="5" style="text-align:center; padding:20px; color:#888;">No products configured. Click "+ Add Product" to create one.</td></tr>';
  } else {
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
  }
  
  tbody.innerHTML = html;
}

function renderBoardConfigs() {
  var container = document.getElementById('board-config-list');
  if (!container) return;
  
  container.innerHTML = '<p class="no-boards-msg">Loading boards...</p>';
  
  // Fetch real boards from API
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API + '/api/boards/status', true);
  xhr.timeout = 5000;
  
  xhr.onload = function() {
    console.log('Board config XHR status:', xhr.status);
    console.log('Board config XHR response:', xhr.responseText);
    
    if (xhr.status === 200) {
      try {
        var response = JSON.parse(xhr.responseText);
        
        // Backend returns {"boards": [...]}
        var stations = response.boards || response;
        
        // Only show ONLINE stations
        var onlineStations = stations.filter(function(s) { return s.online; });
        
        if (onlineStations.length === 0) {
          container.innerHTML = '<p class="no-boards-msg">No boards detected. Please check connections.</p>';
          return;
        }
        
        var html = '';
        onlineStations.forEach(function(station) {
          html += '<div class="board-config-card">';
          html += '  <div class="board-config-info">';
          html += '    <div class="board-config-name">' + (station.name || 'Station ' + station.address) + '</div>';
          html += '    <div class="board-config-details">';
          html += '      <div class="board-config-detail"><strong>Address:</strong> ' + station.address + '</div>';
          html += '      <div class="board-config-detail"><strong>Status:</strong> <span class="status-online">● ONLINE</span></div>';
          html += '    </div>';
          html += '  </div>';
          html += '</div>';
        });
        
        container.innerHTML = html;
      } catch(e) {
        console.error('Failed to parse boards:', e);
        container.innerHTML = '<p class="error-msg">Error parsing response: ' + e.message + '</p>';
      }
    } else {
      container.innerHTML = '<p class="error-msg">Server returned status ' + xhr.status + '</p>';
    }
  };
  
  xhr.onerror = function() {
    console.error('Board config XHR error');
    container.innerHTML = '<p class="error-msg">Connection error - Check WiFi</p>';
  };
  
  xhr.ontimeout = function() {
    console.error('Board config XHR timeout');
    container.innerHTML = '<p class="error-msg">Request timeout</p>';
  };
  
  xhr.send();
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
    // Edit existing - not implemented yet
    alert('Editing products not supported yet');
    return;
  } else {
    // Add new product to ESP32
    var productData = {
      name: name,
      ppl: pulses,  // Backend expects "ppl" for pulses per liter
      ppg: pulses * 0.264172,  // Convert to pulses per gallon
      closeTime: valveTime
    };
    
    console.log('Sending product to API:', productData);
    
    var xhr = new XMLHttpRequest();
    xhr.open('POST', API + '/api/products', true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.onload = function() {
      console.log('Add product response:', xhr.status, xhr.responseText);
      if (xhr.status === 200) {
        alert('Product added successfully!');
        fetchProductsData();  // Reload products from server
        closeProductModal();
      } else {
        alert('Failed to add product: ' + xhr.responseText);
      }
    };
    xhr.onerror = function() {
      alert('Network error - could not add product');
    };
    xhr.send(JSON.stringify(productData));
  }
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
