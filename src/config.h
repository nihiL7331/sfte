#pragma once

#include <float.h>
#include <stdint.h>

#ifndef SFTE_CUSTOM_BACKEND
#ifndef SFTE_WAYLAND
#define SFTE_WAYLAND 1
#endif  // SFTE_WAYLAND
#else   // SFTE_CUSTOM_BACKEND
#define SFTE_WAYLAND 0
#endif  // SFTE_CUSTOM_BACKEND

#if SFTE_WAYLAND && !defined(SFTE_XKB_COMMON)
#define SFTE_XKB_COMMON
#endif  // SFTE_WAYLAND && !defined(SFTE_XKB_COMMON)

// default value for SFTE_IMG_KITTY is 1, so if it's undefined its 1
#if !defined(SFTE_IMG_KITTY) || SFTE_IMG_KITTY
#include "vendor/stb_image.h"
#endif  // !defined(SFTE_IMG_KITTY) || SFTE_IMG_KITTY

#ifndef SFTE_FONT_CUSTOM_BACKEND
#include "vendor/stb_truetype.h"
typedef stbtt_fontinfo sfte_font_backend_info;
#else
typedef struct sfte_font_backend_info sfte_font_backend_info;
#endif  // SFTE_FONT_CUSTOM_BACKEND

typedef struct sfte_ctx sfte_ctx;
typedef struct sfte_term sfte_term;
typedef struct sfte_stack sfte_stack;

// =================================================================================================
// >>system/memory macros
// =================================================================================================

#define SFTE_ATTR_NONE 0
#define SFTE_ATTR_BOLD 1 << 0
#define SFTE_ATTR_ITALIC 1 << 1
#define SFTE_ATTR_UNDERLINE 1 << 2
#define SFTE_ATTR_REVERSE 1 << 3

/*
    Memory allocation macro hooks.
*/
#ifndef SFTE_MALLOC
#include <stdlib.h>
#define SFTE_MALLOC(sz) malloc(sz)
#define SFTE_REALLOC(p, sz) realloc(p, sz)
#define SFTE_CALLOC(n, sz) calloc(n, sz)
#define SFTE_FREE(p) free(p)
#endif  // !SFTE_MALLOC

#ifndef SFTE_ASSERT
#include <assert.h>
#define SFTE_ASSERT(c, m) assert((c) && (m))
#define SFTE_STATIC_ASSERT(c, m) _Static_assert(c, m)
#endif  // !SFTE_ASSERT

/*
    Internal helper macro used to ensure the value set
    for a macro is within predefined bounds [min;max].

    Use

    #define SFTE_NO_ENSURES

    to disable it, allowing to apply any value to any macro.
*/
#ifdef SFTE_NO_ENSURES
#define _SFTE_ENSURE_RANGE(val, min, max)
#else  // !SFTE_NO_ENSURES
#define _SFTE_ENSURE_RANGE(val, min, max)                                                          \
    SFTE_STATIC_ASSERT((val) >= (min) && (val) <= (max),                                           \
                       #val " must be strictly between " #min " and " #max)
#endif  // !SFTE_NO_ENSURES

/*
    Internal helper macro used to ensure the dependencies
    for a macro value were met.
    Usage:

    _SFTE_ENSURE_DEPS(dependent_macro, dependency1 && dependency2);

    Use

    #define SFTE_NO_ENSURES

    to disable it, allowing to apply any value to any macro.
*/
#ifdef SFTE_NO_ENSURES
#define _SFTE_ENSURE_DEPS(macro, dep)
#else  // !SFTE_NO_ENSURES
#define _SFTE_ENSURE_DEPS(macro, dep)                                                              \
    SFTE_STATIC_ASSERT(!(macro) || (dep), #macro " unmet dependencies: " #dep)
#endif  // !SFTE_NO_ENSURES

/*
    Internal helper macro used to ensure the value set
    for a macro is matching the internal implementation value.

    This ISN'T disabled by SFTE_NO_ENSURES, as disabling it would provide no value.
*/
#define _SFTE_ENSURE_EQUAL(val1, val2)                                                             \
    SFTE_STATIC_ASSERT((val1) == (val2), #val1 " must be strictly equal to " #val2)

typedef enum {
    SFTE_LOG_LVL_PANIC,
    SFTE_LOG_LVL_ERROR,
    SFTE_LOG_LVL_WARN,
    SFTE_LOG_LVL_INFO,
} sfte_log_level;

typedef void (*sfte_log_func)(const char *tag, sfte_log_level log_level, const char *msg,
                              uint32_t line_nr);

/*
    Available levels of severity:
    SFTE_LOG_LVL_PANIC
    SFTE_LOG_LVL_ERROR (default)
    SFTE_LOG_LVL_WARN
    SFTE_LOG_LVL_INFO
*/
#ifndef SFTE_LOG_LEVEL
#define SFTE_LOG_LEVEL SFTE_LOG_LVL_ERROR
#endif  // SFTE_LOG_LEVEL
_SFTE_ENSURE_RANGE(SFTE_LOG_LEVEL, SFTE_LOG_LVL_PANIC, SFTE_LOG_LVL_INFO);

/*
    String prefixed to every log output.
*/
#ifndef SFTE_LOG_TAG
#define SFTE_LOG_TAG "sfte"
#endif  // SFTE_LOG_TAG

/*
    Macro hook for logging callback.
    Must match the following definition:

    void log_func(const char *tag, sfte_log_level log_level, const char *msg, uint32_t line_nr);

*/
#ifndef SFTE_LOG_FUNC
#define SFTE_LOG_FUNC _sfte_log_default_func /* fuzz skip */
#endif                                       // SFTE_LOG_FUNC

/*
    Maximum size of the global scratch stack in bytes.
    Used for temporary allocations during OSC parsing, Sixel and Kitty image processing.

    WARN:
    Setting this value too low might cause unexpected issues.
*/
#ifndef SFTE_MEM_STACK_SIZE
#define SFTE_MEM_STACK_SIZE (1024 * 1024 * 4)
#endif  // SFTE_MEM_STACK_SIZE
_SFTE_ENSURE_RANGE(SFTE_MEM_STACK_SIZE, 1024, INT32_MAX);

// =================================================================================================
// >>term macros
// =================================================================================================

/*
    Enables focus tracking (DECSET 1004) and hollow cursor rendering.
*/
#ifndef SFTE_TERM_FOCUS
#define SFTE_TERM_FOCUS 1
#endif  // SFTE_TERM_FOCUS
_SFTE_ENSURE_RANGE(SFTE_TERM_FOCUS, 0, 1);

/*
    Number of bytes read from the PTY per poll event.
*/
#ifndef SFTE_TERM_PTY_BUF_SIZE
#define SFTE_TERM_PTY_BUF_SIZE UINT16_MAX
#endif  // SFTE_TERM_PTY_BUF_SIZE
_SFTE_ENSURE_RANGE(SFTE_TERM_PTY_BUF_SIZE, 1, UINT16_MAX);

/*
    Number of poll events read per 'burst'.
    Allows the compositor to draw a frame once in a while
    when output is constantly flowing.

    With *_PTY_BURST_SIZE=4 and *_PTY_BUF_SIZE=UINT16_MAX=65535,
    every 262140 bytes read a render occurs.
*/
#ifndef SFTE_TERM_PTY_BURST_SIZE
#define SFTE_TERM_PTY_BURST_SIZE 4
#endif  // SFTE_TERM_PTY_BURST_SIZE

/*
    Enables procedural rendering for box drawing characters (U+2500 - U+257F).
    Guarantees seamless, gapless lines for TUIs.
*/
#ifndef SFTE_TERM_CUSTOM_BOXES
#define SFTE_TERM_CUSTOM_BOXES 0
#endif  // SFTE_TERM_CUSTOM_BOXES
_SFTE_ENSURE_RANGE(SFTE_TERM_CUSTOM_BOXES, 0, 1);

/*
    Enables standard terminal alternate screen buffer (used by TUIs extensively).
    Can be disabled to halve the memory usage for logical grid,
    but CAN and WILL break TUIs rendering.
*/
#ifndef SFTE_TERM_ALT_SCREEN
#define SFTE_TERM_ALT_SCREEN 1
#endif  // SFTE_TERM_ALT_SCREEN
_SFTE_ENSURE_RANGE(SFTE_TERM_ALT_SCREEN, 0, 1);

/*
    Maintains a secondary pixel buffer to prevent tearing.
    Can be disabled without any noticeable changes on minimal setups,
    lowering the memory usage (and requiring one less memcpy in hot path).
    NOTE:
    Needs to be enabled for cursor trails and screen buffer transitions.
*/
#ifndef SFTE_TERM_DOUBLE_BUFFER
#define SFTE_TERM_DOUBLE_BUFFER 1
#endif  // SFTE_TERM_DOUBLE_BUFFER
_SFTE_ENSURE_RANGE(SFTE_TERM_DOUBLE_BUFFER, 0, 1);

/*
    Enables scrolling animation on TUI open/close (alt screen toggle).
    Obviously requires `SFTE_TERM_ALT_SCREEN` to be enabled to work.

    NOTE:
    This is an experimental feature.
    It makes the animation toggle on EVERY alt screen switch.
    It's purely sugar candy, might slightly affect the CPU usage.
    It also doesn't work if inside tmux.
*/
#ifndef SFTE_TERM_ANIMATE_SCREEN
#define SFTE_TERM_ANIMATE_SCREEN 0
#endif  // SFTE_TERM_ANIMATE_SCREEN
_SFTE_ENSURE_RANGE(SFTE_TERM_ANIMATE_SCREEN, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_TERM_ANIMATE_SCREEN, SFTE_TERM_ALT_SCREEN);

/*
    Enables text reflow when resizing the terminal window.
    With SFTE_TERM_REFLOW disabled, any text going off the right edge is deleted immediately.
*/
#ifndef SFTE_TERM_REFLOW
#define SFTE_TERM_REFLOW 1
#endif  // SFTE_TERM_REFLOW
_SFTE_ENSURE_RANGE(SFTE_TERM_REFLOW, 0, 1);

/*
    Enables scrollback history buffer.
*/
#ifndef SFTE_TERM_SCROLLBACK
#define SFTE_TERM_SCROLLBACK 1
#endif  // SFTE_TERM_SCROLLBACK_CAP
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLLBACK, 0, 1);

/*
    Enables smooth text scrolling.
    Essentially enables linear interpolation to closest row, replacing instant snapping.
    Because it requires polling while scrolling, it will affect CPU usage while scrolling.
*/
#ifndef SFTE_TERM_SCROLL_SMOOTH
#define SFTE_TERM_SCROLL_SMOOTH 0
#endif  // SFTE_TERM_SCROLL_SMOOTH
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLL_SMOOTH, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_TERM_SCROLL_SMOOTH, SFTE_TERM_SCROLLBACK);

/*
    Determines if `clear` commands (CSI 3 J) actually wipe the scrollback buffer.
    By default, the scrollback DOES get wiped on `clear` call.
*/
#ifndef SFTE_TERM_SCROLLBACK_CLEAR
#define SFTE_TERM_SCROLLBACK_CLEAR 1
#endif  // SFTE_TERM_SCROLLBACK_CLEAR
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLLBACK_CLEAR, 0, 1);

