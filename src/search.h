#pragma once

#include "src/grid.h"
#include "src/types.h"

#if SFTE_SEARCH
#ifndef SFTE_REGEX_CUSTOM_BACKEND
#if SFTE_NO_POSIX

#include "vendor/re.h"

static inline void *_sfte_search_tiny_re_init(sfte_stack *stack, const char *pattern,
                                              uint8_t ignore_case) {
    (void)stack, (void)ignore_case;
    return (void *)re_compile(pattern);
}

static inline uint8_t _sfte_search_tiny_re_match(void *re_ctx, const char *text, int32_t *out_idx,
                                                 int32_t *out_len) {
    int32_t match_len = 0;
    int32_t match_idx = re_matchp(re_ctx, text, &match_len);
    if (match_idx >= 0) {
        *out_idx = match_idx;
        *out_len = match_len;
        return 1;
    }
    return 0;
}

static inline void _sfte_search_tiny_re_free(sfte_stack *stack, void *re_ctx) {
    (void)stack, (void)re_ctx;
}

#define SFTE_SEARCH_RE_INIT _sfte_search_tiny_re_init
#define SFTE_SEARCH_RE_MATCH _sfte_search_tiny_re_match
#define SFTE_SEARCH_RE_FREE _sfte_search_tiny_re_free

#else  // !SFTE_NO_POSIX

#include <regex.h>

static inline void *_sfte_search_posix_re_init(sfte_stack *stack, const char *pattern,
                                               uint8_t ignore_case) {
    regex_t *re = (regex_t *)_sfte_mem_stack_alloc(stack, sizeof(regex_t), _Alignof(regex_t));
    if (!re) return NULL;

    uint8_t flags = REG_EXTENDED;
    if (ignore_case) flags |= REG_ICASE;
    if (regcomp(re, pattern, flags)) return NULL;
    return (void *)re;
}

static inline uint8_t _sfte_search_posix_re_match(void *re_ctx, const char *text, int32_t *out_idx,
                                                  int32_t *out_len) {
    if (!re_ctx) return 0;
    regex_t *re = (regex_t *)re_ctx;
    regmatch_t pmatch[1];

    if (!regexec(re, text, 1, pmatch, 0)) {
        // `rm_so` and `rm_eo` are of type `off_t`, which is a signed integer
        // For simplicitys sake and custom backends implementations they're casted
        *out_idx = (int32_t)pmatch[0].rm_so;
        *out_len = (int32_t)(pmatch[0].rm_eo - pmatch[0].rm_so);
        return 1;
    }
    return 0;
}

static inline void _sfte_search_posix_re_free(sfte_stack *stack, void *re_ctx) {
    (void)stack;
    if (re_ctx) regfree((regex_t *)re_ctx);
}

#define SFTE_SEARCH_RE_INIT _sfte_search_posix_re_init
#define SFTE_SEARCH_RE_MATCH _sfte_search_posix_re_match
#define SFTE_SEARCH_RE_FREE _sfte_search_posix_re_free

#endif  // !SFTE_NO_POSIX
#endif  // !SFTE_REGEX_CUSTOM_BACKEND

/*
    Used for default search shortcuts.
    Toggles (on/off) the search bar visibility.
*/
static inline uint8_t _sfte_search_toggle_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    const sfte_config *cfg = &ctx->cfg;
#if SFTE_MUX
    const sfte_mux *mux = &ctx->mux;
#endif  // SFTE_MUX
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_search_state *search = &term->search;
    const sfte_modes_state *modes = &term->modes;
    const sfte_viewport_state *vp = &term->viewport;

#if SFTE_TERM_ALT_SCREEN
    if (modes->alt_active) return 0;
#endif  // SFTE_TERM_ALT_SCREEN
#if SFTE_MUX
    if (mux->is_renaming) return 0;
#endif  // SFTE_MUX

    if (search->is_active) {
        const sfte_arg clear_arg = {.i = 1};
        return _sfte_search_clear_shortcut(ctx, &clear_arg);
    } else {
        search->pre_off = 0;
        search->is_active = 1;
        search->text_input.buf[0] = '\0';
        search->text_input.len = 0;
        search->text_input.cursor_byte_idx = 0;

        uint16_t row = cfg->search.top ? 0 : vp->rows - 1;
        _sfte_grid_dirty_rows(term, row, row, cfg);
        search->ui_dirty = 1;

        return search->is_active;
    }
}

