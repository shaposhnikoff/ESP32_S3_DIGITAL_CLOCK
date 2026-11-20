#pragma once
#include <stdbool.h>
#include <time.h>

typedef struct {
    bool valid;
    float temperature;
    int code;
    time_t observed_at;
} clock_weather_t;

bool clock_weather_parse(const char *json, clock_weather_t *result);
const char *clock_weather_condition(int code);
void clock_weather_start(void);
clock_weather_t clock_weather_read(void);
