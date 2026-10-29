#include "display.h"

#include <stdio.h>
#include "display.h"

static U8 u8g2;

Display display;

#define WHITE u8g2.setColorIndex(1);
#define BLACK u8g2.setColorIndex(0);

// Width: 24
// Height: 24
static const unsigned char pump_bitmap[72] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x07, 0x00,
    0x60, 0x02, 0x00, 0x60, 0x02, 0x00, 0x60, 0xE2, 0x7F, 0x60, 0xF2, 0xFF,
    0x60, 0x37, 0xC0, 0xF8, 0xFF, 0xCF, 0x1B, 0xF0, 0xDF, 0x1F, 0x30, 0xC0,
    0x1F, 0x30, 0xC0, 0x1B, 0xF0, 0xDF, 0x1B, 0xF0, 0xCF, 0x1F, 0x30, 0xC0,
    0x1B, 0xF0, 0xCF, 0xF8, 0xFF, 0xDF, 0xF8, 0x3F, 0xC0, 0x00, 0x30, 0xC0,
    0x00, 0xE0, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// static const unsigned char pump_bitmap[72] = {
//     // new
//     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x07, 0x00,
//     0x60, 0x02, 0x00, 0x60, 0x02, 0x00, 0x60, 0xe2, 0x7f, 0x60, 0xf2, 0xff,
//     0x60, 0x37, 0xc0, 0xf8, 0xff, 0xcf, 0x1b, 0xf0, 0xdf, 0x1f, 0x30, 0xc0,
//     0x1f, 0x30, 0xc0, 0x1b, 0xf0, 0xdf, 0x1b, 0xf0, 0xcf, 0x1f, 0x30, 0xc0,
//     0x1b, 0xf0, 0xcf, 0xf8, 0xff, 0xdf, 0xf8, 0x3f, 0xc0, 0x00, 0x30, 0xc0,
//     0x00, 0xe0, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Width: 18
// Height: 12
static const unsigned char heating_bitmap[36] = {
    0x8C, 0x73, 0x00, 0x38, 0xC6, 0x01, 0x60, 0x8C, 0x03, 0x60, 0x1C, 0x03,
    0x60, 0x8C, 0x01, 0x38, 0xC6, 0x00, 0x8C, 0x73, 0x00, 0xC6, 0x38, 0x00,
    0xC7, 0x18, 0x00, 0xC3, 0x18, 0x00, 0xC6, 0x31, 0x00, 0x1C, 0x63, 0x00};

// static const unsigned char heating_bitmap[36] = {
//     // new
// 	0x31, 0xce, 0x00, 0x1c, 0x73, 0x00, 0x06, 0x39, 0x00, 0x06, 0x39, 0x00, 0x0e, 0x31, 0x00, 0x1c, 
// 	0xf3, 0x00, 0x39, 0xce, 0x00, 0x63, 0x9c, 0x00, 0xe7, 0x18, 0x00, 0xe7, 0x18, 0x00, 0x73, 0x9c, 
// 	0x00, 0x38, 0xc6, 0x00};

// Width: 10
// Height: 8
static const unsigned char wifi_bitmap[16] = {
    0xFC, 0x00, 0xFF, 0x03, 0x03, 0x03, 0x78, 0x00,
    0xFE, 0x01, 0x86, 0x01, 0x30, 0x00, 0x30, 0x00};

uint8 getCenterX(uint8 size) { return (128 - size) / 2; }

uint8 getCenterY(uint8 size) { return (64 - size) / 2; }

void printCurrentTime() {
  static uint8 maxStrLen = 9;
  static char dateStr[12];

  u8g2.setFont(u8g2_font_profont12_tf);

  uint8 clearWitdh = u8g2.getMaxCharWidth() * maxStrLen;
  uint8 clearHeight = u8g2.getMaxCharHeight();
  BLACK
  u8g2.drawBox(0, 0, clearWitdh, clearHeight);
  WHITE

  // todo
  CurrentTime time = realTime.getComputedTime();
  //CurrentTime time = realTime.getCurrentTime();
  sprintf(dateStr, "%02u:%02u:%02u", (uint8)time.hour, (uint8)time.minutes,
          (uint8)time.seconds);
  // Serial.printf("Draw: %s\n", dateStr);

  u8g2.setCursor(0, 0);
  u8g2.print(dateStr);
}

void updateModal() {
  BLACK
  u8g2.drawBox(0, 12, 128, 52);
  
  uint8 x, y = 12;
  WHITE

  u8g2.setFont(u8g2_font_profont15_tf);
  uint8 titleWidth = u8g2.getStrWidth(display.modal.title);
  uint8 titleHeight = u8g2.getMaxCharHeight();
  x = getCenterX(titleWidth);
  y += 3;
  u8g2.setCursor(x, y);
  u8g2.print(display.modal.title);

  u8g2.setFont(u8g2_font_profont12_mr);
  uint8 str1Width = u8g2.getStrWidth(display.modal.str1);
  uint8 str1Height = u8g2.getMaxCharHeight();
  x = getCenterX(str1Width);
  y += titleHeight + 3;
  u8g2.setCursor(x, y);
  u8g2.print(display.modal.str1);

  // Serial.print("Str2: ");
  // Serial.println(display.modal.str2);

  uint8 str2Width = u8g2.getStrWidth(display.modal.str2);
//  uint8 str2Height = u8g2.getMaxCharHeight();
  x = getCenterX(str2Width);
  y += str1Height + 3;
  u8g2.setCursor(x, y);
  u8g2.print(display.modal.str2);
  
}

