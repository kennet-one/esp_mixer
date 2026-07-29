#pragma once

#include <stdbool.h>

#include "esp_err.h"

#include "sensor_drivers.h"

esp_err_t paj7620_driver_init(void);
esp_err_t paj7620_driver_read(mixer_gesture_t *gesture);
bool paj7620_driver_present(void);
