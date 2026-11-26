#include "M5CoreBoard.h"

// M5IOE1 registers (low bytes cover IO1..IO8, which is all this module uses)
#define IOE_REG_MODE_L  0x03   // 1 = output
#define IOE_REG_OUT_L   0x05
#define IOE_REG_DRV_L   0x13   // 0 = push-pull, 1 = open-drain

bool M5CoreBoard::ioeRead(uint8_t reg, uint8_t& val) {
  Wire.beginTransmission((uint8_t)LORA_IOE_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)LORA_IOE_ADDR, (uint8_t)1) != 1) return false;
  val = Wire.read();
  return true;
}

bool M5CoreBoard::ioeWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission((uint8_t)LORA_IOE_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool M5CoreBoard::ioeUpdate(uint8_t reg, uint8_t clear_mask, uint8_t set_mask) {
  for (int attempt = 0; attempt < 3; attempt++) {
    uint8_t v;
    if (ioeRead(reg, v)) {
      v = (v & ~clear_mask) | set_mask;
      if (ioeWrite(reg, v)) return true;
    }
    delay(20);
  }
  return false;
}

bool M5CoreBoard::loraPowerCycle() {
  const uint8_t all = LORA_IOE_BIT_RST | LORA_IOE_BIT_BYPASS | LORA_IOE_BIT_PWR;
  const uint8_t bypass = LORA_IOE_BYPASS_LEVEL ? LORA_IOE_BIT_BYPASS : 0;

  // levels first (reset held, power off), then push-pull, then switch the pins to outputs
  bool ok = ioeUpdate(IOE_REG_OUT_L, all, bypass);
  ok = ioeUpdate(IOE_REG_DRV_L, all, 0) && ok;
  ok = ioeUpdate(IOE_REG_MODE_L, 0, all) && ok;
  if (!ok) return false;

  delay(25);
  ok = ioeUpdate(IOE_REG_OUT_L, 0, LORA_IOE_BIT_PWR);    // power on
  delay(120);
  ok = ioeUpdate(IOE_REG_OUT_L, 0, LORA_IOE_BIT_RST) && ok;   // release reset
  delay(120);
  return ok;
}

void M5CoreBoard::begin() {
  // deselect everything on the shared SPI bus
  static const int8_t cs_pins[] = { PIN_CORE_TFT_CS, PIN_CORE_SD_CS, P_LORA_NSS, P_ETH_CS };
  for (unsigned i = 0; i < sizeof(cs_pins); i++) {
    pinMode(cs_pins[i], OUTPUT);
    digitalWrite(cs_pins[i], HIGH);
  }

  // keep the speaker amplifier input quiet
  pinMode(PIN_CORE_SPEAKER, OUTPUT);
  digitalWrite(PIN_CORE_SPEAKER, LOW);

  // hardware reset of the W5500
  pinMode(P_ETH_RST, OUTPUT);
  digitalWrite(P_ETH_RST, LOW);

  ESP32Board::begin();   // also starts Wire on PIN_BOARD_SDA / PIN_BOARD_SCL

  SPI.begin(PIN_CORE_SCK, PIN_CORE_MISO, PIN_CORE_MOSI, -1);

  delay(50);
  digitalWrite(P_ETH_RST, HIGH);

  _ioe_ok = loraPowerCycle();
  if (!_ioe_ok) {
    Serial.printf("ERROR: M5IOE1 expander not answering at 0x%02X (check SW2 on the LoRa module)\n", LORA_IOE_ADDR);
  }
}
