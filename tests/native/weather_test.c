#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "clock_weather.h"

int main(void)
{
    clock_weather_t value = {0};
    assert(clock_weather_parse(
        "{\"current\":{\"temperature_2m\":4.2,\"weather_code\":3,\"time\":1795190400}}", &value));
    assert(value.valid && fabsf(value.temperature - 4.2f) < 0.001f);
    assert(value.observed_at == 1795190400 && strcmp(clock_weather_condition(value.code), "OVERCAST") == 0);
    const char *invalid[] = {
        "{}", "{\"current\":null}", "not json",
        "{\"current\":{\"temperature_2m\":null,\"weather_code\":3,\"time\":1795190400}}",
        "{\"current\":{\"temperature_2m\":400,\"weather_code\":3,\"time\":1795190400}}",
        "{\"current\":{\"temperature_2m\":4,\"weather_code\":3.5,\"time\":1795190400}}",
        "{\"current\":{\"temperature_2m\":4,\"weather_code\":3,\"time\":1e99}}",
        "{\"current\":{\"temperature_2m\":4,\"weather_code\":3,\"time\":1795190400}} trailing",
    };
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(!clock_weather_parse(invalid[i], &value));
        assert(value.valid && value.code == 3);
    }
    assert(strcmp(clock_weather_condition(999), "UNKNOWN") == 0);
    printf("PASS: real weather parser accepts current data and rejects malformed readings\n");
    return 0;
}