uint32 endTimeShowModal = 0;
bool canUpdate = true;    
void draw() {
  if (!canUpdate) return;
  if (display.isModal) {
    updateModal();
    if(display.modal.timeShow > 0 && millis() > endTimeShowModal) {
      printf("hideModal\n");

      display.hideModal();
    }
  }
  // todo restoreTime
  printCurrentTime(); 
  u8g2.sendBuffer();
}


Display::Display() : canUpdate(::canUpdate), gDisplay(u8g2) {}

static TimerHandle_t timer;

void Display::init(i2c_master_bus_handle_t &i2cBus) {
  u8g2.begin(i2cBus);
  u8g2.setFont(u8g2_font_profont15_tf);
  u8g2.setFontPosTop();

  const char *loading = "Loading...";
  setCursorToCenter(loading);

  u8g2.print(loading);
  u8g2.sendBuffer();
  u8g2.clearBuffer();  
  u8g2.setFont(u8g2_font_profont12_tf);

//   postponedTask.setInterval(1000, draw);
  timer = xTimerCreate("display", pdMS_TO_TICKS(1000), pdTRUE, nullptr,
                       [](TimerHandle_t xTimer) { draw(); });
  xTimerStart(timer, 0);
}

void Display::startUpdate() {canUpdate = true;}

void Display::stopUpdate() {canUpdate = false;}

void Display::forceUpdade() { u8g2.sendBuffer(); }

void Display::drawNow() { draw(); }

void Display::powerSave() {
  u8g2.setPowerSave(true);
}

void Display::print(const char *str, uint8 x, uint8 y) {
  u8g2.setCursor(x, y);
  u8g2.print(str);
}

void Display::printToCenter(const char* str) {
  u8g2.setFont(u8g2_font_profont12_tf);
  setCursorToCenter(str);
  u8g2.print(str);
}

void Display::setCursorToCenter(const char* str) {
  uint8 width = u8g2.getStrWidth(str);
  uint8 height = u8g2.getMaxCharHeight();
  uint8 x = (128 - width) / 2;
  uint8 y = (64 - height) / 2;

  if (width >= 128) x = 0;

//  Serial.printf("\nSet cursor widthChr: %u, x: %u, y: %u\n", width, x, y);
  u8g2.setCursor(x, y);
}

void Display::setFontSize(FONT_SIZE size) {
  switch (size) {
    case FONT_SIZE::F10:
    //  u8g2.setFont(u8g2_font_profont10_tf);
      break;
    case FONT_SIZE::F11:
     // u8g2.setFont(u8g2_font_profont11_tf);
      break;
    case FONT_SIZE::F12:
      u8g2.setFont(u8g2_font_profont12_tf);
      break;
    case FONT_SIZE::F15:
      u8g2.setFont(u8g2_font_profont15_tf);
      break;
    case FONT_SIZE::F17:
  //    u8g2.setFont(u8g2_font_profont17_tf);
      break;
    case FONT_SIZE::F22:
//      u8g2.setFont(u8g2_font_profont22_tf);
      break;
    default:
      u8g2.setFont(u8g2_font_profont12_tf);
      break;
  }
}

void Display::toggleWiFi(bool isEnable) {
  uint8 x = 118, y = 2, w = 10, h = 8;
  // clean

  if (isEnable) {
    WHITE
    u8g2.drawXBM(x, y, w, h, wifi_bitmap);
  } else {
    BLACK
    u8g2.drawBox(x, y, w, h);
  }
}

void Display::toggleHeating(bool isEnable) {
  uint8 x = 97, y = 0, w = 18, h = 12;
  // clean
  BLACK
  u8g2.drawBox(x, y, w, h);
  if (isEnable) {
    WHITE
    u8g2.drawXBM(x, y, w, h, heating_bitmap);
  } else {
    BLACK
    u8g2.drawBox(x, y, w, h);
  }
}

void Display::togglePump(bool isEnable) {
  uint8 size = 24;
  uint8 y = getCenterY(size);
  uint8 x = 128 - size;
  BLACK
  u8g2.drawBox(x, y, size, size);

  if (isEnable) {
    WHITE
    u8g2.drawXBM(x, y, size, size, pump_bitmap);
  }
}

void Display::togglePumpBtn(bool isEnable) {
  //Serial.println("togglePumpBtn");
  uint8 size = 12;
  uint8 y = 0;
  uint8 x = 82;
  // clear
  BLACK
  u8g2.drawBox(x, y, size, size);
  if (isEnable) { 
    WHITE
    u8g2.drawRBox(x, y, size, size, 1);
    BLACK
    u8g2.drawRBox(x + 1, y + 1, 10, 10, 1);
    WHITE
    u8g2.drawBox(x + 3, y + 2, 2, 8);
    u8g2.drawBox(x + 7, y + 2, 2, 8);
  }
}

uint8 Display::showModal(Modal &modal) {
  uint8 id = showModal();
  this->modal = modal;
  if(modal.timeShow > 0) {
    endTimeShowModal = millis() + (modal.timeShow + 1) * 1000;
  }
  draw();
  return id;
}

uint8 Display::showModal() {
  isModal = true;
  modalId++;
  if(modalId == 0) modalId++;
  return modalId;
}

void Display::hideModal() {
  isModal = false;  
  modalId = 0;
  BLACK
  u8g2.drawBox(0, 12, 128, 52);
  WHITE
}

void Display::hideModal(uint8 id) {
  if(modalId > 0 && modalId == id) hideModal();
}

uint8 Display::getModalId() { return modalId; }