/*
    Jumps to the currently active search match.
*/
static inline void _sfte_search_jump(sfte_ctx *ctx, int8_t delta) {
    const sfte_config *cfg = &ctx->cfg;
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_search_state *search = &term->search;
    const sfte_viewport_state *vp = &term->viewport;

    if (!search->match_cnt) return;

    search->active_match_idx += delta + search->match_cnt;
    search->active_match_idx %= search->match_cnt;

    int32_t match_logical_r = search->matches[search->active_match_idx].logical_r;
    int32_t top_logical_r = -vp->sb_off;
    int32_t bot_logical_r = top_logical_r + vp->rows - 2;  // -2 because search takes row

    if (match_logical_r < top_logical_r || match_logical_r > bot_logical_r) {
        int32_t target_off = -match_logical_r + (vp->rows / 2);
        if (target_off < 0) target_off = 0;
        sfte_view_scroll(ctx, target_off + top_logical_r);
    }

    // If `delta` is 0, sfte_view_scroll will NOT dirty the screen,
    // so force a redraw here just in case.
    top_logical_r = -vp->sb_off;
    bot_logical_r = top_logical_r + vp->rows - 1;

    _sfte_grid_dirty_rows(term, top_logical_r, bot_logical_r, cfg);
    search->ui_dirty = 1;
}

/*
    Used for default search shortcuts.
*/
static inline uint8_t _sfte_search_prev_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_search_state *search = &term->search;

    _sfte_search_jump(ctx, -1);
    return search->is_active;
}

/*
    Used for default search shortcuts.
*/
static inline uint8_t _sfte_search_next_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_search_state *search = &term->search;

    _sfte_search_jump(ctx, 1);
    return search->is_active;
}

/*
    Frees the compiled regex context and clears the match list.
*/
static inline void _sfte_search_clear(sfte_term *term, const sfte_config *cfg) {
    sfte_search_state *search = &term->search;
    sfte_stack *stack = &term->stack;
    const sfte_viewport_state *vp = &term->viewport;

    if (!search->is_active) return;

    if (search->compiled_re) {
        SFTE_SEARCH_RE_FREE(stack, search->compiled_re);
        search->compiled_re = NULL;
    }

    _sfte_mem_stack_rewind(stack, search->pre_off);
    search->is_active = 0;
    search->ui_dirty = 1;
    search->match_cnt = 0;
    search->match_cap = 0;
    search->matches = NULL;
    search->active_match_idx = 0;
    search->text_input.buf[0] = '\0';
    search->text_input.len = 0;
    search->text_input.cursor_byte_idx = 0;

    int32_t top_logical_r = -vp->sb_off;
    int32_t bot_logical_r = top_logical_r + vp->rows - 1;
    _sfte_grid_dirty_rows(term, top_logical_r, bot_logical_r, cfg);
}

/*
    Used for default search shortcuts.
    If `arg` is 0, exits unconditionally.
    If `arg` is 1, exits if query is empty.
*/
static inline uint8_t _sfte_search_clear_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    const sfte_config *cfg = &ctx->cfg;
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_search_state *search = &term->search;

    uint8_t ignore_query = arg->i;
    if (!ignore_query && search->text_input.len) return 0;

    uint8_t was_active = search->is_active;
    _sfte_search_clear(term, cfg);

    return was_active;
}

/*
    Reads a logical row and its wrapped continuations from the grid into a flat C string.
    Returns the number of physical rows consumed.
*/
static inline int16_t _sfte_search_extract_logical_line(const sfte_term *term, int32_t logical_row,
                                                        char *out_buf, size_t max_len,
                                                        const sfte_config *cfg) {
    const sfte_viewport_state *vp = &term->viewport;

    int16_t rows_consumed = 0;
    size_t buf_idx = 0;

    // Leave 4 bytes of room at the end to safely encode a 4-byte UTF-8 char + null terminator
    while (buf_idx < max_len - 5) {
        int32_t cur_r = logical_row + rows_consumed;

        uint8_t is_wrapped = 0;
#if SFTE_TERM_REFLOW
        is_wrapped = _sfte_grid_get_cell(term, vp->cols - 1, cur_r, cfg)->wrapped;
#endif  // SFTE_TERM_REFLOW

        // Find the true length of the row to ignore empty trailing cells
        int16_t actual_cols = vp->cols;
        if (!is_wrapped)
            while (actual_cols > 0) {
                sfte_cell *cell = _sfte_grid_get_cell(term, actual_cols - 1, cur_r, cfg);
                if ((cell->rune != 0 && cell->rune != ' ') || cell->bg != cfg->color.bg) break;
                actual_cols--;
            }

        // Encode runes to UTF-8 bytes
        for (int16_t c = 0; c < actual_cols; ++c) {
            sfte_cell *cell = _sfte_grid_get_cell(term, c, cur_r, cfg);
            uint32_t rune = cell->rune ? cell->rune : ' ';

            if (rune <= 0x7F) {
                out_buf[buf_idx++] = (char)rune;
            } else if (rune <= 0x7FF) {
                out_buf[buf_idx++] = (char)(0xC0 | (rune >> 6));
                out_buf[buf_idx++] = (char)(0x80 | (rune & 0x3F));
            } else if (rune <= 0xFFFF) {
                out_buf[buf_idx++] = (char)(0xE0 | (rune >> 12));
                out_buf[buf_idx++] = (char)(0x80 | ((rune >> 6) & 0x3F));
                out_buf[buf_idx++] = (char)(0x80 | (rune & 0x3F));
            }
        }

        rows_consumed++;

        if (!is_wrapped) break;
    }

    out_buf[buf_idx] = '\0';
    return rows_consumed;
}

