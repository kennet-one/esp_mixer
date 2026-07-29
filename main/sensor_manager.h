#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t sensor_manager_start(void);
bool sensor_manager_ready(void);
