#pragma once

#include "src/config.h"
#include "src/grid.h"
#include "src/search.h"
#include "src/types.h"
#include "src/util.h"
#if SFTE_WAYLAND
#include "vendor/xdg-shell.h"
#endif  // SFTE_WAYLAND

typedef enum {
    VT_GROUND,     // normal
    VT_ESCAPE,     // \033
    VT_CSI_ENTRY,  // \033[
    VT_CSI_PARAM,  // nums
    VT_OSC,        // \033]
    VT_CHARSET,    // \033( \033)
    VT_HASH,       // #
    VT_DCS,        // P / _ / ^
#if SFTE_IMG_SIXEL
    VT_SIXEL,
#endif  // SFTE_IMG_SIXEL
} sfte_vt_state;

/*
    Feeds a single byte into the UTF-8 state machine.
    Returns 1 if a complete rune has been successfully decoded.
    Returns 0 if more bytes are needed, or if an invalid sequence was encountered (which aborts
   the current sequence and resets the state machine).

    If `SFTE_TERM_ASCII_CHARSET` is 1, skips UTF8 bytes to ensure they don't break the layout.
*/
static inline uint8_t _sfte_rune_utf8_decode(sfte_parser_state *parser, uint8_t b) {
    if (parser->utf8_bytes_left > 0) {
        if ((b & 0xC0) == 0x80) {  // Continuation byte
#if !SFTE_TERM_ASCII_CHARSET
            parser->utf8_rune_acc = (parser->utf8_rune_acc << 6) | (b & 0x3F);
#endif  // !SFTE_TERM_ASCII_CHARSET
            parser->utf8_bytes_left--;
            if (!parser->utf8_bytes_left) return 1;
            return 0;
        } else
            parser->utf8_bytes_left = 0;  // Invalid sequence, abort
    }

#if !SFTE_TERM_ASCII_CHARSET
    // Update UTF-8 rune accumulator
    if ((b & 0x80) == 0x00)
        parser->utf8_rune_acc = b;
    else if ((b & 0xE0) == 0xC0)
        parser->utf8_rune_acc = b & 0x1F;
    else if ((b & 0xF0) == 0xE0)
        parser->utf8_rune_acc = b & 0x0F;
    else if ((b & 0xF8) == 0xF0)
        parser->utf8_rune_acc = b & 0x07;
#endif  // !SFTE_TERM_ASCII_CHARSET

    // Get UTF-8 byte count
    if ((b & 0x80) == 0x00) {
        parser->utf8_bytes_left = 0;
        return 1;
    } else if ((b & 0xE0) == 0xC0)
        parser->utf8_bytes_left = 1;
    else if ((b & 0xF0) == 0xE0)
        parser->utf8_bytes_left = 2;
    else if ((b & 0xF8) == 0xF0)
        parser->utf8_bytes_left = 3;

    return 0;
}

// =================================================================================================
// >>csi
// =================================================================================================

/*
    VT500 parameters default to 1 if omitted or set to 0.
*/
#define _SFTE_P(val) ((val) > 0 ? (val) : 1)

/*
    Converts 1-based VT500 coordinates to 0-based array indices.
*/
#define _SFTE_P_IDX(val) (_SFTE_P(val) - 1)

/*
    Unpacks a 24-bit TrueColor RGB sequence from the parameter array.

    NOTE:
    It's not wrapped in `SFTE_COLOR_TRUECOLOR`,
    because this value is converted to the closest 256-color palette value
    if TrueColor is disabled.
*/
static inline uint32_t _sfte_csi_parse_truecolor(uint16_t *p, uint16_t i) {
    return SFTE_COLOR_ALPHA_MASK | (p[i + 2] << 16) | (p[i + 3] << 8) | p[i + 4];
}

/*
    Handles Insert Character / CSI @.

    Inserts n spaces at the cursor, shifting text right.
*/
static inline void _sfte_csi_exec_ich(sfte_term *term, uint16_t *p, int16_t col) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;

    uint16_t n = _SFTE_P(p[0]);
    int16_t rem = vp->cols - col;
    if (n > rem) n = rem;

    int16_t move_cnt = rem - n;
    int32_t base_idx = _sfte_grid_get_idx(vp, 0, cursor->row);
    if (move_cnt > 0)
        memmove(&term->cells[base_idx + col + n], &term->cells[base_idx + col],
                move_cnt * sizeof(sfte_cell));

    _sfte_grid_clear_cells(term, base_idx + col, n);
    _sfte_grid_dirty_range(term, base_idx + col, rem);
}

/*
    Handles Cursor Next Line / CSI E.

    Moves cursor to the beginning of the line n lines down.
*/
static inline void _sfte_csi_exec_cnl(sfte_term *term, uint16_t *p) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;

    cursor->col = 0;
    cursor->row = _SFTE_CLAMP(cursor->row + _SFTE_P(p[0]), 0, vp->rows - 1);
}

/*
    Handles Cursor Previous Line / CSI F.

    Moves cursor to the beginning of the line n lines up.
*/
static inline void _sfte_csi_exec_cpl(sfte_term *term, uint16_t *p) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;

    cursor->col = 0;
    cursor->row = _SFTE_CLAMP(cursor->row - _SFTE_P(p[0]), 0, vp->rows - 1);
}

/*
    Handles Erase in Display / CSI J.
*/
static inline void _sfte_csi_exec_ed(sfte_term *term, int16_t mode, int16_t col) {
#if SFTE_TERM_SCROLLBACK_CAP && (SFTE_IMG_SIXEL || SFTE_IMG_KITTY) && SFTE_TERM_ALT_SCREEN
    const sfte_modes_state *modes = &term->modes;
#endif  // SFTE_TERM_SCROLLBACK_CAP && (SFTE_IMG_SIXEL || SFTE_IMG_KITTY) && SFTE_TERM_ALT_SCREEN
    sfte_cursor_state *cursor = &term->cursor;
    sfte_viewport_state *vp = &term->viewport;
#if SFTE_TERM_SCROLLBACK_CAP && (SFTE_IMG_SIXEL || SFTE_IMG_KITTY)
    sfte_image_state *images = &term->images;
#endif  // SFTE_TERM_SCROLLBACK_CAP && (SFTE_IMG_SIXEL || SFTE_IMG_KITTY)

    // If we clear entire screen and scrollback exists,
    // push the data to scrollback instead of erasing it in its entirety
    if (mode == 2 || (mode == 0 && cursor->col == 0 && cursor->row == 0)) {
#if SFTE_TERM_SCROLLBACK_CAP
        // Find last populated row
        int16_t last_r = cursor->row;
        for (int16_t r = vp->rows - 1; r > last_r; --r) {
            for (int16_t c = 0; c < vp->cols; ++c) {
                sfte_cell *cell = _sfte_grid_get_cell(term, c, r);
                if (cell->rune != ' ' && cell->rune != '\0') {
                    last_r = r;
                    break;
                }
            }

            if (last_r == r) break;
        }

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
        for (uint32_t i = 0; i < images->placements_len; ++i) {
            sfte_img_placement *p = &images->placements[i];
#if SFTE_TERM_ALT_SCREEN
            if (p->alt_screen != modes->alt_active) continue;
#endif  // SFTE_TERM_ALT_SCREEN

            sfte_img *img = _sfte_img_find(images, p->img_id);
            if (!img) continue;

            int16_t rows = _sfte_grid_span(img->height, p->y_off, vp->cell_height);
            int16_t img_bot = p->start_row + rows - 1;
            if (img_bot > last_r) last_r = img_bot;
        }
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

        if (last_r >= vp->rows) last_r = vp->rows - 1;
        int16_t lines_to_push = last_r + 1;

        // Temporarily bypass scroll margins to ensure full-screen push
        int16_t old_top = vp->scroll_top;
        int16_t old_bot = vp->scroll_bot;

        vp->scroll_top = 0;
        vp->scroll_bot = vp->rows - 1;

        if (lines_to_push > 0) _sfte_grid_scroll(term, lines_to_push);

        vp->scroll_top = old_top;
        vp->scroll_bot = old_bot;
#endif  // SFTE_TERM_SCROLLBACK_CAP

        _sfte_grid_clear_rows(term, 0, vp->rows);
        return;
    }
    if (mode == 0) {
        // Clear the rest of the current line
        _sfte_grid_clear_cells(term, _sfte_grid_get_idx(vp, col, cursor->row), vp->cols - col);
        // Clear all rows below the cursor
        if (cursor->row < vp->rows - 1)
            _sfte_grid_clear_rows(term, cursor->row + 1, vp->rows - 1 - cursor->row);
    } else if (mode == 1) {
        // Clear all rows above the cursor
        if (cursor->row > 0) _sfte_grid_clear_rows(term, 0, cursor->row);
        // Clear the start of the current line
        _sfte_grid_clear_cells(term, _sfte_grid_get_idx(vp, 0, cursor->row), col + 1);
    } else if (mode == 2) {
        _sfte_grid_clear_rows(term, 0, vp->rows);
    } else if (mode == 3) {
#if SFTE_TERM_SCROLLBACK_CAP && SFTE_TERM_SCROLLBACK_CLEAR
        vp->sb_len = 0;
        vp->sb_head = 0;
        vp->sb_off = 0;
#endif  // SFTE_TERM_SCROLLBACK_CAP && SFTE_TERM_SCROLLBACK_CLEAR
    }
}

