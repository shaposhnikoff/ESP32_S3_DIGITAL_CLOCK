#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

typedef int esp_err_t;
typedef const char *esp_event_base_t;
typedef unsigned TickType_t;
typedef int BaseType_t;
typedef void *QueueHandle_t;
typedef struct { int unused; } esp_netif_t;

#define ESP_OK 0
#define ESP_ERROR_CHECK(expression) do { assert((expression) == ESP_OK); } while (0)
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(milliseconds) (milliseconds)

extern const char test_wifi_base[];
extern const char test_ip_base[];
#define WIFI_EVENT test_wifi_base
#define IP_EVENT test_ip_base
enum { WIFI_EVENT_STA_START = 1, WIFI_EVENT_STA_DISCONNECTED, IP_EVENT_STA_GOT_IP };
enum { ESP_EVENT_ANY_ID = -1, WIFI_AUTH_OPEN, WIFI_AUTH_WPA2_PSK,
       WPA3_SAE_PWE_BOTH, WIFI_STORAGE_RAM, WIFI_MODE_STA, WIFI_IF_STA, WIFI_PS_NONE };

typedef struct { bool nvs_enable; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() { .nvs_enable = true }
typedef struct {
    struct {
        uint8_t ssid[32];
        uint8_t password[64];
        struct { int authmode; } threshold;
        int sae_pwe_h2e;
    } sta;
} wifi_config_t;
typedef struct {
    bool start;
    bool wait_for_sync;
    void (*sync_cb)(struct timeval *);
    unsigned num_of_servers;
    const char *servers[3];
} esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(count, list) \
    { .start = true, .wait_for_sync = true, .num_of_servers = count, .servers = list }

typedef void (*event_handler_t)(void *, esp_event_base_t, int32_t, void *);
esp_err_t esp_netif_init(void);
esp_err_t esp_event_loop_create_default(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
QueueHandle_t xQueueCreate(unsigned count, unsigned size);
BaseType_t xQueueOverwrite(QueueHandle_t queue, const void *item);
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait);
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config);
void esp_sntp_set_sync_interval(uint32_t milliseconds);
esp_err_t esp_netif_sntp_start(void);
esp_err_t esp_wifi_init(const wifi_init_config_t *config);
esp_err_t esp_wifi_set_storage(int storage);
esp_err_t esp_event_handler_register(esp_event_base_t base, int32_t id,
                                     event_handler_t handler, void *argument);
esp_err_t esp_wifi_set_mode(int mode);
esp_err_t esp_wifi_set_config(int interface, const wifi_config_t *config);
BaseType_t xTaskCreate(void (*entry)(void *), const char *name, unsigned stack,
                       void *argument, unsigned priority, void *handle);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_set_ps(int mode);
esp_err_t esp_wifi_connect(void);
const char *esp_err_to_name(esp_err_t error);
