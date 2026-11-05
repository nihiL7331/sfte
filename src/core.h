#pragma once

#include "src/mux.h"
#include "src/render.h"
#include "src/types.h"
#include "src/util.h"
#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <unistd.h>

static inline void _sfte_config_init(sfte_ctx *ctx, const sfte_config *cfg) {
    if (!cfg) {
        ctx->cfg = _sfte_default_config;
        return;
    }

    ctx->cfg = *cfg;

    sfte_config *c = &ctx->cfg;
    const sfte_config *def = &_sfte_default_config;

    if (c->font.default_size <= 0.0f) c->font.default_size = def->font.default_size;
    if (c->font.min_size <= 0.0f) c->font.min_size = def->font.min_size;
    if (c->font.max_size <= 0.0f) c->font.max_size = def->font.max_size;
    for (uint8_t i = 0; i < SFTE_FONT_MAX_COUNT; ++i)
        if (c->font.scales[i] <= 0.0f) c->font.scales[i] = def->font.scales[i];

    if (c->color.bg == 0x00000000) c->color.bg = def->color.bg;
    if (c->color.fg == 0x00000000) c->color.fg = def->color.fg;
    if (c->color.cursor == 0x00000000) c->color.cursor = def->color.cursor;
    uint8_t set_def = 1;  // if all colors from ansi palette are 0, set default
    for (uint8_t i = 0; i < 16; ++i)
        if (c->color.ansi_palette[i] != 0x00000000) {
            set_def = 0;
            break;
        }
    if (set_def)
        for (uint8_t i = 0; i < 16; ++i) c->color.ansi_palette[i] = def->color.ansi_palette[i];

    if (c->window.init_cols == 0) c->window.init_cols = def->window.init_cols;
    if (c->window.init_rows == 0) c->window.init_rows = def->window.init_rows;
    if (c->window.tab_width == 0) c->window.tab_width = def->window.tab_width;
    if (c->window.refresh_rate == 0) c->window.refresh_rate = def->window.refresh_rate;
    if (c->window.default_title == NULL) c->window.default_title = def->window.default_title;
    if (c->window.term_env == NULL) c->window.term_env = def->window.term_env;
#if SFTE_TERM_ANIMATE_SCREEN
    if (c->window.anim_dur_ms <= 0.0f) c->window.anim_dur_ms = def->window.anim_dur_ms;
#endif  // SFTE_TERM_ANIMATE_SCREEN

    if (c->cursor.blink_rate_ms == 0) c->cursor.blink_rate_ms = def->cursor.blink_rate_ms;
    if (c->cursor.thick_ratio <= 0.0f) c->cursor.thick_ratio = def->cursor.thick_ratio;
    if (c->cursor.trail_decay <= 0.0f) c->cursor.trail_decay = def->cursor.trail_decay;

    if (c->underline.thick_ratio <= 0.0f) c->underline.thick_ratio = def->underline.thick_ratio;
    if (c->underline.off_ratio <= 0.0f) c->underline.off_ratio = def->underline.off_ratio;

#if SFTE_TERM_SCROLLBACK
    if (c->scroll.scrollback_cap <= 0) c->scroll.scrollback_cap = def->scroll.scrollback_cap;
#endif  // SFTE_TERM_SCROLLBACK
    if (c->scroll.step == 0) c->scroll.step = def->scroll.step;
#if SFTE_TERM_SCROLL_SMOOTH
    if (c->scroll.decay <= 0.0f) c->scroll.decay = def->scroll.decay;
#endif  // SFTE_TERM_SCROLL_SMOOTH
    if (c->scroll.threshold <= 0.0) c->scroll.threshold = def->scroll.threshold;

#if SFTE_SEARCH
    if (c->search.prefix == NULL) c->search.prefix = def->search.prefix;
    if (c->search.bg_bar == 0x00000000) c->search.bg_bar = def->search.bg_bar;
    if (c->search.bg_active == 0x00000000) c->search.bg_active = def->search.bg_active;
    if (c->search.fg_active == 0x00000000) c->search.fg_active = def->search.fg_active;
    if (c->search.bg_match == 0x00000000) c->search.bg_match = def->search.bg_match;
    if (c->search.fg_match == 0x00000000) c->search.fg_match = def->search.fg_match;
#endif  // SFTE_SEARCH

#if SFTE_MUX_BAR
    if (c->mux_bar.format == NULL) c->mux_bar.format = def->mux_bar.format;
    if (c->mux_bar.bg_active == 0x00000000) c->mux_bar.bg_active = def->mux_bar.bg_active;
    if (c->mux_bar.bg_inactive == 0x00000000) c->mux_bar.bg_inactive = def->mux_bar.bg_inactive;
    if (c->mux_bar.bg_hover == 0x00000000) c->mux_bar.bg_hover = def->mux_bar.bg_hover;
    if (c->mux_bar.fg_active == 0x00000000) c->mux_bar.fg_active = def->mux_bar.fg_active;
    if (c->mux_bar.fg_inactive == 0x00000000) c->mux_bar.fg_inactive = def->mux_bar.fg_inactive;
    if (c->mux_bar.fg_hover == 0x00000000) c->mux_bar.fg_hover = def->mux_bar.fg_hover;
#endif  // SFTE_MUX_BAR

#if SFTE_MUX
    if (c->mux.preview_border_fg == 0x00000000)
        c->mux.preview_border_fg = def->mux.preview_border_fg;
    if (c->mux.preview_badge_bg == 0x00000000) c->mux.preview_badge_bg = def->mux.preview_badge_bg;
    if (c->mux.preview_bg == 0x00000000) c->mux.preview_bg = def->mux.preview_bg;
    if (c->mux.preview_hover_bg == 0x00000000) c->mux.preview_hover_bg = def->mux.preview_hover_bg;
#endif  // SFTE_MUX

    memcpy(ctx->palette, _sfte_palette_256, sizeof(_sfte_palette_256));
    for (uint8_t i = 0; i < 16; ++i) ctx->palette[i] = c->color.ansi_palette[i];

    SFTE_ASSERT(c->font.regular[0] != NULL,
                "cfg->font.regular must specify at least one primary font path");
    SFTE_ASSERT(c->font.min_size > 0.0f, "cfg->font.min_size must be > 0.0f");
    SFTE_ASSERT(c->font.max_size >= c->font.min_size,
                "cfg->font.max_size must be >= cfg->font.min_size");
    SFTE_ASSERT(c->font.default_size >= c->font.min_size &&
                    c->font.default_size <= c->font.max_size,
                "cfg->font.default_size must be between cfg->font.min_size and cfg->font.max_size");

    SFTE_ASSERT(c->window.init_cols > 0, "cfg->window.init_cols must be >0");
    SFTE_ASSERT(c->window.init_rows > 0, "cfg->window.init_rows must be >0");
    SFTE_ASSERT(c->window.tab_width > 0, "cfg->window.tab_width must be >0");
    SFTE_ASSERT(c->window.refresh_rate > 0, "cfg->window.refresh_rate must be >0");

#if SFTE_TERM_SCROLLBACK
    SFTE_ASSERT(c->scroll.scrollback_cap > 0,
                "cfg->scroll.scrollback_cap must be >0 when SFTE_TERM_SCROLLBACK is enabled");
#endif  // SFTE_TERM_SCROLLBACK
    SFTE_ASSERT(c->scroll.step > 0, "cfg->scroll.step must be >0");
    SFTE_ASSERT(c->scroll.threshold > 0.0, "cfg->scroll.threshold must be >0.0");
#if SFTE_TERM_SCROLL_SMOOTH
    SFTE_ASSERT(c->scroll.decay > 0.0f && c->scroll.decay <= 1.0f,
                "cfg->scroll.decay must be in (0.0f, 1.0f]");
#endif  // SFTE_TERM_SCROLL_SMOOTH

    SFTE_ASSERT(c->cursor.style >= SFTE_CURSOR_STYLE_BLOCK &&
                    c->cursor.style <= SFTE_CURSOR_STYLE_BAR,
                "invalid cfg->cursor.style enum value");
    SFTE_ASSERT(c->cursor.thick_ratio > 0.0f && c->cursor.thick_ratio <= 1.0f,
                "cfg->cursor.thick_ratio must be in (0.0f, 1.0f]");

    SFTE_ASSERT(c->underline.thick_ratio > 0.0f && c->underline.thick_ratio <= 1.0f,
                "cfg->underline.thick_ratio must be in (0.0f, 1.0f]");
    SFTE_ASSERT(c->underline.off_ratio >= -1.0f && c->underline.off_ratio <= 1.0f,
                "cfg->underline.off_ratio must be in [-1.0f, 1.0f]");

#if SFTE_MUX_BAR
    SFTE_ASSERT(c->mux_bar.align >= SFTE_MUX_BAR_ALIGN_LEFT &&
                    c->mux_bar.align <= SFTE_MUX_BAR_ALIGN_FILL,
                "invalid cfg->mux_bar.align enum value");
#endif  // SFTE_MUX_BAR
}

