#include "board.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "clock_config.h"

#ifndef CLOCK_BOARD_5B
#error "The verified panel requires the 1024x600 Waveshare RGB profile"
#endif

static const char *TAG = "clock_board";
static esp_lcd_panel_handle_t panel;
static i2c_master_dev_handle_t output_device;
static TaskHandle_t backlight_task;
static esp_timer_handle_t backlight_timer;

static void backlight_wake(void *argument)
{
    (void)argument;
    xTaskNotifyGive(backlight_task);
}

static void backlight_pwm(void *argument)
{
    const unsigned on_us = (unsigned)(uintptr_t)argument * 100;
    bool on = true;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint8_t output = on ? 0x1E : 0x1A;
        ESP_ERROR_CHECK(i2c_master_transmit(output_device, &output, 1, 1000));
        ESP_ERROR_CHECK(esp_timer_start_once(backlight_timer, on ? on_us : 10000 - on_us));
        on = !on;
    }
}

esp_err_t board_init(uint8_t brightness_percent)
{
    i2c_master_bus_handle_t bus;
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 8,
        .scl_io_num = 9,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG, "I2C bus");
    i2c_master_dev_handle_t mode_device;
    i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x24,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &device, &mode_device), TAG, "CH422G mode");
    device.device_address = 0x38;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &device, &output_device),
                        TAG, "CH422G output");
    uint8_t value = 0x01;
    ESP_RETURN_ON_ERROR(i2c_master_transmit(mode_device, &value, 1, 1000), TAG, "CH422G enable");
    value = 0x12;
    ESP_RETURN_ON_ERROR(i2c_master_transmit(output_device, &value, 1, 1000), TAG, "LCD reset low");
    vTaskDelay(pdMS_TO_TICKS(100));
    value = 0x1A;
    ESP_RETURN_ON_ERROR(i2c_master_transmit(output_device, &value, 1, 1000), TAG, "LCD reset high");
    vTaskDelay(pdMS_TO_TICKS(100));

    const esp_lcd_rgb_panel_config_t rgb = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = 21000000,
            .h_res = 1024,
            .v_res = 600,
            .hsync_pulse_width = 30,
            .hsync_back_porch = 145,
            .hsync_front_porch = 170,
            .vsync_pulse_width = 2,
            .vsync_back_porch = 23,
            .vsync_front_porch = 12,
            .flags.pclk_active_neg = true,
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = 1024 * 10,
        .dma_burst_size = 64,
        .hsync_gpio_num = 46,
        .vsync_gpio_num = 3,
        .de_gpio_num = 5,
        .pclk_gpio_num = 7,
        .disp_gpio_num = -1,
        .data_gpio_nums = {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40},
        .flags.fb_in_psram = true,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&rgb, &panel), TAG, "RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "RGB reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "RGB init");
    if (brightness_percent == 0 || brightness_percent == 100) {
        value = brightness_percent == 0 ? 0x1A : 0x1E;
        return i2c_master_transmit(output_device, &value, 1, 1000);
    }
    if (xTaskCreate(backlight_pwm, "backlight_pwm", 3072, (void *)(uintptr_t)brightness_percent,
                    4, &backlight_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    const esp_timer_create_args_t timer = {
        .callback = backlight_wake,
        .name = "backlight_pwm",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer, &backlight_timer), TAG, "Backlight timer");
    xTaskNotifyGive(backlight_task);
    return ESP_OK;
}

esp_lcd_panel_handle_t board_panel(void) { return panel; }
int board_width(void) { return 1024; }
int board_height(void) { return 600; }
