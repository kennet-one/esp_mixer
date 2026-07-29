#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
	MIXER_SCREEN_ENVIRONMENT = 0,
	MIXER_SCREEN_PULSE = 1,
	MIXER_SCREEN_CLOCK = 2,
	MIXER_SCREEN_EGG = 3,
	MIXER_SCREEN_MESH = 4,
	MIXER_SCREEN_LIGHTING = 5,
} mixer_screen_t;

esp_err_t display_controller_start(void);
bool display_controller_ready(void);
mixer_screen_t display_controller_screen(void);
void display_controller_set_screen(mixer_screen_t screen);
void display_controller_toggle_language(void);
void display_controller_set_mesh(bool connected, bool reliable);
void display_controller_show_mesh_event(const char *text);
void display_controller_set_lighting_state(const char *token);
