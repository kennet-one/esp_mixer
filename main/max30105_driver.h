#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t max30105_driver_init(void);
esp_err_t max30105_driver_enable(bool enable);
esp_err_t max30105_driver_read(int32_t *bpm_x10, uint32_t *ir_value);
bool max30105_driver_present(void);
