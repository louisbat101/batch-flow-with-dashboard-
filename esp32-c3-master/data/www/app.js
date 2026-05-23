/* ══════════════════════════════════════════════════════
   Batch Loader – App Logic  (ES5 – no const/let/arrow/fetch)
   ══════════════════════════════════════════════════════ */

var API = '';   // same origin – ESP32 serves this page

// ── Unit settings (loaded from ESP32) ────────────────
var currentUnit = 'litres';
var unitLabel   = 'L';
var convFactor  = 1.0;
var autoStartLine2 = false;
var valve1Addr = 11;
var valve2Addr = 12;

// ── Selected line on Load page (1 or 2) ──────────────
var selectedLine = 1;

function loadSettings(callback) {
  try {
    var r = new XMLHttpRequest();
    r.open('GET', API + '/api/settings', true);
    r.timeout = 3000;
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        currentUnit = d.unit || 'litres';
        unitLabel   = d.unitLabel || 'L';
        convFactor  = d.conversionFactor || 1.0;
        autoStartLine2 = d.autoStartLine2 || false;
        valve1Addr = d.valve1Addr || 11;
        valve2Addr = d.valve2Addr || 12;
        updateUnitUI();
        updateAutoUI();
        updateValveUI();
      } catch(e) {}
      if (callback) callback();
    };
    r.onerror = function() { if (callback) callback(); };
    r.send();
  } catch(e) { if (callback) callback(); }
}

function setUnit(u) {
  try {
    var r = new XMLHttpRequest();
    r.open('PUT', API + '/api/settings', true);
    r.setRequestHeader('Content-Type', 'application/json');
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        currentUnit = d.unit || 'litres';
        unitLabel   = d.unitLabel || 'L';
        convFactor  = d.conversionFactor || 1.0;
        updateUnitUI();
      } catch(e) {}
    };
    r.send(JSON.stringify({ unit: u }));
  } catch(e) {}
}

function updateUnitUI() {
  var btnL = document.getElementById('btn-unit-litres');
  var btnG = document.getElementById('btn-unit-gallons');
  if (btnL && btnG) {
    if (currentUnit === 'gallons') {
      btnL.className = 'btn';
      btnG.className = 'btn btn-primary';
    } else {
      btnL.className = 'btn btn-primary';
      btnG.className = 'btn';
    }
  }
  var info = document.getElementById('unit-info');
  if (info) {
    info.textContent = currentUnit === 'gallons'
      ? 'Current: US Gallons (1 gal = 3.785 L)'
      : 'Current: Litres (Metric)';
  }
  var ll = document.getElementById('load-unit-label');
  if (ll) ll.textContent = currentUnit === 'gallons' ? 'Gallons' : 'Litres';
}

function fmtUnit(litres) {
  var val = litres * convFactor;
  return val.toFixed(2) + ' ' + unitLabel;
}

function setAutoStart(on) {
  try {
    var r = new XMLHttpRequest();
    r.open('PUT', API + '/api/settings', true);
    r.setRequestHeader('Content-Type', 'application/json');
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        autoStartLine2 = d.autoStartLine2 || false;
        updateAutoUI();
      } catch(e) {}
    };
    r.send(JSON.stringify({ autoStartLine2: on }));
  } catch(e) {}
}

function updateAutoUI() {
  var btnOn  = document.getElementById('btn-auto-on');
  var btnOff = document.getElementById('btn-auto-off');
  var info   = document.getElementById('auto-info');
  if (btnOn && btnOff) {
    if (autoStartLine2) {
      btnOn.className  = 'btn btn-accent';
      btnOff.className = 'btn';
    } else {
      btnOn.className  = 'btn';
      btnOff.className = 'btn btn-primary';
    }
  }
  if (info) {
    info.textContent = autoStartLine2
      ? 'Line 2 will auto-start when Line 1 finishes'
      : 'Line 2 must be started manually';
  }
}

