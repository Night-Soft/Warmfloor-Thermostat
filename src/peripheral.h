#ifndef PERIPHERAL_H
#define PERIPHERAL_H

#include <driver/i2c_master.h>
#include <esp_log.h>

i2c_master_bus_handle_t i2cBus = nullptr;

void initI2cBus() {
  i2c_master_bus_config_t busConfig = {};

  busConfig.i2c_port = I2C_NUM_0;
  busConfig.sda_io_num = GPIO_NUM_5;
  busConfig.scl_io_num = GPIO_NUM_9;
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;

  ESP_ERROR_CHECK(i2c_new_master_bus(&busConfig, &i2cBus));
}

static i2c_master_dev_handle_t oledDevice = nullptr;
static i2c_master_dev_handle_t rtcDevice = nullptr;

void initI2cDevices() {
  i2c_device_config_t oledConfig = {};
  oledConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  oledConfig.device_address = 0x3C;
  oledConfig.scl_speed_hz = 400000;

  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2cBus, &oledConfig, &oledDevice));

  i2c_device_config_t rtcConfig = {};
  rtcConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  rtcConfig.device_address = 0x68;
  rtcConfig.scl_speed_hz = 100000;

  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2cBus, &rtcConfig, &rtcDevice));
}
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <cstring>

static const char WIFI_SSID[] = "TP-LINK_112900";
static const char WIFI_PASS[] = "QTBz59WB";

static const char *TAG_WIFI = "wifi";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
void initialize_and_get_ntp_time();
void printTi();
static void event_handler(void* arg, esp_event_base_t base, int32_t id,
                          void* data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_event_sta_disconnected_t* e = (wifi_event_sta_disconnected_t*)data;
    ESP_LOGW(TAG, "Disconnected, reason=%d", e->reason);
    esp_wifi_connect();
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)data;
    ESP_LOGI(TAG_WIFI, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    //printTi();
    ESP_LOGI(TAG_WIFI, "Next init ntp time");
    initialize_and_get_ntp_time();
  }
}

void init_wifi(void)
{
    // NVS нужен для хранения калибровочных данных Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {};
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;//WIFI_AUTH_WPA2_PSK;
    memcpy(wifi_config.sta.ssid, WIFI_SSID, sizeof(WIFI_SSID));
    memcpy(wifi_config.sta.password, WIFI_PASS, sizeof(WIFI_PASS));

    printf("ssid: %s | %u, pass: %s | %u\n", wifi_config.sta.ssid,
           sizeof(wifi_config.sta.ssid), wifi_config.sta.password,
           sizeof(wifi_config.sta.password));

    memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));
    memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));

    memcpy(wifi_config.sta.ssid, WIFI_SSID, sizeof(WIFI_SSID));
    memcpy(wifi_config.sta.password, WIFI_PASS, sizeof(WIFI_PASS));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(34)); // ?
    // Ждём подключения
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                        portMAX_DELAY);
    ESP_LOGI(TAG_WIFI, "Connected to %s", WIFI_SSID);
}

// #include <cstring>

// #include "esp_wifi.h"
// #include "esp_event.h"
// #include "esp_netif.h"
// #include "nvs_flash.h"
// #include "esp_log.h"

// static const char *TAGS = "WIFI";

// static const char *WIFI_SSID = "TP-LINK_112900";
// static const char *WIFI_PASSWORD = "QTBz59WB";

// static void wifi_event_handler(
//     void *arg,
//     esp_event_base_t event_base,
//     int32_t event_id,
//     void *event_data)
// {
//     if (event_base == WIFI_EVENT) {
//         switch (event_id) {
//         case WIFI_EVENT_STA_START:
//             ESP_LOGI(TAGS, "Wi-Fi started");
//             break;

//         case WIFI_EVENT_STA_DISCONNECTED:
//             ESP_LOGW(TAGS, "Wi-Fi disconnected");
//             break;

//         default:
//             break;
//         }
//     }

//     if (event_base == IP_EVENT &&
//         event_id == IP_EVENT_STA_GOT_IP) {

//         auto *event =
//             static_cast<ip_event_got_ip_t *>(event_data);

//         ESP_LOGI(TAGS, "IP: " IPSTR,
//                  IP2STR(&event->ip_info.ip));
//     }
// }

// void wifi_init_sta()
// {
//     // 1. Инициализация NVS
//     esp_err_t ret = nvs_flash_init();

//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
//         ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ESP_ERROR_CHECK(nvs_flash_init());
//     } else {
//         ESP_ERROR_CHECK(ret);
//     }

//     // 2. TCP/IP и event loop
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());

//     // 3. Сетевой интерфейс клиента
//     esp_netif_create_default_wifi_sta();

//     // 4. Инициализация драйвера Wi-Fi
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     // 5. Регистрация обработчика событий
//     ESP_ERROR_CHECK(esp_event_handler_register(
//         WIFI_EVENT,
//         ESP_EVENT_ANY_ID,
//         wifi_event_handler,
//         nullptr
//     ));

//     ESP_ERROR_CHECK(esp_event_handler_register(
//         IP_EVENT,
//         IP_EVENT_STA_GOT_IP,
//         wifi_event_handler,
//         nullptr
//     ));

//     // 6. Настройка режима клиента
//     wifi_config_t wifi_config = {};

//     std::strncpy(
//         reinterpret_cast<char *>(wifi_config.sta.ssid),
//         WIFI_SSID,
//         sizeof(wifi_config.sta.ssid)
//     );

