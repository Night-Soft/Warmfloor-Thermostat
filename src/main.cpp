#include <cstdint>

#include "Radio.h"
#include "display.h"
#include "peripheral.h"
#include "rtc/RealRtc.h"
#include "wifi/wifi.h"
#include "littlefs.h"
#include "wifi/web.h"

RealRtc realTime;

// WiFiHolder wifi;

extern "C" void app_main(void) {
  vTaskDelay(pdMS_TO_TICKS(3000));
  
  initI2cBus();
  littlefsInit();
  ESP_LOGI(TAG, "initI2cBus inited\n");

  display.init(i2cBus);
  ESP_LOGI(TAG, "Display inited\n");

  realTime.init();
  ESP_LOGI(TAG, "Real time inited\n");

  vTaskDelay(pdMS_TO_TICKS(500));

  testFS();

  setupRFM95();
  printf("\n");
  bool isWebServer = false;
  while (1) {
    static uint32_t tim = 0, tim2 = 0;

    if (millis() - tim > 60 * 1000) {
      printf("\rSystem uptime: %ld seconds\n", millis() / 1000);

      tim = millis();
      realTime.printComputedTime();
    }

    if (millis() - tim2 > 140) {
      tim2 = millis();
      firstCheck();
    }

    if(isWebServer == false) {
      isWebServer = true;
      initWebServer();
    }
    vTaskDelay(pdMS_TO_TICKS(15));
  }
}