#pragma once

#include "src/grid.h"
#include "src/types.h"
#include "src/util.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if SFTE_IMG_KITTY

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF  // NOTE: remove this with gif support added
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_THREAD_LOCALS
#include "vendor/stb_image.h"

#define _SFTE_KITTY_FMT_RGB 24
#define _SFTE_KITTY_FMT_RGBA 32
#define _SFTE_KITTY_FMT_PNG_JPEG 100

#endif  // SFTE_IMG_KITTY

#if SFTE_INPUT_KITTY
/*
    Generates the kitty keyboard (CSI u) or standard CSI escape sequence for a given key.
    Returns bytes written to `out_buf`, or `0` if inactive/unhandled.
    `key` is a backend-agnostic `sfte_key` enum. may be SFTE_KEY_KONE if key is purely text.
    `codepoint` is the raw UTF-32 character value of the key (if applicable).
    `mod_mask` is a bitmask of the active modifiers using SFTE_MOD_* definitions.
*/
static inline size_t _sfte_kitty_kb_encode(const sfte_term *term, sfte_key key, uint32_t codepoint,
                                           uint32_t mod_mask, char *out_buf, size_t max_bytes) {
#if SFTE_TERM_ALT_SCREEN
    const sfte_modes_state *modes = &term->modes;
#endif  // SFTE_TERM_ALT_SCREEN
    const sfte_keyboard_state *kb = &term->keyboard;

    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = modes->alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    uint16_t flags = kb->stack[s_idx][kb->stack_idx[s_idx]];
    if (flags == 0) return 0;

    uint8_t disambiguate = (flags & 1) != 0;
    uint8_t report_all = (flags & 8) != 0;

    uint32_t csi_mod = 1;
    if (mod_mask & SFTE_MOD_SHIFT) csi_mod += 1;
    if (mod_mask & SFTE_MOD_ALT) csi_mod += 2;
    if (mod_mask & SFTE_MOD_CTRL) csi_mod += 4;
    if (mod_mask & SFTE_MOD_SUPER) csi_mod += 8;

    // arrows, home, end, f1-f4 -> CSI 1 ; mods [char]
    char func_char = 0;
    switch (key) {
    case SFTE_KEY_UP: func_char = 'A'; break;
    case SFTE_KEY_DOWN: func_char = 'B'; break;
    case SFTE_KEY_RIGHT: func_char = 'C'; break;
    case SFTE_KEY_LEFT: func_char = 'D'; break;
    case SFTE_KEY_HOME: func_char = 'H'; break;
    case SFTE_KEY_END: func_char = 'F'; break;
    case SFTE_KEY_F1: func_char = 'P'; break;
    case SFTE_KEY_F2: func_char = 'Q'; break;
    case SFTE_KEY_F3: func_char = 'R'; break;
    case SFTE_KEY_F4: func_char = 'S'; break;
    default: break;
    }

    if (func_char) {
        if (csi_mod > 1 || disambiguate)
            return snprintf(out_buf, max_bytes, "\033[1;%u%c", csi_mod, func_char);
        return 0;
    }

    // insert, delete, pgup, pgdn, f5-f12 -> CSI num ; mods ~
    uint8_t tilde_num = 0;
    switch (key) {
    case SFTE_KEY_INSERT: tilde_num = 2; break;
    case SFTE_KEY_DELETE: tilde_num = 3; break;
    case SFTE_KEY_PAGE_UP: tilde_num = 5; break;
    case SFTE_KEY_PAGE_DOWN: tilde_num = 6; break;
    case SFTE_KEY_F5: tilde_num = 15; break;
    case SFTE_KEY_F6: tilde_num = 17; break;
    case SFTE_KEY_F7: tilde_num = 18; break;
    case SFTE_KEY_F8: tilde_num = 19; break;
    case SFTE_KEY_F9: tilde_num = 20; break;
    case SFTE_KEY_F10: tilde_num = 21; break;
    case SFTE_KEY_F11: tilde_num = 23; break;
    case SFTE_KEY_F12: tilde_num = 24; break;
    default: break;
    }

    if (tilde_num) {
        if (csi_mod > 1 || disambiguate)
            return snprintf(out_buf, max_bytes, "\033[%d;%u~", tilde_num, csi_mod);
        return 0;
    }

    // text keys and control keys -> CSI codepoint ; mods u
    uint32_t target_cp = codepoint ? codepoint : (uint32_t)key;
    // we can safely use SFTE_KEY_* as ASCII values
    if (target_cp > 0) {
        uint8_t is_control = (target_cp == SFTE_KEY_TAB || target_cp == SFTE_KEY_ENTER ||
                              target_cp == SFTE_KEY_ESCAPE || target_cp == SFTE_KEY_BACKSPACE);

        // Emit kitty sequence if:
        // - modifiers are pressed, or
        // - it's a special control key, or
        // - its requested all keys to be escaped.
        if (csi_mod > 1 || is_control || report_all)
            return snprintf(out_buf, max_bytes, "\033[%u;%uu", target_cp, csi_mod);
    }

    return 0;
}
#endif  // SFTE_INPUT_KITTY