// ── Valve Address Dropdowns ──────────────────────────
function initValveDropdowns() {
  var sel1 = document.getElementById('sel-valve1');
  var sel2 = document.getElementById('sel-valve2');
  if (!sel1 || !sel2) return;
  if (sel1.options.length > 0) return;  // already populated
  var i;
  for (i = 1; i <= 247; i++) {
    var o1 = document.createElement('option');
    o1.value = i;
    o1.textContent = 'Address ' + i;
    sel1.appendChild(o1);
    var o2 = document.createElement('option');
    o2.value = i;
    o2.textContent = 'Address ' + i;
    sel2.appendChild(o2);
  }
}

function setValveAddr(valveNum, addr) {
  var a = parseInt(addr);
  if (isNaN(a) || a < 1 || a > 247) return;
  var body = {};
  if (valveNum === 1) body.valve1Addr = a;
  else                body.valve2Addr = a;
  try {
    var r = new XMLHttpRequest();
    r.open('PUT', API + '/api/settings', true);
    r.setRequestHeader('Content-Type', 'application/json');
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        valve1Addr = d.valve1Addr || 11;
        valve2Addr = d.valve2Addr || 12;
        updateValveUI();
      } catch(e) {}
    };
    r.send(JSON.stringify(body));
  } catch(e) {}
}

function updateValveUI() {
  initValveDropdowns();
  var sel1 = document.getElementById('sel-valve1');
  var sel2 = document.getElementById('sel-valve2');
  if (sel1) sel1.value = valve1Addr;
  if (sel2) sel2.value = valve2Addr;
  var info = document.getElementById('valve-info');
  if (info) {
    info.textContent = 'Valve 1: addr ' + valve1Addr + '  |  Valve 2: addr ' + valve2Addr;
  }
}

function testValve(valveNum, action) {
  var msg = document.getElementById('valve-test-msg');
  if (msg) showMsg(msg, 'Sending ' + action + ' to Valve ' + valveNum + '...', false);
  try {
    var r = new XMLHttpRequest();
    r.open('POST', API + '/api/valve/test', true);
    r.setRequestHeader('Content-Type', 'application/json');
    r.timeout = 5000;
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        if (d.ok) {
          showMsg(msg, 'Valve ' + valveNum + ' ' + action + ' – OK ✓', false);
        } else {
          showMsg(msg, 'Valve ' + valveNum + ' ' + action + ' – ' + (d.error || 'failed'), true);
        }
      } catch(e) { showMsg(msg, 'Error parsing response', true); }
    };
    r.onerror = function() { showMsg(msg, 'Connection error', true); };
    r.ontimeout = function() { showMsg(msg, 'Timeout – no response from valve', true); };
    r.send(JSON.stringify({ valve: valveNum, action: action }));
  } catch(e) { if (msg) showMsg(msg, 'Error', true); }
}

function scanValves() {
  var res = document.getElementById('valve-scan-result');
  var btn = document.getElementById('btn-scan-valves');
  if (res) res.textContent = 'Scanning addr 1-20 (8N1 + 8E1) … ~20 sec';
  if (res) res.style.color = '#ff0';
  if (btn) btn.disabled = true;

  // Start the scan (returns immediately with {status:"started"})
  try {
    var r = new XMLHttpRequest();
    r.open('GET', API + '/api/valve/scan', true);
    r.timeout = 5000;
    r.onload = function() {
      // Now poll every 2s until status is "done"
      var pollTimer = setInterval(function() {
        try {
          var p = new XMLHttpRequest();
          p.open('GET', API + '/api/valve/scan', true);
          p.timeout = 5000;
          p.onload = function() {
            try {
              var d = JSON.parse(p.responseText);
              if (d.status === 'running') return; // still going
              clearInterval(pollTimer);
              if (btn) btn.disabled = false;
              if (d.status === 'done') {
                if (d.count === 0) {
                  if (res) { res.textContent = 'No Modbus devices found! Check wiring.'; res.style.color = '#f44'; }
                } else {
                  var txt = 'Found ' + d.count + ' device(s): ';
                  for (var i = 0; i < d.found.length; i++) {
                    txt += 'addr ' + d.found[i].addr + ' (' + (d.found[i].parity || '?') + ') ';
                  }
                  if (res) { res.textContent = txt; res.style.color = '#4f4'; }
                }
              }
            } catch(e) { clearInterval(pollTimer); if (btn) btn.disabled = false; }
          };
          p.onerror = function() { clearInterval(pollTimer); if (btn) btn.disabled = false; };
          p.send();
        } catch(e) { clearInterval(pollTimer); if (btn) btn.disabled = false; }
      }, 2000);
    };
    r.onerror = function() { if (btn) btn.disabled = false; if (res) res.textContent = 'Connection error'; };
    r.ontimeout = function() { if (btn) btn.disabled = false; if (res) res.textContent = 'Scan start timed out'; };
    r.send();
  } catch(e) { if (btn) btn.disabled = false; if (res) res.textContent = 'Error'; }
}

