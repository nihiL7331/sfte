#pragma once

#include "src/config.h"
#include "src/types.h"
#include <stddef.h>
#include <stdint.h>
#ifndef SFTE_NO_LOGGING
#define _SFTE_LOG_MAX_MSG_LEN 512

#include <stdarg.h>
#include <stdio.h>
#endif  // !defined(SFTE_NO_LOGGING)

// =================================================================================================
// >>infrastructure macros
// =================================================================================================

// Retrieves the number of elements in a statically allocated array.
#define _SFTE_ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

// Constraints a value within a specified inclusive range [min, max].
#define _SFTE_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

// Reads a 16-bit big-endian value from `d` array starting from `off` byte.
#define _SFTE_R16BE(d, off) (uint16_t)(((d)[off] << 8) | (d)[(off) + 1])

// Reads a 32-bit big-endian value from `d` array starting from `off` byte.
#define _SFTE_R32BE(d, off)                                                                        \
    (uint32_t)(((d)[off] << 24) | ((d)[(off) + 1] << 16) | ((d)[(off) + 2] << 8) | (d)[(off) + 3])

#if SFTE_FONT_WIDE_CHARS
#include <wchar.h>
#define _SFTE_CHAR_WIDTH(rune) wcwidth(rune)
#else
#define _SFTE_CHAR_WIDTH(rune) 1
#endif

#if SFTE_CURSOR_BLINK || SFTE_CURSOR_TRAIL || SFTE_TERM_SCROLL_SMOOTH
#ifndef SFTE_TIME_MS
#include <time.h>
static inline uint64_t _sfte_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#define SFTE_TIME_MS() _sfte_time_ms()
#endif  // SFTE_TIME_MS
#endif  // SFTE_CURSOR_BLINK || SFTE_CURSOR_TRAIL || SFTE_TERM_SCROLL_SMOOTH

// =================================================================================================
// >>mem
// =================================================================================================

/*
    Dynamically resizes an 'arr' buffer of type 'type *',
    doubling its 'cap' until the required length 'len' + 'add'
    is met, up to a specified 'max_cap' limit.
    Sets 'oom_flag' to 1 if 'max_cap' is exceeded.
    If 'cap' is 0, it initializes capacity to 'init_cap'.
*/
#define _SFTE_MEM_ENSURE_CAP(type, arr, len, cap, add, init_cap, max_cap, oom_flag)                \
    do {                                                                                           \
        size_t _req = (len) + (add);                                                               \
        if (_req > (cap)) {                                                                        \
            size_t _new = (cap) == 0 ? (init_cap) : (cap) * 2;                                     \
            while (_new < _req) _new *= 2;                                                         \
            if (_new > (max_cap))                                                                  \
                (oom_flag) = 1;                                                                    \
            else {                                                                                 \
                (cap) = _new;                                                                      \
                (arr) = (type *)SFTE_REALLOC((arr), (cap) * sizeof(type));                         \
            }                                                                                      \
        }                                                                                          \
    } while (0)

/*
    Initializes the stack allocator with a pre-allocated `back_buf`.
*/
static inline void _sfte_mem_stack_init(sfte_stack *stack, void *back_buf, size_t cap) {
    stack->buf = (uint8_t *)back_buf;
    stack->cap = cap;
    stack->off = 0;
}

/*
    Allocates `size` bytes of memory in `stack`, aligned to `align` bytes.
    `align` must be a power of 2.
*/
static inline void *_sfte_mem_stack_alloc(sfte_stack *stack, size_t size, size_t align) {
    uintptr_t cur_ptr = (uintptr_t)stack->buf + stack->off;  // Current (unaligned) memory address
    // `align` is a power of 2, in binary represented as 10...0.
    // `align - 1` hence is represented in binary as     01...1.
    // `(ptr + align - 1) & ~(align - 1)` ceils `ptr` to the closest multiple of `align`.
    // ` ... + align - 1) ...` ensures that it will ceil instead of flooring
    // `              ... & ~(align - 1)` floors `ptr` to the closest multiple of `align`.
    uintptr_t off_pad = (align - 1);
    uintptr_t align_ptr = (cur_ptr + off_pad) & ~off_pad;
    size_t new_off = (align_ptr - (uintptr_t)stack->buf) + size;
    if (new_off > stack->cap) return NULL;  // OOM
    stack->off = new_off;
    return (void *)align_ptr;
}