/*
    Handles Erase Line / CSI K.

    Erases part or all of the current line.
*/
static inline void _sfte_csi_exec_el(sfte_term *term, int16_t mode, int16_t col) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;

    if (mode == 0)
        _sfte_grid_clear_cells(term, _sfte_grid_get_idx(vp, col, cursor->row), vp->cols - col);
    else if (mode == 1)
        _sfte_grid_clear_cells(term, _sfte_grid_get_idx(vp, 0, cursor->row), col + 1);
    else if (mode == 2)
        _sfte_grid_clear_cells(term, _sfte_grid_get_idx(vp, 0, cursor->row), vp->cols);
}

/*
    Handles Insert Line / CSI L.

    Inserts n blank lines at cursor position, pushing bottom lines off.
*/
static inline void _sfte_csi_exec_il(sfte_term *term, uint16_t *p) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;

    uint16_t n = _SFTE_P(p[0]);
    if (cursor->row > vp->scroll_bot) return;

    int16_t rem = vp->scroll_bot - cursor->row + 1;
    if (n > rem) n = rem;
    int16_t move_cnt = rem - n;

    for (int16_t i = move_cnt - 1; i >= 0; --i) {
        uint32_t dst_idx = _sfte_grid_get_idx(vp, 0, cursor->row + n + i);
        uint32_t src_idx = _sfte_grid_get_idx(vp, 0, cursor->row + i);
        memcpy(&term->cells[dst_idx], &term->cells[src_idx], vp->cols * sizeof(sfte_cell));
    }

    _sfte_grid_clear_rows(term, cursor->row, n);
    _sfte_grid_dirty_rows(term, cursor->row, vp->scroll_bot);
}

/*
    Handles Delete Line / CSI M.

    Deletes n lines at the cursor, pulling bottom lines up.
*/
static inline void _sfte_csi_exec_dl(sfte_term *term, uint16_t *p) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;

    uint16_t n = _SFTE_P(p[0]);
    if (cursor->row > vp->scroll_bot) return;

    int16_t rem = vp->scroll_bot - cursor->row + 1;
    if (n > rem) n = rem;
    int16_t move_cnt = rem - n;

    for (int16_t i = 0; i < move_cnt; ++i) {
        uint32_t dst_idx = _sfte_grid_get_idx(vp, 0, cursor->row + i);
        uint32_t src_idx = _sfte_grid_get_idx(vp, 0, cursor->row + n + i);
        memcpy(&term->cells[dst_idx], &term->cells[src_idx], vp->cols * sizeof(sfte_cell));
    }

    _sfte_grid_clear_rows(term, vp->scroll_bot - n + 1, n);
    _sfte_grid_dirty_rows(term, cursor->row, vp->scroll_bot);
}

/*
    Handles Delete Character / CSI P.

    Deletes n characters at the cursor, shifting right text left.
*/
static inline void _sfte_csi_exec_dch(sfte_term *term, uint16_t *p, int16_t col) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;

    uint16_t n = _SFTE_P(p[0]);
    int16_t rem = vp->cols - col;
    if (n > rem) n = rem;

    int16_t move_cnt = rem - n;
    int32_t base_idx = _sfte_grid_get_idx(vp, 0, cursor->row);
    if (move_cnt > 0)
        memmove(&term->cells[base_idx + col], &term->cells[base_idx + col + n],
                move_cnt * sizeof(sfte_cell));

    _sfte_grid_clear_cells(term, base_idx + vp->cols - n, n);
    _sfte_grid_dirty_range(term, base_idx + col, rem);
}

/*
    Handles Erase Character / CSI X.

    Replaces n characters with spaces starting at the cursor.
*/
static inline void _sfte_csi_exec_ech(sfte_term *term, uint16_t *p, int16_t col) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;

    uint16_t n = _SFTE_P(p[0]);
    int16_t rem = vp->cols - col;
    if (n > rem) n = rem;

    _sfte_grid_clear_cells(term, _sfte_grid_get_idx(vp, col, cursor->row), n);
}

/*
    Handles Device Attributes / CSI c.

    Reports the terminals identity and capabilities to the host.

    TODO:
    Add identity to customization. This might be useful especially for custom backends and such.
*/
static inline void _sfte_csi_exec_da(sfte_term *term) {
    const sfte_parser_state *parser = &term->parser;
    const sfte_callbacks *cb = term->cb;

    if (parser->dec_priv == 2) {
        const char *sda = "\033[>0;95;0c";
        if (cb->write) cb->write(cb->user_data, sda, strlen(sda));
    } else {
        const char *da = "\033[?62c";
        if (cb->write) cb->write(cb->user_data, da, strlen(da));
    }
}

/*
    Handles Vertical Position Absolute / CSI d.

    Moves cursor to the specific row n.

    NOTE:
    Respects origin mode.
*/
static inline void _sfte_csi_exec_vpa(sfte_term *term, uint16_t *p) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_modes_state *modes = &term->modes;
    sfte_cursor_state *cursor = &term->cursor;

    uint16_t n = _SFTE_P_IDX(p[0]);

    if (modes->origin_mode)
        cursor->row = _SFTE_CLAMP(n + vp->scroll_top, vp->scroll_top, vp->scroll_bot);
    else if (n >= vp->rows)
        cursor->row = vp->rows - 1;
    else
        cursor->row = n;
}

/*
    Handles Horizontal Vertical Position / CSI f.

    Moves cursor to row n, column m.

    NOTE:
    Respects origin mode.
*/
static inline void _sfte_csi_exec_hvp(sfte_term *term, uint16_t *p) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_modes_state *modes = &term->modes;
    sfte_cursor_state *cursor = &term->cursor;

    uint16_t n = _SFTE_P_IDX(p[0]);
    uint16_t m = _SFTE_P_IDX(p[1]);

    cursor->col = m < vp->cols ? m : vp->cols - 1;
    if (modes->origin_mode)
        cursor->row = _SFTE_CLAMP(n + vp->scroll_top, vp->scroll_top, vp->scroll_bot);
    else
        cursor->row = n < vp->rows ? n : vp->rows - 1;
}

/*
    Handles Tab Clear / CSI g.

    Clears tab stops at current column (0) or all columns (3).
*/
static inline void _sfte_csi_exec_tbc(sfte_term *term, uint16_t *p) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_cursor_state *cursor = &term->cursor;

    if (p[0] == 0)
        term->tab_stops[cursor->col] = 0;
    else if (p[0] == 3)
        memset(term->tab_stops, 0, vp->cols);
}

