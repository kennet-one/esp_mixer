#include "sensor_drivers.h"

#include "esp_log.h"

#include "dht22_driver.h"
#include "ds3231_driver.h"
#include "max30105_driver.h"
#include "max44009_driver.h"
#include "mhz19_driver.h"
#include "mixer_i2c_bus.h"
#include "paj7620_driver.h"

static const char *TAG = "sensors";

static void log_init(const char *name, esp_err_t error)
{
	if (error == ESP_OK) {
		ESP_LOGI(TAG, "%s ready", name);
	} else {
		ESP_LOGW(TAG, "%s unavailable: %s",
			 name, esp_err_to_name(error));
	}
}

esp_err_t sensor_drivers_init(void)
{
	esp_err_t err = mixer_i2c_bus_init();
	if (err != ESP_OK) return err;

	esp_err_t lux = max44009_driver_init();
	esp_err_t pulse = max30105_driver_init();
	esp_err_t rtc = ds3231_driver_init();
	esp_err_t gesture = paj7620_driver_init();
	log_init("MAX44009@0x4a", lux);
	log_init("MAX30105@0x57", pulse);
	log_init("DS3231@0x68", rtc);
	log_init("PAJ7620@0x73", gesture);
	if (lux != ESP_OK || pulse != ESP_OK || rtc != ESP_OK ||
	    gesture != ESP_OK) {
		mixer_i2c_scan_log();
	}

	log_init("DHT22", dht22_driver_init());
	log_init("MH-Z19B", mhz19_driver_init());
	return ESP_OK;
}

esp_err_t sensor_read_dht(int32_t *temperature_x10, int32_t *humidity_x10)
{
	return dht22_driver_read(temperature_x10, humidity_x10);
}

esp_err_t sensor_read_co2(int32_t *ppm_x10)
{
	return mhz19_driver_read(ppm_x10);
}

esp_err_t sensor_read_lux(int32_t *lux_x10)
{
	return max44009_driver_read(lux_x10);
}

esp_err_t sensor_read_rtc(uint8_t *hour, uint8_t *minute, uint8_t *second,
			  uint8_t *day, uint8_t *month)
{
	return ds3231_driver_read(hour, minute, second, day, month);
}

esp_err_t sensor_read_gesture(mixer_gesture_t *gesture)
{
	return paj7620_driver_read(gesture);
}

esp_err_t sensor_pulse_enable(bool enable)
{
	return max30105_driver_enable(enable);
}

esp_err_t sensor_read_pulse(int32_t *bpm_x10, uint32_t *ir_value)
{
	return max30105_driver_read(bpm_x10, ir_value);
}
