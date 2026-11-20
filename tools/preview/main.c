#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lvgl.h"
#include "clock_ui.h"

enum { WIDTH = 1024, HEIGHT = 600 };
static lv_color_t framebuffer[WIDTH * HEIGHT];
static unsigned flush_count;
static unsigned flushed_pixels;
static int minimum_flush_x;

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    unsigned count = 0;
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            framebuffer[y * WIDTH + x] = pixels[count++];
        }
    }
    flush_count++;
    flushed_pixels += count;
    if (area->x1 < minimum_flush_x) {
        minimum_flush_x = area->x1;
    }
    lv_disp_flush_ready(driver);
}

static void advance(unsigned milliseconds)
{
    for (unsigned i = 0; i < milliseconds; i++) {
        lv_tick_inc(1);
        lv_timer_handler();
    }
    lv_refr_now(NULL);
}

static void capture(const char *name)
{
    FILE *file = fopen(name, "wb");
    assert(file != NULL);
    fprintf(file, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        lv_color32_t pixel;
        pixel.full = lv_color_to32(framebuffer[i]);
        unsigned char rgb[] = {pixel.ch.red, pixel.ch.green, pixel.ch.blue};
        if (fwrite(rgb, 1, sizeof(rgb), file) != sizeof(rgb)) {
            abort();
        }
    }
    if (fclose(file) != 0) {
        abort();
    }
}

int main(void)
{
    lv_init();
    static lv_color_t buffer[WIDTH * 40];
    static lv_disp_draw_buf_t draw_buffer;
    lv_disp_draw_buf_init(&draw_buffer, buffer, NULL, WIDTH * 40);
    static lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = WIDTH; driver.ver_res = HEIGHT;
    driver.flush_cb = flush; driver.draw_buf = &draw_buffer;
    lv_disp_drv_register(&driver);
    clock_ui_init(WIDTH, HEIGHT);
    clock_ui_update(NULL, false, true, "en_US");
    advance(500);
    capture("waiting.ppm");
    assert(framebuffer[0].full == lv_color_hex(0x0B0B0C).full);
    struct tm local = {.tm_year=126, .tm_mon=10, .tm_mday=20, .tm_wday=5,
                       .tm_hour=20, .tm_min=11, .tm_sec=13};
    clock_ui_status_update(4, "OVERCAST", -55);
    clock_ui_update(&local, true, true, "en_US");
    advance(500);
    capture("clock-digital.ppm");
    unsigned settled = flush_count;
    clock_ui_update(&local, true, true, "en_US");
    advance(100);
    assert(flush_count == settled);
    printf("PASS: unchanged time causes no redraw\n");
    lv_color_t orange = lv_color_hex(0xFF6A2E);
    assert(framebuffer[250 * WIDTH + 565].full == orange.full);
    for (int i=0; i<60; ++i) {
        int x=57+i*864/60;
        assert((framebuffer[543*WIDTH+x].full == orange.full) == (i<13));
    }
    local.tm_sec=14;
    clock_ui_update(&local, true, true, "en_US");
    advance(100);
    assert(framebuffer[543*WIDTH+57+13*864/60].full == orange.full);
    assert(framebuffer[250*WIDTH+565].full == orange.full);
    capture("seconds-14.ppm");
    local.tm_sec=59;
    clock_ui_update(&local, true, true, "en_US");
    advance(100);
    capture("seconds-59.ppm");
    local.tm_sec=0;
    local.tm_min=12;
    clock_ui_update(&local, true, true, "en_US");
    lv_refr_now(NULL);
    unsigned minute_settled = flush_count;
    advance(400);
    assert(flush_count == minute_settled);
    printf("PASS: minute digits switch immediately with no delayed animation\n");
    local.tm_hour=21;
    clock_ui_update(&local, true, true, "en_US");
    advance(85); capture("flip-085.ppm");
    unsigned hour_frames = flush_count;
    advance(90); capture("flip-175.ppm");
    assert(flush_count > hour_frames);
    advance(85); capture("flip-260.ppm");
    advance(140);
    printf("PASS: hour digits retain the flip animation\n");
    for (int i=0; i<60; ++i)
        assert(framebuffer[543*WIDTH+57+i*864/60].full != orange.full);
    capture("seconds-reset.ppm");
    printf("PASS: seconds grow from 13 to 14 to 59 and reset at minute rollover\n");
    printf("PASS: orange colon remains visible and unchanged\n");
    local.tm_hour=23; local.tm_min=59; local.tm_sec=59;
    clock_ui_update(&local, true, true, "en_US"); advance(500);
    capture("before-midnight.ppm");
    local.tm_hour=0; local.tm_min=0; local.tm_sec=0; local.tm_mday=21; local.tm_wday=6;
    clock_ui_update(&local, true, true, "uk_UA"); advance(500);
    capture("midnight-uk.ppm");
    clock_ui_update(&local, true, false, "en_US"); advance(500);
    capture("midnight-12h-en.ppm");
    printf("PASS: midnight and both time formats render\n");
    return 0;
}
