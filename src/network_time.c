#include "network_time.h"

#include <stdatomic.h>
#include <stdlib.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "clock_config.h"

typedef enum {
    NETWORK_START,
    NETWORK_UP,
    NETWORK_DOWN,
} network_signal_t;

static const char *TAG = "clock_time";
static QueueHandle_t network_events;
static atomic_bool time_valid;

static void synchronized(struct timeval *utc)
{
    (void)utc;
    atomic_store_explicit(&time_valid, true, memory_order_release);
    ESP_LOGI(TAG, "UTC synchronized");
}

static void network_event(void *argument, esp_event_base_t base, int32_t id, void *data)
{
    (void)argument;
    (void)data;
    network_signal_t signal;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        signal = NETWORK_START;
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        signal = NETWORK_DOWN;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        signal = NETWORK_UP;
    } else {
        return;
    }
    xQueueOverwrite(network_events, &signal);
}

static void network_worker(void *argument)
{
    (void)argument;
    TickType_t wait = portMAX_DELAY;
    unsigned backoff_seconds = 1;
    while (true) {
        network_signal_t signal;
        if (xQueueReceive(network_events, &signal, wait) != pdTRUE) {
            signal = NETWORK_START;
        }
        switch (signal) {
        case NETWORK_UP:
            ESP_LOGI(TAG, "Wi-Fi connected; requesting NTP");
            ESP_ERROR_CHECK(esp_netif_sntp_start());
            backoff_seconds = 1;
            wait = portMAX_DELAY;
            break;
        case NETWORK_DOWN:
            ESP_LOGW(TAG, "Wi-Fi disconnected; retry in %u seconds", backoff_seconds);
            wait = pdMS_TO_TICKS(backoff_seconds * 1000);
            backoff_seconds = backoff_seconds < 32 ? backoff_seconds * 2 : 60;
            break;
        case NETWORK_START: {
            esp_err_t error = esp_wifi_connect();
            if (error == ESP_OK) {
                wait = portMAX_DELAY;
            } else {
                ESP_LOGW(TAG, "Wi-Fi connect failed: %s", esp_err_to_name(error));
                wait = pdMS_TO_TICKS(backoff_seconds * 1000);
                backoff_seconds = backoff_seconds < 32 ? backoff_seconds * 2 : 60;
            }
            break;
        }
        }
    }
}

void network_time_start(void)
{
    if (setenv("TZ", CLOCK_TIMEZONE_POSIX, 1) != 0) {
        abort();
    }
    tzset();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *station_interface = esp_netif_create_default_wifi_sta();
    if (station_interface == NULL) {
        abort();
    }
    network_events = xQueueCreate(1, sizeof(network_signal_t));
    if (network_events == NULL) {
        abort();
    }

    esp_sntp_config_t time_config =
        ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(CLOCK_NTP_SERVER_COUNT, CLOCK_NTP_SERVERS);
    time_config.start = false;
    time_config.wait_for_sync = false;
    time_config.sync_cb = synchronized;
    esp_sntp_set_sync_interval(CLOCK_SYNC_INTERVAL_SECONDS * 1000U);
    ESP_ERROR_CHECK(esp_netif_sntp_init(&time_config));

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    init.nvs_enable = false;
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, network_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, network_event, NULL));
    wifi_config_t station = {
        .sta = {
            .ssid = CLOCK_WIFI_SSID,
            .password = CLOCK_WIFI_PASSWORD,
            .threshold.authmode = sizeof(CLOCK_WIFI_PASSWORD) == 1
                                      ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &station));
    BaseType_t created = xTaskCreate(network_worker, "clock_network", 4096, NULL, 4, NULL);
    if (created != pdPASS) {
        abort();
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

bool network_time_local(struct tm *result)
{
    if (!atomic_load_explicit(&time_valid, memory_order_acquire)) {
        return false;
    }
    time_t utc = time(NULL);
    return localtime_r(&utc, result) != NULL;
}
