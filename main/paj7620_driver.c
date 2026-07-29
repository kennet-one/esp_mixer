/*
 * SPDX-License-Identifier: MIT
 *
 * Register setup adapted from RevEng_PAJ7620 by Aaron Crandall and the
 * Seeed Technology PAJ7620 library.
 * Copyright (c) 2015 Seeed Technology Co., Ltd.
 */
#include "paj7620_driver.h"

#include "driver/i2c_master.h"
#include "esp_rom_sys.h"

#include "mixer_i2c_bus.h"

#define PAJ7620_ADDR 0x73

static i2c_master_dev_handle_t s_device;

static const uint16_t s_init_registers[] = {
	0xef00, 0x4100, 0x4200, 0x3707, 0x3817, 0x3906, 0x4201,
	0x462d, 0x470f, 0x483c, 0x4900, 0x4a1e, 0x4c22, 0x5110,
	0x5e10, 0x6027, 0x8042, 0x8144, 0x8204, 0x8b01, 0x9006,
	0x950a, 0x960c, 0x9705, 0x9a14, 0x9c3f, 0xa519, 0xcc19,
	0xcd0b, 0xce13, 0xcf64, 0xd021, 0xef01, 0x020f, 0x0310,
	0x0402, 0x2501, 0x2739, 0x287f, 0x2908, 0x3eff, 0x5e3d,
	0x6596, 0x6797, 0x69cd, 0x6a01, 0x6d2c, 0x6e01, 0x7201,
	0x7335, 0x7400, 0x7701, 0xef00, 0x41ff, 0x4201,
};

static const uint16_t s_gesture_registers[] = {
	0xef00, 0x4100, 0x4200, 0x483c, 0x4900, 0x5110, 0x8320,
	0x9ff9, 0xef01, 0x011e, 0x020f, 0x0310, 0x0402, 0x4140,
	0x4330, 0x6596, 0x6600, 0x6797, 0x6801, 0x69cd, 0x6a01,
	0x6bb0, 0x6c04, 0x6d2c, 0x6e01, 0x7400, 0xef00, 0x41ff,
	0x4201,
};

static esp_err_t write_table(const uint16_t *table, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		esp_err_t err = mixer_i2c_write_reg(
			s_device, (uint8_t)(table[i] >> 8),
			(uint8_t)(table[i] & 0xff));
		if (err != ESP_OK) return err;
	}
	return ESP_OK;
}

esp_err_t paj7620_driver_init(void)
{
	esp_rom_delay_us(700);
	if (!mixer_i2c_probe(PAJ7620_ADDR)) return ESP_ERR_NOT_FOUND;
	esp_err_t err = mixer_i2c_attach(PAJ7620_ADDR, &s_device);
	if (err != ESP_OK) return err;
	/* The first bank-select may only wake the device. Send it twice. */
	if ((err = mixer_i2c_write_reg(s_device, 0xef, 0x00)) != ESP_OK) return err;
	if ((err = mixer_i2c_write_reg(s_device, 0xef, 0x00)) != ESP_OK) return err;
	uint8_t id[2] = {0};
	if ((err = mixer_i2c_read_reg(s_device, 0x00, id, sizeof(id))) != ESP_OK) {
		return err;
	}
	if (id[0] != 0x20 || id[1] != 0x76) return ESP_ERR_INVALID_RESPONSE;
	if ((err = write_table(s_init_registers,
			      sizeof(s_init_registers) /
				      sizeof(s_init_registers[0]))) != ESP_OK) {
		return err;
	}
	return write_table(s_gesture_registers,
			   sizeof(s_gesture_registers) /
				   sizeof(s_gesture_registers[0]));
}

bool paj7620_driver_present(void)
{
	return s_device != NULL;
}

esp_err_t paj7620_driver_read(mixer_gesture_t *gesture)
{
	if (!gesture) return ESP_ERR_INVALID_ARG;
	if (!s_device) return ESP_ERR_INVALID_STATE;
	*gesture = MIXER_GESTURE_NONE;
	uint8_t flags[2] = {0};
	esp_err_t err = mixer_i2c_read_reg(s_device, 0x43, flags,
					   sizeof(flags));
	if (err != ESP_OK) return err;
	if (flags[0] & 0x01) *gesture = MIXER_GESTURE_RIGHT;
	else if (flags[0] & 0x02) *gesture = MIXER_GESTURE_LEFT;
	else if (flags[0] & 0x04) *gesture = MIXER_GESTURE_UP;
	else if (flags[0] & 0x08) *gesture = MIXER_GESTURE_DOWN;
	else if (flags[0] & 0x10) *gesture = MIXER_GESTURE_FORWARD;
	else if (flags[0] & 0x20) *gesture = MIXER_GESTURE_BACKWARD;
	else if (flags[0] & 0x40) *gesture = MIXER_GESTURE_CLOCKWISE;
	else if (flags[0] & 0x80) *gesture = MIXER_GESTURE_ANTICLOCKWISE;
	else if (flags[1] & 0x01) *gesture = MIXER_GESTURE_WAVE;
	return ESP_OK;
}
