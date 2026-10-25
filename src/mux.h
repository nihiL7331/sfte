#pragma once

#include "src/font.h"
#include "src/grid.h"
#include "src/kitty.h"
#include "src/sixel.h"
#include "src/types.h"

#undef SFTE_MUX
#define SFTE_MUX 1

// =================================================================================================
// >>term
// =================================================================================================

/*
    Returns an unallocated terminal context struct index.
    This function is useful for PTY spawning for multiplexing with custom backend implementations.
    Returns -1 if none is found.
*/
static inline int8_t _sfte_term_get_new_idx(sfte_ctx *ctx) {
    sfte_mux *mux = &ctx->mux;

    for (uint8_t i = 0; i < SFTE_MUX_MAX_WINDOWS; ++i)
        if (!mux->terms[i].is_allocated) return (int8_t)i;
    return -1;
}

/*
    Initializes the provided `term` terminal context struct.
*/
static inline void _sfte_term_init(sfte_ctx *ctx, sfte_term *term, int16_t cols, int16_t rows) {
    if (!term) return;

    const sfte_config *cfg = &ctx->cfg;
    const sfte_callbacks *cb = &ctx->cb;
    sfte_viewport_state *vp = &term->viewport;
    const sfte_parser_state *parser = &term->parser;
#if SFTE_INPUT_HYPERLINKS
    const sfte_link_state *links = &term->links;
#endif  // SFTE_INPUT_HYPERLINKS
#if SFTE_IMG_SIXEL
    sfte_sixel_state *sixel = &term->sixel;
#endif  // SFTE_IMG_SIXEL
    sfte_stack *stack = &term->stack;
    sfte_render_buffers *render = &term->render;
    sfte_modes_state *modes = &term->modes;

    memset(term, 0, sizeof(sfte_term));

    void *stack_back_buf = SFTE_MALLOC(SFTE_MEM_STACK_SIZE);
    _sfte_mem_stack_init(stack, stack_back_buf, SFTE_MEM_STACK_SIZE);

    term->tab_width = cfg->window.tab_width;
    _sfte_grid_resize_tabs(term, 0, cols, cfg);

    term->is_allocated = 1;
    vp->cols = cols;
    vp->rows = rows;
    modes->auto_wrap = 1;
    vp->scroll_bot = rows - 1;

    term->cb = cb;

#if SFTE_FONT_LIGATURES
    render->shaper_ids = (uint16_t *)SFTE_CALLOC(cols, sizeof(uint16_t));
    render->row_hashes = (uint64_t *)SFTE_CALLOC(rows, sizeof(uint64_t));
    render->row_shaper_ids = (uint16_t *)SFTE_CALLOC(cols * rows, sizeof(uint16_t));
#endif  // SFTE_FONT_LIGATURES
    render->ids = (uint16_t *)SFTE_CALLOC(cols, sizeof(uint16_t));
    render->font_indices = (uint8_t *)SFTE_CALLOC(cols, sizeof(uint8_t));
    render->target_caches = (sfte_font_cache **)SFTE_CALLOC(cols, sizeof(sfte_font_cache *));

#if SFTE_IMG_SIXEL
    memcpy(sixel->palette, ctx->palette, sizeof(ctx->palette));
#endif  // SFTE_IMG_SIXEL

#if SFTE_TERM_SCROLLBACK
    term->scrollback = (sfte_cell *)SFTE_CALLOC(cfg->scroll.scrollback_cap * vp->cols,
                                                sizeof(sfte_cell));
#endif  // SFTE_TERM_SCROLLBACK

#if SFTE_CURSOR_BLINK
    term->cursor.blink_enabled = 1;
    term->cursor.blink_visible = 1;
    term->cursor.next_blink_ms = SFTE_TIME_MS() + cfg->cursor.blink_rate_ms;
#endif  // SFTE_CURSOR_BLINK

#if SFTE_CURSOR_DYNAMIC
    term->cursor.color = cfg->color.cursor;
    term->cursor.style = cfg->cursor.style;
#endif  // SFTE_CURSOR_DYNAMIC

#if SFTE_UNDERLINE_COLORED
    term->sgr.ul_color = cfg->color.fg;
#endif  // SFTE_UNDERLINE_COLORED
    term->sgr.fg = cfg->color.fg;
    term->sgr.bg = cfg->color.bg;

    term->parser.osc_cap = SFTE_OSC_INIT_CAP;
    term->parser.osc_payload = (char *)SFTE_MALLOC(parser->osc_cap);

#if SFTE_INPUT_HYPERLINKS
    term->links.pool_cap = SFTE_INPUT_HYPERLINKS_INIT_CAP;
    term->links.pool = (char **)SFTE_CALLOC(links->pool_cap, sizeof(char *));
    term->links.pool_len = 1;  // Index 0 is reserved for no link
#endif                         // SFTE_INPUT_HYPERLINKS

    term->cells = (sfte_cell *)SFTE_CALLOC(cols * rows, sizeof(sfte_cell));
    SFTE_ASSERT(term->cells, "failed to allocate terminal grid");
}

