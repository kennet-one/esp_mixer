#include "ds3231_driver.h"

#include "driver/i2c_master.h"

#include "mixer_i2c_bus.h"

#define DS3231_ADDR 0x68

static i2c_master_dev_handle_t s_device;

static uint8_t bcd(uint8_t value)
{
	return (uint8_t)((value >> 4) * 10 + (value & 0x0f));
}

esp_err_t ds3231_driver_init(void)
{
	if (!mixer_i2c_probe(DS3231_ADDR)) return ESP_ERR_NOT_FOUND;
	return mixer_i2c_attach(DS3231_ADDR, &s_device);
}

bool ds3231_driver_present(void)
{
	return s_device != NULL;
}

esp_err_t ds3231_driver_read(uint8_t *hour, uint8_t *minute, uint8_t *second,
			     uint8_t *day, uint8_t *month)
{
	if (!hour || !minute || !second || !day || !month) {
		return ESP_ERR_INVALID_ARG;
	}
	if (!s_device) return ESP_ERR_INVALID_STATE;
	uint8_t raw[7] = {0};
	esp_err_t err = mixer_i2c_read_reg(s_device, 0x00, raw, sizeof(raw));
	if (err != ESP_OK) return err;
	*second = bcd(raw[0] & 0x7f);
	*minute = bcd(raw[1] & 0x7f);
	*hour = bcd(raw[2] & 0x3f);
	*day = bcd(raw[4] & 0x3f);
	*month = bcd(raw[5] & 0x1f);
	if (*second > 59 || *minute > 59 || *hour > 23 ||
	    *day == 0 || *day > 31 || *month == 0 || *month > 12) {
		return ESP_ERR_INVALID_RESPONSE;
	}
	return ESP_OK;
}
