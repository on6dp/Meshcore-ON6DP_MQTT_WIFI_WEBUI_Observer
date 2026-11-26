#pragma once

// ON6DP - display driver for the 320x240 ILI9342C LCD of the M5Stack Core Basic/Gray/Fire.
// Frames are drawn into an off-screen sprite and pushed in one go (no flicker).

#include <helpers/ui/DisplayDriver.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "M5CoreBoard.h"

class M5CoreLGFX : public lgfx::LGFX_Device {
  // Same trick as M5Stack's own library: newer Cores have an IPS panel that
  // needs colour inversion, and it can be recognised from the reset line.
  struct Panel_M5Core : public lgfx::Panel_ILI9342 {
    bool init(bool use_reset) override {
    #ifdef M5_LCD_INVERT
      _cfg.invert = (M5_LCD_INVERT != 0);
    #else
      ::pinMode(PIN_CORE_TFT_RST, INPUT_PULLDOWN);
      delay(1);
      _cfg.invert = (::digitalRead(PIN_CORE_TFT_RST) != 0);
      ::pinMode(PIN_CORE_TFT_RST, OUTPUT);
    #endif
      return lgfx::Panel_ILI9342::init(use_reset);
    }
  };

  Panel_M5Core    _panel;
  lgfx::Bus_SPI   _bus;
  lgfx::Light_PWM _light;

public:
  M5CoreLGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host   = VSPI_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = true;
      cfg.use_lock   = true;
      cfg.dma_channel = 1;
      cfg.pin_sclk = PIN_CORE_SCK;
      cfg.pin_mosi = PIN_CORE_MOSI;
      cfg.pin_miso = PIN_CORE_MISO;
      cfg.pin_dc   = PIN_CORE_TFT_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = PIN_CORE_TFT_CS;
      cfg.pin_rst  = PIN_CORE_TFT_RST;
      cfg.pin_busy = -1;
      cfg.panel_width  = 320;
      cfg.panel_height = 240;
      cfg.offset_rotation = 3;
      cfg.readable   = true;
      cfg.invert     = false;
      cfg.rgb_order  = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;    // the bus is shared with the SX1262 and the W5500
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = PIN_CORE_TFT_BL;
      cfg.invert = false;
      cfg.freq   = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

class M5CoreDisplay : public DisplayDriver {
  M5CoreLGFX  _lcd;
  LGFX_Sprite _buf;
  bool     _ready = false;
  bool     _isOn = false;
  uint8_t  _brightness = 160;
  uint16_t _color = 0xFFFF;

public:
  M5CoreDisplay() : DisplayDriver(320, 240), _buf(&_lcd) { }

  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;

  // extras used by the M5Stack Core UI
  void fillRoundRect(int x, int y, int w, int h, int r);
  void drawRoundRect(int x, int y, int w, int h, int r);
  void setBrightness(uint8_t level);      // 0..255, remembered for the next turnOn()
  uint8_t getBrightness() const { return _brightness; }
};
