#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t max44009_driver_init(void);
esp_err_t max44009_driver_read(int32_t *lux_x10);
bool max44009_driver_present(void);
