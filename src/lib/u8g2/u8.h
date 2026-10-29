#ifndef U8_H
#define U8_H

#include <stdint.h>
#include <driver/i2c_master.h>

#include "profont.h"  // ваш массив u8g2_font_profont15_tf

class U8 {
 public:
  U8(i2c_master_dev_handle_t &dev);
  U8();
  ~U8();

 public:
  void begin(i2c_master_bus_handle_t &i2cBus);
  void sendBuffer();
  void clearBuffer();

  void setFont(font_t font);
  void setFontPosTop();
  void setFontPosBottom();

  void setColorIndex(const uint8_t colorIdx);
  void setCursor(const uint8_t x, const uint8_t y);
  void print(const char* str);

  uint8_t getMaxCharHeight();
  uint8_t getMaxCharWidth();
  uint8_t getStrWidth(const char* str);

  void drawBox(const uint8_t x, const uint8_t y, const uint8_t w,
               const uint8_t h);
  void drawRBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r);
  void drawXBM(const uint8_t xS, const uint8_t yS, const uint8_t w,
               const uint8_t h, const uint8_t* bitmap);

  void setPowerSave(bool value);
};

#endif