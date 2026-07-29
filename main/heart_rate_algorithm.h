/*
 * SPDX-License-Identifier: MIT
 *
 * Adapted from SparkFun MAX3010x heartRate.cpp and Maxim's PBA algorithm.
 * Copyright (c) 2016 SparkFun Electronics.
 * Copyright (c) 2016 Maxim Integrated Products, Inc.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	int16_t ac_max;
	int16_t ac_min;
	int16_t signal_current;
	int16_t signal_previous;
	int16_t signal_min;
	int16_t signal_max;
	int16_t positive_edge;
	int16_t negative_edge;
	int32_t average_reg;
	int16_t buffer[32];
	uint8_t offset;
} heart_rate_algorithm_t;

void heart_rate_algorithm_reset(heart_rate_algorithm_t *state);
bool heart_rate_algorithm_check(heart_rate_algorithm_t *state, int32_t sample);