#if SFTE_IMG_KITTY

/*
    Performs bilinear interpolation for image scaling.
    Kitty allows the terminal to dictate the final render size in rows/columns,
    this is the main purpose for this function.
    Uses the stack allocator to safely allocate the destination buffer,
    then slides it back over the source buffer to reclaim the memory.
*/
static inline uint32_t *_sfte_kitty_scale_image_bilinear(sfte_stack *stack, uint32_t *src,
                                                         int32_t src_wid, int32_t src_hei,
                                                         int32_t dst_wid, int32_t dst_hei) {
    size_t dst_bytes = dst_wid * dst_hei * sizeof(uint32_t);
    size_t pre_off = _sfte_mem_stack_save(stack);
    uint32_t *dst = (uint32_t *)_sfte_mem_stack_alloc(stack, dst_bytes, _Alignof(uint32_t));
    if (!dst) {
        _sfte_mem_stack_rewind(stack, pre_off);
        return NULL;
    }

    float x_ratio = ((float)(src_wid - 1)) / dst_wid;
    float y_ratio = ((float)(src_hei - 1)) / dst_hei;

    for (int32_t i = 0; i < dst_hei; ++i)
        for (int32_t j = 0; j < dst_wid; ++j) {
            int32_t x = (int32_t)(x_ratio * j);
            int32_t y = (int32_t)(y_ratio * i);
            float x_diff = (x_ratio * j) - x;
            float y_diff = (y_ratio * i) - y;
            // 4 nearest pixels
            size_t idx = y * src_wid + x;
            uint32_t p1 = src[idx];
            uint32_t p2 = (x + 1 < src_wid) ? src[idx + 1] : p1;
            uint32_t p3 = (y + 1 < src_hei) ? src[idx + src_wid] : p1;
            uint32_t p4 = (x + 1 < src_wid && y + 1 < src_hei) ? src[idx + src_wid + 1] : p1;
            // Calculate weights
            float w1 = (1.0f - x_diff) * (1.0f - y_diff);
            float w2 = x_diff * (1.0f - y_diff);
            float w3 = (1.0f - x_diff) * y_diff;
            float w4 = x_diff * y_diff;
            // Interpolate
            uint32_t r = (uint32_t)(((p1 >> 16) & 0xFF) * w1 + ((p2 >> 16) & 0xFF) * w2 +
                                    ((p3 >> 16) & 0xFF) * w3 + ((p4 >> 16) & 0xFF) * w4);
            uint32_t g = (uint32_t)(((p1 >> 8) & 0xFF) * w1 + ((p2 >> 8) & 0xFF) * w2 +
                                    ((p3 >> 8) & 0xFF) * w3 + ((p4 >> 8) & 0xFF) * w4);
            uint32_t b = (uint32_t)((p1 & 0xFF) * w1 + (p2 & 0xFF) * w2 + (p3 & 0xFF) * w3 +
                                    (p4 & 0xFF) * w4);
            uint32_t a = (uint32_t)(((p1 >> 24) & 0xFF) * w1 + ((p2 >> 24) & 0xFF) * w2 +
                                    ((p3 >> 24) & 0xFF) * w3 + ((p4 >> 24) & 0xFF) * w4);
            // Store temporarily in destination
            dst[i * dst_wid + j] = (a << 24) | (r << 16) | (g << 8) | b;
        }

    // Since the source image isn't needed anymore,
    // the scaled image can be moved to original image memory address.
    memmove(src, dst, dst_bytes);

    // Now we can reclaim the stack memory
    // that was used for scaled image before `memmove`.
    size_t src_off = (size_t)((uint8_t *)src - stack->buf);
    _sfte_mem_stack_rewind(stack, src_off + dst_bytes);

    return src;
}

