#pragma once

// ON6DP - configuration mode of the M5Stack Core repeater.
//
// Started from the device itself (long press on the middle button, "Reseau" page).
// The Core opens its own WiFi access point, protected by a random password shown
// on the screen, and serves one setup page at http://192.168.4.1 : the two WiFi
// networks and the MQTT settings. It closes after a save, on a button press, or
// after 10 minutes without a request.

namespace m5portal {
  bool start();
  void stop();
  void loop();
  bool active();
  const char* apName();
  const char* apPassword();
}
