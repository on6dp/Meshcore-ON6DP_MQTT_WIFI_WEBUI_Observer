#ifdef M5CORE_UI

#include "M5CoreUITask.h"
#include "MyMesh.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <M5CoreLan.h>

extern MyMesh the_mesh;

// ---- behaviour ----
#ifndef M5UI_AUTO_OFF_SECS
  #define M5UI_AUTO_OFF_SECS   300     // backlight off after 5 minutes without a key press (0 = never)
#endif
#define LONG_PRESS_MILLIS      800
#define BOOT_SCREEN_MILLIS     3000
#define TOAST_MILLIS           2500
#define NUM_PAGES              4

// ---- layout (320x240, text size 2 = 12x16 pixels per character) ----
#define HEADER_H               26
#define ROW_Y0                 36
#define ROW_STEP               21
#define LABEL_X                6
#define VALUE_X                118
#define FOOTER_Y               212
#define FOOTER_H               26

// ---- colours (RGB565) ----
#define COL_GREEN              0x07E0
#define COL_RED                0xF800
#define COL_BTN                0x212A   // dark blue (a colour the 8-bit frame buffer renders exactly)

static const uint8_t brightness_steps[] = { 160, 50 };

M5CoreUITask::M5CoreUITask(mesh::MainBoard& board, M5CoreDisplay& display)
  : _board(&board), _display(&display),
    _btnA(PIN_CORE_BTN_A, LONG_PRESS_MILLIS, true, false, false),
    _btnB(PIN_CORE_BTN_B, LONG_PRESS_MILLIS, true, false, false),
    _btnC(PIN_CORE_BTN_C, LONG_PRESS_MILLIS, true, false, false) {
  _version[0] = 0;
  _build_date[0] = 0;
  _toast[0] = 0;
}

void M5CoreUITask::begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version) {
  _node_prefs = node_prefs;
  _started_at = _last_activity = millis();

  // v1.2.3-abcdef -> v1.2.3
  strncpy(_version, firmware_version, sizeof(_version) - 1);
  _version[sizeof(_version) - 1] = 0;
  char* dash = strchr(_version, '-');
  if (dash) *dash = 0;
  strncpy(_build_date, build_date, sizeof(_build_date) - 1);
  _build_date[sizeof(_build_date) - 1] = 0;

  _btnA.begin();
  _btnB.begin();
  _btnC.begin();

  _display->turnOn();
}

void M5CoreUITask::showToast(const char* msg) {
  strncpy(_toast, msg, sizeof(_toast) - 1);
  _toast[sizeof(_toast) - 1] = 0;
  _toast_until = millis() + TOAST_MILLIS;
}

// ------------------------------------------------------------------ buttons

void M5CoreUITask::handleButtons() {
  int a = _btnA.check();
  int b = _btnB.check();
  int c = _btnC.check();
  if (a == BUTTON_EVENT_NONE && b == BUTTON_EVENT_NONE && c == BUTTON_EVENT_NONE) return;

  _last_activity = millis();
  _next_refresh = 0;   // redraw at once

  if (!_display->isOn()) {   // first press only wakes the screen up
    if (_bright_step >= sizeof(brightness_steps)) _bright_step = sizeof(brightness_steps) - 1;
    _display->turnOn();
    return;
  }

  // A: page navigation
  if (a == BUTTON_EVENT_CLICK) {
    _page = (_page + 1) % NUM_PAGES;
  } else if (a == BUTTON_EVENT_LONG_PRESS) {
    _page = (_page + NUM_PAGES - 1) % NUM_PAGES;
  }

  // B: send an advertisement
  if (b == BUTTON_EVENT_CLICK) {
    the_mesh.sendSelfAdvertisement(300, false);
    showToast("Advert local envoye");
  } else if (b == BUTTON_EVENT_LONG_PRESS) {
    the_mesh.sendSelfAdvertisement(300, true);
    showToast("Advert FLOOD envoye");
  }

  // C: backlight / reboot
  if (c == BUTTON_EVENT_CLICK) {
    _bright_step++;
    if (_bright_step > sizeof(brightness_steps)) _bright_step = 0;
    if (_bright_step == sizeof(brightness_steps)) {
      _display->turnOff();
    } else {
      _display->setBrightness(brightness_steps[_bright_step]);
      _display->turnOn();
    }
  } else if (c == BUTTON_EVENT_LONG_PRESS) {
    showToast("Redemarrage...");
    _reboot_at = millis() + 1200;
  }
}

