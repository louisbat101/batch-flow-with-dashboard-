/* ══════════════════════════════════════════════════════
   Intelligent Batching Control UI
   Pulse-based batching with adaptive shutoff
   ══════════════════════════════════════════════════════ */

var batchingSystem = {
  // Configuration
  product: {
    name: 'Acid',
    pulsesPerGallon: 450,
    pulsesPerLiter: 119,
    valveCloseTime: 0.5  // seconds
  },
  
  // Shutoff control
  shutoffMode: 1,  // 0 = manual, 1 = intelligent
  manualOffset: 0.5,  // gallons
  adaptiveLearning: true,
  learningGain: 0.15,
  
  // Current batch state
  batch: {
    state: 'IDLE',  // IDLE, PRIMING, RUNNING, SHUTOFF, DONE
    targetGallons: 5.0,
    currentVolume: 0.0,
    currentFlowRateGPM: 0.0,
    dynamicOffset: 0.5,
    shutoffPoint: 0.0,
    pulsesReceived: 0,
    progress: 0,
    error: 0,
    learnCorrection: 0.0
  },
  
  // History
  batches: []
};

// ═══════════════════════════════════════════════════════
// BATCHING CALCULATIONS
// ═══════════════════════════════════════════════════════

function calculateTargetPulses(gallons) {
  return gallons * batchingSystem.product.pulsesPerGallon;
}

function calculateShutoffPoint() {
  var batch = batchingSystem.batch;
  var product = batchingSystem.product;
  
  if (batchingSystem.shutoffMode === 0) {
    // Manual mode: fixed offset
    batch.dynamicOffset = batchingSystem.manualOffset;
  } else {
    // Intelligent mode: dynamic offset from flow rate
    if (batch.currentFlowRateGPM > 0.01) {
      var flowRateGPS = batch.currentFlowRateGPM / 60.0;
      var dynamicOffsetGallons = flowRateGPS * product.valveCloseTime;
      
      // Add adaptive learning correction
      if (batchingSystem.adaptiveLearning) {
        dynamicOffsetGallons += batch.learnCorrection;
      }
      
      batch.dynamicOffset = dynamicOffsetGallons;
    }
  }
  
  // Shutoff point = Target - Offset (in gallons)
  batch.shutoffPoint = batch.targetGallons - batch.dynamicOffset;
  
  return batch.shutoffPoint;
}

function updateFlowRate(pulseCount, timeMs) {
  var batch = batchingSystem.batch;
  var product = batchingSystem.product;
  
  if (timeMs > 100 && pulseCount > 0) {
    var deltaSeconds = timeMs / 1000.0;
    var gallonsDispensed = pulseCount / product.pulsesPerGallon;
    batch.currentFlowRateGPM = (gallonsDispensed / deltaSeconds) * 60.0;
  }
}

function shouldShutoff() {
  var batch = batchingSystem.batch;
  var targetPulses = calculateTargetPulses(batch.targetGallons);
  var shutoffPulses = calculateTargetPulses(batch.shutoffPoint);
  
  return batch.pulsesReceived >= shutoffPulses;
}

function completeBatch(actualVolume) {
  var batch = batchingSystem.batch;
  
  // Calculate error
  batch.error = actualVolume - batch.targetGallons;
  
  // Update adaptive learning
  if (batchingSystem.adaptiveLearning) {
    batch.learnCorrection += (batch.error * batchingSystem.learningGain);
    batch.learnCorrection = Math.max(-2.0, Math.min(2.0, batch.learnCorrection));
  }
  
  // Save to history
  batchingSystem.batches.push({
    timestamp: new Date(),
    target: batch.targetGallons,
    actual: actualVolume,
    error: batch.error,
    flowRate: batch.currentFlowRateGPM,
    offset: batch.dynamicOffset,
    pulses: batch.pulsesReceived
  });
  
  batch.state = 'DONE';
}

// ═══════════════════════════════════════════════════════
// UI RENDERING
// ═══════════════════════════════════════════════════════