static inline void _sfte_config_load_fonts(sfte_ctx *ctx) {
    const sfte_config *cfg = &ctx->cfg;

    for (uint8_t i = 0; i < SFTE_FONT_MAX_COUNT && cfg->font.regular[i]; ++i)
        sfte_font_load_file(ctx, SFTE_FONT_STYLE_REGULAR, cfg->font.regular[i]);
#ifdef SFTE_FONT_BOLD
    for (uint8_t i = 0; i < SFTE_FONT_MAX_COUNT && cfg->font.bold[i]; ++i)
        sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD, cfg->font.bold[i]);
#endif  // defined(SFTE_FONT_BOLD)
#ifdef SFTE_FONT_ITALIC
    for (uint8_t i = 0; i < SFTE_FONT_MAX_COUNT && cfg->font.italic[i]; ++i)
        sfte_font_load_file(ctx, SFTE_FONT_STYLE_ITALIC, cfg->font.italic[i]);
#endif  // defined(SFTE_FONT_ITALIC)
#ifdef SFTE_FONT_BOLD_ITALIC
    for (uint8_t i = 0; i < SFTE_FONT_MAX_COUNT && cfg->font.bold_italic[i]; ++i)
        sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD_ITALIC, cfg->font.bold_italic[i]);
#endif  // defined(SFTE_FONT_BOLD_ITALIC)
}