/*
    Narrows the allowed characters range to ASCII, lowering the memory usage.
    Enabling this allows the terminal to store each rune as a `uint8_t` rather than a `uint32_t`.
*/
#ifndef SFTE_TERM_ASCII_CHARSET
#define SFTE_TERM_ASCII_CHARSET 0
#endif  // SFTE_TERM_ASCII_CHARSET
_SFTE_ENSURE_RANGE(SFTE_TERM_ASCII_CHARSET, 0, 1);

// =================================================================================================
// >>color macros
// =================================================================================================

/*
    A macro helper used to draw pixels.
    This can be overriden to support different color formats,
    e.g.: monochrome, RGB233, RGB565.
*/
#ifndef SFTE_COLOR_DRAW_PIXEL
#define SFTE_COLOR_DRAW_PIXEL(buf, x, y, stride, height, color)                                    \
    do {                                                                                           \
        if ((x) >= 0 && (x) < (stride) && (y) >= 0 && (y) < (height))                              \
            ((uint32_t *)(buf))[(y) * (stride) + (x)] = (color);                                   \
    } while (0)
#endif  // SFTE_COLOR_DRAW_PIXEL

/*
    A macro helper used to blend pixel colors based on their alpha value.
    This can be overriden to support different color formats,
    e.g.: monochrome, RGB233, RGB565.
    Needed for cursor trail and images support.
*/
#ifndef SFTE_COLOR_BLEND_PIXEL
#define SFTE_COLOR_BLEND_PIXEL(buf, x, y, stride, rgb_color, alpha)                                \
    do {                                                                                           \
        (((uint32_t *)(buf))[(y) * (stride) + (x)] = _sfte_render_blend_argb(                      \
             ((uint32_t *)(buf))[(y) * (stride) + (x)], (rgb_color), (alpha)));                    \
    } while (0)
#endif  // SFTE_COLOR_BLEND_PIXEL

#define SFTE_COLOR_BLEND_PIXEL_SAFE(buf, x, y, stride, height, rgb_color, alpha)                   \
    do {                                                                                           \
        if ((x) >= 0 && (x) < (stride) && (y) >= 0 && (y) < (height))                              \
            SFTE_COLOR_BLEND_PIXEL((buf), (x), (y), (stride), (rgb_color), (alpha));               \
    } while (0)

#define SFTE_COLOR_ALPHA_MASK 0xFF000000

// =================================================================================================
// >>font macros
// =================================================================================================

#ifndef SFTE_FONT_CUSTOM_BACKEND
static inline void _sfte_stb_init(sfte_font_backend_info *info, const uint8_t *data);
static inline float _sfte_stb_get_scale(sfte_font_backend_info *info, float px_hei);
static inline void _sfte_stb_vmetrics(sfte_font_backend_info *info, int *ascent, int *descent,
                                      int *linegap);
static inline void _sfte_stb_bounds(sfte_font_backend_info *info, int glyph_id, float scale,
                                    int *adv, int *x0, int *y0, int *x1, int *y1);
