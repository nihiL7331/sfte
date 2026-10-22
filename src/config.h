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

// Full declaration of these values are placed in the implementation layer as an enum.
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
    The $TERM environment variable exposed to the shell.
*/
#ifndef SFTE_TERM_ENV
#define SFTE_TERM_ENV "xterm-256color"
#endif  // SFTE_TERM_ENV

/*
    Default terminal name assigned on initialization.
*/
#ifndef SFTE_TERM_DEFAULT_TITLE
#define SFTE_TERM_DEFAULT_TITLE "sh"
#endif  // SFTE_TERM_DEFAULT_TITLE

/*
    Enables focus tracking (DECSET 1004) and hollow cursor rendering.
*/
#ifndef SFTE_TERM_FOCUS
#define SFTE_TERM_FOCUS 1
#endif  // SFTE_TERM_FOCUS
_SFTE_ENSURE_RANGE(SFTE_TERM_FOCUS, 0, 1);

/*
    Initial terminal grid column size.
*/
#ifndef SFTE_TERM_INIT_COLS
#define SFTE_TERM_INIT_COLS 80
#endif  // SFTE_TERM_INIT_COLS
_SFTE_ENSURE_RANGE(SFTE_TERM_INIT_COLS, 1, INT16_MAX);

/*
    Initial terminal grid row size.
*/
#ifndef SFTE_TERM_INIT_ROWS
#define SFTE_TERM_INIT_ROWS 24
#endif  // SFTE_TERM_INIT_ROWS
_SFTE_ENSURE_RANGE(SFTE_TERM_INIT_ROWS, 1, INT16_MAX);

/*
    Number of bytes read from the PTY per poll event.
*/
#ifndef SFTE_TERM_PTY_BUF_SIZE
#define SFTE_TERM_PTY_BUF_SIZE UINT16_MAX
#endif  // SFTE_TERM_PTY_BUF_SIZE
_SFTE_ENSURE_RANGE(SFTE_TERM_PTY_BUF_SIZE, 1, UINT16_MAX);

/*
    Tab stop interval in grid cells.
*/
#ifndef SFTE_TERM_TAB_WIDTH
#define SFTE_TERM_TAB_WIDTH 8
#endif  // SFTE_TERM_TAB_WIDTH
_SFTE_ENSURE_RANGE(SFTE_TERM_TAB_WIDTH, 1, INT16_MAX);

/*
    Enables procedural rendering for box drawing characters (U+2500 - U+257F).
    Guarantees seamless, gapless lines for TUIs.
*/
#ifndef SFTE_TERM_CUSTOM_BOXES
#define SFTE_TERM_CUSTOM_BOXES 1
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
    Defines frame rate during rendering (in frames per second).
    Used for screen animations, smooth scrolling, cursor trail.
*/
#ifndef SFTE_TERM_REFRESH_RATE
#define SFTE_TERM_REFRESH_RATE 60
#endif  // SFTE_TERM_REFRESH_RATE
_SFTE_ENSURE_RANGE(SFTE_TERM_REFRESH_RATE, 1, 720);

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
    Duration of the scrolling animation on TUI open/close (alt screen toggle).
*/
#ifndef SFTE_TERM_ANIM_DUR_MS
#define SFTE_TERM_ANIM_DUR_MS 250.0f
#endif  // SFTE_TERM_ANIM_DUR_MS
_SFTE_ENSURE_RANGE(SFTE_TERM_ANIM_DUR_MS, 0.0f, FLT_MAX);

/*
    Enables text reflow when resizing the terminal window.
    With SFTE_TERM_REFLOW disabled, any text going off the right edge is deleted immediately.
*/
#ifndef SFTE_TERM_REFLOW
#define SFTE_TERM_REFLOW 1
#endif  // SFTE_TERM_REFLOW
_SFTE_ENSURE_RANGE(SFTE_TERM_REFLOW, 0, 1);

/*
    Maximum lines of scrollback history kept in memory.
    Increasing it WILL affect the memory footprint size.
    Setting it to 0 disables scrollback logic entirely.
*/
#ifndef SFTE_TERM_SCROLLBACK_CAP
#define SFTE_TERM_SCROLLBACK_CAP 2000
#endif  // SFTE_TERM_SCROLLBACK_CAP
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLLBACK_CAP, 0, INT32_MAX);

