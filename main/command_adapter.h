#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

bool command_adapter_execute(const char *command, uint8_t *status,
			     char *result, size_t result_size);
esp_err_t command_adapter_publish(uint16_t flags, uint32_t request_id,
				  uint16_t metric_filter);
esp_err_t command_adapter_emit(const char *command);
esp_err_t command_adapter_start(void);
