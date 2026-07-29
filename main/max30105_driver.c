#include "max30105_driver.h"

#include <string.h>

#include "driver/i2c_master.h"
#include "esp_timer.h"

#include "heart_rate_algorithm.h"
#include "mixer_i2c_bus.h"

#define MAX30105_ADDR 0x57
#define MAX30105_FIFO_DEPTH 32
#define MAX30105_BYTES_PER_SAMPLE 6

static i2c_master_dev_handle_t s_device;
static bool s_enabled;
static uint32_t s_last_beat_ms;
static int32_t s_bpm_x10;
static heart_rate_algorithm_t s_algorithm;

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
	return mixer_i2c_write_reg(s_device, reg, value);
}

esp_err_t max30105_driver_init(void)
{
	if (!mixer_i2c_probe(MAX30105_ADDR)) return ESP_ERR_NOT_FOUND;
	esp_err_t err = mixer_i2c_attach(MAX30105_ADDR, &s_device);
	if (err != ESP_OK) return err;
	return max30105_driver_enable(false);
}

bool max30105_driver_present(void)
{
	return s_device != NULL;
}

esp_err_t max30105_driver_enable(bool enable)
{
	if (!s_device) return ESP_ERR_INVALID_STATE;
	if (!enable) {
		esp_err_t err = write_reg(0x0c, 0x00);
		if (err == ESP_OK) err = write_reg(0x0d, 0x00);
		s_enabled = false;
		return err;
	}

	/* FIFO average=8, rollover enabled, almost-full threshold=15. */
	esp_err_t err = write_reg(0x08, 0x7f);
	if (err == ESP_OK) err = write_reg(0x09, 0x03);
	/* ADC=16384, 200 samples/s, 411 us pulse width. */
	if (err == ESP_OK) err = write_reg(0x0a, 0x6f);
	if (err == ESP_OK) err = write_reg(0x0c, 0xdc);
	if (err == ESP_OK) err = write_reg(0x0d, 0xdc);
	if (err == ESP_OK) err = write_reg(0x04, 0x00);
	if (err == ESP_OK) err = write_reg(0x05, 0x00);
	if (err == ESP_OK) err = write_reg(0x06, 0x00);
	if (err != ESP_OK) return err;

	s_enabled = true;
	s_last_beat_ms = 0;
	s_bpm_x10 = 0;
	heart_rate_algorithm_reset(&s_algorithm);
	return ESP_OK;
}

static void process_sample(uint32_t ir)
{
	uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
	if (!heart_rate_algorithm_check(&s_algorithm, (int32_t)ir)) return;
	if (s_last_beat_ms != 0) {
		uint32_t delta = now - s_last_beat_ms;
		if (delta >= 250 && delta <= 3000) {
			int32_t candidate = (int32_t)(600000U / delta);
			if (candidate >= 200 && candidate <= 2400) {
				s_bpm_x10 = s_bpm_x10 == 0 ? candidate :
					(s_bpm_x10 * 3 + candidate) / 4;
			}
		}
	}
	s_last_beat_ms = now;
}

esp_err_t max30105_driver_read(int32_t *bpm_x10, uint32_t *ir_value)
{
	if (!bpm_x10 || !ir_value || !s_enabled || !s_device) {
		return ESP_ERR_INVALID_STATE;
	}
	uint8_t pointers[3] = {0};
	esp_err_t err = mixer_i2c_read_reg(s_device, 0x04, pointers,
					   sizeof(pointers));
	if (err != ESP_OK) return err;
	uint8_t write_pointer = pointers[0] & 0x1f;
	uint8_t read_pointer = pointers[2] & 0x1f;
	uint8_t samples = (uint8_t)((write_pointer - read_pointer) & 0x1f);
	if (samples == 0) return ESP_ERR_NOT_FOUND;

	uint8_t fifo[MAX30105_FIFO_DEPTH * MAX30105_BYTES_PER_SAMPLE];
	size_t bytes = (size_t)samples * MAX30105_BYTES_PER_SAMPLE;
	err = mixer_i2c_read_reg(s_device, 0x07, fifo, bytes);
	if (err != ESP_OK) return err;
	uint32_t latest_ir = 0;
	for (uint8_t i = 0; i < samples; i++) {
		const uint8_t *sample = &fifo[i * MAX30105_BYTES_PER_SAMPLE];
		latest_ir = (((uint32_t)sample[3] << 16) |
			     ((uint32_t)sample[4] << 8) | sample[5]) & 0x3ffff;
		process_sample(latest_ir);
	}
	*ir_value = latest_ir;
	*bpm_x10 = s_bpm_x10;
	return ESP_OK;
}
