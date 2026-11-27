#pragma once

// ON6DP - WiFi station used as a fallback network when no Ethernet cable is
// plugged in (or when the LAN module is not in the stack at all).
// Kept in its own file so that WiFi.h and the W5500 library never meet in the
// same translation unit.

#include <Arduino.h>
#include <IPAddress.h>
#include <Client.h>

namespace m5wifi {
  void begin();                    // loads the credentials saved in NVS
  void loop(bool wanted);          // wanted=false switches the WiFi radio off

  bool configured();               // an SSID has been set
  bool connected();
  bool timeSynced();               // clock set from NTP over WiFi at least once
  IPAddress localIP();
  int rssi();
  const char* ssid();
  void statusText(char* dest, size_t len);   // short human-readable state

  // Saved in NVS and applied at once. An empty SSID disables WiFi.
  void setSsid(const char* ssid);
  void setPassword(const char* pwd);

#ifdef WITH_M5_WIFI
  Client& client();                // TCP client for the MQTT bridge
#endif
}
