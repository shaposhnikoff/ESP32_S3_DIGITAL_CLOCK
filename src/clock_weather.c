#include "clock_weather.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "clock_config.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network_time.h"

static portMUX_TYPE weather_lock = portMUX_INITIALIZER_UNLOCKED;
static clock_weather_t latest;
typedef struct { char json[4096]; size_t used; bool overflow; } response_t;

static esp_err_t receive(esp_http_client_event_t *event)
{
    response_t *response = event->user_data;
    if (event->event_id == HTTP_EVENT_ON_DATA) {
        if ((size_t)event->data_len >= sizeof(response->json) - response->used) {
            response->overflow = true;
            return ESP_ERR_NO_MEM;
        }
        memcpy(response->json + response->used, event->data, event->data_len);
        response->used += event->data_len;
        response->json[response->used] = 0;
    }
    return ESP_OK;
}

static bool fetch(clock_weather_t *result)
{
    char url[256];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.6f&longitude=%.6f"
        "&current=temperature_2m,weather_code&timeformat=unixtime&timezone=UTC",
        CLOCK_WEATHER_LATITUDE, CLOCK_WEATHER_LONGITUDE);
    response_t response = {0};
    esp_http_client_config_t config = {
        .url = url, .timeout_ms = CLOCK_WEATHER_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = receive, .user_data = &response,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return false;
    esp_err_t error = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    bool valid = error == ESP_OK && status == 200 && !response.overflow &&
        clock_weather_parse(response.json, result);
    if (!valid) ESP_LOGW("weather", "Request failed: %s, HTTP %d", esp_err_to_name(error), status);
    esp_http_client_cleanup(client);
    return valid;
}

static void weather_worker(void *argument)
{
    (void)argument;
    int64_t next_poll = 0;
    while (true) {
        struct tm local;
        wifi_ap_record_t access_point;
        int64_t now = esp_timer_get_time();
        if (now >= next_poll && network_time_local(&local) &&
            esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
            clock_weather_t reading;
            if (fetch(&reading)) {
                portENTER_CRITICAL(&weather_lock);
                latest = reading;
                portEXIT_CRITICAL(&weather_lock);
                ESP_LOGI("weather", "Current weather updated");
            }
            next_poll = esp_timer_get_time() + (int64_t)CLOCK_WEATHER_UPDATE_SECONDS * 1000000;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void clock_weather_start(void)
{
    assert(xTaskCreate(weather_worker, "weather", 12288, NULL, 3, NULL) == pdPASS);
}

clock_weather_t clock_weather_read(void)
{
    portENTER_CRITICAL(&weather_lock);
    clock_weather_t reading = latest;
    portEXIT_CRITICAL(&weather_lock);
    double age = difftime(time(NULL), reading.observed_at);
    if (age < -300 || age > CLOCK_WEATHER_STALE_SECONDS) reading.valid = false;
    return reading;
}
