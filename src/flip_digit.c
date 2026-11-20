#include "flip_digit.h"
#include <math.h>
#include <stdlib.h>
#include "fonts/clock_fonts.h"

#if LV_COLOR_DEPTH != 16 || !LV_MEM_CUSTOM || !LV_FONT_FMT_TXT_LARGE
#error "The clock requires RGB565, system heap and large font descriptors"
#endif

enum { MAX_WIDTH = 256, HEIGHT = 340, HALF = HEIGHT / 2, FLIP_MS = 350 };
struct flip_digit { lv_obj_t *canvas; lv_color_t *pixels; char current, target; int width; };
static lv_color_t *atlas[11];
static int widths[11];
static int index_of(char value) { return value == '-' ? 10 : value - '0'; }

static void prepare_atlas(lv_obj_t *parent)
{
    lv_obj_t *scratch = lv_canvas_create(parent);
    lv_obj_add_flag(scratch, LV_OBJ_FLAG_HIDDEN);
    lv_draw_label_dsc_t text;
    lv_draw_label_dsc_init(&text);
    text.font = &clock_digits_448;
    text.color = lv_color_hex(0xECE8E0);
    text.align = LV_TEXT_ALIGN_CENTER;
    for (int i = 0; i < 11; ++i) {
        char symbol[] = {(char)(i == 10 ? '-' : '0' + i), 0};
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(text.font, &glyph, symbol[0], 0)) abort();
        widths[i] = glyph.adv_w;
        if (widths[i] > MAX_WIDTH || widths[i] <= 0) abort();
        atlas[i] = lv_mem_alloc(widths[i] * HEIGHT * sizeof(lv_color_t));
        if (atlas[i] == NULL) abort();
        lv_canvas_set_buffer(scratch, atlas[i], widths[i], HEIGHT, LV_IMG_CF_TRUE_COLOR);
        lv_canvas_fill_bg(scratch, lv_color_hex(0x0B0B0C), LV_OPA_COVER);
        lv_canvas_draw_text(scratch, 0, (HEIGHT - text.font->line_height) / 2,
                            widths[i], &text, symbol);
    }
    lv_obj_del(scratch);
}

static void render(flip_digit_t *digit, int milliseconds)
{
    int old = index_of(digit->current), next = index_of(digit->target);
    bool upper = milliseconds < FLIP_MS / 2;
    float fraction = upper ? milliseconds / 175.0f : (milliseconds - 175) / 175.0f;
    float scale = upper ? cosf(fraction * 1.5707963268f) : sinf(fraction * 1.5707963268f);
    int height = milliseconds < FLIP_MS ? (int)lroundf(HALF * scale) : 0;
    int start = upper ? HALF - height : HALF;
    for (int y = 0; y < HEIGHT; ++y) {
        int source = milliseconds >= FLIP_MS || y < HALF ? next : old;
        int row = y;
        lv_opa_t shade = 0;
        if (milliseconds < FLIP_MS && y >= start && y < start + height) {
            source = upper ? old : next;
            row = (upper ? 0 : HALF) + (y - start) * HALF / height;
            shade = (lv_opa_t)((1.0f - scale) * 90);
        }
        for (int x = 0; x < digit->width; ++x) {
            lv_color_t color = atlas[source][row * widths[source] + x * widths[source] / digit->width];
            digit->pixels[y * digit->width + x] = lv_color_mix(lv_color_black(), color, shade);
        }
    }
    lv_obj_invalidate(digit->canvas);
}
static void progress(void *variable, int32_t milliseconds) { render(variable, milliseconds); }
static void finished(lv_anim_t *animation)
{
    flip_digit_t *digit = animation->var;
    digit->current = digit->target;
}
int flip_digit_width(char value) { return widths[index_of(value)]; }
void flip_digit_position(flip_digit_t *digit, int x, int y) { lv_obj_set_pos(digit->canvas, x, y); }

flip_digit_t *flip_digit_create(lv_obj_t *parent, const lv_area_t *bounds)
{
    if (atlas[0] == NULL) prepare_atlas(parent);
    flip_digit_t *digit = lv_mem_alloc(sizeof(*digit));
    if (digit == NULL) abort();
    *digit = (flip_digit_t){.current = '-', .target = '-', .width = widths[10]};
    digit->pixels = lv_mem_alloc(MAX_WIDTH * HEIGHT * sizeof(lv_color_t));
    if (digit->pixels == NULL) abort();
    digit->canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(digit->canvas, digit->pixels, digit->width, HEIGHT, LV_IMG_CF_TRUE_COLOR);
    flip_digit_position(digit, bounds->x1, bounds->y1);
    render(digit, FLIP_MS);
    return digit;
}
void flip_digit_set(flip_digit_t *digit, char value, bool animate)
{
    if (digit->target == value) return;
    lv_anim_del(digit, progress);
    digit->current = digit->target;
    digit->target = value;
    digit->width = widths[index_of(value)];
    lv_canvas_set_buffer(digit->canvas, digit->pixels, digit->width, HEIGHT, LV_IMG_CF_TRUE_COLOR);
    if (!animate) { digit->current = value; render(digit, FLIP_MS); return; }
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, digit);
    lv_anim_set_exec_cb(&animation, progress);
    lv_anim_set_ready_cb(&animation, finished);
    lv_anim_set_values(&animation, 0, FLIP_MS);
    lv_anim_set_time(&animation, FLIP_MS);
    lv_anim_start(&animation);
}
