#include "M5CoreWifi.h"

#ifdef WITH_M5_WIFI

#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <esp_sntp.h>
#include <helpers/AutoDiscoverRTCClock.h>

extern AutoDiscoverRTCClock rtc_clock;

#ifndef NTP_SERVER
  #define NTP_SERVER "pool.ntp.org"
#endif
#define WIFI_RETRY_MS      12000   // time given to one network before the other one is tried
#define WIFI_SLOTS         2

static char     cfg_ssid[WIFI_SLOTS][33] = { "", "" };
static char     cfg_pwd[WIFI_SLOTS][65] = { "", "" };
static const char* const key_ssid[WIFI_SLOTS] = { "ssid", "ssid2" };
static const char* const key_pwd[WIFI_SLOTS]  = { "pwd", "pwd2" };
static int      cur = 0;                 // slot in use / being tried
static bool     started = false;
static bool     ap_active = false;        // configuration access point running
static bool     was_connected = false;
static bool     time_ok = false;
static volatile bool sntp_done = false;   // set by the SNTP task when a server has answered
static uint32_t last_begin = 0;
static uint32_t next_check = 0;

static void onSntpSync(struct timeval*) {
  sntp_done = true;
}

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
}

// Next slot that holds a network, starting after 'from' (returns 'from' when it is the only one).
static int nextSlot(int from) {
  for (int i = 1; i <= WIFI_SLOTS; i++) {
    int s = (from + i) % WIFI_SLOTS;
    if (cfg_ssid[s][0]) return s;
  }
  return from;
}

static void tryCurrent() {
  WiFi.begin(cfg_ssid[cur], cfg_pwd[cur]);
  last_begin = millis();
  Serial.printf("WIFI: connecting to '%s'\n", cfg_ssid[cur]);
}

static void startRadio() {
  if (!cfg_ssid[cur][0]) cur = nextSlot(cur);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // modem sleep causes glitches on GPIO36/39 (radio BUSY line, button A)
  WiFi.setAutoReconnect(true);
  started = true;
  tryCurrent();
}

namespace m5wifi {

void begin() {
  Preferences store;
  store.begin("m5wifi", true);
  bool has_saved = store.isKey("ssid");
  for (int i = 0; i < WIFI_SLOTS; i++) {
    String s = store.getString(key_ssid[i], "");
    String p = store.getString(key_pwd[i], "");
    strncpy(cfg_ssid[i], s.c_str(), sizeof(cfg_ssid[i]) - 1);
    strncpy(cfg_pwd[i], p.c_str(), sizeof(cfg_pwd[i]) - 1);
  }
  store.end();

#ifdef WIFI_SSID
  if (!has_saved) {   // build-time defaults for slot 0, until something is saved from the CLI
    strncpy(cfg_ssid[0], WIFI_SSID, sizeof(cfg_ssid[0]) - 1);
  #ifdef WIFI_PWD
    strncpy(cfg_pwd[0], WIFI_PWD, sizeof(cfg_pwd[0]) - 1);
  #endif
  }
#else
  (void)has_saved;
#endif

  WiFi.mode(WIFI_OFF);
}

bool configured() { return cfg_ssid[0][0] != 0 || cfg_ssid[1][0] != 0; }
bool connected()  { return started && WiFi.status() == WL_CONNECTED; }
bool timeSynced() { return time_ok; }
const char* ssid() { return cfg_ssid[cur]; }
const char* ssid(int slot) { return (slot >= 0 && slot < WIFI_SLOTS) ? cfg_ssid[slot] : ""; }
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
    snprintf(dest, len, "%s %ddBm", cfg_ssid[cur], (int)WiFi.RSSI());
  }
}

void setSsid(int slot, const char* value) {
  if (slot < 0 || slot >= WIFI_SLOTS) return;
  strncpy(cfg_ssid[slot], value, sizeof(cfg_ssid[slot]) - 1);
  cfg_ssid[slot][sizeof(cfg_ssid[slot]) - 1] = 0;
  saveString(key_ssid[slot], cfg_ssid[slot]);
  stopRadio();   // restarted from loop() with the new settings
}

void setPassword(int slot, const char* value) {
  if (slot < 0 || slot >= WIFI_SLOTS) return;
  strncpy(cfg_pwd[slot], value, sizeof(cfg_pwd[slot]) - 1);
  cfg_pwd[slot][sizeof(cfg_pwd[slot]) - 1] = 0;
  saveString(key_pwd[slot], cfg_pwd[slot]);
  stopRadio();
}

Client& client() {
  static WiFiClient wifi_client;
  return wifi_client;
}

bool startAccessPoint(const char* ap_ssid, const char* ap_pwd) {
  stopRadio();
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  ap_active = WiFi.softAP(ap_ssid, ap_pwd);
  if (!ap_active) WiFi.mode(WIFI_OFF);
  Serial.printf("WIFI: access point '%s' %s\n", ap_ssid, ap_active ? "started" : "FAILED");
  return ap_active;
}

void stopAccessPoint() {
  if (!ap_active) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  ap_active = false;
  started = false;
  next_check = millis() + 1000;
  Serial.println("WIFI: access point stopped");
}

bool accessPointActive() { return ap_active; }
int  accessPointClients() { return ap_active ? WiFi.softAPgetStationNum() : 0; }

void loop(bool wanted) {
  uint32_t now = millis();
  if ((int32_t)(now - next_check) < 0) return;
  next_check = now + 500;

  if (ap_active) return;   // configuration mode owns the radio

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
    sntp_done = false;
    sntp_set_time_sync_notification_cb(onSntpSync);
    configTime(0, 0, NTP_SERVER);   // SNTP in the background (UTC), repeated by the system every hour
  } else if (!up && was_connected) {
    Serial.println("WIFI: connection lost");
  }
  was_connected = up;

  if (!up) {
    if (now - last_begin > WIFI_RETRY_MS) {   // no answer: give the other saved network a chance
      WiFi.disconnect();
      cur = nextSlot(cur);
      tryCurrent();
    }
    return;
  }

  if (sntp_done) {   // a real answer from the NTP server (the system clock alone proves nothing)
    sntp_done = false;
    time_t t = time(nullptr);
    rtc_clock.setCurrentTime((uint32_t)t);
    if (!time_ok) Serial.printf("NTP: clock set over WiFi, epoch=%u\n", (unsigned)t);
    time_ok = true;
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
  const char* ssid(int) { return ""; }
  void statusText(char* dest, size_t len) { snprintf(dest, len, "desactive"); }
  void setSsid(int, const char*) {}
  void setPassword(int, const char*) {}
  bool startAccessPoint(const char*, const char*) { return false; }
  void stopAccessPoint() {}
  bool accessPointActive() { return false; }
  int  accessPointClients() { return 0; }
}

#endif
