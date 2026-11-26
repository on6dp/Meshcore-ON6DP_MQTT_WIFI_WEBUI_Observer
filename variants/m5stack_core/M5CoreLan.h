#pragma once

// ON6DP - wired network for the M5Stack LAN Module 13.2 (W5500).
//
// The W5500 shares the SPI bus with the LCD and the SX1262, so it is driven
// from the main loop only (polled, no interrupt, no background task).

#include <Arduino.h>
#include <IPAddress.h>

namespace m5lan {
  void begin();            // detects the W5500 (no network traffic yet)
  void loop();             // link watch, DHCP, lease renewal, NTP

  bool hasHardware();      // W5500 answered on SPI
  bool linkUp();           // cable plugged, link negotiated
  bool hasIP();            // address obtained (DHCP or static)
  bool timeSynced();       // clock set from NTP at least once
  IPAddress localIP();
  const char* statusText();   // short human-readable state
}
