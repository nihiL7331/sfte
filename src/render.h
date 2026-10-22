#pragma once

#include "src/font.h"
#include "src/grid.h"
#include "src/input.h"
#include "src/mux.h"
#include "src/search.h"
#include "src/types.h"
#include <stdint.h>

#if SFTE_CURSOR_DYNAMIC
#define _SFTE_CUR_STYLE(cursor) (cursor->style)
#else
#define _SFTE_CUR_STYLE(cursor) (SFTE_CURSOR_STYLE)
#endif  // !SFTE_CURSOR_DYNAMIC

// =================================================================================================
// >>view
// =================================================================================================

#if SFTE_WINDOW_PAD_X || SFTE_WINDOW_PAD_Y
/*
    Fills the padding regions around the terminal grid with the background color.
*/
static inline void _sfte_view_clear_padding_rects(sfte_ctx *ctx, void *px_buf) {
    const sfte_window_state *win = &ctx->window;
    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);

    int32_t w = win->width;
    int32_t h = win->height;
    int32_t grid_w = vp->cols * vp->cell_width;
    int32_t grid_h = (vp->rows + layout.reserved_rows) * vp->cell_height;
    uint32_t bg = (SFTE_COLOR_BG_OPACITY << 24) | (SFTE_COLOR_BG & ~SFTE_COLOR_ALPHA_MASK);

#if SFTE_WINDOW_PAD_Y
    for (int32_t y = 0; y < SFTE_WINDOW_PAD_Y && y < h; ++y)
        for (int32_t x = 0; x < w; ++x) SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, w, h, bg);

    for (int32_t y = SFTE_WINDOW_PAD_Y + grid_h; y < h; ++y)
        for (int32_t x = 0; x < w; ++x) SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, w, h, bg);
#endif  // SFTE_WINDOW_PAD_Y

#if SFTE_WINDOW_PAD_X
    for (int32_t y = SFTE_WINDOW_PAD_Y; y < SFTE_WINDOW_PAD_Y + grid_h && y < h; ++y) {
        for (int32_t x = 0; x < SFTE_WINDOW_PAD_X && x < w; ++x)
            SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, w, h, bg);
        for (int32_t x = SFTE_WINDOW_PAD_X + grid_w; x < w; ++x)
            SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, w, h, bg);
    }
#endif  // SFTE_WINDOW_PAD_X
}
#endif  // SFTE_WINDOW_PAD_X || SFTE_WINDOW_PAD_Y

// =================================================================================================
// >>render
// =================================================================================================