/*
    Handles Set Mode / CSI h.

    Enables various terminal modes.
    Supports DECTCEM (Cursor Show), DECAWM (Auto-Wrap),
    DECOM (Origin Mode), and alt screen buffer toggles.
*/
static inline void _sfte_csi_set_mode(sfte_term *term, uint16_t *p, uint16_t cnt, int16_t col) {
    const sfte_parser_state *parser = &term->parser;
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_sgr_state *sgr = &term->sgr;
    sfte_cursor_state *cursor = &term->cursor;
    sfte_modes_state *modes = &term->modes;
    sfte_saved_state *saved = &term->saved;
#if SFTE_INPUT_KITTY
    sfte_keyboard_state *kb = &term->keyboard;
#endif  // SFTE_INPUT_KITTY
#if SFTE_CURSOR_TRAIL
    sfte_trail_state *trail = &cursor->trail;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_INPUT_MOUSE
    sfte_mouse_state *mouse = &term->mouse;
#endif  // SFTE_INPUT_MOUSE

    if (!parser->dec_priv) return;

    for (int i = 0; i < cnt; ++i) {
        if (p[i] == 25) {
            cursor->hidden = 0;
            _sfte_grid_get_cell(term, col, cursor->row)->dirty = 1;
        }
#if SFTE_TERM_FOCUS
        else if (p[i] == 1004)
            modes->report_focus = 1;
#endif  // SFTE_TERM_FOCUS
        else if (p[i] == 2004)
            modes->bracketed_paste = 1;
        else if (p[i] == 7)
            modes->auto_wrap = 1;
        else if (p[i] == 6) {
            modes->origin_mode = 1;
            cursor->col = 0;
            cursor->row = vp->scroll_top;
        } else if (p[i] == 1047 || p[i] == 1048 || p[i] == 1049) {
            // 1048 / 1049 save cursor
            if (p[i] == 1048 || p[i] == 1049) {
                uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
                s_idx = modes->alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
                saved->grid_off[s_idx] = vp->grid_off;
                saved->col[s_idx] = cursor->col;
                saved->row[s_idx] = cursor->row;
                saved->fg[s_idx] = sgr->fg;
                saved->bg[s_idx] = sgr->bg;
                saved->attr[s_idx] = term->sgr.attr;
            }

#if SFTE_TERM_ALT_SCREEN
            // 1047 / 1049 switch to alt screen
            if ((p[i] == 1047 || p[i] == 1049) && !modes->alt_active) {
                modes->alt_active = 1;
#if SFTE_INPUT_KITTY
                kb->stack_idx[1] = 0;
                kb->stack[1][0] = 0;
#endif  // SFTE_INPUT_KITTY
#if SFTE_CURSOR_TRAIL
                trail->last_move_ms = 0;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_TERM_ANIMATE_SCREEN
                term->is_animating = 1;
                term->anim_dir = 1;
                term->anim_start_ms = SFTE_TIME_MS();
#endif  // SFTE_TERM_ANIMATE_SCREEN

                if (!term->alt_cells)
                    term->alt_cells = (sfte_cell *)SFTE_CALLOC(vp->cols * vp->rows,
                                                               sizeof(sfte_cell));

                sfte_cell *tmp = term->cells;
                term->cells = term->alt_cells;
                term->alt_cells = tmp;
            }
#endif  // SFTE_TERM_ALT_SCREEN

            if (p[i] == 1049) {
                _sfte_grid_clear_cells(term, 0, vp->cols * vp->rows);
                cursor->col = 0;
                cursor->row = 0;
            } else if (p[i] == 1047) {
                _sfte_grid_dirty_range(term, 0, vp->cols * vp->rows);
            }
        }
#if SFTE_INPUT_MOUSE
        else if (p[i] == 1000 || p[i] == 1002 || p[i] == 1003)
            mouse->mode = p[i];
        else if (p[i] == 1006)
            mouse->ext = 1006;
#endif  // SFTE_INPUT_MOUSE
    }
}

/*
    Handles Reset Mode / CSI l

    Disables various terminal modes.
    Matches the implementations found in SM.
*/
static inline void _sfte_csi_reset_mode(sfte_term *term, uint16_t *p, uint16_t cnt, int16_t col) {
    const sfte_parser_state *parser = &term->parser;
    sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;
    sfte_modes_state *modes = &term->modes;
    sfte_saved_state *saved = &term->saved;
    sfte_sgr_state *sgr = &term->sgr;
#if SFTE_INPUT_KITTY
    sfte_keyboard_state *kb = &term->keyboard;
#endif  // SFTE_INPUT_KITTY
#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    sfte_image_state *images = &term->images;
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
#if SFTE_CURSOR_TRAIL
    sfte_trail_state *trail = &cursor->trail;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_INPUT_MOUSE
    sfte_mouse_state *mouse = &term->mouse;
#endif  // SFTE_INPUT_MOUSE

    if (!parser->dec_priv) return;

    for (int i = 0; i < cnt; ++i) {
        if (p[i] == 25) {
            cursor->hidden = 1;
            _sfte_grid_get_cell(term, col, cursor->row)->dirty = 1;
        }
#if SFTE_TERM_FOCUS
        else if (p[i] == 1004)
            modes->report_focus = 0;
#endif  // SFTE_TERM_FOCUS
        else if (p[i] == 2004)
            modes->bracketed_paste = 0;
        else if (p[i] == 7)
            modes->auto_wrap = 0;
        else if (p[i] == 6) {
            modes->origin_mode = 0;
            cursor->col = 0;
            cursor->row = 0;
        } else if (p[i] == 1047 || p[i] == 1048 || p[i] == 1049) {
#if SFTE_TERM_ALT_SCREEN
            if ((p[i] == 1047 || p[i] == 1049) && modes->alt_active) {
                modes->alt_active = 0;
#if SFTE_INPUT_KITTY
                kb->stack_idx[0] = 0;
                kb->stack_idx[1] = 0;
                kb->stack[0][0] = 0;
                kb->stack[1][0] = 0;
#endif  // SFTE_INPUT_KITTY
#if SFTE_TERM_ANIMATE_SCREEN
                term->is_animating = 1;
                term->anim_dir = -1;
                term->anim_start_ms = SFTE_TIME_MS();
#endif  // SFTE_TERM_ANIMATE_SCREEN

                if (term->alt_cells) {
                    sfte_cell *tmp = term->cells;
                    term->cells = term->alt_cells;
                    term->alt_cells = tmp;
                    _sfte_grid_dirty_range(term, 0, vp->cols * vp->rows);
                }

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
                // Destroy all images created on alt screen
                for (uint32_t j = 0; j < images->placements_len; ++j) {
                    if (!images->placements[j].alt_screen) continue;
                    for (uint32_t k = 0; k < images->pool_len; ++k)
                        if (images->pool[k].id == images->placements[j].img_id) {
                            images->pool[k].ref_cnt--;
                            break;
                        }
                    images->placements[j--] = images->placements[--images->placements_len];
                }
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
            }
#endif  // SFTE_TERM_ALT_SCREEN

            if (p[i] == 1048 || p[i] == 1049) {
                uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
                s_idx = modes->alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
                vp->grid_off = saved->grid_off[s_idx];
                cursor->col = _SFTE_CLAMP(saved->col[s_idx], 0, vp->cols - 1);
                cursor->row = _SFTE_CLAMP(saved->row[s_idx], 0, vp->rows - 1);

                // force external output below prompt
                if (p[i] == 1049) {
                    cursor->col = 0;
                    if (cursor->row == vp->scroll_bot)
                        _sfte_grid_scroll(term, 1);
                    else if (cursor->row == vp->rows - 1) {
                        int16_t old_t = vp->scroll_top;
                        int16_t old_b = vp->scroll_bot;

                        vp->scroll_top = 0;
                        vp->scroll_bot = vp->rows - 1;

                        _sfte_grid_scroll(term, 1);

                        vp->scroll_top = old_t;
                        vp->scroll_bot = old_b;
                    } else if (cursor->row < vp->rows - 1)
                        cursor->row++;
                }

                sgr->fg = saved->fg[s_idx];
                sgr->bg = saved->bg[s_idx];
                sgr->attr = saved->attr[s_idx];
                _sfte_grid_get_cell(term, cursor->col, cursor->row)->dirty = 1;

#if SFTE_CURSOR_TRAIL
                trail->last_move_ms = 0;
                trail->is_trailing = 0;
                trail->x = cursor->col * vp->cell_width;
                trail->y = cursor->row * vp->cell_height;
                trail->dmg.w = 0;
#endif  // SFTE_CURSOR_TRAIL
            }
        }
#if SFTE_INPUT_MOUSE
        else if (p[i] == 1000 || p[i] == 1002 || p[i] == 1003)
            mouse->mode = 0;
        else if (p[i] == 1006)
            mouse->ext = 0;
#endif  // SFTE_INPUT_MOUSE
    }
}

