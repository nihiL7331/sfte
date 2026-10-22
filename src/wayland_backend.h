#pragma once

#define SFTE_WAYLAND 1

#if SFTE_WAYLAND

#include "src/core.h"
#include "src/types.h"
#include "src/util.h"
#include "vendor/xdg-shell.c"
#include "vendor/xdg-shell.h"
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

/*
    Returns the currently active shells file descriptor and process ID.
*/
static inline void _sfte_wayland_get_pty(sfte_wayland_app *app, int32_t **out_fd, pid_t **out_pid) {
    sfte_ctx *ctx = app->ctx;
    sfte_mux *mux = &ctx->mux;
    sfte_wayland_window_backend *backend = &app->win_backends[mux->active_idx];
    if (out_fd) *out_fd = &backend->pty_fd;
    if (out_pid) *out_pid = &backend->pty_pid;
}

/*
    Callback triggered by the emulator core to send bytes back to the shell (PTY).
*/
static inline void _sfte_wayland_write_cb(void *user_data, const char *data, size_t len) {
    sfte_wayland_app *app = (sfte_wayland_app *)user_data;
    int32_t *fd;
    _sfte_wayland_get_pty(app, &fd, NULL);
    if (fd) (void)write(*fd, data, len);
}

static inline void _sfte_wayland_pty_spawn(sfte_wayland_app *app) {
    sfte_ctx *ctx = app->ctx;
#if SFTE_MUX
    sfte_mux *mux = &ctx->mux;
#endif  // SFTE_MUX

    sfte_term *new_term = sfte_term_spawn(ctx);
    if (!new_term) return;  // Mux is full

#if SFTE_MUX
    int8_t idx = sfte_term_get_idx(ctx, new_term);
    uint8_t prev_idx = mux->active_idx;
    _SFTE_MUX_SWITCH_FOCUS(ctx, prev_idx, idx);
#endif  // SFTE_MUX

    int32_t *fd;
    pid_t *pid;
    _sfte_wayland_get_pty(app, &fd, &pid);

    const sfte_viewport_state *vp = &new_term->viewport;
    *pid = sfte_posix_pty_spawn(ctx, fd, vp->cols, vp->rows, app->width, app->height);

    if (*pid == -1) {
        sfte_term_close(ctx, new_term);
#if SFTE_MUX
        mux->active_idx = prev_idx;
#endif  // SFTE_MUX
        return;
    }
}

static inline void _sfte_wayland_pty_update(sfte_wayland_app *app) {
    sfte_ctx *ctx = app->ctx;
    sfte_mux *mux = &ctx->mux;
    for (uint8_t i = 0; i < SFTE_MUX_MAX_WINDOWS; ++i) {
        sfte_wayland_window_backend backend = app->win_backends[i];
        if (mux->terms[i].is_allocated && backend.pty_fd >= 0)
            sfte_posix_pty_resize(&mux->terms[i], backend.pty_fd, app->width, app->height);
    }
}

#if SFTE_FONT_ZOOM
static inline uint8_t _sfte_wayland_font_resize(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_zoom(ctx, arg->f);
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->cb.user_data;
    _sfte_wayland_pty_update(app);
    app->needs_render = 1;
    return 1;
}

static inline uint8_t _sfte_wayland_font_reset(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    const sfte_arg actual_arg = {.f = SFTE_FONT_DEFAULT_SIZE - ctx->font.cur_size};
    _sfte_wayland_font_resize(ctx, &actual_arg);
    return 1;
}
#endif  // SFTE_FONT_ZOOM

#if SFTE_TERM_SCROLLBACK_CAP
static inline uint8_t _sfte_wayland_view_scroll(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->cb.user_data;
    sfte_term *term = sfte_term_get_active(ctx);

    sfte_view_scroll(term, arg->i);
    app->needs_render = 1;

    return 1;
}
#endif  // SFTE_TERM_SCROLLBACK_CAP

