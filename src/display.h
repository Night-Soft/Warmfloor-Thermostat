#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#include "lib/u8g2/u8.h"
#include "utils.h"
#include "rtc/RealRtc.h"

using uint8 = uint8_t;
using uint32 = uint32_t;

enum FONT_SIZE : uint8_t {
    F10,
    F11,
    F12,
    F15,
    F17,
    F22
};

struct Modal {
    char title[30];
    char str1[34];
    char str2[34];

    uint16_t timeShow = 0; // seconds, 0 = always show
};

class Display {
    bool &canUpdate;
    uint8_t modalId = 0;

public:
    Display();

    U8& gDisplay;

    void init(i2c_master_bus_handle_t &i2cBus);

    void startUpdate();
    void stopUpdate();

    // Send current framebuffer to OLED
    void forceUpdade();

    // Redraw everything
    void drawNow();

    void powerSave();

    void print(const char* str, uint8_t x, uint8_t y);
    void printToCenter(const char* str);
    void printLeftPumpTime(const char* str);
    void setCursorToCenter(const char* str);

    void setFontSize(FONT_SIZE size);
    FONT_SIZE fontSize = FONT_SIZE::F12;

    Modal modal;
    bool isModal = false;

    uint8_t getModalId();

    uint8_t showModal(Modal &modal);
    uint8_t showModal();

    void hideModal();
    void hideModal(uint8_t id);

    void toggleWiFi(bool isEnable);
    void toggleHeating(bool isEnable);
    void togglePump(bool isEnable);
    void togglePumpBtn(bool isEnable);
};

extern Display display;

#endif