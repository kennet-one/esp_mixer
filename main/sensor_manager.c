#include "sensor_manager.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keemash_mesh_proto.h"

#include "app_snapshot.h"
#include "command_adapter.h"
#include "display_controller.h"
#include "sensor_drivers.h"

static const char *TAG = "sensor_mgr";

typedef struct {
	esp_err_t last_error;
	uint32_t last_log_ms;
	bool initialized;
} sensor_health_t;

static sensor_health_t s_dht_health;
static sensor_health_t s_co2_health;
static sensor_health_t s_lux_health;
static sensor_health_t s_rtc_health;
static sensor_health_t s_pulse_health;
static TaskHandle_t s_manager_task;

static uint32_t now_ms(void)
{
	return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void update_metric(uint16_t id, esp_err_t err, int32_t value)
{
	app_snapshot_update_metric(id, err == ESP_OK, true, value, err, now_ms());
}

static void report_health(const char *name, sensor_health_t *health,
			  esp_err_t error, uint32_t now)
{
	bool changed = !health->initialized || health->last_error != error;
	bool periodic = error != ESP_OK &&
		(uint32_t)(now - health->last_log_ms) >= 60000U;
	if (changed || periodic) {
		if (error == ESP_OK) {
			ESP_LOGI(TAG, "%s recovered", name);
		} else {
			ESP_LOGW(TAG, "%s read failed: %s",
				 name, esp_err_to_name(error));
		}
		health->last_log_ms = now;
	}
	health->last_error = error;
	health->initialized = true;
}

static void manager_task(void *arg)
{
	(void)arg;
	uint32_t last_dht = 0, last_co2 = 0, last_lux = 0, last_rtc = 0;
	uint32_t initial_due_ms = now_ms();
	uint32_t last_initial_attempt_ms = 0;
	uint32_t last_automation = now_ms();
	bool initial_sent = false;
	for (;;) {
		uint32_t now = now_ms();
		if (now - last_dht >= 2000) {
			int32_t temperature = 0, humidity = 0;
			esp_err_t err = sensor_read_dht(&temperature, &humidity);
			update_metric(MESH_V2_SENSOR_METRIC_TEMPERATURE_C, err, temperature);
			update_metric(MESH_V2_SENSOR_METRIC_HUMIDITY_RH, err, humidity);
			report_health("DHT22", &s_dht_health, err, now);
			last_dht = now;
		}
		if (now - last_co2 >= 2000) {
			int32_t value = 0;
			esp_err_t err = sensor_read_co2(&value);
			update_metric(MESH_V2_SENSOR_METRIC_CO2_PPM, err, value);
			report_health("MH-Z19B", &s_co2_health, err, now);
			last_co2 = now;
		}
		if (now - last_lux >= 1000) {
			int32_t value = 0;
			esp_err_t err = sensor_read_lux(&value);
			update_metric(MESH_V2_SENSOR_METRIC_ILLUMINANCE_LUX, err, value);
			report_health("MAX44009", &s_lux_health, err, now);
			last_lux = now;
		}
		if (now - last_rtc >= 1000) {
			uint8_t h = 0, m = 0, s = 0, d = 0, month = 0;
			esp_err_t err = sensor_read_rtc(&h, &m, &s, &d, &month);
			app_snapshot_update_rtc(err == ESP_OK, h, m, s, d, month,
						err, now);
			report_health("DS3231", &s_rtc_health, err, now);
			last_rtc = now;
		}
		if (display_controller_screen() == MIXER_SCREEN_PULSE) {
			int32_t bpm = 0;
			uint32_t ir = 0;
			esp_err_t err = sensor_read_pulse(&bpm, &ir);
			if (ir < 50000) err = ESP_ERR_INVALID_STATE;
			update_metric(MESH_V2_SENSOR_METRIC_HEART_RATE_BPM, err, bpm);
			report_health("MAX30105", &s_pulse_health, err, now);
		}
		if (!initial_sent &&
		    now - initial_due_ms >= 20000 &&
		    (last_initial_attempt_ms == 0 ||
		     now - last_initial_attempt_ms >= 5000)) {
			last_initial_attempt_ms = now;
			if (command_adapter_publish(MESH_V2_SENSOR_FLAG_LEGACY_REPLY,
						    0, 0) == ESP_OK) {
				initial_sent = true;
			}
		}
		if (now - last_automation >= 200000) {
			(void)command_adapter_publish(
				MESH_V2_SENSOR_FLAG_AUTOMATION_UPDATE, 0,
				MESH_V2_SENSOR_METRIC_TEMPERATURE_C);
			last_automation = now;
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

esp_err_t sensor_manager_start(void)
{
	if (s_manager_task) return ESP_OK;
	esp_err_t err = sensor_drivers_init();
	if (err != ESP_OK) ESP_LOGW(TAG, "shared bus init: %s", esp_err_to_name(err));
	if (xTaskCreatePinnedToCore(manager_task, "sensor_mgr", 4096, NULL, 4,
				    &s_manager_task, 1) != pdPASS) {
		s_manager_task = NULL;
		return ESP_ERR_NO_MEM;
	}
	return ESP_OK;
}

bool sensor_manager_ready(void)
{
	return s_manager_task != NULL;
}