loadSettings();

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  LINE SELECTOR  (Load page)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
function selectLine(n) {
  selectedLine = n;
  var btn1 = document.getElementById('btn-line-1');
  var btn2 = document.getElementById('btn-line-2');
  if (btn1 && btn2) {
    btn1.className = (n === 1) ? 'btn btn-primary' : 'btn';
    btn2.className = (n === 2) ? 'btn btn-primary' : 'btn';
  }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  MAIN PAGE – live status polling (dual-line + queue)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
function pollStatus() {
  try {
    var r = new XMLHttpRequest();
    r.open('GET', API + '/api/status', true);
    r.timeout = 2000;
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        updateLineUI(1, d.line1);
        updateLineUI(2, d.line2);
        // Also update Load page queue display
        updateLoadQueues(d.line1, d.line2);
      } catch(e) {}
    };
    r.onerror = function() {
      var s1 = document.getElementById('st1-state');
      var s2 = document.getElementById('st2-state');
      if (s1) { s1.textContent = 'OFFLINE'; s1.className = 'value badge idle'; }
      if (s2) { s2.textContent = 'OFFLINE'; s2.className = 'value badge idle'; }
    };
    r.send();
  } catch(e) {}
}

function updateLineUI(num, ln) {
  if (!ln) return;
  var pre = 'st' + num;
  var stState   = document.getElementById(pre + '-state');
  var stProduct = document.getElementById(pre + '-product');
  var stTarget  = document.getElementById(pre + '-target');
  var stBar     = document.getElementById(pre + '-bar');
  var stPct     = document.getElementById(pre + '-pct');
  var stLoaded  = document.getElementById(pre + '-loaded');
  var btnStart  = document.getElementById('btn-start' + num);
  var btnStop   = document.getElementById('btn-stop' + num);
  var queueDiv  = document.getElementById(pre + '-queue');

  if (stState) {
    stState.textContent = ln.state.toUpperCase();
    stState.className = 'value badge ' + ln.state;
  }
  if (stProduct) stProduct.textContent = ln.product || '—';
  if (stTarget)  stTarget.textContent  = fmtUnit(ln.target);
  if (stLoaded)  stLoaded.textContent  = fmtUnit(ln.loaded);

  var pct = Math.min(100, ln.progress).toFixed(1);
  if (stBar) stBar.style.width = pct + '%';
  if (stPct) stPct.textContent = pct + ' %';

  // Show START when queued/idle with queue, show STOP when running
  var hasQueue = ln.queue && ln.queue.length > 0;
  if (btnStart) {
    btnStart.style.display = (ln.state !== 'running' && hasQueue) ? 'block' : 'none';
  }
  if (btnStop) {
    btnStop.style.display = (ln.state === 'running') ? 'block' : 'none';
  }

  // Render queue on main page
  if (queueDiv) {
    renderQueue(queueDiv, num, ln.queue, true);
  }
}

function renderQueue(container, lineNum, queue, showRemove) {
  if (!queue || queue.length === 0) {
    container.innerHTML = '';
    return;
  }
  var html = '';
  for (var i = 0; i < queue.length; i++) {
    html += '<div class="queue-item">';
    html += '<div class="qi-info">';
    html += '<span class="qi-name">' + esc(queue[i].product) + '</span> ';
    html += '<span class="qi-amt">' + fmtUnit(queue[i].litres) + '</span>';
    html += '</div>';
    if (showRemove) {
      html += '<button class="qi-del" onclick="removeQueue(' + lineNum + ',' + i + ')">✕</button>';
    }
    html += '</div>';
  }
  container.innerHTML = html;
}

