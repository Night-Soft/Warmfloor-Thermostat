#include "Radio.h"

#define PIN_SCK 6
#define PIN_MISO 2
#define PIN_MOSI 7
#define PIN_NSS 10

#define PIN_RST 4 // may problem
#define PIN_DIO0 3   // PayloadReady в режиме FSK (обязателен)
#define PIN_DIO1 33  // не используется, но передаётся в конструктор

#define PIN_4 GPIO_NUM_4  // не используется, но передаётся в конструктор

const char* TAG = "MAIN_APP";
// ---------------- Параметры радио ----------------
const float FREQ_MHZ = 868.0;
const float BITRATE_KBPS = 2.4;
const float DEVIATION_KHZ = 36.0;
const float RX_BW_KHZ = 62.5;
const int8_t TX_POWER_DBM = 10;
const uint16_t PREAMBLE_BITS = 256;

const uint8_t SYNC_WORD[] = { 0x2D, 0xD4, 's', 'o', 'n', 'g'}; // 
//const uint8_t SYNC_WORD[] = { 0x2D, 0xD4};
const uint8_t PAYLOAD_LEN = 4;
const uint8_t PKT_LEN = 5;

SX1276* radio;

struct ThermostatCommand { // summer command
  uint8_t enable[PAYLOAD_LEN] = {0xD2, 0x79, 0xA1, 0xEC};
  uint8_t allreadyEnabled[PAYLOAD_LEN] = {0xD2, 0x79, 0xA2, 0xED};
  uint8_t disable[PAYLOAD_LEN] = {0xD2, 0x79, 0xA0, 0xEB};
  uint8_t allreadyDisabled[PAYLOAD_LEN] = {0xD2, 0x79, 0xA3, 0xEE};
} thmCmd;

struct ThermostatCommandWinter { 
  uint8_t enable[PAYLOAD_LEN] = {0xD2, 0x79, 0xA1, 0xEC};
  uint8_t allreadyEnabled[PAYLOAD_LEN] = {0xD2, 0x79, 0xA2, 0xED};
  uint8_t disable[PAYLOAD_LEN] = {0xD2, 0x79, 0xA0, 0xEB};
  uint8_t allreadyDisabled[PAYLOAD_LEN] = {0x73, 0x6f, 0x6e, 0x67};
};

const char* commandsStr[4] = {
    "enable",
    "allreadyEnabled",
    "disable",
    "allreadyDisabled",
};

enum class HEATING_STATE : uint8_t {
  ENABLED,
  ALLREADY_ENABLED,
  DISABLED,
  ALLREADY_DISABLED,
  UNDEFINED,
};

HEATING_STATE heatingState = HEATING_STATE::UNDEFINED;

volatile bool rxFlag = false;
void IRAM_ATTR onPacket() { rxFlag = true; }

void fail(const char* step, int state) {
  printf("Ошибка на шаге: %d", *step);
  printf(", код %d\n", state);
  while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}

void check(const char* step, int state) {
  if (state != RADIOLIB_ERR_NONE) fail(step, state);
}

void printHex(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) printf("0");
    printf("%02x ", data[i]);
  }
}

void setHeatingToUndefined(TimerHandle_t xTimer) {
  heatingState = HEATING_STATE::UNDEFINED;
 // gpio_set_level(GPIO_NUM_0, 0);
//  gpio_set_level(GPIO_NUM_1, 0);
}

TimerHandle_t timer;

void setupRFM95() {
  gpio_reset_pin((gpio_num_t)PIN_RST);

  EspHal* hal = new EspHal(PIN_SCK, PIN_MISO, PIN_MOSI);
  Module* module = new Module(hal, PIN_NSS, PIN_DIO0, PIN_RST);
  radio = new SX1276(module);

  ESP_LOGI(TAG, "RFM95 приёмник (формат RFM31B, проверка CRC)");

  check("beginFSK", radio->beginFSK(FREQ_MHZ, BITRATE_KBPS, DEVIATION_KHZ,
                                   RX_BW_KHZ, TX_POWER_DBM, PREAMBLE_BITS));
  check("setSyncWord",
        radio->setSyncWord((uint8_t*)SYNC_WORD, sizeof(SYNC_WORD)));
  check("setDataShaping", radio->setDataShaping(RADIOLIB_SHAPING_NONE));
  check("setEncoding", radio->setEncoding(RADIOLIB_ENCODING_NRZ));
  check("setCRC", radio->setCRC(false));  // проверяем CRC сами
  // check("variablePacketLengthMode", radio->variablePacketLengthMode(255));
  // first
  check("fixedPacketLengthMode", radio->fixedPacketLengthMode(PKT_LEN));

  radio->setPacketReceivedAction(onPacket);
  check("startReceive", radio->startReceive());

  ESP_LOGI(TAG, "Жду кадры...");

  timer = xTimerCreate("myTimer", pdMS_TO_TICKS(15000), pdFALSE, nullptr,
                       setHeatingToUndefined);
  xTimerStart(timer, 0);
}


void firstCheck() {
  if (!rxFlag) return;
  rxFlag = false;

  uint8_t buf[256];
  size_t len = radio->getPacketLength();
  if (len > sizeof(buf)) len = sizeof(buf);

  int state = radio->readData(buf, len);
  printf("readData len: %u ", len);

  if (state == RADIOLIB_ERR_NONE) {
    printf("Пакет, %u", len);
    printf(" байт, RSSI %f\n", radio->getRSSI());
    printf("  HEX:   ");
    printHex(buf, len);
   // ESP_LOGI(TAG, "");
   // printf("  ASCII: "));
   // printAscii(buf, len);
   // ESP_LOGI(TAG, "");
   
    uint8_t* ptr = (uint8_t*)&thmCmd.enable;
    bool isCmd = false;
    uint8_t i = 0;
    for (; i < PAYLOAD_LEN; i++) {
      uint8_t counter = 0;
      for (uint8_t j = 0; j < 4; j++) {
        if (ptr[j] != buf[j+1]) continue;
        counter++;
      }
      if (counter == 4) {
        isCmd = true;
        break;
      }
      ptr += 4;
    }

    if (isCmd) {
        static uint32_t lastMsgTime = 0;
        
      HEATING_STATE state = (HEATING_STATE)i;
      if (state == HEATING_STATE::ENABLED ||
          state == HEATING_STATE::ALLREADY_ENABLED) {
        heatingState = state;

        //gpio_set_level(GPIO_NUM_0, 1);
        //gpio_set_level(GPIO_NUM_1, 0);
        xTimerReset(timer, 0);
      }
      if (state == HEATING_STATE::DISABLED ||
          state == HEATING_STATE::ALLREADY_DISABLED) {
        heatingState = state;

       // gpio_set_level(GPIO_NUM_0, 0);
      //  gpio_set_level(GPIO_NUM_1, 1);
        xTimerReset(timer, 0);
      }

      printf("Time beetween message: %lu", (millis() - lastMsgTime) / 1000);
      printf(" | currentCmd: %s \n", commandsStr[i]);
      lastMsgTime = millis();
    }

  } else {
    printf("Ошибка чтения, код %d \n", state);
  }

  // после каждого пакета приёмник нужно запустить заново
  radio->startReceive();
}
