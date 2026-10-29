#ifndef TIME_RANGE_H
#define TIME_RANGE_H

// #include <Arduino.h>
#include "lib/FileData/FileData.h"

//static constexpr uint8_t JSON_COLS = 4;

struct Range {
  uint8_t startHour;
  uint8_t startMinute;
  uint8_t durationHour;
  uint8_t durationMinute;
};

struct NextWakeUp {
  uint8_t hour, minutes;
};

using Ranges = Range*; 

void printTimeRanges(uint8_t* jsonData, uint8_t len);
void printTimeRanges(Ranges& ranges, uint8_t len);

class TimeRange {
  bool isUpdate = true; // after write show what is new time ranges but only in rom
  bool isRead = false;
 public:
  TimeRange();
  ~TimeRange();
  uint8_t size = 0;
  Ranges ranges = nullptr;
  char * json = nullptr;
  FDstat_t status = FD_NO_DIF;

  uint16_t getWorkingTime(Range& r, uint8_t hour, uint8_t minutes);
  // the time for pump on
  uint16_t getWorkingTime(uint8_t hour, uint8_t minutes);
  // totalSeconds is current time (hours * 3600 + minutes * 60 + seconds), 
  // returns the amount of time( in seconds) remaining from the current moment until the next time-range
  uint32_t getTimeToNextRange(const uint32_t totalSeconds);
  NextWakeUp getNextWakeUp(uint8_t curHours, uint8_t curMinutes);
  void read();

  FDstat_t write(uint8_t* jsonData, uint8_t len);
};

extern TimeRange timeRanges;
#endif