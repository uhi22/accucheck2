#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"
#include "logger.h"
#include "watchdog.h"

#define BUFFER_SIZE 10
#define HTTP_TIMEOUT_MS 5000
/* Max buffered points to flush per loggerTick(). Bounds one loop() iteration to
   FLUSH_MAX_PER_TICK * HTTP_TIMEOUT_MS well under the watchdog timeout. */
#define FLUSH_MAX_PER_TICK 3

/* Connectivity watchdog: when unsent data is piling up (WiFi down or HTTP
   persistently failing, e.g. "HTTP error: -1"), first try to recover on the
   fly by forcing a WiFi re-association; if that does not help within the hard
   window, reset the whole device. A reset stops the discharge (FET pull-down)
   and starts fresh on the next boot. */
#define CONN_SOFT_RECOVER_MS  30000   /* force WiFi re-association after 30 s */
#define CONN_RECOVER_EVERY_MS 20000   /* but not more often than every 20 s */
#define CONN_HARD_RESET_MS    120000  /* full reset after 2 min still failing */

/* Connectivity-health telemetry: send a compact diagnostic record to the server
   every DIAG_INTERVAL_MS so network stability can be tracked on the website. */
#define DIAG_INTERVAL_MS      60000

static MeasurementData buffer[BUFFER_SIZE];
static uint8_t bufHead = 0;
static uint8_t bufCount = 0;
static unsigned long lastReconnectAttempt = 0;
static bool firstRequest = true;
static unsigned long lastOkMs = 0;       /* last successful HTTP 200 */
static unsigned long lastRecoverMs = 0;  /* last forced re-association */
static unsigned long lastDiagMs = 0;     /* last diagnostic record sent */

/* Connectivity-health counters (cumulative since boot; reset by a reboot, which
   is itself visible via the uptime field dropping back to ~0). */
static uint32_t httpOkCount = 0;
static uint32_t httpErrCount = 0;
static uint32_t wifiReconnectCount = 0;  /* full WiFi.begin() after a disconnect */
static uint32_t wifiReassocCount = 0;    /* forced re-association by the watchdog */
static const char* resetReasonStr = "UNKNOWN";  /* set once at boot by the sketch */

static bool sendHttp(const MeasurementData &data) {
  HTTPClient http;
  String url = String("http://") + SERVER_HOST + ":" + String(SERVER_PORT) + SERVER_PATH
    + "?key=" + SERVER_KEY
    + "&t=" + String(data.timestamp_s)
    + "&v=" + String(data.voltage_mV)
    + "&i=" + String(data.current_mA)
    + "&cap=" + String(data.capacity_mAh, 1)
    + "&e=" + String(data.energy_mWh, 1)
    + "&ri=" + String(data.ri_mOhm, 1)
    + "&state=" + String(data.state);

  /* First request after boot triggers a new log file on the server */
  if (firstRequest) {
    url += "&new=1";
    firstRequest = false;
  }

  http.setTimeout(HTTP_TIMEOUT_MS);
  /* setTimeout() only bounds the stream read. Without an explicit connect
     timeout, a GET to an unreachable server (WiFi still associated, but the
     host/internet down) blocks ~19 s on the TCP connect - and several such
     calls in one loop() iteration trip the 30 s task watchdog. */
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.begin(url);
  int code = http.GET();
  http.end();
  /* The GET above can block up to HTTP_TIMEOUT_MS; refresh the watchdog so
     stacked sends within a single loop() iteration cannot starve it. */
  watchdogFeed();

  if (code == 200) {
    httpOkCount++;
    return true;
  } else {
    httpErrCount++;
    Serial.print("HTTP error: ");
    Serial.println(code);
    return false;
  }
}

void loggerInit() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  /* Disable WiFi modem-sleep. In the default power-save mode the radio wakes
     every DTIM beacon (~100 ms), drawing a current burst that appears as
     periodic current peaks / voltage dips in the DCIR samples. A steady draw
     also eases power delivery (fewer brownout-style glitches, possibly fewer
     hangs). The extra constant current is still measured by the shunt and
     cancels in the DCIR delta. */
  WiFi.setSleep(false);
  Serial.print("WiFi connecting");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed, will retry in background.");
  }
  lastOkMs = millis();  /* grace period before the watchdog can escalate */
}

