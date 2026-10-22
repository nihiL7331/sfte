#pragma once

#include "src/mux.h"
#include "src/search.h"
#include <stdint.h>

#ifdef SFTE_XKB_COMMON
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>
#endif  // SFTE_XKB_COMMON

/*
    Converts physical pixel coordinates into discrete grid coordinates.
    `out_logical_row` includes scrollback offset (can be negative).
    `out_screen_row` is strictly clamped to the physical screen (0 to rows-1).
*/
static inline void _sfte_grid_from_px(const sfte_viewport_state *vp, int32_t px_x, int32_t px_y,
                                      int16_t *out_col, int32_t *out_logical_row,
                                      int16_t *out_screen_row) {
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);
    int32_t off_y = layout.grid_off_y;

    int16_t c = _SFTE_CLAMP((px_x - SFTE_WINDOW_PAD_X) / vp->cell_width, 0, vp->cols - 1);
    int16_t r = _SFTE_CLAMP((px_y - SFTE_WINDOW_PAD_Y - off_y) / vp->cell_height, 0, vp->rows - 1);

    if (out_col) *out_col = c;
    if (out_screen_row) *out_screen_row = r;
    if (out_logical_row) *out_logical_row = _sfte_grid_vis2log(vp, r);
}

// =================================================================================================
// >>input
// =================================================================================================

#if SFTE_INPUT_SELECTION
/*
    Determines if a specific cell falls within the active selection bounds.
    Evaluates against logical rows, meaning selections correclty scroll with text history.
*/
static inline uint8_t _sfte_input_is_selected(const sfte_mouse_state *mouse, int16_t col,
                                              int32_t row) {
    if (!mouse->sel_active) return 0;

    int16_t start_col = mouse->sel_start_col;
    int32_t start_row = mouse->sel_start_row;
    int16_t end_col = mouse->sel_end_col;
    int32_t end_row = mouse->sel_end_row;

    // Normalize backward drags
    if (start_row > end_row || (start_row == end_row && start_col > end_col)) {
        int32_t tmp_row = start_row;
        start_row = end_row, end_row = tmp_row;
        int16_t tmp_col = start_col;
        start_col = end_col, end_col = tmp_col;
    }

    if (row < start_row || row > end_row) return 0;
    if (start_row == end_row) return (col >= start_col && col <= end_col);  // same line
    if (row == start_row) return col >= start_col;                          // first line
    if (row == end_row) return col <= end_col;                              // last line
    return 1;                                                               // middle lines
}
#endif  // SFTE_INPUT_SELECTION

#if SFTE_INPUT_MOUSE
// Tracking modes (DECSET)
#define SFTE_INPUT_MOUSE_MODE_CLICK 1000   // Report button press/release only
#define SFTE_INPUT_MOUSE_MODE_DRAG 1002    // Report clicks and drag motion
#define SFTE_INPUT_MOUSE_MODE_MOTION 1003  // Report all hover and drag motion

// Formatting extensions (DECSET)
#define SFTE_INPUT_MOUSE_EXT_DEFAULT 0  // Legacy X10 encoding
#define SFTE_INPUT_MOUSE_EXT_SGR 1006   // Modern SGR encoding

// Encoding offsets
#define SFTE_INPUT_MOUSE_BTN_RELEASE 3     // The default "button released" state in legacy modes
#define SFTE_INPUT_MOUSE_MOTION_OFFSET 32  // Added to the button state to indicate motion/dragging
#define SFTE_INPUT_MOUSE_X10_OFFSET                                                                \
    32  // Added to coords to ensure they are printable ASCII characters
#define SFTE_INPUT_MOUSE_X10_MAX_COORD 223  // 255 (max byte) - 32 (offset)

