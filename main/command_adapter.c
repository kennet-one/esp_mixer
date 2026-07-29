#include "command_adapter.h"

#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "keemash_mesh_event_outbox.h"
#include "keemash_mesh_node.h"
#include "keemash_mesh_proto.h"

#include "app_snapshot.h"
#include "display_controller.h"

static keemash_mesh_event_outbox_t *s_outbox;

static esp_err_t send_event(void *user, const char *text)
{
	(void)user;
	return mesh_v2_node_send_event(0, text);
}

esp_err_t command_adapter_start(void)
{
	keemash_mesh_event_outbox_config_t cfg = {
		.slots = 16,
		.text_size = MESH_V2_CONTROL_TEXT_MAX,
		.retry_ms = 1000,
		.task_stack_words = 3072,
		.task_priority = 4,
		.task_name = "event_outbox",
		.send = send_event,
	};
	return keemash_mesh_event_outbox_init(&s_outbox, &cfg);
}

esp_err_t command_adapter_emit(const char *command)
{
	if (!s_outbox || !command || !command[0]) return ESP_ERR_INVALID_STATE;
	char event[MESH_V2_CONTROL_TEXT_MAX];
	int n = snprintf(event, sizeof(event), "cmd:%s", command);
	if (n <= 0 || (size_t)n >= sizeof(event)) return ESP_ERR_INVALID_SIZE;
	return keemash_mesh_event_outbox_enqueue(s_outbox, NULL, event);
}

static void add_metric(mesh_v2_sensor_snapshot_payload_t *payload,
		       uint16_t metric_id, const mixer_metric_t *metric)
{
	if (!payload || !metric ||
	    payload->count >= MESH_V2_SENSOR_MAX_ENTRIES) return;
	mesh_v2_sensor_entry_t *entry = &payload->entries[payload->count++];
	entry->metric_id = metric_id;
	entry->status = metric->valid ? MESH_V2_SENSOR_STATUS_VALID :
		MESH_V2_SENSOR_STATUS_ERROR;
	if (metric->calibrated) entry->status |= MESH_V2_SENSOR_STATUS_CALIBRATED;
	entry->scale10 = -1;
	entry->value = metric->value_x10;
}

esp_err_t command_adapter_publish(uint16_t flags, uint32_t request_id,
				  uint16_t metric_filter)
{
	mixer_snapshot_t snapshot;
	app_snapshot_get(&snapshot);
	mesh_v2_sensor_snapshot_payload_t payload = {
		.generation = snapshot.generation,
		.sample_uptime_ms =
			(uint32_t)(esp_timer_get_time() / 1000ULL),
		.request_id = request_id,
		.flags = flags,
	};
	if (metric_filter == 0 ||
	    metric_filter == MESH_V2_SENSOR_METRIC_CO2_PPM)
		add_metric(&payload, MESH_V2_SENSOR_METRIC_CO2_PPM, &snapshot.co2);
	if (metric_filter == 0 ||
	    metric_filter == MESH_V2_SENSOR_METRIC_TEMPERATURE_C)
		add_metric(&payload, MESH_V2_SENSOR_METRIC_TEMPERATURE_C,
			   &snapshot.temperature);
	if (metric_filter == 0 ||
	    metric_filter == MESH_V2_SENSOR_METRIC_HUMIDITY_RH)
		add_metric(&payload, MESH_V2_SENSOR_METRIC_HUMIDITY_RH,
			   &snapshot.humidity);
	if (metric_filter == 0 ||
	    metric_filter == MESH_V2_SENSOR_METRIC_ILLUMINANCE_LUX)
		add_metric(&payload, MESH_V2_SENSOR_METRIC_ILLUMINANCE_LUX,
			   &snapshot.lux);
	if (metric_filter == 0 ||
	    metric_filter == MESH_V2_SENSOR_METRIC_HEART_RATE_BPM)
		add_metric(&payload, MESH_V2_SENSOR_METRIC_HEART_RATE_BPM,
			   &snapshot.heart_rate);
	return mesh_v2_node_send_sensor_snapshot(&payload);
}

bool command_adapter_execute(const char *command, uint8_t *status,
			     char *result, size_t result_size)
{
	if (!command) return false;
	uint16_t metric = 0;
	if (strcmp(command, "ppm_echo") == 0)
		metric = MESH_V2_SENSOR_METRIC_CO2_PPM;
	else if (strcmp(command, "temp_echo") == 0)
		metric = MESH_V2_SENSOR_METRIC_TEMPERATURE_C;
	else if (strcmp(command, "humi_echo") == 0)
		metric = MESH_V2_SENSOR_METRIC_HUMIDITY_RH;
	else if (strcmp(command, "lux_echo") == 0)
		metric = MESH_V2_SENSOR_METRIC_ILLUMINANCE_LUX;
	else if (strcmp(command, "sens_echo") != 0 && strncmp(command, "state:", 6) != 0)
		return false;

	esp_err_t err = ESP_OK;
	if (strncmp(command, "state:", 6) == 0) {
		display_controller_set_lighting_state(command + 6);
	} else {
		err = command_adapter_publish(MESH_V2_SENSOR_FLAG_LEGACY_REPLY,
					     0, metric);
	}
	if (status) *status = err == ESP_OK ? MESH_V2_CONTROL_STATUS_OK :
		MESH_V2_CONTROL_STATUS_FAILED;
	if (result && result_size > 0) {
		snprintf(result, result_size, "%s",
			 err == ESP_OK ? "accepted" : esp_err_to_name(err));
	}
	return true;
}
