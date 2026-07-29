#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t mixer_i2c_bus_init(void);
bool mixer_i2c_probe(uint8_t address);
esp_err_t mixer_i2c_attach(uint8_t address, i2c_master_dev_handle_t *out);
esp_err_t mixer_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg,
			      uint8_t value);
esp_err_t mixer_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg,
			     void *data, size_t length);
void mixer_i2c_scan_log(void);