static inline void _sfte_stb_bake(sfte_stack *stack, sfte_font_backend_info *info, int glyph_idx,
                                  float scale, uint8_t *atlas_ptr, int gw, int gh,
                                  int atlas_stride);
static inline int _sfte_stb_get_id(sfte_font_backend_info *info, uint32_t rune);
#else  // SFTE_FONT_CUSTOM_BACKEND
#if !defined(SFTE_FONT_INIT) || !defined(SFTE_FONT_GET_SCALE) || !defined(SFTE_FONT_VMETRICS) ||   \
    !defined(SFTE_FONT_BOUNDS) || !defined(SFTE_FONT_BAKE) || !defined(SFTE_FONT_GET_ID)
#error                                                                                             \
    "SFTE_FONT_CUSTOM_BACKEND requires defining all 6 macro hooks: INIT, GET_SCALE, VMETRICS, BOUNDS, BAKE and GET_ID."
#endif  // !defined(SFTE_FONT_INIT) || !defined(SFTE_FONT_GET_SCALE) || !defined(SFTE_FONT_VMETRICS)
        // || !defined(SFTE_FONT_BOUNDS) || !defined(SFTE_FONT_BAKE) || !defined(SFTE_FONT_GET_ID)
#endif  // SFTE_FONT_CUSTOM_BACKEND

/*
    Distance in pixels between each glyph in the atlas 2D texture.
*/
#ifndef SFTE_FONT_PADDING
#define SFTE_FONT_PADDING 1
#endif  // SFTE_FONT_PADDING

/*
    Enables font ligatures support.
*/
#ifndef SFTE_FONT_LIGATURES
#define SFTE_FONT_LIGATURES 1
#endif  // SFTE_FONT_LIGATURES
_SFTE_ENSURE_RANGE(SFTE_FONT_LIGATURES, 0, 1);

/*
    Maximum amount of lookup indices for a ligature shaper feature.
*/
#ifndef SFTE_FONT_MAX_LIGATURE_LOOKUPS
#define SFTE_FONT_MAX_LIGATURE_LOOKUPS 256
#endif  // SFTE_FONT_MAX_LIGATURE_LOOKUPS
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_LIGATURE_LOOKUPS, 1, UINT16_MAX);

/*
    Maximum amount of subtables per lookup for ligatures.
*/
#ifndef SFTE_FONT_MAX_LIGATURE_SUBTABLES
#define SFTE_FONT_MAX_LIGATURE_SUBTABLES 16
#endif  // SFTE_FONT_MAX_LIGATURE_SUBTABLES
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_LIGATURE_SUBTABLES, 1, UINT16_MAX);

/*
    Maximum amount of records for ligatures.
*/
#ifndef SFTE_FONT_MAX_LIGATURE_RECORDS
#define SFTE_FONT_MAX_LIGATURE_RECORDS 8
#endif  // SFTE_FONT_MAX_LIGATURE_RECORDS
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_LIGATURE_RECORDS, 1, UINT16_MAX);

/*
    Max number of fallback fonts (primary + fallbacks).
*/
#ifndef SFTE_FONT_MAX_COUNT
#define SFTE_FONT_MAX_COUNT 4
#endif  // SFTE_FONT_MAX_COUNT
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_COUNT, 1, INT8_MAX);

/*
    Font oversample scale.
    If set to 1, no oversampling occurs.

    NOTE:
    To avoid jagged baseline issues, blurring of horizontal stems
    oversampling only applies horizontally, not vertically.
*/
#ifndef SFTE_FONT_OVERSAMPLE
#define SFTE_FONT_OVERSAMPLE 1
#endif  // SFTE_FONT_OVERSAMPLE
_SFTE_ENSURE_RANGE(SFTE_FONT_OVERSAMPLE, 1, 16);

/*
    Enables Ctrl +/- font zooming at runtime.
*/
#ifndef SFTE_FONT_ZOOM
#define SFTE_FONT_ZOOM 1
#endif  // SFTE_FONT_ZOOM
_SFTE_ENSURE_RANGE(SFTE_FONT_ZOOM, 0, 1);

/*
    Enables double-width characters rendering (useful e.g. for Chinese symbols).
*/
#ifndef SFTE_FONT_WIDE_CHARS
#define SFTE_FONT_WIDE_CHARS 1
#endif  // SFTE_FONT_WIDE_CHARS
_SFTE_ENSURE_RANGE(SFTE_FONT_WIDE_CHARS, 0, 1);

/*
    Fixes clipping issues on symbols bigger than their cell by expanding the dirty render box.
    This is necessary for nerd symbols to render correctly,
    since very often they go out of their cell bounds.
    Disabling it removes a O(W^2 x H) loop from the hot path.
*/
#ifndef SFTE_FONT_BLEED
#define SFTE_FONT_BLEED 1
#endif  // SFTE_FONT_BLEED
_SFTE_ENSURE_RANGE(SFTE_FONT_BLEED, 0, 1);

/*
    Dimensions for the 2D texture atlas caching rendered glyphs.
    One atlas is shared across all font types and font fallbacks.
*/
#ifndef SFTE_FONT_ATLAS_SIZE
#define SFTE_FONT_ATLAS_SIZE 512
#endif  // SFTE_FONT_ATLAS_SIZE
_SFTE_ENSURE_RANGE(SFTE_FONT_ATLAS_SIZE, 1, INT32_MAX);

/*
    Maximum number of distinct characters cached in memory at once.
    One glyph buffer is shared across all font types and font fallbacks.
*/
#ifndef SFTE_FONT_GLYPH_CAP
#define SFTE_FONT_GLYPH_CAP 4096
#endif  // SFTE_FONT_GLYPH_CAP
_SFTE_ENSURE_RANGE(SFTE_FONT_GLYPH_CAP, 1, UINT16_MAX);

/*
    Maximum amount of combining (width = 0) glyphs on one cell.
*/
#ifndef SFTE_FONT_MAX_COMBINING
#define SFTE_FONT_MAX_COMBINING 2
#endif  // SFTE_FONT_MAX_COMBINING
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_COMBINING, 0, 16);

// =================================================================================================
// >>cursor macros
// =================================================================================================

typedef enum sfte_cursor_style {
    SFTE_CURSOR_STYLE_BLOCK,
    SFTE_CURSOR_STYLE_UNDERLINE,
    SFTE_CURSOR_STYLE_BAR,
} sfte_cursor_style;

/*
    Allows programs to dynamically change the cursor shape via escape sequences.
    If set to 0, cursor style stays as SFTE_CURSOR_STYLE.
*/
#ifndef SFTE_CURSOR_DYNAMIC
#define SFTE_CURSOR_DYNAMIC 1
#endif  // SFTE_CURSOR_DYNAMIC
_SFTE_ENSURE_RANGE(SFTE_CURSOR_DYNAMIC, 0, 1);

/*
    Enables cursor blinking.
    This technically affects CPU usage, since it requires polling on every state change.
*/
#ifndef SFTE_CURSOR_BLINK
#define SFTE_CURSOR_BLINK 1
#endif  // SFTE_CURSOR_BLINK
_SFTE_ENSURE_RANGE(SFTE_CURSOR_BLINK, 0, 1);