/*
    Allocates a SHared Memory (SHM) buffer that both the emulator and
    the Wayland compositor can access simultaneously.
*/
static inline void _sfte_wayland_create_buffer(sfte_wayland_app *app) {
    int32_t stride = app->width * 4;  // 4 bytes per pixel (ARGB8888)
    app->shm_size = stride * app->height;

    // memfd_create provides an anonymous file descriptor backed by RAM, not disk
    int32_t fd = memfd_create("sfte-buffer", MFD_CLOEXEC);
    SFTE_ASSERT(fd != -1, "failed to create memfd");
    SFTE_ASSERT(ftruncate(fd, app->shm_size) != -1, "failed to truncate memfd");

    app->shm_data = (uint32_t *)mmap(NULL, app->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                     0);
    SFTE_ASSERT(app->shm_data != MAP_FAILED, "failed to mmap shm data");

    // NOTE:
    // If enabled, we allocate a secondary heap buffer for the emulator to draw into.
    // Once drawing is complete, we memcpy the damaged regions into the Wayland SHM
    // buffer to prevent the compositor from displaying half-drawn frames.
#if SFTE_TERM_DOUBLE_BUFFER
    if (app->back_buffer) SFTE_FREE(app->back_buffer);
    app->back_buffer = (uint32_t *)SFTE_MALLOC(app->shm_size);
    SFTE_ASSERT(app->back_buffer, "failed to allocate back buffer");
#endif  // SFTE_TERM_DOUBLE_BUFFER

    struct wl_shm_pool *pool = wl_shm_create_pool(app->shm, fd, app->shm_size);
    app->buffer = wl_shm_pool_create_buffer(pool, 0, app->width, app->height, stride,
                                            WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
}

static inline void _sfte_wayland_callback_listener_done(void *data, struct wl_callback *cb,
                                                        uint32_t time) {
    (void)time;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    wl_callback_destroy(cb);
    app->frame_pending = 0;
}

static const struct wl_callback_listener _sfte_wayland_callback_listener = {
    .done = _sfte_wayland_callback_listener_done,
};

#if SFTE_CLIPBOARD
static inline void _sfte_wayland_data_offer_offer(void *data, struct wl_data_offer *offer,
                                                  const char *mime_type) {
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (strcmp(mime_type, "text/plain;charset=utf-8") == 0) {
        snprintf(app->data_offer_mime, sizeof(app->data_offer_mime), "%s", mime_type);
        wl_data_offer_accept(offer, app->serial, mime_type);
    } else if (strcmp(mime_type, "UTF8_STRING") == 0 || strcmp(mime_type, "text/plain") == 0 ||
               strcmp(mime_type, "STRING") == 0 || strcmp(mime_type, "TEXT") == 0) {
        if (strcmp(app->data_offer_mime, "text/plain;charset=utf-8") != 0)
            snprintf(app->data_offer_mime, sizeof(app->data_offer_mime), "%s", mime_type);
        wl_data_offer_accept(offer, app->serial, mime_type);
    }
}

static inline void _sfte_wayland_data_offer_source_actions(void *data, struct wl_data_offer *offer,
                                                           uint32_t actions) {
    (void)data, (void)offer, (void)actions;
}

static inline void _sfte_wayland_data_offer_action(void *data, struct wl_data_offer *offer,
                                                   uint32_t action) {
    (void)data, (void)offer, (void)action;
}

static const struct wl_data_offer_listener _sfte_wayland_data_offer_listener = {
    .offer = _sfte_wayland_data_offer_offer,
    .source_actions = _sfte_wayland_data_offer_source_actions,
    .action = _sfte_wayland_data_offer_action,
};

static inline void _sfte_wayland_data_device_data_offer(void *data, struct wl_data_device *device,
                                                        struct wl_data_offer *offer) {
    (void)device;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    app->data_offer_mime[0] = '\0';
    wl_data_offer_add_listener(offer, &_sfte_wayland_data_offer_listener, data);
}

static inline void _sfte_wayland_data_device_enter(void *data, struct wl_data_device *device,
                                                   uint32_t serial, struct wl_surface *surface,
                                                   wl_fixed_t x, wl_fixed_t y,
                                                   struct wl_data_offer *offer) {
    (void)data, (void)device, (void)serial, (void)surface, (void)x, (void)y, (void)offer;
}

static inline void _sfte_wayland_data_device_leave(void *data, struct wl_data_device *device) {
    (void)data, (void)device;
}

static inline void _sfte_wayland_data_device_motion(void *data, struct wl_data_device *device,
                                                    uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)data, (void)device, (void)time, (void)x, (void)y;
}

static inline void _sfte_wayland_data_device_drop(void *data, struct wl_data_device *device) {
    (void)data, (void)device;
}

static inline void _sfte_wayland_data_device_selection(void *data, struct wl_data_device *device,
                                                       struct wl_data_offer *offer) {
    (void)device;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (app->data_offer && app->data_offer != offer) wl_data_offer_destroy(app->data_offer);
    app->data_offer = offer;
}

static const struct wl_data_device_listener _sfte_wayland_data_device_listener = {
    .data_offer = _sfte_wayland_data_device_data_offer,
    .enter = _sfte_wayland_data_device_enter,
    .leave = _sfte_wayland_data_device_leave,
    .motion = _sfte_wayland_data_device_motion,
    .drop = _sfte_wayland_data_device_drop,
    .selection = _sfte_wayland_data_device_selection,
};

static inline void _sfte_wayland_data_source_target(void *data, struct wl_data_source *src,
                                                    const char *mime_type) {
    (void)data, (void)src, (void)mime_type;
}

static inline void _sfte_wayland_data_source_send(void *data, struct wl_data_source *src,
                                                  const char *mime_type, int32_t fd) {
    (void)src, (void)mime_type;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (app->selection_text) (void)write(fd, app->selection_text, strlen(app->selection_text));

    close(fd);
}

static inline void _sfte_wayland_data_source_cancelled(void *data, struct wl_data_source *src) {
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    wl_data_source_destroy(src);
    if (app->selection_text) {
        SFTE_FREE(app->selection_text);
        app->selection_text = NULL;
    }
    app->data_source = NULL;
}

static inline void _sfte_wayland_data_source_dnd_drop_performed(void *data,
                                                                struct wl_data_source *src) {
    (void)data, (void)src;
}

static inline void _sfte_wayland_data_source_dnd_finished(void *data, struct wl_data_source *src) {
    (void)data, (void)src;
}

static inline void _sfte_wayland_data_source_action(void *data, struct wl_data_source *src,
                                                    uint32_t action) {
    (void)data, (void)src, (void)action;
}

static const struct wl_data_source_listener _sfte_wayland_data_source_listener = {
    .target = _sfte_wayland_data_source_target,
    .send = _sfte_wayland_data_source_send,
    .cancelled = _sfte_wayland_data_source_cancelled,
    .dnd_drop_performed = _sfte_wayland_data_source_dnd_drop_performed,
    .dnd_finished = _sfte_wayland_data_source_dnd_finished,
    .action = _sfte_wayland_data_source_action,
};
#endif  // SFTE_CLIPBOARD