// ------------------------------------------------------------------ drawing helpers

static void formatUptime(char* dest, size_t len) {
  uint32_t secs = (uint32_t)(esp_timer_get_time() / 1000000LL);
  uint32_t days = secs / 86400;
  uint32_t hours = (secs % 86400) / 3600;
  uint32_t mins = (secs % 3600) / 60;
  if (days > 0) {
    snprintf(dest, len, "%uj %02uh%02u", (unsigned)days, (unsigned)hours, (unsigned)mins);
  } else {
    snprintf(dest, len, "%02uh%02um%02us", (unsigned)hours, (unsigned)mins, (unsigned)(secs % 60));
  }
}

void M5CoreUITask::drawHeader(const char* page_title) {
  char tmp[40];

  _display->setColor(UIColor::title_bkg);
  _display->fillRect(0, 0, _display->width(), HEADER_H);

  _display->setTextSize(2);
  _display->setColor(UIColor::title_txt);
  char name[40];
  _display->translateUTF8ToBlocks(name, _node_prefs->node_name, sizeof(name));
  _display->drawTextEllipsized(LABEL_X, 5, 180, name);

  snprintf(tmp, sizeof(tmp), "%s %d/%d", page_title, _page + 1, NUM_PAGES);
  _display->drawTextRightAlign(_display->width() - LABEL_X, 5, tmp);
}

void M5CoreUITask::drawFooter() {
  static const char* labels[3] = { "PAGE", "ADVERT", "ECRAN" };
  static const int centers[3] = { 65, 160, 255 };   // above the three physical buttons

  _display->setTextSize(2);
  for (int i = 0; i < 3; i++) {
    _display->setColor(COL_BTN);
    _display->fillRoundRect(centers[i] - 46, FOOTER_Y, 92, FOOTER_H, 8);
    _display->setColor(UIColor::secondary_txt);
    _display->drawRoundRect(centers[i] - 46, FOOTER_Y, 92, FOOTER_H, 8);
    _display->setColor(UIColor::title_txt);
    _display->drawTextCentered(centers[i], FOOTER_Y + 5, labels[i]);
  }
}

void M5CoreUITask::drawRow(int row, const char* label, const char* value, ColorVal value_color) {
  int y = ROW_Y0 + row * ROW_STEP;
  _display->setTextSize(2);
  _display->setColor(UIColor::secondary_txt);
  _display->setCursor(LABEL_X, y);
  _display->print(label);
  _display->setColor(value_color);
  _display->drawTextEllipsized(VALUE_X, y, _display->width() - VALUE_X - 4, value);
}

void M5CoreUITask::drawRow(int row, const char* label, const char* value) {
  drawRow(row, label, value, UIColor::primary_txt);
}

// ------------------------------------------------------------------ pages

void M5CoreUITask::pageBoot() {
  _display->setTextSize(3);
  _display->setColor(UIColor::corp_blue);
  _display->drawTextCentered(_display->width() / 2, 50, "MeshCore");
  _display->setTextSize(2);
  _display->setColor(UIColor::primary_txt);
  _display->drawTextCentered(_display->width() / 2, 100, "< Repeater >");
  _display->drawTextCentered(_display->width() / 2, 130, _version);
  _display->setColor(UIColor::secondary_txt);
  _display->drawTextCentered(_display->width() / 2, 156, _build_date);
  _display->drawTextCentered(_display->width() / 2, 196, "M5Stack Core + LoRa-1262");
}

