#include <ArduinoJson.h>
#include <esp_http_server.h>
#include <esp_timer.h>

#include "../littlefs.h"
#include "../display.h"
#include "../lib/FileData/EspCompat.h"
#include "../lib/FileData/FileData.h"
#include "HttpRequest.h"
#include "TimeRange.h"
#include "rtc/RealRtc.h"

#include "web.h"

static httpd_handle_t webServer = nullptr;
static WiFiHolder* wifi = nullptr;

bool isWebServer = false;
bool isAP = false;
static httpd_handle_t start_web_server();

void endServer() {
  println("End web server");
  // display.hideModal();
  Modal modal{"", "Web server off!", ""};
  modal.timeShow = 5;
  display.showModal(modal);

  isWebServer = false;
  isAP = false;

  httpd_stop(webServer);
  webServer = nullptr;

  if (wifi != nullptr) delete wifi;
}

static TimerHandle_t timerServerNoActivity = nullptr;
unsigned long lastActivity = 0;

void endServerIfNoActivity(TimerHandle_t t) {
  static uint32_t prev = lastActivity;
  uint32_t now = millis();

  printf("check server if no activity: %ld\n", now - prev);
  prev = now;
  if (isWebServer == false) {
    println("Webserver already off!");
    println("Remove task endServerIfNoActivity!");
    removeTimer(timerServerNoActivity);
  }

  if (now - lastActivity > 0.5 * 60 * 1000) {  // 3 minutes
    endServer();
    removeTimer(timerServerNoActivity);
    println("Task 'endServerIfNoActivity' should be removed!");
  }
}