/*
    Determines if `clear` commands (CSI 3 J) actually wipe the scrollback buffer.
    By default, the scrollback DOES get wiped on `clear` call.
*/
#ifndef SFTE_TERM_SCROLLBACK_CLEAR
#define SFTE_TERM_SCROLLBACK_CLEAR 1
#endif  // SFTE_TERM_SCROLLBACK_CLEAR
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLLBACK_CLEAR, 0, 1);

/*
    Number of lines to shift per mouse wheel / trackpad scroll tick.
    NOTE:
    This doesn't affect the Shift+PgUp/Dn scrolling speed.
    It can be changed in the default shortcuts, lower in the file.
*/
#ifndef SFTE_TERM_SCROLL_STEP
#define SFTE_TERM_SCROLL_STEP 3
#endif  // SFTE_TERM_SCROLL_STEP
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLL_STEP, 0, INT32_MAX);

/*
    Enables smooth text scrolling.
    Essentially enables linear interpolation to closest row, replacing instant snapping.
    Because it requires polling while scrolling, it will affect CPU usage while scrolling.
*/
#ifndef SFTE_TERM_SCROLL_SMOOTH
#define SFTE_TERM_SCROLL_SMOOTH 1
#endif  // SFTE_TERM_SCROLL_SMOOTH
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLL_SMOOTH, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_TERM_SCROLL_SMOOTH, SFTE_TERM_SCROLLBACK_CAP);

/*
    Affects how fast the scroll smooths to its target position.
*/
#ifndef SFTE_TERM_SCROLL_DECAY
#define SFTE_TERM_SCROLL_DECAY 0.05f
#endif  // SFTE_TERM_SCROLL_DECAY
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLL_DECAY, 0.0f, FLT_MAX);

/*
    Narrows the allowed characters range to ASCII, lowering the memory usage.
    Enabling this allows the terminal to store each rune as a `uint8_t` rather than a `uint32_t`.
*/
#ifndef SFTE_TERM_ASCII_CHARSET
#define SFTE_TERM_ASCII_CHARSET 0
#endif  // SFTE_TERM_ASCII_CHARSET
_SFTE_ENSURE_RANGE(SFTE_TERM_ASCII_CHARSET, 0, 1);

// =================================================================================================
// >>window macros
// =================================================================================================

/*
    Horizontal padding around the terminal grid in pixels.
*/
#ifndef SFTE_WINDOW_PAD_X
#define SFTE_WINDOW_PAD_X 8
#endif  // SFTE_WINDOW_PAD_X
_SFTE_ENSURE_RANGE(SFTE_WINDOW_PAD_X, 0, INT32_MAX);

/*
    Vertical padding around the terminal grid in pixels.
*/
#ifndef SFTE_WINDOW_PAD_Y
#define SFTE_WINDOW_PAD_Y 8
#endif  // SFTE_WINDOW_PAD_Y
_SFTE_ENSURE_RANGE(SFTE_WINDOW_PAD_Y, 0, INT32_MAX);

// =================================================================================================
// >>color macros
// =================================================================================================

/*
    Enables parsing of 24-bit TrueColor sequences (CSI 38;2;R;G;B m).
*/
#ifndef SFTE_COLOR_TRUECOLOR
#define SFTE_COLOR_TRUECOLOR 1
#endif  // SFTE_COLOR_TRUECOLOR
_SFTE_ENSURE_RANGE(SFTE_COLOR_TRUECOLOR, 0, 1);

/*
    Default background color (RGB888).
*/
#ifndef SFTE_COLOR_BG
#define SFTE_COLOR_BG 0x000000
#endif  // SFTE_COLOR_BG
_SFTE_ENSURE_RANGE(SFTE_COLOR_BG, 0x000000, 0xFFFFFF);

/*
    Default text foreground color (text and underlines, if applicable).
    Format RGB888.
*/
#ifndef SFTE_COLOR_FG
#define SFTE_COLOR_FG 0xFFFFFF
#endif  // SFTE_COLOR_FG
_SFTE_ENSURE_RANGE(SFTE_COLOR_FG, 0x000000, 0xFFFFFF);