/*
    Parses raw pixel buffers or delegates to stb_image for PNG/JPEG decoding.
    This kitty protocol implementation supports:
    - direct base64 pixel streams (via 'd'),
    - reading from a local file path (via 'f', used by e.g. yazi),
    - reading from a temporary file that the terminal is expected to delete after reading (via
   't').
*/
static inline uint32_t *_sfte_kitty_decode_payload(sfte_term *term, uint8_t *raw_data,
                                                   size_t raw_len, uint8_t is_file,
                                                   const char *file_path, int32_t *w, int32_t *h) {
    const sfte_kitty_state *kitty = &term->kitty;
    sfte_stack *stack = &term->stack;

    size_t pre_off = _sfte_mem_stack_save(stack);
    uint32_t *pxs = NULL;

    if (kitty->format == _SFTE_KITTY_FMT_PNG_JPEG) {
        int channels = 0;
        uint8_t *stb_pxs = is_file ? stbi_load(file_path, w, h, &channels, 4)
                                   : stbi_load_from_memory(raw_data, raw_len, w, h, &channels, 4);

        if (stb_pxs && *w && *h) {
            pxs = (uint32_t *)_sfte_mem_stack_alloc(stack, *w * *h * sizeof(uint32_t),
                                                    _Alignof(uint32_t));
            if (!pxs) {
                _sfte_mem_stack_rewind(stack, pre_off);
                stbi_image_free(stb_pxs);
                return NULL;
            }

            for (int64_t i = 0; i < *w * *h; ++i)
                pxs[i] = (stb_pxs[i * 4 + 3] << 24) | (stb_pxs[i * 4 + 0] << 16) |
                         (stb_pxs[i * 4 + 1] << 8) | stb_pxs[i * 4 + 2];
            stbi_image_free(stb_pxs);
        }
    } else if ((kitty->format == _SFTE_KITTY_FMT_RGB || kitty->format == _SFTE_KITTY_FMT_RGBA) &&
               *w && *h) {
        uint8_t bpp = (kitty->format == _SFTE_KITTY_FMT_RGB) ? 3 : 4;
        pxs = (uint32_t *)_sfte_mem_stack_alloc(stack, *w * *h * sizeof(uint32_t),
                                                _Alignof(uint32_t));
        if (!pxs) {
            _sfte_mem_stack_rewind(stack, pre_off);
            return NULL;
        }
        size_t post_pxs_off = _sfte_mem_stack_save(stack);

        uint8_t *pixel_src = raw_data;
        size_t pixel_len = raw_len;

        if (is_file) {
            FILE *f = fopen(file_path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                pixel_len = ftell(f);
                fseek(f, 0, SEEK_SET);

                if (pixel_len >= (size_t)(*w * *h * bpp)) {
                    pixel_src = (uint8_t *)_sfte_mem_stack_alloc(stack, pixel_len,
                                                                 _Alignof(uint8_t));
                    if (!pixel_src) {
                        _sfte_mem_stack_rewind(stack, pre_off);
                        fclose(f);
                        return NULL;
                    }
                    (void)fread(pixel_src, 1, pixel_len, f);
                } else
                    pixel_src = NULL;
                fclose(f);
            } else
                pixel_src = NULL;
        }

        if (pixel_src && pixel_len >= (size_t)(*w * *h * bpp)) {
            for (int64_t i = 0; i < *w * *h; ++i) {
                uint8_t a = (bpp == 4) ? pixel_src[i * bpp + 3] : 255;
                pxs[i] = (a << 24) | (pixel_src[i * bpp + 0] << 16) |
                         (pixel_src[i * bpp + 1] << 8) | pixel_src[i * bpp + 2];
            }

            _sfte_mem_stack_rewind(stack, post_pxs_off);
        } else {
            _sfte_mem_stack_rewind(stack, pre_off);
            pxs = NULL;
        }
    }
    return pxs;
}

