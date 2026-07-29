/*
 * SPDX-License-Identifier: MIT
 *
 * Adapted from SparkFun MAX3010x heartRate.cpp and Maxim's PBA algorithm.
 * Copyright (c) 2016 SparkFun Electronics.
 * Copyright (c) 2016 Maxim Integrated Products, Inc.
 */
#include "heart_rate_algorithm.h"

#include <string.h>

static const uint16_t s_fir_coefficients[12] = {
	172, 321, 579, 927, 1360, 1858, 2390, 2916, 3391, 3768, 4012, 4096
};

static int16_t average_dc(int32_t *average, uint16_t sample)
{
	*average += ((((int32_t)sample << 15) - *average) >> 4);
	return (int16_t)(*average >> 15);
}

static int16_t low_pass(heart_rate_algorithm_t *state, int16_t input)
{
	state->buffer[state->offset] = input;
	int32_t sum = (int32_t)s_fir_coefficients[11] *
		state->buffer[(state->offset - 11) & 0x1f];
	for (uint8_t i = 0; i < 11; i++) {
		sum += (int32_t)s_fir_coefficients[i] *
			(state->buffer[(state->offset - i) & 0x1f] +
			 state->buffer[(state->offset - 22 + i) & 0x1f]);
	}
	state->offset = (uint8_t)((state->offset + 1) & 0x1f);
	return (int16_t)(sum >> 15);
}

void heart_rate_algorithm_reset(heart_rate_algorithm_t *state)
{
	if (!state) return;
	memset(state, 0, sizeof(*state));
	state->ac_max = 20;
	state->ac_min = -20;
}

bool heart_rate_algorithm_check(heart_rate_algorithm_t *state, int32_t sample)
{
	if (!state) return false;
	bool beat = false;
	state->signal_previous = state->signal_current;
	int16_t average = average_dc(&state->average_reg, (uint16_t)sample);
	state->signal_current = low_pass(state, (int16_t)(sample - average));

	if (state->signal_previous < 0 && state->signal_current >= 0) {
		state->ac_max = state->signal_max;
		state->ac_min = state->signal_min;
		state->positive_edge = 1;
		state->negative_edge = 0;
		state->signal_max = 0;
		int16_t amplitude = state->ac_max - state->ac_min;
		beat = amplitude > 20 && amplitude < 1000;
	}
	if (state->signal_previous > 0 && state->signal_current <= 0) {
		state->positive_edge = 0;
		state->negative_edge = 1;
		state->signal_min = 0;
	}
	if (state->positive_edge &&
	    state->signal_current > state->signal_previous) {
		state->signal_max = state->signal_current;
	}
	if (state->negative_edge &&
	    state->signal_current < state->signal_previous) {
		state->signal_min = state->signal_current;
	}
	return beat;
}