#if SFTE_INPUT_SELECTION
static inline void _sfte_wayland_pointer_enter(void *data, struct wl_pointer *pointer,
                                               uint32_t serial, struct wl_surface *surface,
                                               wl_fixed_t surface_x, wl_fixed_t surface_y) {
    (void)data, (void)pointer, (void)serial, (void)surface, (void)surface_x, (void)surface_y;
#if SFTE_INPUT_MOUSE
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    sfte_ctx *ctx = app->ctx;
    app->mouse_x = wl_fixed_to_int(surface_x);
    app->mouse_y = wl_fixed_to_int(surface_y);
    sfte_mouse_move(ctx, app->mouse_x, app->mouse_y);
    app->needs_render = 1;
#endif  // SFTE_INPUT_MOUSE
}

static inline void _sfte_wayland_pointer_leave(void *data, struct wl_pointer *pointer,
                                               uint32_t serial, struct wl_surface *surface) {
    (void)data, (void)pointer, (void)serial, (void)surface;
}

static inline void _sfte_wayland_pointer_motion(void *data, struct wl_pointer *pointer,
                                                uint32_t time, wl_fixed_t surface_x,
                                                wl_fixed_t surface_y) {
    (void)data, (void)pointer, (void)time, (void)surface_x, (void)surface_y;
#if SFTE_INPUT_MOUSE
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    sfte_ctx *ctx = app->ctx;
    app->mouse_x = wl_fixed_to_int(surface_x);
    app->mouse_y = wl_fixed_to_int(surface_y);
    sfte_mouse_move(ctx, app->mouse_x, app->mouse_y);
    app->needs_render = 1;
#endif  // SFTE_INPUT_MOUSE
}

static inline void _sfte_wayland_pointer_button(void *data, struct wl_pointer *pointer,
                                                uint32_t serial, uint32_t time, uint32_t button,
                                                uint32_t state) {
    (void)data, (void)pointer, (void)serial, (void)time, (void)button, (void)state;
#if SFTE_INPUT_MOUSE
    if (button != 0x110 /* Wayland LMB */) return;

    sfte_wayland_app *app = (sfte_wayland_app *)data;
    sfte_ctx *ctx = app->ctx;

#if SFTE_CLIPBOARD
    app->serial = serial;
#endif  // SFTE_CLIPBOARD

    sfte_mouse_click(ctx, SFTE_MOUSE_BUTTON_LEFT, state == WL_POINTER_BUTTON_STATE_PRESSED,
                     app->mouse_x, app->mouse_y);
    app->needs_render = 1;

#if SFTE_CLIPBOARD && SFTE_INPUT_SELECTION
    if (state == WL_POINTER_BUTTON_STATE_RELEASED) _sfte_wayland_clipboard_copy(ctx, NULL);
#endif  // SFTE_CLIPBOARD && SFTE_INPUT_SELECTION
#endif  // SFTE_INPUT_MOUSE
}

static inline void _sfte_wayland_pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time,
                                              uint32_t axis, wl_fixed_t value) {
    (void)data, (void)pointer, (void)time, (void)axis, (void)value;
#if SFTE_INPUT_MOUSE
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) return;

    sfte_wayland_app *app = (sfte_wayland_app *)data;
    sfte_ctx *ctx = app->ctx;
    sfte_term *term = sfte_term_get_active(ctx);

    int8_t dir = (wl_fixed_to_double(value) < 0) ? 1 : -1;
    sfte_mouse_scroll(term, dir, app->mouse_x, app->mouse_y);
    app->needs_render = 1;
#endif  // SFTE_INPUT_MOUSE
}

static inline void _sfte_wayland_pointer_frame(void *data, struct wl_pointer *pointer) {
    (void)data, (void)pointer;
}

static inline void _sfte_wayland_pointer_axis_source(void *data, struct wl_pointer *pointer,
                                                     uint32_t axis_source) {
    (void)data, (void)pointer, (void)axis_source;
}

static inline void _sfte_wayland_pointer_axis_stop(void *data, struct wl_pointer *pointer,
                                                   uint32_t time, uint32_t axis) {
    (void)data, (void)pointer, (void)time, (void)axis;
}

static inline void _sfte_wayland_pointer_axis_discrete(void *data, struct wl_pointer *pointer,
                                                       uint32_t axis, int32_t discrete) {
    (void)data, (void)pointer, (void)axis, (void)discrete;
}

static const struct wl_pointer_listener _sfte_wayland_pointer_listener = {
    .enter = _sfte_wayland_pointer_enter,
    .leave = _sfte_wayland_pointer_leave,
    .motion = _sfte_wayland_pointer_motion,
    .button = _sfte_wayland_pointer_button,
    .axis = _sfte_wayland_pointer_axis,
    .frame = _sfte_wayland_pointer_frame,
    .axis_source = _sfte_wayland_pointer_axis_source,
    .axis_stop = _sfte_wayland_pointer_axis_stop,
    .axis_discrete = _sfte_wayland_pointer_axis_discrete,
};
#endif  // SFTE_INPUT_SELECTION

#if SFTE_INPUT_HYPERLINKS
static inline void _sfte_wayland_open_link_cb(void *user_data, const char *uri) {
    (void)user_data;
    if (!uri) return;

    pid_t pid = fork();
    if (pid == 0) {
        if (fork() == 0) {
            (void)freopen("/dev/null", "w", stdout);
            (void)freopen("/dev/null", "w", stderr);
            execlp("xdg-open", "xdg-open", uri, NULL);
            exit(1);
        }
        exit(0);
    } else if (pid > 0)
        waitpid(pid, NULL, 0);
}
#endif  // SFTE_INPUT_HYPERLINKS

