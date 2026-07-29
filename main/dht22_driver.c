#include "dht22_driver.h"

#include "dht.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

esp_err_t dht22_driver_init(void)
{
	const gpio_num_t pin = (gpio_num_t)CONFIG_MIXER_DHT_GPIO;
	if (!GPIO_IS_VALID_GPIO(pin)) return ESP_ERR_INVALID_ARG;
	return gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
}

esp_err_t dht22_driver_read(int32_t *temperature_x10, int32_t *humidity_x10)
{
	if (!temperature_x10 || !humidity_x10) return ESP_ERR_INVALID_ARG;
	int16_t humidity = 0;
	int16_t temperature = 0;
	esp_err_t err = dht_read_data(DHT_TYPE_AM2301,
				      (gpio_num_t)CONFIG_MIXER_DHT_GPIO,
				      &humidity, &temperature);
	if (err != ESP_OK) return err;
	*humidity_x10 = humidity;
	*temperature_x10 = temperature;
	if (*humidity_x10 > 1000 || *temperature_x10 < -400 ||
	    *temperature_x10 > 800) {
		return ESP_ERR_INVALID_RESPONSE;
	}
	return ESP_OK;
}
