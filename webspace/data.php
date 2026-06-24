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
