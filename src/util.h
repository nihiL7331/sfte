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
    SFTE_COLOR_ANSI_PALETTE,
    // ---------------------  16-231 6x6x6 RGB color cube  ------------------------
    0x000000, 0x00005F, 0x000087, 0x0000AF, 0x0000D7, 0x0000FF, 0x005F00, 0x005F5F,
    0x005F87, 0x005FAF, 0x005FD7, 0x005FFF, 0x008700, 0x00875F, 0x008787, 0x0087AF,
    0x0087D7, 0x0087FF, 0x00AF00, 0x00AF5F, 0x00AF87, 0x00AFAF, 0x00AFD7, 0x00AFFF,
    0x00D700, 0x00D75F, 0x00D787, 0x00D7AF, 0x00D7D7, 0x00D7FF, 0x00FF00, 0x00FF5F,
    0x00FF87, 0x00FFAF, 0x00FFD7, 0x00FFFF, 0x5F0000, 0x5F005F, 0x5F0087, 0x5F00AF,
    0x5F00D7, 0x5F00FF, 0x5F5F00, 0x5F5F5F, 0x5F5F87, 0x5F5FAF, 0x5F5FD7, 0x5F5FFF,
    0x5F8700, 0x5F875F, 0x5F8787, 0x5F87AF, 0x5F87D7, 0x5F87FF, 0x5FAF00, 0x5FAF5F,
    0x5FAF87, 0x5FAFAF, 0x5FAFD7, 0x5FAFFF, 0x5FD700, 0x5FD75F, 0x5FD787, 0x5FD7AF,
    0x5FD7D7, 0x5FD7FF, 0x5FFF00, 0x5FFF5F, 0x5FFF87, 0x5FFFAF, 0x5FFFD7, 0x5FFFFF,
    0x870000, 0x87005F, 0x870087, 0x8700AF, 0x8700D7, 0x8700FF, 0x875F00, 0x875F5F,
    0x875F87, 0x875FAF, 0x875FD7, 0x875FFF, 0x878700, 0x87875F, 0x878787, 0x8787AF,
    0x8787D7, 0x8787FF, 0x87AF00, 0x87AF5F, 0x87AF87, 0x87AFAF, 0x87AFD7, 0x87AFFF,
    0x87D700, 0x87D75F, 0x87D787, 0x87D7AF, 0x87D7D7, 0x87D7FF, 0x87FF00, 0x87FF5F,
    0x87FF87, 0x87FFAF, 0x87FFD7, 0x87FFFF, 0xAF0000, 0xAF005F, 0xAF0087, 0xAF00AF,
    0xAF00D7, 0xAF00FF, 0xAF5F00, 0xAF5F5F, 0xAF5F87, 0xAF5FAF, 0xAF5FD7, 0xAF5FFF,
    0xAF8700, 0xAF875F, 0xAF8787, 0xAF87AF, 0xAF87D7, 0xAF87FF, 0xAFAF00, 0xAFAF5F,
    0xAFAF87, 0xAFAFAF, 0xAFAFD7, 0xAFAFFF, 0xAFD700, 0xAFD75F, 0xAFD787, 0xAFD7AF,
    0xAFD7D7, 0xAFD7FF, 0xAFFF00, 0xAFFF5F, 0xAFFF87, 0xAFFFAF, 0xAFFFD7, 0xAFFFFF,
    0xD70000, 0xD7005F, 0xD70087, 0xD700AF, 0xD700D7, 0xD700FF, 0xD75F00, 0xD75F5F,
    0xD75F87, 0xD75FAF, 0xD75FD7, 0xD75FFF, 0xD78700, 0xD7875F, 0xD78787, 0xD787AF,
    0xD787D7, 0xD787FF, 0xD7AF00, 0xD7AF5F, 0xD7AF87, 0xD7AFAF, 0xD7AFD7, 0xD7AFFF,
    0xD7D700, 0xD7D75F, 0xD7D787, 0xD7D7AF, 0xD7D7D7, 0xD7D7FF, 0xD7FF00, 0xD7FF5F,
    0xD7FF87, 0xD7FFAF, 0xD7FFD7, 0xD7FFFF, 0xFF0000, 0xFF005F, 0xFF0087, 0xFF00AF,
    0xFF00D7, 0xFF00FF, 0xFF5F00, 0xFF5F5F, 0xFF5F87, 0xFF5FAF, 0xFF5FD7, 0xFF5FFF,
    0xFF8700, 0xFF875F, 0xFF8787, 0xFF87AF, 0xFF87D7, 0xFF87FF, 0xFFAF00, 0xFFAF5F,
    0xFFAF87, 0xFFAFAF, 0xFFAFD7, 0xFFAFFF, 0xFFD700, 0xFFD75F, 0xFFD787, 0xFFD7AF,
    0xFFD7D7, 0xFFD7FF, 0xFFFF00, 0xFFFF5F, 0xFFFF87, 0xFFFFAF, 0xFFFFD7, 0xFFFFFF,
    // -------------------------  232-255 grayscale ramp  -------------------------
    0x080808, 0x121212, 0x1C1C1C, 0x262626, 0x303030, 0x3A3A3A, 0x444444, 0x4E4E4E,
    0x585858, 0x626262, 0x6C6C6C, 0x767676, 0x808080, 0x8A8A8A, 0x949494, 0x9E9E9E,
    0xA8A8A8, 0xB2B2B2, 0xBCBCBC, 0xC6C6C6, 0xD0D0D0, 0xDADADA, 0xE4E4E4, 0xEEEEEE,
};
// clang-format on

