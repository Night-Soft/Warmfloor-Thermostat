
#include "NtpTime.h"

#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

static const char* TAG_NTP = "NTP";

uint16_t getTimeZoneOffset() {
  time_t now;
  time(&now);
  struct tm tInfo;
  localtime_r(&now, &tInfo);

  time_t timestamp = timegm(&tInfo);
  printf("timestamp: %llu, now: %llu\n", timestamp, now);

  return (uint16_t)(timestamp - now);
}

void printTi();  // todo remove

uint32_t getEpochTime();

void (*timeReadyCallback)(bool, uint32_t) = nullptr;
void printTi();
bool isTimeSync = false;
void onTimeSync(timeval* tv) {
  isTimeSync = true;
  printf("Ntp time successfully received!\n");
  if (timeReadyCallback != nullptr) {
    timeReadyCallback(true, getEpochTime());
  }
}

void initialize_and_get_ntp_time(void) {
  // 4. Set your local timezone (e.g., Eastern Standard Time / New York)
  // Find your TZ string here: https://github.com
  setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
  tzset();

  ESP_LOGI(TAG_NTP, "Initializing SNTP...");
  // esp_sntp_restart(); // get Time now
  // 1. Configure the SNTP service using the default macro and your preferred
  // NTP server
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  config.start = true;
  config.sync_cb = onTimeSync;

  esp_netif_sntp_init(&config);
  // esp_netif_sntp_start;
  //  3. Wait for the time to synchronize (Timeout set to 10 seconds)
  int retry = 0;
  const int retry_count = 10;
  while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) != ESP_OK &&
         ++retry < retry_count) {
    ESP_LOGI(TAG_NTP, "Waiting for system time to be set... (%d/%d)", retry,
             retry_count);
  }

  // Check if we successfully got the time
  if (retry == retry_count) {
    ESP_LOGE(TAG_NTP, "Failed to get NTP time within timeout.");
    return;
  }

  // 5. Read and print the synchronized local time
  // printTi();
}

void printTi() {
  time_t now;
  struct tm timeinfo;
  char strftime_buf[64];

  time(&now);
  localtime_r(&now, &timeinfo);
  ESP_LOGI(TAG_NTP, "\n%02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min,
           timeinfo.tm_sec);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG_NTP, "The current date/time is: %s, unix: %llu", strftime_buf,
           now);
}

#define YEAR_2020 1577836800

bool isTimeSynced() { return time(nullptr) > YEAR_2020; }

bool isWifiEvent = false;
static void onWifiEvent(ON_WIFI_EVENT ev) {
  isWifiEvent = true;
  if (ON_WIFI_EVENT::ErrorConnect == ev) {
    printf("ERror init ntp, no wifi\n");
    timeReadyCallback(false, 0);
    return;
  }

  if (esp_sntp_enabled() == false) {
    printf("Init ntp time\n");
    initialize_and_get_ntp_time();
  } else {
    printf("Restart ntp time\n");
    esp_sntp_restart();
  }
}

uint32_t getEpochTime() {
  time_t now;
  time(&now);

  return now > YEAR_2020 ? now + getTimeZoneOffset() : 0;
}

NtpTime::NtpTime() { isWifiEvent = false; }

NtpTime::~NtpTime() {
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
    esp_netif_sntp_deinit();
  }
  if (this->wifiHolder != nullptr) {
    delete this->wifiHolder;
    this->wifiHolder = nullptr;
  }
}
// init wifi connect
bool NtpTime::init() {
  wifiHolder = new WiFiHolder();
  wifiHolder->turnOn();
  wifiHolder->onEvent(ON_WIFI_EVENT::Connected, onWifiEvent);

  return true;
}

void NtpTime::onTimeEvent(void (*callback)(bool, uint32_t)) {
  timeReadyCallback = callback;
  if (isWifiEvent) {
    callback(isTimeSynced(), getEpochTime());
  }
}

void NtpTime::print() {
  // printf(ntpTime->getFormattedTime());
  // todo
}