/*
    Fast integer-based alpha blending.
    Used for cursor trails and antialiased font rendering to avoid slow floating-point math.
*/
static inline uint32_t _sfte_render_blend_argb(uint32_t dst_col, uint32_t src_col, uint8_t src_a) {
    if (src_a == 0) return dst_col;  // no trail
    if (src_a == 255)
        return SFTE_COLOR_ALPHA_MASK | (src_col & ~SFTE_COLOR_ALPHA_MASK);  // solid trail

    uint8_t da = (dst_col >> 24) & 0xFF;
    uint8_t dr = (dst_col >> 16) & 0xFF, dg = (dst_col >> 8) & 0xFF, db = dst_col & 0xFF;
    uint8_t sr = (src_col >> 16) & 0xFF, sg = (src_col >> 8) & 0xFF, sb = src_col & 0xFF;

    uint8_t out_r = (sr * src_a + dr * (255 - src_a)) >> 8;
    uint8_t out_g = (sg * src_a + dg * (255 - src_a)) >> 8;
    uint8_t out_b = (sb * src_a + db * (255 - src_a)) >> 8;
    uint8_t out_a = da + ((src_a * (255 - da)) >> 8);

    return (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

/*
    Safely expands a bounding box to encompass a new dirty region.
*/
static inline void _sfte_render_damage_add(sfte_damage_rect *dmg, int32_t x, int32_t y, int32_t w,
                                           int32_t h) {
    if (w <= 0 || h <= 0) return;

    if (dmg->w <= 0 || dmg->h <= 0) {
        dmg->x = x, dmg->y = y;
        dmg->w = w, dmg->h = h;
        return;
    }

    int32_t r1 = dmg->x + dmg->w;
    int32_t b1 = dmg->y + dmg->h;
    int32_t r2 = x + w;
    int32_t b2 = y + h;

    dmg->x = (dmg->x < x) ? dmg->x : x;
    dmg->y = (dmg->y < y) ? dmg->y : y;

    int32_t max_r = (r1 > r2) ? r1 : r2;
    int32_t max_b = (b1 > b2) ? b1 : b2;

    dmg->w = max_r - dmg->x;
    dmg->h = max_b - dmg->y;
}

/*
    Evaluates cursor movement and font bleed.

    Fonts often spill slightly outside their strict grid cell bounds.
    If we only redraw the specific cell that changed, we slice off the edges of adjacent
   letters. This pass detects damaged cells and intentionally bleeds the dirty flag to adjacent
   rows and columns to guarantee seamless redrawing.
*/
static inline void _sfte_render_propagate_damage(sfte_ctx *ctx, int16_t vis_col, int16_t vis_row) {
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_window_state *win = &ctx->window;
    const sfte_viewport_state *vp = &term->viewport;

    int32_t logical_r_curr = _sfte_grid_vis2log(vp, vis_row);
    int32_t logical_r_last = _sfte_grid_vis2log(vp, term->last_drawn_row);

    if (term->last_drawn_col != vis_col || term->last_drawn_row != vis_row) {
        if (term->last_drawn_col >= 0 && term->last_drawn_col < term->viewport.cols &&
            term->last_drawn_row >= 0 && term->last_drawn_row < term->viewport.rows)
            _sfte_grid_get_cell(term, term->last_drawn_col, logical_r_last)->dirty = 1;

        if (vis_col >= 0 && vis_col < vp->cols && vis_row >= 0 && vis_row < vp->rows)
            _sfte_grid_get_cell(term, vis_col, logical_r_curr)->dirty = 1;

        term->last_drawn_col = vis_col;
        term->last_drawn_row = vis_row;
    }

#if SFTE_FONT_BLEED
    for (int16_t r = 0; r < vp->rows; ++r) {
        int32_t logical_r = _sfte_grid_vis2log(vp, r);
        uint8_t row_has_damage = 0;

        for (int16_t c = 0; c < vp->cols; ++c)
            if (_sfte_grid_get_cell(term, c, logical_r)->dirty) {
                row_has_damage = 1;
                break;
            }

        if (row_has_damage) {
            int16_t logical_r_min = r > 0 ? logical_r - 1 : logical_r;
            int16_t logical_r_max = r < vp->rows - 1 ? logical_r + 1 : logical_r;

            // NOTE:
            // Mark padding as dirty to clear the bleed area.
            // it's not hidden behind an `if (r_min == 0 || r_max == ctx->term.viewport.rows - 1)`,
            // because if damage is at left or right edge, the bleed area will be visible there
            // too.
            win->pad_dirty = 1;

            for (int32_t lr = logical_r_min; lr <= logical_r_max; ++lr)
                for (int16_t c = 0; c < vp->cols; ++c) {
                    sfte_cell *cell = _sfte_grid_get_cell(term, c, lr);
                    if (!cell->dirty) cell->dirty = 2;  // bleed-dirty
                }
        }
    }

    // Normalize bleed-dirty flags back to standard dirty flags
    for (int16_t r = 0; r < vp->rows; ++r) {
        int32_t logical_r = _sfte_grid_vis2log(vp, r);

        for (int16_t c = 0; c < vp->cols; ++c) {
            sfte_cell *cell = _sfte_grid_get_cell(term, c, logical_r);
            if (cell->dirty == 2) cell->dirty = 1;
        }
    }
#endif  // SFTE_FONT_BLEED
}

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
/*
    Sorts active image placements by z index using a fast insertion sort.
*/
static inline void _sfte_render_sort_images(sfte_ctx *ctx) {
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_image_state *images = &term->images;

    for (uint32_t i = 1; i < images->placements_len; ++i) {
        sfte_img_placement key = images->placements[i];
        int32_t j = i - 1;

        while (j >= 0 && images->placements[j].z_idx > key.z_idx) {
            images->placements[j + 1] = images->placements[j];
            j--;
        }

        images->placements[j + 1] = key;
    }
}
/*
    Compositor pass for images.
    Images can be drawn behind text (is_bg_pass = 1) or in front of text (is_bg_pass = 0).
*/
static inline void _sfte_render_images(sfte_ctx *ctx, void *px_buf, uint8_t is_bg_pass,
                                       int32_t base_y_off, uint8_t pad_was_dirty) {
    const sfte_window_state *win = &ctx->window;
    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_modes_state *modes = &term->modes;
    const sfte_viewport_state *vp = &term->viewport;
    sfte_image_state *images = &term->images;

    for (uint32_t i = 0; i < images->placements_len; ++i) {
        sfte_img_placement *p = &images->placements[i];
#if SFTE_TERM_ALT_SCREEN
        if (p->alt_screen != modes->alt_active) continue;
#endif  // SFTE_TERM_ALT_SCREEN

        uint8_t is_bg_img = (p->z_idx < 0);
        if (is_bg_img != is_bg_pass) continue;

        sfte_img *img = _sfte_img_find(images, p->img_id);
        if (!img) continue;

        int32_t base_x = (p->start_col * vp->cell_width) + SFTE_WINDOW_PAD_X + p->x_off;
        int32_t base_y = (p->start_row * vp->cell_height) + SFTE_WINDOW_PAD_Y + p->y_off +
                         base_y_off;

        for (int32_t iy = 0; iy < img->height; ++iy) {
            int32_t out_y = base_y + iy;
            if (out_y < 0 || out_y >= win->height) continue;

            for (int32_t ix = 0; ix < img->width; ++ix) {
                int32_t out_x = base_x + ix;
                if (out_x < 0 || out_x >= win->width) continue;

                uint8_t is_dirty = 0;
                if (out_x < SFTE_WINDOW_PAD_X || out_y < SFTE_WINDOW_PAD_Y ||
                    out_x >= win->width - SFTE_WINDOW_PAD_X ||
                    out_y >= win->height - SFTE_WINDOW_PAD_Y) {
                    is_dirty = pad_was_dirty;
                } else {
                    int16_t grid_c = (out_x - SFTE_WINDOW_PAD_X) / vp->cell_width;
                    int16_t grid_r = (out_y - SFTE_WINDOW_PAD_Y) / vp->cell_height;

                    grid_c = _SFTE_CLAMP(grid_c, 0, vp->cols - 1);
                    grid_r = _SFTE_CLAMP(grid_r, 0, vp->rows - 1);

                    int32_t logical_r = _sfte_grid_vis2log(vp, grid_r);
                    is_dirty = _sfte_grid_get_cell(term, grid_c, logical_r)->dirty;
                }
                if (!is_dirty) continue;

                uint32_t img_pxs = img->pixels[iy * img->width + ix];
                if (!(img_pxs & SFTE_COLOR_ALPHA_MASK)) continue;

                SFTE_COLOR_BLEND_PIXEL(px_buf, out_x, out_y, win->width, img_pxs,
                                       (uint8_t)(img_pxs >> 24));
            }
        }
    }
}
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

#if SFTE_TERM_CUSTOM_BOXES && !SFTE_TERM_ASCII_CHARSET
/*
    A stripped down Xialoin Wu line algorithm optimized for integer endpoints.
    Draws anti-aliased lines for terminal cell diagonals.
*/
static inline void _sfte_render_line(sfte_ctx *ctx, void *px_buf, int32_t x0, int32_t y0,
                                     int32_t x1, int32_t y1, int32_t thickness, uint32_t col) {
    const sfte_window_state *win = &ctx->window;

    if (thickness < 1) thickness = 1;
    uint8_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        int32_t tmp = x0;
        x0 = y0;
        y0 = tmp;
        tmp = x1;
        x1 = y1;
        y1 = tmp;
    }
    if (x0 > x1) {
        int32_t tmp = x0;
        x0 = x1;
        x1 = tmp;
        tmp = y0;
        y0 = y1;
        y1 = tmp;
    }

    float dx = (float)(x1 - x0);
    float dy = (float)(y1 - y0);
    float grad = (dx == 0.0f) ? 1.0f : (dy / dx);
    float intery = y0;

#define _SFTE_SAFE_BLEND(_x, _y, _a)                                                               \
    if ((_x) >= 0 && (_x) < win->width && (_y) >= 0 && (_y) < win->height)                         \
        SFTE_COLOR_BLEND_PIXEL(px_buf, _x, _y, win->width, col, _a);

    for (int32_t x = x0; x <= x1; ++x) {
        float intery_frac = intery - floorf(intery);
        uint8_t a1 = (uint8_t)((1.0f - intery_frac) * 255.0f);
        uint8_t a2 = (uint8_t)(intery_frac * 255.0f);
        int32_t y = (int32_t)intery;

        int32_t bound_min = y - (thickness / 2);
        int32_t bound_max = bound_min + thickness;

        if (steep) {
            _SFTE_SAFE_BLEND(bound_min, x, a1);
            for (int32_t w = 1; w < thickness; ++w) _SFTE_SAFE_BLEND(bound_min + w, x, 255);
            _SFTE_SAFE_BLEND(bound_max, x, a2);
        } else {
            _SFTE_SAFE_BLEND(x, bound_min, a1);
            for (int32_t w = 1; w < thickness; ++w) _SFTE_SAFE_BLEND(x, bound_min + w, 255);
            _SFTE_SAFE_BLEND(x, bound_max, a2);
        }
        intery += grad;
    }

#undef _SFTE_SAFE_BLEND
}

/*
    Renders box characters (0x2500-0x259F) to ensure there's no gaps between characters.
    Returns 0 for unhandled cases, ensuring that they're drawn using font data.
 */
static inline uint8_t _sfte_render_box_char(sfte_ctx *ctx, void *px_buf, int32_t cx, int32_t cy,
                                            uint32_t col, uint32_t rune, int32_t y_off) {
    const sfte_viewport_state *vp = &sfte_term_get_active(ctx)->viewport;
    const sfte_window_state *win = &ctx->window;

    int16_t cw = vp->cell_width;
    int16_t ch = vp->cell_height;

#define _SFTE_RECT(rx, ry, rw, rh)                                                                 \
    for (int16_t y = (ry); y < (ry) + (rh); ++y)                                                   \
        for (int16_t x = (rx); x < (rx) + (rw); ++x)                                               \
    SFTE_COLOR_DRAW_PIXEL(px_buf, x, y + y_off, win->width, win->height, col)

    // Block elements
    if (rune >= 0x2580 && rune <= 0x259F) {
#define _BLOCK_B(f)                                                                                \
    _SFTE_RECT(cx, cy + ch - ((((f) * ch + 4) / 8 > 0) ? (((f) * ch + 4) / 8) : 1), cw,            \
               (((f) * ch + 4) / 8 > 0) ? (((f) * ch + 4) / 8) : 1)
#define _BLOCK_L(f) _SFTE_RECT(cx, cy, (((f) * cw + 4) / 8 > 0) ? (((f) * cw + 4) / 8) : 1, ch)
#define _BLOCK_T(f) _SFTE_RECT(cx, cy, cw, (((f) * ch + 4) / 8 > 0) ? (((f) * ch + 4) / 8) : 1)
#define _BLOCK_R(f)                                                                                \
    _SFTE_RECT(cx + cw - ((((f) * cw + 4) / 8 > 0) ? (((f) * cw + 4) / 8) : 1), cy,                \
               (((f) * cw + 4) / 8 > 0) ? (((f) * cw + 4) / 8) : 1, ch)
#define _QUAD(tl, tr, bl, br)                                                                      \
    do {                                                                                           \
        if (tl) _SFTE_RECT(cx, cy, cw / 2, ch / 2);                                                \
        if (tr) _SFTE_RECT(cx + cw / 2, cy, cw - cw / 2, ch / 2);                                  \
        if (bl) _SFTE_RECT(cx, cy + ch / 2, cw / 2, ch - ch / 2);                                  \
        if (br) _SFTE_RECT(cx + cw / 2, cy + ch / 2, cw - cw / 2, ch - ch / 2);                    \
    } while (0)

        switch (rune) {
        case 0x2580: _BLOCK_T(4); break;        // ▀
        case 0x2581: _BLOCK_B(1); break;        // ▁
        case 0x2582: _BLOCK_B(2); break;        // ▂
        case 0x2583: _BLOCK_B(3); break;        // ▃
        case 0x2584: _BLOCK_B(4); break;        // ▄
        case 0x2585: _BLOCK_B(5); break;        // ▅
        case 0x2586: _BLOCK_B(6); break;        // ▆
        case 0x2587: _BLOCK_B(7); break;        // ▇
        case 0x2588: _BLOCK_B(8); break;        // █
        case 0x2589: _BLOCK_L(7); break;        // ▉
        case 0x258A: _BLOCK_L(6); break;        // ▊
        case 0x258B: _BLOCK_L(5); break;        // ▋
        case 0x258C: _BLOCK_L(4); break;        // ▌
        case 0x258D: _BLOCK_L(3); break;        // ▍
        case 0x258E: _BLOCK_L(2); break;        // ▎
        case 0x258F: _BLOCK_L(1); break;        // ▏
        case 0x2590: _BLOCK_R(4); break;        // ▐
        case 0x2594: _BLOCK_T(1); break;        // ▔
        case 0x2595: _BLOCK_R(1); break;        // ▕
        case 0x2596: _QUAD(0, 0, 1, 0); break;  // ▖
        case 0x2597: _QUAD(0, 0, 0, 1); break;  // ▗
        case 0x2598: _QUAD(1, 0, 0, 0); break;  // ▘
        case 0x2599: _QUAD(1, 0, 1, 1); break;  // ▙
        case 0x259A: _QUAD(1, 0, 0, 1); break;  // ▚
        case 0x259B: _QUAD(1, 1, 1, 0); break;  // ▛
        case 0x259C: _QUAD(1, 1, 0, 1); break;  // ▜
        case 0x259D: _QUAD(0, 1, 0, 0); break;  // ▝
        case 0x259E: _QUAD(0, 1, 1, 0); break;  // ▞
        case 0x259F: _QUAD(0, 1, 1, 1); break;  // ▟
        default: return 0;
        }
        return 1;
    }

    // Braille patterns
    if (rune >= 0x2800 && rune <= 0x28FF) {
        uint8_t dots = rune - 0x2800;
        if (dots == 0) return 1;

        // 2 columns, 4 rows
        int16_t sw = cw / 2;
        int16_t sh = ch / 4;

        int16_t dot_w = (cw / 4 > 0) ? cw / 4 : 1;
        int16_t dot_h = (ch / 8 > 0) ? ch / 8 : 1;

        int16_t ox = (sw - dot_w) / 2;
        int16_t oy = (sh - dot_h) / 2;

#define _DRAW_DOT(bit, col, row)                                                                   \
    if (dots & (bit)) _SFTE_RECT(cx + ((col) * sw) + ox, cy + ((row) * sh) + oy, dot_w, dot_h);

        _DRAW_DOT(0x01, 0, 0);  // ⠁
        _DRAW_DOT(0x02, 0, 1);  // ⠂
        _DRAW_DOT(0x04, 0, 2);  // ⠄
        _DRAW_DOT(0x08, 1, 0);  // ⠈
        _DRAW_DOT(0x10, 1, 1);  // ⠐
        _DRAW_DOT(0x20, 1, 2);  // ⠠
        _DRAW_DOT(0x40, 0, 3);  // ⡀
        _DRAW_DOT(0x80, 1, 3);  // ⢀

#undef _DRAW_DOT
        return 1;
    }

    uint8_t up = 0, down = 0, left = 0, right = 0;
    uint8_t heavy = 0;

#define _SET_LINE(u, d, l, r, h) up = u, down = d, left = l, right = r, heavy = h

    switch (rune) {
    case 0x2500: _SET_LINE(0, 0, 1, 1, 0); break;  // ─
    case 0x2501: _SET_LINE(0, 0, 1, 1, 1); break;  // ━
    case 0x2502: _SET_LINE(1, 1, 0, 0, 0); break;  // │
    case 0x2503: _SET_LINE(1, 1, 0, 0, 1); break;  // ┃
    case 0x250C: _SET_LINE(0, 1, 0, 1, 0); break;  // ┌
    case 0x250F: _SET_LINE(0, 1, 0, 1, 1); break;  // ┏
    case 0x2510: _SET_LINE(0, 1, 1, 0, 0); break;  // ┐
    case 0x2513: _SET_LINE(0, 1, 1, 0, 1); break;  // ┓
    case 0x2514: _SET_LINE(1, 0, 0, 1, 0); break;  // └
    case 0x2517: _SET_LINE(1, 0, 0, 1, 1); break;  // ┗
    case 0x2518: _SET_LINE(1, 0, 1, 0, 0); break;  // ┘
    case 0x251B: _SET_LINE(1, 0, 1, 0, 1); break;  // ┛
    case 0x251C: _SET_LINE(1, 1, 0, 1, 0); break;  // ├
    case 0x2523: _SET_LINE(1, 1, 0, 1, 1); break;  // ┣
    case 0x2524: _SET_LINE(1, 1, 1, 0, 0); break;  // ┤
    case 0x252B: _SET_LINE(1, 1, 1, 0, 1); break;  // ┫
    case 0x252C: _SET_LINE(0, 1, 1, 1, 0); break;  // ┬
    case 0x2533: _SET_LINE(0, 1, 1, 1, 1); break;  // ┳
    case 0x2534: _SET_LINE(1, 0, 1, 1, 0); break;  // ┴
    case 0x253B: _SET_LINE(1, 0, 1, 1, 1); break;  // ┻
    case 0x253C: _SET_LINE(1, 1, 1, 1, 0); break;  // ┼
    case 0x254B: _SET_LINE(1, 1, 1, 1, 1); break;  // ╋
    case 0x2571:                                   // ╱
    case 0x2572:                                   // ╲
    case 0x2573:                                   // ╳
    {
        int16_t b_lw = (cw / 8 > 0) ? cw / 8 : 1;
        if (rune == 0x2571 || rune == 0x2573)
            _sfte_render_line(ctx, px_buf, cx, cy + ch - 1 + y_off, cx + cw - 1, cy + y_off, b_lw,
                              col);
        if (rune == 0x2572 || rune == 0x2573)
            _sfte_render_line(ctx, px_buf, cx, cy + y_off, cx + cw - 1, cy + ch - 1 + y_off, b_lw,
                              col);
        return 1;
    }
    default: return 0;
    }

    int16_t base_lw = (cw / 8 > 0) ? cw / 8 : 1;
    int16_t lw = heavy ? (base_lw * 3) : base_lw;
    int16_t hw = lw / 2;
    int16_t mx = cx + cw / 2;
    int16_t my = cy + ch / 2;

    if (up) _SFTE_RECT(mx - hw, cy, lw, my - cy + hw);
    if (down) _SFTE_RECT(mx - hw, my - hw, lw, cy + ch - my + hw);
    if (left) _SFTE_RECT(cx, my - hw, mx - cx + hw, lw);
    if (right) _SFTE_RECT(mx - hw, my - hw, cx + cw - mx + hw, lw);

#undef _SET_LINE
#undef _SFTE_RECT

    return 1;
}
#endif  // SFTE_TERM_CUSTOM_BOXES && !SFTE_TERM_ASCII_CHARSET

/*
    Samples the font atlas and paints a glyph.
    The texture atlas only stores alpha values.
    It blends the requested foreground color into the existing background using this alpha mask.
*/
static inline void _sfte_render_fg_cell(sfte_ctx *ctx, void *px_buf, int16_t col, int16_t row,
                                        int32_t y_off, sfte_rune rune, uint16_t glyph_id,
                                        uint8_t font_idx, uint32_t fg,
                                        sfte_font_cache *target_cache) {
    if (rune == ' ') return;

    const sfte_font_state *font = &ctx->font;
    const sfte_window_state *win = &ctx->window;
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_stack *stack = &term->stack;
    const sfte_viewport_state *vp = &term->viewport;

    int32_t cx = col * vp->cell_width + SFTE_WINDOW_PAD_X;
    int32_t cy = row * vp->cell_height + SFTE_WINDOW_PAD_Y;

#if SFTE_TERM_CUSTOM_BOXES && !SFTE_TERM_ASCII_CHARSET
    if ((rune >= 0x2500 && rune <= 0x259F) || (rune >= 0x2800 && rune <= 0x28FF))
        if (_sfte_render_box_char(ctx, px_buf, cx, cy, fg, rune, y_off)) return;
#endif  // SFTE_TERM_CUSTOM_BOXES

    sfte_glyph *g = _sfte_font_get_glyph(stack, target_cache, glyph_id, font_idx);
    if (!g) return;

    int32_t glyph_width = g->x1 - g->x0;
    int32_t glyph_height = g->y1 - g->y0;

    int32_t draw_x = cx + (int)g->xoff;
    int32_t draw_y = cy + font->ascent + (int)g->yoff + y_off;

    for (int32_t y = 0; y < glyph_height; ++y) {
        for (int32_t x = 0; x < glyph_width; ++x) {
            int32_t screen_x = draw_x + x;
            int32_t screen_y = draw_y + y;
            if (screen_x < 0 || screen_x >= win->width || screen_y < 0 || screen_y >= win->height)
                continue;

            uint8_t alpha = target_cache
                                ->atlas_pxs[(g->y0 + y) * SFTE_FONT_ATLAS_SIZE + (g->x0 + x)] &
                            0xFF;

            SFTE_COLOR_BLEND_PIXEL(px_buf, screen_x, screen_y, win->width, fg, alpha);
        }
    }
}

#if SFTE_SEARCH
static inline void _sfte_render_search_overlay(sfte_ctx *ctx, void *px_buf,
                                               sfte_damage_rect *out_dmg) {
    const sfte_window_state *win = &ctx->window;
    const sfte_term *term = sfte_term_get_active(ctx);
    const sfte_search_state *search = &term->search;
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);

    if (!search->is_active) return;

    int32_t bar_h = vp->cell_height;
#if SFTE_SEARCH_PLACEMENT_TOP
    int32_t r = 0;
#else   // !SFTE_SEARCH_PLACEMENT_TOP
    int32_t r = vp->rows - 1;
#endif  // !SFTE_SEARCH_PLACEMENT_TOP
    int32_t y_off = layout.grid_off_y;
    int32_t start_y = r * vp->cell_height + SFTE_WINDOW_PAD_Y + y_off;

    char right_buf[64];
    int16_t right_len = snprintf(right_buf, sizeof(right_buf), " [%u/%u] ",
                                 search->match_cnt > 0 ? search->active_match_idx + 1 : 0,
                                 search->match_cnt);

    // Calculate available space for query
    int16_t prefix_len = strlen(SFTE_SEARCH_PREFIX);
    int16_t query_len = search->text_input.len;

    int16_t avail_query_space = vp->cols - prefix_len - right_len - 1;

    char left_buf[SFTE_INPUT_TEXT_MAX_LEN + 64];
    int16_t left_len = 0;

    if (query_len > avail_query_space && avail_query_space >= 3) {  // Query won't fit
        int16_t copy_len = avail_query_space - 3;
        int16_t start_idx = query_len - copy_len;

        // Snap to next valid byte to prevent splitting an UTF-8 character
        while (start_idx < query_len && (search->text_input.buf[start_idx] & 0xC0) == 0x80)
            start_idx++;

        left_len = snprintf(left_buf, sizeof(left_buf), "%s...%s ", SFTE_SEARCH_PREFIX,
                            search->text_input.buf + start_idx);
    } else
        left_len = snprintf(left_buf, sizeof(left_buf), "%s%s ", SFTE_SEARCH_PREFIX,
                            search->text_input.buf);

    int32_t bar_w = vp->cols * vp->cell_width;
    int32_t start_x = SFTE_WINDOW_PAD_X;

    for (int32_t y = 0; y < bar_h; ++y) {
        int32_t py = start_y + y;
        if (py >= win->height) break;

        for (int32_t x = 0; x < bar_w; ++x) {
            int32_t px = start_x + x;
            if (px >= win->width) break;

            uint32_t col_bg = (SFTE_SEARCH_BG_BAR_OPACITY << 24) |
                              (SFTE_SEARCH_BG_BAR & ~SFTE_COLOR_ALPHA_MASK);
            SFTE_COLOR_BLEND_PIXEL(px_buf, px, py, win->width, col_bg, (uint8_t)(col_bg >> 24));
        }
    }

    sfte_font_cache *font_cache = _sfte_font_get_cache(ctx, SFTE_FONT_STYLE_REGULAR);

    for (int16_t c = 0; c < left_len && c < vp->cols; ++c) {
        uint32_t rune = (uint8_t)left_buf[c];

        uint8_t font_idx = 0;
        uint16_t glyph_id = 0;
        _sfte_font_resolve_rune(font_cache, rune, &font_idx, &glyph_id);
        _sfte_render_fg_cell(ctx, px_buf, c, r, y_off, rune, glyph_id, font_idx, SFTE_COLOR_FG,
                             font_cache);
    }

    int16_t right_start_c = vp->cols - right_len;
    if (right_start_c < 0) right_start_c = 0;

    for (int16_t c = 0; c < right_len && (right_start_c + c) < vp->cols; ++c) {
        uint32_t rune = (uint8_t)right_buf[c];

        uint8_t font_idx = 0;
        uint16_t glyph_id = 0;
        _sfte_font_resolve_rune(font_cache, rune, &font_idx, &glyph_id);
        _sfte_render_fg_cell(ctx, px_buf, right_start_c + c, r, y_off, rune, glyph_id, font_idx,
                             SFTE_COLOR_FG, font_cache);
    }

    _sfte_render_damage_add(out_dmg, start_x, start_y, bar_w, bar_h);
}
#endif  // SFTE_SEARCH

