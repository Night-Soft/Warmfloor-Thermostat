
#pragma once

#ifdef ARDUINO
#elif defined(ESP_PLATFORM)  // ESP-IDF

#include <stdio.h>
// #include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <rom/ets_sys.h>

#define LOW 0
#define HIGH 1

#define INPUT GPIO_MODE_INPUT
#define OUTPUT GPIO_MODE_OUTPUT  // todo modes

#define miccros() (esp_timer_get_time())
#define millis() ((uint32_t)(esp_timer_get_time() / 1000))

#define delayMicroseconds(us) (ets_delay_us(us))
#define delay(ms) (vTaskDelay(pdMS_TO_TICKS(ms)))

#define pinMode(pin, mode)                                  \
  do {                                                      \
    gpio_reset_pin((gpio_num_t)pin);                        \
    gpio_set_direction((gpio_num_t)pin, (gpio_mode_t)mode); \
  } while (0)

#define digitalWrite(pin, value) (gpio_set_level((gpio_num_t)pin, value))
#define digitalRead(pin) (gpio_get_level((gpio_num_t)pin))

#endif