/*
    Background opacity (0x00 transparent to 0xFF opaque).
*/
#ifndef SFTE_COLOR_BG_OPACITY
#define SFTE_COLOR_BG_OPACITY 0xFF
#endif  // SFTE_COLOR_BG_OPACITY
_SFTE_ENSURE_RANGE(SFTE_COLOR_BG_OPACITY, 0x00, 0xFF);

/*
    16-color ANSI fallback palette.
    Colors are, in order: black, red, green, yellow, blue, magenta, cyan, white.
    Indices 0 to 7 are regular colors.
    Indices 8 to 15 are bright colors.

    WARN:
    Do NOT wrap the colors in curly braces.
    These values act as the 0-15 index colors in a 256 color palette.
*/
#ifndef SFTE_COLOR_ANSI_PALETTE
#define SFTE_COLOR_ANSI_PALETTE                                                                    \
    0x181818, 0xCC241D, 0x98971A, 0xD79921, 0x458588, 0xB16286, 0x689D6A, 0xA89984, /* regular */  \
        0x928374, 0xFB4934, 0xB8BB26, 0xFABD2F, 0x83A598, 0xD3869B, 0x8EC07C,                      \
        0xEBDBB2 /* bright  */
#endif           // SFTE_COLOR_ANSI_PALETTE

/*
    A macro helper used to draw pixels.
    This can be overriden to support different color formats,
    e.g.: monochrome, RGB233, RGB565.
*/
#ifndef SFTE_COLOR_DRAW_PIXEL
#define SFTE_COLOR_DRAW_PIXEL(buf, x, y, stride, height, color)                                    \
    if ((x) >= 0 && (x) < (stride) && (y) >= 0 && (y) < (height))                                  \
        ((uint32_t *)(buf))[(y) * (stride) + (x)] = (color);
#endif  // SFTE_COLOR_DRAW_PIXEL

/*
    A macro helper used to blend pixel colors based on their alpha value.
    This can be overriden to support different color formats,
    e.g.: monochrome, RGB233, RGB565.
    Needed for cursor trail and images support.
*/
#ifndef SFTE_COLOR_BLEND_PIXEL
#define SFTE_COLOR_BLEND_PIXEL(buf, x, y, stride, rgb_color, alpha)                                \
    (((uint32_t *)(buf))[(y) * (stride) + (x)] = _sfte_render_blend_argb(                          \
         ((uint32_t *)(buf))[(y) * (stride) + (x)], (rgb_color), (alpha)))
#endif  // SFTE_COLOR_BLEND_PIXEL

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
    Minimum size of the font in pixels.
*/
#ifndef SFTE_FONT_MIN_SIZE
#define SFTE_FONT_MIN_SIZE 1.0f
#endif  // SFTE_FONT_MIN_SIZE
_SFTE_ENSURE_RANGE(SFTE_FONT_MIN_SIZE, 0.1f, FLT_MAX);

/*
    Maximum size of the font in pixels.
*/
#ifndef SFTE_FONT_MAX_SIZE
#define SFTE_FONT_MAX_SIZE 96.0f
#endif  // SFTE_FONT_MAX_SIZE
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_SIZE, 0.1f, FLT_MAX);

/*
    Starting font size in pixels.
*/
#ifndef SFTE_FONT_DEFAULT_SIZE
#define SFTE_FONT_DEFAULT_SIZE 12.0f
#endif  // SFTE_FONT_DEFAULT_SIZE
_SFTE_ENSURE_RANGE(SFTE_FONT_DEFAULT_SIZE, 0.1f, FLT_MAX);

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
    Tweaks scaling per-font to match baseline heights.
    Useful for nerd symbol fonts, where often symbols are too big.
    With primary font in slot 0 and nerd font in slot 1, something like this can be used:
    #define SFTE_FONT_SCALES {1.0f, 0.8f}
*/
#ifndef SFTE_FONT_SCALES
#define SFTE_FONT_SCALES {1.0f, 1.0f, 1.0f, 1.0f}
#endif  // SFTE_FONT_SCALES

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
#define SFTE_FONT_ATLAS_SIZE 1024
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
    Sets the default terminal cursor style.
    Available options:
    SFTE_CURSOR_STYLE_BLOCK (default)
    SFTE_CURSOR_STYLE_UNDERLINE
    SFTE_CURSOR_STYLE_BAR

    NOTE:
    This value still can be overwritten by certain applications in runtime,
    #define SFTE_CURSOR_DYNAMIC 0
    can be used to keep the cursor at one, default style.
