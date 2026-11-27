#pragma once

// ON6DP - M5Stack Core Basic/Gray/Fire (ESP32) + Module13.2 LoRa-1262 + Module13.2 LAN (W5500)
//
// The LCD, the TF card slot, the SX1262 and the W5500 all share the same SPI bus
// (SCK=18, MISO=19, MOSI=23). Every chip-select line is therefore driven HIGH
// before anything talks on the bus.

#include <Arduino.h>
#include <SPI.h>
#include <helpers/ESP32Board.h>

// ---- Core Basic built-ins (fixed by the hardware) ----
#define PIN_CORE_SCK      18
#define PIN_CORE_MISO     19
#define PIN_CORE_MOSI     23
#define PIN_CORE_TFT_CS   14
#define PIN_CORE_TFT_DC   27
#define PIN_CORE_TFT_RST  33
#define PIN_CORE_TFT_BL   32
#define PIN_CORE_SD_CS     4
#define PIN_CORE_SPEAKER  25
#define PIN_CORE_BTN_A    39
#define PIN_CORE_BTN_B    38
#define PIN_CORE_BTN_C    37

// ---- LAN Module 13.2 (selected with the module's jumper caps) ----
#ifndef P_ETH_CS
  #define P_ETH_CS   5      // factory jumper position
#endif
#ifndef P_ETH_RST
  #define P_ETH_RST  0      // factory jumper position
#endif
#ifndef P_ETH_INT
  #define P_ETH_INT  35     // factory jumper position (not used by the firmware, W5500 is polled)
#endif

// ---- Module13.2 LoRa-1262: M5IOE1 I/O expander (I2C) ----
#ifndef LORA_IOE_ADDR
  #define LORA_IOE_ADDR  0x74   // SW2: A=0, B=0
#endif
// expander pin indexes (IOn -> bit n-1)
#define LORA_IOE_BIT_RST     (1 << 1)   // IO2 = LoRa reset (active low)
#define LORA_IOE_BIT_BYPASS  (1 << 2)   // IO3 = "bypass" / RF switch enable
#define LORA_IOE_BIT_PWR     (1 << 4)   // IO5 = 3V3 power enable
#ifndef LORA_IOE_BYPASS_LEVEL
  #define LORA_IOE_BYPASS_LEVEL 1       // same level as M5Stack's own example
#endif

class M5CoreBoard : public ESP32Board {
  bool _ioe_ok = false;

  bool ioeRead(uint8_t reg, uint8_t& val);
  bool ioeWrite(uint8_t reg, uint8_t val);
  bool ioeUpdate(uint8_t reg, uint8_t clear_mask, uint8_t set_mask);

public:
  void begin();

  // Power-cycles and resets the SX1262 through the M5IOE1 expander.
  bool loraPowerCycle();
  bool hasLoRaExpander() const { return _ioe_ok; }

  const char* getManufacturerName() const override {
    return "M5Stack Core";
  }
};
