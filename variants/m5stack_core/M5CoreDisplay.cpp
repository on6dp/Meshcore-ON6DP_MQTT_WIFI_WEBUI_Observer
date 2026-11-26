#include "M5CoreDisplay.h"

// Colour scheme (RGB565) - dark theme
ColorVal UIColor::window_bkg    = 0x0000;
ColorVal UIColor::title_bkg     = 0x1A9F;   // blue
ColorVal UIColor::title_txt     = 0xFFFF;
ColorVal UIColor::primary_txt   = 0xFFFF;
ColorVal UIColor::secondary_txt = 0x9CF3;   // light grey
ColorVal UIColor::warning_txt   = 0xFD20;   // orange
ColorVal UIColor::popup_bkg     = 0x07FF;   // cyan
ColorVal UIColor::popup_txt     = 0x0000;
ColorVal UIColor::corp_blue     = 0x34BF;

bool M5CoreDisplay::begin() {
  _lcd.init();
  _lcd.setRotation(1);
  _lcd.fillScreen((uint16_t)0x0000);

  _buf.setColorDepth(8);        // 320x240 @ 8 bits = 77 KB of heap (the Basic has no PSRAM)
  _buf.setPsram(false);
  _ready = (_buf.createSprite(width(), height()) != nullptr);
  if (!_ready) {
    Serial.println("ERROR: not enough memory for the display buffer");
  }
  _buf.setTextWrap(false);

  turnOn();
  return _ready;
}

void M5CoreDisplay::turnOn() {
  _lcd.setBrightness(_brightness ? _brightness : 160);
  _isOn = true;
}

void M5CoreDisplay::turnOff() {
  _lcd.setBrightness(0);
  _isOn = false;
}

void M5CoreDisplay::setBrightness(uint8_t level) {
  _brightness = level;
  if (_isOn) _lcd.setBrightness(level);
}

void M5CoreDisplay::clear() {
  _buf.fillScreen((uint16_t)0x0000);
}

void M5CoreDisplay::startFrame(ColorVal bkg) {
  _buf.fillScreen((uint16_t)bkg);
  _buf.setTextSize(1);
  _color = UIColor::primary_txt;
  _buf.setTextColor(_color);
}

void M5CoreDisplay::setTextSize(int sz) {
  _buf.setTextSize(sz);
}

void M5CoreDisplay::setColor(ColorVal c) {
  _color = c;
  _buf.setTextColor(_color);
}

void M5CoreDisplay::setCursor(int x, int y) {
  _buf.setCursor(x, y);
}

void M5CoreDisplay::print(const char* str) {
  _buf.print(str);
}

void M5CoreDisplay::fillRect(int x, int y, int w, int h) {
  _buf.fillRect(x, y, w, h, _color);
}

void M5CoreDisplay::drawRect(int x, int y, int w, int h) {
  _buf.drawRect(x, y, w, h, _color);
}

void M5CoreDisplay::fillRoundRect(int x, int y, int w, int h, int r) {
  _buf.fillRoundRect(x, y, w, h, r, _color);
}

void M5CoreDisplay::drawRoundRect(int x, int y, int w, int h, int r) {
  _buf.drawRoundRect(x, y, w, h, r, _color);
}

void M5CoreDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  _buf.drawBitmap(x, y, bits, w, h, _color);
}

uint16_t M5CoreDisplay::getTextWidth(const char* str) {
  return _buf.textWidth(str);
}

void M5CoreDisplay::endFrame() {
  if (!_ready) return;
  _buf.pushSprite(0, 0);
}