function renderBatchingControl() {
  var batch = batchingSystem.batch;
  var product = batchingSystem.product;
  var html = '';
  
  // Target settings
  html += '<div class="batching-section">';
  html += '<h3>⚙️ Batch Setup</h3>';
  html += '<div class="control-group">';
  html += '<label>Target Volume (Gallons):</label>';
  html += '<input type="number" id="target-gallons" value="' + batch.targetGallons + '" step="0.1" min="0.1" max="100" />';
  html += '</div>';
  
  html += '<div class="control-group">';
  html += '<label>Product:</label>';
  html += '<select id="product-select">';
  html += '<option value="acid">Acid (' + product.pulsesPerGallon + ' PPG)</option>';
  html += '<option value="caustic">Caustic (480 PPG)</option>';
  html += '<option value="water">Water (420 PPG)</option>';
  html += '</select>';
  html += '</div>';
  
  html += '<button onclick="startBatch()" class="btn-primary">START BATCH</button>';
  html += '</div>';
  
  // Shutoff mode selection
  html += '<div class="batching-section">';
  html += '<h3>🎯 Shutoff Control</h3>';
  html += '<div class="mode-selector">';
  html += '<label><input type="radio" name="shutoff-mode" value="0" onchange="setShutoffMode(0)" ' + 
    (batchingSystem.shutoffMode === 0 ? 'checked' : '') + ' /> Manual (Fixed Offset)</label>';
  html += '<label><input type="radio" name="shutoff-mode" value="1" onchange="setShutoffMode(1)" ' + 
    (batchingSystem.shutoffMode === 1 ? 'checked' : '') + ' /> Intelligent (Dynamic)</label>';
  html += '</div>';
  
  if (batchingSystem.shutoffMode === 0) {
    html += '<div class="control-group">';
    html += '<label>Manual Offset (Gallons):</label>';
    html += '<input type="number" id="manual-offset" value="' + batchingSystem.manualOffset + '" step="0.01" min="0" max="5" />';
    html += '</div>';
  } else {
    html += '<div class="info-box">';
    html += '<p><strong>Intelligent Mode Active</strong></p>';
    html += '<p>Shutoff point calculated from: Flow Rate × Valve Close Time</p>';
    html += '</div>';
  }
  
  html += '</div>';
  
  // Adaptive learning
  html += '<div class="batching-section">';
  html += '<h3>🧠 Adaptive Learning</h3>';
  html += '<div class="control-group">';
  html += '<label><input type="checkbox" id="adaptive-learning" ' + 
    (batchingSystem.adaptiveLearning ? 'checked' : '') + ' onchange="toggleAdaptiveLearning()" /> Enable Adaptive Learning</label>';
  html += '</div>';
  
  if (batchingSystem.adaptiveLearning) {
    html += '<div class="control-group">';
    html += '<label>Learning Gain (0.0 - 1.0):</label>';
    html += '<input type="range" id="learning-gain" min="0" max="1" step="0.05" value="' + batchingSystem.learningGain + '" onchange="setLearningGain(this.value)" />';
    html += '<span id="gain-value">' + batchingSystem.learningGain.toFixed(2) + '</span>';
    html += '</div>';
    
    html += '<div class="metric">';
    html += '<span class="label">Learn Correction:</span>';
    html += '<span class="value ' + (batch.learnCorrection > 0 ? 'positive' : 'negative') + '">' + 
      batch.learnCorrection.toFixed(3) + ' gal</span>';
    html += '</div>';
  }
  
  html += '</div>';
  
  document.getElementById('batching-controls').innerHTML = html;
}

function renderBatchProgress() {
  var batch = batchingSystem.batch;
  var html = '';
  
  // Progress bar
  var progressPercent = (batch.currentVolume / batch.targetGallons) * 100;
  progressPercent = Math.min(100, progressPercent);
  
  html += '<div class="batch-progress">';
  html += '<div class="progress-header">';
  html += '<span>Batch Progress</span>';
  html += '<span class="status-badge ' + batch.state.toLowerCase() + '">' + batch.state + '</span>';
  html += '</div>';
  
  html += '<div class="progress-bar-container">';
  html += '<div class="progress-bar" style="width: ' + progressPercent + '%"></div>';
  html += '</div>';
  
  html += '<div class="progress-metrics">';
  html += '<div class="metric">';
  html += '<span class="label">Current Volume:</span>';
  html += '<span class="value">' + batch.currentVolume.toFixed(2) + ' / ' + batch.targetGallons.toFixed(2) + ' gal</span>';
  html += '</div>';
  
  html += '<div class="metric">';
  html += '<span class="label">Flow Rate:</span>';
  html += '<span class="value">' + batch.currentFlowRateGPM.toFixed(1) + ' GPM</span>';
  html += '</div>';
  
  html += '<div class="metric">';
  html += '<span class="label">Shutoff Point:</span>';
  html += '<span class="value">' + batch.shutoffPoint.toFixed(2) + ' gal</span>';
  html += '</div>';
  
  html += '<div class="metric">';
  html += '<span class="label">Dynamic Offset:</span>';
  html += '<span class="value">' + batch.dynamicOffset.toFixed(3) + ' gal</span>';
  html += '</div>';
  
  if (batch.state === 'DONE' && batch.error !== 0) {
    var errorPercent = (Math.abs(batch.error) / batch.targetGallons) * 100;
    html += '<div class="metric">';
    html += '<span class="label">Batch Error:</span>';
    html += '<span class="value ' + (batch.error > 0 ? 'over' : 'under') + '">' + 
      (batch.error > 0 ? '+' : '') + batch.error.toFixed(3) + ' gal (' + errorPercent.toFixed(1) + '%)</span>';
    html += '</div>';
  }
  
  html += '</div>';
  html += '</div>';
  
  document.getElementById('batch-progress').innerHTML = html;
}