static inline void _sfte_wayland_keyboard_keymap(void *data, struct wl_keyboard *keyboard,
                                                 uint32_t format, int32_t fd, uint32_t size) {
    (void)keyboard;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    SFTE_ASSERT(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, "unsupported keymap format");

    char *map_str = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    SFTE_ASSERT(map_str != MAP_FAILED, "failed to mmap keyboard");

    if (app->xkb_keymap) xkb_keymap_unref(app->xkb_keymap);
    if (app->xkb_state) xkb_state_unref(app->xkb_state);

    app->xkb_keymap = xkb_keymap_new_from_string(
        app->xkb_context, map_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    app->xkb_state = xkb_state_new(app->xkb_keymap);

    _SFTE_INFO(app->ctx, KEYMAP_LOADED);
    munmap(map_str, size);
    close(fd);  // close the fd to avoid leak
}

static inline void _sfte_wayland_keyboard_enter(void *data, struct wl_keyboard *keyboard,
                                                uint32_t serial, struct wl_surface *surface,
                                                struct wl_array *keys) {
    (void)data, (void)keyboard, (void)serial, (void)surface, (void)keys;
}

static inline void _sfte_wayland_keyboard_leave(void *data, struct wl_keyboard *keyboard,
                                                uint32_t serial, struct wl_surface *surface) {
    (void)keyboard, (void)serial, (void)surface;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (app->repeat_timer_fd > 0) {
        struct itimerspec its = {0};
        timerfd_settime(app->repeat_timer_fd, 0, &its, NULL);
    }
    app->repeating_key = 0;
}

static inline void _sfte_wayland_keyboard_key(void *data, struct wl_keyboard *keyboard,
                                              uint32_t serial, uint32_t time, uint32_t key,
                                              uint32_t state) {
    (void)data, (void)keyboard, (void)time;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    sfte_ctx *ctx = app->ctx;

#if SFTE_CLIPBOARD
    app->serial = serial;
#endif  // SFTE_CLIPBOARD

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED && key == app->repeating_key) {
        struct itimerspec its = {0};
        timerfd_settime(app->repeat_timer_fd, 0, &its, NULL);
        app->repeating_key = 0;
        return;
    }

    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !app->xkb_state) return;

    xkb_keycode_t keycode = key + 8;  // WARN: evdev codes are offset by 8 from xkb keycodes
    xkb_keysym_t sym = xkb_state_key_get_one_sym(app->xkb_state, keycode);
    uint8_t is_mod = (sym >= XKB_KEY_Shift_L && sym <= XKB_KEY_Hyper_R) ||
                     sym == XKB_KEY_ISO_Level3_Shift || sym == XKB_KEY_Mode_switch;
    uint8_t can_repeat = !is_mod &&
                         (!app->xkb_keymap || xkb_keymap_key_repeats(app->xkb_keymap, keycode));

    if (can_repeat && app->repeat_rate > 0 && app->repeating_key != key) {
        struct itimerspec its;
        its.it_value.tv_sec = app->repeat_delay / 1000;
        its.it_value.tv_nsec = (app->repeat_delay % 1000) * 1000000;
        its.it_interval.tv_sec = 0;
        if (app->repeat_rate > 0)
            its.it_interval.tv_nsec = 1000000000 / app->repeat_rate;
        else
            its.it_interval.tv_nsec = 0;

        timerfd_settime(app->repeat_timer_fd, 0, &its, NULL);
        app->repeating_key = key;
    }

    sfte_xkb_process_key(ctx, app->xkb_state, keycode);
    app->needs_render = 1;
}

static inline void _sfte_wayland_keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                                                    uint32_t serial, uint32_t mods_depressed,
                                                    uint32_t mods_latched, uint32_t mods_locked,
                                                    uint32_t group) {
    (void)keyboard, (void)serial;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (!app->xkb_state) return;

    xkb_state_update_mask(app->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static inline void _sfte_wayland_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
                                                      int32_t rate, int32_t delay) {
    (void)keyboard;
    sfte_wayland_app *app = (sfte_wayland_app *)data;

    app->repeat_rate = rate;
    app->repeat_delay = delay;
}

static const struct wl_keyboard_listener _sfte_wayland_keyboard_listener = {
    .keymap = _sfte_wayland_keyboard_keymap,
    .enter = _sfte_wayland_keyboard_enter,
    .leave = _sfte_wayland_keyboard_leave,
    .key = _sfte_wayland_keyboard_key,
    .modifiers = _sfte_wayland_keyboard_modifiers,
    .repeat_info = _sfte_wayland_keyboard_repeat_info,
};

static inline void _sfte_wayland_seat_capabilities(void *data, struct wl_seat *seat,
                                                   uint32_t capabilities) {
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    // if seat has a keyboard and we haven't grabbed it yet
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !app->keyboard) {
        app->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(app->keyboard, &_sfte_wayland_keyboard_listener, app);
    }
    // if seat lost keyboard and we still hold the ptr
    else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && app->keyboard) {
        wl_keyboard_release(app->keyboard);
        app->keyboard = NULL;
    }

#if SFTE_INPUT_SELECTION
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) {
        app->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(app->pointer, &_sfte_wayland_pointer_listener, app);

    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && app->pointer) {
        wl_pointer_release(app->pointer);
        app->pointer = NULL;
    }
#endif  // SFTE_INPUT_SELECTION

#if SFTE_CLIPBOARD
    if (app->data_device_manager && !app->data_device) {
        app->data_device = (struct wl_data_device *)wl_data_device_manager_get_data_device(
            app->data_device_manager, seat);
        wl_data_device_add_listener(app->data_device, &_sfte_wayland_data_device_listener, app);
    }
