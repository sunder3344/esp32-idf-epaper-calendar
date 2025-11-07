#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define NVS_NAMESPACE "storage"

// NVS 初始化
esp_err_t nvs_init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());  // 如果出现错误，擦除并重新初始化
        err = nvs_flash_init();
    }
    return err;
}

// 写入字符串到 NVS
esp_err_t nvs_write(const char *key, const char *value) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS handle!");
        return err;
    }

    // 写入字符串
    err = nvs_set_str(my_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to write string to NVS");
    }

    // 提交更改
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to commit changes");
    }

    // 关闭 NVS
    nvs_close(my_handle);
    return err;
}

// 从 NVS 读取字符串
esp_err_t nvs_read(const char *key, char *value, size_t *length) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS handle!");
        return err;
    }

    // 读取字符串长度
    size_t required_size;
    err = nvs_get_str(my_handle, key, NULL, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "The key doesn't exist");
    } else if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to get string length from NVS");
        nvs_close(my_handle);
        return err;
    }

    // 读取字符串
    err = nvs_get_str(my_handle, key, value, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to read string from NVS");
    }

    // 关闭 NVS
    nvs_close(my_handle);
    return err;
}
