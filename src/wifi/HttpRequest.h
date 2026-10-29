#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "esp_http_server.h"

class HttpRequest {
 public:
  explicit HttpRequest(httpd_req_t* req);
  ~HttpRequest();

  HttpRequest(const HttpRequest&) = delete;
  HttpRequest& operator=(const HttpRequest&) = delete;

  // Проверка и получение параметров
  bool hasParam(const char* name) const;
  bool getBody(std::string &body);
  bool getParam(const char* name, std::string& value) const;
  std::string getParam(const char* name) const;

  // Получение числовых значений
  bool getParamInt(const char* name, int& value) const;
  bool getParamFloat(const char* name, float& value) const;
  bool getParamBool(const char* name, bool& value) const;

  // HTTP-запрос
  const char* uri() const;
  httpd_method_t method() const;
  httpd_req_t* raw() const;

  // Ошибка чтения тела запроса
  bool bodyOk() const;
  
  std::vector<char> params_;

 private:
  httpd_req_t* req_;
  bool body_ok_;

  bool parseParams();
};