sfte_ctx *sfte_init(sfte_write_cb write_fn, void *user_data, const sfte_config *cfg) {
    sfte_ctx *ctx = (sfte_ctx *)SFTE_CALLOC(1, sizeof(sfte_ctx));
    SFTE_ASSERT(ctx, "failed to allocate core context");

    _sfte_config_init(ctx, cfg);

    sfte_callbacks *cb = &ctx->cb;
    sfte_logger *logger = &ctx->logger;
    sfte_window_state *win = &ctx->window;
    sfte_term *term = sfte_term_get_active(ctx);
#if SFTE_INPUT_MOUSE
    sfte_parser_state *parser = &term->parser;
#endif  // SFTE_INPUT_MOUSE

    cb->write = write_fn;
    cb->user_data = user_data;

#ifndef SFTE_NO_LOGGING
    logger->func = SFTE_LOG_FUNC;
#endif  // !SFTE_NO_LOGGING

#if SFTE_INPUT_MOUSE
    parser->mouse_btn_state = 3;
#endif  // SFTE_INPUT_MOUSE

#if SFTE_TERM_FOCUS
    win->is_focused = 1;
#endif  // SFTE_TERM_FOCUS

    _sfte_config_load_fonts(ctx);

    return ctx;
}

void sfte_free(sfte_ctx *ctx) {
    if (!ctx) return;

    sfte_mux *mux = &ctx->mux;
    for (uint8_t i = 0; i < SFTE_MUX_MAX_WINDOWS; ++i) _sfte_term_free(&mux->terms[i]);

    sfte_font_state *font = &ctx->font;

#define FREE_CACHE(type)                                                                           \
    do {                                                                                           \
        for (int i = 0; i < type.num_fonts; ++i)                                                   \
            if (type.owns_ttf_buf[i]) SFTE_FREE(type.ttf_buf[i]);                                  \
        SFTE_FREE(type.atlas_pxs);                                                                 \
        SFTE_FREE(type.glyphs);                                                                    \
    } while (0)

    FREE_CACHE(font->regular);
#ifdef SFTE_FONT_BOLD
    FREE_CACHE(font->bold);
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    FREE_CACHE(font->italic);
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    FREE_CACHE(font->bold_italic);
#endif  // SFTE_FONT_BOLD_ITALIC

#undef FREE_CACHE

    SFTE_FREE(ctx);
}

