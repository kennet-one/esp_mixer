#include "app_snapshot.h"

#include <string.h>

#include "freertos/FreeRTOS.h"

#include "keemash_mesh_proto.h"

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static mixer_snapshot_t s_snapshot;

void app_snapshot_init(void)
{
	portENTER_CRITICAL(&s_lock);
	memset(&s_snapshot, 0, sizeof(s_snapshot));
	portEXIT_CRITICAL(&s_lock);
}

void app_snapshot_get(mixer_snapshot_t *out)
{
	if (!out) return;
	portENTER_CRITICAL(&s_lock);
	*out = s_snapshot;
	portEXIT_CRITICAL(&s_lock);
}

static mixer_metric_t *metric_for_id(uint16_t metric_id)
{
	switch (metric_id) {
	case MESH_V2_SENSOR_METRIC_CO2_PPM: return &s_snapshot.co2;
	case MESH_V2_SENSOR_METRIC_TEMPERATURE_C: return &s_snapshot.temperature;
	case MESH_V2_SENSOR_METRIC_HUMIDITY_RH: return &s_snapshot.humidity;
	case MESH_V2_SENSOR_METRIC_ILLUMINANCE_LUX: return &s_snapshot.lux;
	case MESH_V2_SENSOR_METRIC_HEART_RATE_BPM: return &s_snapshot.heart_rate;
	default: return NULL;
	}
}

void app_snapshot_update_metric(uint16_t metric_id, bool valid, bool calibrated,
				int32_t value_x10, esp_err_t error,
				uint32_t updated_ms)
{
	portENTER_CRITICAL(&s_lock);
	mixer_metric_t *metric = metric_for_id(metric_id);
	if (metric) {
		metric->valid = valid;
		metric->calibrated = calibrated;
		metric->value_x10 = value_x10;
		metric->error = error;
		metric->updated_ms = updated_ms;
		s_snapshot.generation++;
	}
	portEXIT_CRITICAL(&s_lock);
}

void app_snapshot_update_rtc(bool valid, uint8_t hour, uint8_t minute,
			     uint8_t second, uint8_t day, uint8_t month,
			     esp_err_t error, uint32_t updated_ms)
{
	portENTER_CRITICAL(&s_lock);
	s_snapshot.rtc_valid = valid;
	s_snapshot.hour = hour;
	s_snapshot.minute = minute;
	s_snapshot.second = second;
	s_snapshot.day = day;
	s_snapshot.month = month;
	s_snapshot.rtc_error = error;
	s_snapshot.rtc_updated_ms = updated_ms;
	s_snapshot.generation++;
	portEXIT_CRITICAL(&s_lock);
}