/*
    Applies requested croppping limits before placing the image.
*/
static inline uint32_t *_sfte_kitty_apply_crop(sfte_term *term, size_t pre_pxs_off, uint32_t *pxs,
                                               int32_t *w, int32_t *h) {
    const sfte_kitty_state *kitty = &term->kitty;
    sfte_stack *stack = &term->stack;

    int32_t cx = _SFTE_CLAMP(kitty->crop_x, 0, *w);
    int32_t cy = _SFTE_CLAMP(kitty->crop_y, 0, *h);
    int32_t cw = kitty->crop_w ? kitty->crop_w : (*w - cx);
    int32_t ch = kitty->crop_h ? kitty->crop_h : (*h - cy);
    if (cx + cw > *w) cw = *w - cx;
    if (cy + ch > *h) ch = *h - cy;
    if (cw <= 0 || ch <= 0) {
        _sfte_mem_stack_rewind(stack, pre_pxs_off);
        return NULL;
    }
    // If only cropped from bottom, the pointer doesn't change
    // so we can return immediately without any `memmove` calls.
    if (cx == 0 && cy == 0 && cw == *w) {
        *h = ch;
        return pxs;
    }

    if (cw == *w && ch == *h && !cx && !cy) return pxs;

    // Since for a pixel at coordinates (x,y) in cropped image:
    // crop_idx = (y * crop_w) + x
    // orig_idx = ((crop_y + y) * orig_w) + crop_x + x
    // where:
    // crop_x, crop_y >= 0
    // crop_w <= orig_w
    // we know that:
    // crop_idx <= orig_idx
    // so we can crop in place iterating over original image memory.
    // The original image index "outruns" the cropped image index.
    for (int32_t y = 0; y < ch; ++y)
        memmove(&pxs[y * cw], &pxs[(cy + y) * *w + cx], cw * sizeof(uint32_t));

    *w = cw;
    *h = ch;
    _sfte_mem_stack_rewind(stack, pre_pxs_off + (cw * ch * sizeof(uint32_t)));
    return pxs;
}

/*
    Translates terminal cell bounds (columns/rows) into physical
    pixel dimensions, and scales the raster to match.
*/
static inline uint32_t *_sfte_kitty_apply_scale(sfte_term *term, uint32_t *pxs, int32_t *w,
                                                int32_t *h) {
    const sfte_kitty_state *kitty = &term->kitty;
    const sfte_viewport_state *vp = &term->viewport;
    sfte_stack *stack = &term->stack;

    if (kitty->cols <= 0 && kitty->rows <= 0) return pxs;
    int32_t target_w = *w;
    int32_t target_h = *h;
    if (kitty->cols && !kitty->rows) {
        target_w = kitty->cols * vp->cell_width;
        target_h = (target_w * *h) / *w;  // Maintain aspect ratio
    } else if (!kitty->cols && kitty->rows) {
        target_h = kitty->rows * vp->cell_height;
        target_w = (target_h * *w) / *h;  // Maintain aspect ratio
    } else {
        target_w = kitty->cols * vp->cell_width;
        target_h = kitty->rows * vp->cell_height;
    }

    if (target_w <= 0 || target_h <= 0 || (target_w == *w && target_h == *h)) return pxs;

    uint32_t *scaled = _sfte_kitty_scale_image_bilinear(stack, pxs, *w, *h, target_w, target_h);
    if (!scaled) return NULL;

    *w = target_w;
    *h = target_h;
    return pxs;
}

