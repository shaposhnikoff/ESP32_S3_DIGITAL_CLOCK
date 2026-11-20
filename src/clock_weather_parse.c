#include "clock_weather.h"
#include <math.h>
#include "cJSON.h"

bool clock_weather_parse(const char *json, clock_weather_t *result)
{
    cJSON *root = cJSON_ParseWithOpts(json, NULL, true);
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    const cJSON *temperature = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
    const cJSON *code = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
    const cJSON *instant = cJSON_GetObjectItemCaseSensitive(current, "time");
    bool valid = cJSON_IsNumber(temperature) && cJSON_IsNumber(code) && cJSON_IsNumber(instant);
    if (valid) {
        valid = isfinite(temperature->valuedouble) &&
            temperature->valuedouble >= -100 && temperature->valuedouble <= 70 &&
            isfinite(code->valuedouble) && code->valuedouble >= 0 && code->valuedouble <= 99 &&
            floor(code->valuedouble) == code->valuedouble &&
            isfinite(instant->valuedouble) && instant->valuedouble >= 946684800 &&
            instant->valuedouble <= 4102444800.0 &&
            floor(instant->valuedouble) == instant->valuedouble;
    }
    if (valid) {
        *result = (clock_weather_t){
            .valid = true, .temperature = (float)temperature->valuedouble,
            .code = (int)code->valuedouble, .observed_at = (time_t)instant->valuedouble,
        };
    }
    cJSON_Delete(root);
    return valid;
}

const char *clock_weather_condition(int code)
{
    switch (code) {
    case 0: return "CLEAR";
    case 1: return "MAINLY CLEAR";
    case 2: return "PARTLY CLOUDY";
    case 3: return "OVERCAST";
    case 45: case 48: return "FOG";
    case 51: case 53: case 55: return "DRIZZLE";
    case 56: case 57: case 66: case 67: return "FREEZING RAIN";
    case 61: case 63: case 65: case 80: case 81: case 82: return "RAIN";
    case 71: case 73: case 75: case 77: case 85: case 86: return "SNOW";
    case 95: case 96: case 99: return "THUNDERSTORM";
    default: return "UNKNOWN";
    }
}
