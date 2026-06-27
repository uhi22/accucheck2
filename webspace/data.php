<?php
$DATA_DIR = __DIR__ . '/data';
$CURRENT_FILE = $DATA_DIR . '/current.txt';

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');

// List available log files
if (isset($_GET['list'])) {
    $files = glob($DATA_DIR . '/log_*.txt');
    $result = [];
    foreach ($files as $f) {
        $result[] = basename($f);
    }
    rsort($result);
    echo json_encode(['files' => $result]);
    exit;
}

// Connectivity-health diagnostics log
if (isset($_GET['diaglog'])) {
    $path = $DATA_DIR . '/diag.txt';
    if (!file_exists($path)) {
        echo json_encode(['header' => [], 'rows' => []]);
        exit;
    }
    $lines = file($path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    if (empty($lines)) {
        echo json_encode(['header' => [], 'rows' => []]);
        exit;
    }
    $header = explode(';', $lines[0]);
    // Return only the most recent records (default 500) to keep the page light.
    $limit = isset($_GET['limit']) ? max(1, intval($_GET['limit'])) : 500;
    $start = max(1, count($lines) - $limit);
    $rows = [];
    for ($j = $start; $j < count($lines); $j++) {
        $rows[] = explode(';', $lines[$j]);
    }
    echo json_encode(['header' => $header, 'rows' => $rows]);
    exit;
}

// List available DCIR detail files
if (isset($_GET['dcirlist'])) {
    $files = glob($DATA_DIR . '/dcir_*.txt');
    $result = [];
    foreach ($files as $f) {
        $result[] = basename($f);
    }
    rsort($result);
    echo json_encode(['files' => $result]);
    exit;
}

// DCIR detail samples: one file, or the latest
if (isset($_GET['dcir'])) {
    $req = $_GET['dcir'];
    if ($req === 'latest') {
        $files = glob($DATA_DIR . '/dcir_*.txt');
        rsort($files);
        $path = empty($files) ? null : $files[0];
    } else {
        $fname = basename($req);
        $path = preg_match('/^dcir_[\d_\-]+\.txt$/', $fname) ? ($DATA_DIR . '/' . $fname) : null;
    }
    if ($path === null || !file_exists($path)) {
        echo json_encode(['file' => null, 't_ms' => [], 'v' => [], 'i' => []]);
        exit;
    }
    $lines = file($path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    $t = []; $v = []; $cur = [];
    for ($k = 1; $k < count($lines); $k++) {  // skip header
        $p = explode(';', $lines[$k]);
        if (count($p) >= 3) {
            $t[] = floatval($p[0]);
            $v[] = floatval($p[1]);
            $cur[] = floatval($p[2]);
        }
    }
    echo json_encode(['file' => basename($path), 't_ms' => $t, 'v' => $v, 'i' => $cur]);
    exit;
}

// Determine which file to read
$filename = null;
if (isset($_GET['file'])) {
    $requested = basename($_GET['file']);
    if (preg_match('/^log_\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}\.txt$/', $requested)) {
        $filename = $requested;
    }
}

if ($filename === null && file_exists($CURRENT_FILE)) {
    $filename = trim(file_get_contents($CURRENT_FILE));
}

if ($filename === null || !file_exists($DATA_DIR . '/' . $filename)) {
    echo json_encode(['header' => [], 'rows' => [], 'totalLines' => 0]);
    exit;
}

$lines = file($DATA_DIR . '/' . $filename, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
if (empty($lines)) {
    echo json_encode(['header' => [], 'rows' => [], 'totalLines' => 0]);
    exit;
}

$header = explode(';', $lines[0]);
$totalLines = count($lines) - 1;

// Incremental update support
$since = isset($_GET['since']) ? intval($_GET['since']) : 0;
$startLine = max(1, $since + 1);

$rows = [];
for ($j = $startLine; $j < count($lines); $j++) {
    $fields = explode(';', $lines[$j]);
    $row = [];
    foreach ($fields as $k => $val) {
        if ($k < count($header) && $header[$k] === 'state') {
            $row[] = $val;
        } else {
            $row[] = is_numeric($val) ? floatval($val) : $val;
        }
    }
    $rows[] = $row;
}

echo json_encode([
    'header' => $header,
    'rows' => $rows,
    'totalLines' => $totalLines
]);