*/
#ifndef SFTE_CURSOR_STYLE
#define SFTE_CURSOR_STYLE SFTE_CURSOR_STYLE_BLOCK
#endif  // SFTE_CURSOR_STYLE
_SFTE_ENSURE_RANGE(SFTE_CURSOR_STYLE, SFTE_CURSOR_STYLE_BLOCK, SFTE_CURSOR_STYLE_BAR);

/*
    Allows programs to dynamically change the cursor shape via escape sequences.
    If set to 0, cursor style stays as SFTE_CURSOR_STYLE.
*/
#ifndef SFTE_CURSOR_DYNAMIC
#define SFTE_CURSOR_DYNAMIC 1
#endif  // SFTE_CURSOR_DYNAMIC
_SFTE_ENSURE_RANGE(SFTE_CURSOR_DYNAMIC, 0, 1);

/*
    Default cursor color (RGB888).
*/
#ifndef SFTE_CURSOR_COLOR
#define SFTE_CURSOR_COLOR 0xFFFFFF
#endif  // SFTE_CURSOR_COLOR
_SFTE_ENSURE_RANGE(SFTE_CURSOR_COLOR, 0x000000, 0xFFFFFF);

/*
    Determines the width of bar cursors and height of underline cursors relative to font size.
*/
#ifndef SFTE_CURSOR_THICK_RATIO
#define SFTE_CURSOR_THICK_RATIO 0.1f
#endif  // SFTE_CURSOR_THICK_RATIO
_SFTE_ENSURE_RANGE(SFTE_CURSOR_THICK_RATIO, 0.0f, 1.0f);

/*
    Enables cursor blinking.
    This technically affects CPU usage, since it requires polling on every state change.
*/
#ifndef SFTE_CURSOR_BLINK
#define SFTE_CURSOR_BLINK 1
#endif  // SFTE_CURSOR_BLINK
_SFTE_ENSURE_RANGE(SFTE_CURSOR_BLINK, 0, 1);

#ifndef SFTE_CURSOR_BLINK_RATE_MS
#define SFTE_CURSOR_BLINK_RATE_MS 500
#endif  // SFTE_CURSOR_BLINK_RATE_MS
_SFTE_ENSURE_RANGE(SFTE_CURSOR_BLINK_RATE_MS, 0, UINT32_MAX);

/*
    Renders an animated smooth-scrolling ghost trail behind the cursor,
    similar to cursor_trail from kitty or default (I believe?) neovide cursor trail.
    INFO:
    This is NOT a toggle, it defines time (in ms) of movement required for trail to start rendering.
    If it's set to 0, the value is disabled.
    Extremely low values may cause flickers when programs render themselves.
    10 seems like a sensible default for trail enabled.

    This affects CPU usage, since it requires polling every 16ms when the trail is visible.
    It requires double buffering, otherwise resulting in terrible rendering artifacts.
*/
#ifndef SFTE_CURSOR_TRAIL
#define SFTE_CURSOR_TRAIL 0
#endif  // SFTE_CURSOR_TRAIL
_SFTE_ENSURE_RANGE(SFTE_CURSOR_TRAIL, 0, 1024);
_SFTE_ENSURE_DEPS(SFTE_CURSOR_TRAIL, SFTE_TERM_DOUBLE_BUFFER);

/*
    Affects how fast the trail disappears.
*/
#ifndef SFTE_CURSOR_TRAIL_DECAY
#define SFTE_CURSOR_TRAIL_DECAY 0.01f
#endif  // SFTE_CURSOR_TRAIL_DECAY
_SFTE_ENSURE_RANGE(SFTE_CURSOR_TRAIL_DECAY, 0.01f, FLT_MAX);

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
    Default underline color is SFTE_COLOR_FG.
*/
#ifndef SFTE_UNDERLINE_COLORED
#define SFTE_UNDERLINE_COLORED 1
#endif  // SFTE_UNDERLINE_COLORED
_SFTE_ENSURE_RANGE(SFTE_UNDERLINE_COLORED, 0, 1);