/*
    Frees the memory used for provided `term` terminal context struct.
*/
static inline void _sfte_term_free(sfte_term *term) {
    if (!term || !term->is_allocated) return;

#if SFTE_INPUT_HYPERLINKS
    sfte_link_state *links = &term->links;
#endif  // SFTE_INPUT_HYPERLINKS
#if SFTE_IMG_SIXEL
    sfte_sixel_state *sixel = &term->sixel;
#endif  // SFTE_IMG_SIXEL
#if SFTE_IMG_KITTY
    sfte_kitty_state *kitty = &term->kitty;
#endif  // SFTE_IMG_KITTY
    sfte_parser_state *parser = &term->parser;
    sfte_image_state *images = &term->images;
    sfte_stack *stack = &term->stack;
    sfte_render_buffers *render = &term->render;

    SFTE_FREE(stack->buf);
#if SFTE_IMG_SIXEL
    _sfte_sixel_deinit(sixel);
#endif  // SFTE_IMG_KITTY
#if SFTE_IMG_KITTY
    _sfte_kitty_deinit(kitty);
#endif  // SFTE_IMG_KITTY

#if SFTE_FONT_LIGATURES
    SFTE_FREE(render->shaper_ids);
    SFTE_FREE(render->row_hashes);
    SFTE_FREE(render->row_shaper_ids);
#endif  // SFTE_FONT_LIGATURES
    SFTE_FREE(render->ids);
    SFTE_FREE(render->font_indices);
    SFTE_FREE(render->target_caches);

    SFTE_FREE(term->tab_stops);
    SFTE_FREE(parser->osc_payload);
    SFTE_FREE(term->cells);

#if SFTE_TERM_ALT_SCREEN
    SFTE_FREE(term->alt_cells);
#endif  // SFTE_TERM_ALT_SCREEN

#if SFTE_TERM_SCROLLBACK
    SFTE_FREE(term->scrollback);
#endif  // SFTE_TERM_SCROLLBACK

#if SFTE_INPUT_HYPERLINKS
    if (links->pool) {
        // Starts at 1 because index 0 is reserved for no link
        for (uint32_t i = 1; i < links->pool_len; ++i) SFTE_FREE(links->pool[i]);
        SFTE_FREE(links->pool);
    }
#endif  // SFTE_INPUT_HYPERLINKS

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    if (images->pool) {
        for (uint32_t i = 0; i < images->pool_len; ++i) SFTE_FREE(images->pool[i].pixels);
        SFTE_FREE(images->pool);
    }
    if (images->placements) SFTE_FREE(images->placements);
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

    term->is_allocated = 0;
}
// =================================================================================================
// >>mux
// =================================================================================================
#if SFTE_MUX

#if SFTE_CURSOR_TRAIL
#define _SFTE_MUX_SWITCH_FOCUS(ctx_ptr, old_idx, new_idx)                                          \
    do {                                                                                           \
        const sfte_config *_cfg = &(ctx_ptr)->cfg;                                                 \
        sfte_term *_old = &(ctx_ptr)->mux.terms[old_idx];                                          \
        sfte_term *_new = &(ctx_ptr)->mux.terms[new_idx];                                          \
        const sfte_viewport_state *_new_vp = &(_new)->viewport;                                    \
        if ((old_idx) != (new_idx)) {                                                              \
            _new->cursor.trail = _old->cursor.trail;                                               \
            _new->cursor.trail.is_trailing = 1;                                                    \
            _new->cursor.trail.last_move_ms = SFTE_TIME_MS();                                      \
        }                                                                                          \
        int32_t sb_off = _sfte_grid_get_sb_off(_new_vp);                                           \
        _sfte_grid_dirty_rows(_new, -sb_off, -sb_off + _new->viewport.rows - 1, _cfg);             \
        (ctx_ptr)->window.pad_dirty = 1;                                                           \
        (ctx_ptr)->mux.active_idx = (new_idx);                                                     \
    } while (0)
#else
#define _SFTE_MUX_SWITCH_FOCUS(ctx_ptr, old_idx, new_idx)                                          \
    do {                                                                                           \
        sfte_term *_new = &(ctx_ptr)->mux.terms[new_idx];                                          \
        const sfte_viewport_state *_new_vp = &(_new)->viewport;                                    \
        int32_t sb_off = _sfte_grid_get_sb_off(_new_vp);                                           \
        _sfte_grid_dirty_rows(_new, -sb_off, -sb_off + _new->viewport.rows - 1);                   \
        (ctx_ptr)->window.pad_dirty = 1;                                                           \
        (ctx_ptr)->mux.active_idx = (new_idx);                                                     \
    } while (0)
