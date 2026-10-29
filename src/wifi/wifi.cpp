#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <cstring>

#include "wifi.h"
#include "../display.h"

void postTurnOffWifi();
void createAP();

WifiEvents wifiEvents[32];
uint8_t wifiEventsCount = 0;

static constexpr char SSID_AP[] = "ESP32";
static const char *TAG = "wifi";
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0

void initialize_and_get_ntp_time();
void printTi();

void callWifiEvents(ON_WIFI_EVENT ev) {
  for (int i = 0; i < wifiEventsCount; i++) {
    if (wifiEvents[i].event == ev) {
      wifiEvents[i].calback(ev);
    }
    if (wifiEvents[i].event == ON_WIFI_EVENT::Any) {
      wifiEvents[i].calback(ev);
    }
  }
}

bool apStarted = false;
static void event_handler(void* arg, esp_event_base_t base, int32_t id,
                          void* data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_AP_START) {
    apStarted = true;
    ESP_LOGW(TAG, "apStarted");
    callWifiEvents(ON_WIFI_EVENT::APStart);
    // todo
  }
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();  // todo
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_event_sta_disconnected_t* e = (wifi_event_sta_disconnected_t*)data;
    ESP_LOGW(TAG, "Disconnected, reason=%d", e->reason);
    callWifiEvents(ON_WIFI_EVENT::Disconnected);

  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    // // Ждём подключения
    // xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE,
    // pdTRUE,
    //                     portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to %s", ssid);

    // ESP_LOGI(TAG, "Next init ntp time");
    // initialize_and_get_ntp_time();
  }
}

void postTurnOffWifi(TimerHandle_t timerWiFiOff);

static TimerHandle_t timerWiFiOff = nullptr;
static TimerHandle_t timerWaitingWiFi = nullptr;


void postTurnOffWifi(TimerHandle_t timerWiFiOff) {
      turnOffWiFi();
}


#define LED_BUILTIN GPIO_NUM_8
enum class WIFI_STATE : uint8_t {
  wifi_enabled, // never return
  wifi_disabled,
  sta_connected, // use
  sta_disabled,
  ip_address0, // if 0 error in connection
};

wifi_mode_t getWifiMode() {
  static wifi_mode_t mode;
  esp_wifi_get_mode(&mode);
   return mode;
}

WIFI_STATE getWifiState() {
  wifi_mode_t mode;
  if (esp_wifi_get_mode(&mode) != ESP_OK) return WIFI_STATE::wifi_disabled;
  if (mode == WIFI_MODE_NULL) return WIFI_STATE::wifi_disabled;

  wifi_ap_record_t apInfo;
  if (esp_wifi_sta_get_ap_info(&apInfo) != ESP_OK)
    return WIFI_STATE::sta_disabled;

  esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif == nullptr) return WIFI_STATE::ip_address0;

  esp_netif_ip_info_t ipInfo;
  if (esp_netif_get_ip_info(netif, &ipInfo) != ESP_OK)
    return WIFI_STATE::ip_address0;

  return ipInfo.ip.addr != 0 ? WIFI_STATE::sta_connected
                             : WIFI_STATE::ip_address0;
}

void toggleWifiUi(bool value) {
  gpio_set_level(LED_BUILTIN, !value);
  display.toggleWiFi(value);
}

#define MAX_WAITING_WIFI_TIME 15000
uint32_t waitingWifiTime = 0;
void waitingConnectToWifi(TimerHandle_t xTimer) {  // auto reload 150ms
  bool isConnected = false;

  if (getWifiMode() == WIFI_MODE_STA) {
    isConnected = getWifiState() == WIFI_STATE::sta_connected;
  } else if (getWifiMode() == WIFI_MODE_AP) {
    isConnected = apStarted;
  } else {
    return;
  }

  static bool toggle = true;

  toggleWifiUi(toggle);


  if (!isConnected && millis() - waitingWifiTime > MAX_WAITING_WIFI_TIME) {
    toggle = false;
    toggleWifiUi(false);

    printf("\nFailed connect to %s!\n", ssid);
    printf("Wifi failed status: %d\n", (int)(getWifiState()));
    esp_wifi_disconnect();

    xTimerStop(timerWaitingWiFi, 0);

    callWifiEvents(ON_WIFI_EVENT::ErrorConnect);
    return;
  }

  toggle = !toggle;
  display.forceUpdade();

  if (isConnected == false) return;

  toggle = true;
  toggleWifiUi(true);

  printf("\n%s connected!\n", ssid);
  callWifiEvents(ON_WIFI_EVENT::Connected);
  xTimerStop(timerWaitingWiFi, 0);
}

// create the network stack and event loop — initialization should be performed once.
void initNetworkStack() {
  // NVS нужен для хранения калибровочных данных Wi-Fi
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

esp_netif_t * netifAP = nullptr;
esp_netif_t * netifSTA = nullptr;

void createAP() {
  if(netifSTA != nullptr) {
    esp_netif_destroy_default_wifi(netifSTA);
    netifSTA = nullptr;
  }
  // Создаём сетевой интерфейс точки доступа
  netifAP = esp_netif_create_default_wifi_ap(); 

  // Инициализация Wi-Fi
  // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  // ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Настройки точки доступа
  wifi_config_t wifi_config = {};

  strcpy((char*)wifi_config.ap.ssid, SSID_AP);
  // strcpy((char*)wifi_config.ap.password, "12345678");

  wifi_config.ap.ssid_len = strlen(SSID_AP);
  wifi_config.ap.channel = 1;
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  // wifi_config.ap.ssid_hidden = 0;

  // Запуск в режиме точки доступа
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(34));  // ?

  ESP_LOGE(TAG, "Wi-Fi AP started");
  ESP_LOGE(TAG, "SSID: %s", SSID_AP);
  ESP_LOGE(TAG, "IP: 192.168.4.1");
}