void M5CoreUITask::pageStatus() {
  char tmp[48];
  drawHeader("Etat");

  snprintf(tmp, sizeof(tmp), "%.3f MHz", _node_prefs->freq);
  drawRow(0, "Frequence", tmp);

  snprintf(tmp, sizeof(tmp), "SF%d BW%.1f CR%d", (int)_node_prefs->sf, _node_prefs->bw, (int)_node_prefs->cr);
  drawRow(1, "Modulat.", tmp);

  snprintf(tmp, sizeof(tmp), "%d dBm", (int)_node_prefs->tx_power_dbm);
  drawRow(2, "Puissance", tmp);

  formatUptime(tmp, sizeof(tmp));
  drawRow(3, "Uptime", tmp);

  uint32_t now = rtc_clock.getCurrentTime();
  if (now > 1700000000UL) {
    uint32_t t = now % 86400;
    snprintf(tmp, sizeof(tmp), "%02u:%02u:%02u UTC", (unsigned)(t / 3600), (unsigned)((t % 3600) / 60), (unsigned)(t % 60));
    drawRow(4, "Heure", tmp);
  } else {
    drawRow(4, "Heure", "non reglee", UIColor::warning_txt);
  }

  if (m5lan::hasIP()) {
    IPAddress ip = m5lan::localIP();
    snprintf(tmp, sizeof(tmp), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    drawRow(5, "LAN", tmp, COL_GREEN);
  } else {
    drawRow(5, "LAN", m5lan::statusText(), UIColor::warning_txt);
  }

#ifdef WITH_MQTT_BRIDGE
  if (the_mesh.getMqttBridge().isConnected()) {
    drawRow(6, "MQTT", "connecte", COL_GREEN);
  } else {
    drawRow(6, "MQTT", "deconnecte", UIColor::warning_txt);
  }
#else
  drawRow(6, "MQTT", "desactive", UIColor::secondary_txt);
#endif

  drawRow(7, "Version", _version);
}

void M5CoreUITask::pageRadio() {
  char tmp[48];
  drawHeader("Radio");

  snprintf(tmp, sizeof(tmp), "%u", (unsigned)radio_driver.getPacketsRecv());
  drawRow(0, "Recus", tmp);

  snprintf(tmp, sizeof(tmp), "%u", (unsigned)radio_driver.getPacketsSent());
  drawRow(1, "Emis", tmp);

  snprintf(tmp, sizeof(tmp), "%u", (unsigned)radio_driver.getPacketsRecvErrors());
  drawRow(2, "Erreurs RX", tmp);

  snprintf(tmp, sizeof(tmp), "rx %u tx %u", (unsigned)the_mesh.getNumRecvFlood(), (unsigned)the_mesh.getNumSentFlood());
  drawRow(3, "Flood", tmp);

  snprintf(tmp, sizeof(tmp), "rx %u tx %u", (unsigned)the_mesh.getNumRecvDirect(), (unsigned)the_mesh.getNumSentDirect());
  drawRow(4, "Direct", tmp);

  snprintf(tmp, sizeof(tmp), "%d dBm / %.1f dB", (int)radio_driver.getLastRSSI(), radio_driver.getLastSNR());
  drawRow(5, "RSSI/SNR", tmp);

  snprintf(tmp, sizeof(tmp), "%d dBm", (int)radio_driver.getNoiseFloor());
  drawRow(6, "Bruit", tmp);

  snprintf(tmp, sizeof(tmp), "%lu s", (unsigned long)(the_mesh.getTotalAirTime() / 1000));
  drawRow(7, "Temps TX", tmp);
}

void M5CoreUITask::pageNetwork() {
  char tmp[64];
  drawHeader("Reseau");

  drawRow(0, "Ethernet", m5lan::statusText(), m5lan::hasIP() ? COL_GREEN : UIColor::warning_txt);

  IPAddress ip = m5lan::localIP();
  snprintf(tmp, sizeof(tmp), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  drawRow(1, "Adresse IP", m5lan::hasIP() ? tmp : "-");

  drawRow(2, "Heure NTP", m5lan::timeSynced() ? "synchronisee" : "en attente",
          m5lan::timeSynced() ? COL_GREEN : UIColor::warning_txt);

#ifdef WITH_MQTT_BRIDGE
  MqttBridge& mqtt = the_mesh.getMqttBridge();

  drawRow(3, "Broker", mqtt.getServer().c_str());

  snprintf(tmp, sizeof(tmp), "%d", mqtt.getPort());
  drawRow(4, "Port", tmp);

  if (mqtt.isConnected()) {
    drawRow(5, "MQTT", "connecte", COL_GREEN);
  } else {
    drawRow(5, "MQTT", "deconnecte", UIColor::warning_txt);
  }

  snprintf(tmp, sizeof(tmp), "%u (echecs %u)", (unsigned)mqtt.getPublishCount(), (unsigned)mqtt.getPublishFailCount());
  drawRow(6, "Publies", tmp);

  // the topic is long: one full-width line in small characters
  _display->setTextSize(1);
  _display->setColor(UIColor::secondary_txt);
  _display->drawTextEllipsized(LABEL_X, ROW_Y0 + 7 * ROW_STEP + 4, _display->width() - 2 * LABEL_X, mqtt.getTopic().c_str());
#else
  drawRow(3, "MQTT", "desactive", UIColor::secondary_txt);
#endif
}

void M5CoreUITask::pageNeighbours() {
  drawHeader("Voisins");

  // "xxxxxxxx:secs_ago:snr_x4" per line, newest first ("-none-" when the list is empty)
  char list[220];
  the_mesh.formatNeighborsReply(list);

  int row = 0;
  char* line = list;
  while (line && *line && row < 8) {
    char* next = strchr(line, '\n');
    if (next) *next++ = 0;

    char id[12];
    unsigned secs = 0;
    int snr4 = 0;
    if (sscanf(line, "%8[0-9A-Fa-f]:%u:%d", id, &secs, &snr4) == 3) {
      char age[16], tmp[40];
      if (secs < 60) snprintf(age, sizeof(age), "%us", secs);
      else if (secs < 3600) snprintf(age, sizeof(age), "%um", secs / 60);
      else if (secs < 86400) snprintf(age, sizeof(age), "%uh", secs / 3600);
      else snprintf(age, sizeof(age), "%uj", secs / 86400);

      snprintf(tmp, sizeof(tmp), "%5s  SNR %.1f", age, snr4 / 4.0f);
      drawRow(row, id, tmp);
      row++;
    }
    line = next;
  }

  if (row == 0) {
    _display->setTextSize(2);
    _display->setColor(UIColor::secondary_txt);
    _display->drawTextCentered(_display->width() / 2, 110, "Aucun voisin entendu");
  }
}

void M5CoreUITask::render() {
  if (millis() - _started_at < BOOT_SCREEN_MILLIS) {
    pageBoot();
    return;
  }

  switch (_page) {
    case 0: pageStatus(); break;
    case 1: pageRadio(); break;
    case 2: pageNetwork(); break;
    default: pageNeighbours(); break;
  }
  drawFooter();

  if (_toast[0] && (long)(millis() - _toast_until) < 0) {
    int w = 280, h = 44;
    int x = (_display->width() - w) / 2, y = 96;
    _display->setColor(UIColor::popup_bkg);
    _display->fillRoundRect(x, y, w, h, 10);
    _display->setColor(UIColor::popup_txt);
    _display->setTextSize(2);
    _display->drawTextCentered(_display->width() / 2, y + 14, _toast);
  } else {
    _toast[0] = 0;
  }
}

// ------------------------------------------------------------------ main loop

void M5CoreUITask::loop() {
  handleButtons();

  if (_reboot_at && (long)(millis() - _reboot_at) > 0) {
    _board->reboot();
  }

#if M5UI_AUTO_OFF_SECS > 0
  if (_display->isOn() && millis() - _last_activity > (unsigned long)M5UI_AUTO_OFF_SECS * 1000UL) {
    _display->turnOff();
  }
#endif

  if (_display->isOn() && (long)(millis() - _next_refresh) >= 0) {
    _display->startFrame();
    render();
    _display->endFrame();
    _next_refresh = millis() + 1000;   // once per second
  }
}

#endif  // M5CORE_UI