function updateLoadQueues(l1, l2) {
  var lq1 = document.getElementById('lq1');
  var lq2 = document.getElementById('lq2');
  if (lq1 && l1) {
    if (l1.queue && l1.queue.length > 0) {
      renderQueue(lq1, 1, l1.queue, true);
    } else {
      lq1.innerHTML = '<span class="queue-empty">Empty</span>';
    }
  }
  if (lq2 && l2) {
    if (l2.queue && l2.queue.length > 0) {
      renderQueue(lq2, 2, l2.queue, true);
    } else {
      lq2.innerHTML = '<span class="queue-empty">Empty</span>';
    }
  }
}

function startLine(num) {
  try {
    var r = new XMLHttpRequest();
    r.open('POST', API + '/api/batch/start', true);
    r.setRequestHeader('Content-Type', 'application/json');
    r.send(JSON.stringify({ line: num }));
  } catch(e) {}
}

function stopLine(num) {
  try {
    var r = new XMLHttpRequest();
    r.open('POST', API + '/api/batch/stop', true);
    r.setRequestHeader('Content-Type', 'application/json');
    r.send(JSON.stringify({ line: num }));
  } catch(e) {}
}

function removeQueue(lineNum, index) {
  try {
    var r = new XMLHttpRequest();
    r.open('POST', API + '/api/batch/queue/remove', true);
    r.setRequestHeader('Content-Type', 'application/json');
    r.send(JSON.stringify({ line: lineNum, index: index }));
  } catch(e) {}
}

setInterval(pollStatus, 500);
pollStatus();

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  LOAD PAGE – add to queue
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
var loadSelect = document.getElementById('load-product');
var loadAmount = document.getElementById('load-amount');
var btnQueue   = document.getElementById('btn-queue');
var loadMsg    = document.getElementById('load-msg');

function loadProducts() {
  var prevVal = loadSelect.value;
  try {
    var r = new XMLHttpRequest();
    r.open('GET', API + '/api/products', true);
    r.timeout = 3000;
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        loadSelect.innerHTML = '<option value="">— select product —</option>';
        var arr = d.products || [];
        for (var i = 0; i < arr.length; i++) {
          var o = document.createElement('option');
          o.value = arr[i].id;
          o.textContent = arr[i].name;
          loadSelect.appendChild(o);
        }
        if (prevVal) loadSelect.value = prevVal;
      } catch(e) {}
    };
    r.send();
  } catch(e) {}
}

btnQueue.onclick = function() {
  var pid = parseInt(loadSelect.value);
  var amt = parseFloat(loadAmount.value);
  if (isNaN(pid) || pid < 0) { showMsg(loadMsg, 'Select a product', true); return; }
  if (isNaN(amt) || amt <= 0) { showMsg(loadMsg, 'Enter a valid amount', true); return; }

  var litresForEsp = (convFactor !== 0) ? amt / convFactor : amt;

  try {
    var r = new XMLHttpRequest();
    r.open('POST', API + '/api/batch/queue', true);
    r.setRequestHeader('Content-Type', 'application/json');
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        if (d.ok) {
          showMsg(loadMsg, 'Added to Line ' + selectedLine + ' queue', false);
          loadAmount.value = '';
        } else {
          showMsg(loadMsg, d.error || 'Failed to add', true);
        }
      } catch(e) { showMsg(loadMsg, 'Error', true); }
    };
    r.onerror = function() { showMsg(loadMsg, 'Connection error', true); };
    r.send(JSON.stringify({ line: selectedLine, productId: pid, litres: litresForEsp }));
  } catch(e) {
    showMsg(loadMsg, 'Connection error', true);
  }
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  SETUP PAGE – product CRUD
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
var productList = document.getElementById('product-list');
var productForm = document.getElementById('product-form');
var formTitle   = document.getElementById('form-title');
var pfId   = document.getElementById('pf-id');
var pfName = document.getElementById('pf-name');
var pfCal1 = document.getElementById('pf-cal1');
var pfCal2 = document.getElementById('pf-cal2');
var pfCt1  = document.getElementById('pf-ct1');
var pfCt2  = document.getElementById('pf-ct2');
var setupMsg = document.getElementById('setup-msg');

