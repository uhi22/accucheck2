<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>accucheck2 — Connectivity Diagnostics</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4"></script>
<style>
  body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
  h1 { color: #333; }
  a { color: #1565c0; }
  .status-bar {
    display: flex; gap: 20px; flex-wrap: wrap;
    background: #fff; padding: 15px; border-radius: 8px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 20px;
  }
  .status-item { text-align: center; }
  .status-item .label { font-size: 12px; color: #888; text-transform: uppercase; }
  .status-item .value { font-size: 24px; font-weight: bold; color: #333; }
  .status-item .unit { font-size: 14px; color: #666; }
  .chart-container {
    background: #fff; padding: 15px; border-radius: 8px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 20px;
  }
  .auto-refresh { font-size: 13px; color: #666; }
  canvas { max-height: 300px; }
  table { border-collapse: collapse; width: 100%; font-size: 13px; background: #fff; }
  th, td { border: 1px solid #ddd; padding: 5px 8px; text-align: right; }
  th { background: #f0f0f0; }
  td.reset, th.reset { text-align: center; }
  tr.alert td { background: #fde8e8; color: #b71c1c; font-weight: bold; }
</style>
</head>
<body>

<h1>accucheck2 — Connectivity Diagnostics</h1>
<p><a href="index.php">&larr; Back to Live Monitor</a>
   &nbsp;|&nbsp; <span class="auto-refresh" id="refreshInfo">Auto-refresh: active</span></p>

<div class="status-bar" id="statusBar">
  <div class="status-item"><div class="label">Last seen</div><div class="value" id="stTime" style="font-size:16px;">—</div></div>
  <div class="status-item"><div class="label">Uptime</div><div class="value" id="stUptime">—</div><div class="unit">s</div></div>
  <div class="status-item"><div class="label">RSSI</div><div class="value" id="stRssi">—</div><div class="unit">dBm</div></div>
  <div class="status-item"><div class="label">Free heap</div><div class="value" id="stHeap">—</div><div class="unit">bytes</div></div>
  <div class="status-item"><div class="label">HTTP ok</div><div class="value" id="stOk">—</div></div>
  <div class="status-item"><div class="label">HTTP err</div><div class="value" id="stErr">—</div></div>
  <div class="status-item"><div class="label">Reconnects</div><div class="value" id="stReconn">—</div></div>
  <div class="status-item"><div class="label">Re-assoc</div><div class="value" id="stReassoc">—</div></div>
  <div class="status-item"><div class="label">Last reset</div><div class="value" id="stReset" style="font-size:16px;">—</div></div>
</div>

<div class="chart-container"><canvas id="chartRssi"></canvas></div>
<div class="chart-container"><canvas id="chartHeap"></canvas></div>
<div class="chart-container"><canvas id="chartErr"></canvas></div>

<div class="chart-container">
  <div style="font-size:13px;color:#666;margin-bottom:8px;">Recent records (newest first)</div>
  <table id="recTable">
    <thead><tr>
      <th>recv_time</th><th>uptime_s</th><th>rssi</th><th>free_heap</th><th>min_heap</th>
      <th>ok</th><th>err</th><th>reconn</th><th>reassoc</th><th class="reset">reset</th>
    </tr></thead>
    <tbody></tbody>
  </table>
</div>

<script>
/* column order matches add.php diag line:
   recv_time;uptime_s;rssi_dbm;free_heap;min_heap;http_ok;http_err;wifi_reconn;wifi_reassoc;reset_reason */
const D = { TIME:0, UP:1, RSSI:2, HEAP:3, MINHEAP:4, OK:5, ERR:6, RECONN:7, REASSOC:8, RESET:9 };

function makeChart(canvasId, datasets, yLabel, y1Label) {
  const scales = {
    x: { title: { display: true, text: 'Received time' }, ticks: { maxRotation: 60, autoSkip: true, maxTicksLimit: 12 } },
    y: { position: 'left', title: { display: true, text: yLabel } }
  };
  if (y1Label) scales.y1 = { position: 'right', title: { display: true, text: y1Label }, grid: { drawOnChartArea: false } };
  return new Chart(document.getElementById(canvasId), {
    type: 'line',
    data: { labels: [], datasets: datasets },
    options: { responsive: true, animation: false, scales: scales }
  });
}

const chartRssi = makeChart('chartRssi',
  [{ label: 'RSSI (dBm)', data: [], borderColor: 'teal', borderWidth: 2, pointRadius: 1, fill: false }],
  'RSSI (dBm)');
const chartHeap = makeChart('chartHeap',
  [{ label: 'Free heap', data: [], borderColor: 'royalblue', borderWidth: 2, pointRadius: 1, fill: false },
   { label: 'Min heap (since boot)', data: [], borderColor: 'crimson', borderWidth: 1, pointRadius: 1, borderDash: [4,3], fill: false }],
  'Heap (bytes)');
/* HTTP errors get their own right-hand axis: cumulative ok climbs fast and would
   otherwise squash the error line flat against zero, hiding real failures. The
   error line is stepped + beginAtZero so each failure shows as a clear jump. */
const chartErr = makeChart('chartErr',
  [{ label: 'HTTP errors (right axis)', data: [], borderColor: 'crimson', borderWidth: 2, pointRadius: 2, stepped: true, fill: false, yAxisID: 'y1' },
   { label: 'HTTP ok (left axis)', data: [], borderColor: 'seagreen', borderWidth: 1, pointRadius: 1, fill: false, yAxisID: 'y' }],
  'HTTP ok (cumulative)', 'HTTP errors (cumulative)');
chartErr.options.scales.y.beginAtZero = true;
chartErr.options.scales.y1.beginAtZero = true;
chartErr.options.scales.y1.ticks = { precision: 0 };

function num(x) { return parseFloat(x); }

function render(rows) {
  const labels = rows.map(r => r[D.TIME]);
  chartRssi.data.labels = labels;
  chartRssi.data.datasets[0].data = rows.map(r => num(r[D.RSSI]));

  chartHeap.data.labels = labels;
  chartHeap.data.datasets[0].data = rows.map(r => num(r[D.HEAP]));
  chartHeap.data.datasets[1].data = rows.map(r => num(r[D.MINHEAP]));

  chartErr.data.labels = labels;
  chartErr.data.datasets[0].data = rows.map(r => num(r[D.ERR]));
  chartErr.data.datasets[1].data = rows.map(r => num(r[D.OK]));

  [chartRssi, chartHeap, chartErr].forEach(c => c.update());

  if (rows.length > 0) {
    const last = rows[rows.length - 1];
    document.getElementById('stTime').textContent    = last[D.TIME];
    document.getElementById('stUptime').textContent  = last[D.UP];
    document.getElementById('stRssi').textContent    = last[D.RSSI];
    document.getElementById('stHeap').textContent    = last[D.HEAP];
    document.getElementById('stOk').textContent      = last[D.OK];
    document.getElementById('stErr').textContent     = last[D.ERR];
    document.getElementById('stReconn').textContent  = last[D.RECONN];
    document.getElementById('stReassoc').textContent = last[D.REASSOC];
    document.getElementById('stReset').textContent   = last[D.RESET];
  }

  /* recent records table, newest first, last 50 */
  const tbody = document.querySelector('#recTable tbody');
  tbody.innerHTML = '';
  const recent = rows.slice(-50).reverse();
  recent.forEach(r => {
    const tr = document.createElement('tr');
    /* highlight rows where the device had just rebooted for a bad reason */
    if (r[D.RESET] && !['POWERON', 'SW', 'UNKNOWN', ''].includes(r[D.RESET]) && num(r[D.UP]) < 120) {
      tr.className = 'alert';
    }
    [D.TIME, D.UP, D.RSSI, D.HEAP, D.MINHEAP, D.OK, D.ERR, D.RECONN, D.REASSOC, D.RESET].forEach(c => {
      const td = document.createElement('td');
      if (c === D.RESET) td.className = 'reset';
      td.textContent = r[c];
      tr.appendChild(td);
    });
    tbody.appendChild(tr);
  });
}

function fetchDiag() {
  fetch('data.php?diaglog=1')
    .then(r => r.json())
    .then(data => { if (data.rows) render(data.rows); })
    .catch(err => console.error('Diag fetch error:', err));
}

fetchDiag();
setInterval(fetchDiag, 30000);
</script>

</body>
</html>