#if SFTE_MUX_BAR
static inline void _sfte_render_mux_bar(sfte_ctx *ctx, void *px_buf, sfte_damage_rect *out_dmg) {
    const sfte_window_state *win = &ctx->window;
    sfte_mux *mux = &ctx->mux;
    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);

    if (!term || vp->cell_width == 0) return;

    int32_t bar_y = layout.bar_y;
    int16_t bar_row = (bar_y - SFTE_WINDOW_PAD_Y) / vp->cell_height - 1;

    char labels[SFTE_MUX_MAX_WINDOWS][32];
    uint16_t label_lens[SFTE_MUX_MAX_WINDOWS];
    uint16_t bar_len = 0;

    uint16_t pad_l = 0, pad_r = 0;
#if SFTE_MUX_BAR_EDGE_LEFT
    pad_l++;
#endif  // SFTE_MUX_BAR_EDGE_LEFT
#if SFTE_MUX_BAR_EDGE_RIGHT
    pad_r++;
#endif  // SFTE_MUX_BAR_EDGE_RIGHT

    for (uint8_t pos = 0; pos < mux->tab_cnt; ++pos) {
        uint8_t i = mux->tab_order[pos];

        uint16_t c = 0;
        const char *fmt = SFTE_MUX_BAR_FORMAT;

        while (*fmt && c < sizeof(labels[i]) - 1)
            if (fmt[0] == '%' && fmt[1] == 'u') {
                c += snprintf(labels[i] + c, sizeof(labels[i]) - c, "%u", pos + 1);
                fmt += 2;
            } else if (fmt[0] == '%' && fmt[1] == 's') {
                const char *term_title = mux->terms[i].title;

                if (mux->is_renaming && mux->active_idx == i) {
                    term_title = mux->text_input.buf;

                    mux->rename_cursor_col = c;
                } else if (term_title[0] == '\0')
                    term_title = SFTE_TERM_DEFAULT_TITLE;

                c += snprintf(labels[i] + c, sizeof(labels[i]) - c, "%s", term_title);
                fmt += 2;
            } else
                labels[i][c++] = *fmt++;

        labels[i][c] = '\0';
        label_lens[i] = c;

        bar_len += label_lens[i] + pad_l + pad_r + (SFTE_MUX_BAR_MARGIN * 2) + SFTE_MUX_BAR_PAD;
    }

    if (bar_len > 0) bar_len -= SFTE_MUX_BAR_PAD;

#if SFTE_MUX_BAR_ALIGN == SFTE_MUX_BAR_ALIGN_LEFT
    (void)bar_len;
    uint16_t cur_col = SFTE_MUX_BAR_PAD;
#elif SFTE_MUX_BAR_ALIGN == SFTE_MUX_BAR_ALIGN_CENTER
    uint16_t cur_col = (vp->cols > bar_len) ? (vp->cols - bar_len) / 2 : 0;
#elif SFTE_MUX_BAR_ALIGN == SFTE_MUX_BAR_ALIGN_RIGHT
    uint16_t cur_col = (vp->cols > bar_len + SFTE_MUX_BAR_PAD)
                           ? vp->cols - bar_len - SFTE_MUX_BAR_PAD
                           : 0;
#elif SFTE_MUX_BAR_ALIGN == SFTE_MUX_BAR_ALIGN_FILL
    uint16_t cur_col = 0;
    int32_t base_tab_cols = 0, leftover_cols = 0;
    if (num_tabs > 0) {
        int32_t total_pad = (num_tabs - 1) * SFTE_MUX_BAR_PAD;
        int32_t avail_cols = vp->cols - total_pad;
        if (avail_cols < 0) avail_cols = 0;
        base_tab_cols = avail_cols / mux->tab_cnt;
        leftover_cols = avail_cols % mux->tab_cnt;
    }
#endif  // SFTE_MUX_BAR_ALIGN == SFTE_MUX_BAR_ALIGN_FILL

#if !SFTE_MUX_BAR_HOVER
    // Clear background
    for (int32_t y = bar_y; y < bar_y + vp->cell_height; ++y)
        for (int32_t x = SFTE_WINDOW_PAD_X; x < win->width - SFTE_WINDOW_PAD_X; ++x)
            if (x >= 0 && x < win->width && y >= 0 && y < win->height)
                SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, win->width, win->height,
                                      (SFTE_COLOR_BG_OPACITY) << 24 |
                                          (SFTE_COLOR_BG & ~SFTE_COLOR_ALPHA_MASK));
