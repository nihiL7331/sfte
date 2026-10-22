#pragma once

#include "src/grid.h"
#include "src/types.h"
#include <string.h>

#if SFTE_IMG_SIXEL

#define _SFTE_IMG_SIXEL_OFFSET 63         // ASCII offset ('?' is 63)
#define _SFTE_IMG_SIXEL_BAND_HEIGHT 6     // Each sixel row represents 6 vertical pixels
#define _SFTE_IMG_SIXEL_MAX_PARAMS 5      // Max parameters in a color definition
#define _SFTE_IMG_SIXEL_RGB_MAX 100       // RGB values are set as percentages
#define _SFTE_IMG_SIXEL_COLORSPACE_HLS 1  // HLS color space identifier
#define _SFTE_IMG_SIXEL_COLORSPACE_RGB 2  // RGB color space identifier

/*
    Finalizes a completed sixel sequence.

    NOTE:
    Sixel requires the text cursor to be pushed to the beginning of next line
    after image finishes drawing, scrolling the screen if necessary.
*/
static inline void _sfte_sixel_commit(sfte_sixel_state *sixel, sfte_term *term) {
    const sfte_modes_state *modes = &term->modes;
    const sfte_viewport_state *vp = &term->viewport;
    sfte_image_state *images = &term->images;
    sfte_cursor_state *cursor = &term->cursor;

    if (sixel->width > 0 && sixel->height > 0) {
        // Slide each row backward to eliminate empty capacity gaps
        for (int32_t y = 0; y < sixel->height; ++y)
            memmove(&sixel->pixels[y * sixel->width], &sixel->pixels[y * sixel->cap_w],
                    sixel->width * sizeof(uint32_t));

        size_t final_bytes = sixel->width * sixel->height * sizeof(uint32_t);
        uint32_t *packed_pixels = (uint32_t *)SFTE_REALLOC(sixel->pixels, final_bytes);
        if (!packed_pixels) packed_pixels = sixel->pixels;

        sfte_img *img = _sfte_img_pool_insert(images, (sfte_img){
                                                          .pixels = packed_pixels,
                                                          .id = ++images->next_id,
                                                          .width = sixel->width,
                                                          .height = sixel->height,
                                                          .is_sixel = 1,
                                                      });
        if (img) {
            sfte_img_placement *p = _sfte_img_placement_insert(images,
                                                               (sfte_img_placement){
                                                                   .img_id = img->id,
                                                                   .start_col = sixel->start_col,
                                                                   .start_row = sixel->start_row,
                                                                   .z_idx = 1,
                                                                   .is_sixel = 1,
#if SFTE_TERM_ALT_SCREEN
                                                                   .alt_screen = modes->alt_active,
#endif  // SFTE_TERM_ALT_SCREEN
                                                               });
            if (p) {
                img->ref_cnt++;

                int16_t rows = _sfte_grid_span(sixel->height, p->y_off, vp->cell_height);
                if (sixel->height % vp->cell_height != 0) rows++;

                // Force the text cursor below the placed image
                cursor->col = 0;
                cursor->row = sixel->start_row + rows;

                // If the image pushed the cursor off the screen, scroll down
                while (cursor->row > vp->scroll_bot) {
                    _sfte_grid_scroll(term, 1);
                    cursor->row--;
                }
            } else
                SFTE_FREE(packed_pixels);
        } else
            SFTE_FREE(packed_pixels);
    } else if (sixel->pixels)
        SFTE_FREE(sixel->pixels);

    sixel->pixels = NULL;
    sixel->cap_w = 0;
    sixel->cap_h = 0;
    sixel->width = 0;
    sixel->height = 0;
}