#endif  // SFTE_CLIPBOARD
}

static inline void _sfte_wayland_seat_name(void *data, struct wl_seat *seat, const char *name) {
    (void)data, (void)seat, (void)name;
}

static const struct wl_seat_listener _sfte_wayland_seat_listener = {
    .capabilities = _sfte_wayland_seat_capabilities,
    .name = _sfte_wayland_seat_name,
};

static inline void _sfte_wayland_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                                                  uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(xdg_wm_base, serial);  // compositor pinged, pong back with same serial
}

static const struct xdg_wm_base_listener _sfte_wayland_xdg_wm_base_listener = {
    .ping = _sfte_wayland_xdg_wm_base_ping,
};

static inline void _sfte_wayland_registry_global(void *data, struct wl_registry *registry,
                                                 uint32_t name, const char *interface,
                                                 uint32_t version) {
    (void)version;
    sfte_wayland_app *app = (sfte_wayland_app *)data;

    if (strcmp(interface, wl_compositor_interface.name) == 0)
        app->compositor = (struct wl_compositor *)wl_registry_bind(
            registry, name, &wl_compositor_interface, 4 /* wl compositor version */);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        app->shm = (struct wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface,
                                                     1 /* wl shm version */);
    else if (strcmp(interface, wl_seat_interface.name) == 0) {
        app->seat = (struct wl_seat *)wl_registry_bind(registry, name, &wl_seat_interface,
                                                       7 /* wl seat version */);
        wl_seat_add_listener(app->seat, &_sfte_wayland_seat_listener, app);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        app->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(
            registry, name, &xdg_wm_base_interface, 1 /* xdg wm base version */);
        xdg_wm_base_add_listener(app->xdg_wm_base, &_sfte_wayland_xdg_wm_base_listener, app);
    }
#if SFTE_CLIPBOARD
    else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
        app->data_device_manager = (struct wl_data_device_manager *)wl_registry_bind(
            registry, name, &wl_data_device_manager_interface, 3 /* data device manager version */);
    }
#endif  // SFTE_CLIPBOARD
}

static inline void _sfte_wayland_registry_global_remove(void *data, struct wl_registry *registry,
                                                        uint32_t name) {
    (void)data, (void)registry, (void)name;
}

static const struct wl_registry_listener _sfte_wayland_registry_listener = {
    .global = _sfte_wayland_registry_global,
    .global_remove = _sfte_wayland_registry_global_remove,
};

static inline void _sfte_wayland_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                                       uint32_t serial) {
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    xdg_surface_ack_configure(xdg_surface, serial);

    if (app->pending_width > 0 && app->pending_height > 0) {
        app->width = app->pending_width;
        app->height = app->pending_height;
        app->pending_width = 0;
        app->pending_height = 0;

        sfte_resize(app->ctx, app->width, app->height);
        _sfte_wayland_pty_update(app);
    }

    // resize recalc
    size_t needed_size = app->width * app->height * 4;
    if (app->shm_size != needed_size) {
        if (app->buffer) wl_buffer_destroy(app->buffer);
        if (app->shm_data) munmap(app->shm_data, app->shm_size);

        _sfte_wayland_create_buffer(app);
    }

    app->needs_render = 1;
}

static const struct xdg_surface_listener _sfte_wayland_xdg_surface_listener = {
    .configure = _sfte_wayland_xdg_surface_configure,
};

static inline void _sfte_wayland_xdg_toplevel_configure(void *data,
                                                        struct xdg_toplevel *xdg_toplevel,
                                                        int32_t width, int32_t height,
                                                        struct wl_array *states) {
    (void)xdg_toplevel, (void)states;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
#if SFTE_TERM_FOCUS
    uint32_t *state;
    uint8_t focused = 0;
    wl_array_for_each(state, states) {
        if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) {
            focused = 1;
            break;
        }
    }

    sfte_set_focus(app->ctx, focused);
    app->needs_render = 1;
#endif  // SFTE_TERM_FOCUS

    if (width <= 0 || height <= 0) return;

    app->pending_width = width;
    app->pending_height = height;
}

static inline void _sfte_wayland_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    (void)xdg_toplevel;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    app->running = 0;
}

static const struct xdg_toplevel_listener _sfte_wayland_xdg_toplevel_listener = {
    .configure = _sfte_wayland_xdg_toplevel_configure,
    .close = _sfte_wayland_xdg_toplevel_close,
};

/*
    Initializes the Wayland connection and binds global registry interfaces.
*/
static inline void _sfte_wayland_load(sfte_wayland_app *app) {
    app->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    SFTE_ASSERT(app->xkb_context, "failed to create xkb context");

    app->display = wl_display_connect(NULL);
    SFTE_ASSERT(app->display, "failed to connect to Wayland display\n");

    app->registry = wl_display_get_registry(app->display);
    wl_registry_add_listener(app->registry, &_sfte_wayland_registry_listener, app);

    // Initial roundtrip to let the registry listener discover compositor/shm/seat
    wl_display_roundtrip(app->display);
    SFTE_ASSERT(app->compositor, "failed to initialize compositor\n");
    SFTE_ASSERT(app->shm, "compositor missing required interfaces\n");
    SFTE_ASSERT(app->xdg_wm_base, "failed to bind xdg_wm_base\n");

    app->surface = wl_compositor_create_surface(app->compositor);
    app->xdg_surface = xdg_wm_base_get_xdg_surface(app->xdg_wm_base, app->surface);
    xdg_surface_add_listener(app->xdg_surface, &_sfte_wayland_xdg_surface_listener, app);

    app->xdg_toplevel = xdg_surface_get_toplevel(app->xdg_surface);
    xdg_toplevel_add_listener(app->xdg_toplevel, &_sfte_wayland_xdg_toplevel_listener, app);
    xdg_toplevel_set_title(app->xdg_toplevel, "sfte");
    xdg_toplevel_set_app_id(app->xdg_toplevel, "sfte");

    wl_surface_commit(app->surface);
    wl_display_roundtrip(app->display);
    _SFTE_INFO(app->ctx, WAYLAND_REGISTRY_BOUND);
}