/*
    Line thickness relative to font cell height.
*/
#ifndef SFTE_UNDERLINE_THICK_RATIO
#define SFTE_UNDERLINE_THICK_RATIO 0.1f
#endif  // SFTE_UNDERLINE_THICK_RATIO
_SFTE_ENSURE_RANGE(SFTE_UNDERLINE_THICK_RATIO, 0.0f, 1.0f);

/*
    Gap between text baseline and the underline relative to cell height.
*/
#ifndef SFTE_UNDERLINE_OFFSET_RATIO
#define SFTE_UNDERLINE_OFFSET_RATIO 0.12f
#endif  // SFTE_UNDERLINE_OFFSET_RATIO
_SFTE_ENSURE_RANGE(SFTE_UNDERLINE_OFFSET_RATIO, -1.0f, 1.0f);

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
_SFTE_ENSURE_DEPS(SFTE_SEARCH, SFTE_TERM_SCROLLBACK_CAP);

/*
    Prefix of the search bar.
    Mainly a cosmetic setting, although at extra-small terminal sizes
    making it shorter will allow to see more of the query on screen at once.
*/
#ifndef SFTE_SEARCH_PREFIX
#define SFTE_SEARCH_PREFIX " search: "
#endif  // SFTE_SEARCH_PREFIX

/*
    If enabled, places the search bar at the top.
*/
#ifndef SFTE_SEARCH_PLACEMENT_TOP
#define SFTE_SEARCH_PLACEMENT_TOP 0
#endif  // SFTE_SEARCH_PLACEMENT_TOP

/*
    Background color of the search query bar.
*/
#ifndef SFTE_SEARCH_BG_BAR
#define SFTE_SEARCH_BG_BAR 0x000000
#endif  // SFTE_SEARCH_BG
_SFTE_ENSURE_RANGE(SFTE_SEARCH_BG_BAR, 0x000000, 0xFFFFFF);

/*
    Background color opacity of the search query bar.
*/
#ifndef SFTE_SEARCH_BG_BAR_OPACITY
#define SFTE_SEARCH_BG_BAR_OPACITY 0x7F
#endif  // SFTE_SEARCH_BG_BAR_OPACITY
_SFTE_ENSURE_RANGE(SFTE_SEARCH_BG_BAR_OPACITY, 0x00, 0xFF);

/*
    Background color of a highlighted active search result.
*/
#ifndef SFTE_SEARCH_BG_ACTIVE
#define SFTE_SEARCH_BG_ACTIVE SFTE_COLOR_FG
#endif  // SFTE_SEARCH_BG_ACTIVE
_SFTE_ENSURE_RANGE(SFTE_SEARCH_BG_ACTIVE, 0x000000, 0xFFFFFF);

/*
    Background color of a highlighted inactive search result.
*/
#ifndef SFTE_SEARCH_BG_MATCH
#define SFTE_SEARCH_BG_MATCH 0x7F7F7F
#endif  // SFTE_SEARCH_BG_MATCH
_SFTE_ENSURE_RANGE(SFTE_SEARCH_BG_MATCH, 0x000000, 0xFFFFFF);

/*
    Foreground color of a highlighted active search result.
*/
#ifndef SFTE_SEARCH_FG_ACTIVE
#define SFTE_SEARCH_FG_ACTIVE SFTE_COLOR_BG
#endif  // SFTE_SEARCH_FG_ACTIVE
_SFTE_ENSURE_RANGE(SFTE_SEARCH_FG_ACTIVE, 0x000000, 0xFFFFFF);

/*
    Foreground color of a highlighted inactive search result.
*/
#ifndef SFTE_SEARCH_FG_MATCH
#define SFTE_SEARCH_FG_MATCH SFTE_COLOR_BG
#endif  // SFTE_SEARCH_FG_MATCH
_SFTE_ENSURE_RANGE(SFTE_SEARCH_FG_MATCH, 0x000000, 0xFFFFFF);