/*
    Evaluates the kitty deletion matrix to determine if a specific
    placement should be destroyed. This protocol implementation allows wiping by:
    - ID,
    - Z index,
    - viewport intersection,
    - specific cursor locations.
*/
static inline uint8_t _sfte_kitty_should_delete(const sfte_term *term, sfte_img_placement *p,
                                                sfte_img *img) {
    const sfte_kitty_state *kitty = &term->kitty;
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;

    uint8_t matches_id = (!kitty->id || kitty->id == p->img_id);
    if (!matches_id) return 0;

    int16_t cols = _sfte_grid_span(img->width, p->x_off, vp->cell_width);
    int16_t rows = _sfte_grid_span(img->height, p->y_off, vp->cell_height);

    int16_t target_c = kitty->crop_x - 1;
    int16_t target_r = kitty->crop_y - 1;

    uint8_t intersects_x = (target_c >= p->start_col && target_c < p->start_col + cols);
    uint8_t intersects_y = (target_r >= p->start_row && target_r < p->start_row + rows);

    switch (kitty->d_action) {
    case 'A':
    case 'a': return 1;  // Delete all
    case 'I':
    case 'i': return !kitty->placement_id || kitty->placement_id == p->placement_id;
    case 'C':
    case 'c':  // Delete if intersecting cursor
        return cursor->col >= p->start_col && cursor->col < p->start_col + cols &&
               cursor->row >= p->start_row && cursor->row < p->start_row + rows;
    case 'P':
    case 'p': return intersects_x && intersects_y;
    case 'X':
    case 'x': return intersects_x;
    case 'Y':
    case 'y': return intersects_y;
    case 'Z':
    case 'z': return p->z_idx == kitty->z_idx;
    case 'Q':
    case 'q': return intersects_x && intersects_y && p->z_idx == kitty->z_idx;
    default: return 0;
    }
}

/*
    Garbage collection via a simple reference counting.
    The image data is separated from image placements, we only free the image data
    when its `ref_cnt` drops to 0 (no placements are actively displaying it).
*/
static inline void _sfte_kitty_gc_pool(sfte_term *term) {
    const sfte_kitty_state *kitty = &term->kitty;
    sfte_image_state *images = &term->images;

    for (uint32_t i = 0; i < images->pool_len; ++i) {
        sfte_img *img = &images->pool[i];
        if (img->is_sixel || img->ref_cnt) continue;  // Skip active and sixel
        uint8_t should_del = (kitty->d_action == 'A' || kitty->d_action == 'a') ||
                             ((kitty->d_action == 'I' || kitty->d_action == 'i') &&
                              img->id == kitty->id);
        if (!should_del) continue;
        if (img->pixels) SFTE_FREE(img->pixels);
        images->pool[i--] = images->pool[--images->pool_len];
    }
}

/*
    Registers a new viewport placement for an existing image buffer,
    incrementing its reference count.

    Returns a error message used for acknowledgements if 'img' is NULL or placement pool is OOM.
    Returns NULL on success.
*/
static inline const char *_sfte_kitty_apply_placement(sfte_term *term, sfte_img *img,
                                                      const sfte_config *cfg) {
    const sfte_kitty_state *kitty = &term->kitty;
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;
#if SFTE_TERM_ALT_SCREEN
    const sfte_modes_state *modes = &term->modes;
#endif  // SFTE_TERM_ALT_SCREEN
    sfte_image_state *images = &term->images;

    if (!img) return "EINVAL: cannot apply placement to NULL image";

    sfte_img_placement *p = _sfte_img_placement_insert(images,
                                                       (sfte_img_placement){
                                                           .img_id = img->id,
                                                           .placement_id = kitty->placement_id,
                                                           .start_col = cursor->col,
                                                           .start_row = cursor->row,
                                                           .x_off = kitty->x_off,
                                                           .y_off = kitty->y_off,
                                                           .z_idx = kitty->z_idx,
#if SFTE_TERM_ALT_SCREEN
                                                           .alt_screen = modes->alt_active,
#endif  // SFTE_TERM_ALT_SCREEN
                                                       });

    if (!p) return "ENOMEM: placement pool capacity reached";

    img->ref_cnt++;
    int16_t cols = _sfte_grid_span(img->width, kitty->x_off, vp->cell_width);
    int16_t rows = _sfte_grid_span(img->height, kitty->y_off, vp->cell_height);
    _sfte_grid_dirty_rect(term, cursor->col, cursor->row, cols, rows, cfg);
    return NULL;
}

