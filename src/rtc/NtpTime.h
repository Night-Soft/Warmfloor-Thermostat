#ifndef NTP_TIME_H
#define NTP_TIME_H

// #include <ESP8266WiFi.h>
// #include <NTPClient.h>

#include <ctype.h>

#include "../wifi/wifi.h"

class NtpTime {
 private:
  WiFiHolder* wifiHolder = nullptr;
  // NTPClient* ntpTime = nullptr;

 public:
  void onTimeEvent(void (*callback)(bool, uint32_t));
  bool init();
  void print();
  NtpTime();
  ~NtpTime();
};

#endif