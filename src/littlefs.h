#pragma once

#include "esp_err.h"
#include <cstddef>

esp_err_t littlefsInit();
esp_err_t littlefsGetInfo(size_t* total, size_t* used);
void testFS();

#include "lib/FileData/EspCompat.h"
extern fs::FS LittleFS;