/*
    Queries the terminals image support or interrogates the status of a specific image ID.
    If the terminal successfully parses the request, it replies with an OK status.

    Always returns NULL.
*/
static inline const char *_sfte_kitty_exec_query(const sfte_term *term) {
    const sfte_kitty_state *kitty = &term->kitty;
    const sfte_callbacks *cb = term->cb;

    char reply[64];
    size_t len = snprintf(reply, sizeof(reply), "\033_Gi=%u;OK\033\\", kitty->id);
    if (cb->write) cb->write(cb->user_data, reply, len);
    return NULL;
}

/*
    Iterates through active image placements and removes those that match the requested
    deletion criteria passed via a kitty sequence.

    Returns a error message used for acknowledgements if no image is found to delete.
    Returns NULL on success.
*/
static inline const char *_sfte_kitty_exec_delete(sfte_term *term, const sfte_config *cfg) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_image_state *images = &term->images;
#if SFTE_TERM_ALT_SCREEN
    const sfte_modes_state *modes = &term->modes;
#endif  // SFTE_TERM_ALT_SCREEN

    for (uint32_t i = 0; i < images->placements_len; ++i) {
        sfte_img_placement *p = &images->placements[i];
#if SFTE_TERM_ALT_SCREEN
        if (p->alt_screen != modes->alt_active) continue;
#endif  // SFTE_TERM_ALT_SCREEN
        if (p->is_sixel) continue;

        sfte_img *img = _sfte_img_find(images, p->img_id);
        if (!img) return "EINVAL: failed to find image to delete";

        if (_sfte_kitty_should_delete(term, p, img)) {
            img->ref_cnt--;
            int16_t cols = _sfte_grid_span(img->width, p->x_off, vp->cell_width);
            int16_t rows = _sfte_grid_span(img->height, p->y_off, vp->cell_height);

            _sfte_grid_dirty_rect(term, p->start_col, p->start_row, cols, rows, cfg);

            images->placements[i--] = images->placements[--images->placements_len];
        }
    }

    _sfte_kitty_gc_pool(term);
    return NULL;
}

/*
    Decodes the accumulated base64 payload into raw pixels, applies any requested
    cropping and scaling, and stores the final raster in the image pool.

    WARN:
    If the transmission medium is 't', the client sent a path to a temp file containing the
   pixels. The protocol dictates that the emulator assumes ownership of this file and MUST
   delete it after decoding to prevent disk leaks.

    Returns a error message used for acknowledgements if the base64 decode fails.
    Returns NULL on success.
*/
static inline const char *_sfte_kitty_exec_transmit(sfte_term *term, sfte_img **out_img) {
    sfte_kitty_state *kitty = &term->kitty;
    sfte_stack *stack = &term->stack;
    sfte_image_state *images = &term->images;

    size_t raw_len = 0;
    size_t pre_b64_off = _sfte_mem_stack_save(stack);
    uint8_t *raw_data = _sfte_b64_decode(stack, (uint8_t *)kitty->b64_buf, kitty->b64_len,
                                         &raw_len);
    if (!raw_data) return "ENOMEM: base64 decode failed";

    uint8_t is_file = (kitty->t_medium == 'f' || kitty->t_medium == 't');
    char *file_path = NULL;
    if (is_file) {
        file_path = (char *)_sfte_mem_stack_alloc(stack, raw_len + 1, _Alignof(char));
        if (!file_path) {
            _sfte_mem_stack_rewind(stack, pre_b64_off);
            return "ENOMEM: file path allocation failed";
        }

        memcpy(file_path, raw_data, raw_len);
        file_path[raw_len] = '\0';
    }

    int32_t w = kitty->width;
    int32_t h = kitty->height;
    size_t pre_pxs_off = _sfte_mem_stack_save(stack);
    uint32_t *pixels = _sfte_kitty_decode_payload(term, raw_data, raw_len, is_file, file_path, &w,
                                                  &h);

    if (kitty->t_medium == 't' && is_file) remove(file_path);
    if (!pixels) return "EBADFMT: failed to decode image data";

    pixels = _sfte_kitty_apply_crop(term, pre_pxs_off, pixels, &w, &h);
    if (!pixels) {
        _sfte_mem_stack_rewind(stack, pre_b64_off);
        return "EINVAL: invalid crop dimensions";
    }

    pixels = _sfte_kitty_apply_scale(term, pixels, &w, &h);
    if (!pixels) {
        _sfte_mem_stack_rewind(stack, pre_b64_off);
        return "ENOMEM: scaling failed";
    }

    uint32_t *final_pixels = (uint32_t *)SFTE_MALLOC(w * h * sizeof(uint32_t));
    if (!final_pixels) {
        _sfte_mem_stack_rewind(stack, pre_b64_off);
        return "ENOMEM: failed to allocate presistent image buffer";
    }

    memcpy(final_pixels, pixels, w * h * sizeof(uint32_t));
    _sfte_mem_stack_rewind(stack, pre_b64_off);

    if (!kitty->id) kitty->id = ++images->next_id;
    sfte_img *new_img = _sfte_img_pool_insert(images, (sfte_img){
                                                          .pixels = final_pixels,
                                                          .id = kitty->id,
                                                          .width = w,
                                                          .height = h,
                                                      });

    if (!new_img) {
        SFTE_FREE(final_pixels);
        return "ENOMEM: image pool capacity reached";
    }

    if (out_img) *out_img = new_img;
    return NULL;
}