/*
    Encodes and flushes mouse events back to the host application via terminal escape sequences.
    Supports both legacy X10 (max coords 223) and modern SGR 1006 formats.
    Must be fed `screen_r` coordinates, never `logical_r` coordinates.
*/
static inline void _sfte_input_send_mouse_event(sfte_term *term, uint8_t btn, uint8_t is_release,
                                                int16_t col, int16_t row, uint8_t is_motion) {
    const sfte_callbacks *cb = term->cb;
    const sfte_parser_state *parser = &term->parser;
    sfte_mouse_state *mouse = &term->mouse;

    if (!mouse->mode) return;

    uint8_t encoded_btn = btn;
    // Legacy modes cannot encode which button was released
    if (is_release && mouse->ext != SFTE_INPUT_MOUSE_EXT_SGR) {
        encoded_btn = SFTE_INPUT_MOUSE_BTN_RELEASE;
    }

    if (is_motion) {
        if (mouse->mode == SFTE_INPUT_MOUSE_MODE_DRAG &&
            parser->mouse_btn_state != SFTE_INPUT_MOUSE_BTN_RELEASE)
            encoded_btn = parser->mouse_btn_state + SFTE_INPUT_MOUSE_MOTION_OFFSET;  // Dragging
        else if (mouse->mode == SFTE_INPUT_MOUSE_MODE_MOTION)
            encoded_btn = parser->mouse_btn_state +
                          SFTE_INPUT_MOUSE_MOTION_OFFSET;  // Hover/dragging
        else
            return;  // Mode 1000 ignores motion
    }

    char buf[16];
    size_t len = 0;
    int16_t term_col = col + 1;
    int16_t term_row = row + 1;  // 1-based indexing for terminal escape sequences

    if (mouse->ext == SFTE_INPUT_MOUSE_EXT_SGR) {
        // SGR: ESC [ < btn ; x ; y M/m
        char end_char = is_release ? 'm' : 'M';
        len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c", encoded_btn, term_col, term_row,
                       end_char);
    } else {
        // X10: ESC [ <btn+32> <x+32> <y+32>
        if (term_col > SFTE_INPUT_MOUSE_X10_MAX_COORD || term_row > SFTE_INPUT_MOUSE_X10_MAX_COORD)
            return;
        len = snprintf(buf, sizeof(buf), "\033[M%c%c%c", encoded_btn + SFTE_INPUT_MOUSE_X10_OFFSET,
                       term_col + SFTE_INPUT_MOUSE_X10_OFFSET,
                       term_row + SFTE_INPUT_MOUSE_X10_OFFSET);
    }

    if (cb->write) cb->write(cb->user_data, buf, len);
}
#endif  // SFTE_INPUT_MOUSE

/*
    Contains the text input logic.
    Appends `text_input->buf` with `text` if applicable.
    If `key` is an arrow key, moves the cursor.
    If `key` is BackSpace, removes the character before cursor.

    Returns:
    - 1 if the key/text modified the buffer,
    - 0 otherwise.
*/
static inline uint8_t _sfte_input_text_handle(sfte_text_input_state *text_input, sfte_key key,
                                              const char *text, int8_t text_len) {
    if (key == SFTE_KEY_LEFT && text_input->cursor_byte_idx > 0) {
        do {
            text_input->cursor_byte_idx--;
        } while (text_input->cursor_byte_idx > 0 &&
                 (text_input->buf[text_input->cursor_byte_idx] & 0xC0) == 0x80);

        return 1;
    }

    if (key == SFTE_KEY_RIGHT && text_input->cursor_byte_idx < text_input->len) {
        do {
            text_input->cursor_byte_idx++;
        } while (text_input->cursor_byte_idx < text_input->len &&
                 (text_input->buf[text_input->cursor_byte_idx] & 0xC0) == 0x80);

        return 1;
    }

    if (key == SFTE_KEY_BACKSPACE && text_input->cursor_byte_idx > 0) {
        uint16_t old_idx = text_input->cursor_byte_idx;

        do {
            text_input->cursor_byte_idx--;
        } while (text_input->cursor_byte_idx > 0 &&
                 (text_input->buf[text_input->cursor_byte_idx] & 0xC0) == 0x80);

        uint16_t bytes = old_idx - text_input->cursor_byte_idx;
        memmove(text_input->buf + text_input->cursor_byte_idx, text_input->buf + old_idx,
                text_input->len - old_idx + 1);
        text_input->len -= bytes;

        return 1;
    }

    if (text_len > 0 && text_input->len + text_len < SFTE_INPUT_TEXT_MAX_LEN - 1) {
        memmove(text_input->buf + text_input->cursor_byte_idx + text_len,
                text_input->buf + text_input->cursor_byte_idx,
                text_input->len - text_input->cursor_byte_idx + 1);
        memcpy(text_input->buf + text_input->cursor_byte_idx, text, text_len);
        text_input->cursor_byte_idx += text_len;
        text_input->len += text_len;

        return 1;
    }

    return 0;
}

// =================================================================================================
// >>public api
// =================================================================================================
void sfte_input_text(sfte_ctx *ctx, const char *text, size_t len) {
    if (!ctx || !text || !len) return;

    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_callbacks *cb = term->cb;
    sfte_viewport_state *vp = &term->viewport;

#if SFTE_MUX
    sfte_mux *mux = &ctx->mux;

    if (mux->is_renaming) {
        if (_sfte_input_text_handle(&mux->text_input, SFTE_KEY_NONE, text, len)) {
            mux->ui_dirty = 1;
        }

        return;
    }
#endif  // SFTE_MUX

#if SFTE_SEARCH
    sfte_search_state *search = &term->search;

    if (search->is_active) {
        if (_sfte_input_text_handle(&search->text_input, SFTE_KEY_NONE, text, len)) {
            _sfte_search_exec(term, search->text_input.buf);
            search->ui_dirty = 1;
        }

        return;
    }
#endif  // SFTE_SEARCH

#if SFTE_TERM_SCROLLBACK_CAP
    if (vp->sb_off > 0) {
        vp->sb_off = 0;
        _sfte_grid_dirty_range(term, 0, vp->cols * vp->rows);
    }
#endif  // SFTE_TERM_SCROLLBACK_CAP

    if (cb->write) cb->write(cb->user_data, text, len);
}

