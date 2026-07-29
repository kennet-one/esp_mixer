#include "max44009_driver.h"

#include <math.h>

#include "driver/i2c_master.h"

#include "mixer_i2c_bus.h"

#define MAX44009_ADDR 0x4a

static i2c_master_dev_handle_t s_device;

esp_err_t max44009_driver_init(void)
{
	if (!mixer_i2c_probe(MAX44009_ADDR)) return ESP_ERR_NOT_FOUND;
	esp_err_t err = mixer_i2c_attach(MAX44009_ADDR, &s_device);
	if (err != ESP_OK) return err;
	/* Automatic range and continuous conversion. */
	return mixer_i2c_write_reg(s_device, 0x02, 0x00);
}

bool max44009_driver_present(void)
{
	return s_device != NULL;
}

esp_err_t max44009_driver_read(int32_t *lux_x10)
{
	if (!lux_x10 || !s_device) return ESP_ERR_INVALID_STATE;
	uint8_t raw[2] = {0};
	esp_err_t err = mixer_i2c_read_reg(s_device, 0x03, raw, sizeof(raw));
	if (err != ESP_OK) return err;
	uint8_t exponent = raw[0] >> 4;
	uint16_t mantissa = ((uint16_t)(raw[0] & 0x0f) << 4) | raw[1];
	float lux = ldexpf((float)mantissa, exponent) * 0.045f;
	*lux_x10 = (int32_t)lroundf(lux * 10.0f);
	return ESP_OK;
}
