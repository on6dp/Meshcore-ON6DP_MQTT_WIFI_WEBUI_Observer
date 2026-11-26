#include <Arduino.h>
#include "target.h"

M5CoreBoard board;

// The SX1262 sits on the Core's default SPI bus (shared with LCD / W5500).
// Its reset line is not a GPIO: it is driven by the M5IOE1 expander (see M5CoreBoard).
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY);

WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
SensorManager sensors;

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
#endif

bool radio_init() {
  fallback_clock.begin();
  rtc_clock.begin(Wire);

  if (radio.std_init()) return true;

  // one more try after a full power cycle of the LoRa module
  Serial.println("Radio init failed, power-cycling the LoRa module and retrying...");
  board.loraPowerCycle();
  return radio.std_init();
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}
