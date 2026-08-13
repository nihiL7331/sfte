/*
    sfte -- single-file terminal emulator

    Project URL: https://github.com/nihiL7331/sfte

    Optionally provide the following defines with your own implementations:

    WHAT
    ====

    HOW
    ===

    FUTURE PLANS
    ============

    LICENSE
    =======
    zlib/libpng license

    Copyright (c) 2026 Patryk Pujanek

    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.

        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.

        3. This notice may not be removed or altered from any source
        distribution.
*/
#include <stdint.h>
#include <stdio.h>

// >>config
#ifndef SFTE_LOG_LEVEL
#define SFTE_LOG_LEVEL 3
#endif  // SFTE_LOG_LEVEL

#ifndef SFTE_LOGGER_FUNC
#define SFTE_LOGGER_FUNC _sfte_logger_default
#endif  // SFTE_LOGGER_FUNC

// >>structs
typedef struct sfte_logger {
    void (*func)(const char *tag,              // always "sfte"
                 uint32_t log_level,           // 0=panic, 1=error, 2=warning, 3=info
                 const char *message_or_null,  // a message string, may be nullptr in release mode
                 uint32_t line_nr              // line number in sfte.h
    );
} sfte_logger;

// >>api
int sfte_run(void);

/*=== IMPLEMENTATION =========================================================*/
#ifdef SFTE_IMPL

#include "xdg-shell.c"
#include "xdg-shell.h"
#include <string.h>
#include <wayland-client.h>

// >>structs
typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    sfte_logger logger;
} _sfte_state;
static _sfte_state _sfte;

// >>memory

// >>logging
#ifndef SFTE_ASSERT
#include <assert.h>
#define SFTE_ASSERT(c, m) assert(c &&m)
#endif  // SFTE_ASSERT

static void _sfte_logger_default(const char *tag, uint32_t log_level, const char *msg,
                                 uint32_t line_nr) {
    const char *level_str = "???";
    switch (log_level) {
    case 0: level_str = "PANIC"; break;
    case 1: level_str = "ERROR"; break;
    case 2: level_str = "WARN"; break;
    case 3: level_str = "INFO"; break;
    }
    fprintf(stderr, "[%s:%d](%s) %s\n", tag, line_nr, level_str, msg);
}

#define _SFTE_LOG_ITEMS _SFTE_LOGITEM_XMACRO(OK, "Ok")
#define _SFTE_LOGITEM_XMACRO(item, msg) item,
typedef enum { _SFTE_LOG_ITEMS } _sfte_log_item_t;
#undef _SFTE_LOGITEM_XMACRO
#define _SFTE_LOGITEM_XMACRO(item, msg) #item ": " msg,
static const char *_sfte_log_messages[] = {_SFTE_LOG_ITEMS};
#undef _SFTE_LOGITEM_XMACRO

static void _sfte_log(_sfte_log_item_t log_item, uint32_t log_level, uint32_t line_nr) {
    if (log_level > SFTE_LOG_LEVEL) return;
    void (*log_func)(const char *, uint32_t, const char *,
                     uint32_t) = _sfte.logger.func ? _sfte.logger.func : _sfte_logger_default;
    log_func("sfte", log_level, _sfte_log_messages[log_item], line_nr);

    // for log level PANIC it would be 'undefined behaviour' to continue
    if (log_level == 0) abort();
}

#define _SFTE_PANIC(code) _sfte_log(code, 0, __LINE__)
#define _SFTE_ERROR(code) _sfte_log(code, 1, __LINE__)
#define _SFTE_WARN(code) _sfte_log(code, 2, __LINE__)
#define _SFTE_INFO(code) _sfte_log(code, 3, __LINE__)

// >>wayland
static void _sfte_wayland_reg_global(void *data, struct wl_registry *registry, uint32_t name,
                                     const char *interface, uint32_t version) {
    (void)data, (void)version;
    if (strcmp(interface, wl_compositor_interface.name) == 0)
        _sfte.compositor = (struct wl_compositor *)wl_registry_bind(registry, name,
                                                                    &wl_compositor_interface, 4);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        _sfte.shm = (struct wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else if (strcmp(interface, wl_seat_interface.name) == 0)
        _sfte.seat = (struct wl_seat *)wl_registry_bind(registry, name, &wl_seat_interface, 7);
}

static void _sfte_wayland_reg_global_remove(void *data, struct wl_registry *registry,
                                            uint32_t name) {
    (void)data, (void)registry, (void)name;
}

static const struct wl_registry_listener _sfte_wayland_reg_listener = {
    .global = _sfte_wayland_reg_global,
    .global_remove = _sfte_wayland_reg_global_remove,
};

static void _sfte_wayland_load(void) {
    _sfte.display = wl_display_connect(NULL);
    SFTE_ASSERT(_sfte.display, "failed to connect to Wayland display\n");
    _sfte.registry = wl_display_get_registry(_sfte.display);
    wl_registry_add_listener(_sfte.registry, &_sfte_wayland_reg_listener, &_sfte);
    wl_display_roundtrip(_sfte.display);
    SFTE_ASSERT(_sfte.compositor, "failed to initialize compositor\n");
    SFTE_ASSERT(_sfte.shm, "compositor missing required interfaces\n");
}

static void _sfte_wayland_unload(void) {
    wl_registry_destroy(_sfte.registry);
    wl_display_disconnect(_sfte.display);
}

// >>state
static void _sfte_state_load(void) {
    memset(&_sfte, 0, sizeof(_sfte));
    _sfte.logger.func = SFTE_LOGGER_FUNC;
}

// >>api
int sfte_run(void) {
    _sfte_state_load();
    _sfte_wayland_load();
    _sfte_wayland_unload();
    _SFTE_INFO(OK);
    return 0;
}

#endif  // SFTE_IMPL