/*
    Ensures the image buffer is large enough for the incoming pixels.
    Size gets doubled on each dimension when the buffer is not big enough.
    Maxes out at `SFTE_IMG_SIXEL_MAX_SIZE` x `SFTE_IMG_SIXEL_MAX_SIZE`.
*/
static inline void _sfte_sixel_ensure_cap(sfte_sixel_state *sixel, int32_t req_w, int32_t req_h) {
    if (req_w < sixel->cap_w && req_h < sixel->cap_h) return;

    int32_t new_w = sixel->cap_w == 0 ? SFTE_IMG_SIXEL_INIT_SIZE : sixel->cap_w;
    int32_t new_h = sixel->cap_h == 0 ? SFTE_IMG_SIXEL_INIT_SIZE : sixel->cap_h;

    while (new_w >= req_w && new_w < SFTE_IMG_SIXEL_MAX_SIZE) new_w *= 2;
    while (new_h >= req_h && new_h < SFTE_IMG_SIXEL_MAX_SIZE) new_h *= 2;

    if (new_w > SFTE_IMG_SIXEL_MAX_SIZE) new_w = SFTE_IMG_SIXEL_MAX_SIZE;
    if (new_h > SFTE_IMG_SIXEL_MAX_SIZE) new_h = SFTE_IMG_SIXEL_MAX_SIZE;

    if (new_w <= sixel->cap_w && new_h <= sixel->cap_h) return;

    uint32_t *new_pixels = (uint32_t *)SFTE_CALLOC(new_w * new_h, sizeof(uint32_t));
    SFTE_ASSERT(new_pixels, "failed to allocate sixel buffer");

    if (sixel->pixels) {
        for (int32_t r = 0; r < sixel->height; ++r)
            memcpy(&new_pixels[r * new_w], &sixel->pixels[r * sixel->cap_w],
                   sixel->width * sizeof(uint32_t));
        SFTE_FREE(sixel->pixels);
    }

    sixel->pixels = new_pixels;
    sixel->cap_w = new_w;
    sixel->cap_h = new_h;
}

/*
    Decodes a 6-bit bitmask and paints it onto the image buffer.
    Handles buffer resizing if it's too small.

    A sixel is a vertical column of 6 pixels encoded into a single ASCII character.
    The bits (from LSB to MSB) represent pixels from top to bottom.
    ASCII '?' (value 63) minus offset 63 = 000000 (all blank).
    ASCII '~' (value 126) minus offset 63 = 111111 (all solid).
*/
static inline void _sfte_sixel_draw_pattern(sfte_sixel_state *sixel, uint8_t pattern,
                                            int32_t repeats) {
    int32_t max_x = sixel->x + repeats - 1;
    int32_t max_y = sixel->y + _SFTE_IMG_SIXEL_BAND_HEIGHT - 1;

    _sfte_sixel_ensure_cap(sixel, max_x, max_y);
    uint32_t col = sixel->palette[sixel->col_idx];

    for (int32_t dx = 0; dx < repeats; ++dx) {
        int32_t px_x = sixel->x + dx;

        for (uint8_t bit = 0; bit < _SFTE_IMG_SIXEL_BAND_HEIGHT; ++bit) {
            if (!(pattern & (1 << bit))) continue;
            int32_t px_y = sixel->y + bit;

            // Skip invalid sequence requests
            if (px_x >= sixel->cap_w || px_y >= sixel->cap_h) continue;

            sixel->pixels[px_y * sixel->cap_w + px_x] = col;

            if (px_x >= sixel->width) sixel->width = px_x + 1;
            if (px_y >= sixel->height) sixel->height = px_y + 1;
        }
    }

    sixel->x += repeats;
}

