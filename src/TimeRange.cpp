#include <esp_log.h>
#include <ArduinoJson.h>

#include "TimeRange.h"
#include "littlefs.h"

TimeRange::TimeRange(){}

TimeRange::~TimeRange() {
  if (json != nullptr) delete[] json;
  if (ranges != nullptr) delete[] ranges;
}

uint16_t TimeRange::getWorkingTime(Range& r, uint8_t hour, uint8_t minutes) {
  uint16_t now = hour * 60 + minutes;
  uint16_t start = r.startHour * 60 + r.startMinute;
  uint16_t end = r.durationHour * 60 + r.durationMinute + start;
  return (now >= start && now < end) ? end - now : 0;
}

// the time for pump on in minutes
uint16_t TimeRange::getWorkingTime(uint8_t hour, uint8_t minutes) {
  if (isRead == false) read();

  uint16_t result = 0;
  for (uint8_t i = 0; i < size; i++) {
    result = getWorkingTime(ranges[i], hour, minutes);
    if (result > 0) return result;
  }
  return result;
}

// returns the amount of time remaining from the current moment until the next time-range
uint32_t TimeRange::getTimeToNextRange(uint32_t now) {
  uint32_t nextStart = 0xffffffff;
  read();
  printf("\nNearestTime: ");

  for (uint8_t i = 0; i < size; i++) {
    uint32_t start = ranges[i].startHour * 3600 + ranges[i].startMinute * 60;
    if (start < now) start += 24 * 3600;
    printf("%lu, ", start - now);
    if (start - now > nextStart) continue;
    nextStart = start - now;
  }
  printf("\n");

  return nextStart;
}

// todo from seconds
NextWakeUp TimeRange::getNextWakeUp(uint8_t curHours, uint8_t curMinutes) {
  if (isRead == false) read();

  NextWakeUp wakeUp{};
  for (uint8_t i = 0; i < size; i++) {
    uint16_t res = getWorkingTime(ranges[i], curHours, curMinutes);
    if (res == 0) continue;
    wakeUp.hour = ranges[i].startHour;
    wakeUp.minutes = ranges[i].startMinute;
    break;
  }

  return wakeUp;
}

void printTimeRanges(uint8_t* jsonData, uint8_t len) {
  JsonDocument doc;
  deserializeJson(doc, jsonData, len);

  JsonArray arr = doc.as<JsonArray>();
  printf("Print time ranges from json:\n"); 
  for (size_t i = 0; i < arr.size(); i++) {
    for (JsonVariant item : arr[i].as<JsonArray>()) {
      printf("%d, ", item.as<int>());
    }
      printf("\n");
  }
}

void printTimeRanges(Ranges& ranges, uint8_t len) {
  printf("Print time ranges from Range:\n");
  for (uint8_t i = 0; i < len; i++) {
    printf("sH: %d, ", ranges[i].startHour);
    printf("sM: %d, ", ranges[i].startMinute);
    printf("dH: %d, ", ranges[i].durationHour);
    printf("dM: %d, ", ranges[i].durationMinute);
    printf("\n");
  }
}

FDstat_t TimeRange::write(uint8_t* jsonData, uint8_t len) {
  FileData listTimeSize(&LittleFS, "/listTurnOnTimeSize.dat", 'c', &len, sizeof(len));
  FileData listTime(&LittleFS, "/listTurnOnTime.dat", 'B', jsonData, len);

  listTimeSize.updateNow();
  FDstat_t stat = listTime.updateNow();
  if (stat == FDstat_t::FD_FILE_ERR) {
    printf("Create new files: listTurnOnTimeSize, listTurnOnTime.\n");
    listTimeSize.write();
    stat = listTime.write();
  }

  const char error[] = "Error";
  const char success[] = "Success";
  const char* statStr = stat == FDstat_t::FD_FILE_ERR ? error : success;

  printf(statStr);
  printf(" writing time ranges!\n");

  isUpdate = true;
  isRead = false;
  return stat;
}

void TimeRange::read() {
  if (isUpdate) {
    if (json != nullptr) delete[] json;
    if (ranges != nullptr) delete[] ranges;
    isUpdate = false;
  } else {
    if (json != nullptr && ranges != nullptr) return;
  }

  printf("Read time ranges.\n");
  printf("Read /listTurnOnTimeSize.dat\n");
  
  size_t size = 0;
  FileData(&LittleFS, "/listTurnOnTimeSize.dat", 'c', &size, sizeof(uint8_t))
      .read();
  if (size == 0) {
    printf("File 'listTurnOnTimeSize.dat' is empty!\n");
    return;
  }
  isRead = true;
  printf("listTurnOnTimeSize size: %u\n", size);

  printf("Read /listTurnOnTime.dat\n");

  this->json = new char[size + 1]();
  FileData data(&LittleFS, "/listTurnOnTime.dat", 'B', this->json, size);
  this->status = data.read();

  JsonDocument doc;
  deserializeJson(doc, this->json, size);
  JsonArray arr = doc.as<JsonArray>();

  this->size = arr.size();
  this->ranges = (Ranges)new Range[arr.size()];
  uint8_t* rPtr = (uint8_t*)this->ranges;
  uint8_t idx = 0;
  for (size_t i = 0; i < arr.size(); i++) {
    for (JsonVariant item : arr[i].as<JsonArray>()) {
      rPtr[idx++] = item.as<uint8_t>();
    }
  }
}

TimeRange timeRanges;