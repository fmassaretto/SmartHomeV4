#pragma once
#ifndef WIFICONNECTION_H_
#define WIFICONNECTION_H_

#include "esp_err.h"
#include "esp_log.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"

esp_err_t wifi_init(void);

esp_err_t wifi_connect(char* wifi_ssid, char* wifi_password);

esp_err_t wifi_disconnect(void);

esp_err_t wifi_deinit(void);

#endif