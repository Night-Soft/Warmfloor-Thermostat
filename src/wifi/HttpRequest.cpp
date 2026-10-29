#include "HttpRequest.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "esp_log.h"

static constexpr size_t MAX_PARAMS_SIZE = 4096;

HttpRequest::HttpRequest(httpd_req_t* req) : req_(req), body_ok_(true) {
  body_ok_ = parseParams();
}

HttpRequest::~HttpRequest() = default;

bool HttpRequest::parseParams() {
  if (req_ == nullptr) {
    return false;
  }

  // GET: параметры из URL
  if (req_->method == HTTP_GET || req_->method == HTTP_HEAD ||
      req_->method == HTTP_DELETE) {
    size_t len = httpd_req_get_url_query_len(req_);

    if (len == 0) {
      return true;
    }

    if (len > MAX_PARAMS_SIZE) {
      return false;
    }

    params_.resize(len + 1);

    if (httpd_req_get_url_query_str(req_, params_.data(), params_.size()) !=
        ESP_OK) {
      params_.clear();
      return false;
    }

    return true;
  }

  // POST / PUT / PATCH:
  // application/x-www-form-urlencoded
  if (req_->method == HTTP_POST || req_->method == HTTP_PUT ||
      req_->method == HTTP_PATCH) {
    if (req_->content_len == 0) {
      return true;
    }

    if (req_->content_len > MAX_PARAMS_SIZE) {
      return false;
    }

    char content_type[64] = {};

    if (httpd_req_get_hdr_value_str(req_, "Content-Type", content_type,
                                    sizeof(content_type)) != ESP_OK) {
      return false;
    }

    if (strncmp(content_type, "application/x-www-form-urlencoded", 33) != 0) {
      // JSON и другие форматы здесь не разбираются
      return false;
    }

    params_.resize(req_->content_len + 1);

    size_t received = 0;

    while (received < req_->content_len) {
      int ret = httpd_req_recv(req_, params_.data() + received,
                               req_->content_len - received);

      if (ret <= 0) {
        params_.clear();
        return false;
      }

      received += static_cast<size_t>(ret);
    }

    params_[received] = '\0';
    return true;
  }

  // Другие методы пока не поддерживаются
  return true;
}

bool HttpRequest::hasParam(const char* name) const {
  if (params_.empty() || name == nullptr || *name == '\0') {
    return false;
  }

  char value[256];

  esp_err_t err =
      httpd_query_key_value(params_.data(), name, value, sizeof(value));

  return err == ESP_OK || err == ESP_ERR_HTTPD_RESULT_TRUNC;
}

bool HttpRequest::getBody(std::string& body) {
  body.clear();

  if (req_ == nullptr) {
    return false;
  }

  constexpr size_t MAX_BODY_SIZE = 4096;

  size_t body_size = req_->content_len;

  if (body_size > MAX_BODY_SIZE) {
    ESP_LOGE("HttpRequest", "Body too large: %u bytes",
             static_cast<unsigned>(body_size));
    return false;
  }

  // GET без тела — допустимый пустой результат
  if (body_size == 0) {
    return true;
  }

  body.resize(body_size);

  size_t received = 0;

  while (received < body_size) {
    int ret = httpd_req_recv(req_, &body[received], body_size - received);

    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      continue;
    }

    if (ret <= 0) {
      body.clear();

      ESP_LOGE("HttpRequest", "Failed to receive body");

      return false;
    }

    received += static_cast<size_t>(ret);
  }

  return true;
}

bool HttpRequest::getParam(const char* name, std::string& value) const {
  value.clear();

  if (params_.empty() || name == nullptr || *name == '\0') {
    return false;
  }

  // Буфер достаточного размера для типичных параметров.
  // Если значение длиннее, функция сообщит об усечении.
  constexpr size_t MAX_VALUE_SIZE = 4096;
  std::vector<char> buffer(MAX_VALUE_SIZE);

  esp_err_t err =
      httpd_query_key_value(params_.data(), name, buffer.data(), buffer.size());

  if (err != ESP_OK) {
    return false;
  }

  value.assign(buffer.data());
  return true;
}

std::string HttpRequest::getParam(const char* name) const {
  std::string value;
  getParam(name, value);
  return value;
}

bool HttpRequest::getParamInt(const char* name, int& value) const {
  std::string str;

  if (!getParam(name, str) || str.empty()) {
    return false;
  }

  char* end = nullptr;
  errno = 0;

  long result = strtol(str.c_str(), &end, 10);

  if (errno != 0 || end == str.c_str() || *end != '\0' || result < INT_MIN ||
      result > INT_MAX) {
    return false;
  }

  value = static_cast<int>(result);
  return true;
}

bool HttpRequest::getParamFloat(const char* name, float& value) const {
  std::string str;

  if (!getParam(name, str) || str.empty()) {
    return false;
  }

  char* end = nullptr;
  errno = 0;

  float result = strtof(str.c_str(), &end);

  if (errno != 0 || end == str.c_str() || *end != '\0' || !isfinite(result)) {
    return false;
  }

  value = result;
  return true;
}

bool HttpRequest::getParamBool(const char* name, bool& value) const {
  std::string str;

  if (!getParam(name, str)) {
    return false;
  }

  if (str == "1" || str == "true" || str == "on" || str == "yes") {
    value = true;
    return true;
  }

  if (str == "0" || str == "false" || str == "off" || str == "no") {
    value = false;
    return true;
  }

  return false;
}

const char* HttpRequest::uri() const { return req_ ? req_->uri : ""; }

httpd_method_t HttpRequest::method() const {
  return req_ ? (httpd_method_t)req_->method : HTTP_GET;
}

httpd_req_t* HttpRequest::raw() const { return req_; }

bool HttpRequest::bodyOk() const { return body_ok_; }