/*
    Translates a 1D string match index back into a 2D grid coordinate.
*/
static inline void _sfte_search_map_to_coords(const sfte_term *term, int32_t start_logical_row,
                                              uint32_t match_idx, uint32_t match_len,
                                              sfte_search_match *out_match,
                                              const sfte_config *cfg) {
    const sfte_viewport_state *vp = &term->viewport;

    int32_t cur_r = start_logical_row;
    uint32_t cur_idx = 0;
    uint32_t end_idx = match_idx + match_len;
    uint8_t found_start = 0;
    uint16_t match_cell_len = 0;

    out_match->logical_r = start_logical_row;
    out_match->start_c = 0;
    out_match->len = 0;

    while (1) {
        uint8_t is_wrapped = 0;
#if SFTE_TERM_REFLOW
        is_wrapped = _sfte_grid_get_cell(term, vp->cols - 1, cur_r, cfg)->wrapped;
#endif  // SFTE_TERM_REFLOW

        int16_t actual_cols = vp->cols;
        if (!is_wrapped)
            while (actual_cols > 0) {
                sfte_cell *cell = _sfte_grid_get_cell(term, actual_cols - 1, cur_r, cfg);
                if ((cell->rune != 0 && cell->rune != ' ') || cell->bg != cfg->color.bg) break;
                actual_cols--;
            }

        for (int16_t c = 0; c < actual_cols; ++c) {
            // Hit start of the match
            if (cur_idx == match_idx) {
                out_match->logical_r = cur_r;
                out_match->start_c = c;
                found_start = 1;
            }

            // Hit end of the match
            if (cur_idx == end_idx) {
                out_match->len = match_cell_len;
                return;
            }

            sfte_cell *cell = _sfte_grid_get_cell(term, c, cur_r, cfg);
            uint32_t rune = cell->rune ? cell->rune : ' ';

            uint8_t byte_len = 0;
            if (rune <= 0x7F)
                byte_len = 1;
            else if (rune <= 0x7FF)
                byte_len = 2;
            else if (rune <= 0xFFFF)
                byte_len = 3;
            else
                byte_len = 4;

            cur_idx += byte_len;
            if (found_start) match_cell_len++;
        }

        if (cur_idx == end_idx) {
            out_match->len = match_cell_len;
            return;
        }

        if (!is_wrapped) break;
        cur_r++;
    }
}

/*
    Queries if a specific grid cell falls within the active search match boundaries.
    Returns 0 if is not highlighted.
    Returns 1 if is highlighted and ISN'T the current match.
    Returns 2 if is highlighted and IS the current match.
*/
static inline uint8_t _sfte_search_is_highlighted(const sfte_search_state *search,
                                                  const sfte_viewport_state *vp, int16_t col,
                                                  int32_t logical_row) {
    if (!search->is_active || !search->match_cnt) return 0;

    for (uint32_t i = 0; i < search->match_cnt; ++i) {
        sfte_search_match *m = &search->matches[i];

        // Calculate the vertical distance
        // If `row_diff` is negative, match is below
        int32_t row_diff = logical_row - m->logical_r;
        if (row_diff < 0) continue;

        // Check if the cell falls inside the 1D wrap distance
        int32_t cell_off = (row_diff * vp->cols) + col - m->start_c;
        if (cell_off >= 0 && cell_off < m->len) return (i == search->active_match_idx) ? 2 : 1;

        // If this match is above current row
        // and it's length isn't long enough to wrap down to current row,
        // no older match in the array will reach
        if (row_diff > (m->len / vp->cols) + 1) break;
    }

    return 0;
}

