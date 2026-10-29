#pragma once

#include <stdio.h>
#include <esp_log.h>  
#include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
#include <esp_timer.h>
#include <driver/gpio.h>

#define millis() ((uint32_t)(esp_timer_get_time() / 1000))

inline void setTimeout(uint32_t timeMs, TimerHandle_t &timerHandle,
                       void (*callback)(TimerHandle_t)) {
  timerHandle =
      xTimerCreate("timer", pdMS_TO_TICKS(timeMs), pdFALSE, nullptr, callback);
  xTimerStart(timerHandle, 0);
}

inline void setInterval(uint32_t timeMs, TimerHandle_t &timerHandle,
                       void (*callback)(TimerHandle_t)) {
  timerHandle =
      xTimerCreate("timer", pdMS_TO_TICKS(timeMs), pdTRUE, nullptr, callback);
  xTimerStart(timerHandle, 0);
}
// check IsTimerActive
inline bool timerHas(TimerHandle_t &timerHandle) {
  return (timerHandle != nullptr && xTimerIsTimerActive(timerHandle));
}

inline void removeTimer(TimerHandle_t& timerHandle) {
  if (timerHandle != nullptr) {
    xTimerStop(timerHandle, 0);
  }
}
