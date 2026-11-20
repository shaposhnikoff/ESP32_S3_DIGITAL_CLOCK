#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "network_time.h"

const char test_wifi_base[] = "wifi";
const char test_ip_base[] = "ip";
static event_handler_t registered_event;
static void (*worker_entry)(void *);
static void (*sync_callback)(struct timeval *);
static unsigned char queue_item[32];
static unsigned queue_size;
static bool queued;
static unsigned receive_step;
static unsigned connect_count;
static unsigned sync_count;
static time_t simulated_utc = 1795178280;
static jmp_buf worker_done;

time_t time(time_t *result)
{
    if (result != NULL) {
        *result = simulated_utc;
    }
    return simulated_utc;
}

static void inject(esp_event_base_t base, int32_t event)
{
    registered_event(NULL, base, event, NULL);
}

esp_err_t esp_netif_init(void) { return ESP_OK; }
esp_err_t esp_event_loop_create_default(void) { return ESP_OK; }
esp_netif_t *esp_netif_create_default_wifi_sta(void)
{
    static esp_netif_t interface;
    return &interface;
}
QueueHandle_t xQueueCreate(unsigned count, unsigned size)
{
    assert(count == 1 && size <= sizeof(queue_item));
    queue_size = size;
    return queue_item;
}
BaseType_t xQueueOverwrite(QueueHandle_t queue, const void *item)
{
    assert(queue == queue_item);
    memcpy(queue_item, item, queue_size);
    queued = true;
    return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait)
{
    assert(queue == queue_item);
    struct tm local;
    unsigned step = receive_step++;
    if (step == 0) {
        assert(wait == portMAX_DELAY);
    } else if (step == 1) {
        assert(connect_count == 1 && wait == portMAX_DELAY);
        inject(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
    } else if (step == 2) {
        assert(wait == 1000);
        return 0;
    } else if (step == 3) {
        assert(connect_count == 2);
        inject(IP_EVENT, IP_EVENT_STA_GOT_IP);
    } else if (step == 4) {
        assert(sync_count == 1 && !network_time_local(&local));
        struct timeval utc = { .tv_sec = simulated_utc };
        sync_callback(&utc);
        assert(network_time_local(&local));
        assert(local.tm_hour == 14 && local.tm_min == 38);
        inject(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
    } else if (step == 5) {
        assert(wait == 1000);
        simulated_utc += 60;
        assert(network_time_local(&local) && local.tm_min == 39);
        return 0;
    } else if (step == 6) {
        assert(connect_count == 3);
        inject(IP_EVENT, IP_EVENT_STA_GOT_IP);
    } else if (step == 7) {
        assert(sync_count == 2);
        inject(IP_EVENT, IP_EVENT_STA_GOT_IP);
        inject(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
    } else if (step >= 8 && step <= 15) {
        const unsigned expected[] = {1000, 2000, 4000, 8000, 16000, 32000, 60000, 60000};
        assert(wait == expected[step - 8] && sync_count == 2);
        inject(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
    } else if (step == 16) {
        inject(IP_EVENT, IP_EVENT_STA_GOT_IP);
    } else if (step == 17) {
        assert(sync_count == 3 && wait == portMAX_DELAY);
        inject(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
    } else if (step == 18) {
        assert(wait == 1000);
        return 0;
    } else {
        assert(connect_count == 4 && network_time_local(&local));
        longjmp(worker_done, 1);
    }
    assert(queued);
    memcpy(item, queue_item, queue_size);
    queued = false;
    return pdTRUE;
}
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config)
{
    assert(!config->start && !config->wait_for_sync);
    assert(config->num_of_servers == 3);
    assert(strcmp(config->servers[0], "time.cloudflare.com") == 0);
    assert(strcmp(config->servers[1], "pool.ntp.org") == 0);
    assert(strcmp(config->servers[2], "time.google.com") == 0);
    sync_callback = config->sync_cb;
    return ESP_OK;
}
void esp_sntp_set_sync_interval(uint32_t milliseconds) { assert(milliseconds == 3600000); }
esp_err_t esp_netif_sntp_start(void) { sync_count++; return ESP_OK; }
esp_err_t esp_wifi_init(const wifi_init_config_t *config)
{
    assert(!config->nvs_enable);
    return ESP_OK;
}
esp_err_t esp_wifi_set_storage(int storage) { assert(storage == WIFI_STORAGE_RAM); return ESP_OK; }
esp_err_t esp_event_handler_register(esp_event_base_t base, int32_t id,
                                     event_handler_t handler, void *argument)
{
    (void)base;
    (void)id;
    assert(argument == NULL);
    registered_event = handler;
    return ESP_OK;
}
esp_err_t esp_wifi_set_mode(int mode) { assert(mode == WIFI_MODE_STA); return ESP_OK; }
esp_err_t esp_wifi_set_config(int interface, const wifi_config_t *config)
{
    assert(interface == WIFI_IF_STA);
    assert(strcmp((const char *)config->sta.ssid, "YOUR_WIFI_SSID") == 0);
    return ESP_OK;
}
BaseType_t xTaskCreate(void (*entry)(void *), const char *name, unsigned stack,
                       void *argument, unsigned priority, void *handle)
{
    (void)name;
    (void)stack;
    (void)argument;
    (void)priority;
    (void)handle;
    worker_entry = entry;
    return pdPASS;
}
esp_err_t esp_wifi_start(void) { inject(WIFI_EVENT, WIFI_EVENT_STA_START); return ESP_OK; }
esp_err_t esp_wifi_set_ps(int mode) { assert(mode == WIFI_PS_NONE); return ESP_OK; }
esp_err_t esp_wifi_connect(void) { connect_count++; return ESP_OK; }
const char *esp_err_to_name(esp_err_t error) { (void)error; return "test"; }

int main(void)
{
    struct tm local;
    assert(!network_time_local(&local));
    network_time_start();
    assert(!network_time_local(&local) && connect_count == 0);
    if (setjmp(worker_done) == 0) {
        worker_entry(NULL);
    }
    puts("PASS: startup awaits NTP; offline time advances; reconnect restarts SNTP");
    puts("PASS: latest link state wins; retry backoff caps at 60 seconds and resets");
    return 0;
}