/*
    Converts a hue angle into a single RGB channel.
*/
static inline float _sfte_sixel_hue_to_rgb(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

/*
    Converts HLS values to a packed 32-bit ARGB color.
    Unlike modern HSL, sixel uses integer degrees for hue (0-360) and percentages for L/S
   (0-100).
*/
static inline uint32_t _sfte_sixel_hls_to_rgb(uint16_t h_deg, uint16_t l_pct, uint16_t s_pct) {
    float h = h_deg / 360.0f;
    float l = l_pct / 100.0f;
    float s = s_pct / 100.0f;

    uint8_t r, g, b;

    if (s == 0.0f)
        r = g = b = (uint8_t)(l * 255.0f);
    else {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        r = (uint8_t)(_sfte_sixel_hue_to_rgb(p, q, h + 1.0f / 3.0f) * 255.0f);
        g = (uint8_t)(_sfte_sixel_hue_to_rgb(p, q, h) * 255.0f);
        b = (uint8_t)(_sfte_sixel_hue_to_rgb(p, q, h - 1.0f / 3.0f) * 255.0f);
    }

    return SFTE_COLOR_ALPHA_MASK | (r << 16) | (g << 8) | b;
}

/*
    Parses the accumulated numerical parameters to update the active color or palette.
    Initiated by the pound symbol (`#`).
    If one parameter is provided, the active color index gets changed to value of that
   parameter. If five parameters are provided, it defines a new color. Parameter values are as
   follows: #<idx>;<space>;<c1>;<c2>;<c3>. Color space is 1 for HLS, 2 for RGB.
*/
static inline void _sfte_sixel_apply_color(sfte_sixel_state *sixel) {
    if (sixel->param_idx == 0 && sixel->params[0] < 256)
        sixel->col_idx = sixel->params[0];
    else if (sixel->param_idx == 4) {
        int idx = sixel->params[0];
        int space = sixel->params[1];

        if (idx >= 0 && idx <= 256) {
            if (space == _SFTE_IMG_SIXEL_COLORSPACE_HLS)
                sixel->palette[idx] = _sfte_sixel_hls_to_rgb(sixel->params[2], sixel->params[3],
                                                             sixel->params[4]);
            else if (space == _SFTE_IMG_SIXEL_COLORSPACE_RGB) {
                uint8_t r = (sixel->params[2] * 255) / _SFTE_IMG_SIXEL_RGB_MAX;
                uint8_t g = (sixel->params[3] * 255) / _SFTE_IMG_SIXEL_RGB_MAX;
                uint8_t b = (sixel->params[4] * 255) / _SFTE_IMG_SIXEL_RGB_MAX;
                sixel->palette[idx] = SFTE_COLOR_ALPHA_MASK | (r << 16) | (g << 8) | b;
            }
        }
    }
}

/*
    The core state machine for the sixel byte stream.

    When parsing repeats (e.g. `!255`) or colors (e.g. `#1;2;100;0;0`), the terminal
    receives ASCII digits one at a time. To convert these characters into integers
    without external buffers, we use:
        `acc = acc * 10 + (b - '0')`
    `b - '0'` subtracts 48 to convert an ASCII character (e.g. `5`) to an integer (5).
    `acc * 10` shifts the previously parsed value one decimal place to the left.

    To avoid growing the call stack via recursion when a state
    needs to hand a byte back to SIXEL_GROUND, it uses a while-loop based on the `cont`
   variable.
*/
static inline void _sfte_sixel_parse_byte(sfte_sixel_state *sixel, uint8_t b) {
    uint8_t cont = 1;
    while (cont) {
        cont = 0;

        switch (sixel->state) {
        case SIXEL_GROUND:
            if (b >= '?' && b <= '~') {
                uint8_t pattern = b - _SFTE_IMG_SIXEL_OFFSET;
                int32_t repeats = sixel->repeat_cnt > 0 ? sixel->repeat_cnt : 1;
                _sfte_sixel_draw_pattern(sixel, pattern, repeats);
                sixel->repeat_cnt = 0;
            } else if (b == '$')  // Carriage return
                sixel->x = 0;
            else if (b == '-') {  // Move down one band
                sixel->x = 0;
                sixel->y += _SFTE_IMG_SIXEL_BAND_HEIGHT;
            } else if (b == '!') {  // Start repeat sequence
                sixel->state = SIXEL_REPEAT;
                sixel->repeat_cnt = 0;
            } else if (b == '#') {  // Start color definition/selection
                sixel->state = SIXEL_COLOR_PARAM;
                sixel->param_idx = 0;
                memset(sixel->params, 0, sizeof(sixel->params));
            }
            break;
        case SIXEL_REPEAT:
            if (b >= '0' && b <= '9')
                sixel->repeat_cnt = sixel->repeat_cnt * 10 + (b - '0');
            else {
                // Done parsing repeat count, go back to ground state
                sixel->state = SIXEL_GROUND;
                cont = 1;
            }
            break;
        case SIXEL_COLOR_INTRO:
        case SIXEL_COLOR_PARAM:
            if (b >= '0' && b <= '9')
                sixel->params[sixel->param_idx] = sixel->params[sixel->param_idx] * 10 + (b - '0');
            else if (b == ';' && sixel->param_idx < _SFTE_IMG_SIXEL_MAX_PARAMS - 1)
                // Move to next parameter
                sixel->param_idx++;
            else {  // Color sequence terminated by any non-digit/semicolon byte
                _sfte_sixel_apply_color(sixel);
                sixel->state = SIXEL_GROUND;
                cont = 1;
            }
            break;
        }
    }
}

/*
    Frees all sixel memory allocations.
*/
static inline void _sfte_sixel_deinit(sfte_sixel_state *sixel) {
    SFTE_FREE(sixel->pixels);
    sixel->pixels = NULL;
}
#endif  // SFTE_IMG_SIXEL