/*
    Enables rendering an animated smooth-scrolling ghost trail behind the cursor,
    similar to cursor_trail from kitty or default (I believe?) neovide cursor trail.

    This affects CPU usage, since it requires polling every 16ms when the trail is visible.
    It requires double buffering, otherwise resulting in terrible rendering artifacts.
*/
#ifndef SFTE_CURSOR_TRAIL
#define SFTE_CURSOR_TRAIL 1
#endif  // SFTE_CURSOR_TRAIL
_SFTE_ENSURE_RANGE(SFTE_CURSOR_TRAIL, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_CURSOR_TRAIL, SFTE_TERM_DOUBLE_BUFFER);

// =================================================================================================
// >>underline macros
// =================================================================================================

/*
    Enables rendering of double, curly, dotted and dashed underlines (CSI 4:x m).
    Undercurls are rendered using a simple triangle wave to avoid CPU-heavy math calls.
*/
#ifndef SFTE_UNDERLINE_EXTENDED
#define SFTE_UNDERLINE_EXTENDED 1
#endif  // SFTE_UNDERLINE_EXTENDED
_SFTE_ENSURE_RANGE(SFTE_UNDERLINE_EXTENDED, 0, 1);

/*
    Enables underlines with custom true-colors distint from the text (CSI 58:2::R:G:B m).
    Default underline color is `cfg->color.fg`.
*/
#ifndef SFTE_UNDERLINE_COLORED
#define SFTE_UNDERLINE_COLORED 1
#endif  // SFTE_UNDERLINE_COLORED
_SFTE_ENSURE_RANGE(SFTE_UNDERLINE_COLORED, 0, 1);

// =================================================================================================
// >>img macros
// =================================================================================================

/*
    Enables DEC VT340 sixel bitmap graphics support.
*/
#ifndef SFTE_IMG_SIXEL
#define SFTE_IMG_SIXEL 1
#endif  // SFTE_IMG_SIXEL
_SFTE_ENSURE_RANGE(SFTE_IMG_SIXEL, 0, 1);

/*
    Enables kitty image protocol support.
*/
#ifndef SFTE_IMG_KITTY
#define SFTE_IMG_KITTY 1
#endif  // SFTE_IMG_KITTY
_SFTE_ENSURE_RANGE(SFTE_IMG_KITTY, 0, 1);

/*
    Maximum allocated dimension size for a temporary pixel buffer
    used while creating a sixel image from escape sequences.
    Biggest renderable Sixel image is SFTE_IMG_SIXEL_MAX_SIZE x SFTE_IMG_SIXEL_MAX_SIZE,
    assuming image pool size is sufficient.
*/
#ifndef SFTE_IMG_SIXEL_MAX_SIZE
#define SFTE_IMG_SIXEL_MAX_SIZE 4096
#endif  // SFTE_IMG_SIXEL_MAX_SIZE
_SFTE_ENSURE_RANGE(SFTE_IMG_SIXEL_MAX_SIZE, 64, INT32_MAX);

/*
    Smallest allocated dimension size for a temporary pixel buffer
    used while creating a sixel image from escape sequences.
    Initial allocation is SFTE_IMG_SIXEL_INIT_SIZE x SFTE_IMG_SIXEL_INIT_SIZE.
*/
#ifndef SFTE_IMG_SIXEL_INIT_SIZE
#define SFTE_IMG_SIXEL_INIT_SIZE 64
#endif  // SFTE_IMG_SIXEL_INIT_SIZE
_SFTE_ENSURE_RANGE(SFTE_IMG_SIXEL_INIT_SIZE, 1, SFTE_IMG_SIXEL_MAX_SIZE);

/*
    Maximum capacity of the base64 kitty encoding temporary buffer.
    This is required so that the terminal doesn't
    get bombed with a 100GB bugged/malware escape sequence.
    Default is 16MB, might not be enough for big 4K images.

    NOTE:
    Currently sfte doesn't run encoding on a worker thread,
    so opening big images might cause visible stutters.
*/
#ifndef SFTE_KITTY_B64_MAX_CAP
#define SFTE_KITTY_B64_MAX_CAP (16 * 1024 * 1024)
#endif  // SFTE_KITTY_B64_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_KITTY_B64_MAX_CAP, 128, UINT32_MAX);

/*
    Initial capacity of the base64 kitty encoding temporary buffer.
    Due to how kitty sequences are structured, it can grow (gets doubled on OOM)
    while reading the image data, since size is unknown during the read.
*/
#ifndef SFTE_KITTY_B64_INIT_CAP
#define SFTE_KITTY_B64_INIT_CAP 128
#endif  // SFTE_KITTY_B64_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_KITTY_B64_INIT_CAP, 1, SFTE_KITTY_B64_MAX_CAP);

/*
    Maximum capacity of the shared image reference pool.
    This defines how many DIFFERENT images can be rendered at once.
*/
#ifndef SFTE_IMG_POOL_MAX_CAP
#define SFTE_IMG_POOL_MAX_CAP 1024
#endif  // SFTE_IMG_POOL_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_IMG_POOL_MAX_CAP, 8, UINT32_MAX);

/*
    Initial capacity of the shared image reference pool.
*/
#ifndef SFTE_IMG_POOL_INIT_CAP
#define SFTE_IMG_POOL_INIT_CAP 8
#endif  // SFTE_IMG_POOL_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_IMG_POOL_INIT_CAP, 1, SFTE_IMG_POOL_MAX_CAP);

/*
    Maximum capacity of active viewport placements.
    This defines how many IDENTICAL images can be rendered at once.
*/
#ifndef SFTE_IMG_PLACEMENT_MAX_CAP
#define SFTE_IMG_PLACEMENT_MAX_CAP 4096
#endif  // SFTE_IMG_PLACEMENT_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_IMG_PLACEMENT_MAX_CAP, 16, UINT32_MAX);

/*
    Initial capacity of active viewport placements.
*/
#ifndef SFTE_IMG_PLACEMENT_INIT_CAP
#define SFTE_IMG_PLACEMENT_INIT_CAP 16
#endif  // SFTE_IMG_PLACEMENT_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_IMG_PLACEMENT_INIT_CAP, 1, SFTE_IMG_PLACEMENT_MAX_CAP);

// =================================================================================================
// >>search macros
// =================================================================================================

/*
    Adds scrollback search capability to the terminal.
    Supports regex via regex.h for POSIX, tiny-regex-c (github.com/kokke/tiny-regex-c) otherwise.
    The default search shortcut is Ctrl+Shift+/.

    NOTE:
    tiny-regex-c implementation is modified to be a single header and
    scoped only to this translation unit via `static inline` prefixes.
*/
#ifndef SFTE_SEARCH
#define SFTE_SEARCH 1
#endif  // SFTE_SEARCH
_SFTE_ENSURE_RANGE(SFTE_SEARCH, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_SEARCH, SFTE_TERM_SCROLLBACK);

/*
    Maximum capacity of the search matches array.
*/
#ifndef SFTE_SEARCH_MATCH_MAX_CAP
#define SFTE_SEARCH_MATCH_MAX_CAP 4096
#endif  // SFTE_SEARCH_MATCH_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_SEARCH_MATCH_MAX_CAP, 16, UINT32_MAX);