/*
    Compiles the regex, scans the scrollback via extraction helpers,
    populates the match list, and snaps the viewport to the most recent match.

    Populates the match list from the bottom (live screen), later going to scrollback.
*/
static inline void _sfte_search_exec(sfte_ctx *ctx, const char *query) {
    const sfte_config *cfg = &ctx->cfg;
    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_viewport_state *vp = &term->viewport;
    sfte_stack *stack = &term->stack;
    sfte_search_state *search = &term->search;

    char temp_query[SFTE_INPUT_TEXT_MAX_LEN];
    size_t query_len = strlen(query);
    if (query_len >= SFTE_INPUT_TEXT_MAX_LEN) query_len = SFTE_INPUT_TEXT_MAX_LEN;
    memcpy(temp_query, query, query_len);
    temp_query[query_len] = '\0';

    uint8_t ignore_case = 1;
    for (size_t i = 0; i < query_len; ++i)
        if (temp_query[i] >= 'A' && temp_query[i] <= 'Z') {
            ignore_case = 0;
            break;
        }

    if (search->compiled_re) {
        SFTE_SEARCH_RE_FREE(stack, search->compiled_re);
        search->compiled_re = NULL;
    }

    if (search->pre_off == 0)
        search->pre_off = _sfte_mem_stack_save(stack);
    else
        _sfte_mem_stack_rewind(stack, search->pre_off);

    search->is_active = 1;
    search->ignore_case = ignore_case;
    memcpy(search->text_input.buf, temp_query, query_len + 1);

    search->match_cnt = 0;
    search->matches = NULL;

    uint16_t row = cfg->search.top ? 0 : vp->rows - 1;
    _sfte_grid_dirty_rows(term, row, row, cfg);
    search->ui_dirty = 1;

    if (query_len == 0) return;

    search->compiled_re = SFTE_SEARCH_RE_INIT(stack, search->text_input.buf, ignore_case);

    if (!search->compiled_re) return;

    search->match_cap = SFTE_SEARCH_MATCH_INIT_CAP;
    search->matches = (sfte_search_match *)_sfte_mem_stack_alloc(
        stack, sizeof(sfte_search_match) * search->match_cap, _Alignof(sfte_search_match));

    int32_t min_logical_r = -(vp->sb_len < cfg->scroll.scrollback_cap ? vp->sb_len
                                                                      : cfg->scroll.scrollback_cap);
    int32_t max_logical_r = vp->rows - 1;

    char row_buf[SFTE_SEARCH_ROW_BUF_CAP];

    int32_t cur_r = max_logical_r;
    while (cur_r >= min_logical_r) {
        // Find start of this logical line block
        int32_t logical_start_r = cur_r;
        while (logical_start_r > min_logical_r) {
            // Check if previous row wrapped into this one
            int32_t prev_r = logical_start_r - 1;
            sfte_cell *last_cell = _sfte_grid_get_cell(term, vp->cols - 1, prev_r, cfg);
            if (!last_cell) break;
#if SFTE_TERM_REFLOW
            if (!last_cell->wrapped) break;
#endif  // SFTE_TERM_REFLOW
            logical_start_r--;
        }

        // Extract full logical line starting from head
        _sfte_search_extract_logical_line(term, logical_start_r, row_buf, SFTE_SEARCH_ROW_BUF_CAP,
                                          cfg);

        int32_t search_off = 0;
        int32_t match_idx, match_len;

        // Loop offset to catch lines with multiple hits
        while (SFTE_SEARCH_RE_MATCH(search->compiled_re, row_buf + search_off, &match_idx,
                                    &match_len)) {
            // Expand arena array if full
            if (search->match_cnt >= search->match_cap) {
                // Since there were no allocations between last alloc and this,
                // we can just allocate the CURRENT capacity (essentially doubling it).
                // The memory is continuous so after the allocation we can keep
                // adding to the array without any memcpy'ing and such.
                _sfte_mem_stack_alloc(stack, sizeof(sfte_search_match) * search->match_cap,
                                      _Alignof(sfte_search_match));
                search->match_cap *= 2;
            }

            sfte_search_match *m = &search->matches[search->match_cnt];
            _sfte_search_map_to_coords(term, logical_start_r, search_off + match_idx, match_len, m,
                                       cfg);
            search->match_cnt++;

            if (match_len == 0)
                search_off += match_idx + 1;
            else
                search_off += match_idx + match_len;

            if (search_off >= SFTE_SEARCH_ROW_BUF_CAP) break;
        }

        // Jump loop counter behind logical block just processed
        cur_r = logical_start_r - 1;
    }

    if (search->match_cnt > 0) {
        int32_t cur_center_r = -vp->sb_off + (vp->rows / 2);
        uint32_t closest_idx = 0;
        int32_t min_dist = INT32_MAX;

        for (uint32_t i = 0; i < search->match_cnt; ++i) {
            int32_t dist = search->matches[i].logical_r - cur_center_r;
            if (dist < 0) dist = -dist;

            if (dist < min_dist) {
                min_dist = dist;
                closest_idx = i;
            }
        }

        search->active_match_idx = closest_idx;
        _sfte_search_jump(ctx, 0);
    } else
        search->active_match_idx = 0;
}

#endif  // SFTE_SEARCH