#endif

/*
    Returns the visual position of `slot` in `tab_order`, or -1 if not found.
*/
static inline int8_t _sfte_mux_slot_to_pos(const sfte_mux *mux, uint8_t slot) {
    for (uint8_t pos = 0; pos < mux->tab_cnt; ++pos)
        if (mux->tab_order[pos] == slot) return (int8_t)pos;
    return -1;
}

/*
    Evaluates and returns multiplexer layout information based on bar configuration.
    Returns 0-ed struct if `SFTE_MUX_BAR` is 0.
*/
static inline sfte_mux_layout _sfte_mux_get_layout(const sfte_viewport_state *vp,
                                                   const sfte_config *cfg) {
    sfte_mux_layout l = {0};
#if SFTE_MUX_BAR
    l.reserved_rows = cfg->mux_bar.hover ? 0 : 1;
    if (cfg->mux_bar.top && !cfg->mux_bar.hover) l.grid_off_y = vp->cell_height;
    int16_t bar_row = cfg->mux_bar.top ? -1 : cfg->mux_bar.hover ? vp->rows - 2 : vp->rows - 1;
    l.bar_y = (bar_row + 1) * vp->cell_height + cfg->window.pad_y;
#else   // !SFTE_MUX_BAR
    (void)vp;
#endif  // !SFTE_MUX_BAR
    return l;
}

/*
    Toggled when the prefix keys are pressed.
    Next key will be consumed to determine the action.

    NOTE:
    This function returns 2 rather than 1 to indicate that the prefix key was pressed and consumed,
    but the next key is still expected to be consumed as part of the prefix sequence.
*/
static inline uint8_t _sfte_mux_prefix_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_mux *mux = &ctx->mux;

    if (!mux->is_prefix_active) {
        mux->is_prefix_active = 1;
#if SFTE_MUX_PREFIX_PREVIEW
        mux->preview_selected_idx = mux->active_idx;
#endif  // STE_MUX_PREFIX_PREVIEW
        mux->ui_dirty = 1;
    }

    return 2;
}

/*
    Spawns a new terminal window.
    For it to be supported on custom backends,
    logic checking whether spawn is pending via `mux->pending_spawn` needs to be implemented.
    See `_sfte_wayland_loop` for an example.
*/
static inline uint8_t _sfte_mux_spawn_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_mux *mux = &ctx->mux;

    if (!mux->is_prefix_active) return 0;

    mux->is_prefix_active = 0;
    mux->pending_spawn = 1;
    mux->ui_dirty = 1;

    return 1;
}

/*
    Closes the currently open terminal window.
    For it to be supported on custom backends,
    logic checking whether spawn is pending via `mux->pending_close` needs to be implemented.
    See `_sfte_wayland_loop` for an example.
*/
static inline uint8_t _sfte_mux_close_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_mux *mux = &ctx->mux;

    if (!mux->is_prefix_active) return 0;

    mux->is_prefix_active = 0;
    mux->pending_close = 1;
    mux->ui_dirty = 1;

    return 1;
}

/*
    Jumps to the window offset from active terminal window by `arg->i`.
    Wraps the index value in range.
*/
static inline uint8_t _sfte_mux_jump_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_mux *mux = &ctx->mux;

    if (!mux->is_prefix_active) return 0;

    mux->is_prefix_active = 0;
    int step = arg->i;
    if (step == 0 || mux->tab_cnt == 0) return 1;

    int8_t cur_pos = _sfte_mux_slot_to_pos(mux, mux->active_idx);
    if (cur_pos < 0) return 1;

    int8_t target_pos = ((cur_pos + step) % mux->tab_cnt + mux->tab_cnt) % mux->tab_cnt;
    uint8_t target_slot = mux->tab_order[target_pos];
    _SFTE_MUX_SWITCH_FOCUS(ctx, mux->active_idx, target_slot);

    mux->ui_dirty = 1;

    return 1;
}

/*
    Jumps to the terminal window of index `arg->i`.
*/
static inline uint8_t _sfte_mux_goto_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_mux *mux = &ctx->mux;

    if (!mux->is_prefix_active) return 0;

    mux->is_prefix_active = 0;
    int8_t target_pos = arg->i;

    if (target_pos >= 0 && target_pos < mux->tab_cnt) {
        uint8_t target_slot = mux->tab_order[target_pos];
        _SFTE_MUX_SWITCH_FOCUS(ctx, mux->active_idx, target_slot);
    }

    mux->ui_dirty = 1;

    return 1;
}