/*
    Rewinds the stack offset to `off`.
    If called with `off` = 0, resets the entire arena.
*/
static inline void _sfte_mem_stack_rewind(sfte_stack *stack, size_t off) {
    stack->off = off;
}

/*
    Returns the current offset of `stack`.
*/
static inline size_t _sfte_mem_stack_save(sfte_stack *stack) {
    return stack->off;
}

// =================================================================================================
// >>log
// =================================================================================================
#ifndef SFTE_NO_LOGGING

#define _SFTE_LOG_ITEMS                                                                            \
    _SFTE_LOGITEM_XMACRO(OK, "ok")                                                                 \
    _SFTE_LOGITEM_XMACRO(FONT_LOADED, "font loaded and baked to atlas")                            \
    _SFTE_LOGITEM_XMACRO(                                                                          \
        FONT_LOAD_FAIL,                                                                            \
        "Failed to open font file: '%s'\nIf this is your first time running sfte, you need to "    \
        "edit 'config.c' and set the correct font paths, then recompile.\n")                       \
    _SFTE_LOGITEM_XMACRO(PTY_SPAWN, "master/slave pair successfully spawned")                      \
    _SFTE_LOGITEM_XMACRO(PTY_FORK_FAIL, "forkpty failed with errno: '%d'")                         \
    _SFTE_LOGITEM_XMACRO(UNHANDLED_CSI, "unhandled CSI command: '%c' with '%d' parms")             \
    _SFTE_LOGITEM_XMACRO(UNHANDLED_OSC, "unhandled OSC payload: '%s'")                             \
    _SFTE_LOGITEM_XMACRO(TERM_RESIZE, "resized grid to '%dx%d'")                                   \
    _SFTE_LOGITEM_XMACRO(WAYLAND_REGISTRY_BOUND, "wayland globals bound")                          \
    _SFTE_LOGITEM_XMACRO(KEYMAP_LOADED, "xkb keymap loaded from compositor")                       \
    _SFTE_LOGITEM_XMACRO(SHELL_FALLBACK, "SHELL env var unset, falling back to /bin/sh")           \
    _SFTE_LOGITEM_XMACRO(CLIPBOARD_EMPTY, "clipboard call requested but buffer is empty")

#define _SFTE_LOGITEM_XMACRO(item, msg) item,
typedef enum { _SFTE_LOG_ITEMS } _sfte_log_item;
#undef _SFTE_LOGITEM_XMACRO

#define _SFTE_LOGITEM_XMACRO(item, msg) #item ": " msg,
static const char *_sfte_log_messages[] = {_SFTE_LOG_ITEMS};
#undef _SFTE_LOGITEM_XMACRO

