#include "M5CoreWifi.h"

#ifdef WITH_M5_WIFI

#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <helpers/AutoDiscoverRTCClock.h>

extern AutoDiscoverRTCClock rtc_clock;

#ifndef NTP_SERVER
  #define NTP_SERVER "pool.ntp.org"
#endif
#define WIFI_RETRY_MS      20000
#define NTP_RESYNC_MS      (6UL * 60 * 60 * 1000)

static char     cfg_ssid[33] = "";
static char     cfg_pwd[65] = "";
static bool     started = false;
static bool     was_connected = false;
static bool     time_ok = false;
static bool     waiting_ntp = false;
static uint32_t last_begin = 0;
static uint32_t next_check = 0;
static uint32_t last_sync = 0;

static void saveString(const char* key, const char* value) {
  Preferences store;
  store.begin("m5wifi", false);
  store.putString(key, value);
  store.end();
}

static void stopRadio() {
  if (!started) return;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  started = false;
  was_connected = false;
  waiting_ntp = false;
}

static void startRadio() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // modem sleep causes glitches on GPIO36/39 (radio BUSY line, button A)
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg_ssid, cfg_pwd);
  started = true;
  last_begin = millis();
  Serial.printf("WIFI: connecting to '%s'\n", cfg_ssid);
}

namespace m5wifi {

void begin() {
  Preferences store;
  store.begin("m5wifi", true);
  String s = store.getString("ssid", "");
  String p = store.getString("pwd", "");
  bool has_saved = store.isKey("ssid");
  store.end();

  strncpy(cfg_ssid, s.c_str(), sizeof(cfg_ssid) - 1);
  strncpy(cfg_pwd, p.c_str(), sizeof(cfg_pwd) - 1);

#ifdef WIFI_SSID
  if (!has_saved) {   // build-time defaults, until something is saved from the CLI
    strncpy(cfg_ssid, WIFI_SSID, sizeof(cfg_ssid) - 1);
  #ifdef WIFI_PWD
    strncpy(cfg_pwd, WIFI_PWD, sizeof(cfg_pwd) - 1);
  #endif
  }
#else
  (void)has_saved;
#endif

  WiFi.mode(WIFI_OFF);
}

bool configured() { return cfg_ssid[0] != 0; }
bool connected()  { return started && WiFi.status() == WL_CONNECTED; }
bool timeSynced() { return time_ok; }
const char* ssid() { return cfg_ssid; }
int rssi() { return connected() ? WiFi.RSSI() : 0; }

IPAddress localIP() {
  return connected() ? WiFi.localIP() : IPAddress(0, 0, 0, 0);
}

void statusText(char* dest, size_t len) {
  if (!configured()) {
    snprintf(dest, len, "non configure");
  } else if (!started) {
    snprintf(dest, len, "en veille");
  } else if (!connected()) {
    snprintf(dest, len, "connexion...");
  } else {
    snprintf(dest, len, "%s %ddBm", cfg_ssid, (int)WiFi.RSSI());
  }
}

void setSsid(const char* value) {
  strncpy(cfg_ssid, value, sizeof(cfg_ssid) - 1);
  cfg_ssid[sizeof(cfg_ssid) - 1] = 0;
  saveString("ssid", cfg_ssid);
  stopRadio();   // restarted from loop() with the new settings
}

void setPassword(const char* value) {
  strncpy(cfg_pwd, value, sizeof(cfg_pwd) - 1);
  cfg_pwd[sizeof(cfg_pwd) - 1] = 0;
  saveString("pwd", cfg_pwd);
  stopRadio();
}

Client& client() {
  static WiFiClient wifi_client;
  return wifi_client;
}

void loop(bool wanted) {
  uint32_t now = millis();
  if ((int32_t)(now - next_check) < 0) return;
  next_check = now + 500;

  // the OTA update ("start ota") runs its own access point: leave it alone
  wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) return;

  if (!wanted || !configured()) {
    stopRadio();
    return;
  }

  if (!started) {
    startRadio();
    return;
  }

  bool up = (WiFi.status() == WL_CONNECTED);
  if (up && !was_connected) {
    IPAddress ip = WiFi.localIP();
    Serial.printf("WIFI: connected, IP %u.%u.%u.%u, RSSI %d\n", ip[0], ip[1], ip[2], ip[3], (int)WiFi.RSSI());
    configTime(0, 0, NTP_SERVER);   // SNTP in the background, UTC
    waiting_ntp = true;
  } else if (!up && was_connected) {
    Serial.println("WIFI: connection lost");
  }
  was_connected = up;

  if (!up) {
    if (now - last_begin > WIFI_RETRY_MS) {
      WiFi.disconnect();
      WiFi.begin(cfg_ssid, cfg_pwd);
      last_begin = now;
    }
    return;
  }

  if (!waiting_ntp && time_ok && now - last_sync > NTP_RESYNC_MS) {
    waiting_ntp = true;   // SNTP keeps the system clock fresh; copy it over again
  }
  if (waiting_ntp) {
    time_t t = time(nullptr);
    if (t > 1700000000) {
      rtc_clock.setCurrentTime((uint32_t)t);
      time_ok = true;
      waiting_ntp = false;
      last_sync = now;
      Serial.printf("NTP: clock set over WiFi, epoch=%u\n", (unsigned)t);
    }
  }
}

}  // namespace m5wifi

#else   // WITH_M5_WIFI not defined

namespace m5wifi {
  void begin() {}
  void loop(bool) {}
  bool configured() { return false; }
  bool connected() { return false; }
  bool timeSynced() { return false; }
  IPAddress localIP() { return IPAddress(0, 0, 0, 0); }
  int rssi() { return 0; }
  const char* ssid() { return ""; }
  void statusText(char* dest, size_t len) { snprintf(dest, len, "desactive"); }
  void setSsid(const char*) {}
  void setPassword(const char*) {}
}

#endif
