#pragma once

#include <helpers/ESP32Board.h>
#include <Arduino.h>
#include <Wire.h>
#include <M5Cardputer.h>

class M5CardputerADVBoard : public ESP32Board {
public:
  M5CardputerADVBoard() : ESP32Board() {
  }

  void begin() {
    // Step 1: Enable power to I/O expander on LoRa Cap (GPIO 46)
    pinMode(46, OUTPUT);
    digitalWrite(46, HIGH);
    delay(100);  // Give I/O expander time to power up

    // Step 2: Initialize main I2C (SDA=2, SCL=1) before M5Cardputer keyboard init
    ESP32Board::begin();

    // Step 3: Initialize M5Cardputer hardware with keyboard enabled
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.internal_imu = false;
    cfg.internal_rtc = true;
    cfg.internal_spk = true;
    cfg.internal_mic = true;
    M5Cardputer.begin(cfg, true);
    delay(100);
    M5Cardputer.Keyboard.begin();

    // Step 4: Configure PI4IOE I/O expander at 0x43 to enable LoRa module power
    // The I/O expander is on the internal I2C bus (SDA=8, SCL=9)
    M5.In_I2C.writeRegister8(0x43, 0x03, 0x00, 100000);  // all pins output
    delay(10);
    M5.In_I2C.writeRegister8(0x43, 0x01, 0xFF, 100000);  // all outputs HIGH
    delay(200);  // Give LoRa module time to power up

    Serial.println("LoRa Cap I/O expander (PI4IOE at 0x43 on SDA=8,SCL=9) configured");

    Serial.println("M5Stack Cardputer-Adv initialized");
    Serial.print("Battery voltage: ");
    Serial.print(getBattMilliVolts());
    Serial.println(" mV");
  }

  uint16_t getBattMilliVolts() override {
    // Battery voltage on GPIO 10 with 2:1 voltage divider
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) {
      sum += analogReadMilliVolts(10);
      delay(5);
    }
    return (sum / 8) * 2;  // Apply 2:1 divider correction
  }

  const char* getManufacturerName() const override {
    return "M5Stack Cardputer ADV";
  }
};
