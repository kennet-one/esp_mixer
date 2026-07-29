#include "mhz19_driver.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#define MHZ_UART UART_NUM_2

esp_err_t mhz19_driver_init(void)
{
	uart_config_t config = {
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	esp_err_t err = uart_driver_install(MHZ_UART, 256, 0, 0, NULL, 0);
	if (err == ESP_ERR_INVALID_STATE) err = ESP_OK;
	if (err == ESP_OK) err = uart_param_config(MHZ_UART, &config);
	if (err == ESP_OK) {
		err = uart_set_pin(MHZ_UART, CONFIG_MIXER_MHZ19_TX_GPIO,
				   CONFIG_MIXER_MHZ19_RX_GPIO,
				   UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	}
	return err;
}

esp_err_t mhz19_driver_read(int32_t *ppm_x10)
{
	if (!ppm_x10) return ESP_ERR_INVALID_ARG;
	static const uint8_t command[9] = {
		0xff, 0x01, 0x86, 0, 0, 0, 0, 0, 0x79
	};
	uart_flush_input(MHZ_UART);
	if (uart_write_bytes(MHZ_UART, command, sizeof(command)) !=
	    sizeof(command)) {
		return ESP_FAIL;
	}
	uint8_t reply[9] = {0};
	int received = uart_read_bytes(MHZ_UART, reply, sizeof(reply),
				       pdMS_TO_TICKS(250));
	if (received != sizeof(reply) || reply[0] != 0xff || reply[1] != 0x86) {
		return ESP_ERR_INVALID_RESPONSE;
	}
	uint8_t checksum = 0;
	for (int i = 1; i < 8; i++) checksum += reply[i];
	checksum = (uint8_t)(0xff - checksum + 1);
	if (checksum != reply[8]) return ESP_ERR_INVALID_CRC;
	int32_t ppm = ((int32_t)reply[2] << 8) | reply[3];
	if (ppm > 10000) return ESP_ERR_INVALID_RESPONSE;
	*ppm_x10 = ppm * 10;
	return ESP_OK;
}