/*
    Cleans up all Wayland objects and memory mappings.
*/
static inline void _sfte_wayland_unload(sfte_wayland_app *app) {
#if SFTE_TERM_DOUBLE_BUFFER
    SFTE_FREE(app->back_buffer);
#endif  // SFTE_TERM_DOUBLE_BUFFER

    if (app->buffer) wl_buffer_destroy(app->buffer);
    if (app->shm_data) munmap(app->shm_data, app->shm_size);
    if (app->xdg_toplevel) xdg_toplevel_destroy(app->xdg_toplevel);
    if (app->xdg_surface) xdg_surface_destroy(app->xdg_surface);
    if (app->surface) wl_surface_destroy(app->surface);
    if (app->xdg_wm_base) xdg_wm_base_destroy(app->xdg_wm_base);

    if (app->keyboard) wl_keyboard_release(app->keyboard);
#if SFTE_INPUT_SELECTION
    if (app->pointer) wl_pointer_release(app->pointer);
#endif  // SFTE_INPUT_SELECTION
#if SFTE_CLIPBOARD
    if (app->data_offer) wl_data_offer_destroy(app->data_offer);
    if (app->data_source) wl_data_source_destroy(app->data_source);
    if (app->data_device) wl_data_device_release(app->data_device);
    if (app->data_device_manager) wl_data_device_manager_destroy(app->data_device_manager);
    if (app->selection_text) SFTE_FREE(app->selection_text);
#endif  // SFTE_CLIPBOARD

    if (app->seat) wl_seat_release(app->seat);

    if (app->shm) wl_shm_destroy(app->shm);
    if (app->compositor) wl_compositor_destroy(app->compositor);

    wl_registry_destroy(app->registry);
    wl_display_disconnect(app->display);

    xkb_state_unref(app->xkb_state);
    xkb_keymap_unref(app->xkb_keymap);
    xkb_context_unref(app->xkb_context);
}

#if SFTE_CLIPBOARD
#if SFTE_INPUT_SELECTION
#if SFTE_CLIPBOARD_OSC52
static inline void _sfte_wayland_osc52_clipboard_cb(void *user_data, char target,
                                                    const char *data) {
    (void)target;  // TODO: wl primary selection protocol
    if (target != 'c') return;
    sfte_wayland_app *app = (sfte_wayland_app *)user_data;

    if (app->data_source) {
        wl_data_source_destroy(app->data_source);
        app->data_source = NULL;
    }
    if (app->selection_text) {
        SFTE_FREE(app->selection_text);
        app->selection_text = NULL;
    }

    if (!data || !app->data_device_manager || !app->data_device) return;

    size_t len = strlen(data);
    app->selection_text = (char *)SFTE_MALLOC(len + 1);
    memcpy(app->selection_text, data, len + 1);

    app->data_source = wl_data_device_manager_create_data_source(app->data_device_manager);
    wl_data_source_add_listener(app->data_source, &_sfte_wayland_data_source_listener, app);
    wl_data_source_offer(app->data_source, "text/plain;charset=utf-8");
    wl_data_source_offer(app->data_source, "text/plain");

    wl_data_device_set_selection(app->data_device, app->data_source, app->serial);
}
#endif  // SFTE_CLIPBOARD_OSC52

static inline uint8_t _sfte_wayland_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->cb.user_data;
    const sfte_term *term = sfte_term_get_active(ctx);

    if (!term->mouse.sel_active || !app->data_device_manager || !app->data_device) return 0;

    size_t needed_bytes = sfte_get_selection(term, NULL, 0);
    if (needed_bytes <= 1) {
        _SFTE_INFO(ctx, CLIPBOARD_EMPTY);
        return 0;
    }

    if (app->data_source) {
        wl_data_source_destroy(app->data_source);
        app->data_source = NULL;
    }
    if (app->selection_text) {
        SFTE_FREE(app->selection_text);
        app->selection_text = NULL;
    }

    app->selection_text = (char *)SFTE_MALLOC(needed_bytes);
    sfte_get_selection(term, app->selection_text, needed_bytes);
    if (!app->selection_text) return 0;

    app->data_source = wl_data_device_manager_create_data_source(app->data_device_manager);
    wl_data_source_add_listener(app->data_source, &_sfte_wayland_data_source_listener, app);
    wl_data_source_offer(app->data_source, "text/plain;charset=utf-8");
    wl_data_source_offer(app->data_source, "UTF8_STRING");
    wl_data_source_offer(app->data_source, "text/plain");
    wl_data_source_offer(app->data_source, "STRING");
    wl_data_source_offer(app->data_source, "TEXT");
    wl_data_device_set_selection(app->data_device, app->data_source, app->serial);

    return 1;
}
#endif  // SFTE_INPUT_SELECTION