#if !SFTE_COLOR_TRUECOLOR
/*
    Given a RGB888 color, returns an index to the closest color in the 256-color palette.
    The closest color is chosen picking a color with the shortest Euclidean distance.
*/
static inline uint16_t _sfte_color_rgb_to_256(uint8_t r, uint8_t g, uint8_t b) {
    // If it's grayscale, map to the 24-step grayscale ramp (232-255)
    if (r == g && g == b) {
        if (r < 8) return 16;     // Black color index
        if (r > 248) return 231;  // White color index
        uint8_t idx = 232 + ((r - 8) * 24) / 247;
        if (idx >= _SFTE_COLOR_FG_DEFAULT) return _SFTE_COLOR_FG_DEFAULT - 1;
        return idx;
    }
    // Map to the color region (6x6x6)
    uint16_t cr = (r * 5) / 255;
    uint16_t cg = (g * 5) / 255;
    uint16_t cb = (b * 5) / 255;
    return 16 + (36 * cr) + (6 * cg) + cb;
}
#endif  // !SFTE_COLOR_TRUECOLOR

/*
    Returns a color in a form compatible with how colors are stored in `sfte_cell`/`sfte_term`.
    If SFTE_COLOR_TRUECOLOR is 1, returns a RGB color from the 256-color palette.
    Otherwise returns the index itself.
*/
static inline uint32_t _sfte_color_from_idx(uint16_t idx) {
#if SFTE_COLOR_TRUECOLOR
    return SFTE_COLOR_ALPHA_MASK | _sfte_palette_256[idx];
#else   // !SFTE_COLOR_TRUECOLOR
    // Last two indices are reserved for default colors
    if (idx >= _SFTE_COLOR_FG_DEFAULT) return _SFTE_COLOR_FG_DEFAULT - 1;
    return idx;
#endif  // !SFTE_COLOR_TRUECOLOR
}

/*
    Returns a color in a form compatible with how colors are stored in `sfte_cell`/`sfte_term`.
    If SFTE_COLOR_TRUECOLOR is 1, returns the RGB color directly.
    Otherwise returns the index to the closest color to `rgb` in the 256-color palette.
 */
static inline uint32_t _sfte_color_from_rgb(uint32_t rgb) {
#if SFTE_COLOR_TRUECOLOR
    return rgb;
#else   // !SFTE_COLOR_TRUECOLOR
    return _sfte_color_rgb_to_256((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
#endif  // !SFTE_COLOR_TRUECOLOR
}

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
