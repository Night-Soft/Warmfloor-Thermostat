#include "ComWire.h"

#include <driver/i2c_master.h>
#include <string.h>

extern i2c_master_bus_handle_t i2cBus;  // ?
static i2c_master_dev_handle_t rtcDev = nullptr;

ComWire::ComWire() {}

void ComWire::begin() {
  i2c_device_config_t rtcConfig = {};
  rtcConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  rtcConfig.device_address = 0x68;
  rtcConfig.scl_speed_hz = 100000;

  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2cBus, &rtcConfig, &rtcDev));
}
void ComWire::begin(uint8_t sda, uint8_t scl) { this->begin(); }

void ComWire::write(uint8_t value) { send.buffer[this->send.size++] = value; }

void ComWire::write(uint8_t* data, size_t size) {
  memcpy((send.buffer + send.size), data, size);
  send.size += size;
}

// запросить N байт с адреса, вернёт сколько получено
uint8_t ComWire::requestFrom(uint8_t address, size_t reqSize) {
  esp_err_t err = i2c_master_receive(rtcDev, receive.buffer, reqSize, 10);
  // esp_err_t err =
  //     i2c_master_transmit_receive(rtcDevice, &reg, 1, data, sizeof(data),
  //     1000);
  if (err == ESP_OK) {
    receive.size += reqSize;
    return reqSize;
  }

  return 0;  // error
}

uint8_t ComWire::read() {
  if (receive.size > 0) {
    const uint8_t result = receive.buffer[receive.nextReadIdx++];

    if (receive.size == receive.nextReadIdx) {
      receive.size = 0;
      receive.nextReadIdx = 0;
    }

    return result;
  }
  return 0;
}

uint8_t ComWire::available() { return receive.size - receive.nextReadIdx; }
// начать отправку по адресу
// (отправит START + заголовок)
void ComWire::beginTransmission(uint8_t address) {
  // may no  need
}

uint8_t ComWire::endTransmission() {  // // отправить на устройство
  esp_err_t err = i2c_master_transmit(rtcDev, send.buffer, send.size, 10);
  send.size = 0;

  return (uint8_t)err;
}