/*
    Handles Select Graphic Rendition / CSI m

    Sets colors and style of the characters following this code.
*/
static inline void _sfte_csi_exec_sgr(sfte_sgr_state *sgr, uint16_t *p, uint16_t cnt) {
    if (cnt == 0) {
        cnt = 1;
        p[0] = 0;
    }

    for (int16_t i = 0; i < cnt; ++i) {
        uint16_t code = p[i];

        switch (code) {
        case 0:  // Reset all
            sgr->fg = _SFTE_COLOR_FG_DEFAULT;
            sgr->bg = _SFTE_COLOR_BG_DEFAULT;
            sgr->attr = 0;
#if SFTE_UNDERLINE_COLORED
            sgr->ul_color = _SFTE_COLOR_FG_DEFAULT;
#endif  // SFTE_UNDERLINE_COLORED
#if SFTE_UNDERLINE_EXTENDED
            sgr->ul_style = 0;
#endif  // SFTE_UNDERLINE_EXTENDED
            break;
        case 1: sgr->attr |= _SFTE_ATTR_BOLD; break;
        case 2: /* TODO: add _SFTE_ATTR_DIM */ break;
        case 3: sgr->attr |= _SFTE_ATTR_ITALIC; break;
        case 4: sgr->attr |= _SFTE_ATTR_UNDERLINE;
#if SFTE_UNDERLINE_EXTENDED
            // Default to straight if no sub-param is provided
            sgr->ul_style = _SFTE_UNDERLINE_STYLE_STRAIGHT;

            // If the sub-param exists and defines a style set it
            if (i + 1 < cnt && p[i + 1] <= _SFTE_UNDERLINE_STYLE_DASHED) sgr->ul_style = p[++i];
#endif  // SFTE_UNDERLINE_EXTENDED
            break;
        case 7: sgr->attr |= _SFTE_ATTR_REVERSE; break;
        case 22: sgr->attr &= ~_SFTE_ATTR_BOLD; break;
        case 23: sgr->attr &= ~_SFTE_ATTR_ITALIC; break;
        case 24: sgr->attr &= ~_SFTE_ATTR_UNDERLINE;
#if SFTE_UNDERLINE_EXTENDED
            sgr->ul_style = 0;
#endif  // SFTE_UNDERLINE_EXTENDED
            break;
        case 27: sgr->attr &= ~_SFTE_ATTR_REVERSE; break;
        case 39: sgr->fg = _SFTE_COLOR_FG_DEFAULT; break;
        case 49: sgr->bg = _SFTE_COLOR_BG_DEFAULT; break;
#if SFTE_UNDERLINE_COLORED
        case 59: sgr->ul_color = sgr->fg; break;
#endif            // SFTE_UNDERLINE_COLORED
        case 38:  // FG
        case 48:  // BG
#if SFTE_UNDERLINE_COLORED
        case 58:  // Underline
#endif            // SFTE_UNDERLINE_COLORED
        {
            if (i + 2 < cnt && p[i + 1] == 5) {
                uint32_t color = _sfte_color_from_idx(p[i + 2]);
                if (code == 38)
                    sgr->fg = color;
                else if (code == 48)
                    sgr->bg = color;
                i += 2;
            } else if (i + 4 < cnt && p[i + 1] == 2) {
                // Truecolor
                uint32_t rgb = _sfte_csi_parse_truecolor(p, i);
                uint32_t color = _sfte_color_from_rgb(rgb);
                if (code == 38)
                    sgr->fg = color;
                else if (code == 48)
                    sgr->bg = color;
#if SFTE_UNDERLINE_COLORED
                else if (code == 58)
                    sgr->ul_color = color;
#endif  // SFTE_UNDERLINE_COLORED
                i += 4;
            }
            break;
        }
        default: {
            if (code >= 30 && code <= 37)
                sgr->fg = _sfte_color_from_idx(code - 30);
            else if (code >= 90 && code <= 97)
                sgr->fg = _sfte_color_from_idx((code - 90) + 8);
            else if (code >= 40 && code <= 47)
                sgr->bg = _sfte_color_from_idx(code - 40);
            else if (code >= 100 && code <= 107)
                sgr->bg = _sfte_color_from_idx((code - 100) + 8);
            break;
        }
        }
    }
}

/*
    Handles Device Status Report / CSI n.

    Reports cursor position (6) or terminal status (5).
*/
static inline void _sfte_csi_exec_dsr(sfte_term *term, uint16_t *p) {
    const sfte_cursor_state *cursor = &term->cursor;
    const sfte_callbacks *cb = term->cb;

    if (p[0] == 5) {
        const char *reply = "\033[0n";
        if (cb->write) cb->write(cb->user_data, reply, strlen(reply));
    } else if (p[0] == 6) {
        char buf[32];
        size_t len = snprintf(buf, sizeof(buf), "\033[%d;%dR", cursor->row + 1, cursor->col + 1);
        if (cb->write) cb->write(cb->user_data, buf, len);
    }
}

/*
    Handles Soft Terminal Reset / CSI p.

    Resets terminal state to default values.
*/
static inline void _sfte_csi_exec_decstr(sfte_term *term, int16_t col) {
    sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;
    sfte_sgr_state *sgr = &term->sgr;
#if SFTE_INPUT_MOUSE
    sfte_mouse_state *mouse = &term->mouse;
#endif  // SFTE_INPUT_MOUSE
#if SFTE_INPUT_KITTY
    sfte_keyboard_state *kb = &term->keyboard;
#endif  // SFTE_INPUT_KITTY
#if SFTE_INPUT_HYPERLINKS
    sfte_link_state *links = &term->links;
#endif  // SFTE_INPUT_HYPERLINKS

#if SFTE_INPUT_MOUSE
    mouse->mode = 0;
    mouse->ext = 0;
#endif  // SFTE_INPUT_MOUSE
#if SFTE_INPUT_KITTY
    kb->stack_idx[0] = 0;
    kb->stack_idx[1] = 0;
    kb->stack[0][0] = 0;
    kb->stack[1][0] = 0;
#endif  // SFTE_INPUT_KITTY
#if SFTE_CURSOR_BLINK
    cursor->blink_enabled = 1;
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_DYNAMIC
    cursor->color = SFTE_CURSOR_COLOR;
    cursor->style = SFTE_CURSOR_STYLE;
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_UNDERLINE_COLORED
    sgr->ul_color = _SFTE_COLOR_FG_DEFAULT;
#endif  // SFTE_UNDERLINE_COLORED
#if SFTE_UNDERLINE_EXTENDED
    sgr->ul_style = 0;
#endif  // SFTE_UNDERLINE_EXTENDED
#if SFTE_INPUT_HYPERLINKS
    links->cur_idx = 0;
#endif  // SFTE_INPUT_HYPERLINKS
    vp->scroll_top = 0;
    vp->scroll_bot = vp->rows - 1;
    sgr->fg = _SFTE_COLOR_FG_DEFAULT;
    sgr->bg = _SFTE_COLOR_BG_DEFAULT;
    sgr->attr = 0;
    cursor->hidden = 0;
    term->tab_width = SFTE_TERM_TAB_WIDTH;
    _sfte_grid_resize_tabs(term, vp->cols, 0);
    _sfte_grid_get_cell(term, col, cursor->row)->dirty = 1;
}

/*
    Handles Set Cursor Style / CSI q.

    Changes the cursor shape and blinking style.
*/
static inline void _sfte_csi_exec_decscusr(sfte_term *term, uint16_t *p, int16_t col) {
    sfte_cursor_state *cursor = &term->cursor;

#if SFTE_CURSOR_BLINK
    switch (p[0]) {
    case 0:
    case 1:
    case 3:
    case 5: cursor->blink_enabled = 1; break;
    case 2:
    case 4:
    case 6: cursor->blink_enabled = 0; break;
    }
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_DYNAMIC
    switch (p[0]) {
    case 0: cursor->style = SFTE_CURSOR_STYLE; break;
    case 1:
    case 2: cursor->style = SFTE_CURSOR_STYLE_BLOCK; break;
    case 3:
    case 4: cursor->style = SFTE_CURSOR_STYLE_UNDERLINE; break;
    case 5:
    case 6: cursor->style = SFTE_CURSOR_STYLE_BAR; break;
    }
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_CURSOR_BLINK || SFTE_CURSOR_DYNAMIC
    _sfte_grid_get_cell(term, col, cursor->row)->dirty = 1;
#endif  // SFTE_CURSOR_BLINK || SFTE_CURSOR_DYNAMIC
}