static inline uint8_t _sfte_wayland_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->cb.user_data;
    sfte_term *term = sfte_term_get_active(ctx);

    if (app->selection_text) {
        size_t len = strlen(app->selection_text);
        if (len > 0) {
            sfte_input_paste_begin(term);
            sfte_input_text(ctx, app->selection_text, len);
            sfte_input_paste_end(term);
        }
        return 1;
    }

    if (!app->data_offer) return 0;

    const char *mime = app->data_offer_mime[0] ? app->data_offer_mime : "text/plain;charset=utf-8";

    int fds[2];
    if (pipe(fds) == 0) {
        wl_data_offer_receive(app->data_offer, mime, fds[1]);
        close(fds[1]);

        wl_display_flush(app->display);

        sfte_input_paste_begin(term);

        char buf[SFTE_CLIPBOARD_BUF_SIZE];
        ssize_t n;

        while ((n = read(fds[0], buf, sizeof(buf))) > 0) sfte_input_text(ctx, buf, n);

        sfte_input_paste_end(term);

        close(fds[0]);
    }

    return 1;
}
#endif  // SFTE_CLIPBOARD

/*
    The primary event loop for the terminal emulator using Wayland.
    Uses `poll` to simultaneously wait for Wayland compositor events,
    shell output events (PTY data), and timer expirations (on cursor blink, key repeat, trail).
*/
static inline void _sfte_wayland_loop(sfte_wayland_app *app) {
    sfte_ctx *ctx = app->ctx;
    sfte_mux *mux = &ctx->mux;

    signal(SIGPIPE, SIG_IGN);
    setlocale(LC_ALL, "");

    app->repeat_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    int wl_fd = wl_display_get_fd(app->display);

    while (app->running) {
        // Dispatch pending Wayland events before polling to prevent deadlock
        wl_display_dispatch_pending(app->display);
        wl_display_flush(app->display);

        struct pollfd fds[2 + SFTE_MUX_MAX_WINDOWS];
        sfte_term *term_map[SFTE_MUX_MAX_WINDOWS];

        fds[0] = (struct pollfd){.fd = wl_fd, .events = POLLIN};
        fds[1] = (struct pollfd){.fd = app->repeat_timer_fd, .events = POLLIN};
        uint8_t nfds = 2;

        for (uint8_t i = 0; i < SFTE_MUX_MAX_WINDOWS; ++i) {
            sfte_wayland_window_backend backend = app->win_backends[i];

            if (backend.pty_fd >= 0 && mux->terms[i].is_allocated) {
                fds[nfds] = (struct pollfd){.fd = backend.pty_fd, .events = POLLIN};
                term_map[nfds - 2] = &mux->terms[i];
                nfds++;
            }
        }

        int32_t timeout = sfte_get_timeout_ms(ctx);
        if (app->needs_render) timeout = 0;  // Don't sleep if we already know we need to draw
            timeout = 0;  // Don't sleep if we already know we need to draw

        if (poll(fds, nfds, timeout) == -1) break;

        if (sfte_tick(ctx)) app->needs_render = 1;

        // Handle incoming Wayland events (keys, resizes)
        if (fds[0].revents & (POLLIN | POLLERR | POLLHUP))
            if (wl_display_dispatch(app->display) == -1) app->running = 0;

// Handle core stage changes triggered by Wayland events
#if SFTE_MUX
        if (mux->pending_spawn) {
            mux->pending_spawn = 0;
            _sfte_wayland_pty_spawn(app);
            app->needs_render = 1;
        }

        if (mux->pending_close) {
            mux->pending_close = 0;

            sfte_wayland_window_backend *backend = &app->win_backends[mux->active_idx];

            if (backend->pty_pid > 0) {
                kill(backend->pty_pid, SIGHUP);
                backend->pty_pid = -1;
            }

            if (backend->pty_fd >= 0) {
                close(backend->pty_fd);
                backend->pty_fd = -1;
            }

            if (!sfte_term_close(ctx, &mux->terms[mux->active_idx])) app->running = 0;
            app->needs_render = 1;
        }

        if (mux->pending_reorder != 0) {
            int8_t step = mux->pending_reorder;
            mux->pending_reorder = 0;

            // Find cur active slots vis pos
            int8_t cur_pos = _sfte_mux_slot_to_pos(mux, mux->active_idx);

            int8_t target_pos = cur_pos + step;
            if (cur_pos != -1 && target_pos >= 0 && target_pos < mux->tab_cnt) {
                // Swap vis pos
                uint8_t tmp = mux->tab_order[cur_pos];
                mux->tab_order[cur_pos] = mux->tab_order[target_pos];
                mux->tab_order[target_pos] = tmp;

                app->needs_render = 1;
            }
        }
#endif  // SFTE_MUX

        // Handle key repeat timer
        if (fds[1].revents & POLLIN) {
            uint64_t expirations;

            // Simulate a key press to autorepeat
            if (read(app->repeat_timer_fd, &expirations, sizeof(expirations)) > 0 &&
                app->repeating_key != 0)
                _sfte_wayland_keyboard_key(app, app->keyboard, 0, 0, app->repeating_key,
                                           WL_KEYBOARD_KEY_STATE_PRESSED);
        }

        for (uint8_t p = 2; p < nfds; ++p) {
            if (!(fds[p].revents & (POLLIN | POLLERR | POLLHUP))) continue;

            int32_t cur_fd = fds[p].fd;
            sfte_term *target_term = term_map[p - 2];
            uint8_t buf[SFTE_TERM_PTY_BUF_SIZE];
            uint8_t did_read = 0;

            while (1) {
                ssize_t n = read(cur_fd, buf, SFTE_TERM_PTY_BUF_SIZE);

                if (n > 0) {
                    sfte_parse(target_term, buf, n);
                    did_read = 1;
                } else if (n == 0) {
                    close(cur_fd);

                    int8_t idx = sfte_term_get_idx(ctx, target_term);
                    sfte_wayland_window_backend *backend = &app->win_backends[idx];
                    backend->pty_fd = -1;
                    backend->pty_pid = -1;

                    if (!sfte_term_close(ctx, target_term)) app->running = 0;
                    break;
                } else if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                else if (errno == EINTR)
                    continue;
                else {
                    close(cur_fd);

                    int8_t idx = sfte_term_get_idx(ctx, target_term);
                    sfte_wayland_window_backend *backend = &app->win_backends[idx];
                    backend->pty_fd = -1;
                    backend->pty_pid = -1;

                    if (!sfte_term_close(ctx, target_term)) app->running = 0;
                    break;
                }
            }

            if (did_read && sfte_term_get_active(ctx) == target_term) app->needs_render = 1;
        }

        // Dispatch render pass
        if (app->needs_render && !app->frame_pending) {
            sfte_damage_rect dmg = {0};

            uint32_t *target_pxs = app->shm_data;
#if SFTE_TERM_DOUBLE_BUFFER
            target_pxs = app->back_buffer;
#endif  // SFTE_TERM_DOUBLE_BUFFER

            sfte_render(ctx, target_pxs, app->width, app->height, &dmg);

            if (dmg.w > 0 && dmg.h > 0) {
#if SFTE_TERM_DOUBLE_BUFFER
                if (dmg.w == app->width)
                    memcpy(&app->shm_data[dmg.y * app->width],
                           &app->back_buffer[dmg.y * app->width], dmg.w * dmg.h * sizeof(uint32_t));
                else
                    for (int32_t y = dmg.y; y < dmg.y + dmg.h; ++y)
                        memcpy(&app->shm_data[y * app->width + dmg.x],
                               &app->back_buffer[y * app->width + dmg.x], dmg.w * sizeof(uint32_t));
#endif  // SFTE_TERM_DOUBLE_BUFFER

                wl_surface_damage_buffer(app->surface, dmg.x, dmg.y, dmg.w, dmg.h);
                wl_surface_attach(app->surface, app->buffer, 0, 0);
                struct wl_callback *cb = wl_surface_frame(app->surface);
                wl_callback_add_listener(cb, &_sfte_wayland_callback_listener, app);
                app->frame_pending = 1;
                wl_surface_commit(app->surface);
            }

            app->needs_render = 0;
        }
    }
}
#endif  // SFTE_WAYLAND

