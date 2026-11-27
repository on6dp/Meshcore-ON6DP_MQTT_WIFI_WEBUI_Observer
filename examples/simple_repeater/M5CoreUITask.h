#pragma once

// ON6DP - on-device UI for the M5Stack Core Basic/Gray/Fire repeater:
// 320x240 colour screen, 4 status pages, 3 buttons (A / B / C).
// Only compiled when M5CORE_UI is defined (see variants/m5stack_core/platformio.ini).

#ifdef M5CORE_UI

#include <helpers/CommonCLI.h>
#include "target.h"

// Debounced push button (active low). A press only counts once the line has been
// stable for 25 ms: GPIO39 (button A) picks up very short glitches while WiFi is on.
class M5CoreButton {
  int8_t _pin;
  bool _raw = false, _stable = false, _long_fired = false;
  unsigned long _changed_at = 0, _down_at = 0;
public:
  enum { EVENT_NONE = 0, EVENT_CLICK, EVENT_LONG_PRESS };
  explicit M5CoreButton(int8_t pin) : _pin(pin) { }
  void begin();
  int check();
};

class M5CoreUITask {
  mesh::MainBoard* _board;
  M5CoreDisplay*   _display;
  NodePrefs*       _node_prefs = NULL;
  M5CoreButton     _btnA, _btnB, _btnC;

  char _version[24];
  char _build_date[16];
  char _toast[28];
  unsigned long _toast_until = 0;
  unsigned long _next_refresh = 0;
  unsigned long _last_activity = 0;
  unsigned long _started_at = 0;
  unsigned long _reboot_at = 0;
  uint8_t _page = 0;
  uint8_t _bright_step = 0;

  void showToast(const char* msg);
  void handleButtons();
  void render();
  void drawHeader(const char* page_title);
  void drawFooter();
  void drawRow(int row, const char* label, const char* value, ColorVal value_color);
  void drawRow(int row, const char* label, const char* value);
  void pageStatus();
  void pageRadio();
  void pageNetwork();
  void pageNeighbours();
  void pageBoot();

public:
  M5CoreUITask(mesh::MainBoard& board, M5CoreDisplay& display);
  void begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version);
  void loop();
};

#endif
