#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

esp_err_t board_init(uint8_t brightness_percent);
esp_lcd_panel_handle_t board_panel(void);
int board_width(void);
int board_height(void);
