<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>accucheck2 — Live Monitor</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4"></script>
<script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-annotation@3"></script>
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
<div class="chart-container">
  <div style="display:flex;gap:8px;align-items:center;margin-bottom:8px;font-size:13px;color:#666;">
    <span>DCIR detail (high-speed load step):</span>
    <button onclick="dcirStep(1)">&larr; Older</button>
    <select id="dcirSelect" onchange="dcirPick()"></select>
    <button onclick="dcirStep(-1)">Newer &rarr;</button>
  </div>
  <div id="dcirCalc" style="font-size:14px;color:#333;margin-bottom:8px;">R_i: &mdash;</div>
  <canvas id="chartDcirDetail"></canvas>
</div>

<script>
const COL = { T:0, V:1, I:2, CAP:3, RI:4, E:5, STATE:6 };

Chart.register(window['chartjs-plugin-annotation']);

/* sample indices used for the DCIR calculation (set per selected file) */
let dcirRestSet = new Set();
let dcirLoadSet = new Set();
function dcirPointRadius(ctx) {
  const k = ctx.dataIndex;
  return (dcirRestSet.has(k) || dcirLoadSet.has(k)) ? 6 : 1;
}
function dcirPointColor(base) {
  return (ctx) => {
    const k = ctx.dataIndex;
    if (dcirRestSet.has(k)) return 'seagreen';
    if (dcirLoadSet.has(k)) return 'darkorange';
    return base;
  };
}

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

/* DCIR detail: dual-axis (voltage left, current right) vs. time in ms */
const chartDcirDetail = new Chart(document.getElementById('chartDcirDetail'), {
  type: 'line',
  data: { datasets: [
    { label: 'Voltage (mV)', data: [], borderColor: 'royalblue', borderWidth: 2,
      pointRadius: dcirPointRadius, pointBackgroundColor: dcirPointColor('royalblue'), yAxisID: 'y' },
    { label: 'Current (mA)', data: [], borderColor: 'crimson', borderWidth: 2,
      pointRadius: dcirPointRadius, pointBackgroundColor: dcirPointColor('crimson'), yAxisID: 'y1' }
  ]},
  options: {
    responsive: true,
    animation: false,
    scales: {
      x: { type: 'linear', title: { display: true, text: 'Time (ms)' } },
      y: { position: 'left', title: { display: true, text: 'Voltage (mV)' } },
      y1: { position: 'right', title: { display: true, text: 'Current (mA)' }, grid: { drawOnChartArea: false } }
    },
    plugins: { annotation: { annotations: {} } }
  }
});

let dcirFiles = [];      /* newest first */
let dcirSel = 0;         /* index into dcirFiles */
let dcirFollow = true;   /* keep showing the newest as new DCIRs arrive */
let dcirShownFile = null;

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

function renderDcir(filename) {
  if (!filename) return;
  fetch('data.php?dcir=' + encodeURIComponent(filename))
    .then(r => r.json())
    .then(data => {
      if (!data.file) return;
      dcirShownFile = data.file;
      chartDcirDetail.data.datasets[0].data = data.t_ms.map((t, idx) => ({ x: t, y: data.v[idx] }));
      chartDcirDetail.data.datasets[1].data = data.t_ms.map((t, idx) => ({ x: t, y: data.i[idx] }));
      computeDcir(data);
      chartDcirDetail.update();
    })
    .catch(err => console.error('DCIR detail error:', err));
}

function mean(arr, from, count) {
  let s = 0;
  for (let k = from; k < from + count; k++) s += arr[k];
  return s / count;
}

function hLine(scaleID, value, color, text, pos) {
  return {
    type: 'line', scaleID: scaleID, value: value,
    borderColor: color, borderWidth: 1, borderDash: [6, 4],
    label: { display: true, content: text, position: pos,
             backgroundColor: color, color: '#fff', font: { size: 10 }, padding: 3 }
  };
}

/* Recompute R_i from the selected high-speed samples, mirroring the firmware:
   rest = last 5 of the 20-sample rest block (idx 15-19),
   load = last 5 of the 20-sample load block (idx 35-39). See dcir.cpp. */