/*
    Initial capacity of the search matches array.
*/
#ifndef SFTE_SEARCH_MATCH_INIT_CAP
#define SFTE_SEARCH_MATCH_INIT_CAP 16
#endif  // SFTE_SEARCH_MATCH_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_SEARCH_MATCH_INIT_CAP, 1, SFTE_SEARCH_MATCH_MAX_CAP);

/*
    Size of the extraction row buffer (in bytes).
    Allocated temporarily on the thread stack during the search execution.
*/
#ifndef SFTE_SEARCH_ROW_BUF_CAP
#define SFTE_SEARCH_ROW_BUF_CAP 8192
#endif  // SFTE_SEARCH_ROW_BUF_CAP
_SFTE_ENSURE_RANGE(SFTE_SEARCH_ROW_BUF_CAP, 128, 65536);

// =================================================================================================
// >>mux macros
// =================================================================================================

/*
    Enables the native terminal multiplexer.
*/
#ifndef SFTE_MUX
#define SFTE_MUX 1
#endif  // SFTE_MUX
_SFTE_ENSURE_RANGE(SFTE_MUX, 0, 1);

/*
    Enables the multiplexer bar, known from tmux.
    Visuals of the bar can be customized via options below.
*/
#ifndef SFTE_MUX_BAR
#define SFTE_MUX_BAR 1
#endif  // SFTE_MUX_BAR
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_MUX_BAR, SFTE_MUX);

#define SFTE_MUX_BAR_ALIGN_LEFT 0
#define SFTE_MUX_BAR_ALIGN_CENTER 1
#define SFTE_MUX_BAR_ALIGN_RIGHT 2
#define SFTE_MUX_BAR_ALIGN_FILL 3

/*
    If enabled, renders a preview of available windows when prefix is clicked.
    They are rendered as a vertical list, each block showing the bottom each window.
    A window in that mode can be picked using keyboard numbers (0-9).
*/
#ifndef SFTE_MUX_PREFIX_PREVIEW
#define SFTE_MUX_PREFIX_PREVIEW 0
#endif  // SFTE_MUX_PREFIX_PREVIEW
_SFTE_ENSURE_RANGE(SFTE_MUX_PREFIX_PREVIEW, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_MUX_PREFIX_PREVIEW, SFTE_MUX);

/*
    Set of characters used to render borders for the prefix preview.
*/
#ifndef SFTE_MUX_PREVIEW_BORDER_LR
#define SFTE_MUX_PREVIEW_BORDER_LR 0x2500   // ─
#define SFTE_MUX_PREVIEW_BORDER_TB 0x2502   // │
#define SFTE_MUX_PREVIEW_BORDER_BR 0x250C   // ┌
#define SFTE_MUX_PREVIEW_BORDER_BL 0x2510   // ┐
#define SFTE_MUX_PREVIEW_BORDER_TR 0x2514   // └
#define SFTE_MUX_PREVIEW_BORDER_TL 0x2518   // ┘
#define SFTE_MUX_PREVIEW_BORDER_TBR 0x251C  // ├
#define SFTE_MUX_PREVIEW_BORDER_TBL 0x2524  // ┤
#endif                                      // SFTE_MUX_PREVIEW_BORDER_LR

/*
    The maximum number concurrent windows the multiplexer can hold.

    NOTE:
    Windows with indexes greater than 9 (0-indexed)
    can't be accessed by default via one key press.
    They can be mapped via shortcuts
    or opened using arrows in preview mode.
*/
#ifndef SFTE_MUX_MAX_WINDOWS
#define SFTE_MUX_MAX_WINDOWS 10
#endif  // SFTE_MUX_MAX_WINDOWS
_SFTE_ENSURE_RANGE(SFTE_MUX_MAX_WINDOWS, 0, 99);

/*
    If multiplexer is disabled,
    treat the terminal as a one-window multiplexed terminal.
*/
#if !SFTE_MUX
#undef SFTE_MUX_MAX_WINDOWS
#define SFTE_MUX_MAX_WINDOWS 1
#endif  // !SFTE_MUX

// =================================================================================================
// >>input macros
// =================================================================================================

/*
    Enables mouse support.
    Required for SFTE_INPUT_SELECTION to work.
*/
#ifndef SFTE_INPUT_MOUSE
#define SFTE_INPUT_MOUSE 1
#endif  // SFTE_INPUT_MOUSE
_SFTE_ENSURE_RANGE(SFTE_INPUT_MOUSE, 0, 1);

/*
    Enables text selection.

    Requires
    #define SFTE_INPUT_MOUSE 1
    to work.
*/
#ifndef SFTE_INPUT_SELECTION
#define SFTE_INPUT_SELECTION 1
#endif  // SFTE_INPUT_SELECTION
_SFTE_ENSURE_RANGE(SFTE_INPUT_SELECTION, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_INPUT_SELECTION, SFTE_INPUT_MOUSE);

/*
    Enables kitty extended keyboard protocol.
    If enabled, the terminal sends key release events and complex modifiers.
*/
#ifndef SFTE_INPUT_KITTY
#define SFTE_INPUT_KITTY 1
#endif  // SFTE_INPUT_KITTY
_SFTE_ENSURE_RANGE(SFTE_INPUT_KITTY, 0, 1);

/*
    Enables OSC 8 clickable terminal hyperlinks.
*/
#ifndef SFTE_INPUT_HYPERLINKS
#define SFTE_INPUT_HYPERLINKS 1
#endif  // SFTE_INPUT_HYPERLINKS
_SFTE_ENSURE_RANGE(SFTE_INPUT_HYPERLINKS, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_INPUT_HYPERLINKS, SFTE_INPUT_MOUSE);

/*
    Defines the maximum size (in bytes) of a input text buffer used for search box and
    multiplexing window title renames.
*/
#ifndef SFTE_INPUT_TEXT_MAX_LEN
#define SFTE_INPUT_TEXT_MAX_LEN 256
#endif  // SFTE_INPUT_TEXT_MAX_LEN

/*
    Maximum dynamic buffer count for hyperlinks.
    Each cell stores a link index, so this defines the maximum amount of UNIQUE hyperlinks.
*/
#ifndef SFTE_INPUT_HYPERLINKS_MAX_CAP
#define SFTE_INPUT_HYPERLINKS_MAX_CAP 65535
#endif  // SFTE_INPUT_HYPERLINKS_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_INPUT_HYPERLINKS_MAX_CAP, 16, UINT32_MAX);

/*
    Initial dynamic buffer count for hyperlinks.
*/
#ifndef SFTE_INPUT_HYPERLINKS_INIT_CAP
#define SFTE_INPUT_HYPERLINKS_INIT_CAP 16
#endif  // SFTE_INPUT_HYPERLINKS_POOL_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_INPUT_HYPERLINKS_INIT_CAP, 1, SFTE_INPUT_HYPERLINKS_MAX_CAP);

// =================================================================================================
// >>clipboard macros
// =================================================================================================

/*
    Enables system clipboard sync.
*/
#ifndef SFTE_CLIPBOARD
#define SFTE_CLIPBOARD 1
#endif  // SFTE_CLIPBOARD
_SFTE_ENSURE_RANGE(SFTE_CLIPBOARD, 0, 1);