/*
    Attributes applied to a highlighted active search result.

    Available options:

    SFTE_ATTR_NONE
    SFTE_ATTR_BOLD
    SFTE_ATTR_ITALIC
    SFTE_ATTR_UNDERLINE
    SFTE_ATTR_REVERSE

    To combine multiple attributes, use the '|' (binary OR) operator.
*/
#ifndef SFTE_SEARCH_ATTR_ACTIVE
#define SFTE_SEARCH_ATTR_ACTIVE SFTE_ATTR_BOLD
#endif  // SFTE_SEARCH_ATTR_ACTIVE
_SFTE_ENSURE_RANGE(SFTE_SEARCH_ATTR_ACTIVE, SFTE_ATTR_NONE,
                   SFTE_ATTR_BOLD | SFTE_ATTR_ITALIC | SFTE_ATTR_UNDERLINE | SFTE_ATTR_REVERSE);

/*
    Attributes applied to a highlighted inactive search result.

    Available options:

    SFTE_ATTR_NONE
    SFTE_ATTR_BOLD
    SFTE_ATTR_ITALIC
    SFTE_ATTR_UNDERLINE
    SFTE_ATTR_REVERSE

    To combine multiple attributes, use the '|' (binary OR) operator.
*/
#ifndef SFTE_SEARCH_ATTR_MATCH
#define SFTE_SEARCH_ATTR_MATCH SFTE_ATTR_NONE
#endif  // SFTE_SEARCH_ATTR_MATCH
_SFTE_ENSURE_RANGE(SFTE_SEARCH_ATTR_MATCH, SFTE_ATTR_NONE,
                   SFTE_ATTR_BOLD | SFTE_ATTR_ITALIC | SFTE_ATTR_UNDERLINE | SFTE_ATTR_REVERSE);

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
#define SFTE_MUX 0
#endif  // SFTE_MUX
_SFTE_ENSURE_RANGE(SFTE_MUX, 0, 1);

/*
    Enables the multiplexer bar, known from tmux.
    Visuals of the bar can be customized via options below.
*/
#ifndef SFTE_MUX_BAR
#define SFTE_MUX_BAR 0
#endif  // SFTE_MUX_BAR
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_MUX_BAR, SFTE_MUX);

/*
    If enabled, the multiplexer bar hovers above the grid viewport.
    This, with SFTE_MUX_BAR_ALIGN set to SFTE_MUX_BAR_ALIGN_RIGHT
    and opacity set low, can achieve a nice, non-disruptive look
    that maximizes the vertical space.
*/
#ifndef SFTE_MUX_BAR_HOVER
#define SFTE_MUX_BAR_HOVER 0
#endif  // SFTE_MUX_BAR_HOVER
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_HOVER, 0, 1);

/*
    If enabled, the multiplexer bar is placed at the top of the window.
*/
#ifndef SFTE_MUX_BAR_PLACEMENT_TOP
#define SFTE_MUX_BAR_PLACEMENT_TOP 0
#endif  // SFTE_MUX_BAR_PLACEMENT_TOP
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_PLACEMENT_TOP, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_MUX_BAR_PLACEMENT_TOP, SFTE_MUX_BAR);

/*
    Defines the string format for each multiplexer bar window text.
    The implementation uses a custom parser to generate the the window text.
    Use `%u` and `%s` (in any order) to render, in order: index and name of the window.
    Both can be omitted.

    So, for example, "[ %s : %u ]" where name is 'nvim' and index is 3 generates this window name:

    "[ nvim : 3 ]"

    If one of the values is omitted, e.g., format is set to "| %s |", it becomes:

    "| nvim |"
*/
#ifndef SFTE_MUX_BAR_FORMAT
#define SFTE_MUX_BAR_FORMAT "%u: %s"
#endif  // SFTE_MUX_BAR_FORMAT

/*
    Defines the character used to draw the left and right edge of each window on the bar.
    If set to 0, no character is drawn.

    WARN:
    Using characters outside of ASCII range with SFTE_TERM_ASCII_CHARSET
    defined will result in '?' rendered instead.
*/
#ifndef SFTE_MUX_BAR_EDGE_LEFT
#define SFTE_MUX_BAR_EDGE_LEFT 0
#endif  // SFTE_MUX_BAR_EDGE_LEFT
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_EDGE_LEFT, 0, INT32_MAX);