#ifndef SFTE_NO_POSIX
pid_t sfte_posix_pty_spawn(sfte_ctx *ctx, int32_t *out_fd, uint16_t cols, uint16_t rows,
                           uint16_t px_w, uint16_t px_h) {
    const sfte_config *cfg = &ctx->cfg;

    struct winsize ws = {
        .ws_row = (unsigned short)rows,
        .ws_col = (unsigned short)cols,
        .ws_xpixel = (unsigned short)px_w,
        .ws_ypixel = (unsigned short)px_h,
    };

    pid_t pid = forkpty(out_fd, NULL, NULL, &ws);
    if (pid == -1) {
        _SFTE_ERROR(ctx, PTY_FORK_FAIL, errno);
        return -1;
    }

    if (pid == 0) {
        setenv("TERM", cfg->window.term_env, 1);
        char *shell = getenv("SHELL");
        if (!shell) {
            shell = (char *)"/bin/sh";
            _SFTE_WARN(ctx, SHELL_FALLBACK);
        }
        execlp(shell, shell, NULL);
        abort();  // if execlp returns, it failed to exec the shell
    }

    int flags = fcntl(*out_fd, F_GETFL, 0);
    fcntl(*out_fd, F_SETFL, flags | O_NONBLOCK);

    _SFTE_INFO(ctx, PTY_SPAWN);
    return pid;
}

void sfte_posix_pty_resize(const sfte_term *term, int32_t pty_fd, uint16_t px_w, uint16_t px_h) {
    if (pty_fd <= 0) return;

    const sfte_viewport_state *vp = &term->viewport;

    struct winsize ws = {
        .ws_row = (unsigned short)vp->rows,
        .ws_col = (unsigned short)vp->cols,
        .ws_xpixel = (unsigned short)px_w,
        .ws_ypixel = (unsigned short)px_h,
    };
    ioctl(pty_fd, TIOCSWINSZ, &ws);
}
#endif  // !SFTE_NO_POSIX

int32_t sfte_get_timeout_ms(sfte_ctx *ctx) {
    if (!ctx) return -1;

    const sfte_config *cfg = &ctx->cfg;
    const sfte_window_state *win = &ctx->window;
#if SFTE_MUX
    const sfte_mux *mux = &ctx->mux;
#endif  // SFTE_MUX
    const sfte_term *term = sfte_term_get_active(ctx);
    const sfte_cursor_state *cursor = &term->cursor;
#if SFTE_SEARCH
    const sfte_search_state *search = &term->search;
#endif  // SFTE_SEARCH
#if SFTE_TERM_SCROLL_SMOOTH
    const sfte_viewport_state *vp = &term->viewport;
#endif  // SFTE_TERM_SCROLL_SMOOTH
#if SFTE_CURSOR_TRAIL
    const sfte_trail_state *trail = &cursor->trail;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK
    const sfte_mouse_state *mouse = &term->mouse;
#endif  // SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK

#if SFTE_MUX
    if (mux->ui_dirty) return 0;
#endif  // SFTE_MUX
#if SFTE_SEARCH
    if (search->ui_dirty) return 0;
#endif  // SFTE_SEARCH

    int32_t frame_ms = 1000 / cfg->window.refresh_rate;
    if (frame_ms < 1) frame_ms = 1;

#if SFTE_TERM_ANIMATE_SCREEN
    if (term->is_animating) return frame_ms;
#endif  // SFTE_TERM_ANIMATE_SCREEN
#if SFTE_TERM_SCROLL_SMOOTH
    if (vp->is_scrolling) return frame_ms;
#endif  // SFTE_TERM_SCROLL_SMOOTH
#if SFTE_CURSOR_TRAIL
    if (trail->is_trailing) return frame_ms;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK
    if (mouse->sel_dragging && mouse->sel_scroll_dir != 0) return frame_ms;
#endif  // SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK

    int32_t timeout = -1;

#if SFTE_CURSOR_BLINK
    uint8_t can_blink = cursor->blink_enabled && !cursor->hidden;
#if SFTE_TERM_FOCUS
    can_blink &= win->is_focused;
#endif  // SFTE_TERM_FOCUS

    if (can_blink) {
        uint64_t now = SFTE_TIME_MS();
        int32_t time_to_next = (int32_t)(cursor->next_blink_ms - now);
        if (time_to_next < 0) time_to_next = 0;
        timeout = time_to_next;
    }
#endif  // SFTE_CURSOR_BLINK

    return timeout;
}