// =================================================================================================
// >>public api
// =================================================================================================
#if SFTE_WAYLAND
sfte_wayland_app *sfte_wayland_init(void) {
    sfte_wayland_app *app = (sfte_wayland_app *)SFTE_CALLOC(1, sizeof(sfte_wayland_app));
    app->running = 1;
    app->ctx = sfte_init(_sfte_wayland_write_cb, app);

#if SFTE_CLIPBOARD && SFTE_INPUT_SELECTION && SFTE_CLIPBOARD_OSC52
    app->ctx->cb.osc52_clipboard = _sfte_wayland_osc52_clipboard_cb;
#endif  // SFTE_CLIPBOARD && SFTE_INPUT_SELECTION && SFTE_CLIPBOARD_OSC52
#if SFTE_INPUT_HYPERLINKS
    app->ctx->cb.open_link = _sfte_wayland_open_link_cb;
#endif  // SFTE_INPUT_HYPERLINKS

    for (uint8_t i = 0; i < SFTE_MUX_MAX_WINDOWS; ++i) {
        sfte_wayland_window_backend *backend = &app->win_backends[i];

        backend->pty_fd = -1;
        backend->pty_pid = -1;
    }

    _sfte_wayland_load(app);
    _sfte_wayland_pty_spawn(app);

    return app;
}

sfte_ctx *sfte_wayland_get_ctx(sfte_wayland_app *app) {
    return app->ctx;
}

int sfte_wayland_run(sfte_wayland_app *app) {
    sfte_ctx *ctx = app->ctx;
    sfte_term *term = sfte_term_get_active(ctx);

#ifdef SFTE_FONT_BOLD
    SFTE_ASSERT(app->ctx->font.bold.glyphs && app->ctx->font.bold.atlas_pxs,
                "if SFTE_FONT_BOLD is defined, a bold font must be provided using "
                "sfte_font_load_*");
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    SFTE_ASSERT(app->ctx->font.italic.glyphs && app->ctx->font.italic.atlas_pxs,
                "if SFTE_FONT_ITALIC is defined, an italic font must be provided using "
                "sfte_font_load_*");
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    SFTE_ASSERT(app->ctx->font.bold_italic.glyphs && app->ctx->font.bold_italic.atlas_pxs,
                "if SFTE_FONT_BOLD_ITALIC is defined, a bold italic font must be provided using "
                "sfte_font_load_*");
#endif  // SFTE_FONT_BOLD_ITALIC

    int32_t ideal_w, ideal_h;
    sfte_get_ideal_size(term, SFTE_TERM_INIT_COLS, SFTE_TERM_INIT_ROWS, &ideal_w, &ideal_h);
    app->width = ideal_w;
    app->height = ideal_h;
    sfte_resize(ctx, app->width, app->height);
    if (!app->buffer) _sfte_wayland_create_buffer(app);

    _sfte_wayland_pty_update(app);
    _sfte_wayland_loop(app);

    _sfte_wayland_unload(app);
    sfte_free(ctx);
    SFTE_FREE(app);
    return 0;
}
#endif  // SFTE_WAYLAND
