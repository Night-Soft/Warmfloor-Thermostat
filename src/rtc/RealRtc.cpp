#include "../lib/FileData/FileData.h"
#include "../littlefs.h"

#include "RealRtc.h"
#include "CompilationTime.h"

ComWire wire;

unsigned int getMaxTime(unsigned int a, unsigned int b, unsigned int c) {
  if (a > b && a > c) return a;
  if (b > a && b > c) return b;
  if (c > a && c > b) return c;
  return a;
}

CurrentTime CurrentTime::get(const RtcDateTime& time) {
  uint32 sumSeconds = time.Hour() * 3600 + time.Minute() * 60 + time.Second();
  return CurrentTime{.sumSeconds = sumSeconds,
                     .hour = time.Hour(),
                     .minutes = time.Minute(),
                     .seconds = time.Second()};
}

CurrentTime CurrentTime::get(uint32 seconds) {
  RtcDateTime time = RtcDateTime(seconds);
  return CurrentTime::get(time);
}

RealRtc::RealRtc() : rtc(wire) { }
void setTimeFromNtp(RealRtc *rtc);

void RealRtc::init() {
  // LittleFS.begin();
  // rtc.Begin(SDA, SCL);
  rtc.Begin();

  if (!isRtcCorrect()) {
    Ser_println(
        "RTC is NOT running, let's set the time! Or replace battery.");
    Ser_println("RTC was not actively running, starting now");
    rtc.SetIsRunning(true);
  }

  // never assume the rtc was last configured by you, so
  // just clear them to your needed state
  rtc.SetSquareWavePin(DS1307SquareWaveOut_Low);
  // delay(100);

  if (needUpdateTime()) {  //
    setTimeFromNtp(this); 
    // setTimeFromCompile();
    // uint64_t time = 1791928588 + 3 * 3600;
    // setTimeFromUnix(time);
  }
  Ser_print("\nCurrent rtc time: ");
  printTime();
  updateComputedTime();
  if (!isRtcCorrect()) {  // todo show on display
    Ser_println("Rtc error!");
  }

}

bool RealRtc::isRtcCorrect() {
  if (rtc.GetIsRunning() == false) return false;
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  
  uint32 unix2050 = 2528305866;
  int64_t timeSinceCompile =
      rtc.GetDateTime().Unix32Time() - compiled.Unix32Time();
      printf("\nrtc.GetDateTime().Unix32Time() - compiled.Unix32Time()\n");
      printf("%lu, %lu\n\n", rtc.GetDateTime().Unix32Time(), compiled.Unix32Time());
      return timeSinceCompile > 0 && timeSinceCompile < unix2050;
}

// check if a time update is needed if more than 24 hours have passed since the last update
bool RealRtc::needUpdateTime() {
  uint64 lastNtpTime = getLastSetNtpTime(); // 1792789376 = Saturday, October 24, 2026 at 00:02:56
  uint64 curUnixRtc = rtc.GetDateTime().Unix64Time();
  //uint64 timeFrom2000 = lastNtpTime - c_UnixEpoch32;
  // if (timeFrom2000) {
  //   printf("\nlastNtpTime: %llu, curUnixRtc: %llu \n", lastNtpTime,
  //                 curUnixRtc);
  //   RtcDateTime last = RtcDateTime(timeFrom2000);
  //   printTime(&last);
  // } else {
  //   printf("\nlastNtpTime < 0: %llu", timeFrom2000);
  // }

  unsigned long unix2050 = 2528305866;
  unsigned long unixCompile = UNIX_TIMESTAMP;
  if (curUnixRtc > unix2050 || curUnixRtc < unixCompile - 3600) {
    Ser_println("Rtc has wrong time, cheak your wire!");
    Ser_print("Rtc wrong time: ");
    printTime();
    return true;
  }

  Ser_println("\nRtcTime: ");
  printTime();

  uint32 day = 3600 * 24 * 7; // 259200
  return (lastNtpTime + day < curUnixRtc);
}

// unix time in seconds
uint64 RealRtc::getLastSetNtpTime() {
  uint64 lastNtpTime = 0;
  FileData data(&LittleFS, "/lastNtpTime.dat", 'A', &lastNtpTime, sizeof(lastNtpTime));
  data.read();
  printf("\nRead lastNtpTime: %llu\n", lastNtpTime);

  return lastNtpTime;
}

void RealRtc::updateLastSetNtpTime(uint64 unix64time) {
  FileData data(&LittleFS, "/lastNtpTime.dat", 'A', &unix64time, sizeof(unix64time));
  data.updateNow();
  printf("\nUpdate memory /lastNtpTime.dat: %llu\n", unix64time);
}

