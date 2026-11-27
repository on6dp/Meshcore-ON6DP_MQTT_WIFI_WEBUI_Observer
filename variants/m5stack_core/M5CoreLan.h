#pragma once

// ON6DP - network for the M5Stack Core repeater.
//
// Wired Ethernet (LAN Module 13.2, W5500) has priority. When WITH_M5_WIFI is
// defined, WiFi takes over while no cable is plugged in (or when the LAN module
// is not in the stack) and is switched off again as soon as the cable is back.
//
// The W5500 shares the SPI bus with the LCD and the SX1262, so it is driven
// from the main loop only (polled, no interrupt, no background task).

#include <Arduino.h>
#include <IPAddress.h>

namespace m5lan {
  void begin();            // detects the W5500 (no network traffic yet)
  void loop();             // link watch, DHCP, lease renewal, NTP

  // wired side only
  bool hasHardware();      // W5500 answered on SPI
  bool linkUp();           // cable plugged, link negotiated
  bool hasIP();            // wired address obtained (DHCP or static)
  IPAddress localIP();     // wired address
  const char* statusText();   // short human-readable state of the wired side

  // whichever network is in use
  bool online();           // wired or WiFi connection usable
  bool viaWifi();          // true when the active connection is WiFi
  IPAddress activeIP();
  bool timeSynced();       // clock set from NTP at least once
}
