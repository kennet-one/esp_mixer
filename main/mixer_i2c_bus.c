#include "mixer_i2c_bus.h"

#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#define I2C_PORT I2C_NUM_0

static const char *TAG = "mixer_i2c";
static i2c_master_bus_handle_t s_bus;
static SemaphoreHandle_t s_lock;

esp_err_t mixer_i2c_bus_init(void)
{
	if (s_bus && s_lock) return ESP_OK;
	i2c_master_bus_config_t config = {
		.i2c_port = I2C_PORT,
		.sda_io_num = CONFIG_MIXER_I2C_SDA_GPIO,
		.scl_io_num = CONFIG_MIXER_I2C_SCL_GPIO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};
	esp_err_t err = i2c_new_master_bus(&config, &s_bus);
	if (err != ESP_OK) return err;
	s_lock = xSemaphoreCreateMutex();
	if (s_lock) return ESP_OK;
	(void)i2c_del_master_bus(s_bus);
	s_bus = NULL;
	return ESP_ERR_NO_MEM;
}

bool mixer_i2c_probe(uint8_t address)
{
	if (!s_bus || !s_lock) return false;
	if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) return false;
	esp_err_t err = i2c_master_probe(s_bus, address, 20);
	xSemaphoreGive(s_lock);
	return err == ESP_OK;
}

esp_err_t mixer_i2c_attach(uint8_t address, i2c_master_dev_handle_t *out)
{
	if (!s_bus || !out) return ESP_ERR_INVALID_ARG;
	i2c_device_config_t config = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = address,
		.scl_speed_hz = 400000,
	};
	return i2c_master_bus_add_device(s_bus, &config, out);
}

esp_err_t mixer_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg,
			      uint8_t value)
{
	if (!dev || !s_lock) return ESP_ERR_INVALID_STATE;
	if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) != pdTRUE) {
		return ESP_ERR_TIMEOUT;
	}
	uint8_t data[2] = {reg, value};
	esp_err_t err = i2c_master_transmit(dev, data, sizeof(data), 100);
	xSemaphoreGive(s_lock);
	return err;
}

esp_err_t mixer_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg,
			     void *data, size_t length)
{
	if (!dev || !data || length == 0 || !s_lock) return ESP_ERR_INVALID_ARG;
	if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) != pdTRUE) {
		return ESP_ERR_TIMEOUT;
	}
	esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, data,
						    length, 100);
	xSemaphoreGive(s_lock);
	return err;
}

void mixer_i2c_scan_log(void)
{
	char found[192] = {0};
	size_t used = 0;
	unsigned count = 0;
	for (uint8_t address = 0x08; address <= 0x77; address++) {
		if (!mixer_i2c_probe(address)) continue;
		int written = snprintf(found + used, sizeof(found) - used,
				       "%s0x%02x", count == 0 ? "" : ",", address);
		if (written > 0 && (size_t)written < sizeof(found) - used) {
			used += (size_t)written;
		}
		count++;
	}
	ESP_LOGI(TAG, "scan: %u device(s): %s", count, count ? found : "none");
}