/*
    Maximum buffer size to copy at once.
*/
#ifndef SFTE_CLIPBOARD_BUF_SIZE
#define SFTE_CLIPBOARD_BUF_SIZE 4096
#endif  // SFTE_CLIPBOARD_BUF_SIZE
_SFTE_ENSURE_RANGE(SFTE_CLIPBOARD_BUF_SIZE, 1, UINT32_MAX);

/*
    Allows host applications to read/write the clipboard via OSC 52.
    This is especially useful to synchronize the clipboard with a SSH'd machine.
*/
#ifndef SFTE_CLIPBOARD_OSC52
#define SFTE_CLIPBOARD_OSC52 1
#endif  // SFTE_CLIPBOARD_OSC52
_SFTE_ENSURE_RANGE(SFTE_CLIPBOARD_OSC52, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_CLIPBOARD_OSC52, SFTE_CLIPBOARD);

// =================================================================================================
// >>osc macros
// =================================================================================================

/*
    Maximum size of OSC payload buffer.
    Used for base64 clipboard data or long links.
*/
#ifndef SFTE_OSC_MAX_CAP
#define SFTE_OSC_MAX_CAP (16 * 1024 * 1024)
#endif  // SFTE_OSC_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_OSC_MAX_CAP, 128, UINT32_MAX);

/*
    Initial size of OSC payload buffer.
*/
#ifndef SFTE_OSC_INIT_CAP
#define SFTE_OSC_INIT_CAP 128
#endif  // SFTE_OSC_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_OSC_INIT_CAP, 1, SFTE_OSC_MAX_CAP);

// =================================================================================================
// >>modifiers and shortcuts macros
// =================================================================================================

typedef union {
    int i;
    float f;
    const void *v;
} sfte_arg;

typedef enum sfte_key {
    SFTE_KEY_NONE = 0,
    // Control keys (ASCII-based)
    SFTE_KEY_TAB = 9,
    SFTE_KEY_ENTER = 13,
    SFTE_KEY_ESCAPE = 27,
    SFTE_KEY_BACKSPACE = 127,
    // Navigation keys (offset into dedicated range)
    SFTE_KEY_UP = 0x110000,  // Unicode maximum is 0x10FFFF, this goes one above to not collide
    SFTE_KEY_DOWN,
    SFTE_KEY_LEFT,
    SFTE_KEY_RIGHT,
    SFTE_KEY_HOME,
    SFTE_KEY_END,
    SFTE_KEY_PAGE_UP,
    SFTE_KEY_PAGE_DOWN,
    SFTE_KEY_INSERT,
    SFTE_KEY_DELETE,
    // Function keys
    SFTE_KEY_F1,
    SFTE_KEY_F2,
    SFTE_KEY_F3,
    SFTE_KEY_F4,
    SFTE_KEY_F5,
    SFTE_KEY_F6,
    SFTE_KEY_F7,
    SFTE_KEY_F8,
    SFTE_KEY_F9,
    SFTE_KEY_F10,
    SFTE_KEY_F11,
    SFTE_KEY_F12,
} sfte_key;

typedef struct sfte_shortcut {
    uint32_t mod_mask;
    uint32_t key;
    uint8_t (*func)(sfte_ctx *ctx, const sfte_arg *);
    const sfte_arg arg;
} sfte_shortcut;

