#include "gesture_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "command_adapter.h"
#include "display_controller.h"
#include "sensor_drivers.h"

static TaskHandle_t s_gesture_task;

static void gesture_task(void *arg)
{
	(void)arg;
	uint32_t last_lighting_refresh = 0;
	bool lighting_was_open = false;
	for (;;) {
		mixer_gesture_t gesture = MIXER_GESTURE_NONE;
		if (sensor_read_gesture(&gesture) == ESP_OK &&
		    gesture != MIXER_GESTURE_NONE) {
			mixer_screen_t screen = display_controller_screen();
			switch (gesture) {
			case MIXER_GESTURE_BACKWARD:
				(void)command_adapter_emit("next_eff");
				break;
			case MIXER_GESTURE_LEFT:
				if (screen == MIXER_SCREEN_ENVIRONMENT)
					display_controller_set_screen(MIXER_SCREEN_PULSE);
				else if (screen == MIXER_SCREEN_LIGHTING)
					(void)command_adapter_emit("02_bri_5");
				break;
			case MIXER_GESTURE_RIGHT:
				if (screen == MIXER_SCREEN_ENVIRONMENT)
					display_controller_set_screen(MIXER_SCREEN_CLOCK);
				else if (screen == MIXER_SCREEN_LIGHTING)
					(void)command_adapter_emit("garland");
				break;
			case MIXER_GESTURE_UP:
				if (screen != MIXER_SCREEN_LIGHTING)
					display_controller_set_screen(MIXER_SCREEN_ENVIRONMENT);
				break;
			case MIXER_GESTURE_DOWN:
				if (screen == MIXER_SCREEN_ENVIRONMENT)
					display_controller_set_screen(MIXER_SCREEN_LIGHTING);
				break;
			case MIXER_GESTURE_CLOCKWISE:
				(void)command_adapter_emit("power");
				break;
			case MIXER_GESTURE_ANTICLOCKWISE:
				display_controller_toggle_language();
				break;
			case MIXER_GESTURE_WAVE:
				display_controller_set_screen(MIXER_SCREEN_EGG);
				break;
			default:
				break;
			}
		}

		bool lighting = display_controller_screen() == MIXER_SCREEN_LIGHTING;
		uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
		if (lighting && (!lighting_was_open ||
		    (uint32_t)(now - last_lighting_refresh) >= 5000)) {
			(void)command_adapter_emit("garland_echo");
			(void)command_adapter_emit("red_led_echo");
			last_lighting_refresh = now;
		}
		lighting_was_open = lighting;
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}

esp_err_t gesture_controller_start(void)
{
	if (s_gesture_task) return ESP_OK;
	if (xTaskCreate(gesture_task, "gesture", 3072, NULL, 4,
			&s_gesture_task) != pdPASS) {
		s_gesture_task = NULL;
		return ESP_ERR_NO_MEM;
	}
	return ESP_OK;
}