/*
    Handles Set Top and Bottom Margins / CSI r.

    Sets the scrolling region top and bottom margins.

    NOTE:
    Respects origin mode.
*/
static inline void _sfte_csi_exec_decstbm(sfte_term *term, uint16_t *p, uint16_t cnt) {
    const sfte_modes_state *modes = &term->modes;
    sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;

    uint16_t top = _SFTE_P_IDX(p[0]);
    int16_t bot = (cnt > 1 && p[1] > 0 ? p[1] : vp->rows) - 1;
    if (bot >= vp->rows) bot = vp->rows - 1;
    if (top < bot) {
        vp->scroll_top = top;
        vp->scroll_bot = bot;
    }

    cursor->col = 0;
    cursor->row = modes->origin_mode ? vp->scroll_top : 0;
}

/*
    Handles Save Cursor / CSI s.

    Saves the current cursor position and attributes.
*/
static inline void _sfte_csi_exec_scosc(sfte_term *term, uint16_t *p) {
    if (p[0] != 0) return;  // Avoid colliding with kitty support command

    const sfte_modes_state *modes = &term->modes;
    const sfte_cursor_state *cursor = &term->cursor;
    const sfte_sgr_state *sgr = &term->sgr;
    sfte_saved_state *saved = &term->saved;

    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = modes->alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    saved->col[s_idx] = cursor->col;
    saved->row[s_idx] = cursor->row;
    saved->fg[s_idx] = sgr->fg;
    saved->bg[s_idx] = sgr->bg;
    saved->attr[s_idx] = sgr->attr;
}

/*
    Handles Window Manipulation / CSI t.

    Xterm extension for querying or pushing/popping window titles.
*/
static inline void _sfte_csi_exec_xtwinops(sfte_term *term, uint16_t *p) {
    if (p[1] != 0 && p[1] != 2) return;

    const sfte_modes_state *modes = &term->modes;
    const sfte_callbacks *cb = term->cb;
    sfte_saved_state *saved = &term->saved;

    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = modes->alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    if (p[0] == 22)
        snprintf(saved->title[s_idx], sizeof(saved->title[s_idx]), "%s", term->title);
    else if (p[0] == 23) {
        snprintf(term->title, sizeof(term->title), "%s", saved->title[s_idx]);
        // TODO:
        // Make a callback that gets called here for custom backends support.
#if SFTE_WAYLAND
        sfte_wayland_app *app = (sfte_wayland_app *)cb->user_data;
        xdg_toplevel_set_title(app->xdg_toplevel, term->title);
#endif  // SFTE_WAYLAND
    }
}

/*
    Handles Restore Cursor / CSI u.

    Restores the previously saved cursor position and attributes.
*/
static inline void _sfte_csi_exec_scorc(sfte_term *term, uint16_t *p) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_parser_state *parser = &term->parser;
    const sfte_modes_state *modes = &term->modes;
    const sfte_saved_state *saved = &term->saved;
    sfte_cursor_state *cursor = &term->cursor;
    sfte_sgr_state *sgr = &term->sgr;

    if (parser->dec_priv != 0 || p[0] != 0) return;

    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = modes->alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    cursor->col = _SFTE_CLAMP(saved->col[s_idx], 0, vp->cols - 1);
    cursor->row = _SFTE_CLAMP(saved->row[s_idx], 0, vp->rows - 1);
    sgr->fg = term->saved.fg[s_idx];
    sgr->bg = term->saved.bg[s_idx];
    sgr->attr = term->saved.attr[s_idx];
}

#if SFTE_INPUT_KITTY
/*
    Handles Kitty Keyboard Protocol / CSI u extension.

    The VT500 standard defines 'u' as SCORC (Restore Cursor) if no modifiers are present.
    However, the modern kitty keyboard protocol overloads 'u' to manage
    the keyboard flag stack when preceded by >, =, < or ?.
*/
static inline void _sfte_csi_exec_kitty(sfte_term *term, uint16_t *p) {
    const sfte_parser_state *parser = &term->parser;
    const sfte_modes_state *modes = &term->modes;
    const sfte_callbacks *cb = term->cb;
    sfte_keyboard_state *kb = &term->keyboard;

    if (parser->dec_priv == 0) return;

    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = modes->alt_active;
#endif  // SFTE_TERM_ALT_SCREEN

    if (parser->dec_priv == 1) {  // CSI ? u (query)
        char buf[32];
        uint16_t flags = kb->stack[s_idx][kb->stack_idx[s_idx]];

        size_t len = snprintf(buf, sizeof(buf), "\033[?%du", flags);
        if (cb->write) cb->write(cb->user_data, buf, len);
    } else if (parser->dec_priv == 2) {  // CSI > flags u (push)
        if (kb->stack_idx[s_idx] < 15) kb->stack_idx[s_idx]++;

        kb->stack[s_idx][kb->stack_idx[s_idx]] = p[0];
    } else if (parser->dec_priv == 3) {  // CSI < n u (pop)
        uint8_t pop_cnt = (p[0] > 0) ? p[0] : 1;
        if (kb->stack_idx[s_idx] >= pop_cnt) {
            kb->stack_idx[s_idx] -= pop_cnt;
        } else {
            kb->stack_idx[s_idx] = 0;
            kb->stack[s_idx][0] = 0;
        }
    } else if (parser->dec_priv == 4)  // CSI = flags u (set/overwrite)
        kb->stack[s_idx][kb->stack_idx[s_idx]] = p[0];
}
#endif  // SFTE_INPUT_KITTY

/*
    The main routing switch for Control Sequence Introducer events.
*/
static inline void _sfte_csi_dispatch(sfte_term *term, uint8_t cmd) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;
#if SFTE_CURSOR_TRAIL
    sfte_trail_state *trail = &cursor->trail;
#endif  // SFTE_CURSOR_TRAIL
    sfte_parser_state *parser = &term->parser;
    sfte_sgr_state *sgr = &term->sgr;

    uint16_t *p = parser->params;
    int16_t cnt = parser->param_idx + 1;
    int16_t col = cursor->col >= vp->cols ? vp->cols - 1 : cursor->col;

    switch (cmd) {
    case '@': _sfte_csi_exec_ich(term, p, col); break;
    case 'A':  // CUU / Cursor Up
        cursor->row = _SFTE_CLAMP(cursor->row - _SFTE_P(p[0]), 0, vp->rows - 1);
        break;
    case 'B':  // CUD / Cursor Down
        cursor->row = _SFTE_CLAMP(cursor->row + _SFTE_P(p[0]), 0, vp->rows - 1);
        break;
    case 'C':  // CUF / Cursor Forward
        cursor->col = _SFTE_CLAMP(cursor->col + _SFTE_P(p[0]), 0, vp->cols - 1);
        break;
    case 'D':  // CUB / Cursor Back
        cursor->col = _SFTE_CLAMP(cursor->col - _SFTE_P(p[0]), 0, vp->cols - 1);
        break;
    case 'E': _sfte_csi_exec_cnl(term, p); break;
    case 'F': _sfte_csi_exec_cpl(term, p); break;
    case 'G':  // CHA / Cursor Horizontal Absolute
        cursor->col = _SFTE_CLAMP(_SFTE_P_IDX(p[0]), 0, vp->cols - 1);
        break;
    case 'H':  // CUP / Cursor Position
        cursor->col = _SFTE_CLAMP(_SFTE_P_IDX(p[1]), 0, vp->cols - 1);
        cursor->row = _SFTE_CLAMP(_SFTE_P_IDX(p[0]), 0, vp->rows - 1);
        break;
    case 'I':  // CHT / Cursor Forward Tabulation
        for (int16_t i = 0, n = _SFTE_P(p[0]); i < n; ++i) {
            while (cursor->col < vp->cols - 1)
                if (term->tab_stops[++cursor->col]) break;
        }
        break;
    case 'J':
        if (parser->dec_priv == 0)
            for (int i = 0; i < cnt; ++i) _sfte_csi_exec_ed(term, p[i], col);
        break;
    case 'K':
        if (parser->dec_priv == 0)
            for (int i = 0; i < cnt; ++i) _sfte_csi_exec_el(term, p[i], col);
        break;
    case 'L': _sfte_csi_exec_il(term, p); break;
    case 'M': _sfte_csi_exec_dl(term, p); break;
    case 'P': _sfte_csi_exec_dch(term, p, col); break;
    case 'S': _sfte_grid_scroll(term, _SFTE_P(p[0])); break;
    case 'T': _sfte_grid_scroll(term, -_SFTE_P(p[0])); break;
    case 'W':  // DECST8C / Set Tab at Every 8 Columns
        if (parser->dec_priv == 1 && p[0] == 5) {
            term->tab_width = 8;
            for (int16_t i = 0; i < vp->cols; ++i) term->tab_stops[i] = (i % 8 == 0);
        }
        break;
    case 'X': _sfte_csi_exec_ech(term, p, col); break;
    case 'Z':  // CBT / Cursor Backward Tabulation
        for (int16_t i = 0, n = _SFTE_P(p[0]); i < n; ++i) {
            if (cursor->col >= vp->cols) cursor->col = vp->cols - 1;
            while (cursor->col > 0)
                if (term->tab_stops[--cursor->col]) break;
        }
        break;
    case 'c':
        if (parser->dec_priv == 0 || parser->dec_priv == 2) _sfte_csi_exec_da(term);
        break;
    case 'd': _sfte_csi_exec_vpa(term, p); break;
    case 'f': _sfte_csi_exec_hvp(term, p);
#if SFTE_CURSOR_TRAIL
        trail->warp_tail = 1;
#endif  // SFTE_CURSOR_TRAIL
        break;
    case 'g': _sfte_csi_exec_tbc(term, p); break;
    case 'h': _sfte_csi_set_mode(term, p, cnt, col); break;
    case 'l': _sfte_csi_reset_mode(term, p, cnt, col); break;
    case 'm':
        if (parser->dec_priv == 0) _sfte_csi_exec_sgr(sgr, p, cnt);
        break;
    case 'n': _sfte_csi_exec_dsr(term, p); break;
    case 'p':
        if (parser->dec_priv == 0 && (parser->csi_inter == 0 || parser->csi_inter == '!')) {
            _sfte_csi_exec_decstr(term, col);
#if SFTE_CURSOR_TRAIL
            trail->last_move_ms = 0;
#endif  // SFTE_CURSOR_TRAIL
        }
        break;
    case 'q':
        if (parser->dec_priv == 0 && (parser->csi_inter == 0 || parser->csi_inter == ' '))
            _sfte_csi_exec_decscusr(term, p, col);
        break;
    case 'r': _sfte_csi_exec_decstbm(term, p, cnt); break;
    case 's':
        if (parser->dec_priv == 0) _sfte_csi_exec_scosc(term, p);
        break;
    case 't': _sfte_csi_exec_xtwinops(term, p); break;
    case 'u':
        if (parser->dec_priv == 0) _sfte_csi_exec_scorc(term, p);
#if SFTE_INPUT_KITTY
        else
            _sfte_csi_exec_kitty(term, p);
#endif  // SFTE_INPUT_KITTY
        break;
    }
}

