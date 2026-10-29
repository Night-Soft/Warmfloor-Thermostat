#pragma once

#ifdef ARDUINO
#elif defined(ESP_PLATFORM)  // ESP-IDF

#include <ctype.h>
#include <driver/i2c_master.h>

class ComWire {
 public:
  ComWire();

  void begin();
  void begin(uint8_t sda, uint8_t scl);
  void write(uint8_t value);
  void write(uint8_t* arr, size_t size);
  uint8_t requestFrom(uint8_t address, size_t reqSize);
  uint8_t read();  // read to byte
  uint8_t available();
  void beginTransmission(uint8_t address);
  uint8_t endTransmission();

 private:
  esp_err_t err;  // 0 ok

  struct SendBuffer {
    uint8_t buffer[32];
    uint8_t size = 0;
  } send;

  struct ReceiveBuffer {
    uint8_t buffer[32];
    uint8_t size = 0;
    uint8_t nextReadIdx = 0;
  } receive;
};

#endif