function computeDcir(data) {
  const info = document.getElementById('dcirCalc');
  const n = data.t_ms.length;
  dcirRestSet = new Set();
  dcirLoadSet = new Set();
  chartDcirDetail.options.plugins.annotation.annotations = {};
  if (n < 40) {
    info.innerHTML = 'R_i: &mdash; (not enough samples)';
    return;
  }
  const restFrom = 15, loadFrom = 35, win = 5;
  const vRest = mean(data.v, restFrom, win);
  const iRest = mean(data.i, restFrom, win);
  const vLoad = mean(data.v, loadFrom, win);
  const iLoad = mean(data.i, loadFrom, win);
  for (let k = restFrom; k < restFrom + win; k++) dcirRestSet.add(k);
  for (let k = loadFrom; k < loadFrom + win; k++) dcirLoadSet.add(k);

  const dV = vRest - vLoad;
  const dI = iLoad - iRest;
  const ri = dV * 1000 / dI;

  info.innerHTML = 'R_i = <b>' + ri.toFixed(1) + ' m&Omega;</b>'
    + ' &nbsp; (V_rest ' + vRest.toFixed(0) + ' &rarr; V_load ' + vLoad.toFixed(0)
    + ' mV, &Delta;V ' + dV.toFixed(1) + ' mV; &nbsp; I_rest ' + iRest.toFixed(0)
    + ' &rarr; I_load ' + iLoad.toFixed(0) + ' mA, &Delta;I ' + dI.toFixed(0) + ' mA)';

  chartDcirDetail.options.plugins.annotation.annotations = {
    vRest: hLine('y',  vRest, 'seagreen',   'V_rest ' + vRest.toFixed(0) + ' mV', 'start'),
    vLoad: hLine('y',  vLoad, 'darkorange', 'V_load ' + vLoad.toFixed(0) + ' mV (ΔV ' + dV.toFixed(1) + ')', 'start'),
    iRest: hLine('y1', iRest, 'seagreen',   'I_rest ' + iRest.toFixed(0) + ' mA', 'end'),
    iLoad: hLine('y1', iLoad, 'darkorange', 'I_load ' + iLoad.toFixed(0) + ' mA (ΔI ' + dI.toFixed(0) + ')', 'end')
  };
}

function updateDcirSelect() {
  const sel = document.getElementById('dcirSelect');
  sel.innerHTML = '';
  dcirFiles.forEach((f, idx) => {
    const opt = document.createElement('option');
    opt.value = f;
    opt.textContent = f.replace('dcir_', '').replace('.txt', '') + (idx === 0 ? ' (latest)' : '');
    sel.appendChild(opt);
  });
  if (dcirFiles.length > 0) sel.selectedIndex = Math.min(dcirSel, dcirFiles.length - 1);
}

function fetchDcirDetail() {
  fetch('data.php?dcirlist=1')
    .then(r => r.json())
    .then(data => {
      dcirFiles = data.files || [];
      if (dcirFiles.length === 0) return;
      if (dcirFollow) {
        dcirSel = 0;
      } else {
        /* keep showing the same file even as newer ones are prepended */
        const idx = dcirFiles.indexOf(dcirShownFile);
        dcirSel = (idx >= 0) ? idx : Math.min(dcirSel, dcirFiles.length - 1);
      }
      updateDcirSelect();
      const target = dcirFiles[dcirSel];
      if (target && target !== dcirShownFile) renderDcir(target);
    })
    .catch(err => console.error('DCIR list error:', err));
}

function dcirPick() {
  dcirSel = document.getElementById('dcirSelect').selectedIndex;
  dcirFollow = (dcirSel === 0);
  renderDcir(dcirFiles[dcirSel]);
}

function dcirStep(dir) {
  /* dir = +1 -> older (higher index), -1 -> newer (lower index) */
  if (dcirFiles.length === 0) return;
  dcirSel = Math.min(Math.max(dcirSel + dir, 0), dcirFiles.length - 1);
  dcirFollow = (dcirSel === 0);
  updateDcirSelect();
  renderDcir(dcirFiles[dcirSel]);
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
    fetchDcirDetail();
  }, 10000);
}

// Initial load
loadFileList();
setTimeout(fetchData, 500);
setTimeout(fetchDcirDetail, 700);
startRefresh();
</script>

</body>
</html>
