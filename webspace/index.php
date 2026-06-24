<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>accucheck2 — Live Monitor</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
  body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
  h1 { color: #333; }
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
  .controls { margin-bottom: 20px; display: flex; gap: 10px; align-items: center; }
  select { padding: 5px 10px; font-size: 14px; }
  .auto-refresh { font-size: 13px; color: #666; }
  canvas { max-height: 300px; }
</style>
</head>
<body>

<h1>accucheck2 — Live Monitor</h1>

<div class="controls">
  <label>Log file: <select id="fileSelect" onchange="switchFile()"></select></label>
  <span class="auto-refresh" id="refreshInfo">Auto-refresh: active</span>
</div>

<div class="status-bar" id="statusBar">
  <div class="status-item"><div class="label">State</div><div class="value" id="stState">—</div></div>
  <div class="status-item"><div class="label">Voltage</div><div class="value" id="stVoltage">—</div><div class="unit">mV</div></div>
  <div class="status-item"><div class="label">Current</div><div class="value" id="stCurrent">—</div><div class="unit">mA</div></div>
  <div class="status-item"><div class="label">Capacity</div><div class="value" id="stCapacity">—</div><div class="unit">mAh</div></div>
  <div class="status-item"><div class="label">Energy</div><div class="value" id="stEnergy">—</div><div class="unit">mWh</div></div>
  <div class="status-item"><div class="label">DCIR</div><div class="value" id="stDCIR">—</div><div class="unit">m&Omega;</div></div>
  <div class="status-item"><div class="label">Time</div><div class="value" id="stTime">—</div><div class="unit">s</div></div>
</div>

<div class="chart-container"><canvas id="chartVoltage"></canvas></div>
<div class="chart-container"><canvas id="chartCurrent"></canvas></div>
<div class="chart-container"><canvas id="chartCapacity"></canvas></div>
<div class="chart-container"><canvas id="chartEnergy"></canvas></div>
<div class="chart-container"><canvas id="chartDCIR"></canvas></div>

<script>
const COL = { T:0, V:1, I:2, CAP:3, RI:4, E:5, STATE:6 };

let currentFile = null;
let fetchedLines = 0;
let refreshTimer = null;

function makeChart(canvasId, label, color, yLabel) {
  return new Chart(document.getElementById(canvasId), {
    type: 'line',
    data: { labels: [], datasets: [{ label: label, data: [], borderColor: color, borderWidth: 2, pointRadius: 1, fill: false }] },
    options: {
      responsive: true,
      animation: false,
      scales: {
        x: { title: { display: true, text: 'Time (s)' } },
        y: { title: { display: true, text: yLabel } }
      }
    }
  });
}

const chartV = makeChart('chartVoltage', 'Voltage', 'royalblue', 'Voltage (mV)');
const chartI = makeChart('chartCurrent', 'Current', 'crimson', 'Current (mA)');
const chartC = makeChart('chartCapacity', 'Capacity', 'seagreen', 'Capacity (mAh)');
const chartE = makeChart('chartEnergy', 'Energy', 'darkorange', 'Energy (mWh)');
const chartR = makeChart('chartDCIR', 'DCIR', 'purple', 'R_i (mOhm)');

const allCharts = [chartV, chartI, chartC, chartE, chartR];

function clearCharts() {
  allCharts.forEach(c => {
    c.data.labels = [];
    c.data.datasets[0].data = [];
    c.update();
  });
  fetchedLines = 0;
}

function addRows(rows) {
  rows.forEach(row => {
    const t = row[COL.T];
    chartV.data.labels.push(t);
    chartV.data.datasets[0].data.push(row[COL.V]);
    chartI.data.labels.push(t);
    chartI.data.datasets[0].data.push(row[COL.I]);
    chartC.data.labels.push(t);
    chartC.data.datasets[0].data.push(row[COL.CAP]);
    chartE.data.labels.push(t);
    chartE.data.datasets[0].data.push(row[COL.E]);
    if (row[COL.RI] > 0) {
      chartR.data.labels.push(t);
      chartR.data.datasets[0].data.push(row[COL.RI]);
    }
  });
  allCharts.forEach(c => c.update());

  if (rows.length > 0) {
    const last = rows[rows.length - 1];
    document.getElementById('stState').textContent = last[COL.STATE];
    document.getElementById('stVoltage').textContent = last[COL.V];
    document.getElementById('stCurrent').textContent = last[COL.I];
    document.getElementById('stCapacity').textContent = parseFloat(last[COL.CAP]).toFixed(1);
    document.getElementById('stEnergy').textContent = parseFloat(last[COL.E]).toFixed(1);
    if (last[COL.RI] > 0) {
      document.getElementById('stDCIR').textContent = parseFloat(last[COL.RI]).toFixed(1);
    }
    document.getElementById('stTime').textContent = last[COL.T];
  }
}

function fetchData() {
  let url = 'data.php?since=' + fetchedLines;
  if (currentFile) url += '&file=' + encodeURIComponent(currentFile);

  fetch(url)
    .then(r => r.json())
    .then(data => {
      if (data.rows && data.rows.length > 0) {
        addRows(data.rows);
      }
      fetchedLines = data.totalLines || fetchedLines;
    })
    .catch(err => console.error('Fetch error:', err));
}

function loadFileList() {
  fetch('data.php?list=1')
    .then(r => r.json())
    .then(data => {
      const sel = document.getElementById('fileSelect');
      sel.innerHTML = '';
      if (data.files) {
        data.files.forEach((f, idx) => {
          const opt = document.createElement('option');
          opt.value = f;
          opt.textContent = f.replace('log_', '').replace('.txt', '');
          if (idx === 0) opt.textContent += ' (latest)';
          sel.appendChild(opt);
        });
        if (data.files.length > 0 && !currentFile) {
          currentFile = data.files[0];
        }
      }
    })
    .catch(err => console.error('File list error:', err));
}

function switchFile() {
  const sel = document.getElementById('fileSelect');
  currentFile = sel.value;
  clearCharts();
  fetchData();
}

function startRefresh() {
  if (refreshTimer) clearInterval(refreshTimer);
  refreshTimer = setInterval(() => {
    fetchData();
    loadFileList();
  }, 10000);
}

// Initial load
loadFileList();
setTimeout(fetchData, 500);
startRefresh();
</script>

</body>
</html>
