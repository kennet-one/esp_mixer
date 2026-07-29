#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
	bool valid;
	bool calibrated;
	int32_t value_x10;
	esp_err_t error;
	uint32_t updated_ms;
} mixer_metric_t;

typedef struct {
	uint32_t generation;
	mixer_metric_t co2;
	mixer_metric_t temperature;
	mixer_metric_t humidity;
	mixer_metric_t lux;
	mixer_metric_t heart_rate;
	bool rtc_valid;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t day;
	uint8_t month;
	esp_err_t rtc_error;
	uint32_t rtc_updated_ms;
} mixer_snapshot_t;

void app_snapshot_init(void);
void app_snapshot_get(mixer_snapshot_t *out);
void app_snapshot_update_metric(uint16_t metric_id, bool valid, bool calibrated,
				int32_t value_x10, esp_err_t error,
				uint32_t updated_ms);
void app_snapshot_update_rtc(bool valid, uint8_t hour, uint8_t minute,
			     uint8_t second, uint8_t day, uint8_t month,
			     esp_err_t error, uint32_t updated_ms);