#undef _SFTE_P
#undef _SFTE_P_IDX

// =================================================================================================
// >>parser
// =================================================================================================
#if SFTE_CURSOR_DYNAMIC
/*
    Parses an X11/OSC color string into a 24-bit RGB uint32_t.
    Supports #RRGGBB and rgb:R/G/B.
    Returns the `fallback` color if parsing fails.
*/
static inline uint32_t _sfte_parser_osc_color(const char *str, uint32_t fallback) {
    if (!str || !*str) return fallback;

    if (str[0] == '#') {  // #RRGGBB
        uint32_t val = 0;
        for (uint8_t i = 1; i <= 6; ++i) {
            char c = str[i];
            val <<= 4;
            if (c >= '0' && c <= '9')
                val |= (c - '0');
            else if (c >= 'a' && c <= 'f')
                val |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                val |= (c - 'A' + 10);
            else
                return fallback;
        }
        return val;
    }

    if (strncmp(str, "rgb:", 4) == 0) {  // rgb:R/G/B
        const char *p = str + strlen("rgb:");
        uint32_t rgb = 0;
        for (uint8_t c = 0; c < 3; ++c) {
            uint32_t channel = 0;
            uint8_t digits = 0;
            while (*p != '/' && *p != '\0' && digits < 4) {
                char ch = *p++;
                channel <<= 4;
                if (ch >= '0' && ch <= '9')
                    channel |= (ch - '0');
                else if (ch >= 'a' && ch <= 'f')
                    channel |= (ch - 'a' + 10);
                else if (ch >= 'A' && ch <= 'F')
                    channel |= (ch - 'A' + 10);
                else
                    return fallback;
                digits++;
            }
            if (*p == '/') p++;

            // If 16 bit channel, truncate to 8 bit
            if (digits > 2) channel >>= (digits - 2) * 4;
            rgb = (rgb << 8) | (channel & 0xFF);
        }
        return rgb;
    }

    return fallback;
}
#endif  // SFTE_CURSOR_DYNAMIC

/*
    Appends a byte to the shared OSC/DCS payload buffer.
    If the buffer is too small, reallocates it with size doubled,
    unless it's already at its max capacity.
*/
static inline void _sfte_parser_append_payload(sfte_parser_state *parser, uint8_t b) {
    if (parser->osc_len + 1 >= parser->osc_cap && parser->osc_cap < SFTE_OSC_MAX_CAP) {
        parser->osc_cap *= 2;
        parser->osc_payload = (char *)SFTE_REALLOC(parser->osc_payload, parser->osc_cap);
    }
    if (parser->osc_len + 1 < parser->osc_cap) parser->osc_payload[parser->osc_len++] = b;
}

/*
    Handles LF, VT, FF (Linefeed).

    Scrolls if at bottom margin.
*/
static inline void _sfte_parser_c0_lf(sfte_term *term) {
    sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;

    if (cursor->row == vp->scroll_bot)
        _sfte_grid_scroll(term, 1);
    else if (cursor->row == vp->rows - 1) {
        // Temporarily set scroll values to bottom to safely scroll
        int16_t old_top = vp->scroll_top;
        int16_t old_bot = vp->scroll_bot;

        vp->scroll_top = 0;
        vp->scroll_bot = vp->rows - 1;

        _sfte_grid_scroll(term, 1);

        vp->scroll_top = old_top;
        vp->scroll_bot = old_bot;
    } else if (cursor->row < vp->rows - 1)
        cursor->row++;
}

/*
    Handles HT (Horizontal Tab).

    Advances to the next set tab stop.
*/
static inline void _sfte_parser_c0_ht(sfte_term *term) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;

    while (cursor->col < vp->cols - 1)
        if (term->tab_stops[++cursor->col]) break;
}

/*
    Handles RIS (Reset to Initial State) ESC c.

    Reuses DECSTR logic.
*/
static inline void _sfte_parser_esc_ris(sfte_term *term) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;

    term->parser.dec_priv = 0;
    term->parser.csi_inter = 0;
    _sfte_csi_dispatch(term, 'p');
    cursor->col = 0, cursor->row = 0;
    _sfte_grid_clear_cells(term, 0, vp->cols * vp->rows);
#if SFTE_SEARCH
    _sfte_search_clear(term);
#endif  // SFTE_SEARCH
}

/*
    Handles SC (Save Cursor) / ESC 7.

    Temporarily stores the data (cursor position, foreground/background color, attribute)
    to load it via RC later.

    The terminal can store one set of data per screen (main/alt).
*/
static inline void _sfte_parser_esc_sc(sfte_term *term) {
#if SFTE_TERM_ALT_SCREEN
    const sfte_modes_state *modes = &term->modes;
#endif  // SFTE_TERM_ALT_SCREEN
    const sfte_cursor_state *cursor = &term->cursor;
    const sfte_sgr_state *sgr = &term->sgr;
    sfte_saved_state *saved = &term->saved;

    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = modes->alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    saved->col[s_idx] = cursor->col;
    saved->row[s_idx] = cursor->row;
    saved->fg[s_idx] = sgr->fg;
    saved->bg[s_idx] = sgr->bg;
    saved->attr[s_idx] = sgr->attr;
}

/*
    Handles RC (Restore Cursor) / ESC 8.

    Loads the data (cursor position, foreground/background color, attribute)
    stored by SC in sequence.

    The terminal can store one set of data per screen (main/alt).
*/
static inline void _sfte_parser_esc_rc(sfte_term *term) {
#if SFTE_TERM_ALT_SCREEN
    const sfte_modes_state *modes = &term->modes;
#endif  // SFTE_TERM_ALT_SCREEN
    const sfte_saved_state *saved = &term->saved;
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;
    sfte_sgr_state *sgr = &term->sgr;

    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = modes->alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    // NOTE:
    // We need to clamp here since between SC and RC a resize could've occured.
    cursor->col = _SFTE_CLAMP(saved->col[s_idx], 0, vp->cols - 1);
    cursor->row = _SFTE_CLAMP(saved->row[s_idx], 0, vp->rows - 1);
    sgr->fg = saved->fg[s_idx];
    sgr->bg = saved->bg[s_idx];
    sgr->attr = saved->attr[s_idx];
}

