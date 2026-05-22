async function api(path, method='GET', body=null) {
  const opts = { method, headers: {} };
  if(body) { opts.body = JSON.stringify(body); opts.headers['Content-Type'] = 'application/json'; }
  const r = await fetch(path, opts);
  return r.json();
}

async function refresh() {
  const s = await api('/api/status');
  document.getElementById('status').innerText = `Flow1: ${s.flow1_litres.toFixed(3)} L | Flow2: ${s.flow2_litres.toFixed(3)} L | Batching: ${s.batching}`;
}

document.getElementById('start').addEventListener('click', async () => {
  const valve = parseInt(document.getElementById('valve').value);
  const litres = parseFloat(document.getElementById('litres').value);
  await api('/api/start', 'POST', { valve, litres });
});

document.getElementById('stop').addEventListener('click', async () => { await api('/api/stop', 'POST'); });

setInterval(refresh, 1000);
refresh();
