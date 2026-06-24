#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"
#include "logger.h"

#define BUFFER_SIZE 10
#define HTTP_TIMEOUT_MS 5000

/* Connectivity watchdog: when unsent data is piling up (WiFi down or HTTP
   persistently failing, e.g. "HTTP error: -1"), first try to recover on the
   fly by forcing a WiFi re-association; if that does not help within the hard
   window, reset the whole device. A reset stops the discharge (FET pull-down)
   and starts fresh on the next boot. */
#define CONN_SOFT_RECOVER_MS  30000   /* force WiFi re-association after 30 s */
#define CONN_RECOVER_EVERY_MS 20000   /* but not more often than every 20 s */
#define CONN_HARD_RESET_MS    120000  /* full reset after 2 min still failing */

static MeasurementData buffer[BUFFER_SIZE];
static uint8_t bufHead = 0;
static uint8_t bufCount = 0;
static unsigned long lastReconnectAttempt = 0;
static bool firstRequest = true;
static unsigned long lastOkMs = 0;       /* last successful HTTP 200 */
static unsigned long lastRecoverMs = 0;  /* last forced re-association */

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
  http.begin(url);
  int code = http.GET();
  http.end();

  if (code == 200) {
    return true;
  } else {
    Serial.print("HTTP error: ");
    Serial.println(code);
    return false;
  }
}

void loggerInit() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      Serial.println("WiFi reconnecting...");
    }
    connectivityWatchdog();
    return;
  }

  /* Flush buffered measurements */
  while (bufCount > 0) {
    uint8_t idx = (bufHead - bufCount + BUFFER_SIZE) % BUFFER_SIZE;
    if (sendHttp(buffer[idx])) {
      bufCount--;
      lastOkMs = millis();
    } else {
      break;
    }
  }

  connectivityWatchdog();
}

bool loggerIsConnected() {
  return WiFi.status() == WL_CONNECTED;
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