#endif  // !SFTE_MUX_BAR_HOVER

    // Render windows
    for (uint8_t pos = 0; pos < mux->tab_cnt; ++pos) {
        uint8_t i = mux->tab_order[pos];

#if SFTE_MUX_BAR_ALIGN == SFTE_MUX_BAR_ALIGN_FILL
        // Add 1 extra column to this tab if we still have leftover remainder columns
        int32_t tab_cols = base_tab_cols + (leftover_cols > 0 ? 1 : 0);
        if (leftover_cols > 0) leftover_cols--;
#else   // SFTE_MUX_BAR_ALIGN != SFTE_MUX_BAR_ALIGN_FILL
        int32_t tab_cols = label_lens[i] + pad_l + pad_r + (SFTE_MUX_BAR_MARGIN * 2);
#endif  // SFTE_MUX_BAR_ALIGN != SFTE_MUX_BAR_ALIGN_FILL

        mux->tab_px_x[i] = cur_col * vp->cell_width + SFTE_WINDOW_PAD_X;
        mux->tab_px_w[i] = tab_cols * vp->cell_width;

        uint32_t col_bg = (mux->active_idx == i)    ? SFTE_MUX_BAR_BG_ACTIVE
                          : (mux->hovered_idx == i) ? SFTE_MUX_BAR_BG_HOVER
                                                    : SFTE_MUX_BAR_BG_INACTIVE;  // RGB8888
        uint32_t col_fg = (mux->active_idx == i)    ? SFTE_MUX_BAR_FG_ACTIVE
                          : (mux->hovered_idx == i) ? SFTE_MUX_BAR_FG_HOVER
                                                    : SFTE_MUX_BAR_FG_INACTIVE;  // RGB888
        col_fg = SFTE_COLOR_ALPHA_MASK | (col_fg & ~SFTE_COLOR_ALPHA_MASK);

        sfte_font_style style = SFTE_FONT_STYLE_REGULAR;

        if (mux->active_idx == i) {
#if SFTE_MUX_BAR_ATTR_ACTIVE & SFTE_ATTR_REVERSE
            uint32_t col_tmp = col_bg;
            col_bg = col_fg;
            col_fg = col_tmp;
#endif  // SFTE_MUX_BAR_ATTR_ACTIVE & SFTE_ATTR_REVERSE

#if defined(SFTE_FONT_BOLD_ITALIC) && (SFTE_MUX_BAR_ATTR_ACTIVE & SFTE_ATTR_BOLD & SFTE_ATTR_ITALIC)
            style = SFTE_FONT_STYLE_BOLD_ITALIC;
#elif defined(SFTE_FONT_BOLD) && (SFTE_MUX_BAR_ATTR_ACTIVE & SFTE_ATTR_BOLD)
            style = SFTE_FONT_STYLE_BOLD;
#elif defined(SFTE_FONT_ITALIC) && (SFTE_MUX_BAR_ATTR_ACTIVE & SFTE_ATTR_ITALIC)
            style = SFTE_FONT_STYLE_ITALIC;
#endif  // defined(SFTE_FONT_ITALIC) && (SFTE_MUX_ATTR_ACTIVE & SFTE_ATTR_ITALIC)
        } else {
#if SFTE_MUX_BAR_ATTR_INACTIVE & SFTE_ATTR_REVERSE
            uint32_t col_tmp = col_bg;
            col_bg = col_fg;
            col_fg = col_tmp;
#endif  // SFTE_MUX_BAR_ATTR_INACTIVE & SFTE_ATTR_REVERSE

#if defined(SFTE_FONT_BOLD_ITALIC) &&                                                              \
    (SFTE_MUX_BAR_ATTR_INACTIVE & SFTE_ATTR_BOLD & SFTE_ATTR_ITALIC)
            style = SFTE_FONT_STYLE_BOLD_ITALIC;
#elif defined(SFTE_ATTR_BOLD) && (SFTE_MUX_BAR_ATTR_INACTIVE & SFTE_ATTR_BOLD)
            style = SFTE_FONT_STYLE_BOLD;
#elif defined(SFTE_ATTR_ITALIC) && (SFTE_MUX_BAR_ATTR_INACTIVE & SFTE_ATTR_ITALIC)
            style = SFTE_FONT_STYLE_ITALIC;
#endif  // defined(SFTE_FONT_ITALIC) && (SFTE_MUX_ATTR_INACTIVE & SFTE_ATTR_ITALIC)
        }

        sfte_font_cache *font_cache = _sfte_font_get_cache(ctx, style);

        int32_t bg_start_x = mux->tab_px_x[i] + (SFTE_MUX_BAR_EDGE_LEFT ? vp->cell_width : 0);
        int32_t bg_end_x = mux->tab_px_x[i] + mux->tab_px_w[i] -
                           (SFTE_MUX_BAR_EDGE_RIGHT ? vp->cell_width : 0);

        for (int32_t y = bar_y; y < bar_y + vp->cell_height; ++y)
            for (int32_t x = bg_start_x; x < bg_end_x; ++x)
                if (x >= 0 && x < win->width && y >= 0 && y < win->height)
                    SFTE_COLOR_BLEND_PIXEL(px_buf, x, y, win->width, col_bg,
                                           (uint8_t)(col_bg >> 24));

#if SFTE_MUX_BAR_EDGE_LEFT
        if (tab_cols >= 1) {
            uint8_t font_l_idx = 0;
            uint16_t glyph_l_id = 0;

            _sfte_font_resolve_rune(font_cache, SFTE_MUX_BAR_EDGE_LEFT, &font_l_idx, &glyph_l_id);
            _sfte_render_fg_cell(ctx, px_buf, cur_col, bar_row, vp->cell_height,
                                 SFTE_MUX_BAR_EDGE_LEFT, glyph_l_id, font_l_idx, col_bg,
                                 font_cache);
        }
#endif  // SFTE_MUX_BAR_EDGE_LEFT
#if SFTE_MUX_BAR_EDGE_RIGHT
        if (tab_cols >= 2) {
            uint8_t font_r_idx = 0;
            uint16_t glyph_r_id = 0;

            _sfte_font_resolve_rune(font_cache, SFTE_MUX_BAR_EDGE_RIGHT, &font_r_idx, &glyph_r_id);
            _sfte_render_fg_cell(ctx, px_buf, cur_col + tab_cols - 1, bar_row, vp->cell_height,
                                 SFTE_MUX_BAR_EDGE_RIGHT, glyph_r_id, font_r_idx, col_bg,
                                 font_cache);
        }
#endif  // SFTE_MUX_BAR_EDGE_LEFT

        uint16_t text_col = cur_col;
        if (tab_cols > label_lens[i]) text_col += (tab_cols - label_lens[i]) / 2;

        if (mux->is_renaming && mux->active_idx == i)
            mux->rename_cursor_col += text_col + mux->text_input.cursor_byte_idx;

        for (uint16_t c = 0; c < label_lens[i]; ++c) {
            uint32_t rune = (uint8_t)labels[i][c];

            uint8_t font_idx = 0;
            uint16_t glyph_id = 0;
            _sfte_font_resolve_rune(font_cache, rune, &font_idx, &glyph_id);
            _sfte_render_fg_cell(ctx, px_buf, text_col + c, bar_row, vp->cell_height, rune,
                                 glyph_id, font_idx, col_fg, font_cache);
        }

        cur_col += tab_cols + SFTE_MUX_BAR_PAD;
    }

    _sfte_render_damage_add(out_dmg, 0, bar_y, win->width, vp->cell_height);
}
#endif  // SFTE_MUX_BAR

#if SFTE_MUX_PREFIX_PREVIEW
static inline void _sfte_render_mux_preview(sfte_ctx *ctx, void *px_buf,
                                            sfte_damage_rect *out_dmg) {
    if (mux->tab_cnt == 0) return;

    const sfte_window_state *win = &ctx->window;
    sfte_mux *mux = &ctx->mux;
    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_viewport_state *vp = &term->viewport;

    for (int32_t y = 0; y < win->height; ++y)
        for (int32_t x = 0; x < win->width; ++x)
            SFTE_COLOR_BLEND_PIXEL(px_buf, x, y, win->width,
                                   SFTE_MUX_PREVIEW_BG & ~SFTE_COLOR_ALPHA_MASK,
                                   (uint8_t)((SFTE_MUX_PREVIEW_BG & SFTE_COLOR_ALPHA_MASK) >> 24));
    _sfte_render_damage_add(out_dmg, 0, 0, win->width, win->height);

    int16_t modal_cols = vp->cols - SFTE_MUX_PREVIEW_MARGIN * 2;
    if (modal_cols < 0) modal_cols = vp->cols;
    int16_t start_c = (vp->cols - modal_cols) / 2;
    int16_t end_c = start_c + modal_cols - 1;

    int16_t avail_rows = vp->rows - (mux->tab_cnt - 1) - 2;  // -2 for top/bot borders
    if (avail_rows < mux->tab_cnt) return;
    int16_t preview_rows = avail_rows / mux->tab_cnt;

    // Window views + separators + top/bot borders
    int16_t modal_rows = (preview_rows * mux->tab_cnt) + (mux->tab_cnt - 1) + 2;

    int16_t start_r = (vp->rows - modal_rows) / 2;

    sfte_font_cache *font = _sfte_font_get_cache(ctx, SFTE_FONT_STYLE_REGULAR);

#define DRAW_BORDER(_c, _r, _rune)                                                                 \
    do {                                                                                           \
        _sfte_render_bg_cell(ctx, px_buf, _c, _r, 0, SFTE_COLOR_BG);                               \
        uint8_t _font_idx = 0;                                                                     \
        uint16_t _glyph_id = 0;                                                                    \
        _sfte_font_resolve_rune(font, _rune, &_font_idx, &_glyph_id);                              \
        _sfte_render_fg_cell(ctx, px_buf, _c, _r, 0, _rune, _glyph_id, _font_idx,                  \
                             SFTE_MUX_PREVIEW_BORDER_FG, font);                                    \
    } while (0)

    int16_t cur_r = start_r;
    uint8_t real_active_idx = mux->active_idx;

    // Top border rendering
    for (int16_t c = start_c; c <= end_c; ++c) {
        sfte_rune rune = (c == start_c) ? SFTE_MUX_PREVIEW_BORDER_BR
                         : (c == end_c) ? SFTE_MUX_PREVIEW_BORDER_BL
                                        : SFTE_MUX_PREVIEW_BORDER_LR;
        DRAW_BORDER(c, cur_r, rune);
    }
    cur_r++;

    // Content rendering
    for (uint8_t pos = 0; pos < mux->tab_cnt; ++pos) {
        uint8_t i = mux->tab_order[pos];
        sfte_term *cur_term = &mux->terms[i];
        mux->active_idx = i;

        int16_t term_start_row = cur_term->viewport.rows - preview_rows;
        if (term_start_row < 0) term_start_row = 0;

        for (int16_t term_r = term_start_row; term_r < cur_term->viewport.rows; ++term_r) {
            int32_t logical_r = _sfte_grid_vis2log(&cur_term->viewport, term_r);
            _sfte_render_extract_fg_row(ctx, logical_r, term_r, 0, 0);

            // Left border rendering
            DRAW_BORDER(start_c, cur_r, SFTE_MUX_PREVIEW_BORDER_TB);

            // Text grid rendering
            char idx_str[16];
            uint8_t idx_len = snprintf(idx_str, sizeof(idx_str), " %d ", i + 1);

            uint32_t line_bg = (mux->preview_selected_idx == i) ? SFTE_MUX_PREVIEW_HOVER_BG
                                                                : SFTE_COLOR_BG;

            for (int16_t c = start_c + 1; c < end_c; ++c) {
                int16_t text_c = c - (start_c + 1);
                uint8_t is_badge = (term_r == term_start_row && text_c < idx_len);

                uint32_t cell_bg = is_badge ? SFTE_MUX_PREVIEW_BADGE_BG : line_bg;
                _sfte_render_bg_cell(ctx, px_buf, c, cur_r, 0, cell_bg);

                if (is_badge) {
                    sfte_rune b_char = (uint8_t)idx_str[text_c];
                    uint8_t font_idx = 0;
                    uint16_t glyph_id = 0;
                    _sfte_font_resolve_rune(font, b_char, &font_idx, &glyph_id);
                    _sfte_render_fg_cell(ctx, px_buf, c, cur_r, 0, b_char, glyph_id, font_idx,
                                         SFTE_COLOR_FG, font);
                } else if (text_c < cur_term->viewport.cols) {
                    sfte_cell *vcell = _sfte_grid_get_cell(cur_term, text_c, logical_r);
                    sfte_rune r_char = vcell->rune ? vcell->rune : ' ';
                    uint32_t fg = _sfte_grid_get_fg(vcell);

                    const sfte_render_buffers *render = &cur_term->render;
                    _sfte_render_fg_cell(ctx, px_buf, c, cur_r, 0, r_char, render->ids[text_c],
                                         render->font_indices[text_c], fg,
                                         render->target_caches[text_c]);
                }
            }

            // Right border rendering
            DRAW_BORDER(end_c, cur_r, SFTE_MUX_PREVIEW_BORDER_TB);
            cur_r++;
        }

        num_tabs--;
        if (num_tabs > 0) {
            // Separator rendering
            for (int16_t c = start_c; c <= end_c; ++c) {
                sfte_rune rune = (c == start_c) ? SFTE_MUX_PREVIEW_BORDER_TBR
                                 : (c == end_c) ? SFTE_MUX_PREVIEW_BORDER_TBL
                                                : SFTE_MUX_PREVIEW_BORDER_LR;
                DRAW_BORDER(c, cur_r, rune);
            }
            cur_r++;
        }
    }

    // Bottom border rendering
    for (int16_t c = start_c; c <= end_c; ++c) {
        sfte_rune rune = (c == start_c) ? SFTE_MUX_PREVIEW_BORDER_TR
                         : (c == end_c) ? SFTE_MUX_PREVIEW_BORDER_TL
                                        : SFTE_MUX_PREVIEW_BORDER_LR;
        DRAW_BORDER(c, cur_r, rune);
    }

#undef DRAW_BORDER

    mux->active_idx = real_active_idx;
}
#endif  // SFTE_MUX_PREFIX_PREVIEW