#ifndef SFTE_MUX_BAR_EDGE_RIGHT
#define SFTE_MUX_BAR_EDGE_RIGHT 0
#endif  // SFTE_MUX_BAR_EDGE_RIGHT
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_EDGE_RIGHT, 0, INT32_MAX);

/*
    Background color for active multiplexer window on the bar (RGB8888).
*/
#ifndef SFTE_MUX_BAR_BG_ACTIVE
#define SFTE_MUX_BAR_BG_ACTIVE 0xFF444444
#endif  // SFTE_MUX_BAR_BG_ACTIVE
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_BG_ACTIVE, 0x00000000, 0xFFFFFFFF);

/*
    Background color for inactive multiplexer window on the bar (RGB8888).
*/
#ifndef SFTE_MUX_BAR_BG_INACTIVE
#define SFTE_MUX_BAR_BG_INACTIVE 0xFF000000
#endif  // SFTE_MUX_BAR_BG_INACTIVE
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_BG_INACTIVE, 0x00000000, 0xFFFFFFFF);

/*
    Background color for hovered multiplexer window on the bar (RGB8888).
*/
#ifndef SFTE_MUX_BAR_BG_HOVER
#define SFTE_MUX_BAR_BG_HOVER 0xFF222222
#endif  // SFTE_MUX_BAR_BG_HOVER
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_BG_HOVER, 0x00000000, 0xFFFFFFFF);

/*
    Foreground color for active multiplexer window on the bar (RGB888).
*/
#ifndef SFTE_MUX_BAR_FG_ACTIVE
#define SFTE_MUX_BAR_FG_ACTIVE 0xFFFFFF
#endif  // SFTE_MUX_BAR_FG_ACTIVE
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_FG_ACTIVE, 0x000000, 0xFFFFFF);

/*
    Foreground color for active multiplexer window on the bar (RGB888).
*/
#ifndef SFTE_MUX_BAR_FG_INACTIVE
#define SFTE_MUX_BAR_FG_INACTIVE 0x888888
#endif  // SFTE_MUX_BAR_FG_INACTIVE
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_FG_INACTIVE, 0x000000, 0xFFFFFF);

/*
    Foreground color for hovered multiplexer window on the bar (RGB888).
*/
#ifndef SFTE_MUX_BAR_FG_HOVER
#define SFTE_MUX_BAR_FG_HOVER 0xAAAAAA
#endif  // SFTE_MUX_BAR_FG_HOVER
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_FG_HOVER, 0x000000, 0xFFFFFF);

/*
    Attributes applied to an active multiplexer window text on the bar.

    Available options:

    SFTE_ATTR_NONE
    SFTE_ATTR_BOLD
    SFTE_ATTR_ITALIC
    SFTE_ATTR_UNDERLINE
    SFTE_ATTR_REVERSE

    To combine multiple attributes, use the '|' (binary OR) operator.
*/
#ifndef SFTE_MUX_BAR_ATTR_ACTIVE
#define SFTE_MUX_BAR_ATTR_ACTIVE SFTE_ATTR_NONE
#endif  // SFTE_MUX_BAR_ATTR_ACTIVE
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_ATTR_ACTIVE, SFTE_ATTR_NONE,
                   SFTE_ATTR_BOLD | SFTE_ATTR_ITALIC | SFTE_ATTR_UNDERLINE | SFTE_ATTR_REVERSE);

/*
    Attributes applied to an inactive multiplexer window text on the bar.

    Available options:

    SFTE_ATTR_NONE
    SFTE_ATTR_BOLD
    SFTE_ATTR_ITALIC
    SFTE_ATTR_UNDERLINE
    SFTE_ATTR_REVERSE

    To combine multiple attributes, use the '|' (binary OR) operator.
*/
#ifndef SFTE_MUX_BAR_ATTR_INACTIVE
#define SFTE_MUX_BAR_ATTR_INACTIVE SFTE_ATTR_NONE
#endif  // SFTE_MUX_BAR_ATTR_INACTIVE
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_ATTR_INACTIVE, SFTE_ATTR_NONE,
                   SFTE_ATTR_BOLD | SFTE_ATTR_ITALIC | SFTE_ATTR_UNDERLINE | SFTE_ATTR_REVERSE);

