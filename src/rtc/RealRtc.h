#ifndef REAL_RTC_H
#define REAL_RTC_H

#include <esp_log.h>
#include <string.h>

#include "../lib/rtc/RtcDS1307.h"
#include "../lib/rtc/compat/ComWire.h"  
#include "../utils.h"
#include "NtpTime.h"

using uint32 = uint32_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint64 = uint64_t;

#define snprintf_P snprintf
#define Ser_print(...) printf(__VA_ARGS__)
#define Ser_println(...) \
  do {                      \
    printf(__VA_ARGS__);    \
    printf("\n");           \
  } while (0)

// The sum time of one day
class CurrentTime {
 public:
  uint32 sumSeconds;
  uint8 hour, minutes, seconds;

  static CurrentTime get(const RtcDateTime &rtcdateTime);
  static CurrentTime get(uint32 seconds);
};

class RealRtc {
  private:
    bool needUpdateTime();
    void updateLastSetNtpTime(uint64 time);
    
    unsigned long computedTime = 0;
    unsigned long lastMillis = 0;

 public:
  RealRtc();
  ~RealRtc();

  NtpTime* ntp = nullptr;
  RtcDS1307<ComWire> rtc;

  void init();
  bool isRtcCorrect();
  bool setTimeFromCompile();
  RtcDateTime getTime();
  CurrentTime getCurrentTime();
  CurrentTime getComputedTime();
  void updateComputedTime();
  void printRtcTime();
  void printComputedTime();
  void printTime(const uint32 seconds);
  void printTime(const CurrentTime &time);
  void printTime(const RtcDateTime *dateTime = nullptr);

  uint64 getLastSetNtpTime();
  // bool setTimeFromNtp();
  bool setTimeFromUnix(uint64 &time);
  void initNtpTime();
  void deleteNtpTime();
};

extern RealRtc realTime;
#endif