/*
    Returns the RENDERED position of the cursor.
    Do not mistake it for the cursor position for the shell - while they're equal 99% of the
   time, they differ when search mode is on.

    To ensure that even when search mode is on ongoing sequences render properly,
    these two values are separate.

    This can be later expanded for further terminal modes, e.g. for a theme picker.
*/
static inline void _sfte_render_get_cursor_pos(sfte_ctx *ctx, int16_t *out_col, int16_t *out_row) {
    sfte_term *term = sfte_term_get_active(ctx);
#if SFTE_MUX
    sfte_mux *mux = &ctx->mux;
#endif  // SFTE_MUX
#if SFTE_SEARCH
    const sfte_search_state *search = &term->search;
#endif  // SFTE_SEARCH
    const sfte_cursor_state *cursor = &term->cursor;
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);

    *out_col = _SFTE_CLAMP(cursor->col, 0, vp->cols - 1);
    *out_row = (int16_t)_sfte_grid_log2vis(vp, cursor->row);
#if SFTE_MUX_BAR
    if (mux->is_renaming) {
        *out_col = mux->rename_cursor_col;
        *out_row = (layout.bar_y - SFTE_WINDOW_PAD_Y - layout.grid_off_y) / vp->cell_height;
        return;
    }
#endif  // SFTE_MUX_BAR
#if SFTE_SEARCH
    if (search->is_active) {
        *out_col = strlen(SFTE_SEARCH_PREFIX) + search->text_input.cursor_byte_idx;
        *out_row = SFTE_SEARCH_PLACEMENT_TOP ? 0 : vp->rows - 1;
    }
#endif  // SFTE_SEARCH
}

#if SFTE_CURSOR_TRAIL
/*
    Updates the cursor trail positions.
    This is separated from _sfte_render_trail to avoid not updating on early returns.
*/
static inline void _sfte_render_update_trail(sfte_ctx *ctx) {
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_trail_state *trail = &term->cursor.trail;
    const sfte_viewport_state *vp = &term->viewport;

    int16_t vis_col, vis_row;
    _sfte_render_get_cursor_pos(ctx, &vis_col, &vis_row);
    float target_x = vis_col * vp->cell_width;
    float target_y = vis_row * vp->cell_height;

    if (vis_col != trail->last_grid_col || vis_row != trail->last_grid_row) {
        uint64_t now = SFTE_TIME_MS();

        if (trail->last_move_ms != 0 && (now - trail->last_move_ms >= SFTE_CURSOR_TRAIL))
            trail->is_trailing = 1;
        else if (!trail->is_trailing) {
            trail->x = target_x;
            trail->y = target_y;
        }

        trail->last_grid_col = vis_col;
        trail->last_grid_row = vis_row;
        trail->last_move_ms = now;
    }
}

#if SFTE_CURSOR_TRAIL
/*
    Flags the AABB of the cursor trail as dirty.
*/
static inline void _sfte_grid_dirty_trail(sfte_trail_state *trail, sfte_term *term) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_damage_rect *dmg = &trail->dmg;

    if (dmg->w <= 0 || dmg->h <= 0) return;

    int16_t start_c, end_c;
    int32_t start_logical_r, end_logical_r;
    _sfte_grid_from_px(vp, dmg->x, dmg->y, &start_c, &start_logical_r, NULL);
    _sfte_grid_from_px(vp, dmg->x + dmg->w, dmg->y + dmg->h, &end_c, &end_logical_r, NULL);
    if (start_logical_r < 0 || end_logical_r < 0) return;

    _sfte_grid_dirty_rect(term, start_c, start_logical_r, (end_c - start_c) + 1,
                          (end_logical_r - start_logical_r) + 1);
}
#endif  // SFTE_CURSOR_TRAIL

/*
    Compositor pass for the cursor trail.
    Drawn as the last element in the rendering loop.
*/
static inline void _sfte_render_trail(sfte_ctx *ctx, void *px_buf, int32_t y_off,
                                      sfte_damage_rect *out_dmg) {
    const sfte_window_state *win = &ctx->window;
    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;
    sfte_trail_state *trail = &cursor->trail;

    if (trail->dmg.w > 0 && trail->dmg.h > 0)
        _sfte_render_damage_add(out_dmg, trail->dmg.x, trail->dmg.y, trail->dmg.w, trail->dmg.h);

    int16_t vis_col, vis_row;
    _sfte_render_get_cursor_pos(ctx, &vis_col, &vis_row);
    float target_x = vis_col * vp->cell_width;
    float target_y = vis_row * vp->cell_height;

    if (term->cursor.hidden || trail->warp_tail || !trail->is_trailing) {
        trail->x = target_x;
        trail->y = target_y;
        trail->warp_tail = 0;

        trail->dmg.w = 0;
        trail->dmg.h = 0;
        return;
    }

    float trail_w = vp->cell_width;
    float trail_h = vp->cell_height;
    float trail_y_off = 0;

    if (_SFTE_CUR_STYLE(cursor) == SFTE_CURSOR_STYLE_UNDERLINE) {
        trail_h *= SFTE_CURSOR_THICK_RATIO;
        if (trail_h < 1.0f) trail_h = 1.0f;
        trail_y_off = vp->cell_height - trail_h;
    } else if (_SFTE_CUR_STYLE(cursor) == SFTE_CURSOR_STYLE_BAR) {
        trail_w *= SFTE_CURSOR_THICK_RATIO;
        if (trail_w < 1.0f) trail_w = 1.0f;
    }

    float rx = trail_w * 0.5f;
    float ry = trail_h * 0.5f;

    float cx0 = trail->x + rx;
    float cx1 = target_x + rx;
    float cy0 = trail->y + trail_y_off + ry;
    float cy1 = target_y + trail_y_off + ry;

    float ab_x = cx1 - cx0, ab_y = cy1 - cy0;

    float l2 = ab_x * ab_x + ab_y * ab_y;
    if (l2 < 0.001f) return;

    float inv_l2 = 1.0f / l2;

    int32_t min_x = _SFTE_CLAMP((cx0 < cx1 ? cx0 : cx1) - rx + SFTE_WINDOW_PAD_X, 0, win->width);
    int32_t max_x = _SFTE_CLAMP((cx0 > cx1 ? cx0 : cx1) + rx + SFTE_WINDOW_PAD_X, 0, win->width);
    int32_t min_y = _SFTE_CLAMP((cy0 < cy1 ? cy0 : cy1) - ry + SFTE_WINDOW_PAD_Y + y_off, 0,
                                win->height);
    int32_t max_y = _SFTE_CLAMP((cy0 > cy1 ? cy0 : cy1) + ry + SFTE_WINDOW_PAD_Y + y_off, 0,
                                win->height);

#if SFTE_CURSOR_DYNAMIC
    uint32_t trail_color = cursor->color;
#else   // !SFTE_CURSOR_DYNAMIC
    uint32_t trail_color = SFTE_CURSOR_COLOR;
#endif  // !SFTE_CURSOR_DYNAMIC

#if SFTE_CURSOR_BLINK
    uint8_t skip_cursor = cursor->blink_visible;
#else
    uint8_t skip_cursor = 1;
#endif

    for (int32_t y = min_y; y < max_y; ++y) {
        float up_y = (float)(y - SFTE_WINDOW_PAD_Y - y_off) + 0.5f;
        float dy_from_cy0 = up_y - cy0;

        for (int32_t x = min_x; x < max_x; ++x) {
            float up_x = (float)(x - SFTE_WINDOW_PAD_X) + 0.5f;

            if (skip_cursor && up_x >= target_x && up_x < target_x + trail_w &&
                up_y >= target_y + trail_y_off && up_y < target_y + trail_y_off + trail_h)
                continue;

            float dx_from_cx0 = up_x - cx0;
            float t = _SFTE_CLAMP((dx_from_cx0 * ab_x + dy_from_cy0 * ab_y) * inv_l2, 0.0f, 1.0f);

            if (fabsf(up_x - cx0 - t * ab_x) <= rx && fabsf(up_y - cy0 - t * ab_y) <= ry) {
                uint8_t alpha = (uint8_t)(128.0f * t);
                SFTE_COLOR_BLEND_PIXEL(px_buf, x, y, win->width, trail_color, alpha);
            }
        }
    }

    sfte_damage_rect *t_dmg = &trail->dmg;

    t_dmg->x = min_x;
    t_dmg->y = min_y;
    t_dmg->w = max_x - min_x;
    t_dmg->h = max_y - min_y;

    _sfte_render_damage_add(out_dmg, t_dmg->x, t_dmg->y, t_dmg->w, t_dmg->h);
}
#endif  // SFTE_CURSOR_TRAIL

#if SFTE_TERM_ANIMATE_SCREEN
/*
    Calculates the Y offsets for the outgoing and incoming screens during an alt-screen
   transition. Returns 2 when finished animating. Returns 1 when currently animating. Returns 0
   if static.
*/
static inline uint8_t _sfte_render_get_anim_offsets(sfte_ctx *ctx, int32_t *out_y, int32_t *in_y) {
    const sfte_window_state *win = &ctx->window;
    sfte_term *term = sfte_term_get_active(ctx);

    if (!term->is_animating) return 0;

    uint64_t now = SFTE_TIME_MS();
    float progress = (now - term->anim_start_ms) / SFTE_TERM_ANIM_DUR_MS;
    if (progress >= 1.0f) {
        term->is_animating = 0;
        return 2;
    }

    float ease = 1.0f - powf(1.0f - progress, 3.0f);
    int32_t slide = (uint32_t)(ease * win->height);

    if (term->anim_dir == 1) {  // entering alt / sliding up
        *out_y = -slide;
        *in_y = win->height - slide;
    } else {  // exiting alt / sliding down
        *out_y = slide;
        *in_y = -win->height + slide;
    }
    return 1;
}
#endif  // SFTE_TERM_ANIMATE_SCREEN

/*
    Prepares render passes.
    Returns the amount of passes in `passes` array.
*/
static inline uint8_t _sfte_render_prepare_passes(sfte_ctx *ctx, void *px_buf,
                                                  sfte_pass_info *passes,
                                                  sfte_damage_rect *out_dmg) {
    sfte_window_state *win = &ctx->window;
    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_cursor_state *cursor = &term->cursor;
    sfte_viewport_state *vp = &term->viewport;
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);

    passes[0].y_off = 0;
    passes[0].grid = term->cells;
    passes[0].hide_cursor = cursor->hidden;

    uint8_t needs_wipe = 0;
    uint8_t passes_cnt = 1;
#if SFTE_TERM_ANIMATE_SCREEN
    int32_t out_y = 0, in_y = 0;
    uint8_t anim_state = _sfte_render_get_anim_offsets(ctx, &out_y, &in_y);
    if (anim_state == 1 || anim_state == 2) needs_wipe = 1;
    if (anim_state == 1) passes_cnt = 2;
#endif  // SFTE_TERM_ANIMATE_SCREEN

#if SFTE_TERM_SCROLL_SMOOTH
    int32_t sb_diff = vp->sb_off - vp->last_sb_off;
    if (sb_diff != 0) {
        vp->scroll_y_off -= sb_diff * vp->cell_height;
        vp->last_sb_off = vp->sb_off;

        if (!vp->is_scrolling) {
            vp->is_scrolling = 1;
            vp->last_scroll_ms = SFTE_TIME_MS();
        }
    }

    if (vp->is_scrolling) {
        uint64_t now = SFTE_TIME_MS();
        float dt = (float)(now - vp->last_scroll_ms);
        vp->last_scroll_ms = now;

        float decay = dt * SFTE_TERM_SCROLL_DECAY;
        if (decay > 1.0f) decay = 1.0f;

        vp->scroll_y_off -= vp->scroll_y_off * decay;

        if (fabsf(vp->scroll_y_off) < 0.5f || !isfinite(vp->scroll_y_off)) {
            vp->scroll_y_off = 0.0f;
            vp->is_scrolling = 0;
        }
        needs_wipe = 1;
    }
