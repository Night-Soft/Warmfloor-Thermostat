#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#include <FS.h>
#else

#define millis() ((uint32_t)(esp_timer_get_time() / 1000))

#pragma once

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <esp_timer.h>

namespace fs {

class File {
private:
    FILE* _file = nullptr;

public:
    File() = default;

    explicit File(FILE* file) : _file(file) {}

    // Запрет копирования, чтобы не закрыть файл дважды
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    // Передача владения файловым дескриптором
    File(File&& other) noexcept : _file(other._file) {
        other._file = nullptr;
    }

    File& operator=(File&& other) noexcept {
        if (this != &other) {
            close();
            _file = other._file;
            other._file = nullptr;
        }
        return *this;
    }

    ~File() {
        close();
    }

    explicit operator bool() const {
        return _file != nullptr;
    }

    size_t size() {
        if (!_file) return 0;

        struct stat info;
        if (fstat(fileno(_file), &info) != 0) return 0;

        return static_cast<size_t>(info.st_size);
    }

    int read() {
        if (!_file) return -1;

        int value = fgetc(_file);
        return value == EOF ? -1 : value;
    }

    size_t read(uint8_t* buffer, size_t length) {
        if (!_file || !buffer) return 0;
        return fread(buffer, 1, length, _file);
    }

    size_t write(uint8_t value) {
        if (!_file) return 0;
        return fwrite(&value, 1, 1, _file);
    }

    size_t write(const uint8_t* buffer, size_t length) {
        if (!_file || !buffer) return 0;
        return fwrite(buffer, 1, length, _file);
    }

    bool seek(uint32_t position) {
        return _file && fseek(_file, position, SEEK_SET) == 0;
    }

    void close() {
        if (_file) {
            fclose(_file);
            _file = nullptr;
        }
    }
};

class FS {
private:
    const char* _mountPoint;

public:
    explicit FS(const char* mountPoint = "/littlefs")
        : _mountPoint(mountPoint) {}

    bool exists(const char* path) {
        struct stat info;
        char fullPath[256];

        if (!makePath(path, fullPath, sizeof(fullPath))) {
            return false;
        }

        return stat(fullPath, &info) == 0;
    }

    File open(const char* path, const char* mode = "r") {
        char fullPath[256];

        if (!makePath(path, fullPath, sizeof(fullPath))) {
            return File();
        }

        return File(fopen(fullPath, mode));
    }

   private:
    bool makePath(const char* path, char* result, size_t resultSize) const {
      if (!path || !result || resultSize == 0) {
        return false;
      }

      size_t mountLen = strlen(_mountPoint);

      // Если путь уже содержит точку монтирования,
      // не добавляем её повторно.
      if (strncmp(path, _mountPoint, mountLen) == 0 &&
          (path[mountLen] == '/' || path[mountLen] == '\0')) {
        int len = snprintf(result, resultSize, "%s", path);

        return len >= 0 && static_cast<size_t>(len) < resultSize;
      }

      // Убираем начальные слеши.
      while (*path == '/') {
        path++;
      }

      int len = snprintf(result, resultSize, "%s/%s", _mountPoint, path);

      return len >= 0 && static_cast<size_t>(len) < resultSize;
    }
};

} // namespace fs

using File = fs::File;
#endif