uint8_t sfte_input_shortcut(sfte_ctx *ctx, sfte_key key, uint32_t mod_mask) {
    const sfte_shortcut _sfte_shortcuts[] = SFTE_SHORTCUTS;

#if SFTE_MUX
    sfte_mux *mux = &ctx->mux;
#endif  // SFTE_MUX

    uint8_t handled = 0;
    sfte_key lower_key = (key >= 'A' && key <= 'Z') ? (sfte_key)(key + ('a' - 'A')) : key;

    for (size_t i = 0; i < _SFTE_ARRAY_LEN(_sfte_shortcuts); ++i) {
        sfte_key sc_key = (sfte_key)_sfte_shortcuts[i].key;
        sfte_key lower_sc_key = (sc_key >= 'A' && sc_key <= 'Z') ? (sfte_key)(sc_key + ('a' - 'A'))
                                                                 : sc_key;
        if (lower_sc_key == lower_key && _sfte_shortcuts[i].mod_mask == mod_mask) {
            handled = _sfte_shortcuts[i].func(ctx, &_sfte_shortcuts[i].arg);
            break;
        }
    }

#if SFTE_MUX
    // If the shortcut returned 2, it's explicitly asking to keep the prefix active.
    // 2 is only returned by `_sfte_mux_prefix_shortcut`.
    if (mux->is_prefix_active && handled != 2) {
        mux->is_prefix_active = 0;
        if (handled == 0) handled = 1;  // Consume invalid post-prefix input
    }
#endif  // SFTE_MUX

    return handled ? 1 : 0;
}

void sfte_input_paste_begin(const sfte_term *term) {
    const sfte_callbacks *cb = term->cb;
    const sfte_modes_state *modes = &term->modes;

    if (!term || !cb->write || !modes->bracketed_paste) return;
    cb->write(cb->user_data, "\033[200~", 6);
}

void sfte_input_paste_end(const sfte_term *term) {
    const sfte_callbacks *cb = term->cb;
    const sfte_modes_state *modes = &term->modes;

    if (!term || !cb->write || !modes->bracketed_paste) return;
    cb->write(cb->user_data, "\033[201~", 6);
}