#define _SFTE_PANIC(ctx, code, ...)                                                                \
    _sfte_log(ctx->logger.func, code, SFTE_LOG_LVL_PANIC, __LINE__, ##__VA_ARGS__)
#define _SFTE_ERROR(ctx, code, ...)                                                                \
    _sfte_log(ctx->logger.func, code, SFTE_LOG_LVL_ERROR, __LINE__, ##__VA_ARGS__)
#define _SFTE_WARN(ctx, code, ...)                                                                 \
    _sfte_log(ctx->logger.func, code, SFTE_LOG_LVL_WARN, __LINE__, ##__VA_ARGS__)
#define _SFTE_INFO(ctx, code, ...)                                                                 \
    _sfte_log(ctx->logger.func, code, SFTE_LOG_LVL_INFO, __LINE__, ##__VA_ARGS__)

/*
    Default standard error logging sink.
    Outputs logs in format: ['tag':'line_nr']('log_level') 'msg'
*/
static inline void _sfte_log_default_func(const char *tag, sfte_log_level log_level,
                                          const char *msg, uint32_t line_nr) {
    const char *level_str = "???";
    switch (log_level) {
    case SFTE_LOG_LVL_PANIC: level_str = "PANIC"; break;
    case SFTE_LOG_LVL_ERROR: level_str = "ERROR"; break;
    case SFTE_LOG_LVL_WARN: level_str = "WARN"; break;
    case SFTE_LOG_LVL_INFO: level_str = "INFO"; break;
    }
    fprintf(stderr, "[%s:%d](%s) %s\n", tag, line_nr, level_str, msg);
}

/*
    Formats a log message from the X-Macro catalog passed by 'log_item' and dispatches it to the
    active sink.

    Routes to a user-provided logger if one is configured in 'ctx', otherwise falls
    back to stderr. Aborts the process if 'log_level' is PANIC.

    This function is not called directly, instead its used by macros
   (`_SFTE_PANIC/ERROR/WARN/INFO`)
*/
static inline void _sfte_log(sfte_log_func log_func, _sfte_log_item log_item,
                             sfte_log_level log_level, uint32_t line_nr, ...) {
    if (log_level > SFTE_LOG_LEVEL) return;

    char buf[_SFTE_LOG_MAX_MSG_LEN];
    va_list args;
    va_start(args, line_nr);
    vsnprintf(buf, sizeof(buf), _sfte_log_messages[log_item], args);
    va_end(args);

    if (!log_func) log_func = _sfte_log_default_func;

    log_func(SFTE_LOG_TAG, log_level, buf, line_nr);

    if (log_level == SFTE_LOG_LVL_ERROR) exit(1);
    // for log level PANIC it would be UB to continue
    if (log_level == SFTE_LOG_LVL_PANIC) abort();
}

#else  // defined(SFTE_NO_LOGGING)

#define _SFTE_PANIC(ctx, code, ...) abort()
#define _SFTE_ERROR(ctx, code, ...)                                                                \
    do {                                                                                           \
    } while (0)
#define _SFTE_WARN(ctx, code, ...)                                                                 \
    do {                                                                                           \
    } while (0)
#define _SFTE_INFO(ctx, code, ...)                                                                 \
    do {                                                                                           \
    } while (0)

#endif  // defined(SFTE_NO_LOGGING)

// =================================================================================================
// >>b64
// =================================================================================================
#if (SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52) || SFTE_IMG_KITTY
static const int8_t _sfte_b64_table[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

/*
    Decodes a Base64 payload into a stack-allocated binary buffer.
    Ignores invalid characters (spaces, newlines).
    The caller assumes ownership of the returned pointer and MUST free it.
    Returns NULL if input is empty or if memory allocation fails.
*/
static inline uint8_t *_sfte_b64_decode(sfte_stack *stack, uint8_t *src, size_t len,
                                        size_t *out_len) {
    SFTE_ASSERT(src && out_len, "b64_decode requires valid pointers");

    // Strip trailing padding
    while (len > 0 && src[len - 1] == '=') len--;

    *out_len = (len * 3) / 4;
    size_t pre_off = _sfte_mem_stack_save(stack);
    uint8_t *dst = (uint8_t *)_sfte_mem_stack_alloc(stack, *out_len, _Alignof(uint8_t));
    if (!dst) {
        _sfte_mem_stack_rewind(stack, pre_off);
        return NULL;
    }

    size_t i = 0, j = 0;
    uint32_t acc = 0;
    uint8_t bits = 0;

    while (i < len) {
        int8_t v = _sfte_b64_table[src[i++]];
        if (v == -1) continue;

        acc = (acc << 6) | (v & 0x3F);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (j < *out_len) dst[j++] = (acc >> bits) & 0xFF;
        }
    }

    // Update out_len since ignored characters might've made the overall size smaller.
    *out_len = j;
    return dst;
}
#endif  // (SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52) || SFTE_IMG_KITTY

// =================================================================================================
// >>color
// =================================================================================================

// clang-format off
static const uint32_t _sfte_palette_256[256] = {
    // ----------------------  0-15 standard ANSI palette  ------------------------
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    // ---------------------  16-231 6x6x6 RGB color cube  ------------------------
    0xFF000000, 0xFF00005F, 0xFF000087, 0xFF0000AF, 0xFF0000D7, 0xFF0000FF, 0xFF005F00, 0xFF005F5F,
    0xFF005F87, 0xFF005FAF, 0xFF005FD7, 0xFF005FFF, 0xFF008700, 0xFF00875F, 0xFF008787, 0xFF0087AF,
    0xFF0087D7, 0xFF0087FF, 0xFF00AF00, 0xFF00AF5F, 0xFF00AF87, 0xFF00AFAF, 0xFF00AFD7, 0xFF00AFFF,
    0xFF00D700, 0xFF00D75F, 0xFF00D787, 0xFF00D7AF, 0xFF00D7D7, 0xFF00D7FF, 0xFF00FF00, 0xFF00FF5F,
    0xFF00FF87, 0xFF00FFAF, 0xFF00FFD7, 0xFF00FFFF, 0xFF5F0000, 0xFF5F005F, 0xFF5F0087, 0xFF5F00AF,
    0xFF5F00D7, 0xFF5F00FF, 0xFF5F5F00, 0xFF5F5F5F, 0xFF5F5F87, 0xFF5F5FAF, 0xFF5F5FD7, 0xFF5F5FFF,
    0xFF5F8700, 0xFF5F875F, 0xFF5F8787, 0xFF5F87AF, 0xFF5F87D7, 0xFF5F87FF, 0xFF5FAF00, 0xFF5FAF5F,
    0xFF5FAF87, 0xFF5FAFAF, 0xFF5FAFD7, 0xFF5FAFFF, 0xFF5FD700, 0xFF5FD75F, 0xFF5FD787, 0xFF5FD7AF,
    0xFF5FD7D7, 0xFF5FD7FF, 0xFF5FFF00, 0xFF5FFF5F, 0xFF5FFF87, 0xFF5FFFAF, 0xFF5FFFD7, 0xFF5FFFFF,
    0xFF870000, 0xFF87005F, 0xFF870087, 0xFF8700AF, 0xFF8700D7, 0xFF8700FF, 0xFF875F00, 0xFF875F5F,
    0xFF875F87, 0xFF875FAF, 0xFF875FD7, 0xFF875FFF, 0xFF878700, 0xFF87875F, 0xFF878787, 0xFF8787AF,
    0xFF8787D7, 0xFF8787FF, 0xFF87AF00, 0xFF87AF5F, 0xFF87AF87, 0xFF87AFAF, 0xFF87AFD7, 0xFF87AFFF,
    0xFF87D700, 0xFF87D75F, 0xFF87D787, 0xFF87D7AF, 0xFF87D7D7, 0xFF87D7FF, 0xFF87FF00, 0xFF87FF5F,
    0xFF87FF87, 0xFF87FFAF, 0xFF87FFD7, 0xFF87FFFF, 0xFFAF0000, 0xFFAF005F, 0xFFAF0087, 0xFFAF00AF,
    0xFFAF00D7, 0xFFAF00FF, 0xFFAF5F00, 0xFFAF5F5F, 0xFFAF5F87, 0xFFAF5FAF, 0xFFAF5FD7, 0xFFAF5FFF,
    0xFFAF8700, 0xFFAF875F, 0xFFAF8787, 0xFFAF87AF, 0xFFAF87D7, 0xFFAF87FF, 0xFFAFAF00, 0xFFAFAF5F,
    0xFFAFAF87, 0xFFAFAFAF, 0xFFAFAFD7, 0xFFAFAFFF, 0xFFAFD700, 0xFFAFD75F, 0xFFAFD787, 0xFFAFD7AF,
    0xFFAFD7D7, 0xFFAFD7FF, 0xFFAFFF00, 0xFFAFFF5F, 0xFFAFFF87, 0xFFAFFFAF, 0xFFAFFFD7, 0xFFAFFFFF,
    0xFFD70000, 0xFFD7005F, 0xFFD70087, 0xFFD700AF, 0xFFD700D7, 0xFFD700FF, 0xFFD75F00, 0xFFD75F5F,
    0xFFD75F87, 0xFFD75FAF, 0xFFD75FD7, 0xFFD75FFF, 0xFFD78700, 0xFFD7875F, 0xFFD78787, 0xFFD787AF,
    0xFFD787D7, 0xFFD787FF, 0xFFD7AF00, 0xFFD7AF5F, 0xFFD7AF87, 0xFFD7AFAF, 0xFFD7AFD7, 0xFFD7AFFF,
    0xFFD7D700, 0xFFD7D75F, 0xFFD7D787, 0xFFD7D7AF, 0xFFD7D7D7, 0xFFD7D7FF, 0xFFD7FF00, 0xFFD7FF5F,
    0xFFD7FF87, 0xFFD7FFAF, 0xFFD7FFD7, 0xFFD7FFFF, 0xFFFF0000, 0xFFFF005F, 0xFFFF0087, 0xFFFF00AF,
    0xFFFF00D7, 0xFFFF00FF, 0xFFFF5F00, 0xFFFF5F5F, 0xFFFF5F87, 0xFFFF5FAF, 0xFFFF5FD7, 0xFFFF5FFF,
    0xFFFF8700, 0xFFFF875F, 0xFFFF8787, 0xFFFF87AF, 0xFFFF87D7, 0xFFFF87FF, 0xFFFFAF00, 0xFFFFAF5F,
    0xFFFFAF87, 0xFFFFAFAF, 0xFFFFAFD7, 0xFFFFAFFF, 0xFFFFD700, 0xFFFFD75F, 0xFFFFD787, 0xFFFFD7AF,
    0xFFFFD7D7, 0xFFFFD7FF, 0xFFFFFF00, 0xFFFFFF5F, 0xFFFFFF87, 0xFFFFFFAF, 0xFFFFFFD7, 0xFFFFFFFF,
    // -------------------------  232-255 grayscale ramp  -------------------------
    0xFF080808, 0xFF121212, 0xFF1C1C1C, 0xFF262626, 0xFF303030, 0xFF3A3A3A, 0xFF444444, 0xFF4E4E4E,
    0xFF585858, 0xFF626262, 0xFF6C6C6C, 0xFF767676, 0xFF808080, 0xFF8A8A8A, 0xFF949494, 0xFF9E9E9E,
    0xFFA8A8A8, 0xFFB2B2B2, 0xFFBCBCBC, 0xFFC6C6C6, 0xFFD0D0D0, 0xFFDADADA, 0xFFE4E4E4, 0xFFEEEEEE,
};
// clang-format on

// =================================================================================================
// >>img
// =================================================================================================

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
/*
    Locates an image in the global pool by its ID.
    Returns NULL if the image was deleted.

    WARN: The returned pointer points directly into a dynamic array.
    It WILL be invalidated the next time a placement is inserted. Do not store this pointer.
*/
static inline sfte_img *_sfte_img_find(const sfte_image_state *images, uint32_t id) {
    for (uint32_t i = 0; i < images->pool_len; ++i)
        if (images->pool[i].id == id) return &images->pool[i];
    return NULL;
}

/*
    Inserts a new image into the global image pool.

    Takes ownership of `img.pxs`.
    If the pool is at maximum capacity, it safely frees `img.pxs` and returns NULL.

    WARN: The returned pointer points directly into a dynamic array.
    It WILL be invalidated the next time an image is inserted. Do not store this pointer.
*/
static inline sfte_img *_sfte_img_pool_insert(sfte_image_state *images, sfte_img img) {
    uint8_t oom = 0;
    _SFTE_MEM_ENSURE_CAP(sfte_img, images->pool, images->pool_len, images->pool_cap, 1,
                         SFTE_IMG_POOL_INIT_CAP, SFTE_IMG_POOL_MAX_CAP, oom);
    if (oom) {
        SFTE_FREE(img.pixels);
        return NULL;
    }

    images->pool[images->pool_len] = img;
    return &images->pool[images->pool_len++];
}

/*
    Inserts a new image placement instruction into the global placements list.

    WARN: The returned pointer points directly into a dynamic array.
    It WILL be invalidated the next time a placement is inserted. Do not store this pointer.
*/
static inline sfte_img_placement *_sfte_img_placement_insert(sfte_image_state *images,
                                                             sfte_img_placement p) {
    uint8_t oom = 0;
    _SFTE_MEM_ENSURE_CAP(sfte_img_placement, images->placements, images->placements_len,
                         images->placements_cap, 1, SFTE_IMG_PLACEMENT_INIT_CAP,
                         SFTE_IMG_PLACEMENT_MAX_CAP, oom);
    if (oom) return NULL;

    images->placements[images->placements_len] = p;
    return &images->placements[images->placements_len++];
}

#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