/*
    Creates a new visual placement on the grid for an image already residing in the pool.

    Returns a error message used for acknowledgements if the requested image ID
    was never transmitted or has been garbage collected, OR if `_sfte_kitty_apply_placement`
   fails. Returns NULL on success.
*/
static inline const char *_sfte_kitty_exec_place(sfte_term *term, const sfte_config *cfg) {
    const sfte_kitty_state *kitty = &term->kitty;
    sfte_image_state *images = &term->images;

    for (uint32_t j = 0; j < images->pool_len; ++j)
        if (images->pool[j].id == kitty->id)
            return _sfte_kitty_apply_placement(term, &images->pool[j], cfg);
    return "ENOENT: image id not found in pool";
}

/*
    Sends an acknowledgement response back to the client application.
    The `q` parameter dictates response verbosity.
    - q=0: Send a response for both success and failure.
    - q=1: Send a response ONLY on failure (defalut).
    - q=2: Silent.
*/
static inline void _sfte_kitty_send_ack(const sfte_term *term, const char *err_msg) {
    const sfte_kitty_state *kitty = &term->kitty;
    const sfte_callbacks *cb = term->cb;

    if (err_msg && (kitty->quiet == 0 || kitty->quiet == 1)) {
        char reply[256];
        size_t len = kitty->id > 0
                         ? snprintf(reply, sizeof(reply), "\033_Gi=%u;%s\033\\", kitty->id, err_msg)
                         : snprintf(reply, sizeof(reply), "\033_G;%s\033\\", err_msg);
        if (cb->write) cb->write(cb->user_data, reply, len);
    } else if (!err_msg && !kitty->quiet) {
        char reply[64];
        size_t len = kitty->id > 0
                         ? snprintf(reply, sizeof(reply), "\033_Gi=%u;OK\033\\", kitty->id)
                         : snprintf(reply, sizeof(reply), "\033_G;OK\033\\");
        if (cb->write) cb->write(cb->user_data, reply, len);
    }
}

