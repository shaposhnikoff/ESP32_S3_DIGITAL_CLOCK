#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "board.h"
#include "clock_config.h"
#include "clock_ui.h"
#include "clock_weather.h"
#include "network_time.h"

static void flush_display(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(board_panel(), area->x1, area->y1,
                                            area->x2 + 1, area->y2 + 1, pixels));
    lv_disp_flush_ready(driver);
}

static void tick(void *argument)
{
    (void)argument;
    lv_tick_inc(2);
}

void app_main(void)
{
    ESP_ERROR_CHECK(board_init(CLOCK_BRIGHTNESS));
    lv_init();
    static lv_disp_draw_buf_t draw_buffer;
    size_t pixels = (size_t)board_width() * 40;
    lv_color_t *buffer = heap_caps_malloc(pixels * sizeof(lv_color_t),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(buffer != NULL);
    lv_disp_draw_buf_init(&draw_buffer, buffer, NULL, pixels);
    static lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = board_width();
    driver.ver_res = board_height();
    driver.flush_cb = flush_display;
    driver.draw_buf = &draw_buffer;
    lv_disp_drv_register(&driver);

    const esp_timer_create_args_t tick_args = {
        .callback = tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 2000));
    clock_ui_init(board_width(), board_height());
    clock_ui_update(NULL, false, CLOCK_FORMAT_24H, CLOCK_LOCALE);
    network_time_start();
    clock_weather_start();
    int64_t last_update = -1000000;
    int64_t last_status = -1000000;
    while (true) {
        int64_t now = esp_timer_get_time();
        if (now - last_status >= 1000000) {
            clock_weather_t weather = clock_weather_read();
            wifi_ap_record_t access_point;
            int rssi = esp_wifi_sta_get_ap_info(&access_point) == ESP_OK ? access_point.rssi : -127;
            clock_ui_status_update(weather.valid ? (int)lroundf(weather.temperature) : 0,
                weather.valid ? clock_weather_condition(weather.code) : NULL, rssi);
            last_status = now;
        }
        if (now - last_update >= 100000) {
            struct tm local_time;
            bool valid = network_time_local(&local_time);
            clock_ui_update(valid ? &local_time : NULL, valid, CLOCK_FORMAT_24H, CLOCK_LOCALE);
            last_update = now;
        }
        uint32_t delay = lv_timer_handler();
        if (delay > 10) {
            delay = 10;
        }
        vTaskDelay(pdMS_TO_TICKS(delay > 0 ? delay : 1));
    }
}