//     std::strncpy(
//         reinterpret_cast<char *>(wifi_config.sta.password),
//         WIFI_PASSWORD,
//         sizeof(wifi_config.sta.password)
//     );

//     ESP_ERROR_CHECK(
//         esp_wifi_set_mode(WIFI_MODE_STA)
//     );

//     ESP_ERROR_CHECK(
//         esp_wifi_set_config(WIFI_IF_STA, &wifi_config)
//     );

//     // 7. Запуск Wi-Fi
//     ESP_ERROR_CHECK(esp_wifi_start());
//     esp_wifi_set_max_tx_power(34);  // 19.5dbm
//     // 8. Отключаем энергосбережение для тестирования
//     //ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

//     ESP_LOGI(TAGS, "Wi-Fi initialized");
// }

// #include "esp_err.h"

// // static const char* TAGS = "WIFI_SCAN";

// void wifi_scan() {
//   // Настройки сканирования
//   wifi_scan_config_t scan_config = {};
//   scan_config.ssid = nullptr;
//   scan_config.bssid = nullptr;
//   scan_config.channel = 0;  // Все каналы
//   scan_config.show_hidden = false;

//   ESP_LOGI(TAGS, "Scanning Wi-Fi networks...");

//   // Синхронное сканирование: ждём завершения
//   ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

//   // Получаем количество найденных сетей
//   uint16_t ap_count = 0;

//   ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

//   if (ap_count == 0) {
//     ESP_LOGW(TAGS, "No networks found");
//     return;
//   }

//   // Выделяем память для записей
//   wifi_ap_record_t* ap_records = new wifi_ap_record_t[ap_count];

//   if (ap_records == nullptr) {
//     ESP_LOGE(TAGS, "Memory allocation failed");
//     return;
//   }

//   uint16_t max_records = ap_count;

//   esp_err_t err = esp_wifi_scan_get_ap_records(&max_records, ap_records);

//   if (err == ESP_OK) {
//     ESP_LOGI(TAGS, "Found %u networks:", max_records);

//     for (uint16_t i = 0; i < max_records; ++i) {
//       const wifi_ap_record_t& ap = ap_records[i];

//       ESP_LOGI(TAGS, "%2u. SSID: %-32s RSSI: %d dBm CH: %u AUTH: %d", i + 1,
//                reinterpret_cast<const char*>(ap.ssid), ap.rssi, ap.primary,
//                static_cast<int>(ap.authmode));
//     }
//   } else {
//     ESP_LOGE(TAGS, "Scan results error: %s", esp_err_to_name(err));
//   }

//   delete[] ap_records;
// }



// #include <time.h>
// #include <sys/time.h>
// #include "esp_log.h"
// #include "esp_netif_sntp.h"
// #include "esp_sntp.h"

// static const char* TAG_NTP = "NTP_EXAMPLE";
// uint16_t getTimeZoneOffset() { // todo
//   time_t now;
//   time(&now);

//   struct tm local = {};
//   struct tm localUtc = {};
//   localtime_r(&now, &local);
//   gmtime_r(&now, &localUtc);
//   time_t sumUtcTime = 0, sumLocalTime = 0;

//   sumUtcTime += localUtc.tm_sec;
//   sumUtcTime += localUtc.tm_min + 60;
//   sumUtcTime += localUtc.tm_hour * 3600;

//   sumLocalTime += local.tm_sec;
//   sumLocalTime += local.tm_min + 60;
//   sumLocalTime += local.tm_hour * 3600;
  
//   return sumLocalTime - sumUtcTime;
// }

// void printTi();
// bool isTimeSync = false;
// void onTimeSync(timeval* tv) { isTimeSync = true; }
// // void ntpForceSync()
// // {
// //     if (esp_sntp_enabled()) {
// //         esp_sntp_restart();
// //     }
// // }
// void initialize_and_get_ntp_time(void)
// {
//     // 4. Set your local timezone (e.g., Eastern Standard Time / New York)
//     // Find your TZ string here: https://github.com
//     setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
//     tzset();

//     ESP_LOGI(TAG_NTP, "Initializing SNTP...");
//     // esp_sntp_restart(); // get Time now
//     // 1. Configure the SNTP service using the default macro and your preferred NTP server
//     esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
//     config.start = true;
//     sntp_set_time_sync_notification_cb(onTimeSync);

//     esp_netif_sntp_init(&config);
//     //esp_netif_sntp_start;
//     // 3. Wait for the time to synchronize (Timeout set to 10 seconds)
//     int retry = 0;
//     const int retry_count = 10;
//     while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) != ESP_OK && ++retry < retry_count) {
//         ESP_LOGI(TAG_NTP, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
//     }

//     // Check if we successfully got the time
//     if (retry == retry_count) {
//         ESP_LOGE(TAG_NTP, "Failed to get NTP time within timeout.");
//         return;
//     }


//     // 5. Read and print the synchronized local time
//    // printTi();
// }

// void printTi() {
//   time_t now;
//   struct tm timeinfo;
//   char strftime_buf[64];

//   time(&now);
//   localtime_r(&now, &timeinfo);

//   strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
//   ESP_LOGI(TAG_NTP, "The current date/time is: %s", strftime_buf);
// }

// bool isTimeSynced()
// {
//     return time(nullptr) > 1577836800;
// }

#endif