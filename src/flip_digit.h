#pragma once
#include "lvgl.h"

typedef struct flip_digit flip_digit_t;

flip_digit_t *flip_digit_create(lv_obj_t *parent, const lv_area_t *bounds);
void flip_digit_set(flip_digit_t *digit, char value, bool animate);
int flip_digit_width(char value);
void flip_digit_position(flip_digit_t *digit, int x, int y);
