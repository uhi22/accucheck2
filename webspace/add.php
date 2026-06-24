<?php
require_once __DIR__ . '/secret.php';

$ALLOWED_STATES = ['idle', 'discharging', 'dcir', 'done', 'error'];
$DATA_DIR = __DIR__ . '/data';
$CURRENT_FILE = $DATA_DIR . '/current.txt';

// Check shared secret
if (!isset($_GET['key']) || $_GET['key'] !== $SHARED_SECRET) {
    http_response_code(403);
    echo "Forbidden";
    exit;
}

// Read and validate parameters
$t     = isset($_GET['t'])     ? $_GET['t']     : null;
$v     = isset($_GET['v'])     ? $_GET['v']     : null;
$i     = isset($_GET['i'])     ? $_GET['i']     : null;
$cap   = isset($_GET['cap'])   ? $_GET['cap']   : null;
$e     = isset($_GET['e'])     ? $_GET['e']     : null;
$ri    = isset($_GET['ri'])    ? $_GET['ri']    : null;
$state = isset($_GET['state']) ? $_GET['state'] : null;

if ($t === null || $v === null || $i === null || $state === null) {
    http_response_code(400);
    echo "Missing parameters";
    exit;
}

if (!is_numeric($t) || !is_numeric($v) || !is_numeric($i)) {
    http_response_code(400);
    echo "Invalid numeric parameters";
    exit;
}

if (!in_array($state, $ALLOWED_STATES)) {
    http_response_code(400);
    echo "Invalid state";
    exit;
}

$cap = is_numeric($cap) ? $cap : '0';
$e   = is_numeric($e)   ? $e   : '0';
$ri  = is_numeric($ri)  ? $ri  : '0';

// Determine log file
$logFile = null;
if (file_exists($CURRENT_FILE)) {
    $logFile = trim(file_get_contents($CURRENT_FILE));
    if (!file_exists($DATA_DIR . '/' . $logFile)) {
        $logFile = null;
    }
}

/* Create new log file on ESP32 boot (new=1) or if no file exists yet */
$isNew = isset($_GET['new']) && $_GET['new'] === '1';
if ($logFile === null || $isNew) {
    $logFile = 'log_' . date('Y-m-d_H-i-s') . '.txt';
    file_put_contents($CURRENT_FILE, $logFile);
    $header = "timestamp;voltage_mV;current_mA;capacity_mAh;dcir_mOhm;energy_mWh;state\n";
    file_put_contents($DATA_DIR . '/' . $logFile, $header);
}

// Append data line
$line = "$t;$v;$i;$cap;$ri;$e;$state\n";
file_put_contents($DATA_DIR . '/' . $logFile, $line, FILE_APPEND | LOCK_EX);

// Handle DCIR samples if present
if ($state === 'dcir' && isset($_GET['samples'])) {
    $dcirFile = 'dcir_' . date('Y-m-d_H-i-s') . '.txt';
    $dcirData = "t_ms;voltage_mV;current_mA\n";
    $samples = explode(',', $_GET['samples']);
    foreach ($samples as $sample) {
        $parts = explode(':', $sample);
        if (count($parts) === 3 && is_numeric($parts[0]) && is_numeric($parts[1]) && is_numeric($parts[2])) {
            $dcirData .= $parts[0] . ';' . $parts[1] . ';' . $parts[2] . "\n";
        }
    }
    file_put_contents($DATA_DIR . '/' . $dcirFile, $dcirData);
}

echo "OK";