static esp_err_t onToggleRelay(httpd_req_t* req) {
  httpd_resp_set_type(req, "application/json");

  println("onToggleRelay");
  HttpRequest request(req);

  if (request.hasParam("enable")) {
    static bool value = false;
    bool rV = false;
    request.getParamBool("enable", rV);

    if (value == true) {
      httpd_resp_send(req, R"({"status": true})", HTTPD_RESP_USE_STRLEN);
    }
    if (value == false) {
      httpd_resp_send(req, R"({"status": false})", HTTPD_RESP_USE_STRLEN);
    }
    value = !value;
    return ESP_OK;
  }
  httpd_resp_send(req, R"({"status": "ok"})", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
};

static esp_err_t onApi(httpd_req_t* request) {
  httpd_resp_set_type(request, "application/json");

  lastActivity = millis();
  char json[80] = "";
  sprintf(json, "{\"status\":\"ok\", \"relay\": %s, \"lastSetTime\": %llu }",
          false ? "true" : "false", realTime.getLastSetNtpTime());

  // if(!pump.isWorking()) {
  //   //strncpy(&json[23], "false", 5);
  //   std::copy_n("false", 5, &json[25]);
  // }
  println("try send json: %s", json);
  httpd_resp_send(request, json, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
};

static esp_err_t onGetTime(httpd_req_t* request) {
  httpd_resp_set_type(request, "application/json");

  lastActivity = millis();
  println("getTime");

  timeRanges.read();
  printTimeRanges(timeRanges.ranges, timeRanges.size);

  if (timeRanges.size == 0) {
    println("File is empty!");
    httpd_resp_send(request, "{\"status\": \"File is empty!\"}",
                    HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  const char* message;
  switch (timeRanges.status) {
    case FD_FS_ERR:  // 2 - File system error
      message = "{\"status\": \"File system error!\"}";
      break;

    case FD_FILE_ERR:  // 3 - Error opening file
      message = "{\"status\": \"Error opening file!\"}";
      break;

    case FD_READ:  // 5 - Reading data from file
      if (strlen(timeRanges.json) > 5) {
        message = timeRanges.json;
      } else {
        message = "{\"status\": \"File is empty!\"}";
      }
      break;

    default:
      message = "{\"status\": \"Unknown error while read!\"}";
      break;
  }

  printf("Read status: %d\n", (int)timeRanges.status);
  httpd_resp_send(request, message, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
};

// set current time to rtc
static esp_err_t onSetRtcTime(httpd_req_t* req) {
  httpd_resp_set_type(req, "application/json");
  printf("onSetRtcTime\n");

  HttpRequest request(req);

  if (request.hasParam("time")) {
    int time = 0;
    request.getParamInt("time", time);
    printf("Time value str: %s, uint64: %d\n", request.getParam("time").c_str(),
           time);
    time += 2 * 3600;  // todo

    uint64_t time64 = time;
    realTime.setTimeFromUnix(time64);

    char resultJSON[35] = "";
    sprintf(resultJSON, "{\"status\": %llu}", realTime.getLastSetNtpTime());
    printf("onSetRtcTime, resultJSON: %s\n", resultJSON);

    httpd_resp_send(req, resultJSON, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
  }

  httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

FDstat_t statusSaveTime = FDstat_t::FD_IDLE;

void setTimeBody(uint8_t* data, size_t len) {
  lastActivity = millis();
  printf("setTimeBody\n");

  statusSaveTime = timeRanges.write(data, len);
  printTimeRanges(data, len);
};

// save time ranges
static esp_err_t onSetTimeRequest(httpd_req_t* req) {  // call after onBody
  httpd_resp_set_type(req, "application/json");
  HttpRequest request(req);

  std::string body;

  printf("Try get body\n");
  request.getBody(body);
  printf("Print body: %s\n", body.c_str());
  setTimeBody((uint8_t*)body.data(), body.size());

  if (statusSaveTime == FD_WRITE || statusSaveTime == FD_NO_DIF) {
    httpd_resp_send(req, "{\"status\":\"Success save\"}",
                    HTTPD_RESP_USE_STRLEN);

  } else {
    httpd_resp_send(req, "{\"status\":\"Error save\"}", HTTPD_RESP_USE_STRLEN);
  }

  printf("Write status: %d\n", (int)statusSaveTime);
  return ESP_OK;
};

void endServerSetup() {
  println(" ");
  printf("\n %10s", "Connect to: ");
  println(wifi->getIp());
  println(" ");

  lastActivity = millis();

  Modal modal{};
  sprintf(modal.title, "Setup time!");
  sprintf(modal.str1, isAP ? "Connect to AP: ESP32" : "Go to:");

  if (isAP) strcpy(modal.str2, "Go to: ");
  sprintf(isAP ? &modal.str2[7] : modal.str2, wifi->getIp());
  display.showModal(modal);

  setInterval(1000, timerServerNoActivity, endServerIfNoActivity);
  isWebServer = true;
}

void displayInitWebServer() {
  Modal modal= {"", "Web server loading...", ""};
  display.showModal(modal);
  display.drawNow();
}

void initWebServer() {
  displayInitWebServer();

  wifi = new WiFiHolder();
  wifi->onEvent(ON_WIFI_EVENT::APStart, [](ON_WIFI_EVENT ev) {
    isAP = true;
    webServer = start_web_server();
    endServerSetup();
  });

  wifi->onEvent(ON_WIFI_EVENT::Any, [](ON_WIFI_EVENT ev) {
    switch (ev) {
      case ON_WIFI_EVENT::Connected:
        webServer = start_web_server();
        endServerSetup();
        break;

      case ON_WIFI_EVENT::ErrorConnect:
        println("Failed connect, init WebServer with Aceess Point");
        wifi->createAP();
        break;
      default:
        println("Other WIFI_EVENT: %d", (int)ev);
        break;
    }
  });

  wifi->turnOn();
}

static const httpd_uri_t uri_handlers[] = {

    {.uri = "/api", .method = HTTP_GET, .handler = onApi, .user_ctx = nullptr},

    {.uri = "/getTime",
     .method = HTTP_GET,
     .handler = onGetTime,
     .user_ctx = nullptr},

    {.uri = "/setTime",
     .method = HTTP_GET,
     .handler = onSetRtcTime,
     .user_ctx = nullptr},

    {.uri = "/toggleRelay",
     .method = HTTP_GET,
     .handler = onToggleRelay,
     .user_ctx = nullptr},

    {.uri = "/saveTime",
     .method = HTTP_POST,
     .handler = onSetTimeRequest,
     .user_ctx = nullptr}};

#include <esp_log.h>

static const char* TAG = "WEB";

static const char* get_content_type(const char* path) {
  if (strstr(path, ".html")) return "text/html; charset=utf-8";
  if (strstr(path, ".css")) return "text/css; charset=utf-8";
  if (strstr(path, ".js")) return "application/javascript";
  if (strstr(path, ".json")) return "application/json";
  if (strstr(path, ".svg")) return "image/svg+xml";
  if (strstr(path, ".png")) return "image/png";
  if (strstr(path, ".ico")) return "image/x-icon";

  return "application/octet-stream";
}

static esp_err_t file_handler(httpd_req_t* req) {
  char filepath[256];

  // GET / → /littlefs/index.html
  const char* uri = req->uri;

  if (strcmp(uri, "/") == 0) {
    uri = "/index.html";
  }

  // Защита от выхода за пределы каталога.
  if (strstr(uri, "..") != nullptr || strchr(uri, '\\') != nullptr) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
    return ESP_OK;
  }

  int len = snprintf(filepath, sizeof(filepath), "/littlefs%s", uri);

  if (len < 0 || len >= sizeof(filepath)) {
    httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "Path too long");
    return ESP_OK;
  }

  FILE* file = fopen(filepath, "rb");

  if (file == nullptr) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_OK;
  }

  httpd_resp_set_type(req, get_content_type(filepath));

  char buffer[1024];
  size_t count;

  while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    if (httpd_resp_send_chunk(req, buffer, count) != ESP_OK) {
      fclose(file);
      return ESP_FAIL;
    }
  }

  fclose(file);

  // Завершение chunked-ответа.
  return httpd_resp_send_chunk(req, nullptr, 0);
}

static httpd_handle_t start_web_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Разрешаем wildcard-маршруты.
  config.uri_match_fn = httpd_uri_match_wildcard;

  httpd_handle_t server = nullptr;

  esp_err_t err = httpd_start(&server, &config);

  if (err != ESP_OK) {
    ESP_LOGI(TAG, "HTTP server start failed");
    return nullptr;
  }

  httpd_uri_t files_uri = {.uri = "/*",
                           .method = HTTP_GET,
                           .handler = file_handler,
                           .user_ctx = nullptr};

  for (const auto& uri : uri_handlers) {
    httpd_register_uri_handler(server, &uri);
  }
  // files regiseter must be lust
  err = httpd_register_uri_handler(server, &files_uri);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "Handler registration failed");
    httpd_stop(server);
    return nullptr;
  }

  ESP_LOGI(TAG, "HTTP server started");
  return server;
}