/*
    Toggles the rename mode.
*/
static inline uint8_t _sfte_mux_rename_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_mux *mux = &ctx->mux;
    sfte_term *term = sfte_term_get_active(ctx);

    if (!term || !mux || !mux->is_prefix_active) return 0;

    mux->is_renaming = 1;
    mux->ui_dirty = 1;
    sfte_text_input_state *in = &mux->text_input;
    in->len = snprintf(in->buf, SFTE_INPUT_TEXT_MAX_LEN, "%s", term->title);
    in->cursor_byte_idx = in->len;

    return 1;
}

/*
    Swaps two adjacent window tabs.
    If `arg->i` is 1, swaps the currently open tab with the one to the right.
    If `arg->i` is -1, swaps the currently open tab with the one to the left.
*/
static inline uint8_t _sfte_mux_reorder_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_mux *mux = &ctx->mux;

    if (!mux->is_prefix_active) return 0;

    mux->is_prefix_active = 0;
    mux->pending_reorder = (int8_t)arg->i;

    return 1;
}
#endif  // SFTE_MUX

// =================================================================================================
// >>public api
// =================================================================================================

sfte_term *sfte_term_get_at(sfte_ctx *ctx, uint8_t idx) {
    if (idx >= SFTE_MUX_MAX_WINDOWS) return NULL;
    sfte_term *term = &ctx->mux.terms[idx];
    return term->is_allocated ? term : NULL;
}

sfte_term *sfte_term_get_active(sfte_ctx *ctx) {
    sfte_mux *mux = &ctx->mux;
    return &mux->terms[mux->active_idx];
}

int8_t sfte_term_get_idx(sfte_ctx *ctx, sfte_term *term) {
    if (!ctx || !term) return -1;
    sfte_mux *mux = &ctx->mux;
    return (int8_t)(term - mux->terms);
}

sfte_term *sfte_term_spawn(sfte_ctx *ctx) {
    const sfte_config *cfg = &ctx->cfg;
    sfte_mux *mux = &ctx->mux;

    int8_t idx = _sfte_term_get_new_idx(ctx);
    if (idx == -1) return NULL;
    sfte_term *term = &mux->terms[idx];

    sfte_term *active_term = sfte_term_get_active(ctx);
    if (active_term && !active_term->is_allocated) active_term = NULL;
    int16_t cols = active_term ? active_term->viewport.cols : cfg->window.init_cols;
    int16_t rows = active_term ? active_term->viewport.rows : cfg->window.init_rows;
    int16_t cell_w = active_term ? active_term->viewport.cell_width : 0;
    int16_t cell_h = active_term ? active_term->viewport.cell_height : 0;

    _sfte_term_init(ctx, term, cols, rows);
    term->viewport.cell_width = cell_w;
    term->viewport.cell_height = cell_h;
    mux->tab_order[mux->tab_cnt++] = idx;

    if (ctx->font.regular.num_fonts > 0) _sfte_font_reset_cache(ctx);

    return term;
}

uint8_t sfte_term_close(sfte_ctx *ctx, sfte_term *term) {
    if (!ctx || !term || !term->is_allocated) return 0;

#if SFTE_MUX
    sfte_mux *mux = &ctx->mux;
#endif  // SFTE_MUX

    _sfte_term_free(term);

#if SFTE_MUX
    int8_t closed_idx = sfte_term_get_idx(ctx, term);

    // Update tab order
    uint8_t pos = 0;
    for (; pos < mux->tab_cnt; ++pos)  // Find where closed tab is in tab_order
        if (mux->tab_order[pos] == closed_idx) break;
    for (; pos + 1 < mux->tab_cnt; ++pos)  // Shift subsequent tabs left by 1
        mux->tab_order[pos] = mux->tab_order[pos + 1];
    mux->tab_cnt--;

    // If closed the active window, shift focus
    if (mux->active_idx == closed_idx) {
        int8_t next_idx = -1;

        for (int8_t i = closed_idx - 1; i >= 0; i--)
            if (mux->terms[i].is_allocated) {
                next_idx = i;
                break;
            }

        if (next_idx == -1)
            for (int8_t i = closed_idx + 1; i < SFTE_MUX_MAX_WINDOWS; ++i)
                if (mux->terms[i].is_allocated) {
                    next_idx = i;
                    break;
                }

        if (next_idx != -1) {
            _SFTE_MUX_SWITCH_FOCUS(ctx, closed_idx, next_idx);
            return 1;  // Shifting focus to another window
        }

        return 0;  // No windows left, closing app
    }

    return 1;  // Closed a background window, still running

#else  // !SFTE_MUX

    return 0;  // Only one window alive at a time, closing it ends the app

#endif  // !SFTE_MUX
}