/*
    Handles IND (Index) / ESC D.

    Moves down, scrolling if at margin.
*/
static inline void _sfte_parser_esc_ind(sfte_term *term) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;

    if (cursor->row == vp->scroll_bot)
        _sfte_grid_scroll(term, 1);
    else if (cursor->row < vp->rows - 1)
        cursor->row++;
}

/*
    Handles RI (Reverse Index) / ESC M.

    Moves up, scrolling (up) if at margin.
*/
static inline void _sfte_parser_esc_ri(sfte_term *term) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;

    if (cursor->row == vp->scroll_top)
        _sfte_grid_scroll(term, -1);
    else if (cursor->row > 0)
        cursor->row--;
}

/*
    Handles NEL (Next Line) / ESC E.

    Moves to start of the next line.

    Reuses Index logic to move down.
*/
static inline void _sfte_parser_esc_nel(sfte_term *term) {
    _sfte_parser_esc_ind(term);
    term->cursor.col = 0;
}

/*
    Handles DECALN (Screen Alignment Pattern) / ESC # 8.
*/
static inline void _sfte_parser_hash_decaln(sfte_term *term) {
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;
    sfte_cell *cells = term->cells;

    for (int32_t i = 0; i < vp->cols * vp->rows; ++i) {
        cells[i].rune = 'E';
        cells[i].fg = _SFTE_COLOR_FG_DEFAULT;
        cells[i].bg = _SFTE_COLOR_BG_DEFAULT;
        cells[i].attr = 0;
        cells[i].dirty = 1;
    }

    cursor->col = 0, cursor->row = 0;
}

/*
    Parses and executes completed OSC strings.
*/
static inline void _sfte_parser_osc_dispatch(sfte_term *term, uint8_t terminator) {
    const sfte_callbacks *cb = term->cb;
    sfte_stack *stack = &term->stack;
    sfte_cursor_state *cursor = &term->cursor;
    sfte_parser_state *parser = &term->parser;
#if SFTE_INPUT_HYPERLINKS
    sfte_link_state *links = &term->links;
#endif  // SFTE_INPUT_HYPERLINKS

    const char *term_str = (terminator == '\x1b') ? "\033\\" : "\x07";
    parser->osc_payload[parser->osc_len] = '\0';

    if (strncmp(parser->osc_payload, "10;?", 4) == 0 || strncmp(parser->osc_payload, "11;?", 4) == 0
#if SFTE_CURSOR_DYNAMIC
        || strncmp(parser->osc_payload, "12;?", 4) == 0
#endif  // SFTE_CURSOR_DYNAMIC
    ) {
        uint8_t code_char = parser->osc_payload[1];
        uint8_t code = (code_char == '0') ? 10 : ((code_char == '1') ? 11 : 12);

        uint32_t col = (code == 11) ? SFTE_COLOR_BG : SFTE_COLOR_FG;
#if SFTE_CURSOR_DYNAMIC
        if (code == 12) col = cursor->color;
#endif  // SFTE_CURSOR_DYNAMIC
        uint8_t col_r = (col >> 16) & 0xFF, col_g = (col >> 8) & 0xFF, col_b = col & 0xFF;

        char reply[64];
        size_t len = snprintf(reply, sizeof(reply), "\033]%d;rgb:%02x%02x/%02x%02x/%02x%02x%s",
                              code, col_r, col_r, col_g, col_g, col_b, col_b, term_str);
        if (cb->write) cb->write(cb->user_data, reply, len);
    }
#if SFTE_CURSOR_DYNAMIC
    else if (strncmp(parser->osc_payload, "12;", 3) == 0)
        cursor->color = _sfte_parser_osc_color(parser->osc_payload + 3, cursor->color);
    else if (strncmp(parser->osc_payload, "112", 3) == 0)
        cursor->color = SFTE_CURSOR_COLOR;
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52
    else if (strncmp(parser->osc_payload, "52;", 3) == 0) {  // Remote Clipboard (OSC 52)
        char *p = parser->osc_payload + strlen("52;");
        char target = (*p && *p != ';') ? *p : 'c';
        while (*p && *p != ';') p++;

        if (*p == ';' && *++p != '?' /* Skips read requests */) {
            size_t b64_len = parser->osc_len - (p - parser->osc_payload);
            size_t raw_len = 0;
            uint8_t *raw_data = _sfte_b64_decode(stack, (uint8_t *)p, b64_len, &raw_len);

            if (raw_data) {
                size_t pre_off = _sfte_mem_stack_save(stack);
                char *data = (char *)_sfte_mem_stack_alloc(stack, raw_len + 1, _Alignof(char));
                if (!data) {
                    _sfte_mem_stack_rewind(stack, pre_off);
                    return;
                }

                memcpy(data, raw_data, raw_len);
                data[raw_len] = '\0';
                if (cb->osc52_clipboard) cb->osc52_clipboard(cb->user_data, target, data);
                _sfte_mem_stack_rewind(stack, pre_off);
            }
        }
    }
#endif  // SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52
#if SFTE_INPUT_HYPERLINKS
    else if (strncmp(parser->osc_payload, "8;", 2) == 0) {  // hyperlink
        char *p = parser->osc_payload + strlen("8;");
        while (*p && *p != ';') p++;

        if (*p++ == ';') {
            if (*p == '\0')  // Empty URI means close the link
                links->cur_idx = 0;
            else {  // Check if URI is already in pool
                uint16_t found_idx = 0;
                for (uint16_t i = 1; i < links->pool_len; ++i)
                    if (strcmp(links->pool[i], p) == 0) {
                        found_idx = i;
                        break;
                    }

                // Add new URI if not found
                if (found_idx == 0 && links->pool_len < SFTE_INPUT_HYPERLINKS_MAX_CAP) {
                    if (links->pool_len >= links->pool_cap) {
                        links->pool_cap *= 2;
                        links->pool = (char **)SFTE_REALLOC(links->pool,
                                                            links->pool_cap * sizeof(char *));
                    }

                    size_t ulen = strlen(p);
                    char *uri = (char *)SFTE_MALLOC(ulen + 1);
                    memcpy(uri, p, ulen + 1);
                    found_idx = links->pool_len++;
                    links->pool[found_idx] = uri;
                }

                links->cur_idx = found_idx;
            }
        }
    }
#endif  // SFTE_INPUT_HYPERLINKS
    else if (strncmp(parser->osc_payload, "0;", 2) == 0 ||
             strncmp(parser->osc_payload, "1;", 2) == 0 ||
             strncmp(parser->osc_payload, "2;", 2) == 0) {
        char *title = parser->osc_payload + 2;
        if (!term->title_locked) snprintf(term->title, sizeof(term->title), "%s", title);
    } else if (strncmp(parser->osc_payload, "9;4;", 4) == 0) {
        char *p = parser->osc_payload + strlen("9;4;");
        uint8_t state = *p - '0';
        while (*p && *p != ';') p++;

        uint8_t progress = 0;
        if (*p == ';') progress = atoi(p + 1);
        if (progress > 100) progress = 100;
        if (cb->progress) cb->progress(cb->user_data, state, progress);
    }
}

/*
    Parses and executes completed DCS payloads.

    If payload begins with 'G', payload is passed to kitty graphics extension.
*/
static inline void _sfte_parser_dcs_dispatch(sfte_term *term, uint8_t terminator) {
    const sfte_callbacks *cb = term->cb;
    sfte_parser_state *parser = &term->parser;

    const char *term_str = (terminator == '\x1b') ? "\033\\" : "\x07";
    parser->osc_payload[parser->osc_len] = '\0';

#if SFTE_IMG_KITTY
    if (parser->osc_payload[0] == 'G')
        _sfte_kitty_parse_graphics(term, parser->osc_payload + strlen("G"));
    else
#endif  // SFTE_IMG_KITTY
        if (strncmp(parser->osc_payload, "+q", 2) == 0) {
            char reply[128];
            size_t len = snprintf(reply, sizeof(reply), "\033P0+r%s%s",
                                  parser->osc_payload + strlen("+q"), term_str);
            if (cb->write) cb->write(cb->user_data, reply, len);
        }
}

