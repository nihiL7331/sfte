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

sfte_ctx *sfte_init(sfte_write_cb write_fn, void *user_data) {
    sfte_ctx *ctx = (sfte_ctx *)SFTE_CALLOC(1, sizeof(sfte_ctx));
    SFTE_ASSERT(ctx, "failed to allocate core context");

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
        setenv("TERM", SFTE_TERM_ENV, 1);
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
#if SFTE_INPUT_MOUSE && SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK_CAP
    const sfte_mouse_state *mouse = &term->mouse;
#endif  // SFTE_INPUT_MOUSE && SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK_CAP

#if SFTE_MUX
    if (mux->ui_dirty) return 0;
#endif  // SFTE_MUX
#if SFTE_SEARCH
    if (search->ui_dirty) return 0;
#endif  // SFTE_SEARCH

    int32_t frame_ms = 1000 / SFTE_TERM_REFRESH_RATE;
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
#if SFTE_INPUT_MOUSE && SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK_CAP
    if (mouse->sel_dragging && mouse->sel_scroll_dir != 0) return frame_ms;
#endif  // SFTE_INPUT_MOUSE && SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK_CAP

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
#if SFTE_INPUT_MOUSE && SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK_CAP
    sfte_mouse_state *mouse = &term->mouse;
#endif  // SFTE_INPUT_MOUSE && SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK_CAP

    uint8_t needs_render = 0;

#if SFTE_INPUT_MOUSE && SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK_CAP
    if (mouse->sel_dragging && mouse->sel_scroll_dir != 0) {
        uint64_t now = SFTE_TIME_MS();
        if (now - mouse->sel_last_scroll_ms >= 50) {
            mouse->sel_last_scroll_ms = now;
            int32_t old_sb_off = vp->sb_off;
            sfte_view_scroll(term, mouse->sel_scroll_dir);
            if (vp->sb_off != old_sb_off) {
                int16_t edge_r = (mouse->sel_scroll_dir > 0) ? 0 : vp->rows - 1;
                _sfte_grid_dirty_rows(term, mouse->sel_start_row, mouse->sel_end_row);
                mouse->sel_end_row = _sfte_grid_vis2log(vp, edge_r);
                _sfte_grid_dirty_rows(term, mouse->sel_start_row, mouse->sel_end_row);
                needs_render = 1;
            }
        }
    }
#endif  // SFTE_INPUT_MOUSE && SFTE_INPUT_SELECTION && SFTE_TERM_SCROLLBACK_CAP

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
            cursor->next_blink_ms = now + SFTE_CURSOR_BLINK_RATE_MS;

            int16_t vis_col, vis_row;
            _sfte_render_get_cursor_pos(ctx, &vis_col, &vis_row);
            _sfte_grid_get_cell(term, vis_col, vis_row)->dirty = 1;
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
            float decay = dt_ms * SFTE_CURSOR_TRAIL_DECAY;
            if (decay > 1.0f) decay = 1.0f;
            trail->x += tx * decay;
            trail->y += ty * decay;
        }
        needs_render = 1;
    }
#endif  // SFTE_CURSOR_TRAIL

    return needs_render;
}