bool RealRtc::setTimeFromCompile() {
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  rtc.SetDateTime(compiled);  // save in ds1307
  updateComputedTime();
  printTime();
  return true;
}

RtcDateTime RealRtc::getTime() { return rtc.GetDateTime(); }

CurrentTime RealRtc::getCurrentTime() {
  return CurrentTime::get(rtc.GetDateTime());
}

CurrentTime RealRtc::getComputedTime() {
  unsigned long timePassed = millis() - lastMillis;
  uint32 seconds = (timePassed + computedTime) / 1000;

  return CurrentTime::get(RtcDateTime(seconds));
}

void RealRtc::updateComputedTime() {
  RtcDateTime currentRtc = rtc.GetDateTime();

  computedTime = currentRtc.Hour() * 3600 * 1000;
  computedTime += currentRtc.Minute() * 60 * 1000;
  computedTime += currentRtc.Second() * 1000;
  lastMillis = millis();
}

void RealRtc::printRtcTime() { 
  RtcDateTime dt = rtc.GetDateTime();
  printTime(&dt); 
}

void RealRtc::printComputedTime() { printTime(this->getComputedTime()); }
// Ser_print time as "D/M/Y | H:M:S", in current line
void RealRtc::printTime(const uint32 seconds) {
  RtcDateTime dateTime = RtcDateTime(seconds);
  printTime(&dateTime);
}

// Ser_print time as "D/M/Y | H:M:S", in current line
// print time of a one day, since 2000
void RealRtc::printTime(const CurrentTime &time) {
  RtcDateTime dateTime = RtcDateTime(time.sumSeconds);
  printTime(&dateTime);
}

// Ser_print time as "D/M/Y | H:M:S", in current line
void RealRtc::printTime(const RtcDateTime *dt) {
  RtcDateTime dataTime;
  if (dt == nullptr) {
    dataTime = rtc.GetDateTime();
    dt = &dataTime;
  }

  char datestring[28];

  snprintf_P(datestring, countof(datestring),
             "%02u/%02u/%04u | %02u:%02u:%02u", dt->Day(), dt->Month(), 
             dt->Year(), dt->Hour(), dt->Minute(), dt->Second());
  Ser_println(datestring);
}

void onWifiEv(ON_WIFI_EVENT ev) {
  if (ev != ON_WIFI_EVENT::Connected) {
        Ser_println("NtpTime: No WiFi, call '.init later.'");
       // delete this->wifiHolder;
        return;
  }

}

static NtpTime* ntp = nullptr;
static RealRtc* rtc = nullptr;
void ntpTimeReady(bool isTimeSet, uint32_t epochTime) {
  if(isTimeSet == false) {
    Ser_println("Error get Ntp time, may be no wifi");
    delete ntp;
    return;
  }
  
  unsigned long ntpUnixTime = epochTime;
  unsigned long compileUnixTime = UNIX_TIMESTAMP;

  if (ntpUnixTime > compileUnixTime) {
    Ser_println("SetTimeFromNtp: ");
    uint64 time = (uint64)ntpUnixTime;
    rtc->setTimeFromUnix(time);
  } else {
    RtcDateTime dataTime = RtcDateTime();
    dataTime.InitWithUnix32Time(ntpUnixTime);
    printf("\nError: setTimeFromNtp nNtpTime: %ld\n", ntpUnixTime);
    printf("ntpUnixTime: %lu, compileUnixTime: %lu\n", ntpUnixTime, compileUnixTime);
    Ser_print("\nNtpTime is: ");
    rtc->printTime(&dataTime);
  }

  Ser_print("currentRtcTime: ");
  rtc->printTime();

  delete ntp;
}

void setTimeFromNtp(RealRtc * rtc) {  // init wifi this one with wifiholder
  ::rtc = rtc;
  ntp = new NtpTime();
  ntp->init();
  ntp->onTimeEvent(ntpTimeReady);
}

bool RealRtc::setTimeFromUnix(uint64& time) {
  RtcDateTime dataTime = RtcDateTime();
  dataTime.InitWithUnix32Time(time);
  rtc.SetDateTime(dataTime);  // save in ds1307
  updateComputedTime();
  updateLastSetNtpTime(time);

  Ser_print("\n SetTimeFromUnix: ");
  printTime(&dataTime);

  return true;
}

void RealRtc::initNtpTime() { // todo no need
  if (ntp != nullptr) return;
  ntp = new NtpTime();
}

void RealRtc::deleteNtpTime() {
  if (ntp != nullptr) delete ntp;
  ntp = nullptr;
}

RealRtc::~RealRtc() { deleteNtpTime(); }

