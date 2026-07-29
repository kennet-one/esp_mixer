#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
	MIXER_GESTURE_NONE = 0,
	MIXER_GESTURE_FORWARD,
	MIXER_GESTURE_BACKWARD,
	MIXER_GESTURE_LEFT,
	MIXER_GESTURE_RIGHT,
	MIXER_GESTURE_UP,
	MIXER_GESTURE_DOWN,
	MIXER_GESTURE_CLOCKWISE,
	MIXER_GESTURE_ANTICLOCKWISE,
	MIXER_GESTURE_WAVE,
} mixer_gesture_t;

esp_err_t sensor_drivers_init(void);
esp_err_t sensor_read_dht(int32_t *temperature_x10, int32_t *humidity_x10);
esp_err_t sensor_read_co2(int32_t *ppm_x10);
esp_err_t sensor_read_lux(int32_t *lux_x10);
esp_err_t sensor_read_rtc(uint8_t *hour, uint8_t *minute, uint8_t *second,
			  uint8_t *day, uint8_t *month);
esp_err_t sensor_read_gesture(mixer_gesture_t *gesture);
esp_err_t sensor_pulse_enable(bool enable);
esp_err_t sensor_read_pulse(int32_t *bpm_x10, uint32_t *ir_value);
