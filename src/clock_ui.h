#pragma once

#include <stdbool.h>
#include <time.h>

void clock_ui_init(int width, int height);
void clock_ui_status_update(int temperature, const char *condition, int rssi);
void clock_ui_update(const struct tm *local_time, bool valid, bool format_24h, const char *locale);