function loadProductList() {
  try {
    var r = new XMLHttpRequest();
    r.open('GET', API + '/api/products', true);
    r.timeout = 3000;
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        productList.innerHTML = '';
        var arr = d.products || [];
        for (var i = 0; i < arr.length; i++) {
          var div = document.createElement('div');
          div.className = 'product-item';
          div.innerHTML =
            '<div>' +
            '<div class="pi-name">' + esc(arr[i].name) + '</div>' +
            '<div class="pi-info">Cal: ' + arr[i].cal1 + '/' + arr[i].cal2 + ' ppl &middot; Close: ' + arr[i].ct1 + '/' + arr[i].ct2 + ' ms</div>' +
            '</div>' +
            '<div class="pi-actions">' +
            '<button class="pi-btn" onclick="editProduct(' + arr[i].id + ')">Edit</button>' +
            '<button class="pi-btn del" onclick="deleteProduct(' + arr[i].id + ')">Del</button>' +
            '</div>';
          productList.appendChild(div);
        }
      } catch(e) {}
    };
    r.send();
  } catch(e) {
    productList.innerHTML = '<p style="color:var(--danger)">Failed to load</p>';
  }
}

document.getElementById('btn-add-product').onclick = function() {
  pfId.value = -1;
  pfName.value = '';
  pfCal1.value = 450;
  pfCal2.value = 450;
  pfCt1.value = 500;
  pfCt2.value = 500;
  formTitle.textContent = 'New Product';
  productForm.style.display = 'block';
  setupMsg.textContent = '';
};

function editProduct(id) {
  try {
    var r = new XMLHttpRequest();
    r.open('GET', API + '/api/products', true);
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        var arr = d.products || [];
        var p = null;
        for (var i = 0; i < arr.length; i++) { if (arr[i].id === id) p = arr[i]; }
        if (!p) return;
        pfId.value   = id;
        pfName.value = p.name;
        pfCal1.value = p.cal1;
        pfCal2.value = p.cal2;
        pfCt1.value  = p.ct1;
        pfCt2.value  = p.ct2;
        formTitle.textContent = 'Edit Product';
        productForm.style.display = 'block';
        setupMsg.textContent = '';
      } catch(e) {}
    };
    r.send();
  } catch(e) {}
}

function deleteProduct(id) {
  if (!confirm('Delete this product?')) return;
  try {
    var r = new XMLHttpRequest();
    r.open('DELETE', API + '/api/products?id=' + id, true);
    r.onload = function() { loadProductList(); };
    r.send();
  } catch(e) {}
}

document.getElementById('btn-save-product').onclick = function() {
  var id = parseInt(pfId.value);
  var body = {
    name: pfName.value.trim(),
    cal1: parseFloat(pfCal1.value),
    cal2: parseFloat(pfCal2.value),
    ct1:  parseInt(pfCt1.value),
    ct2:  parseInt(pfCt2.value)
  };
  if (!body.name) { showMsg(setupMsg, 'Enter a product name', true); return; }

  try {
    var url = id >= 0 ? API + '/api/products?id=' + id : API + '/api/products';
    var method = id >= 0 ? 'PUT' : 'POST';
    var r = new XMLHttpRequest();
    r.open(method, url, true);
    r.setRequestHeader('Content-Type', 'application/json');
    r.onload = function() {
      try {
        var d = JSON.parse(r.responseText);
        if (d.ok) {
          productForm.style.display = 'none';
          loadProductList();
        } else {
          showMsg(setupMsg, d.error || 'Save failed', true);
        }
      } catch(e) { showMsg(setupMsg, 'Error', true); }
    };
    r.onerror = function() { showMsg(setupMsg, 'Connection error', true); };
    r.send(JSON.stringify(body));
  } catch(e) {
    showMsg(setupMsg, 'Connection error', true);
  }
};

document.getElementById('btn-cancel-product').onclick = function() {
  productForm.style.display = 'none';
};

// ── Helpers ──────────────────────────────────────────
function showMsg(el, text, isErr) {
  el.textContent = text;
  el.className = 'msg ' + (isErr ? 'err' : 'ok');
  setTimeout(function() { el.textContent = ''; el.className = 'msg'; }, 3000);
}

function esc(s) {
  var d = document.createElement('div');
  d.textContent = s;
  return d.innerHTML;
}
