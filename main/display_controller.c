#include "display_controller.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "u8g2.h"

#include "app_snapshot.h"
#include "sensor_drivers.h"

static const char *TAG = "display";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static mixer_screen_t s_screen;
static bool s_english;
static bool s_mesh_connected;
static bool s_reliable;
static uint32_t s_screen_entered_ms;
static uint32_t s_mesh_notice_until_ms;
static char s_mesh_notice[32] = "Mesh";
static char s_garland[16] = "--";
static char s_red_power[16] = "--";
static char s_red_mode[16] = "--";
static char s_red_brightness[16] = "--";
static bool s_host_weather_active;
static bool s_host_weather_valid;
static uint8_t s_host_rain_chance;
static uint32_t s_host_weather_updated_ms;
static spi_device_handle_t s_spi;
static u8g2_t s_u8g2;
static TaskHandle_t s_display_task;
static bool s_spi_bus_owned;
static volatile uint32_t s_spi_error_count;
static volatile esp_err_t s_spi_last_error;

static uint32_t now_ms(void)
{
	return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static uint8_t gpio_delay(u8x8_t *u8x8, uint8_t msg,
			  uint8_t arg_int, void *arg_ptr)
{
	(void)u8x8;
	(void)arg_ptr;
	if (msg == U8X8_MSG_GPIO_AND_DELAY_INIT) {
		gpio_set_direction(CONFIG_MIXER_DISPLAY_DC_GPIO, GPIO_MODE_OUTPUT);
		gpio_set_level(CONFIG_MIXER_DISPLAY_DC_GPIO, 0);
	} else if (msg == U8X8_MSG_DELAY_MILLI) {
		vTaskDelay(pdMS_TO_TICKS(arg_int));
	} else if (msg == U8X8_MSG_DELAY_10MICRO) {
		esp_rom_delay_us((uint32_t)arg_int * 10U);
	} else if (msg == U8X8_MSG_GPIO_DC) {
		gpio_set_level(CONFIG_MIXER_DISPLAY_DC_GPIO, arg_int);
	}
	return 1;
}

static uint8_t byte_spi(u8x8_t *u8x8, uint8_t msg,
			uint8_t arg_int, void *arg_ptr)
{
	(void)u8x8;
	switch (msg) {
	case U8X8_MSG_BYTE_INIT:
		return gpio_set_level(CONFIG_MIXER_DISPLAY_DC_GPIO, 0) == ESP_OK;
	case U8X8_MSG_BYTE_SET_DC:
		return gpio_set_level(CONFIG_MIXER_DISPLAY_DC_GPIO, arg_int) == ESP_OK;
	case U8X8_MSG_BYTE_START_TRANSFER:
	case U8X8_MSG_BYTE_END_TRANSFER:
		return 1;
	case U8X8_MSG_BYTE_SEND:
		if (arg_int == 0) return 1;
		if (!s_spi || !arg_ptr) return 0;
		spi_transaction_t transaction = {
			.length = (size_t)arg_int * 8,
			.tx_buffer = arg_ptr,
		};
		esp_err_t err = spi_device_polling_transmit(s_spi, &transaction);
		if (err != ESP_OK) {
			s_spi_last_error = err;
			s_spi_error_count++;
			return 0;
		}
		return 1;
	default:
		return 0;
	}
}

static void environment_shift(int *x, int *y)
{
	static const int8_t offsets[4][2] = {
		{-1, -1}, {1, -1}, {1, 1}, {-1, 1},
	};
	if (CONFIG_MIXER_DISPLAY_PIXEL_SHIFT_SECONDS <= 0) {
		*x = 0;
		*y = 0;
		return;
	}
	uint32_t phase = (now_ms() /
		((uint32_t)CONFIG_MIXER_DISPLAY_PIXEL_SHIFT_SECONDS * 1000U)) % 4U;
	*x = offsets[phase][0];
	*y = offsets[phase][1];
}

static void format_metric(char *buffer, size_t size,
			  const mixer_metric_t *metric, bool decimal)
{
	if (!metric->valid) {
		snprintf(buffer, size, "--");
		return;
	}
	double value = metric->value_x10 / 10.0;
	snprintf(buffer, size, decimal ? "%.1f" : "%.0f", value);
}

static int centered_cell_x(int cell_x, int width)
{
	return cell_x + (64 - width) / 2;
}

static void draw_environment_value(int cell_x, int dx, int value_y,
				   const char *value, const char *suffix)
{
	const uint8_t *number_font = u8g2_font_logisoso22_tn;
	const uint8_t *suffix_font = u8g2_font_6x13B_tf;
	u8g2_SetFont(&s_u8g2, number_font);
	int value_width = u8g2_GetStrWidth(&s_u8g2, value);
	u8g2_SetFont(&s_u8g2, suffix_font);
	int suffix_width = suffix[0] ? u8g2_GetStrWidth(&s_u8g2, suffix) + 1 : 0;
	if (value_width + suffix_width > 60) {
		number_font = u8g2_font_logisoso18_tn;
		u8g2_SetFont(&s_u8g2, number_font);
		value_width = u8g2_GetStrWidth(&s_u8g2, value);
	}
	int total_width = value_width + suffix_width;
	int x = centered_cell_x(cell_x, total_width) + dx;
	u8g2_SetFont(&s_u8g2, number_font);
	u8g2_DrawStr(&s_u8g2, x, value_y, value);
	if (suffix[0]) {
		u8g2_SetFont(&s_u8g2, suffix_font);
		u8g2_DrawStr(&s_u8g2, x + value_width + 1, value_y, suffix);
	}
}

static void draw_environment_cell(int cell_x, int dx,
				  int label_y, int value_y,
				  const char *label, bool label_utf8,
				  const mixer_metric_t *metric,
				  const char *suffix, bool decimal)
{
	char value[16];
	format_metric(value, sizeof(value), metric, decimal);
	u8g2_SetFont(&s_u8g2, label_utf8 ? u8g2_font_9x15_t_cyrillic :
			    u8g2_font_9x15B_tf);
	int label_width = label_utf8 ? u8g2_GetUTF8Width(&s_u8g2, label) :
		u8g2_GetStrWidth(&s_u8g2, label);
	int label_x = centered_cell_x(cell_x, label_width) + dx;
	if (label_utf8)
		u8g2_DrawUTF8(&s_u8g2, label_x, label_y, label);
	else
		u8g2_DrawStr(&s_u8g2, label_x, label_y, label);
	draw_environment_value(cell_x, dx, value_y, value, suffix);
}

static void draw_environment(const mixer_snapshot_t *snapshot, bool english,
			     bool host_weather_active,
			     bool host_weather_valid,
			     uint8_t rain_chance)
{
	int dx = 0;
	int dy = 0;
	environment_shift(&dx, &dy);
	draw_environment_cell(0, dx, 15 + dy, 46 + dy,
			      english ? "TEMP" : "ТЕМП", !english,
			      &snapshot->temperature, "C", true);
	draw_environment_cell(64, dx, 15 + dy, 46 + dy,
			      english ? "HUM" : "ВОЛОГ", !english,
			      &snapshot->humidity, "%", true);
	draw_environment_cell(0, dx, 78 + dy, 109 + dy, "CO2", false,
			      &snapshot->co2, "", false);
	if (host_weather_active) {
		char value[8];
		snprintf(value, sizeof(value), host_weather_valid ? "%u" : "--",
			 (unsigned)rain_chance);
		const char *label = english ? "RAIN" : "ДОЩ";
		u8g2_SetFont(&s_u8g2, english ? u8g2_font_9x15B_tf :
				    u8g2_font_9x15_t_cyrillic);
		int label_width = english ? u8g2_GetStrWidth(&s_u8g2, label) :
			u8g2_GetUTF8Width(&s_u8g2, label);
		int label_x = centered_cell_x(64, label_width) + dx;
		if (english)
			u8g2_DrawStr(&s_u8g2, label_x, 78 + dy, label);
		else
			u8g2_DrawUTF8(&s_u8g2, label_x, 78 + dy, label);
		draw_environment_value(64, dx, 109 + dy, value,
				       host_weather_valid ? "%" : "");
	} else {
		draw_environment_cell(64, dx, 78 + dy, 109 + dy,
				      english ? "LUX" : "ЛЮКС", !english,
				      &snapshot->lux, "", false);
	}
}

static void draw_screen(void)
{
	mixer_snapshot_t snapshot;
	app_snapshot_get(&snapshot);
	mixer_screen_t screen;
	bool english;
	bool connected;
	bool reliable;
	bool mesh_notice;
	bool host_weather_active;
	bool host_weather_valid;
	uint8_t host_rain_chance;
	uint32_t screen_entered;
	char notice[32];
	char garland[16], power[16], mode[16], brightness[16];
	portENTER_CRITICAL(&s_lock);
	screen = s_screen;
	english = s_english;
	connected = s_mesh_connected;
	reliable = s_reliable;
	host_weather_active = s_host_weather_active &&
		(uint32_t)(now_ms() - s_host_weather_updated_ms) <=
			(uint32_t)CONFIG_MIXER_HOST_WEATHER_LEASE_MS;
	host_weather_valid = s_host_weather_valid;
	host_rain_chance = s_host_rain_chance;
	screen_entered = s_screen_entered_ms;
	mesh_notice = (int32_t)(s_mesh_notice_until_ms - now_ms()) > 0;
	memcpy(notice, s_mesh_notice, sizeof(notice));
	memcpy(garland, s_garland, sizeof(garland));
	memcpy(power, s_red_power, sizeof(power));
	memcpy(mode, s_red_mode, sizeof(mode));
	memcpy(brightness, s_red_brightness, sizeof(brightness));
	portEXIT_CRITICAL(&s_lock);

	if (screen == MIXER_SCREEN_PULSE && !snapshot.heart_rate.valid &&
	    (uint32_t)(now_ms() - screen_entered) >= 3000U) {
		display_controller_set_screen(MIXER_SCREEN_ENVIRONMENT);
		screen = MIXER_SCREEN_ENVIRONMENT;
	}
	if (mesh_notice) screen = MIXER_SCREEN_MESH;

	u8g2_ClearBuffer(&s_u8g2);
	u8g2_SetFont(&s_u8g2, english ? u8g2_font_6x13_tf :
			    u8g2_font_cu12_t_cyrillic);
	char line[48];
	switch (screen) {
	case MIXER_SCREEN_ENVIRONMENT:
		draw_environment(&snapshot, english, host_weather_active,
				 host_weather_valid, host_rain_chance);
		break;
	case MIXER_SCREEN_PULSE:
		u8g2_DrawUTF8(&s_u8g2, 0, 18, english ? "Pulse" : "Пульс");
		if (snapshot.heart_rate.valid) {
			snprintf(line, sizeof(line), "%.1f BPM",
				 snapshot.heart_rate.value_x10 / 10.0);
		} else {
			snprintf(line, sizeof(line), "-- BPM");
		}
		u8g2_DrawUTF8(&s_u8g2, 0, 48, line);
		u8g2_DrawUTF8(&s_u8g2, 0, 78,
			      english ? "Put finger on sensor" :
					"Прикладіть палець");
		break;
	case MIXER_SCREEN_CLOCK:
		if (snapshot.rtc_valid) {
			snprintf(line, sizeof(line), "%02u-%02u",
				 snapshot.day, snapshot.month);
			u8g2_DrawStr(&s_u8g2, 0, 42, line);
			snprintf(line, sizeof(line), "%02u:%02u:%02u",
				 snapshot.hour, snapshot.minute, snapshot.second);
			u8g2_DrawStr(&s_u8g2, 0, 82, line);
		} else {
			u8g2_DrawStr(&s_u8g2, 0, 64, "--:--:--");
		}
		break;
	case MIXER_SCREEN_EGG:
		u8g2_DrawStr(&s_u8g2, 34, 58, "KeeMASH");
		u8g2_DrawStr(&s_u8g2, 24, 82, "egg screen");
		break;
	case MIXER_SCREEN_MESH:
		u8g2_DrawUTF8(&s_u8g2, 0, 30, notice);
		snprintf(line, sizeof(line), "Mesh: %s",
			 connected ? (reliable ? "lossless" : "joining") : "offline");
		u8g2_DrawStr(&s_u8g2, 0, 52, line);
		break;
	case MIXER_SCREEN_LIGHTING:
		snprintf(line, sizeof(line), "%s %s",
			 english ? "Light" : "Світло", power);
		u8g2_DrawUTF8(&s_u8g2, 0, 18, line);
		snprintf(line, sizeof(line), "%s %s",
			 english ? "Effect" : "Ефект", mode);
		u8g2_DrawUTF8(&s_u8g2, 0, 43, line);
		snprintf(line, sizeof(line), "%s %s",
			 english ? "Brightness" : "Яскравість", brightness);
		u8g2_DrawUTF8(&s_u8g2, 0, 68, line);
		snprintf(line, sizeof(line), "%s %s",
			 english ? "Garland" : "Гірлянда", garland);
		u8g2_DrawUTF8(&s_u8g2, 0, 98, line);
		break;
	}
	u8g2_SendBuffer(&s_u8g2);
}

static void display_task(void *arg)
{
	(void)arg;
	uint32_t reported_errors = s_spi_error_count;
	for (;;) {
		draw_screen();
		if (reported_errors != s_spi_error_count) {
			reported_errors = s_spi_error_count;
			ESP_LOGW(TAG, "SSD1327 SPI errors=%lu last=%s",
				 (unsigned long)reported_errors,
				 esp_err_to_name(s_spi_last_error));
		}
		vTaskDelay(pdMS_TO_TICKS(CONFIG_MIXER_DISPLAY_REFRESH_MS));
	}
}

esp_err_t display_controller_start(void)
{
	if (s_display_task) return ESP_OK;
	s_screen_entered_ms = now_ms();
	spi_bus_config_t bus = {
		.mosi_io_num = CONFIG_MIXER_DISPLAY_MOSI_GPIO,
		.miso_io_num = -1,
		.sclk_io_num = CONFIG_MIXER_DISPLAY_SCLK_GPIO,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 2048,
	};
	esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
	if (err == ESP_OK) {
		s_spi_bus_owned = true;
	} else if (err != ESP_ERR_INVALID_STATE) {
		return err;
	}
	spi_device_interface_config_t dev = {
		.clock_speed_hz = CONFIG_MIXER_DISPLAY_SPI_CLOCK_HZ,
		.mode = 0,
		.spics_io_num = CONFIG_MIXER_DISPLAY_CS_GPIO,
		.queue_size = 1,
	};
	if ((err = spi_bus_add_device(SPI2_HOST, &dev, &s_spi)) != ESP_OK) {
		if (s_spi_bus_owned) {
			(void)spi_bus_free(SPI2_HOST);
			s_spi_bus_owned = false;
		}
		return err;
	}
	u8g2_Setup_ssd1327_ws_128x128_f(&s_u8g2, U8G2_R0, byte_spi, gpio_delay);
	u8g2_InitDisplay(&s_u8g2);
	u8g2_SetPowerSave(&s_u8g2, 0);
	u8g2_SetContrast(&s_u8g2, CONFIG_MIXER_DISPLAY_CONTRAST);
	u8g2_ClearDisplay(&s_u8g2);
	if (xTaskCreate(display_task, "display", 4096, NULL, 3,
			&s_display_task) != pdPASS) {
		s_display_task = NULL;
		(void)spi_bus_remove_device(s_spi);
		s_spi = NULL;
		if (s_spi_bus_owned) {
			(void)spi_bus_free(SPI2_HOST);
			s_spi_bus_owned = false;
		}
		return ESP_ERR_NO_MEM;
	}
	ESP_LOGI(TAG,
		 "SSD1327 ready spi=%d Hz contrast=%d refresh=%d ms errors=%lu",
		 CONFIG_MIXER_DISPLAY_SPI_CLOCK_HZ,
		 CONFIG_MIXER_DISPLAY_CONTRAST,
		 CONFIG_MIXER_DISPLAY_REFRESH_MS,
		 (unsigned long)s_spi_error_count);
	return ESP_OK;
}

bool display_controller_ready(void)
{
	return s_display_task != NULL && s_spi != NULL;
}

mixer_screen_t display_controller_screen(void)
{
	portENTER_CRITICAL(&s_lock);
	mixer_screen_t screen = s_screen;
	portEXIT_CRITICAL(&s_lock);
	return screen;
}

void display_controller_set_screen(mixer_screen_t screen)
{
	if (screen > MIXER_SCREEN_LIGHTING) return;
	bool pulse_before;
	bool pulse_after;
	portENTER_CRITICAL(&s_lock);
	pulse_before = s_screen == MIXER_SCREEN_PULSE;
	s_screen = screen;
	s_screen_entered_ms = now_ms();
	pulse_after = s_screen == MIXER_SCREEN_PULSE;
	portEXIT_CRITICAL(&s_lock);
	if (pulse_before != pulse_after) (void)sensor_pulse_enable(pulse_after);
}

void display_controller_toggle_language(void)
{
	portENTER_CRITICAL(&s_lock);
	s_english = !s_english;
	portEXIT_CRITICAL(&s_lock);
}

void display_controller_set_mesh(bool connected, bool reliable)
{
	portENTER_CRITICAL(&s_lock);
	s_mesh_connected = connected;
	s_reliable = reliable;
	portEXIT_CRITICAL(&s_lock);
}

void display_controller_show_mesh_event(const char *text)
{
	if (!text || !text[0]) return;
	portENTER_CRITICAL(&s_lock);
	snprintf(s_mesh_notice, sizeof(s_mesh_notice), "%s", text);
	s_mesh_notice_until_ms = now_ms() + 3000U;
	portEXIT_CRITICAL(&s_lock);
}

void display_controller_set_lighting_state(const char *token)
{
	if (!token) return;
	portENTER_CRITICAL(&s_lock);
	if (strcmp(token, "garland_on") == 0) strcpy(s_garland, "ON");
	else if (strcmp(token, "garland_off") == 0) strcpy(s_garland, "OFF");
	else if (strcmp(token, "redled_on") == 0) strcpy(s_red_power, "ON");
	else if (strcmp(token, "redled_off") == 0) strcpy(s_red_power, "OFF");
	else if (strncmp(token, "01_mode_", 8) == 0)
		snprintf(s_red_mode, sizeof(s_red_mode), "%s", token + 8);
	else if (strncmp(token, "02", 2) == 0)
		snprintf(s_red_brightness, sizeof(s_red_brightness), "%s", token + 2);
	portEXIT_CRITICAL(&s_lock);
}

void display_controller_set_host_weather(bool active, bool valid,
					 uint8_t rain_chance_percent)
{
	portENTER_CRITICAL(&s_lock);
	s_host_weather_active = active;
	s_host_weather_valid = active && valid;
	s_host_rain_chance = rain_chance_percent <= 100 ?
		rain_chance_percent : 100;
	s_host_weather_updated_ms = now_ms();
	portEXIT_CRITICAL(&s_lock);
}
