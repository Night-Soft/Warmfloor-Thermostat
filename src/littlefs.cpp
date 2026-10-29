#include "littlefs.h"

#include "esp_littlefs.h"
#include "esp_log.h"

fs::FS LittleFS("/littlefs");

static const char* TAG = "LittleFS";
static constexpr char PARTITION[] = "littlefs";
bool islittleFSMouted = false;

esp_err_t littlefsInit() {
  esp_vfs_littlefs_conf_t conf = {};

  conf.base_path = "/littlefs";
  conf.partition_label = PARTITION;

  // Не форматировать автоматически при ошибке.
  conf.format_if_mount_failed =
      false;  // set true if error mount, format only if error
  conf.dont_mount = false;
  // ESP_LOGI(TAG, "LittleFS format");
  // esp_littlefs_format(PARTITION);

  esp_err_t err = esp_vfs_littlefs_register(&conf);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Mount failed: %s", esp_err_to_name(err));
    islittleFSMouted = false;
    return err;
  }

  islittleFSMouted = true;
  ESP_LOGI(TAG, "LittleFS mounted");
  return ESP_OK;
}

esp_err_t littlefsGetInfo(size_t* total, size_t* used) {
  return esp_littlefs_info(PARTITION, total, used);
}

void testFS() {
  File file = LittleFS.open("/test_example.txt", "r");

  if (!file) {
    printf("Failed to open file for reading\n");
    return;
  }

  printf("File Content:\n");
  char* buffer = new char[file.size()]();
  file.read((uint8_t*)buffer, file.size());
  printf("%s\n", buffer);

  file.close();

  delete[] buffer;
}