/*
    Core VT500 byte routing table.

    TODO:
    In-depth documentation of how this function behaves and why
*/
static inline void _sfte_parser_feed_byte(sfte_term *term, uint8_t b) {
    const sfte_callbacks *cb = term->cb;
    sfte_parser_state *parser = &term->parser;
    sfte_cursor_state *cursor = &term->cursor;
#if SFTE_IMG_SIXEL
    sfte_sixel_state *sixel = &term->sixel;
#endif  // SFTE_IMG_SIXEL

    switch (b) {
    case '\n':
    case '\x0B':
    case '\x0C': _sfte_parser_c0_lf(term); return;
    case '\r': cursor->col = 0; return;
    case '\t': _sfte_parser_c0_ht(term); return;
    case '\b':
    case '\x7f':
        if (cursor->col > 0) cursor->col--;
        return;
    case '\a':
        if (cb->bell) cb->bell(cb->user_data);
        return;
    default: break;
    }

    switch (parser->state) {
    case VT_GROUND:
        if (b == '\033' || b == '\x1b')
            parser->state = VT_ESCAPE;
        else if (b >= 0x20) {
#if SFTE_TERM_ASCII_CHARSET
            if (b >= 0x80) {
                if (_sfte_rune_utf8_decode(parser, b))
                    b = '?';
                else
                    break;
            }
            _sfte_rune_insert(term, b);
#else   // !SFTE_TERM_ASCII_CHARSET
            if (_sfte_rune_utf8_decode(parser, b)) _sfte_rune_insert(term, parser->utf8_rune_acc);
#endif  // !SFTE_TERM_ASCII_CHARSET
        }
        break;
    case VT_ESCAPE:
        if (b == '[') {
            parser->state = VT_CSI_ENTRY;
            parser->param_idx = 0;
            parser->dec_priv = 0;
            parser->csi_inter = 0;
            memset(parser->params, 0, sizeof(parser->params));
        } else if (b == ']') {
            parser->state = VT_OSC;
            parser->osc_len = 0;
        } else if (b == 'c') {
            _sfte_parser_esc_ris(term);
            parser->state = VT_GROUND;
        } else if (b == '\\')
            parser->state = VT_GROUND;
        else if (b == 'P' || b == '_' || b == '^') {
            parser->state = VT_DCS;
            parser->osc_len = 0;
        } else if (b == '(' || b == ')')
            parser->state = VT_CHARSET;
        else if (b == '7') {
            _sfte_parser_esc_sc(term);
            parser->state = VT_GROUND;
        } else if (b == '8') {
            _sfte_parser_esc_rc(term);
            parser->state = VT_GROUND;
        } else if (b == '#')
            parser->state = VT_HASH;
        else if (b == 'D') {
            _sfte_parser_esc_ind(term);
            parser->state = VT_GROUND;
        } else if (b == 'M') {
            _sfte_parser_esc_ri(term);
            parser->state = VT_GROUND;
        } else if (b == 'E') {
            _sfte_parser_esc_nel(term);
            parser->state = VT_GROUND;
        } else if (b == 'H') {
            term->tab_stops[_SFTE_CLAMP(cursor->col, 0, term->viewport.cols - 1)] = 1;
            parser->state = VT_GROUND;
        } else
            parser->state = VT_GROUND;
        break;
    case VT_HASH:
        if (b == '8') _sfte_parser_hash_decaln(term);
        parser->state = VT_GROUND;
        break;
    case VT_CHARSET: parser->state = VT_GROUND; break;
    case VT_OSC:
        if (b == '\x07' || b == '\x1b') {
            _sfte_parser_osc_dispatch(term, b);
            parser->state = (b == '\x1b') ? VT_ESCAPE : VT_GROUND;
        } else
            _sfte_parser_append_payload(parser, b);
        break;
    case VT_DCS:
        if (b == '\x07' || b == '\x1b') {
            _sfte_parser_dcs_dispatch(term, b);
            parser->state = (b == '\x1b') ? VT_ESCAPE : VT_GROUND;
        }
#if SFTE_IMG_SIXEL
        else if (b == 'q') {
            // Detect if this dcs header is strictly sixel params (nums/semicols)
            uint8_t is_sixel = 1;
            for (size_t i = 0; i < parser->osc_len; ++i) {
                char pb = parser->osc_payload[i];
                if (pb != ';' && (pb < '0' || pb > '9')) {
                    is_sixel = 0;
                    break;
                }
            }
            if (is_sixel) {
                parser->state = VT_SIXEL;
                term->sixel = (sfte_sixel_state){
                    .start_col = cursor->col,
                    .start_row = cursor->row,
                    .state = SIXEL_GROUND,
                };
            } else
                _sfte_parser_append_payload(parser, b);
        }
#endif  // SFTE_IMG_SIXEL
        else
            _sfte_parser_append_payload(parser, b);
        break;
#if SFTE_IMG_SIXEL
    case VT_SIXEL:
        if (b == '\x1b' || b == '\x07') {
            _sfte_sixel_commit(sixel, term);
            parser->state = (b == '\x1b') ? VT_ESCAPE : VT_GROUND;
        } else
            _sfte_sixel_parse_byte(sixel, b);
        break;
#endif  // SFTE_IMG_SIXEL
    case VT_CSI_ENTRY:
    case VT_CSI_PARAM:
        if (b == '?') {  // private marker
            parser->state = VT_CSI_PARAM;
            parser->dec_priv = 1;
        } else if (b == '>') {
            parser->state = VT_CSI_PARAM;
            parser->dec_priv = 2;
        } else if (b == '<') {
            parser->state = VT_CSI_PARAM;
            parser->dec_priv = 3;
        } else if (b == '=') {
            parser->state = VT_CSI_PARAM;
            parser->dec_priv = 4;
        } else if (b >= '0' && b <= '9') {
            parser->state = VT_CSI_PARAM;
            parser->params[parser->param_idx] *= 10;
            parser->params[parser->param_idx] += (b - '0');
        } else if (b == ';' || b == ':') {
            if (parser->param_idx < 15) parser->param_idx++;  // Move to next parameter
        } else if (b >= 0x20 && b <= 0x2F) {
            parser->state = VT_CSI_PARAM;
            parser->csi_inter = b;
        } else if (b >= 0x40 && b <= 0x7E) {
            _sfte_csi_dispatch(term, b);
            parser->state = VT_GROUND;
        }
    }
}

// =================================================================================================
// >>public api
// =================================================================================================

void sfte_parse(sfte_term *term, const uint8_t *data, size_t len) {
    if (len == 0 || !data) return;

#if SFTE_TERM_SCROLLBACK_CAP
    sfte_viewport_state *vp = &term->viewport;
#endif  // SFTE_TERM_SCROLLBACK_CAP
#if SFTE_SEARCH
    sfte_search_state *search = &term->search;
#endif  // SFTE_SEARCH
#if SFTE_CURSOR_BLINK
    sfte_cursor_state *cursor = &term->cursor;
#endif  // SFTE_CURSOR_BLINK

#if SFTE_SEARCH
    // Update the search results if new output arrives
    if (search->is_active) _sfte_search_exec(term, search->text_input.buf);
#endif  // SFTE_SEARCH
#if SFTE_SEARCH && SFTE_TERM_SCROLLBACK_CAP
    else
#endif  // SFTE_SEARCH && SFTE_TERM_SCROLLBACK_CAP
#if SFTE_TERM_SCROLLBACK_CAP
        // If not in search mode, snap the view to bottom if new output arrives
        if (vp->sb_off > 0) {
            vp->sb_off = 0;
            _sfte_grid_dirty_rows(term, 0, vp->rows - 1);
        }
#endif  // SFTE_TERM_SCROLLBACK_CAP

#if SFTE_CURSOR_BLINK
    // reset blink timer when typing/outputting
    cursor->blink_visible = 1;
    cursor->next_blink_ms = SFTE_TIME_MS() + SFTE_CURSOR_BLINK_RATE_MS;
#endif  // SFTE_CURSOR_BLINK

    // parse incoming stream
    for (size_t i = 0; i < len; ++i) _sfte_parser_feed_byte(term, data[i]);
}
