#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t ds3231_driver_init(void);
esp_err_t ds3231_driver_read(uint8_t *hour, uint8_t *minute, uint8_t *second,
			     uint8_t *day, uint8_t *month);
bool ds3231_driver_present(void);