/*
    The main entrypoint for kitty graphics sequences.
    Because terminal parsers usually have hard limits on escape sequence length,
    kitty protocol can send big image files across multiple escape sequences.

    The `m=1` parameter indicates that more data is coming.
    The base64 string is accumulated in `kitty->b64_buf` across multiple calls,
    and the evalutaion is triggered the moment a sequence with `m=0` is received.
*/
static inline void _sfte_kitty_parse_graphics(sfte_term *term, const char *payload,
                                              const sfte_config *cfg) {
    sfte_kitty_state *kitty = &term->kitty;

    const char *semi = strchr(payload, ';');
    // if there's no semicolon, the dictionary spans the entire payload
    const char *dict_end = semi ? semi : payload + strlen(payload);

    uint8_t more = 0;
    uint8_t is_new = 0;

    // Lookahead to check if its a new transmission
    for (const char *p = payload; p < semi; ++p) {
        if (*p == 'a' || *p == 'f' || *p == 'i' || *p == 's' || *p == 'v' || *p == 'z')
            if (p + 1 < dict_end && p[1] == '=') {
                is_new = 1;
                break;
            }
    }

    // reset state if fresh
    if (is_new || !kitty->action) {
        char *saved_buf = kitty->b64_buf;
        size_t saved_cap = kitty->b64_cap;
        *kitty = (sfte_kitty_state){
            .b64_buf = saved_buf,
            .b64_cap = saved_cap,
            .format = _SFTE_KITTY_FMT_RGBA,
            .quiet = 1,
            .action = 'T',
            .t_medium = 'd',
            // rest 0-initialized
        };
    }

    // parse params into state
    const char *p = payload;
    while (p && p < dict_end) {
        char key = p[0];
        if (p + 1 >= dict_end || p[1] != '=') break;
        const char *val = p + 2;

        switch (key) {
        case 'a': kitty->action = val[0]; break;
        case 'c': kitty->cols = (int16_t)atoi(val); break;
        case 'd': kitty->d_action = val[0]; break;
        case 'f': kitty->format = (uint8_t)atoi(val); break;
        case 'h': kitty->crop_h = (int32_t)atoi(val); break;
        case 'i': kitty->id = (uint32_t)atoi(val); break;
        case 'm': more = (uint8_t)atoi(val); break;
        case 'p': kitty->placement_id = (uint32_t)atoi(val); break;
        case 'q': kitty->quiet = (uint8_t)atoi(val); break;
        case 'r': kitty->rows = (int16_t)atoi(val); break;
        case 's': kitty->width = (int32_t)atoi(val); break;
        case 't': kitty->t_medium = val[0]; break;
        case 'v': kitty->height = (int32_t)atoi(val); break;
        case 'w': kitty->crop_w = (int32_t)atoi(val); break;
        case 'x': kitty->crop_x = (int32_t)atoi(val); break;
        case 'y': kitty->crop_y = (int32_t)atoi(val); break;
        case 'z': kitty->z_idx = (int8_t)atoi(val); break;
        case 'X': kitty->x_off = (int16_t)atoi(val); break;
        case 'Y': kitty->y_off = (int16_t)atoi(val); break;
        default: break;
        }

        p = strchr(p, ',');
        if (!p || p >= dict_end) break;
        p++;  // skip comma
    }

    if (semi) {
        const char *b64_start = semi + 1;
        size_t b64_len = strlen(b64_start);

        uint8_t oom = 0;
        _SFTE_MEM_ENSURE_CAP(char, kitty->b64_buf, kitty->b64_len, kitty->b64_cap, b64_len + 1,
                             SFTE_KITTY_B64_INIT_CAP, SFTE_KITTY_B64_MAX_CAP, oom);

        if (oom) return;

        memcpy(kitty->b64_buf + kitty->b64_len, b64_start, b64_len);
        kitty->b64_len += b64_len;
    }

    if (more) return;  // Abort and wait for next sequence

    kitty->b64_buf[kitty->b64_len] = '\0';
    const char *err_msg = NULL;

    switch (kitty->action) {
    case 'q': err_msg = _sfte_kitty_exec_query(term); break;
    case 'd': err_msg = _sfte_kitty_exec_delete(term, cfg); break;
    case 't': err_msg = _sfte_kitty_exec_transmit(term, NULL); break;
    case 'T': {
        sfte_img *img = NULL;
        err_msg = _sfte_kitty_exec_transmit(term, &img);
        if (err_msg || !img) break;
        _sfte_kitty_apply_placement(term, img, cfg);
        break;
    }
    case 'p': _sfte_kitty_exec_place(term, cfg); break;
    default: err_msg = "EINVAL: unknown action"; break;
    }

    _sfte_kitty_send_ack(term, err_msg);

    kitty->b64_len = 0;
}

static inline void _sfte_kitty_deinit(sfte_kitty_state *kitty) {
    SFTE_FREE(kitty->b64_buf);
    kitty->b64_buf = NULL;
}
#endif  // SFTE_IMG_KITTY
