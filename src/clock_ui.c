#include "clock_ui.h"
#include <stdio.h>
#include <string.h>
#include "flip_digit.h"
#include "fonts/clock_fonts.h"
#include "lvgl.h"

static flip_digit_t *digits[4];
static lv_obj_t *weekday_label, *date_label, *weather_label, *seconds_label;
static lv_obj_t *ticks[60], *signal_bars[3];
static bool previously_valid;
static int previous_second = -1, offset_x, offset_y;
static char previous_date[96];
static const char *const weekdays[3][7] = {
    {"ВОСКРЕСЕНЬЕ", "ПОНЕДЕЛЬНИК", "ВТОРНИК", "СРЕДА", "ЧЕТВЕРГ", "ПЯТНИЦА", "СУББОТА"},
    {"НЕДІЛЯ", "ПОНЕДІЛОК", "ВІВТОРОК", "СЕРЕДА", "ЧЕТВЕР", "П'ЯТНИЦЯ", "СУБОТА"},
    {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"},
};
static const char *const months[3][12] = {
    {"ЯНВ", "ФЕВ", "МАР", "АПР", "МАЙ", "ИЮН", "ИЮЛ", "АВГ", "СЕН", "ОКТ", "НОЯ", "ДЕК"},
    {"СІЧ", "ЛЮТ", "БЕР", "КВІ", "ТРА", "ЧЕР", "ЛИП", "СЕР", "ВЕР", "ЖОВ", "ЛИС", "ГРУ"},
    {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"},
};
static lv_obj_t *rectangle(lv_obj_t *parent, int x, int y, int width, int height, unsigned color)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x + offset_x, y + offset_y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return object;
}
static lv_obj_t *label(lv_obj_t *parent, int x, int y, unsigned color)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, "");
    lv_obj_set_pos(object, x + offset_x, y + offset_y);
    lv_obj_set_style_text_font(object, &clock_date_32, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    return object;
}
void clock_ui_init(int width, int height)
{
    offset_x = (width - 1024) / 2;
    offset_y = (height - 600) / 2;
    lv_obj_t *screen = lv_scr_act();
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0B0B0C), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 4; ++i) {
        lv_area_t bounds = {.x1 = 0, .y1 = 146 + offset_y};
        digits[i] = flip_digit_create(screen, &bounds);
    }
    rectangle(screen, 558, 242, 29, 40, 0xFF6A2E);
    rectangle(screen, 558, 371, 29, 40, 0xFF6A2E);
    weekday_label = label(screen, 56, 44, 0xECE8E0);
    lv_obj_set_style_text_letter_space(weekday_label, 4, 0);
    date_label = label(screen, 173, 44, 0x7C7A76);
    lv_obj_set_style_text_letter_space(date_label, 4, 0);
    weather_label = label(screen, 700, 44, 0x7C7A76);
    for (int i = 0; i < 3; ++i)
        signal_bars[i] = rectangle(screen, 950 + i * 7, 58 - i * 5, 4, 7 + i * 5, 0x29292B);
    for (int i = 0; i < 60; ++i) {
        int h = i % 5 == 0 ? 20 : 11;
        ticks[i] = rectangle(screen, 56 + i * 864 / 60, 548 - h, 3, h, 0x29292B);
        lv_obj_set_style_radius(ticks[i], 1, 0);
    }
    seconds_label = label(screen, 945, 524, 0xFF6A2E);
    lv_label_set_text(seconds_label, "--");
    clock_ui_status_update(0, NULL, -127);
}
void clock_ui_status_update(int temperature, const char *condition, int rssi)
{
    char value[80];
    if (condition != NULL) snprintf(value, sizeof(value), "%d° %s", temperature, condition);
    else snprintf(value, sizeof(value), "--°");
    if (strcmp(lv_label_get_text(weather_label), value) != 0) {
        lv_label_set_text(weather_label, value);
        lv_obj_align(weather_label, LV_ALIGN_TOP_RIGHT, -92 + offset_x, 44 + offset_y);
    }
    int level = rssi <= -127 ? 0 : rssi < -75 ? 1 : rssi < -60 ? 2 : 3;
    for (int i = 0; i < 3; ++i)
        lv_obj_set_style_bg_color(signal_bars[i], lv_color_hex(i < level ? 0x7C7A76 : 0x29292B), 0);
}
void clock_ui_update(const struct tm *local_time, bool valid, bool format_24h, const char *locale)
{
    char value[5] = "----", date[96] = "";
    int second = valid ? local_time->tm_sec : -1;
    if (valid) {
        int hour = local_time->tm_hour;
        if (!format_24h) { hour %= 12; if (hour == 0) hour = 12; }
        value[0] = '0' + hour / 10; value[1] = '0' + hour % 10;
        value[2] = '0' + local_time->tm_min / 10; value[3] = '0' + local_time->tm_min % 10;
        int language = strcmp(locale, "ru_RU") == 0 ? 0 : strcmp(locale, "uk_UA") == 0 ? 1 : 2;
        snprintf(date, sizeof(date), "%s %d %s", weekdays[language][local_time->tm_wday],
                 local_time->tm_mday, months[language][local_time->tm_mon]);
        if (strcmp(date, previous_date) != 0) {
            lv_label_set_text(weekday_label, weekdays[language][local_time->tm_wday]);
            char day[32];
            snprintf(day, sizeof(day), "%d %s", local_time->tm_mday, months[language][local_time->tm_mon]);
            lv_label_set_text(date_label, day);
            lv_obj_align_to(date_label, weekday_label, LV_ALIGN_OUT_RIGHT_MID, 24, 0);
        }
    } else if (previously_valid) {
        lv_label_set_text(weekday_label, ""); lv_label_set_text(date_label, "");
    }
    int x = 530 - flip_digit_width(value[0]) - flip_digit_width(value[1]);
    for (int i = 0; i < 4; ++i) {
        if (i == 2) x = 613;
        flip_digit_position(digits[i], x + offset_x, 146 + offset_y);
        flip_digit_set(digits[i], value[i], valid && previously_valid && i < 2);
        x += flip_digit_width(value[i]) + (i == 2 ? 13 : 0);
    }
    if (second != previous_second) {
        char text[4] = "--";
        if (valid) snprintf(text, sizeof(text), "%02d", second);
        lv_label_set_text(seconds_label, text);
        for (int i = 0; i < 60; ++i)
            if ((i < previous_second) != (i < second))
                lv_obj_set_style_bg_color(ticks[i], lv_color_hex(i < second ? 0xFF6A2E : 0x29292B), 0);
        previous_second = second;
    }
    memcpy(previous_date, date, strlen(date) + 1);
    previously_valid = valid;
}