uint8_t sfte_tick(sfte_ctx *ctx) {
    if (!ctx) return 0;

    const sfte_config *cfg = &ctx->cfg;
#if SFTE_TERM_FOCUS
    sfte_window_state *win = &ctx->window;
#endif  // SFTE_TERM_FOCUS
#if SFTE_MUX
    sfte_mux *mux = &ctx->mux;
#endif  // SFTE_MUX
    sfte_term *term = sfte_term_get_active(ctx);
    const sfte_viewport_state *vp = &term->viewport;
    sfte_cursor_state *cursor = &term->cursor;
#if SFTE_SEARCH
    sfte_search_state *search = &term->search;
#endif  // SFTE_SEARCH
#if SFTE_CURSOR_TRAIL
    sfte_trail_state *trail = &cursor->trail;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK
    sfte_mouse_state *mouse = &term->mouse;
#endif  // SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK

    uint8_t needs_render = 0;

#if SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK
    if (mouse->sel_dragging && mouse->sel_scroll_dir != 0) {
        uint64_t now = SFTE_TIME_MS();
        if (now - mouse->sel_last_scroll_ms >= 50) {
            mouse->sel_last_scroll_ms = now;
            int32_t old_sb_off = vp->sb_off;
            sfte_view_scroll(ctx, mouse->sel_scroll_dir);
            if (vp->sb_off != old_sb_off) {
                int16_t edge_r = (mouse->sel_scroll_dir > 0) ? 0 : vp->rows - 1;
                _sfte_grid_dirty_rows(term, mouse->sel_start_row, mouse->sel_end_row, cfg);
                mouse->sel_end_row = _sfte_grid_vis2log(vp, edge_r);
                _sfte_grid_dirty_rows(term, mouse->sel_start_row, mouse->sel_end_row, cfg);
                needs_render = 1;
            }
        }
    }
#endif  // SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK

#if SFTE_SEARCH
    if (search->ui_dirty) {
        needs_render = 1;
        search->ui_dirty = 0;
    }
#endif  // SFTE_SEARCH
#if SFTE_MUX
    if (mux->ui_dirty) {
        needs_render = 1;
        mux->ui_dirty = 0;
    }
#endif  // SFTE_MUX
#if SFTE_TERM_ANIMATE_SCREEN
    if (term->is_animating) needs_render = 1;
#endif  // SFTE_TERM_ANIMATE_SCREEN

#if SFTE_TERM_SCROLL_SMOOTH
    if (vp->is_scrolling) needs_render = 1;
#endif  // SFTE_TERM_SCROLL_SMOOTH

#if SFTE_CURSOR_BLINK
    uint8_t can_blink = cursor->blink_enabled && !cursor->hidden;
#if SFTE_TERM_FOCUS
    can_blink &= win->is_focused;
#endif  // SFTE_TERM_FOCUS

    if (can_blink) {
        uint64_t now = SFTE_TIME_MS();
        if (now >= cursor->next_blink_ms) {
            cursor->blink_visible = !cursor->blink_visible;
            cursor->next_blink_ms = now + cfg->cursor.blink_rate_ms;

            int16_t vis_col, vis_row;
            _sfte_render_get_cursor_pos(ctx, &vis_col, &vis_row);
            _sfte_grid_get_cell(term, vis_col, vis_row, cfg)->dirty = 1;
            needs_render = 1;
        }
    }
#endif  // SFTE_CURSOR_BLINK

#if SFTE_CURSOR_TRAIL
    if (trail->is_trailing) {
        int16_t vis_col, vis_row;
        _sfte_render_get_cursor_pos(ctx, &vis_col, &vis_row);
        float target_rx = vis_col * vp->cell_width;
        float target_ry = vis_row * vp->cell_height;

        uint64_t now = SFTE_TIME_MS();
        if (trail->last_update_ms == 0) trail->last_update_ms = now;
        float dt_ms = (float)(now - trail->last_update_ms);
        trail->last_update_ms = now;

        float tx = target_rx - trail->x;
        float ty = target_ry - trail->y;

        // Snap to target if we're close enough to stop animating
        if (tx * tx + ty * ty <= 0.5f || !isfinite(trail->x) || !isfinite(trail->y)) {
            trail->is_trailing = 0;
            trail->x = target_rx;
            trail->y = target_ry;
            trail->last_update_ms = 0;
        } else {
            float decay = dt_ms * cfg->cursor.trail_decay;
            if (decay > 1.0f) decay = 1.0f;
            trail->x += tx * decay;
            trail->y += ty * decay;
        }
        needs_render = 1;
    }
#endif  // SFTE_CURSOR_TRAIL

    return needs_render;
}