#endif  // SFTE_TERM_SCROLL_SMOOTH

    if (needs_wipe) {
        _sfte_render_damage_add(out_dmg, 0, 0, win->width, win->height);
        win->pad_dirty = 1;
        uint32_t clear_bg = (SFTE_COLOR_BG_OPACITY << 24) |
                            (SFTE_COLOR_BG & ~SFTE_COLOR_ALPHA_MASK);
        for (int32_t y = 0; y < win->height; ++y)
            for (int32_t x = 0; x < win->width; ++x)
                SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, win->width, win->height, clear_bg);

        for (int32_t i = 0; i < vp->rows * vp->cols; ++i) {
            term->cells[i].dirty = 1;
#if SFTE_TERM_ALT_SCREEN
            if (passes_cnt == 2 && term->alt_cells) term->alt_cells[i].dirty = 1;
#endif  // SFTE_TERM_ALT_SCREEN
        }
    }

#if SFTE_TERM_ANIMATE_SCREEN
    if (passes_cnt == 2) {
        passes[0].y_off = out_y;
        passes[0].grid = term->alt_cells;
        passes[0].hide_cursor = 1;

        passes[1].y_off = in_y;
#if SFTE_TERM_SCROLL_SMOOTH
        passes[1].y_off += (int32_t)vp->scroll_y_off;
#endif  // SFTE_TERM_SCROLL_SMOOTH
        passes[1].grid = term->cells;
        passes[1].hide_cursor = cursor->hidden;
    }
#endif  // SFTE_TERM_ANIMATE_SCREEN
#if SFTE_TERM_SCROLL_SMOOTH
    passes[0].y_off = (int32_t)vp->scroll_y_off;
#endif  // SFTE_TERM_SCROLL_SMOOTH

    // Shift all passes down by the height of the multiplexer bar if it's at the top of the
    // window and not hovering.
    for (uint8_t p = 0; p < passes_cnt; ++p) passes[p].y_off += layout.grid_off_y;

    return passes_cnt;
}

/*
    Paints the solid background color for a terminal cell.
*/
static inline void _sfte_render_bg_cell(sfte_ctx *ctx, void *px_buf, int16_t col, int16_t row,
                                        int32_t y_off, uint32_t bg) {
    const sfte_window_state *win = &ctx->window;
    const sfte_viewport_state *vp = &sfte_term_get_active(ctx)->viewport;

    int32_t cx = col * vp->cell_width + SFTE_WINDOW_PAD_X;
    int32_t cy = row * vp->cell_height + SFTE_WINDOW_PAD_Y;
    uint32_t final_bg = (SFTE_COLOR_BG_OPACITY << 24) | (bg & ~SFTE_COLOR_ALPHA_MASK);

    for (int32_t y = 0; y < vp->cell_height; ++y)
        for (int32_t x = 0; x < vp->cell_width; ++x)
            SFTE_COLOR_DRAW_PIXEL(px_buf, cx + x, cy + y + y_off, win->width, win->height,
                                  final_bg);
}

/*
    Renders extended underline styles (straight, double, undercurl, dotted, dotted, dashed).

    Undercurls require a periodic wave. To avoid slow math calls per-pixel, this implementation
    approximates a triangle wave using integer mod arithmetic.
*/
static inline void _sfte_render_underline_cell(sfte_ctx *ctx, void *px_buf, int32_t cx, int32_t cy,
                                               int32_t render_w, sfte_cell *vcell) {
    const sfte_font_state *font = &ctx->font;
    const sfte_window_state *win = &ctx->window;
    const sfte_viewport_state *vp = &sfte_term_get_active(ctx)->viewport;

    uint32_t base_ul_col = _sfte_grid_get_ul(vcell);
    uint32_t underline_col = SFTE_COLOR_ALPHA_MASK | (base_ul_col & ~SFTE_COLOR_ALPHA_MASK);

    int32_t thick = vp->cell_height * SFTE_UNDERLINE_THICK_RATIO;
    if (thick < 1) thick = 1;

    uint8_t style = _SFTE_UNDERLINE_STYLE_STRAIGHT;
#if SFTE_UNDERLINE_EXTENDED
    style = _SFTE_CLAMP(vcell->ul_style, _SFTE_UNDERLINE_STYLE_STRAIGHT,
                        _SFTE_UNDERLINE_STYLE_DASHED);
#endif  // SFTE_UNDERLINE_EXTENDED

    int32_t offset = vp->cell_height * SFTE_UNDERLINE_OFFSET_RATIO;
    if (offset < 1) offset = 1;

    int32_t base_y = cy + font->ascent + offset;
    if (base_y + thick > cy + vp->cell_height) base_y = cy + vp->cell_height - thick;

    for (int32_t x = cx; x < cx + render_w; ++x) {
        if (x >= win->width) break;

        int32_t grid_x = x - SFTE_WINDOW_PAD_X;
        int32_t local_x = grid_x % vp->cell_width;

        switch (style) {
        case _SFTE_UNDERLINE_STYLE_CURLY: {
            int32_t half_w = vp->cell_width / 2;
            if (half_w == 0) half_w = 1;

            int32_t amp = thick + 1;
            int32_t dist = local_x > half_w ? local_x - half_w : half_w - local_x;
            int32_t y_off = (dist * amp) / half_w - (amp / 2);

            for (int32_t dy = 0; dy < thick; ++dy) {
                int32_t py = base_y + y_off + dy;
                if (py >= 0 && py < win->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, py, win->width, win->height, underline_col);
            }
            break;
        }
        case _SFTE_UNDERLINE_STYLE_DOUBLE: {
            int32_t half_thick = thick / 2;
            if (half_thick < 1) half_thick = 1;
            int32_t gap = half_thick < 2 ? 1 : half_thick;
            for (int32_t dy = 0; dy < half_thick; ++dy) {
                int32_t py1 = base_y - half_thick + dy;
                int32_t py2 = base_y + gap + dy;
                if (py1 >= 0 && py1 < win->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, py1, win->width, win->height, underline_col);
                if (py2 >= 0 && py2 < win->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, py2, win->width, win->height, underline_col);
            }
            break;
        }
        case _SFTE_UNDERLINE_STYLE_DOTTED:
            if ((grid_x / thick) % 2 != 0) break;
            // fallthrough
        case _SFTE_UNDERLINE_STYLE_DASHED:
            if (style == _SFTE_UNDERLINE_STYLE_DASHED && ((grid_x / thick) % 5 >= 3)) break;
            // fallthrough
        case _SFTE_UNDERLINE_STYLE_STRAIGHT:
        default:
            for (int32_t dy = 0; dy < thick; ++dy) {
                int32_t py = base_y + dy;
                if (py >= 0 && py < win->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, py, win->width, win->height, underline_col);
            }
        }
    }
}

static inline void _sfte_render_cursor(sfte_ctx *ctx, void *px_buf, int16_t col, int16_t row,
                                       int32_t y_off, sfte_damage_rect *out_dmg) {
    (void)out_dmg;
    const sfte_window_state *win = &ctx->window;
#if SFTE_MUX
    const sfte_mux *mux = &ctx->mux;
#endif  // SFTE_MUX
    const sfte_term *term = sfte_term_get_active(ctx);
#if SFTE_SEARCH
    const sfte_search_state *search = &term->search;
#endif  // SFTE_SEARCH
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;

#if SFTE_MUX_PREFIX_PREVIEW
    if (mux->is_prefix_active) return;
#endif  // SFTE_MUX_PREFIX_PREVIEW

#if SFTE_CURSOR_TRAIL
    _sfte_render_update_trail(ctx);
#endif  // SFTE_CURSOR_TRAIL

    if (cursor->hidden) return;

#if SFTE_CURSOR_TRAIL
#if SFTE_TERM_ANIMATE_SCREEN
    if (!term->is_animating)
#endif  // SFTE_TERM_ANIMATE_SCREEN
        _sfte_render_trail(ctx, px_buf, y_off, out_dmg);
#endif  // SFTE_CURSOR_TRAIL

#if SFTE_CURSOR_BLINK
    if (!cursor->blink_visible) return;
#endif  // SFTE_CURSOR_BLINK

#if SFTE_TERM_SCROLLBACK_CAP
    if (vp->sb_off > 0
#if SFTE_SEARCH
        && !search->is_active
#endif  // SFTE_SEARCH
    )
        return;
#endif  // SFTE_TERM_SCROLLBACK_CAP

#if SFTE_CURSOR_DYNAMIC
    uint32_t active_cur_color = cursor->color;
#else   // !SFTE_CURSOR_DYNAMIC
    uint32_t active_cur_color = SFTE_CURSOR_COLOR;
#endif  // !SFTE_CURSOR_DYNAMIC
    uint32_t cur_col = SFTE_COLOR_ALPHA_MASK | (active_cur_color & ~SFTE_COLOR_ALPHA_MASK);

    int32_t cx = col * vp->cell_width + SFTE_WINDOW_PAD_X;
    int32_t cy = row * vp->cell_height + SFTE_WINDOW_PAD_Y + y_off;

    int32_t render_w = vp->cell_width;
    sfte_cell *cell = _sfte_grid_get_cell(term, col, _sfte_grid_vis2log(vp, row));
#if SFTE_FONT_WIDE_CHARS
    render_w *= (cell->attr & _SFTE_ATTR_WIDE) ? 2 : 1;
#endif  // SFTE_FONT_WIDE_CHARS

#if SFTE_TERM_FOCUS
    if (!ctx->window.is_focused) {
        for (int32_t y = cy; y < cy + vp->cell_height; ++y) {
            SFTE_COLOR_DRAW_PIXEL(px_buf, cx, y, win->width, win->height, cur_col);
            SFTE_COLOR_DRAW_PIXEL(px_buf, cx + render_w - 1, y, win->width, win->height, cur_col);
        }
        for (int32_t x = cx; x < cx + render_w; ++x) {
            SFTE_COLOR_DRAW_PIXEL(px_buf, x, cy, win->width, win->height, cur_col);
            SFTE_COLOR_DRAW_PIXEL(px_buf, x, cy + vp->cell_height - 1, win->width, win->height,
                                  cur_col);
        }
        return;
    }
#endif  // SFTE_TERM_FOCUS
    if (_SFTE_CUR_STYLE(cursor) == SFTE_CURSOR_STYLE_BLOCK) {
        _sfte_render_bg_cell(ctx, px_buf, col, row, y_off, cur_col);
#if SFTE_SEARCH
        if (search->is_active) return;
#endif  // SFTE_SEARCH
#if SFTE_MUX
        if (mux->is_renaming) return;
#endif  // SFTE_MUX

#if SFTE_FONT_WIDE_CHARS
        if ((cell->attr & _SFTE_ATTR_WIDE) && col + 1 < vp->cols)
            _sfte_render_bg_cell(ctx, px_buf, col + 1, row, y_off, cur_col);
#endif  // SFTE_FONT_WIDE_CHARS

        uint32_t rune = cell->rune ? cell->rune : ' ';

        if (rune != ' ') {
            sfte_font_cache *font = _sfte_font_get_cache(ctx, SFTE_FONT_STYLE_REGULAR);
            uint8_t font_idx = 0;
            uint16_t glyph_id = 0;
            _sfte_font_resolve_rune(font, rune, &font_idx, &glyph_id);
            _sfte_render_fg_cell(ctx, px_buf, col, row, y_off, rune, glyph_id, font_idx, cell->bg,
                                 font);
        }
    } else if (_SFTE_CUR_STYLE(cursor) == SFTE_CURSOR_STYLE_UNDERLINE) {
        int32_t thick = vp->cell_height * SFTE_CURSOR_THICK_RATIO;
        if (thick < 1) thick = 1;

        for (int32_t y = cy + vp->cell_height - thick; y < cy + vp->cell_height; ++y)
            for (int32_t x = cx; x < cx + render_w; ++x)
                if (x < win->width && y < win->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, win->width, win->height, cur_col);
    } else if (_SFTE_CUR_STYLE(cursor) == SFTE_CURSOR_STYLE_BAR) {
        int32_t thick = vp->cell_width * SFTE_CURSOR_THICK_RATIO;
        if (thick < 1) thick = 1;

        for (int32_t y = cy; y < cy + vp->cell_height; ++y)
            for (int32_t x = cx; x < cx + thick; ++x)
                if (x < win->width && y < win->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, win->width, win->height, cur_col);
    }
}