#define SFTE_MUX_BAR_ALIGN_LEFT 0
#define SFTE_MUX_BAR_ALIGN_CENTER 1
#define SFTE_MUX_BAR_ALIGN_RIGHT 2
#define SFTE_MUX_BAR_ALIGN_FILL 3

/*
    Alignment of the multiplexer bar tabs.
    Available options:
    SFTE_MUX_BAR_ALIGN_LEFT (default)
    SFTE_MUX_BAR_ALIGN_CENTER
    SFTE_MUX_BAR_ALIGN_RIGHT
    SFTE_MUX_BAR_ALIGN_FILL
*/
#ifndef SFTE_MUX_BAR_ALIGN
#define SFTE_MUX_BAR_ALIGN SFTE_MUX_BAR_ALIGN_LEFT
#endif  // SFTE_MUX_BAR_ALIGN
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_ALIGN, SFTE_MUX_BAR_ALIGN_LEFT, SFTE_MUX_BAR_ALIGN_FILL);

/*
    Defines the horizontal padding (in cells) between each tab on the multiplexer bar.
*/
#ifndef SFTE_MUX_BAR_PAD
#define SFTE_MUX_BAR_PAD 1
#endif  // SFTE_MUX_BAR_PAD
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_PAD, 0, 16);

/*
    Defines the horizontal margin (in cells) on the left and right side of the multiplexer tab.
    Does not affect the logic if `SFTE_MUX_BAR_ALIGN == SFTE_MUX_BAR_ALIGN_FILL`.
*/
#ifndef SFTE_MUX_BAR_MARGIN
#define SFTE_MUX_BAR_MARGIN 1
#endif  // SFTE_MUX_BAR_MARGIN
_SFTE_ENSURE_RANGE(SFTE_MUX_BAR_MARGIN, 0, 1);

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
    Defines the horizontal margin (in cells)
    on the left and right side of the prefix preview window.
*/
#ifndef SFTE_MUX_PREVIEW_MARGIN
#define SFTE_MUX_PREVIEW_MARGIN 6
#endif  // SFTE_MUX_PREVIEW_MARGIN

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
    Foreground color of the borders for the prefix preview. (RGB888)
*/
#ifndef SFTE_MUX_PREVIEW_BORDER_FG
#define SFTE_MUX_PREVIEW_BORDER_FG 0xFFFFFF
#endif  // SFTE_MUX_PREVIEW_BORDER_FG
_SFTE_ENSURE_RANGE(SFTE_MUX_PREVIEW_BORDER_FG, 0x000000, 0xFFFFFF);

/*
    Background color for the prefix preview window badge/index (RGB8888).
*/
#ifndef SFTE_MUX_PREVIEW_BADGE_BG
#define SFTE_MUX_PREVIEW_BADGE_BG 0xFF555555
#endif  // SFTE_MUX_PREVIEW_BADGE_BG
_SFTE_ENSURE_RANGE(SFTE_MUX_PREVIEW_BADGE_BG, 0x00000000, 0xFFFFFFFF);

/*
    Background color BEHIND the prefix preview window (RGB8888).
*/
#ifndef SFTE_MUX_PREVIEW_BG
#define SFTE_MUX_PREVIEW_BG 0x44000000
#endif  // SFTE_MUX_PREVIEW_BG
_SFTE_ENSURE_RANGE(SFTE_MUX_PREVIEW_BG, 0x00000000, 0xFFFFFFFF);

/*
    Background color of the hovered prefix preview window (RGB888).
*/
#ifndef SFTE_MUX_PREVIEW_HOVER_BG
#define SFTE_MUX_PREVIEW_HOVER_BG 0x222222
#endif  // SFTE_MUX_PREVIEW_HOVER_BG
_SFTE_ENSURE_RANGE(SFTE_MUX_PREVIEW_HOVER_BG, 0x000000, 0xFFFFFF);

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
#if SFTE_TERM_SCROLLBACK_CAP
static inline uint8_t _sfte_wayland_view_scroll(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_TERM_SCROLLBACK_CAP
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

#ifndef SFTE_SHORTCUTS
#define SFTE_SHORTCUTS {SFTE_BASE_SHORTCUTS}
#endif  // SFTE_SHORTCUTS
