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

  // Two networks are remembered (slot 0 and slot 1, e.g. home and phone hotspot).
  // They are tried in turn until one of them answers.
  bool configured();               // at least one SSID has been set
  bool connected();
  bool timeSynced();               // clock set from NTP over WiFi at least once
  IPAddress localIP();
  int rssi();
  const char* ssid();              // network in use (or being tried)
  const char* ssid(int slot);      // saved network, "" when the slot is empty
  void statusText(char* dest, size_t len);   // short human-readable state

  // Saved in NVS and applied at once. An empty SSID clears the slot.
  void setSsid(int slot, const char* ssid);
  void setPassword(int slot, const char* pwd);

#ifdef WITH_M5_WIFI
  Client& client();                // TCP client for the MQTT bridge
#endif

  // Configuration mode: the Core opens its own WPA2 access point (address 192.168.4.1)
  // so that a phone or PC can reach the setup page. Normal WiFi use is paused meanwhile.
  bool startAccessPoint(const char* ap_ssid, const char* ap_pwd);
  void stopAccessPoint();
  bool accessPointActive();
  int  accessPointClients();
}
