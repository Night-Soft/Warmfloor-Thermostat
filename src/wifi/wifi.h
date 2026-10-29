#ifndef WI_FI_H
#define WI_FI_H

#include <cstdint>

#include "secret.h"
#include "esp_wifi.h"

bool connectToWiFi();
bool isWiFiConnected();
void turnOffWiFi();

enum class ON_WIFI_EVENT : uint8_t {
  Any, // call one callback for diff events
  Connected,
  Disconnected,
  APStart,
  ErrorConnect,
};

using WifiEventCalback = void (*)(ON_WIFI_EVENT ev);

class WiFiHolder {
 public:
  WiFiHolder();
  ~WiFiHolder();
  void turnOff();
  void turnOn();
  void createAP();
  const char* getIp();
  bool isConnected();
  void onEvent(ON_WIFI_EVENT event, WifiEventCalback callback);

  static int counter;
};

struct WifiEvents {
  WifiEventCalback calback;
  ON_WIFI_EVENT event;
  WiFiHolder *holder;
};

#endif