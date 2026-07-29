#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t mhz19_driver_init(void);
esp_err_t mhz19_driver_read(int32_t *ppm_x10);
