/*
 * nvs.h
 *
 *  Created on: 2025年11月3日
 *      Author: sun.zhidong
 */

#ifndef MAIN_NVS_H_
#define MAIN_NVS_H_nvs_read

esp_err_t nvs_init();
esp_err_t nvs_write(const char *key, const char *value);
esp_err_t nvs_read(const char *key, char *value, size_t *length);


#endif /* MAIN_NVS_H_ */