void sfte_input_key(sfte_ctx *ctx, sfte_key key, uint32_t mod_mask) {
    sfte_term *term = sfte_term_get_active(ctx);
    if (!ctx || !term) return;

#if SFTE_MUX
    sfte_mux *mux = &ctx->mux;

    if (mux->is_renaming) {
        if (key == SFTE_KEY_ESCAPE) {
            mux->is_renaming = 0;
            mux->ui_dirty = 1;
        }

        if (key == SFTE_KEY_ENTER) {
            mux->is_renaming = 0;
            mux->ui_dirty = 1;
            term->title_locked = 1;
            snprintf(term->title, sizeof(term->title), "%s", mux->text_input.buf);
        }

        if (key == SFTE_KEY_LEFT || key == SFTE_KEY_RIGHT || key == SFTE_KEY_BACKSPACE)
            if (_sfte_input_text_handle(&mux->text_input, key, NULL, 0)) { mux->ui_dirty = 1; }

        return;
    }
#endif  // SFTE_MUX

#if SFTE_SEARCH
    sfte_search_state *search = &term->search;

    if (search->is_active) {
        if (key == SFTE_KEY_LEFT || key == SFTE_KEY_RIGHT || key == SFTE_KEY_BACKSPACE)
            if (_sfte_input_text_handle(&search->text_input, key, NULL, 0)) {
                _sfte_search_exec(term, search->text_input.buf);
                search->ui_dirty = 1;
            }

        return;
    }
#endif  // SFTE_SEARCH

    char buf[128];
    size_t size = 0;

#if SFTE_INPUT_KITTY
    uint32_t codepoint = 0;
    size = _sfte_kitty_kb_encode(term, key, codepoint, mod_mask, buf, sizeof(buf));
    if (size > 0) {
        sfte_input_text(ctx, buf, size);
        return;
    }
#endif  // SFTE_INPUT_KITTY

    uint8_t csi_mod = 1;
    if (mod_mask & SFTE_MOD_SHIFT) csi_mod += 1;
    if (mod_mask & SFTE_MOD_ALT) csi_mod += 2;
    if (mod_mask & SFTE_MOD_CTRL) csi_mod += 4;
    // SFTE_MOD_SUPER not handled on purpose

    switch (key) {
    case SFTE_KEY_UP:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dA", csi_mod);
        else {
            memcpy(buf, "\033[A", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_DOWN:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dB", csi_mod);
        else {
            memcpy(buf, "\033[B", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_RIGHT:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dC", csi_mod);
        else {
            memcpy(buf, "\033[C", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_LEFT:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dD", csi_mod);
        else {
            memcpy(buf, "\033[D", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_HOME:
        memcpy(buf, "\033[H", 3);
        size = 3;
        break;
    case SFTE_KEY_END:
        memcpy(buf, "\033[F", 3);
        size = 3;
        break;
    case SFTE_KEY_BACKSPACE:
        buf[0] = (mod_mask & SFTE_MOD_ALT) ? '\033' : '\x7f';
        buf[1] = (mod_mask & SFTE_MOD_ALT) ? '\x7f' : '\0';
        size = (mod_mask & SFTE_MOD_ALT) ? 2 : 1;
        break;
    case SFTE_KEY_ENTER:
        buf[0] = '\r';
        size = 1;
        break;
    case SFTE_KEY_TAB:
        buf[0] = '\t';
        size = 1;
        break;
    case SFTE_KEY_ESCAPE:
        buf[0] = '\033';
        size = 1;
        break;
    case SFTE_KEY_PAGE_UP:
        memcpy(buf, "\033[5~", 4);
        size = 4;
        break;
    case SFTE_KEY_PAGE_DOWN:
        memcpy(buf, "\033[6~", 4);
        size = 4;
        break;
    case SFTE_KEY_F1:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dP", csi_mod);
        else {
            memcpy(buf, "\033OP", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_F2:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dQ", csi_mod);
        else {
            memcpy(buf, "\033OQ", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_F3:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dR", csi_mod);
        else {
            memcpy(buf, "\033OR", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_F4:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dS", csi_mod);
        else {
            memcpy(buf, "\033OS", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_F5:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[15;%d~", csi_mod);
        else {
            memcpy(buf, "\033[15~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F6:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[17;%d~", csi_mod);
        else {
            memcpy(buf, "\033[17~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F7:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[18;%d~", csi_mod);
        else {
            memcpy(buf, "\033[18~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F8:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[19;%d~", csi_mod);
        else {
            memcpy(buf, "\033[19~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F9:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[20;%d~", csi_mod);
        else {
            memcpy(buf, "\033[20~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F10:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[21;%d~", csi_mod);
        else {
            memcpy(buf, "\033[21~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F11:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[23;%d~", csi_mod);
        else {
            memcpy(buf, "\033[23~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F12:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[24;%d~", csi_mod);
        else {
            memcpy(buf, "\033[24~", 5);
            size = 5;
        }
        break;
    default: break;
    }

    if (size == 0 && key >= 32 && key < 127) {
        if (mod_mask & SFTE_MOD_CTRL) {
            if (key >= 'a' && key <= 'z') {
                buf[0] = key - 'a' + 1;
                size = 1;
            } else if (key >= 'A' && key <= 'Z') {
                buf[0] = key - 'A' + 1;
                size = 1;
            } else if (key == ' ') {
                buf[0] = '\0';
                size = 1;
            }
        } else if ((mod_mask & SFTE_MOD_ALT) && !(mod_mask & SFTE_MOD_SHIFT)) {
            buf[0] = (char)key;
            size = 1;
        }
    }

    // alt sends escape
    if (size > 0 && (mod_mask & SFTE_MOD_ALT) && buf[0] != '\033') {
        memmove(buf + 1, buf, size);
        buf[0] = '\033';
        size++;
    }

    sfte_input_text(ctx, buf, size);
}

#if SFTE_INPUT_MOUSE
void sfte_mouse_move(sfte_ctx *ctx, int32_t px_x, int32_t px_y) {
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_parser_state *parser = &term->parser;
    sfte_mouse_state *mouse = &term->mouse;
    const sfte_viewport_state *vp = &term->viewport;

#if SFTE_MUX_BAR
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);
    sfte_mux *mux = &ctx->mux;

    int32_t bar_y = layout.bar_y;

    if (px_y >= bar_y && px_y < bar_y + vp->cell_height) {
        int16_t hit_idx = -1;

        for (uint8_t i = 0; i < SFTE_MUX_MAX_WINDOWS; ++i) {
            if (!mux->terms[i].is_allocated) continue;

            if (px_x >= mux->tab_px_x[i] && px_x < mux->tab_px_x[i] + mux->tab_px_w[i]) {
                hit_idx = (int16_t)i;
                break;
            }
        }

        if (mux->hovered_idx != hit_idx) {
            mux->hovered_idx = hit_idx;
            mux->ui_dirty = 1;
        }

#if SFTE_INPUT_SELECTION
        if (!mouse->sel_dragging)
#endif  // SFTE_INPUT_SELECTION
        {
#if SFTE_MUX_BAR_HOVER
            if (hit_idx != -1) return;
#else   // !SFTE_MUX_BAR_HOVER
            return;
#endif  // !SFTE_MUX_BAR_HOVER
        }
    } else if (mux->hovered_idx != -1) {
        mux->hovered_idx = -1;
        mux->ui_dirty = 1;
    }
#endif  // SFTE_MUX_BAR

    int16_t c, screen_r;
    int32_t logical_r;
    _sfte_grid_from_px(vp, px_x, px_y, &c, &logical_r, &screen_r);

#if SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK_CAP
    if (mouse->sel_dragging && !mouse->mode) {
        int8_t new_dir = (screen_r <= 0) ? 1 : (screen_r >= vp->rows - 1) ? -1 : 0;
        if (new_dir != mouse->sel_scroll_dir) {
            mouse->sel_scroll_dir = new_dir;
            mouse->sel_last_scroll_ms = 0;
        }
    } else {
        mouse->sel_scroll_dir = 0;
    }
#endif  // SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK_CAP

    if (parser->mouse_hover_col == c && parser->mouse_hover_row == screen_r) return;

    parser->mouse_hover_col = c;
    parser->mouse_hover_row = screen_r;

    if (mouse->mode) {
        _sfte_input_send_mouse_event(term, parser->mouse_btn_state, 0, c, screen_r, 1);
        return;
    }

#if SFTE_INPUT_SELECTION
    if (!mouse->sel_dragging) return;

    if (mouse->sel_end_col == parser->mouse_hover_col && mouse->sel_end_row == logical_r) return;

    _sfte_grid_dirty_rows(term, mouse->sel_start_row, mouse->sel_end_row);
    mouse->sel_active = 1;
    mouse->sel_end_col = parser->mouse_hover_col;
    mouse->sel_end_row = logical_r;
    _sfte_grid_dirty_rows(term, mouse->sel_start_row, mouse->sel_end_row);
#endif  // SFTE_INPUT_SELECTION
}

void sfte_mouse_click(sfte_ctx *ctx, sfte_mouse_button btn, uint8_t pressed, int32_t px_x,
                      int32_t px_y) {
    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_callbacks *cb = term->cb;
    const sfte_viewport_state *vp = &term->viewport;
    sfte_parser_state *parser = &term->parser;
    sfte_mouse_state *mouse = &term->mouse;

#if SFTE_MUX_BAR
    const sfte_mux_layout layout = _sfte_mux_get_layout(vp);
    sfte_mux *mux = &ctx->mux;

    int32_t bar_y = layout.bar_y;

    if (px_y >= bar_y && px_y < bar_y + vp->cell_height) {
        int16_t hit_idx = -1;

        for (uint8_t i = 0; i < SFTE_MUX_MAX_WINDOWS; ++i) {
            if (!mux->terms[i].is_allocated) continue;

            if (px_x >= mux->tab_px_x[i] && px_x < mux->tab_px_x[i] + mux->tab_px_w[i]) {
                hit_idx = (int16_t)i;
                break;
            }
        }

        if (hit_idx != -1) {
            if (pressed && hit_idx != mux->active_idx)
                _SFTE_MUX_SWITCH_FOCUS(ctx, mux->active_idx, hit_idx);
            return;
        }

#if !SFTE_MUX_BAR_HOVER
        return;
#endif  // !SFTE_MUX_BAR_HOVER
    }
#endif  // SFTE_MUX_BAR

    int16_t c, screen_r;
    int32_t logical_r;
    _sfte_grid_from_px(vp, px_x, px_y, &c, &logical_r, &screen_r);

#if SFTE_INPUT_HYPERLINKS
    if (pressed && btn == SFTE_MOUSE_BUTTON_LEFT && cb->open_link) {
        const char *uri = sfte_get_link_at(term, c, logical_r);
        if (uri) {
            cb->open_link(cb->user_data, uri);
            return;
        }
    }
#endif  // SFTE_INPUT_HYPERLINKS

    if (mouse->mode) {
        if (pressed)
            parser->mouse_btn_state = btn;
        else
            parser->mouse_btn_state = SFTE_INPUT_MOUSE_BTN_RELEASE;

        _sfte_input_send_mouse_event(term, btn, !pressed, c, screen_r, 0);
        return;
    }

#if SFTE_INPUT_SELECTION
    if (pressed && btn == SFTE_MOUSE_BUTTON_LEFT) {
        if (mouse->sel_active)
            _sfte_grid_dirty_rows(term, mouse->sel_start_row, mouse->sel_end_row);

        parser->mouse_hover_col = c;
        parser->mouse_hover_row = screen_r;

        mouse->sel_start_col = c;
        mouse->sel_start_row = logical_r;
        mouse->sel_end_col = c;
        mouse->sel_end_row = logical_r;
        mouse->sel_active = 1;
        mouse->sel_dragging = 1;
#if SFTE_TERM_SCROLLBACK_CAP
        mouse->sel_scroll_dir = 0;
#endif  // SFTE_TERM_SCROLLBACK_CAP

        _sfte_grid_dirty_rows(term, mouse->sel_start_row, mouse->sel_end_row);
    } else {
        mouse->sel_dragging = 0;
#if SFTE_TERM_SCROLLBACK_CAP
        mouse->sel_scroll_dir = 0;
#endif  // SFTE_TERM_SCROLLBACK_CAP
        if (mouse->sel_start_col == mouse->sel_end_col &&
            mouse->sel_start_row == mouse->sel_end_row) {
            mouse->sel_active = 0;
            _sfte_grid_dirty_rows(term, mouse->sel_start_row, mouse->sel_end_row);
        }
    }
#endif  // SFTE_INPUT_SELECTION
}

void sfte_mouse_scroll(sfte_term *term, int8_t dir, int32_t px_x, int32_t px_y) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_mouse_state *mouse = &term->mouse;

    int16_t c, screen_r;
    _sfte_grid_from_px(vp, px_x, px_y, &c, NULL, &screen_r);

    if (mouse->mode) {
        uint8_t btn = (dir > 0) ? 64 : 65;
        _sfte_input_send_mouse_event(term, btn, 0, c, screen_r, 0);
    }

#if SFTE_TERM_SCROLLBACK_CAP
    sfte_view_scroll(term, (dir > 0) ? SFTE_TERM_SCROLL_STEP : -SFTE_TERM_SCROLL_STEP);
#endif  // SFTE_TERM_SCROLLBACK_CAP
}
#endif  // SFTE_INPUT_MOUSE

#if SFTE_INPUT_HYPERLINKS
const char *sfte_get_link_at(sfte_term *term, int16_t col, int16_t row) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_link_state *links = &term->links;

    if (col < 0 || col >= vp->cols || row < 0 || row >= vp->rows) return NULL;

    sfte_cell *c = _sfte_grid_get_cell(term, col, _sfte_grid_vis2log(vp, row));
    if (!c || c->link_idx == 0 || c->link_idx >= links->pool_len) return NULL;

    return links->pool[c->link_idx];
}
#endif  // SFTE_INPUT_HYPERLINKS

#if SFTE_INPUT_SELECTION
size_t sfte_get_selection(const sfte_term *term, char *out_buf, size_t max_bytes) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_mouse_state *mouse = &term->mouse;

    if (!mouse->sel_active) return 0;

    int16_t sc = mouse->sel_start_col, ec = mouse->sel_end_col;
    int32_t sr = mouse->sel_start_row, er = mouse->sel_end_row;

    // Normalize if dragging backwards
    if (sr > er || (sr == er && sc > ec)) {
        int32_t tmp_r = sr;
        sr = er, er = tmp_r;
        int16_t tmp_c = sc;
        sc = ec, ec = tmp_c;
    }

    size_t pos = 0;

#define _SFTE_WRITE_CHAR(c)                                                                        \
    do {                                                                                           \
        if (out_buf && pos < max_bytes - 1) out_buf[pos] = (c);                                    \
        pos++;                                                                                     \
    } while (0)

    for (int32_t logical_r = sr; logical_r <= er; ++logical_r) {
        int16_t row_start = (logical_r == sr) ? sc : 0;
        int16_t row_end = (logical_r == er) ? ec : vp->cols - 1;

        // Trim trailing spaces if the user selected past the end of text
        int16_t actual_end = row_end;
        if (actual_end == vp->cols - 1) {
            while (actual_end >= row_start) {
                sfte_cell *vcell = _sfte_grid_get_cell(term, actual_end, logical_r);
                if (vcell->rune != ' ' && vcell->rune != 0) break;
                actual_end--;
            }
        }

        for (int16_t c = row_start; c <= actual_end; ++c) {
            sfte_cell *vcell = _sfte_grid_get_cell(term, c, logical_r);
            sfte_rune rune = (vcell->rune && vcell->rune != ' ') ? vcell->rune : ' ';

// UTF-8 encoding
#if SFTE_TERM_ASCII_CHARSET
            _SFTE_WRITE_CHAR(rune);
#else   // !SFTE_TERM_ASCII_CHARSET
            if (rune < 0x80) {
                _SFTE_WRITE_CHAR(rune);
            } else if (rune < 0x800) {
                _SFTE_WRITE_CHAR(0xC0 | (rune >> 6));
                _SFTE_WRITE_CHAR(0x80 | (rune & 0x3F));
            } else if (rune < 0x10000) {
                _SFTE_WRITE_CHAR(0xE0 | (rune >> 12));
                _SFTE_WRITE_CHAR(0x80 | ((rune >> 6) & 0x3F));
                _SFTE_WRITE_CHAR(0x80 | (rune & 0x3F));
            } else {
                _SFTE_WRITE_CHAR(0xF0 | (rune >> 18));
                _SFTE_WRITE_CHAR(0x80 | ((rune >> 12) & 0x3F));
                _SFTE_WRITE_CHAR(0x80 | ((rune >> 6) & 0x3F));
                _SFTE_WRITE_CHAR(0x80 | (rune & 0x3F));
            }
#endif  // !SFTE_TERM_ASCII_CHARSET
        }

        // Inject newlines for multi-line selections, unless the line soft-wrapped
        if (logical_r < er) {
#if SFTE_TERM_REFLOW
            if (!_sfte_grid_get_cell(term, vp->cols - 1, logical_r)->wrapped)
#endif
                _SFTE_WRITE_CHAR('\n');
        }
    }
#undef _SFTE_WRITE_CHAR

    if (out_buf && max_bytes > 0) out_buf[pos < max_bytes ? pos : max_bytes - 1] = '\0';
    return pos + 1;
}
#endif  // SFTE_INPUT_SELECTION

#if SFTE_TERM_SCROLLBACK_CAP
void sfte_view_scroll(sfte_term *term, int32_t delta) {
#if SFTE_TERM_ALT_SCREEN
    const sfte_modes_state *modes = &term->modes;
#endif  // SFTE_TERM_ALT_SCREEN
    sfte_viewport_state *vp = &term->viewport;

#if SFTE_TERM_ALT_SCREEN
    if (modes->alt_active) return;
#endif  // SFTE_TERM_ALT_SCREEN

    int32_t new_off = vp->sb_off + delta;
    if (new_off < 0) new_off = 0;

    int32_t max_scroll = vp->sb_len < vp->sb_cap ? vp->sb_len : vp->sb_cap;
    if (new_off > max_scroll) new_off = max_scroll;

    if (new_off != vp->sb_off) {
        vp->sb_off = new_off;

        int32_t top_logical_r = -vp->sb_off;
        int32_t bot_logical_r = top_logical_r + vp->rows - 1;

        _sfte_grid_dirty_rows(term, top_logical_r, bot_logical_r);
    }
}
#endif  // SFTE_TERM_SCROLLBACK_CAP

#if SFTE_TERM_FOCUS
void sfte_set_focus(sfte_ctx *ctx, uint8_t focused) {
    if (!ctx || ctx->window.is_focused == focused) return;

    sfte_window_state *win = &ctx->window;
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_viewport_state *vp = &term->viewport;
    const sfte_callbacks *cb = term->cb;
    sfte_modes_state *modes = &term->modes;
    sfte_cursor_state *cursor = &term->cursor;

    win->is_focused = focused;

    if (!term || !term->is_allocated || !vp->cell_width) return;

#if SFTE_CURSOR_BLINK
    // Force the cursor to be visible when changing focus states
    cursor->blink_visible = 1;
    cursor->next_blink_ms = SFTE_TIME_MS() + SFTE_CURSOR_BLINK_RATE_MS;
#endif  // SFTE_CURSOR_BLINK

#if SFTE_TERM_SCROLL_SMOOTH
    if (!focused) {
        vp->scroll_y_off = 0.0f;
        vp->is_scrolling = 0;
    }
#endif  // SFTE_TERM_SCROLL_SMOOTH

    int16_t vis_col = cursor->col >= vp->cols ? vp->cols - 1 : cursor->col;
    _sfte_grid_get_cell(term, vis_col, cursor->row)->dirty = 1;

    if (modes->report_focus && cb->write) {
        if (focused)
            cb->write(cb->user_data, "\033[I", 3);
        else
            cb->write(cb->user_data, "\033[O", 3);
    }
}
#endif  // SFTE_TERM_FOCUS

#ifdef SFTE_XKB_COMMON
void sfte_xkb_process_key(sfte_ctx *ctx, struct xkb_state *state, uint32_t keycode) {
    if (!ctx || !state) return;

    xkb_keysym_t sym = xkb_state_key_get_one_sym(state, keycode);

    // Ignore standalone modifier keys
    if ((sym >= XKB_KEY_Shift_L && sym <= XKB_KEY_Hyper_R) || sym == XKB_KEY_ISO_Level3_Shift ||
        sym == XKB_KEY_Mode_switch)
        return;

    uint8_t ctrl = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE);
    uint8_t alt = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE);
    uint8_t shift = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT,
                                                 XKB_STATE_MODS_EFFECTIVE);
    uint8_t super = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO,
                                                 XKB_STATE_MODS_EFFECTIVE);

    uint8_t active_mods = SFTE_MOD_NONE;
    if (ctrl) active_mods |= SFTE_MOD_CTRL;
    if (alt) active_mods |= SFTE_MOD_ALT;
    if (shift) active_mods |= SFTE_MOD_SHIFT;
    if (super) active_mods |= SFTE_MOD_SUPER;

    sfte_key key_id = SFTE_KEY_NONE;

    switch (sym) {
    case XKB_KEY_Tab:
    case XKB_KEY_ISO_Left_Tab: key_id = SFTE_KEY_TAB; break;
    case XKB_KEY_Return:
    case XKB_KEY_Linefeed:
    case XKB_KEY_KP_Enter: key_id = SFTE_KEY_ENTER; break;
    case XKB_KEY_BackSpace: key_id = SFTE_KEY_BACKSPACE; break;
    case XKB_KEY_Escape: key_id = SFTE_KEY_ESCAPE; break;
    case XKB_KEY_Up: key_id = SFTE_KEY_UP; break;
    case XKB_KEY_Down: key_id = SFTE_KEY_DOWN; break;
    case XKB_KEY_Left: key_id = SFTE_KEY_LEFT; break;
    case XKB_KEY_Right: key_id = SFTE_KEY_RIGHT; break;
    case XKB_KEY_Home: key_id = SFTE_KEY_HOME; break;
    case XKB_KEY_End: key_id = SFTE_KEY_END; break;
    case XKB_KEY_Page_Up: key_id = SFTE_KEY_PAGE_UP; break;
    case XKB_KEY_Page_Down: key_id = SFTE_KEY_PAGE_DOWN; break;
    case XKB_KEY_Insert: key_id = SFTE_KEY_INSERT; break;
    case XKB_KEY_Delete: key_id = SFTE_KEY_DELETE; break;
    case XKB_KEY_F1: key_id = SFTE_KEY_F1; break;
    case XKB_KEY_F2: key_id = SFTE_KEY_F2; break;
    case XKB_KEY_F3: key_id = SFTE_KEY_F3; break;
    case XKB_KEY_F4: key_id = SFTE_KEY_F4; break;
    case XKB_KEY_F5: key_id = SFTE_KEY_F5; break;
    case XKB_KEY_F6: key_id = SFTE_KEY_F6; break;
    case XKB_KEY_F7: key_id = SFTE_KEY_F7; break;
    case XKB_KEY_F8: key_id = SFTE_KEY_F8; break;
    case XKB_KEY_F9: key_id = SFTE_KEY_F9; break;
    case XKB_KEY_F10: key_id = SFTE_KEY_F10; break;
    case XKB_KEY_F11: key_id = SFTE_KEY_F11; break;
    case XKB_KEY_F12: key_id = SFTE_KEY_F12; break;
    default: key_id = SFTE_KEY_NONE; break;
    }

    uint32_t cp = xkb_keysym_to_utf32(sym);
    sfte_key key = (sfte_key)((key_id != SFTE_KEY_NONE) ? key_id : cp);

    if (key > 0 && sfte_input_shortcut(ctx, key, active_mods)) return;

#if SFTE_INPUT_SELECTION
    sfte_term *term = sfte_term_get_active(ctx);
    if (term && term->mouse.sel_active && !term->mouse.sel_dragging) {
        term->mouse.sel_active = 0;
        _sfte_grid_dirty_rows(term, term->mouse.sel_start_row, term->mouse.sel_end_row);
    }
#endif  // SFTE_INPUT_SELECTION

    if (key_id != SFTE_KEY_NONE)
        // Mapped control key
        sfte_input_key(ctx, key_id, active_mods);
    else {
        if (cp > 0 && (active_mods & (SFTE_MOD_CTRL | SFTE_MOD_ALT | SFTE_MOD_SUPER)))
            // Modified text
            sfte_input_key(ctx, (sfte_key)cp, active_mods);
        else {
            // Pure typing
            char buf[64];
            int8_t size = (int8_t)xkb_state_key_get_utf8(state, keycode, buf, sizeof(buf));
            if (size > 0) sfte_input_text(ctx, buf, size);
        }
    }
}
#endif  // SFTE_XKB_COMMON