function renderBatchHistory() {
  var html = '<h3>Batch History</h3>';
  html += '<table class="history-table">';
  html += '<tr><th>#</th><th>Target</th><th>Actual</th><th>Error</th><th>Flow (GPM)</th><th>Offset</th><th>Status</th></tr>';
  
  batchingSystem.batches.slice(-10).reverse().forEach(function(b, i) {
    var errorStatus = Math.abs(b.error) < 0.05 ? '✅ PASS' : '⚠️ FAIL';
    var errorPercent = (Math.abs(b.error) / b.target) * 100;
    html += '<tr>';
    html += '<td>' + (batchingSystem.batches.length - i) + '</td>';
    html += '<td>' + b.target.toFixed(2) + '</td>';
    html += '<td>' + b.actual.toFixed(2) + '</td>';
    html += '<td class="' + (b.error > 0 ? 'over' : 'under') + '">' + (b.error > 0 ? '+' : '') + b.error.toFixed(3) + '</td>';
    html += '<td>' + b.flowRate.toFixed(1) + '</td>';
    html += '<td>' + b.offset.toFixed(3) + '</td>';
    html += '<td>' + errorStatus + '</td>';
    html += '</tr>';
  });
  
  html += '</table>';
  document.getElementById('batch-history').innerHTML = html;
}

// ═══════════════════════════════════════════════════════
// CONTROL FUNCTIONS
// ═══════════════════════════════════════════════════════

function startBatch() {
  var targetGallons = parseFloat(document.getElementById('target-gallons').value);
  
  if (targetGallons <= 0) {
    alert('Enter a valid target volume');
    return;
  }
  
  batchingSystem.batch.targetGallons = targetGallons;
  batchingSystem.batch.currentVolume = 0;
  batchingSystem.batch.pulsesReceived = 0;
  batchingSystem.batch.state = 'PRIMING';
  batchingSystem.batch.progress = 0;
  
  // Simulate batch start
  setTimeout(function() {
    batchingSystem.batch.state = 'RUNNING';
    simulateBatch();
  }, 1000);
  
  renderBatchProgress();
}

function setShutoffMode(mode) {
  batchingSystem.shutoffMode = parseInt(mode);
  renderBatchingControl();
}

function toggleAdaptiveLearning() {
  batchingSystem.adaptiveLearning = document.getElementById('adaptive-learning').checked;
  renderBatchingControl();
}

function setLearningGain(value) {
  batchingSystem.learningGain = parseFloat(value);
  document.getElementById('gain-value').textContent = batchingSystem.learningGain.toFixed(2);
}

function simulateBatch() {
  var batch = batchingSystem.batch;
  
  if (batch.state !== 'RUNNING') return;
  
  // Simulate pulse reception
  batch.pulsesReceived += Math.floor(Math.random() * 50) + 20;
  batch.currentVolume = batch.pulsesReceived / batchingSystem.product.pulsesPerGallon;
  
  // Update flow rate
  batch.currentFlowRateGPM = 2.5 + (Math.random() * 0.5);
  
  // Calculate shutoff point
  calculateShutoffPoint();
  
  // Check if should shutoff
  if (shouldShutoff()) {
    batch.state = 'SHUTOFF';
    setTimeout(function() {
      // Simulate final volume after shutoff
      var actualVolume = batch.currentVolume + (Math.random() * 0.1);
      completeBatch(actualVolume);
      renderBatchProgress();
      renderBatchHistory();
    }, 500);
  } else {
    renderBatchProgress();
    setTimeout(simulateBatch, 200);
  }
}

// ═══════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════

function initBatchingUI() {
  renderBatchingControl();
  renderBatchProgress();
  renderBatchHistory();
}