/* Escalate when unsent data is piling up: re-associate WiFi, then reset. */
static void connectivityWatchdog() {
  if (bufCount == 0) return;  /* nothing undelivered -> nothing to worry about */

  unsigned long now = millis();
  unsigned long downFor = now - lastOkMs;

  if (downFor > CONN_HARD_RESET_MS) {
    Serial.println("Connectivity dead for too long - resetting device.");
    Serial.flush();
    digitalWrite(FET_PIN, LOW);  /* stop discharge before reset */
    delay(50);
    ESP.restart();
  } else if (downFor > CONN_SOFT_RECOVER_MS && now - lastRecoverMs > CONN_RECOVER_EVERY_MS) {
    lastRecoverMs = now;
    wifiReassocCount++;
    Serial.println("Connectivity stalled - forcing WiFi re-association.");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

void loggerTick() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 10000) {
      lastReconnectAttempt = now;
      wifiReconnectCount++;
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      Serial.println("WiFi reconnecting...");
    }
    connectivityWatchdog();
    return;
  }

  /* Flush buffered measurements. Each sendHttp() can block up to
     HTTP_TIMEOUT_MS, and the buffer holds up to BUFFER_SIZE points, so a full
     flush of a slow-but-connected server can exceed the task-watchdog timeout.
     Feed the watchdog on every send so the flush cannot trip a reset, and cap
     the number of sends per tick so a single loop() iteration stays bounded
     (any remainder is flushed on the next tick). */
  uint8_t sentThisTick = 0;
  while (bufCount > 0 && sentThisTick < FLUSH_MAX_PER_TICK) {
    watchdogFeed();
    uint8_t idx = (bufHead - bufCount + BUFFER_SIZE) % BUFFER_SIZE;
    if (sendHttp(buffer[idx])) {
      bufCount--;
      sentThisTick++;
      lastOkMs = millis();
    } else {
      break;
    }
  }

  connectivityWatchdog();

  /* Periodic connectivity-health telemetry (only reached while connected). */
  unsigned long nowDiag = millis();
  if (nowDiag - lastDiagMs >= DIAG_INTERVAL_MS) {
    lastDiagMs = nowDiag;
    loggerSendDiagnostics();
  }
}

bool loggerIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void loggerSetResetReason(const char* reason) {
  resetReasonStr = reason;
}

/* Best-effort health record: signal strength, heap, and the cumulative
   connectivity counters. Not buffered - if it fails it is simply skipped; the
   next record (60 s later) carries the updated cumulative counts anyway. */
void loggerSendDiagnostics() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String("http://") + SERVER_HOST + ":" + String(SERVER_PORT) + SERVER_PATH
    + "?key=" + SERVER_KEY
    + "&diag=1"
    + "&up=" + String(millis() / 1000)
    + "&rssi=" + String(WiFi.RSSI())
    + "&heap=" + String(ESP.getFreeHeap())
    + "&minheap=" + String(ESP.getMinFreeHeap())
    + "&ok=" + String(httpOkCount)
    + "&err=" + String(httpErrCount)
    + "&reconn=" + String(wifiReconnectCount)
    + "&reassoc=" + String(wifiReassocCount)
    + "&reset=" + resetReasonStr;

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.begin(url);
  int code = http.GET();
  http.end();
  watchdogFeed();
  if (code != 200) {
    Serial.print("Diag HTTP error: ");
    Serial.println(code);
  }
}

void loggerSendDcirSamples(float ri_mOhm, uint32_t t_s, const char* samples) {
  /* Best-effort: the summary "dcir" point (with ri) is logged separately and
     buffered, so if WiFi is down we simply skip the (large) detail batch.
     Also skip when a backlog exists (bufCount > 0): a normal send is already
     failing, so attempting this large GET would just add a second long block
     in the same loop() iteration. */
  if (WiFi.status() != WL_CONNECTED || bufCount > 0) return;

  HTTPClient http;
  String url = String("http://") + SERVER_HOST + ":" + String(SERVER_PORT) + SERVER_PATH
    + "?key=" + SERVER_KEY
    + "&t=" + String(t_s)
    + "&v=0&i=0"                /* required by add.php validation; ignored for sample requests */
    + "&state=dcir"
    + "&ri=" + String(ri_mOhm, 1)
    + "&samples=" + samples;

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.begin(url);
  int code = http.GET();
  http.end();
  watchdogFeed();
  if (code == 200) {
    httpOkCount++;
  } else {
    httpErrCount++;
    Serial.print("DCIR samples HTTP error: ");
    Serial.println(code);
  }
}

void loggerSend(const MeasurementData &data) {
  if (WiFi.status() == WL_CONNECTED && bufCount == 0) {
    if (sendHttp(data)) {
      lastOkMs = millis();
      return;
    }
  }
  buffer[bufHead] = data;
  bufHead = (bufHead + 1) % BUFFER_SIZE;
  if (bufCount < BUFFER_SIZE) {
    bufCount++;
  }
}