#if SFTE_WAYLAND
#if SFTE_FONT_ZOOM
static inline uint8_t _sfte_wayland_font_resize(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_wayland_font_reset(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_FONT_ZOOM
#if SFTE_TERM_SCROLLBACK
static inline uint8_t _sfte_wayland_view_scroll(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_TERM_SCROLLBACK
static inline uint8_t _sfte_wayland_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_wayland_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_WAYLAND

#if SFTE_MUX
static inline uint8_t _sfte_mux_prefix_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_mux_spawn_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_mux_close_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_mux_jump_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_mux_goto_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_mux_rename_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_mux_reorder_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
#ifndef SFTE_MUX_BINDS
#define _SFTE_MUX_BINDS                                                                            \
    {SFTE_MOD_CTRL, 'a', _sfte_mux_prefix_shortcut, {.v = NULL}},                                  \
        {SFTE_MOD_NONE, 'c', _sfte_mux_spawn_shortcut, {.v = NULL}},                               \
        {SFTE_MOD_NONE, 'x', _sfte_mux_close_shortcut, {.v = NULL}},                               \
        {SFTE_MOD_NONE, 'n', _sfte_mux_jump_shortcut, {.i = 1}},                                   \
        {SFTE_MOD_NONE, 'b', _sfte_mux_jump_shortcut, {.i = -1}},                                  \
        {SFTE_MOD_NONE, 'r', _sfte_mux_rename_shortcut, {.v = NULL}},                              \
        {SFTE_MOD_NONE, SFTE_KEY_LEFT, _sfte_mux_reorder_shortcut, {.i = -1}},                     \
        {SFTE_MOD_NONE, SFTE_KEY_RIGHT, _sfte_mux_reorder_shortcut, {.i = 1}},                     \
        {SFTE_MOD_NONE, '0', _sfte_mux_goto_shortcut, {.i = 9}},                                   \
        {SFTE_MOD_NONE, '1', _sfte_mux_goto_shortcut, {.i = 0}},                                   \
        {SFTE_MOD_NONE, '2', _sfte_mux_goto_shortcut, {.i = 1}},                                   \
        {SFTE_MOD_NONE, '3', _sfte_mux_goto_shortcut, {.i = 2}},                                   \
        {SFTE_MOD_NONE, '4', _sfte_mux_goto_shortcut, {.i = 3}},                                   \
        {SFTE_MOD_NONE, '5', _sfte_mux_goto_shortcut, {.i = 4}},                                   \
        {SFTE_MOD_NONE, '6', _sfte_mux_goto_shortcut, {.i = 5}},                                   \
        {SFTE_MOD_NONE, '7', _sfte_mux_goto_shortcut, {.i = 6}},                                   \
        {SFTE_MOD_NONE, '8', _sfte_mux_goto_shortcut, {.i = 7}},                                   \
        {SFTE_MOD_NONE, '9', _sfte_mux_goto_shortcut, {.i = 8}},
#endif  // !SFTE_MUX_BINDS
#else   // !SFTE_MUX
#define _SFTE_MUX_BINDS
#endif  // !SFTE_MUX

#if SFTE_SEARCH
static inline uint8_t _sfte_search_toggle_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_search_prev_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_search_next_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
static inline uint8_t _sfte_search_clear_shortcut(sfte_ctx *ctx, const sfte_arg *arg);
#ifndef _SFTE_SEARCH_BINDS
#define _SFTE_SEARCH_BINDS                                                                         \
    {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, '?', _sfte_search_toggle_shortcut, {.v = NULL}},              \
        {SFTE_MOD_NONE, SFTE_KEY_UP, _sfte_search_next_shortcut, {.v = NULL}},                     \
        {SFTE_MOD_NONE, SFTE_KEY_DOWN, _sfte_search_prev_shortcut, {.v = NULL}},                   \
        {SFTE_MOD_NONE, SFTE_KEY_ESCAPE, _sfte_search_clear_shortcut, {.i = 1}},                   \
        {SFTE_MOD_NONE, SFTE_KEY_BACKSPACE, _sfte_search_clear_shortcut, {.i = 0}},
#endif  // !_SFTE_SEARCH_BINDS
#else   // !SFTE_SEARCH
#define _SFTE_SEARCH_BINDS
#endif  // !SFTE_SEARCH

#if SFTE_FONT_ZOOM && SFTE_WAYLAND
#ifndef _SFTE_WAYLAND_ZOOM_BINDS
#define _SFTE_WAYLAND_ZOOM_BINDS                                                                   \
    {SFTE_MOD_CTRL, '=', _sfte_wayland_font_resize, {.f = 2.0f}},                                  \
        {SFTE_MOD_CTRL, '+', _sfte_wayland_font_resize, {.f = 2.0f}},                              \
        {SFTE_MOD_CTRL, '-', _sfte_wayland_font_resize, {.f = -2.0f}},                             \
        {SFTE_MOD_CTRL, '0', _sfte_wayland_font_reset, {.v = NULL}},
#endif  // !_SFTE_WAYLAND_ZOOM_BINDS
#else   // !SFTE_FONT_ZOOM || !SFTE_WAYLAND
#define _SFTE_WAYLAND_ZOOM_BINDS
#endif  // !SFTE_FONT_ZOOM || !SFTE_WAYLAND

#if SFTE_TERM_SCROLLBACK_CAP && SFTE_WAYLAND
#ifndef _SFTE_WAYLAND_SCROLL_BINDS
#define _SFTE_WAYLAND_SCROLL_BINDS                                                                 \
    {SFTE_MOD_SHIFT, SFTE_KEY_PAGE_UP, _sfte_wayland_view_scroll, {.i = 10}},                      \
        {SFTE_MOD_SHIFT, SFTE_KEY_PAGE_DOWN, _sfte_wayland_view_scroll, {.i = -10}},
#endif  // !_SFTE_WAYLAND_SCROLL_BINDS
#else   // !SFTE_TERM_SCROLLBACK_CAP || !SFTE_WAYLAND
#define _SFTE_WAYLAND_SCROLL_BINDS
#endif  // !SFTE_TERM_SCROLLBACK_CAP || !SFTE_WAYLAND

#if SFTE_CLIPBOARD && SFTE_WAYLAND
#if SFTE_INPUT_SELECTION
#define _SFTE_WAYLAND_COPY_BIND                                                                    \
    {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, 'c', _sfte_wayland_clipboard_copy, {.v = NULL}},
#endif  // SFTE_INPUT_SELECTION
#define _SFTE_WAYLAND_PASTE_BIND                                                                   \
    {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, 'v', _sfte_wayland_clipboard_paste, {.v = NULL}},
#else  // !SFTE_CLIPBOARD || !SFTE_WAYLAND
#define _SFTE_WAYLAND_PASTE_BIND
#endif  // !SFTE_CLIPBOARD || !SFTE_WAYLAND

#define SFTE_BASE_SHORTCUTS                                                                        \
    _SFTE_MUX_BINDS _SFTE_SEARCH_BINDS _SFTE_WAYLAND_ZOOM_BINDS _SFTE_WAYLAND_SCROLL_BINDS         \
        _SFTE_WAYLAND_COPY_BIND _SFTE_WAYLAND_PASTE_BIND

// =================================================================================================
// >>default configuration
// =================================================================================================

typedef struct sfte_config {
    struct {
        /* An array of paths to regular/bold/italic/bold italic fonts, sorted by priority.
           Font of index 0 in each array is the primary font, next ones are used as fallback
           if a glyph is missing.
           This is most useful when using a font without nerd symbols.
           Defining cfg->font.regular as:
           {"/path/to/primary_font.ttf", "/path/to/nerd_symbols.ttf"}
           allows to have both the wanted font and nerd symbols for TUIs like nvim.
           */
        const char *regular[SFTE_FONT_MAX_COUNT];
#ifdef SFTE_FONT_BOLD
        const char *bold[SFTE_FONT_MAX_COUNT];
#endif
#ifdef SFTE_FONT_ITALIC
        const char *italic[SFTE_FONT_MAX_COUNT];
#endif
#ifdef SFTE_FONT_BOLD_ITALIC
        const char *bold_italic[SFTE_FONT_MAX_COUNT];
#endif

        float default_size;  // Initial font size on startup (in pxs).
        float min_size;      // Minimum achievable font size (in pxs).
        float max_size;      // Maximum achievable font size (in pxs).

        // Scales array, matching the order of font file paths as specified in the config.
        // So, if scales={1.0f, 0.8f}, then regular/bold/italic fonts under:
        //  - index 0 will be scaled by 1.0,
        //  - index 1 will be scaled by 0.8.
        float scales[SFTE_FONT_MAX_COUNT];
    } font;
    struct {
        uint32_t bg;                // ARGB 0xAARRGGBB
        uint32_t fg;                // RGB  0x__RRGGBB
        uint32_t cursor;            // ARGB 0xAARRGGBB
        uint32_t ansi_palette[16];  // RGB  0x__RRGGBB
    } color;
    struct {
        uint16_t pad_x;             // Horizontal window padding in pixels.
        uint16_t pad_y;             // Vertical window padding in pixels.
        uint16_t init_cols;         // Initial number of columns.
        uint16_t init_rows;         // Initial number of rows.
        uint16_t refresh_rate;      // Refresh rate used for animations (e.g. cursor trail).
        uint8_t tab_width;          // Tab width in characters (columns).
        const char *default_title;  // Default title set to each window on startup.
        const char *term_env;       // Terminal environment identifier
#if SFTE_TERM_ANIMATE_SCREEN
        float anim_dur_ms;  // Duration of alt-screen swap animation in milliseconds.
#endif
    } window;
    struct {
        sfte_cursor_style style;  // Default cursor style enum value.
#if SFTE_CURSOR_BLINK
        uint32_t blink_rate_ms;  // Half-blink animation duration in milliseconds.
#endif
        float thick_ratio;  // Thickness ratio relative to cell dimensions (for bar/underline).
#if SFTE_CURSOR_TRAIL
        float trail_decay;         // Decaying speed of the cursor trail.
        uint16_t trail_threshold;  // Minimum time of cursor movement required to render trail.
#endif
    } cursor;
    struct {
        float thick_ratio;  // Thickness of underlines relative to cell dimensions.
        float off_ratio;    // Offset from bottom of the cell, relative to cell dimensions.
    } underline;
    struct {
        int32_t scrollback_cap;  // Maximum scrollback capacity (in rows).
        uint32_t step;           // Step taken each time a 'scroll' action is toggled.
#if SFTE_TERM_SCROLL_SMOOTH
        float decay;  // Speed of smooth scroll decay per frame, where 0.0=none; 1.0=instant.
#endif
        double threshold;   // Scroll value required to toggle a 'scroll' action.
        uint8_t on_output;  // If enabled, moves the viewport to prompt on any output.
    } scroll;
    struct {
        /* If custom shortcuts are defined and any of them are overlapping,
           the one first defined will be always toggled. */
        const sfte_shortcut *custom;  // Terminated via a {0} element.
        uint8_t override_defaults;    // If =0, check custom then defaults; if =1, disable defaults.
    } shortcuts;
#if SFTE_SEARCH
    struct {
        const char *prefix;    // String prepending the search query.
        uint8_t top;           // If enabled, the search bar will appear at the top.
        uint32_t bg_bar;       // ARGB 0xAARRGGBB
        uint32_t bg_active;    // ARGB 0xAARRGGBB
        uint32_t bg_match;     // ARGB 0xAARRGGBB
        uint32_t fg_active;    // RGB  0x__RRGGBB
        uint32_t fg_match;     // RGB  0x__RRGGBB
        uint32_t attr_active;  // Attributes applied to active     match, bitmask from SFTE_ATTR_*.
        uint32_t attr_match;   // Attributes applied to non-active match, bitmask from SFTE_ATTR_*.
    } search;
#endif  // SFTE_SEARCH
#if SFTE_MUX_BAR
    struct {
        uint8_t top;    // If enabled, the multiplexer bar will appear at the top.
        uint8_t hover;  // If enabled, the multiplexer bar will hover over top/bottom row.
        uint8_t align;  // SFTE_MUX_BAR_ALIGN_LEFT/CENTER/RIGHT/FILL

        /* Defines the string format for each multiplexer bar window text.
           Use `%u` and `%s` (in any order) to render, in order: index and name of the window.
           Both can be omitted.
           So e.g., "[ %s : %u ]" where name is 'nvim' and idx is 3 generates this window name:
           "[ nvim : 3 ]"
           If one of the values is omitted, e.g., format is set to "| %s |", it becomes:
           "| nvim |" */
        const char *format;

        /* Character used to draw the left and right edge of each window on the bar.
           If set to 0, no character is drawn. */
        uint32_t edge_left;
        uint32_t edge_right;

        uint8_t pad;     // Horizontal padding (in cells) between each tab on the multiplexer bar.
        uint8_t margin;  // Horizontal margin (in cells) on each side of the multiplexer tab.
        uint32_t bg_active;      // ARGB 0xAARRGGBB
        uint32_t bg_inactive;    // ARGB 0xAARRGGBB
        uint32_t bg_hover;       // ARGB 0xAARRGGBB
        uint32_t fg_active;      // RGB  0x__RRGGBB
        uint32_t fg_inactive;    // RGB  0x__RRGGBB
        uint32_t fg_hover;       // RGB  0x__RRGGBB
        uint32_t attr_active;    // Attributes applied to active     tab, bitmask from SFTE_ATTR_*.
        uint32_t attr_inactive;  // Attributes applied to non-active tab, bitmask from SFTE_ATTR_*.
    } mux_bar;
#endif  // SFTE_MUX_BAR
#if SFTE_MUX
    struct {
        uint8_t preview_margin;  // Horizontal margin (in cells) on each side of the preview window.
        uint32_t preview_border_fg;  // RGB  0x__RRGGBB
        uint32_t preview_badge_bg;   // RGBA 0xAARRGGBB
        uint32_t preview_bg;         // RGBA 0xAARRGGBB
        uint32_t preview_hover_bg;   // RGBA 0xAARRGGBB
    } mux;
#endif  // SFTE_MUX
} sfte_config;

static const sfte_config _sfte_default_config = {
    .font =
        {
            .regular = {NULL},
#ifdef SFTE_FONT_BOLD
            .bold = {NULL},
#endif  // defined(SFTE_FONT_BOLD)
#ifdef SFTE_FONT_ITALIC
            .italic = {NULL},
#endif  // defined(SFTE_FONT_ITALIC)
#ifdef SFTE_FONT_BOLD_ITALIC
            .bold_italic = {NULL},
#endif  // defined(SFTE_FONT_BOLD_ITALIC)
            .default_size = 12.0f,
            .min_size = 1.0f,
            .max_size = 96.0f,
            .scales = {1.0f, 1.0f, 1.0f, 1.0f},
        },
    .color =
        {
            .bg = 0xFF000000,
            .fg = 0xFFFFFFFF,
            .cursor = 0xFFFFFFFF,
            .ansi_palette = {0xFF181818, 0xFFCC241D, 0xFF98971A, 0xFFD79921, 0xFF458588, 0xFFB16286,
                             0xFF689D6A, 0xFFA89984, 0xFF928374, 0xFFFB4934, 0xFFB8BB26, 0xFFFABD2F,
                             0xFF83A598, 0xFFD3869B, 0xFF8EC07C, 0xFFEBDBB2},
        },
    .window =
        {
            .pad_x = 8,
            .pad_y = 8,
            .init_cols = 80,
            .init_rows = 24,
            .refresh_rate = 60,
            .tab_width = 8,
            .default_title = "sh",
            .term_env = "xterm-256color",
#if SFTE_TERM_ANIMATE_SCREEN
            .anim_dur_ms = 250.0f,
#endif  // SFTE_TERM_ANIMATE_SCREEN
        },
    .cursor =
        {
            .style = SFTE_CURSOR_STYLE_BLOCK,
            .blink_rate_ms = 500,
            .thick_ratio = 0.1f,
#if SFTE_CURSOR_TRAIL
            .trail_decay = 0.01f,
            .trail_threshold = 10,
#endif  // SFTE_CURSOR_TRAIL
        },
    .underline =
        {
            .thick_ratio = 0.1f,
            .off_ratio = 0.12f,
        },
    .scroll =
        {
            .scrollback_cap = 2000,
            .step = 3,
#if SFTE_TERM_SCROLL_SMOOTH
            .decay = 0.05f,
#endif  // SFTE_TERM_SCROLL_SMOOTH
            .threshold = 10.0,
            .on_output = 0,
        },
    .shortcuts =
        {
            .custom = NULL,
            .override_defaults = 0,
        },
#if SFTE_SEARCH
    .search =
        {
            .prefix = " search: ",
            .top = 0,
            .bg_bar = 0x7F000000,
            .bg_active = 0xFFFFFFFF,
            .fg_active = 0xFF000000,
            .attr_active = SFTE_ATTR_BOLD,
            .bg_match = 0xFF7F7F7F,
            .fg_match = 0xFF000000,
            .attr_match = SFTE_ATTR_NONE,
        },
#endif  // SFTE_SEARCH
#if SFTE_MUX_BAR
    .mux_bar =
        {
            .top = 0,
            .hover = 0,
            .align = SFTE_MUX_BAR_ALIGN_LEFT,
            .format = "%u: %s",
            .edge_left = 0,
            .edge_right = 0,
            .pad = 1,
            .margin = 1,
            .bg_active = 0xFF444444,
            .bg_inactive = 0xFF000000,
            .bg_hover = 0xFF222222,
            .fg_active = 0xFFFFFFFF,
            .fg_inactive = 0xFF888888,
            .fg_hover = 0xFFAAAAAA,
            .attr_active = SFTE_ATTR_NONE,
            .attr_inactive = SFTE_ATTR_NONE,
        },
#endif  // SFTE_MUX_BAR
#if SFTE_MUX
    .mux =
        {
            .preview_margin = 6,
            .preview_border_fg = 0xFFFFFFFF,
            .preview_badge_bg = 0xFF555555,
            .preview_bg = 0x44000000,
            .preview_hover_bg = 0xFF222222,
        },
#endif  // SFTE_MUX
};
