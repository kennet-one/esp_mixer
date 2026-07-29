#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t dht22_driver_init(void);
esp_err_t dht22_driver_read(int32_t *temperature_x10, int32_t *humidity_x10);