/*
    Dispatcher for terminal text decorations (underlines, cursor).
*/
static inline void _sfte_render_decorations_cell(sfte_ctx *ctx, void *px_buf, int16_t col,
                                                 int16_t row, int32_t y_off, sfte_cell *vcell) {
    const sfte_viewport_state *vp = &sfte_term_get_active(ctx)->viewport;

    int32_t cx = col * vp->cell_width + SFTE_WINDOW_PAD_X;
    int32_t cy = row * vp->cell_height + SFTE_WINDOW_PAD_Y;

    int32_t render_w = vp->cell_width;
#if SFTE_FONT_WIDE_CHARS
    render_w *= (vcell->attr & _SFTE_ATTR_WIDE) ? 2 : 1;
#endif  // SFTE_FONT_WIDE_CHARS

    if (vcell->attr & _SFTE_ATTR_UNDERLINE)
        _sfte_render_underline_cell(ctx, px_buf, cx, cy + y_off, render_w, vcell);
}

/*
    Background rendering pass.
    Renders the whole grid, contrary to `_sfte_render_bg_cell`.
*/
static inline void _sfte_render_bg_grid(sfte_ctx *ctx, void *px_buf, int32_t y_off) {
    const sfte_term *term = sfte_term_get_active(ctx);
#if SFTE_SEARCH
    const sfte_search_state *search = &term->search;
#endif  // SFTE_SEARCH
    const sfte_viewport_state *vp = &term->viewport;
#if SFTE_INPUT_SELECTION
    const sfte_mouse_state *mouse = &term->mouse;
#endif  // SFTE_INPUT_SELECTION

    for (int16_t r = 0; r < vp->rows; ++r) {
        int32_t logical_r = _sfte_grid_vis2log(vp, r);
        for (int16_t c = 0; c < vp->cols; ++c) {
            sfte_cell *vcell = _sfte_grid_get_cell(term, c, logical_r);
            if (!vcell->dirty) continue;

            uint32_t bg = _sfte_grid_get_bg(vcell);
            uint8_t attr = vcell->attr;

#if SFTE_INPUT_SELECTION
            if (_sfte_input_is_selected(mouse, c, logical_r)) attr ^= _SFTE_ATTR_REVERSE;
#endif  // SFTE_INPUT_SELECTION

#if SFTE_SEARCH
            uint8_t search_res = _sfte_search_is_highlighted(search, vp, c, logical_r);
            // If `search_res` is 0 it isn't highlighted and we continue,
            // If `search_res` is 1 it is highlighted but not active,
            // If `search_res` is 2 it is highlighted AND active.
            if (search_res == 1) {
                bg = SFTE_SEARCH_BG_MATCH;
                attr |= SFTE_SEARCH_ATTR_MATCH;
            } else if (search_res == 2) {
                bg = SFTE_SEARCH_BG_ACTIVE;
                attr |= SFTE_SEARCH_ATTR_ACTIVE;
            }
#endif  // SFTE_SEARCH

            if (attr & _SFTE_ATTR_REVERSE) bg = _sfte_grid_get_fg(vcell);

            _sfte_render_bg_cell(ctx, px_buf, c, r, y_off, bg);
        }
    }
}

#if SFTE_FONT_LIGATURES
/*
    Computes and returns a 64-bit FNV-1a hash of the row's shaping inputs.
*/
static inline uint64_t _sfte_render_get_row_hash(const sfte_term *term, int32_t logical_row) {
    const sfte_render_buffers *render = &term->render;
    const sfte_viewport_state *vp = &term->viewport;

    uint64_t hash = 14695981039346656037ULL;  // FNV-1a 64-bit offset basis
    const uint64_t prime = 1099511628211ULL;

    for (int16_t c = 0; c < vp->cols; ++c) {
        sfte_cell *vcell = _sfte_grid_get_cell(term, c, logical_row);
        uint8_t attr = vcell->attr;
        uint32_t fg = _sfte_grid_get_fg(vcell);
        uint32_t bg = _sfte_grid_get_bg(vcell);

        hash ^= render->shaper_ids[c];
        hash *= prime;
        hash ^= render->font_indices[c];
        hash *= prime;
        hash ^= attr;
        hash *= prime;
        hash ^= fg;
        hash *= prime;
        hash ^= bg;
        hash *= prime;
    }

    return hash;
}

/*
    Chunks a row into continuous style/color blocks and passes them to the OpenType shaper.
    Ensures ligatures do not bleed across formatting boundaries.
*/
static inline void _sfte_render_shape_fg_row(sfte_term *term, int32_t logical_row) {
    const sfte_render_buffers *render = &term->render;
    const sfte_viewport_state *vp = &term->viewport;

    int16_t span_start = 0;
    while (span_start < vp->cols) {
        if (!render->shaper_ids[span_start]) {
            span_start++;
            continue;
        }

        sfte_cell *start_cell = _sfte_grid_get_cell(term, span_start, logical_row);
        uint32_t active_fg = _sfte_grid_get_fg(start_cell);
        uint32_t active_bg = _sfte_grid_get_bg(start_cell);

        sfte_font_cache *active_cache = render->target_caches[span_start];
        uint8_t active_font_idx = render->font_indices[span_start];

        int16_t span_end = span_start + 1;
        while (span_end < vp->cols && render->shaper_ids[span_end] != 0) {
            sfte_cell *next_cell = _sfte_grid_get_cell(term, span_end, logical_row);
            if (_sfte_grid_get_fg(next_cell) != active_fg ||
                _sfte_grid_get_bg(next_cell) != active_bg ||
                render->target_caches[span_end] != active_cache ||
                render->font_indices[span_end] != active_font_idx)
                break;
            span_end++;
        }

        _sfte_shaper_shape_row(&active_cache->shaper[active_font_idx],
                               active_cache->ttf_buf[active_font_idx],
                               &render->shaper_ids[span_start], span_end - span_start);

        span_start = span_end;
    }
}
#endif  // SFTE_FONT_LIGATURES

/*
    Extracts a single terminal row into linear arrays for shaping and rendering.
    Resolves runes into raw glyph IDs and identifies style/color boundaries.
*/
static inline void _sfte_render_extract_fg_row(sfte_ctx *ctx, int32_t logical_row, int16_t row,
                                               int16_t vis_col, int16_t vis_row) {
    (void)row, (void)vis_col, (void)vis_row;
    sfte_font_state *font = &ctx->font;
    const sfte_term *term = sfte_term_get_active(ctx);
    const sfte_render_buffers *render = &term->render;
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;

    for (int16_t c = 0; c < vp->cols; ++c) {
        sfte_cell *vcell = _sfte_grid_get_cell(term, c, logical_row);
        sfte_rune rune = vcell->rune ? vcell->rune : ' ';
#if defined(SFTE_FONT_BOLD) || defined(SFTE_FONT_ITALIC) || defined(SFTE_FONT_BOLD_ITALIC)
        uint8_t attr = vcell->attr;
#endif  // defined(SFTE_FONT_BOLD) || defined(SFTE_FONT_ITALIC) || defined(SFTE_FONT_BOLD_ITALIC)

        sfte_font_cache *target_cache = &font->regular;
#ifdef SFTE_FONT_BOLD
        if (attr & _SFTE_ATTR_BOLD) target_cache = &font->bold;
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
        if (attr & _SFTE_ATTR_ITALIC) target_cache = &font->italic;
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
        if ((attr & _SFTE_ATTR_BOLD) && (attr & _SFTE_ATTR_ITALIC))
            target_cache = &font->bold_italic;
#endif  // SFTE_FONT_BOLD_ITALIC
        render->target_caches[c] = target_cache;
        _sfte_font_resolve_rune(target_cache, rune, &render->font_indices[c], &render->ids[c]);

#if SFTE_FONT_LIGATURES
        uint8_t is_cursor = (c == vis_col && row == vis_row && !cursor->hidden);
        render->shaper_ids[c] = (rune == ' ' || rune == 0 || is_cursor) ? 0 : render->ids[c];
#endif  // SFTE_FONT_LIGATURES
    }
}

/*
    Iterates over a shaped row and dispatches foreground/decoration drawing.
*/
static inline void _sfte_render_fg_row(sfte_ctx *ctx, void *px_buf, int16_t row,
                                       int32_t logical_row, int32_t y_off,
                                       sfte_damage_rect *out_dmg) {
    const sfte_window_state *win = &ctx->window;
    const sfte_term *term = sfte_term_get_active(ctx);
#if SFTE_SEARCH
    const sfte_search_state *search = &term->search;
#endif  // SFTE_SEARCH
#if SFTE_INPUT_SELECTION
    const sfte_mouse_state *mouse = &term->mouse;
#endif  // SFTE_INPUT_SELECTION
    const sfte_render_buffers *render = &term->render;
    const sfte_viewport_state *vp = &term->viewport;

    for (int16_t c = 0; c < vp->cols; ++c) {
        sfte_cell *vcell = _sfte_grid_get_cell(term, c, logical_row);
        if (!vcell->dirty) continue;

        uint8_t attr = vcell->attr;

#if SFTE_FONT_WIDE_CHARS
        if (attr & _SFTE_ATTR_DUMMY) {
            _sfte_render_damage_add(out_dmg, c * vp->cell_width + SFTE_WINDOW_PAD_X,
                                    row * vp->cell_height + SFTE_WINDOW_PAD_Y, vp->cell_width,
                                    vp->cell_height);
            vcell->dirty = 0;
            continue;
        }
#endif  // SFTE_FONT_WIDE_CHARS
        sfte_rune rune = vcell->rune ? vcell->rune : ' ';
        uint32_t fg = _sfte_grid_get_fg(vcell);

#if SFTE_INPUT_SELECTION
        if (_sfte_input_is_selected(mouse, c, logical_row)) attr ^= _SFTE_ATTR_REVERSE;
#endif  // SFTE_INPUT_SELECTION

#if SFTE_SEARCH
        uint8_t search_res = _sfte_search_is_highlighted(search, vp, c, logical_row);
        // If `search_res` is 0 it isn't highlighted and we continue,
        // If `search_res` is 1 it is highlighted but not active,
        // If `search_res` is 2 it is highlighted AND active.
        if (search_res == 1) {
            fg = SFTE_SEARCH_FG_MATCH;
            attr |= SFTE_SEARCH_ATTR_MATCH;
        } else if (search_res == 2) {
            fg = SFTE_SEARCH_FG_ACTIVE;
            attr |= SFTE_SEARCH_ATTR_ACTIVE;
        }
#endif  // SFTE_SEARCH

        if (attr & _SFTE_ATTR_REVERSE) fg = _sfte_grid_get_bg(vcell);
#ifdef SFTE_BOLD_WHITE
        if ((attr & _SFTE_ATTR_BOLD) && !(attr & _SFTE_ATTR_REVERSE)) fg = SFTE_COLOR_FG;
#endif

#if SFTE_FONT_LIGATURES
        if (render->font_indices[c] == 0 && render->shaper_ids[c] != 0 &&
            render->shaper_ids[c] != render->ids[c]) {
            render->ids[c] = render->shaper_ids[c];
        }
#endif  // SFTE_TERM_LIGATURES

        _sfte_render_fg_cell(ctx, px_buf, c, row, y_off, rune, render->ids[c],
                             render->font_indices[c], fg, render->target_caches[c]);
        _sfte_render_decorations_cell(ctx, px_buf, c, row, y_off, vcell);

        int32_t dmg_cy = row * vp->cell_height + SFTE_WINDOW_PAD_Y;
        int32_t dmg_ch = vp->cell_height;

        if (row == 0) {
            dmg_cy = 0;
            dmg_ch += SFTE_WINDOW_PAD_Y;
        } else if (row == vp->rows - 1) {
            dmg_ch += win->height - (dmg_cy + dmg_ch);
        }
        _sfte_render_damage_add(out_dmg, 0, dmg_cy, win->width, dmg_ch);
    }
}