// connect to exist wifi ap
void createSTA() {
  if (netifAP != nullptr) {
    esp_netif_destroy_default_wifi(netifAP);
    netifAP = nullptr;
  }

  netifSTA = esp_netif_create_default_wifi_sta();

  wifi_config_t wifi_config = {};
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;  // WIFI_AUTH_WPA2_PSK;
  memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
  memcpy(wifi_config.sta.password, password, strlen(password));

  printf("ssid: %s | %u, pass: %s | %u\n", wifi_config.sta.ssid,
         sizeof(wifi_config.sta.ssid), wifi_config.sta.password,
         sizeof(wifi_config.sta.password));

  memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));
  memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));

  memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
  memcpy(wifi_config.sta.password, password, strlen(password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(34));  // ?
}

bool connectToWiFi() {
  if (isWiFiConnected()) return true;

  printf("\nConnecting");

  initNetworkStack();
  // createAP();
  createSTA();

  return true;
}

bool isWiFiConnected() { return getWifiState() == WIFI_STATE::sta_connected; }

void turnOffWiFi() {
  printf("turnOffWiFi\n");
  esp_wifi_disconnect();
  esp_wifi_stop();  // turn off, stop driver. For reload call esp_wifi_start();
  vTaskDelay(pdMS_TO_TICKS(100));
  // gpio_set_level(LED_BUILTIN, 1);  // turn off led
  gpio_reset_pin(LED_BUILTIN);
  display.toggleWiFi(false);
}

// const char * str1 = "string"; // const относится к char, а не к указателю
// const char * const str2 = "string2"; // константный указатель на константные символы
// char * str3 = "string3";

const char* getIp() {
  static const char* wifiStaDef = "WIFI_STA_DEF";
  static const char* wifiApDef = "WIFI_AP_DEF";
  static char wifiIp[16];

  const char* key;
  if (getWifiMode() == WIFI_MODE_STA) {
    key = wifiStaDef;
  } else if (getWifiMode() == WIFI_MODE_AP) {
    key = wifiApDef;
  } else {
    ESP_LOGI(TAG, "No ip, WIFI not started!");
    return "";
  }

  memset(wifiIp, 0, sizeof(wifiIp));

  esp_netif_t* netif = esp_netif_get_handle_from_ifkey(key);

  if (netif != nullptr) {
    esp_netif_ip_info_t ip_info = {};

    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
      ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&ip_info.ip));

      uint32_t* ipaddr = &ip_info.ip.addr;
      uint8_t offset = 0;
      const char *fmt = "%d.", *fmt3 = "%d";
      const char* curFmt = fmt;

      for (int i = 0; i < 4; i++) {
        uint8_t value = ((const uint8_t*)(ipaddr))[i];
        printf("%d", value);

        uint8_t numInVal = 1;
        uint8_t number = value;

        do {
          number /= 10;
          numInVal++;
        } while (number != 0);

        if (i == 3) {
          curFmt = fmt3;
          numInVal--;
        }

       snprintf((wifiIp + offset), sizeof(wifiIp) - offset, curFmt, value);
       offset += numInVal;
      }
    }
    printf("\nwifiIp: %s\n", wifiIp);
  }

  return wifiIp;
}

int WiFiHolder::counter = 0;

WiFiHolder::WiFiHolder() {
  if (timerHas(timerWiFiOff)) {
    printf("Cancel turnOff wifi, remove task");
    xTimerStop(timerWiFiOff, 0);
  }
  WiFiHolder::counter++;
  printf("Count of WiFiHolder is: %d\n", WiFiHolder::counter);
}

WiFiHolder::~WiFiHolder() {
  for (int i = 0; i < wifiEventsCount; i++) {
    if (wifiEvents[i].holder != this) continue;

    WifiEvents* dest = &wifiEvents[i];
    WifiEvents* src = &wifiEvents[i + 1];
    size_t size = sizeof(WifiEvents) * (wifiEventsCount - i - 1);
    if (size > 0) {
      memcpy(dest, src, size);
    }
    wifiEventsCount--;
  }

  WiFiHolder::counter--;
  printf("Remove WiFiHolder, left: %d\n", WiFiHolder::counter);

  if (WiFiHolder::counter == 0) {
    printf("~WiFiHolder: try add turnOffWiFi for 5000\n");
    setTimeout(5000, timerWiFiOff, postTurnOffWifi);
    // todo delete timer
  }
}

void WiFiHolder::turnOff() { turnOffWiFi(); }
void WiFiHolder::createAP() { ::createAP(); }
const char* WiFiHolder::getIp() { return ::getIp(); }

void WiFiHolder::turnOn() {
  if (timerHas(timerWaitingWiFi)) {
    printf("waitingConnectToWifi, add event...\n");
    return;
  }
  if (timerHas(timerWiFiOff)) {
    xTimerStop(timerWiFiOff, 0);
  }

  gpio_reset_pin(LED_BUILTIN);
  gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);
  waitingWifiTime = millis();
  setInterval(150, timerWaitingWiFi, waitingConnectToWifi);
  connectToWiFi();
}

bool WiFiHolder::isConnected() { return isWiFiConnected(); }

void WiFiHolder::onEvent(ON_WIFI_EVENT event, WifiEventCalback callback) {
  uint8_t idx = wifiEventsCount++;
  wifiEvents[idx].event = event;
  wifiEvents[idx].calback = callback;
  wifiEvents[idx].holder = this;
}