/*
    Foreground rendering pass.
    Renders the whole grid, contrary to `_sfte_render_fg_cell`.
*/
static inline void _sfte_render_fg_grid(sfte_ctx *ctx, void *px_buf, int16_t vis_col,
                                        int16_t vis_row, int32_t y_off, sfte_damage_rect *out_dmg) {
    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_render_buffers *render = &term->render;
    const sfte_viewport_state *vp = &term->viewport;

    for (int16_t r = 0; r < vp->rows; ++r) {
        int32_t logical_r = _sfte_grid_vis2log(vp, r);
        _sfte_render_extract_fg_row(ctx, logical_r, r, vis_col, vis_row);

#if SFTE_FONT_LIGATURES
        int32_t cache_idx = (logical_r >= 0) ? ((logical_r + vp->grid_off) % vp->rows) : r;
        uint64_t row_hash = _sfte_render_get_row_hash(term, logical_r);
        uint16_t *memo_ids = &render->row_shaper_ids[cache_idx * vp->cols];
        if (render->row_hashes[cache_idx] == row_hash)
            memcpy(render->shaper_ids, memo_ids, vp->cols * sizeof(uint16_t));
        else {
            _sfte_render_shape_fg_row(term, logical_r);
            render->row_hashes[cache_idx] = row_hash;

            // Dirty any cells that had their shapes changed due to ligatures
            for (int16_t c = 0; c < vp->cols; ++c)
                if (render->shaper_ids[c] != memo_ids[c])
                    _sfte_grid_get_cell(term, c, logical_r)->dirty = 1;

            memcpy(memo_ids, render->shaper_ids, vp->cols * sizeof(uint16_t));
        }
#endif  // SFTE_FONT_LIGATURES
        _sfte_render_fg_row(ctx, px_buf, r, logical_r, y_off, out_dmg);
    }
}

// =================================================================================================
// >>public api
// =================================================================================================

void sfte_render(sfte_ctx *ctx, void *px_buf, int32_t w, int32_t h, sfte_damage_rect *out_dmg) {
#if SFTE_MUX && SFTE_MUX_PREFIX_PREVIEW
    sfte_mux *mux = &ctx->mux;
#endif  // SFTE_MUX && SFTE_MUX_PREFIX_PREVIEW
    sfte_window_state *win = &ctx->window;
    sfte_term *term = sfte_term_get_active(ctx);
#if SFTE_TERM_ALT_SCREEN
    sfte_modes_state *modes = &term->modes;
#endif  // SFTE_TERM_ALT_SCREEN
#if SFTE_SEARCH
    sfte_search_state *search = &term->search;
#endif  // SFTE_SEARCH
    sfte_cursor_state *cursor = &term->cursor;
#if SFTE_CURSOR_TRAIL
    sfte_trail_state *trail = &cursor->trail;
#endif  // SFTE_CURSOR_TRAIL
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);

    win->width = w;
    win->height = h;

    if (!vp->cell_width || !vp->cell_height) {
        out_dmg->w = 0;
        out_dmg->h = 0;
        return;
    }

    int16_t new_cols = (w - (2 * SFTE_WINDOW_PAD_X)) / vp->cell_width;
    int16_t new_rows = (h - (2 * SFTE_WINDOW_PAD_Y)) / vp->cell_height;
// Make space for multiplexer bar if enabled and not hovering above viewport
#if SFTE_MUX_BAR && !SFTE_MUX_BAR_HOVER
    if (new_rows > 1) new_rows--;
#endif  // SFTE_MUX_BAR && !SFTE_MUX_BAR_HOVER
    if (new_cols != vp->cols || new_rows != vp->rows) _sfte_grid_resize(term, new_cols, new_rows);

    sfte_pass_info passes[2];
    uint8_t num_passes = _sfte_render_prepare_passes(ctx, px_buf, passes, out_dmg);

    int16_t vis_col;
    int16_t vis_row;
    _sfte_render_get_cursor_pos(ctx, &vis_col, &vis_row);
#if SFTE_FONT_WIDE_CHARS
    uint8_t cursor_is_visible = (vis_row >= 0 && vis_row < vp->rows);
    if (cursor_is_visible && vis_col > 0 &&
        (_sfte_grid_get_cell(term, vis_col, vis_row)->attr & _SFTE_ATTR_DUMMY))
        vis_col--;
#endif  // SFTE_FONT_WIDE_CHARS

    _sfte_render_propagate_damage(ctx, vis_col, vis_row);

#if SFTE_SEARCH && SFTE_SEARCH_BG_BAR_OPACITY != 0xFF

    // Force the text grid row beneath the overlay to completely redraw
    // This is crucial so that alpha blending doesn't break, hence
    // it's hidden behind `SFTE_SEARCH_BAR_BG_OPACITY != 0xFF`.
#if SFTE_SEARCH_PLACEMENT_TOP
    int16_t search_row = 0;
#else   // !SFTE_SEARCH_PLACEMENT_TOP
    int16_t search_row = vp->rows - 1;
#endif  // !SFTE_SEARCH_PLACEMENT_TOP
    if (search->is_active) _sfte_grid_dirty_rows(term, search_row, search_row);
#endif  // SFTE_SEARCH && SFTE_SEARCH_BG_BAR_OPACITY != 0xFF

    // Similarly to the search logic, we need to completely redraw the row beneath the overlay
    // if it's hovering above the viewport.
    if (layout.overlay) {
        int16_t r = (layout.bar_y - SFTE_WINDOW_PAD_Y) / vp->cell_height;
        _sfte_grid_dirty_rows(term, r, r);
    }

#if SFTE_MUX_PREFIX_PREVIEW
    if (mux->is_prefix_active || mux->was_prefix_active) {
        _sfte_grid_dirty_rows(term, -vp->sb_off, -vp->sb_off + vp->rows - 1);
        mux->was_prefix_active = mux->is_prefix_active;
    }
#endif  // SFTE_MUX_PREFIX_PREVIEW

#if SFTE_CURSOR_TRAIL
    _sfte_grid_dirty_trail(trail, term);
#endif  // SFTE_CURSOR_TRAIL

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    _sfte_render_sort_images(ctx);
    int32_t base_y_off = _sfte_grid_log2vis(vp, 0) * vp->cell_height;
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

#if SFTE_TERM_ALT_SCREEN
    uint8_t orig_alt_active = modes->alt_active;
#endif  // SFTE_TERM_ALT_SCREEN

    for (uint8_t p = 0; p < num_passes; ++p) {
        term->cells = passes[p].grid;
        cursor->hidden = passes[p].hide_cursor;

#if SFTE_TERM_ALT_SCREEN
        if (num_passes == 2 && p == 0)
            modes->alt_active = !orig_alt_active;
        else
            modes->alt_active = orig_alt_active;
#endif  // SFTE_TERM_ALT_SCREEN

        // Rendering order:
        // BG grid -> BG images -> FG grid -> FG images
        _sfte_render_bg_grid(ctx, px_buf, passes[p].y_off);

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
        _sfte_render_images(ctx, px_buf, 1, base_y_off + passes[p].y_off, win->pad_dirty);
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

        _sfte_render_fg_grid(ctx, px_buf, vis_col, vis_row, passes[p].y_off, out_dmg);

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
        _sfte_render_images(ctx, px_buf, 0, base_y_off + passes[p].y_off, win->pad_dirty);
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    }

#if SFTE_TERM_ALT_SCREEN
    term->modes.alt_active = orig_alt_active;
#endif  // SFTE_TERM_ALT_SCREEN

#if SFTE_MUX_BAR
#if SFTE_MUX_PREFIX_PREVIEW
    if (mux->is_prefix_active)
        _sfte_render_mux_preview(ctx, px_buf, out_dmg);
    else
#endif  // SFTE_MUX_PREFIX_PREVIEW
        _sfte_render_mux_bar(ctx, px_buf, out_dmg);
#endif  // SFTE_MUX_BAR

#if SFTE_SEARCH
    _sfte_render_search_overlay(ctx, px_buf, out_dmg);
#endif  // SFTE_SEARCH

    _sfte_render_cursor(ctx, px_buf, vis_col, vis_row, passes[0].y_off, out_dmg);

    if (ctx->window.pad_dirty) {
        _sfte_view_clear_padding_rects(ctx, px_buf);
        ctx->window.pad_dirty--;
        if (num_passes == 1) _sfte_render_damage_add(out_dmg, 0, 0, w, h);
    }

    if (out_dmg->w > 0 && out_dmg->h > 0) {
        if (out_dmg->x < 0) {
            out_dmg->w += out_dmg->x;
            out_dmg->x = 0;
        }
        if (out_dmg->y < 0) {
            out_dmg->h += out_dmg->y;
            out_dmg->y = 0;
        }
        if (out_dmg->x + out_dmg->w > w) out_dmg->w = w - out_dmg->x;
        if (out_dmg->y + out_dmg->h > h) out_dmg->h = h - out_dmg->y;

        if (out_dmg->w <= 0 || out_dmg->h <= 0) {
            out_dmg->w = 0;
            out_dmg->h = 0;
        } else
            for (int32_t r = 0; r < vp->rows; ++r) {
                int32_t logical_r = _sfte_grid_vis2log(vp, r);
                if (logical_r < vp->rows)
                    for (int16_t c = 0; c < vp->cols; ++c)
                        _sfte_grid_get_cell(term, c, logical_r)->dirty = 0;
            }
    } else
        out_dmg->w = 0, out_dmg->h = 0;
}

void sfte_get_ideal_size(const sfte_term *term, int16_t cols, int16_t rows, int32_t *out_w,
                         int32_t *out_h) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);

    if (out_w) *out_w = cols * vp->cell_width + (2 * SFTE_WINDOW_PAD_X);
    if (out_h) *out_h = (rows + layout.reserved_rows) * vp->cell_height + 2 * SFTE_WINDOW_PAD_Y;
}

void sfte_resize(sfte_ctx *ctx, int32_t w, int32_t h) {
    if (w <= 0 || h <= 0) return;

    sfte_term *term = sfte_term_get_active(ctx);
    sfte_window_state *win = &ctx->window;
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);

    win->width = w;
    win->height = h;
    win->pad_dirty = 1;

    int16_t new_cols = (w - (2 * SFTE_WINDOW_PAD_X)) / (int32_t)vp->cell_width;
    if (new_cols < 1) new_cols = 1;
    int16_t new_rows = (h - (2 * SFTE_WINDOW_PAD_Y)) / (int32_t)vp->cell_height;
    // Reserve space for the multiplexer bar if not floating above viewport
    if (new_rows > layout.reserved_rows) new_rows -= layout.reserved_rows;

    sfte_mux *mux = &ctx->mux;
    for (uint8_t i = 0; i < SFTE_MUX_MAX_WINDOWS; ++i) {
        sfte_term *t = &mux->terms[i];
        if (!t->is_allocated) continue;

        if (new_cols != t->viewport.cols || new_rows != t->viewport.rows) {
            _sfte_grid_resize(t, new_cols, new_rows);
#if SFTE_CURSOR_TRAIL
            t->cursor.trail.warp_tail = 1;
#endif  // SFTE_CURSOR_TRAIL
        } else {
            _sfte_grid_dirty_rows(t, -t->viewport.sb_off,
                                  -t->viewport.sb_off + t->viewport.rows - 1);
        }
    }
}

#if SFTE_FONT_ZOOM
void sfte_zoom(sfte_ctx *ctx, float delta) {
    if (delta == 0) return;

    sfte_window_state *win = &ctx->window;
    sfte_font_state *font = &ctx->font;

    float new_size = font->cur_size + delta;
    if (new_size < SFTE_FONT_MIN_SIZE || new_size > SFTE_FONT_MAX_SIZE) return;

    win->pad_dirty = 1;
    font->cur_size = new_size;

    _sfte_font_reset_cache(ctx);
    sfte_resize(ctx, win->width, win->height);
}
#endif  // SFTE_FONT_ZOOM
