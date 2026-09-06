/*
    sfte -- single-file terminal emulator

    Project URL: https://github.com/nihiL7331/sfte

    FIXME: docs

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

#ifndef SFTE_CUSTOM_FONT_BACKEND
#include "stb_truetype.h"
typedef stbtt_fontinfo sfte_font_backend_info;
#else
typedef struct sfte_font_backend_info sfte_font_backend_info;
#endif  // SFTE_CUSTOM_FONT_BACKEND

// default value for SFTE_KITTY_GRAPHICS is 1, so if it's undefined its 1
#if !defined(SFTE_KITTY_GRAPHICS) || SFTE_KITTY_GRAPHICS
#include "stb_image.h"
#endif  // !defined(SFTE_KITTY_GRAPHICS) || SFTE_KITTY_GRAPHICS

#include <stddef.h>  // size_t
#include <stdint.h>

#ifndef SFTE_CUSTOM_BACKEND
#ifndef SFTE_WAYLAND
#define SFTE_WAYLAND 1
#endif  // SFTE_WAYLAND
#else   // SFTE_CUSTOM_BACKEND
#define SFTE_WAYLAND 0
#endif  // SFTE_CUSTOM_BACKEND

typedef enum {
    _SFTE_LOG_LVL_PANIC = 0,
    _SFTE_LOG_LVL_ERROR = 1,
    _SFTE_LOG_LVL_WARN = 2,
    _SFTE_LOG_LVL_INFO = 3,
} _sfte_log_level;

// #################################################################################################
// >>>CONFIGURATION
// #################################################################################################

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
#endif  // SFTE_ASSERT

#ifndef SFTE_LOG_LEVEL
#define SFTE_LOG_LEVEL _SFTE_LOG_LVL_PANIC
#endif  // SFTE_LOG_LEVEL

#ifndef SFTE_LOG_TAG
#define SFTE_LOG_TAG "sfte"
#endif  // SFTE_LOG_TAG

#ifndef SFTE_LOG_FUNC
#define SFTE_LOG_FUNC _sfte_log_default_func
#endif  // SFTE_LOG_FUNC

#ifndef SFTE_TERM_ENV
#define SFTE_TERM_ENV "xterm-256color"
#endif  // SFTE_TERM_ENV

#ifndef SFTE_PTY_BUF_SIZE
#define SFTE_PTY_BUF_SIZE 4096
#endif  // SFTE_PTY_BUF_SIZE

#ifndef SFTE_BG_COLOR  // RGB
#define SFTE_BG_COLOR 0x000000
#endif  // SFTE_BG_COLOR

#ifndef SFTE_BG_OPACITY  // 0x00-0xFF
#define SFTE_BG_OPACITY 0xFF
#endif  // SFTE_BG_OPACITY

#ifndef SFTE_CUSTOM_FONT_BACKEND
static inline void _sfte_stb_init(sfte_font_backend_info *info, const uint8_t *data);
static inline float _sfte_stb_get_scale(sfte_font_backend_info *info, float px_hei);
static inline void _sfte_stb_vmetrics(sfte_font_backend_info *info, int *ascent, int *descent,
                                      int *linegap);
static inline int _sfte_stb_bounds(sfte_font_backend_info *info, uint32_t rune, float scale,
                                   int *adv, int *x0, int *y0, int *x1, int *y1);
static inline void _sfte_stb_bake(sfte_font_backend_info *info, int glyph_idx, float scale,
                                  uint8_t *atlas_ptr, int gw, int gh, int atlas_stride);
#else  // SFTE_CUSTOM_FONT_BACKEND
#if !defined(SFTE_FONT_INIT) || !defined(SFTE_FONT_GET_SCALE) || !defined(SFTE_FONT_VMETRICS) ||   \
    !defined(SFTE_FONT_BOUNDS) || !defined(SFTE_FONT_BAKE)
#error                                                                                             \
    "SFTE_CUSTOM_FONT_BACKEND requires defining all 5 macro hooks: INIT, GET_SCALE, VMETRICS, BOUNDS and BAKE."
#endif  // !defined(SFTE_FONT_INIT) || !defined(SFTE_FONT_GET_SCALE) || !defined(SFTE_FONT_VMETRICS)
        // || !defined(SFTE_FONT_BOUNDS) || !defined(SFTE_FONT_BAKE)
#endif  // SFTE_CUSTOM_FONT_BACKEND

#ifndef SFTE_FONT_DEFAULT_SIZE
#define SFTE_FONT_DEFAULT_SIZE 12.0f
#endif  // SFTE_FONT_DEFAULT_SIZE

#ifndef SFTE_FONT_ATLAS_SIZE
#define SFTE_FONT_ATLAS_SIZE 1024
#endif  // SFTE_FONT_ATLAS_SIZE

#ifndef SFTE_FONT_GLYPH_CAP
#define SFTE_FONT_GLYPH_CAP 4096
#endif  // SFTE_FONT_GLYPH_CAP

#ifndef SFTE_FONT_MAX_COUNT
#define SFTE_FONT_MAX_COUNT 4
#endif  // SFTE_FONT_MAX_COUNT

#ifndef SFTE_FONT_SCALES
#define SFTE_FONT_SCALES {1.0f, 1.0f, 1.0f, 1.0f}
#endif  // SFTE_FONT_SCALES

#ifndef SFTE_FONT_ZOOM
#define SFTE_FONT_ZOOM 1
#endif  // SFTE_FONT_ZOOM

#ifndef SFTE_FONT_BLEED
#define SFTE_FONT_BLEED 1
#endif  // SFTE_FONT_BLEED

#ifndef SFTE_ANSI_PALETTE
#define SFTE_ANSI_PALETTE                                                                          \
    {0x181818, 0xCC241D, 0x98971A, 0xD79921, 0x458588, 0xB16286, 0x689D6A, 0xA89984,               \
     0x928374, 0xFB4934, 0xB8BB26, 0xFABD2F, 0x83A598, 0xD3869B, 0x8EC07C, 0xEBDBB2}
#endif  // SFTE_ANSI_PALETTE

#ifndef SFTE_TRUE_COLOR
#define SFTE_TRUE_COLOR 1
#endif  // SFTE_TRUE_COLOR

#ifndef SFTE_WIDE_CHARS
#define SFTE_WIDE_CHARS 1
#endif  // SFTE_WIDE_CHARS

#ifndef SFTE_EXT_UNDERLINES
#define SFTE_EXT_UNDERLINES 1
#endif  // SFTE_EXT_UNDERLINES

#ifndef SFTE_COLOR_UNDERLINE
#define SFTE_COLOR_UNDERLINE 1
#endif  // SFTE_COLOR_UNDERLINE

#ifndef SFTE_MAX_COMBINING
#define SFTE_MAX_COMBINING 2
#endif  // SFTE_MAX_COMBINING

#ifndef SFTE_PAD_X  // in pxs
#define SFTE_PAD_X 8
#endif  // SFTE_PAD_X

#ifndef SFTE_PAD_Y  // in pxs
#define SFTE_PAD_Y 8
#endif  // SFTE_PAD_Y

#define SFTE_CURSOR_BLOCK 0
#define SFTE_CURSOR_UNDERLINE 1
#define SFTE_CURSOR_BAR 2

#ifndef SFTE_CURSOR_STYLE  // BLOCK/UNDERLINE/BAR
#define SFTE_CURSOR_STYLE SFTE_CURSOR_BLOCK
#endif  // SFTE_CURSOR_STYLE

#ifndef SFTE_CURSOR_DYNAMIC
#define SFTE_CURSOR_DYNAMIC 1
#endif  // SFTE_CURSOR_DYNAMIC

#ifndef SFTE_CURSOR_COLOR
#define SFTE_CURSOR_COLOR 0xFFFFFF
#endif  // SFTE_CURSOR_COLOR

#ifndef SFTE_CURSOR_BLINK
#define SFTE_CURSOR_BLINK 1
#endif  // SFTE_CURSOR_BLINK

#ifndef SFTE_CURSOR_BLINK_RATE  // in ms
#define SFTE_CURSOR_BLINK_RATE 500
#endif  // SFTE_CURSOR_BLINK_RATE

// WARN: it relies on double buffer behavior:
// #define SFTE_CURSOR_TRAIL >0
// #define SFTE_DOUBLE_BUFFER 0
// WILL provide unexpected behavior, permanent smearing, etc.
#ifndef SFTE_CURSOR_TRAIL
#define SFTE_CURSOR_TRAIL 0
#endif  // SFTE_CURSOR_TRAIL

#ifndef SFTE_CURSOR_TRAIL_DECAY
#define SFTE_CURSOR_TRAIL_DECAY 0.01f
#endif  // SFTE_CURSOR_TRAIL_DECAY

#ifndef SFTE_CURSOR_TRAIL_COLOR
#define SFTE_CURSOR_TRAIL_COLOR SFTE_CURSOR_COLOR
#endif  // SFTE_CURSOR_TRAIL_COLOR

#ifndef SFTE_CURSOR_THICK_RATIO
#define SFTE_CURSOR_THICK_RATIO 0.1f
#endif  // SFTE_CURSOR_THICK_RATIO

#ifndef SFTE_UNDERLINE_THICK_RATIO
#define SFTE_UNDERLINE_THICK_RATIO 0.1f
#endif  // SFTE_UNDERLINE_THICK_RATIO

#ifndef SFTE_UNDERLINE_OFFSET_RATIO
#define SFTE_UNDERLINE_OFFSET_RATIO 0.12f
#endif  // SFTE_UNDERLINE_OFFSET_RATIO

#ifndef SFTE_SCROLLBACK_CAP
#define SFTE_SCROLLBACK_CAP 2000
#endif  // SFTE_SCROLLBACK_CAP

#ifndef SFTE_SCROLLBACK_ALLOW_CLEAR
#define SFTE_SCROLLBACK_ALLOW_CLEAR 1
#endif  // SFTE_SCROLLBACK_ALLOW_CLEAR

#ifndef SFTE_TAB_WIDTH
#define SFTE_TAB_WIDTH 8
#endif  // SFTE_TAB_WIDTH

#ifndef SFTE_ALT_SCREEN
#define SFTE_ALT_SCREEN 1
#endif  // SFTE_ALT_SCREEN

#ifndef SFTE_DOUBLE_BUFFER
#define SFTE_DOUBLE_BUFFER 1
#endif  // SFTE_DOUBLE_BUFFER

#ifndef SFTE_REFLOW
#define SFTE_REFLOW 1
#endif  // SFTE_REFLOW

#ifndef SFTE_MOUSE
#define SFTE_MOUSE 1
#endif  // SFTE_MOUSE

#ifndef SFTE_HYPERLINKS
#define SFTE_HYPERLINKS 1
#endif  // SFTE_HYPERLINKS

#ifndef SFTE_HYPERLINKS_INIT_CAP
#define SFTE_HYPERLINKS_INIT_CAP 128
#endif  // SFTE_HYPERLINKS_POOL_INIT_CAP

#ifndef SFTE_HYPERLINKS_MAX_CAP
#define SFTE_HYPERLINKS_MAX_CAP 65535
#endif  // SFTE_HYPERLINKS_MAX_CAP

#ifndef SFTE_SIXEL
#define SFTE_SIXEL 1
#endif  // SFTE_SIXEL

#ifndef SFTE_SIXEL_INIT_SIZE
#define SFTE_SIXEL_INIT_SIZE 256
#endif  // SFTE_SIXEL_INIT_SIZE

#ifndef SFTE_SIXEL_MAX_SIZE
#define SFTE_SIXEL_MAX_SIZE 4096
#endif  // SFTE_SIXEL_MAX_SIZE

#ifndef SFTE_KITTY_GRAPHICS
#define SFTE_KITTY_GRAPHICS 1
#endif  // SFTE_KITTY_GRAPHICS

#ifndef SFTE_KITTY_B64_INIT_CAP
#define SFTE_KITTY_B64_INIT_CAP 4096
#endif  // SFTE_KITTY_B64_INIT_CAP

#ifndef SFTE_KITTY_B64_MAX_CAP
#define SFTE_KITTY_B64_MAX_CAP (16 * 1024 * 1024)
#endif  // SFTE_KITTY_B64_MAX_CAP

#ifndef SFTE_IMG_POOL_INIT_CAP
#define SFTE_IMG_POOL_INIT_CAP 16
#endif  // SFTE_IMG_POOL_INIT_CAP

#ifndef SFTE_IMG_POOL_MAX_CAP
#define SFTE_IMG_POOL_MAX_CAP 1024
#endif  // SFTE_IMG_POOL_MAX_CAP

#ifndef SFTE_IMG_PLACEMENT_INIT_CAP
#define SFTE_IMG_PLACEMENT_INIT_CAP 32
#endif  // SFTE_IMG_PLACEMENT_INIT_CAP

#ifndef SFTE_IMG_PLACEMENT_MAX_CAP
#define SFTE_IMG_PLACEMENT_MAX_CAP 4096
#endif  // SFTE_IMG_PLACEMENT_MAX_CAP

#ifndef SFTE_KITTY_KB
#define SFTE_KITTY_KB 1
#endif  // SFTE_KITTY_KB

// NOTE: SFTE_SELECTION implicitly enables SFTE_MOUSE
#ifndef SFTE_SELECTION
#define SFTE_SELECTION 1
#endif  // SFTE_SELECTION

#ifndef SFTE_SCROLL_STEP
#define SFTE_SCROLL_STEP 3
#endif  // SFTE_SCROLL_STEP

#ifndef SFTE_CLIPBOARD
#define SFTE_CLIPBOARD 1
#endif  // SFTE_CLIPBOARD

#ifndef SFTE_CLIPBOARD_BUF_SIZE
#define SFTE_CLIPBOARD_BUF_SIZE 4096
#endif  // SFTE_CLIPBOARD_BUF_SIZE

#ifndef SFTE_OSC52_CLIPBOARD
#define SFTE_OSC52_CLIPBOARD 1
#endif  // SFTE_OSC52_CLIPBOARD

#ifndef SFTE_OSC_INIT_CAP
#define SFTE_OSC_INIT_CAP 1024
#endif  // SFTE_OSC_INIT_CAP

#ifndef SFTE_OSC_MAX_CAP
#define SFTE_OSC_MAX_CAP (16 * 1024 * 1024)
#endif  // SFTE_OSC_MAX_CAP

#if SFTE_SELECTION
#undef SFTE_MOUSE
#define SFTE_MOUSE 1
#endif  // SFTE_SELECTION

#define SFTE_KITTY_FMT_RGB 24
#define SFTE_KITTY_FMT_RGBA 32
#define SFTE_KITTY_FMT_PNG_JPEG 100

#define SFTE_COLOR_ALPHA_MASK 0xFF000000

#define SFTE_MOD_CTRL 0b0001
#define SFTE_MOD_ALT 0b0010
#define SFTE_MOD_SHIFT 0b0100
#define SFTE_MOD_SUPER 0b1000
#define SFTE_MOD_NONE 0b0000

typedef union {
    int i;
    float f;
    const void *v;
} sfte_arg;

typedef struct sfte_ctx sfte_ctx;

typedef struct {
    uint32_t mod_mask;
    uint32_t /* xkb_keysym_t */ keysym;
    void (*func)(sfte_ctx *ctx, const sfte_arg *);
    const sfte_arg arg;
} sfte_shortcut;

#if SFTE_WAYLAND
static void _sfte_wayland_font_resize(sfte_ctx *ctx, const sfte_arg *arg);
static void _sfte_wayland_font_reset(sfte_ctx *ctx, const sfte_arg *arg);
static void _sfte_wayland_view_scroll(sfte_ctx *ctx, const sfte_arg *arg);
static void _sfte_wayland_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg);
static void _sfte_wayland_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_WAYLAND

#if SFTE_FONT_ZOOM && SFTE_WAYLAND
#define _SFTE_WAYLAND_ZOOM_BINDS                                                                   \
    {SFTE_MOD_CTRL, XKB_KEY_equal, _sfte_wayland_font_resize, {.f = 2.0f}},                        \
        {SFTE_MOD_CTRL, XKB_KEY_plus, _sfte_wayland_font_resize, {.f = 2.0f}},                     \
        {SFTE_MOD_CTRL, XKB_KEY_minus, _sfte_wayland_font_resize, {.f = -2.0f}},                   \
        {SFTE_MOD_CTRL, XKB_KEY_0, _sfte_wayland_font_reset, {.v = NULL}},
#else
#define _SFTE_WAYLAND_ZOOM_BINDS
#endif  // !SFTE_FONT_ZOOM || !SFTE_WAYLAND

#if SFTE_SCROLLBACK_CAP && SFTE_WAYLAND
#define _SFTE_WAYLAND_SCROLL_BINDS                                                                 \
    {SFTE_MOD_SHIFT, XKB_KEY_Page_Up, _sfte_wayland_view_scroll, {.i = 10}},                       \
        {SFTE_MOD_SHIFT, XKB_KEY_Page_Down, _sfte_wayland_view_scroll, {.i = -10}},
#else
#define _SFTE_WAYLAND_SCROLL_BINDS
#endif  // !SFTE_SCROLLBACK_CAP || !SFTE_WAYLAND

#if SFTE_CLIPBOARD && SFTE_WAYLAND
#if SFTE_SELECTION
#define _SFTE_WAYLAND_COPY_BIND                                                                    \
    {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, XKB_KEY_C, _sfte_wayland_clipboard_copy, {.v = NULL}},
#endif  // SFTE_SELECTION
#define _SFTE_WAYLAND_PASTE_BIND                                                                   \
    {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, XKB_KEY_V, _sfte_wayland_clipboard_paste, {.v = NULL}},
#else
#define _SFTE_WAYLAND_PASTE_BIND
#endif  // !SFTE_CLIPBOARD || !SFTE_WAYLAND

#if !SFTE_CLIPBOARD || !SFTE_WAYLAND || !SFTE_SELECTION
#define _SFTE_WAYLAND_COPY_BIND
#endif  // !SFTE_CLIPBOARD || !SFTE_WAYLAND || !SFTE_SELECTION

#ifndef SFTE_SHORTCUTS
#define SFTE_SHORTCUTS                                                                             \
    {_SFTE_WAYLAND_ZOOM_BINDS _SFTE_WAYLAND_SCROLL_BINDS _SFTE_WAYLAND_COPY_BIND                   \
         _SFTE_WAYLAND_PASTE_BIND}
#endif  // SFTE_SHORTCUTS

// #################################################################################################
// >>>PUBLIC API
// #################################################################################################
// used to report what regions of the pixel buffer changed
typedef struct {
    int x, y, w, h;
} sfte_damage_rect;

typedef enum sfte_key {
    SFTE_KEY_NONE = 0,
    // control keys (ASCII-based)
    SFTE_KEY_TAB = 9,
    SFTE_KEY_ENTER = 13,
    SFTE_KEY_ESCAPE = 27,
    SFTE_KEY_BACKSPACE = 127,
    // nav keys (offset into dedicated range)
    SFTE_KEY_UP = 1000,
    SFTE_KEY_DOWN,
    SFTE_KEY_LEFT,
    SFTE_KEY_RIGHT,
    SFTE_KEY_HOME,
    SFTE_KEY_END,
    SFTE_KEY_PAGE_UP,
    SFTE_KEY_PAGE_DOWN,
    SFTE_KEY_INSERT,
    SFTE_KEY_DELETE,
    // function keys
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

// callback interface for the core to talk back to the host (e.g. pty)
typedef void (*sfte_write_cb)(void *user_data, const char *data, size_t len);

#if SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD
// callback interface for the core to request clipboard copy/paste operations
// `target` is typically 'c' (clipboard) or 'p' (primary selection).
typedef void (*sfte_osc52_clipboard_cb)(void *user_data, char target, const char *data);
#endif  // SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD

#if SFTE_HYPERLINKS
// callback interface for the core to request opening a URI
typedef void (*sfte_open_link_cb)(void *user_data, const char *uri);
#endif  // SFTE_HYPERLINKS

// context initialization
sfte_ctx *sfte_init(sfte_write_cb write_fn, void *user_data);
void sfte_free(sfte_ctx *ctx);

#define SFTE_FONT_STYLE_REGULAR 0
#ifdef SFTE_FONT_BOLD
#define SFTE_FONT_STYLE_BOLD 1
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
#define SFTE_FONT_STYLE_ITALIC 2
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
#define SFTE_FONT_STYLE_BOLD_ITALIC 3
#endif  // SFTE_FONT_BOLD_ITALIC

// loads a TTF font from a raw memory buffer (e.g. compiled into the binary)
// ttf_data must remain valid for the lifetime of the context
void sfte_font_load_mem(sfte_ctx *ctx, int style, const uint8_t *ttf_data);

// convenience function to load a TTF font from the disk (e.g. for runtime swaps)
void sfte_font_load_file(sfte_ctx *ctx, int style, const char *path);

#ifndef SFTE_NO_POSIX
#include <errno.h>
#include <unistd.h>

// spawns a shell, returns the PID and populates out_fd with the master PTY descriptor
pid_t sfte_posix_pty_spawn(sfte_ctx *ctx, int *out_fd, int px_w, int px_h);

// sends the TIOCSWINSZ ioctl to keep the os shell in sync with the engine grid
void sfte_posix_pty_resize(sfte_ctx *ctx, int pty_fd, int px_w, int px_h);
#endif  // SFTE_NO_POSIX

// ask engine how many pxs it needs to display a specified grid
void sfte_get_ideal_size(sfte_ctx *ctx, int cols, int rows, int *out_w, int *out_h);

// feed bytes from shell/pty to terminal matrix
void sfte_parse(sfte_ctx *ctx, const uint8_t *data, size_t len);

// render grid to provided ARGB8888 buffer
void sfte_render(sfte_ctx *ctx, uint32_t *px_buf, int w, int h, sfte_damage_rect *out_dmg);

// explicitly tell the terminal engine its canvas size has changed
void sfte_resize(sfte_ctx *ctx, int w, int h);

#if SFTE_FONT_ZOOM
void sfte_zoom(sfte_ctx *ctx, float delta);
#endif  // SFTE_FONT_ZOOM

#if SFTE_MOUSE
// feed raw os mouse events to terminal engine
void sfte_mouse_move(sfte_ctx *ctx, int px_x, int px_y);
void sfte_mouse_click(sfte_ctx *ctx, int btn, int pressed, int px_x, int px_y);
// dir: +up -down
void sfte_mouse_scroll(sfte_ctx *ctx, int dir, int px_x, int px_y);
#endif  // SFTE_MOUSE

#if SFTE_KITTY_KB
// generates the kitty keyboard (CSI u) or standard CSI escape sequence for a given key.
// returns bytes written to out_buf, or 0 if inactive/unhandled.
// `key` is a backend-agnostic sfte_key enum. may be SFTE_KEY_KONE if key is purely text.
// `codepoint` is the raw UTF-32 char value of the key (if applicable).
// `mod_mask` is a bitmask of the active modifiers using SFTE_MOD_* definitions.
int sfte_kitty_kb_encode(sfte_ctx *ctx, sfte_key key, uint32_t codepoint, uint32_t mod_mask,
                         char *out_buf, size_t max_bytes);
#endif  // SFTE_KITTY_KB

#if SFTE_HYPERLINKS
// returns the URL at the given cell coords, or NULL if no link is present
const char *sfte_get_link_at(sfte_ctx *ctx, int col, int row);
#endif  // SFTE_HYPERLINKS

#if SFTE_SELECTION
// copies the UTF-8 selection into out_buf, up to max_bytes.
// if out_buf is NULL, performs a dry-run and returns the required byte size.
// returns 0 if nothing is selected.
size_t sfte_get_selection(sfte_ctx *ctx, char *out_buf, size_t max_bytes);
#endif  // SFTE_SELECTION

#if SFTE_SCROLLBACK_CAP
// shift viewport up or down in the scrollback buffer
void sfte_view_scroll(sfte_ctx *ctx, int delta);
#endif  // SFTE_SCROLLBACK_CAP

#if SFTE_WAYLAND
typedef struct sfte_wayland_app sfte_wayland_app;

// initializes wayland, pty and sfte_ctx
sfte_wayland_app *sfte_wayland_init(void);

// exposes the context for runtime configuration
sfte_ctx *sfte_wayland_get_ctx(sfte_wayland_app *app);

// enters the blocking event loop, run it last
int sfte_wayland_run(sfte_wayland_app *app);
#endif  // SFTE_WAYLAND

#define SFTE_IMPL
#ifdef SFTE_IMPL
// #################################################################################################
// >>>INTERNAL DECLARATIONS
// #################################################################################################

#ifndef SFTE_FONT_CUSTOM_BACKEND
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
#endif  // !SFTE_FONT_CUSTOM_BACKEND

#if SFTE_KITTY_GRAPHICS
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"
#endif  // SFTE_KITTY_GRAPHICS

#include <fcntl.h>
#include <locale.h>  // LC_ALL
#include <poll.h>
#include <pty.h>  // forkpty
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>  // memset
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>  // exec/fork/env
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>

#if SFTE_CURSOR_BLINK
#include <time.h>
#endif  // SFTE_CURSOR_BLINK

#if SFTE_WAYLAND
#include "xdg-shell.c"
#include "xdg-shell.h"
#include <wayland-client.h>
#endif  // SFTE_WAYLAND

// =================================================================================================
// >>infrastructure macros
// =================================================================================================

// Retrieves the number of elements in a statically allocated array.
#define _SFTE_ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

// Constraints a value within a specified inclusive range [min, max].
#define _SFTE_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

#ifndef SFTE_NO_LOGGING

#define _SFTE_LOG_ITEMS                                                                            \
    _SFTE_LOGITEM_XMACRO(OK, "ok")                                                                 \
    _SFTE_LOGITEM_XMACRO(FONT_LOADED, "font loaded and baked to atlas")                            \
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
    _sfte_log(ctx, code, _SFTE_LOG_LVL_PANIC, __LINE__, ##__VA_ARGS__)
#define _SFTE_ERROR(ctx, code, ...)                                                                \
    _sfte_log(ctx, code, _SFTE_LOG_LVL_ERROR, __LINE__, ##__VA_ARGS__)
#define _SFTE_WARN(ctx, code, ...) _sfte_log(ctx, code, _SFTE_LOG_LVL_WARN, __LINE__, ##__VA_ARGS__)
#define _SFTE_INFO(ctx, code, ...) _sfte_log(ctx, code, _SFTE_LOG_LVL_INFO, __LINE__, ##__VA_ARGS__)

#else  // !SFTE_NO_LOGGING

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

#endif  // !SFTE_NO_LOGGING

#if SFTE_WIDE_CHARS
#include <wchar.h>
#define _SFTE_CHAR_WIDTH(rune) wcwidth(rune)
#else
#define _SFTE_CHAR_WIDTH(rune) 1
#endif

#if SFTE_CURSOR_BLINK || SFTE_CURSOR_TRAIL
#ifndef SFTE_TIME_MS
#include <time.h>
static inline uint64_t _sfte_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#define SFTE_TIME_MS() _sfte_time_ms()
#endif  // SFTE_TIME_MS
#endif  // SFTE_CURSOR_BLINK || SFTE_CURSOR_TRAIL
// =================================================================================================
// >>internal data structures
// =================================================================================================
#ifndef SFTE_NO_LOGGING
typedef struct sfte_logger {
    void (*func)(const char *tag,              // always "sfte"
                 _sfte_log_level log_level,    // 0=panic, 1=error, 2=warning, 3=info
                 const char *message_or_null,  // a message string, may be nullptr in release mode
                 uint32_t line_nr              // line number in sfte.h
    );
} sfte_logger;
#endif  // !SFTE_NO_LOGGING

typedef enum {
    ATTR_NONE = 0b00000,
    ATTR_BOLD = 0b00001,
    ATTR_ITALIC = 0b00010,
    ATTR_UNDERLINE = 0b00100,
    ATTR_REVERSE = 0b01000,
#if SFTE_WIDE_CHARS
    ATTR_DUMMY = 0b100000,  // marks skipped trailing cell after wide rune
    ATTR_WIDE = 0b010000
#endif  // SFTE_WIDE_CHARS
} sfte_attr;

typedef struct {
    uint32_t rune;
#if SFTE_WIDE_CHARS
    uint32_t combining[SFTE_MAX_COMBINING];
    uint8_t num_combining;
#endif  // SFTE_WIDE_CHARS
#if SFTE_COLOR_UNDERLINE
    uint32_t ul_color;
#endif  // SFTE_COLOR_UNDERLINE
    uint32_t fg;
    uint32_t bg;
    uint16_t attr;  // bitmask of sfte_attr
#if SFTE_HYPERLINKS
    uint16_t link_idx;  // 0=no link, >0=idxs to `term.link_pool`
#endif                  // SFTE_HYPERLINKS
#if SFTE_EXT_UNDERLINES
    uint8_t ul_style;  // 1=straight, 2=double, 3=curl, 4=dotted, 5=dashed
#endif                 // SFTE_EXT_UNDERLINES
    uint8_t dirty;     // 1 if this cell changed
#if SFTE_REFLOW
    uint8_t wrapped;  // 1 if this cell caused a soft line-wrap
#endif                // SFTE_REFLOW
} sfte_cell;

typedef struct {
    uint32_t rune;
    int x0, y0, x1, y1;  // atlas tex coords
    int xoff, yoff;      // render offsets
    int xadvance;
} sfte_glyph;

#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
typedef struct {
    uint32_t id;
    int width;      // in pxs
    int height;     // in pxs
    uint32_t *pxs;  // ARGB8888
    int ref_cnt;    // how many placements are using this img
    uint8_t is_sixel;
} sfte_img;

// placement of an image onto the terminal grid
typedef struct {
    uint32_t img_id;
    uint32_t placement_id;
    int start_col;
    int start_row;
    int x_off;
    int y_off;
    int z_idx;  // <0=below text, >=0=above text
    uint8_t is_sixel;
    uint8_t alt_screen;  // 0=main, 1=alt
} sfte_img_placement;
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS

#if SFTE_SIXEL
typedef enum {
    SIXEL_GROUND,
    SIXEL_REPEAT,       // !
    SIXEL_COLOR_INTRO,  // #
    SIXEL_COLOR_PARAM   // col definition
} sfte_sixel_state_enum;

typedef struct {
    sfte_sixel_state_enum state;
    int x, y;  // in pxs

    // grid placement
    int start_col;  // grid col where parsing started
    int start_row;  // grid row where parsing started

    // dynamic img buf
    uint32_t *pxs;      // dynamic buffer for the in-progress img
    int width, height;  // max bounds actually touched
    int cap_w, cap_h;   // allocated cap

    // sixel parsing state
    uint8_t col_idx;
    uint32_t palette[256];
    int repeat_cnt;

    // color def
    int params[5];
    uint8_t param_idx;
} sfte_sixel_state;
#endif  // SFTE_SIXEL

#if SFTE_KITTY_GRAPHICS
typedef struct {
    char *b64_buf;
    size_t b64_len;
    size_t b64_cap;

    int action;
    uint32_t id;
    int z_idx;
    int format;
    int width;
    int height;
    char t_medium;
    char d_action;
    int cols;
    int rows;
    int x_off;
    int y_off;
    int crop_x;
    int crop_y;
    int crop_w;
    int crop_h;
    uint32_t placement_id;
    uint8_t quiet;  // 0=always, 1=error, 2=never
} sfte_kitty_state;
#endif  // SFTE_KITTY_GRAPHICS

typedef struct {
    sfte_cell *cells;
    int cols;
    int rows;
    char title[256];
    char saved_title[256];
    uint8_t auto_wrap;
    uint8_t origin_mode;
    uint8_t *tab_stops;
    // save state (0=main, 1=alt)
    int saved_x[2];
    int saved_y[2];
    uint32_t saved_fg[2];
    uint32_t saved_bg[2];
    uint32_t saved_attr[2];
    // dirty state
    int dirty_saved_x;
    int dirty_saved_y;
    // cursor style
#if SFTE_CURSOR_BLINK
    uint8_t blink_enabled;  // toggle used by DECSCUSR
    uint8_t blink_visible;  // defining whether cursor is CURRENTLY visible
    uint64_t next_blink_ms;
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_TRAIL
    float tail_rx, tail_ry;
    int last_grid_x, last_grid_y;
    int trail_damage_x, trail_damage_y;
    int trail_damage_w, trail_damage_h;
    uint64_t last_move_ms;
    uint64_t last_trail_update_ms;
    int is_trailing;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_SCROLLBACK_CAP
    sfte_cell *scrollback;
    int sb_cap;
    int sb_len;
    int sb_offset;  // 0 = live, >0 = history
    int sb_head;
#endif  // SFTE_SCROLLBACK_CAP
    int cursor_x;
    int cursor_y;
    uint8_t hide_cursor;
#if SFTE_CURSOR_DYNAMIC
    uint8_t cursor_style;  // block/underline/bar
#endif                     // SFTE_CURSOR_DYNAMIC
// alt screen state
#if SFTE_ALT_SCREEN
    uint8_t alt_active;  // tracks if in alt buffer
    sfte_cell *alt_cells;
#endif  // SFTE_ALT_SCREEN

// mouse state
#if SFTE_MOUSE
    int mouse_hover_x, mouse_hover_y;
    int mouse_mode;       // 0=off, 1000=normal, 1002=button-event, 1003=any-event
    int mouse_ext;        // 0=off, 1006=SGR
    int mouse_btn_state;  // 0=LMB, 1=MMB, 2=RMB, 3=none
#if SFTE_SELECTION
    int mouse_sel_active;                      // 1 if has selection
    int mouse_sel_dragging;                    // 1 if lmb is held down
    int mouse_sel_start_c, mouse_sel_start_r;  // abs grid coords
    int mouse_sel_end_c, mouse_sel_end_r;
#endif  // SFTE_SELECTION
#endif  // SFTE_MOUSE
#if SFTE_KITTY_KB
    int kitty_kb_stack[2][16];  // 0=main, 1=alt
    int kitty_kb_idx[2];
#endif  // SFTE_KITTY_KB
    int scroll_top;
    int scroll_bottom;
    // osc
    char *osc_payload;  // dynamic buffer
    size_t osc_len;     // track len
    size_t osc_cap;     // track alloc cap
    // pen state
    uint32_t cur_fg;
    uint32_t cur_bg;
    uint16_t cur_attr;
#if SFTE_EXT_UNDERLINES
    uint8_t cur_ul_style;
#endif  // SFTE_EXT_UNDERLINES
#if SFTE_COLOR_UNDERLINE
    uint32_t cur_ul_color;
#endif  // SFTE_COLOR_UNDERLINE
// sixel
#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
    sfte_img *img_pool;
    uint32_t img_pool_cap;
    uint32_t img_pool_len;
    sfte_img_placement *img_placements;
    uint32_t img_placements_cap;
    uint32_t img_placements_len;
    uint32_t next_img_id;
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS
    // parser
    int vt_state;
    int vt_params[16];  // stores nums from esc sequences
    int vt_param_idx;
    uint8_t vt_dec_priv;      // tracks if seq starts with ?
    uint8_t bracketed_paste;  // tracks \033[?2004h
    // utf-8 state
    uint32_t utf8_rune;
    int utf8_bytes_left;
// hyperlink state
#if SFTE_HYPERLINKS
    char **link_pool;        // dynamic arr of URI strs
    uint16_t link_pool_len;  // # of stored links
    uint16_t link_pool_cap;  // allocated cap
    uint16_t cur_link_idx;   // active OSC8 link for new txt
#endif                       // SFTE_HYPERLINKS
} sfte_term;

typedef struct {
    sfte_font_backend_info info[SFTE_FONT_MAX_COUNT];
    uint8_t *ttf_buf[SFTE_FONT_MAX_COUNT];
    uint8_t owns_ttf_buf[SFTE_FONT_MAX_COUNT];  // 1 if sfte allocated it via fopen, 0 if user
                                                // provided it
    float scales[SFTE_FONT_MAX_COUNT];
    int num_fonts;

    uint8_t *atlas_pxs;
    sfte_glyph *glyphs;
    int atlas_x;
    int atlas_y;
    int atlas_row_h;
} sfte_font_cache;

typedef struct {
    int cell_width;   // width of a single mono char
    int cell_height;  // height of a single mono char
    int ascent;       // distance from cell top to the baseline
    int descent;      // distance from baseline to cell bottom
    int line_gap;     // recommended empty space between lines
    float cur_size;   // starts at SFTE_FONT_DEFAULT_SIZE

    sfte_font_cache regular;
#ifdef SFTE_FONT_BOLD
    sfte_font_cache bold;
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    sfte_font_cache italic;
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    sfte_font_cache bold_italic;
#endif  // SFTE_FONT_BOLD_ITALIC
} sfte_font;

struct sfte_ctx {
    sfte_term term;
    sfte_font font;
#ifndef SFTE_NO_LOGGING
    sfte_logger logger;
#endif  // !SFTE_NO_LOGGING
#if SFTE_SIXEL
    sfte_sixel_state sixel;
#endif  // SFTE_SIXEL
#if SFTE_KITTY_GRAPHICS
    sfte_kitty_state kitty;
#endif  // SFTE_KITTY_GRAPHICS

    int width;
    int height;
    uint8_t padding_dirty;

    sfte_write_cb write_cb;
    void (*bell_cb)(void *user_data);
#if SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD
    sfte_osc52_clipboard_cb osc52_clipboard_cb;
#endif  // SFTE_OSC52_CLIPBOARD
#if SFTE_HYPERLINKS
    sfte_open_link_cb open_link_cb;
#endif  // SFTE_HYPERLINKS
    void *user_data;

    int damage_min_x;
    int damage_min_y;
    int damage_max_x;
    int damage_max_y;
};

#if SFTE_WAYLAND
struct sfte_wayland_app {
    sfte_ctx *ctx;

    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct xkb_context *xkb_context;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;
#if SFTE_SELECTION
    struct wl_pointer *pointer;
#endif  // SFTE_SELECTION
#if SFTE_CLIPBOARD
    struct wl_data_device_manager *data_device_manager;
    struct wl_data_device *data_device;
    struct wl_data_source *data_source;
    struct wl_data_offer *data_offer;
    char *selection_text;
#endif  // SFTE_CLIPBOARD
#if SFTE_SELECTION || SFTE_CLIPBOARD
    uint32_t serial;
#endif  // SFTE_SELECTION || SFTE_CLIPBOARD
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_buffer *buffer;
    uint32_t *shm_data;
    int shm_size;
#if SFTE_DOUBLE_BUFFER
    uint32_t *back_buffer;
#endif  // SFTE_DOUBLE_BUFFER

    int pty_fd;     // master fd to r/w from
    pid_t pty_pid;  // pid of shell

    uint8_t running;
    uint8_t needs_render;

    int repeat_timer_fd;
    int32_t repeat_rate;
    int32_t repeat_delay;
    uint32_t repeating_key;

    int width, height;
    int pending_width, pending_height;
};
#endif  // SFTE_WAYLAND

typedef struct {
    sfte_cell *main_grid;
    int new_cx, new_cy;
#if SFTE_SCROLLBACK_CAP
    sfte_cell *sb_grid;
    int sb_lines;
#endif  // SFTE_SCROLLBACK_CAP
} _sfte_resize_buffers;

#if SFTE_REFLOW
typedef struct {
    sfte_cell *temp_rows;
    int new_cols;
    int tr, tc;
    int new_cx, new_cy;
    int target_old_cx, target_old_cy;
    int is_live;
} _sfte_reflow_state;
#endif  // SFTE_REFLOW

static const sfte_shortcut _sfte_shortcuts[] = SFTE_SHORTCUTS;
static const uint32_t _sfte_ansi_palette[] = SFTE_ANSI_PALETTE;
static const float _sfte_font_scales[SFTE_FONT_MAX_COUNT] = SFTE_FONT_SCALES;

// =================================================================================================
// >>internal api
// =================================================================================================

// -------------------------------------------------------------------------------------------------
// >log
// -------------------------------------------------------------------------------------------------
#ifndef SFTE_NO_LOGGING
static void _sfte_log_default_func(const char *tag, _sfte_log_level log_level, const char *msg,
                                   uint32_t line_nr);
static void _sfte_log(sfte_ctx *ctx, _sfte_log_item log_item, _sfte_log_level log_level,
                      uint32_t line_nr, ...);
#endif  // !SFTE_NO_LOGGING

// -------------------------------------------------------------------------------------------------
// >b64
// -------------------------------------------------------------------------------------------------
#if (SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD) || SFTE_KITTY_GRAPHICS
static uint8_t *_sfte_b64_decode(const uint8_t *src, size_t len, size_t *out_len);
#endif  // (SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD) || SFTE_KITTY_GRAPHICS

// -------------------------------------------------------------------------------------------------
// >utf8
// -------------------------------------------------------------------------------------------------
static uint8_t _sfte_utf8_decode(sfte_ctx *ctx, uint8_t b);
static inline void _sfte_utf8_stamp_cell(sfte_ctx *ctx, int idx, uint32_t rune,
                                         uint32_t extra_attr);
static void _sfte_utf8_insert_rune(sfte_ctx *ctx, uint32_t rune);

// -------------------------------------------------------------------------------------------------
// >grid
// -------------------------------------------------------------------------------------------------
#define _SFTE_GRID_IDX(ctx, c, r) ((r) * ctx->term.cols + (c))
static inline sfte_cell *_sfte_grid_get_cell(sfte_ctx *ctx, int c, int r);
static void _sfte_grid_from_px(sfte_ctx *ctx, int px_x, int px_y, int *out_c, int *out_logical_r,
                               int *out_screen_r);
static inline void _sfte_grid_dirty_rows(sfte_ctx *ctx, int r1, int r2);
static inline void _sfte_grid_dirty_rect(sfte_ctx *ctx, int start_c, int start_r, int cols,
                                         int rows);
static inline void _sfte_grid_dirty_range(sfte_ctx *ctx, int start_idx, int cnt);
static inline int _sfte_grid_span(int px_len, int px_off, int cell_px);
#if SFTE_SIXEL
static void _sfte_grid_clear_sixel(sfte_ctx *ctx, int start_idx, int cnt);
#endif  // SFTE_SIXEL
#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
static void _sfte_grid_scroll_images(sfte_ctx *ctx, int lines, int top, int bot);
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS
#if SFTE_SCROLLBACK_CAP
static void _sfte_grid_push_scrollback(sfte_ctx *ctx, int lines);
#endif  // SFTE_SCROLLBACK_CAP
static inline void _sfte_grid_clear_cells(sfte_ctx *ctx, int start_idx, int cnt);
static void _sfte_grid_scroll(sfte_ctx *ctx, int lines);
static inline void _sfte_grid_check_wrap(sfte_ctx *ctx);
static sfte_cell *_sfte_grid_resize_dumb_copy(sfte_cell *old_grid, int old_cols, int old_rows,
                                              int new_cols, int new_rows);
static void _sfte_grid_resize_tabs(sfte_ctx *ctx, int old_cols, int new_cols);
static void _sfte_grid_resize(sfte_ctx *ctx, int new_cols, int new_rows);

// -------------------------------------------------------------------------------------------------
// >img
// -------------------------------------------------------------------------------------------------
#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
static inline sfte_img_placement *_sfte_img_placement_insert(sfte_ctx *ctx, sfte_img_placement p);
static inline sfte_img *_sfte_img_find(sfte_ctx *ctx, uint32_t id);
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS

// -------------------------------------------------------------------------------------------------
// >view
// -------------------------------------------------------------------------------------------------
static void _sfte_view_clear_padding_rects(sfte_ctx *ctx, uint32_t *px_buf);

// -------------------------------------------------------------------------------------------------
// >input
// -------------------------------------------------------------------------------------------------
#if SFTE_SELECTION
static inline int _sfte_input_is_selected(sfte_ctx *ctx, int c, int r);
#endif  // SFTE_SELECTION
#if SFTE_MOUSE
static void _sfte_input_send_mouse_event(sfte_ctx *ctx, int btn, int is_release, int c, int r,
                                         int is_motion);
#endif  // SFTE_MOUSE

// -------------------------------------------------------------------------------------------------
// >reflow
// -------------------------------------------------------------------------------------------------
#if SFTE_REFLOW
static void _sfte_reflow_push(_sfte_reflow_state *st, sfte_cell c, int is_cursor);
static inline int _sfte_reflow_get_len(sfte_cell *row, int cols, int cursor_cx);
static void _sfte_reflow_process_row(sfte_cell *row, int cols, int cursor_cx,
                                     _sfte_reflow_state *st);
static void _sfte_reflow_grid_into_linear(sfte_ctx *ctx, sfte_cell *main_old,
                                          _sfte_reflow_state *st);
static sfte_cell *_sfte_reflow_linearize(sfte_ctx *ctx, sfte_cell *main_old, int new_cols,
                                         int new_rows, _sfte_reflow_state *st);
static void _sfte_reflow_extract_view(sfte_ctx *ctx, int new_cols, int new_rows, int target_cy,
                                      _sfte_reflow_state *st, _sfte_resize_buffers *out);
static _sfte_resize_buffers _sfte_reflow_generate_buffers(sfte_ctx *ctx, sfte_cell *main_old,
                                                          int new_cols, int new_rows, int target_cx,
                                                          int target_cy);
#endif  // SFTE_REFLOW

// -------------------------------------------------------------------------------------------------
// >sixel
// -------------------------------------------------------------------------------------------------
#if SFTE_SIXEL
static inline void _sfte_sixel_commit(sfte_ctx *ctx);
static void _sfte_sixel_ensure_cap(sfte_ctx *ctx, int req_w, int req_h);
static void _sfte_sixel_draw_pattern(sfte_ctx *ctx, uint8_t pattern, int repeats);
static inline float _sfte_sixel_hue_to_rgb(float p, float q, float t);
static inline uint32_t _sfte_sixel_hls_to_rgb(uint16_t h_deg, uint16_t l_pct, uint16_t s_pct);
static void _sfte_sixel_apply_color(sfte_ctx *ctx);
static void _sfte_sixel_parse_byte(sfte_ctx *ctx, uint8_t b);
static void _sfte_sixel_deinit(sfte_ctx *ctx);
#endif  // SFTE_SIXEL

// -------------------------------------------------------------------------------------------------
// >kitty
// -------------------------------------------------------------------------------------------------
#if SFTE_KITTY_GRAPHICS
static uint32_t *_sfte_kitty_scale_image_bilinear(uint32_t *src, int sw, int sh, int dw, int dh);
static uint32_t *_sfte_kitty_decode_payload(sfte_ctx *ctx, uint8_t *raw_data, size_t raw_len,
                                            uint8_t is_file, const char *file_path, int *w, int *h);
static uint32_t *_sfte_kitty_apply_crop(sfte_ctx *ctx, uint32_t *pxs, int *w, int *h);
static uint32_t *_sfte_kitty_apply_scale(sfte_ctx *ctx, uint32_t *pxs, int *w, int *h);
static uint8_t _sfte_kitty_should_delete(sfte_ctx *ctx, sfte_img_placement *p, sfte_img *img);
static void _sfte_kitty_gc_pool(sfte_ctx *ctx);
static const char *_sfte_kitty_apply_placement(sfte_ctx *ctx, sfte_img *img);
static const char *_sfte_kitty_exec_query(sfte_ctx *ctx);
static const char *_sfte_kitty_exec_delete(sfte_ctx *ctx);
static const char *_sfte_kitty_exec_transmit(sfte_ctx *ctx, sfte_img **out_img);
static const char *_sfte_kitty_exec_place(sfte_ctx *ctx);
static void _sfte_kitty_send_ack(sfte_ctx *ctx, const char *err_msg);
static void _sfte_kitty_parse_graphics(sfte_ctx *ctx, const char *payload);
static void _sfte_kitty_deinit(sfte_ctx *ctx);
#endif  // SFTE_KITTY_GRAPHICS

// -------------------------------------------------------------------------------------------------
// >csi
// -------------------------------------------------------------------------------------------------
#if SFTE_TRUE_COLOR
static inline uint32_t _sfte_csi_parse_truecolor(int *p, int i);
#endif  // SFTE_TRUE_COLOR
static inline void _sfte_csi_exec_ich(sfte_ctx *ctx, int *p, int cx);
static inline void _sfte_csi_exec_cnl(sfte_ctx *ctx, int *p);
static inline void _sfte_csi_exec_cpl(sfte_ctx *ctx, int *p);
static inline void _sfte_csi_exec_erase_display(sfte_ctx *ctx, int mode, int cx);
static inline void _sfte_csi_exec_il(sfte_ctx *ctx, int *p);
static inline void _sfte_csi_exec_dl(sfte_ctx *ctx, int *p);
static inline void _sfte_csi_exec_dch(sfte_ctx *ctx, int *p, int cx);
static inline void _sfte_csi_exec_ech(sfte_ctx *ctx, int *p, int cx);
static inline void _sfte_csi_exec_da(sfte_ctx *ctx);
static inline void _sfte_csi_exec_vpa(sfte_ctx *ctx, int *p);
static inline void _sfte_csi_exec_hvp(sfte_ctx *ctx, int *p);
static inline void _sfte_csi_exec_tbc(sfte_ctx *ctx, int *p);
static inline void _sfte_csi_set_mode(sfte_ctx *ctx, int *p, int cnt, int cx);
static inline void _sfte_csi_reset_mode(sfte_ctx *ctx, int *p, int cnt, int cx);
static inline void _sfte_csi_exec_sgr(sfte_ctx *ctx, int *p, int cnt);
static inline void _sfte_csi_exec_dsr(sfte_ctx *ctx, int *p);
static inline void _sfte_csi_exec_decstr(sfte_ctx *ctx, int cx);
static inline void _sfte_csi_exec_decscusr(sfte_ctx *ctx, int *p, int cx);
static inline void _sfte_csi_exec_decstbm(sfte_ctx *ctx, int *p, int cnt);
static inline void _sfte_csi_exec_scosc(sfte_ctx *ctx, int *p);
static inline void _sfte_csi_exec_xtwinops(sfte_ctx *ctx, int *p);
static inline void _sfte_csi_exec_scorc(sfte_ctx *ctx, int *p);
#if SFTE_KITTY_KB
static inline void _sfte_csi_exec_kitty(sfte_ctx *ctx, int *p);
#endif  // SFTE_KITTY_KB
static void _sfte_csi_dispatch(sfte_ctx *ctx, uint8_t cmd);

// -------------------------------------------------------------------------------------------------
// >parser
// -------------------------------------------------------------------------------------------------
static inline void _sfte_parser_append_payload(sfte_ctx *ctx, uint8_t b);
static inline void _sfte_parser_c0_lf(sfte_ctx *ctx);
static inline void _sfte_parser_c0_ht(sfte_ctx *ctx);
static inline void _sfte_parser_esc_ris(sfte_ctx *ctx);
static inline void _sfte_parser_esc_sc(sfte_ctx *ctx);
static inline void _sfte_parser_esc_rc(sfte_ctx *ctx);
static inline void _sfte_parser_esc_ind(sfte_ctx *ctx);
static inline void _sfte_parser_esc_ri(sfte_ctx *ctx);
static inline void _sfte_parser_esc_nel(sfte_ctx *ctx);
static inline void _sfte_parser_hash_decaln(sfte_ctx *ctx);
static inline void _sfte_parser_osc_dispatch(sfte_ctx *ctx, uint8_t terminator);
static inline void _sfte_parser_dcs_dispatch(sfte_ctx *ctx, uint8_t terminator);
static void _sfte_parser_feed_byte(sfte_ctx *ctx, uint8_t b);

// -------------------------------------------------------------------------------------------------
// >font
// -------------------------------------------------------------------------------------------------
#ifndef SFTE_CUSTOM_FONT_BACKEND
#define SFTE_FONT_INIT _sfte_stb_init
#define SFTE_FONT_GET_SCALE _sfte_stb_get_scale
#define SFTE_FONT_VMETRICS _sfte_stb_vmetrics
#define SFTE_FONT_BOUNDS _sfte_stb_bounds
#define SFTE_FONT_BAKE _sfte_stb_bake
#endif  // !SFTE_CUSTOM_FONT_BACKEND
static inline sfte_font_cache *_sfte_font_get_cache(sfte_ctx *ctx, int style);
static inline void _sfte_font_clear_cache(sfte_font_cache *cache);
static inline void _sfte_font_update_scales(sfte_ctx *ctx, sfte_font_cache *cache);
static inline void _sfte_font_pack_and_bake(sfte_font_cache *cache, sfte_glyph *g, int font_idx,
                                            int glyph_idx, int gw, int gh);
static sfte_glyph *_sfte_font_get_glyph(sfte_ctx *ctx, sfte_font_cache **cache_ptr, uint32_t rune);
static void _sfte_font_reset_cache(sfte_ctx *ctx);

// -------------------------------------------------------------------------------------------------
// >render
// -------------------------------------------------------------------------------------------------
static inline void _sfte_render_damage_add(int *x0, int *y0, int *x1, int *y1, int px, int py,
                                           int pw, int ph);
static inline void _sfte_render_propagate_damage(sfte_ctx *ctx, int vis_cx, int vis_cy);
#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
static inline void _sfte_render_sort_images(sfte_ctx *ctx);
static inline void _sfte_render_images(sfte_ctx *ctx, uint32_t *px_buf, int *b_x0, int *b_y0,
                                       int *b_x1, int *b_y1, uint8_t is_bg_pass, int base_y_off,
                                       uint8_t pad_was_dirty);
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS
static inline uint32_t _sfte_render_blend_argb(uint32_t dst, uint32_t src_col, uint8_t src_a);
static void _sfte_render_bg_cell(sfte_ctx *ctx, uint32_t *px_buf, int col, int row, uint32_t bg);
static void _sfte_render_fg_cell(sfte_ctx *ctx, uint32_t *px_buf, int col, int row, uint32_t rune,
                                 uint32_t fg, sfte_font_cache *target_cache);
static inline void _sfte_render_underline_cell(sfte_ctx *ctx, uint32_t *px_buf, int cx, int cy,
                                               int render_w, sfte_cell *vcell);
static inline void _sfte_render_cursor_shape(sfte_ctx *ctx, uint32_t *px_buf, int cx, int cy,
                                             int render_w);
static void _sfte_render_decorations_cell(sfte_ctx *ctx, uint32_t *px_buf, int c, int r,
                                          sfte_cell *vcell, uint8_t is_cursor);
static inline void _sfte_render_bg_grid(sfte_ctx *ctx, uint32_t *px_buf, int vis_cx, int vis_cy);
static inline void _sfte_render_fg_grid(sfte_ctx *ctx, uint32_t *px_buf, int vis_cx, int vis_cy,
                                        int *bx0, int *by0, int *bx1, int *by1);

// -------------------------------------------------------------------------------------------------
// >wayland
// -------------------------------------------------------------------------------------------------
#if SFTE_WAYLAND
static sfte_key _sfte_xkb_to_sfte_key(xkb_keysym_t sym);
static void _sfte_wayland_write_cb(void *user_data, const char *data, size_t len);
static void _sfte_wayland_pty_spawn(sfte_wayland_app *app);
static void _sfte_wayland_pty_update(sfte_wayland_app *app);
#if SFTE_FONT_ZOOM
static void _sfte_wayland_font_resize(sfte_ctx *ctx, const sfte_arg *arg);
static void _sfte_wayland_font_reset(sfte_ctx *ctx, const sfte_arg *dummy);
#endif  // SFTE_FONT_ZOOM
#if SFTE_SCROLLBACK_CAP
static void _sfte_wayland_view_scroll(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_SCROLLBACK_CAP
static void _sfte_wayland_create_buffer(sfte_wayland_app *app);
#if SFTE_HYPERLINKS
static void _sfte_wayland_open_link_cb(void *user_data, const char *uri);
#endif  // SFTE_HYPERLINKS
static void _sfte_wayland_load(sfte_wayland_app *app);
static void _sfte_wayland_unload(sfte_wayland_app *app);
#if SFTE_CLIPBOARD
#if SFTE_SELECTION
#if SFTE_OSC52_CLIPBOARD
static void _sfte_wayland_osc52_clipboard_cb(void *user_data, char target, const char *data);
#endif  // SFTE_OSC52_CLIPBOARD
static void _sfte_wayland_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_SELECTION
static void _sfte_wayland_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_CLIPBOARD
static void _sfte_wayland_loop(sfte_wayland_app *app);
#endif  // SFTE_WAYLAND

// #################################################################################################
// >>>INTERNAL IMPLEMENTATION
// #################################################################################################

// =================================================================================================
// >>memory
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

// =================================================================================================
// >>log
// =================================================================================================
#ifndef SFTE_NO_LOGGING

#define _SFTE_LOG_MAX_MSG_LEN 512

/*
    Default standard error logging sink.
    Outputs logs in format: ['tag':'line_nr']('log_level') 'msg'
*/
static void _sfte_log_default_func(const char *tag, _sfte_log_level log_level, const char *msg,
                                   uint32_t line_nr) {
    const char *level_str = "???";
    switch (log_level) {
    case _SFTE_LOG_LVL_PANIC: level_str = "PANIC"; break;
    case _SFTE_LOG_LVL_ERROR: level_str = "ERROR"; break;
    case _SFTE_LOG_LVL_WARN: level_str = "WARN"; break;
    case _SFTE_LOG_LVL_INFO: level_str = "INFO"; break;
    }
    fprintf(stderr, "[%s:%d](%s) %s\n", tag, line_nr, level_str, msg);
}

/*
    Formats a log message from the X-Macro catalog passed by 'log_item' and dispatches it to the
    active sink.

    Routes to a user-provided logger if one is configured in 'ctx', otherwise falls
    back to stderr. Aborts the process if 'log_level' is PANIC.

    This function is not called directly, instead its used by macros (`_SFTE_PANIC/ERROR/WARN/INFO`)
*/
static void _sfte_log(sfte_ctx *ctx, _sfte_log_item log_item, _sfte_log_level log_level,
                      uint32_t line_nr, ...) {
    if (log_level > SFTE_LOG_LEVEL) return;

    char buf[_SFTE_LOG_MAX_MSG_LEN];
    va_list args;
    va_start(args, line_nr);
    vsnprintf(buf, sizeof(buf), _sfte_log_messages[log_item], args);
    va_end(args);

    void (*log_func)(const char *, _sfte_log_level, const char *,
                     uint32_t) = ctx->logger.func ? ctx->logger.func : _sfte_log_default_func;

    log_func(SFTE_LOG_TAG, log_level, buf, line_nr);

    // for log level PANIC it would be 'undefined behaviour' to continue
    if (log_level == _SFTE_LOG_LVL_PANIC) abort();
}

#endif  // !SFTE_NO_LOGGING
// =================================================================================================
// >>b64
// =================================================================================================
#if (SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD) || SFTE_KITTY_GRAPHICS
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
    Decodes a Base64 payload into a newly allocated binary buffer.
    Ignores invalid characters (spaces, newlines).
    The caller assumes ownership of the returned pointer and MUST free it.
    Returns NULL if input is empty or if memory allocation fails.
*/
static uint8_t *_sfte_b64_decode(const uint8_t *src, size_t len, size_t *out_len) {
    SFTE_ASSERT(src && out_len, "b64_decode requires valid pointers");

    // Strip trailing padding
    while (len > 0 && src[len - 1] == '=') len--;

    *out_len = (len * 3) / 4;
    uint8_t *dst = (uint8_t *)SFTE_MALLOC(*out_len);
    if (!dst) return NULL;

    size_t i = 0, j = 0;
    uint32_t acc = 0;
    int bits = 0;

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
#endif  // (SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD) || SFTE_KITTY_GRAPHICS
// =================================================================================================
// >>utf8
// =================================================================================================

/*
    Feeds a single byte into the UTF-8 state machine.
    Returns 1 if a complete rune has been successfully decoded.
    Returns 0 if more bytes are needed, or if an invalid sequence was encountered (which aborts the
   current sequence and resets the state machine).
*/
static uint8_t _sfte_utf8_decode(sfte_ctx *ctx, uint8_t b) {
    if (ctx->term.utf8_bytes_left > 0) {
        if ((b & 0xC0) == 0x80) {  // Continuation byte
            ctx->term.utf8_rune = (ctx->term.utf8_rune << 6) | (b & 0x3F);
            ctx->term.utf8_bytes_left--;
            if (!ctx->term.utf8_bytes_left) return 1;
            return 0;
        } else
            ctx->term.utf8_bytes_left = 0;  // Invalid sequence, abort
    }

    // Start of a new rune
    if ((b & 0x80) == 0x00) {
        ctx->term.utf8_rune = b;
        ctx->term.utf8_bytes_left = 0;
        return 1;
    } else if ((b & 0xE0) == 0xC0) {
        ctx->term.utf8_rune = b & 0x1F;
        ctx->term.utf8_bytes_left = 1;
    } else if ((b & 0xF0) == 0xE0) {
        ctx->term.utf8_rune = b & 0x0F;
        ctx->term.utf8_bytes_left = 2;
    } else if ((b & 0xF8) == 0xF0) {
        ctx->term.utf8_rune = b & 0x07;
        ctx->term.utf8_bytes_left = 3;
    }

    return 0;
}

/*
    Stamps the active terminal cursor styling onto a specific grid cell.
    Isolates all feature-toggle macros to keep call sites clean.
    Does NOT advance the cursor.
*/
static inline void _sfte_utf8_stamp_cell(sfte_ctx *ctx, int idx, uint32_t rune,
                                         uint32_t extra_attr) {
    sfte_cell *c = &ctx->term.cells[idx];
    c->rune = rune;
    c->fg = ctx->term.cur_fg;
    c->bg = ctx->term.cur_bg;
    c->attr = ctx->term.cur_attr | extra_attr;
    c->dirty = 1;

#if SFTE_HYPERLINKS
    c->link_idx = ctx->term.cur_link_idx;
#endif  // SFTE_HYPERLINKS
#if SFTE_EXT_UNDERLINES
    c->ul_style = ctx->term.cur_ul_style;
#endif  // SFTE_EXT_UNDERLINES
#if SFTE_COLOR_UNDERLINE
    c->ul_color = ctx->term.cur_ul_color;
#endif  // SFTE_COLOR_UNDERLINE
}

/*
    Inserts a decoded unicode rune into the terminal grid at the current cursor position.
    Combining characters (these of width 0) are appended to the previous logical cell.
    Double-width characters (these of width 2) if placed on the last column, the column
    is left blank, the line wraps early, and the character is placed on the next line.
*/
static void _sfte_utf8_insert_rune(sfte_ctx *ctx, uint32_t rune) {
    int w = _SFTE_CHAR_WIDTH(rune);
    if (w < 0) w = 1;

    if (w == 0) {
#if SFTE_WIDE_CHARS
        if (ctx->term.cursor_x > 0) {
            int prev_idx = _SFTE_GRID_IDX(ctx, ctx->term.cursor_x - 1, ctx->term.cursor_y);
            sfte_cell *prev = &ctx->term.cells[prev_idx];

            if (prev->attr & ATTR_DUMMY && ctx->term.cursor_x > 1) {
                prev_idx = _SFTE_GRID_IDX(ctx, ctx->term.cursor_x - 2, ctx->term.cursor_y);
                prev = &ctx->term.cells[prev_idx];
            }

            if (prev->num_combining < SFTE_MAX_COMBINING) {
                prev->combining[prev->num_combining++] = rune;
                prev->dirty = 1;
            }
        }
#endif  // SFTE_WIDE_CHARS
        return;
    }

    // Evaluate line wrapping before drawing,
    // ensures characters placed in the final column enter a pending wrap state.
    _sfte_grid_check_wrap(ctx);

#if SFTE_WIDE_CHARS
    if (w == 2) {
        // A wide character cannot be split across lines,
        // if in last column, leave it blank and wrap early.
        if (ctx->term.cursor_x == ctx->term.cols - 1) {
            int idx = _SFTE_GRID_IDX(ctx, ctx->term.cursor_x, ctx->term.cursor_y);
            _sfte_utf8_stamp_cell(ctx, idx, ' ', 0);
            ctx->term.cells[idx].fg = 0xFFFFFF;
            ctx->term.cells[idx].bg = SFTE_BG_COLOR;
            ctx->term.cells[idx].attr = 0;

            ctx->term.cursor_x++;
            _sfte_grid_check_wrap(ctx);
        }

        // If terminal grid has only one column, we can't draw the double-width character.
        if (ctx->term.cursor_x == ctx->term.cols - 1) return;

        // Draw the actual character.
        int idx = _SFTE_GRID_IDX(ctx, ctx->term.cursor_x, ctx->term.cursor_y);
        _sfte_utf8_stamp_cell(ctx, idx, rune, ATTR_WIDE);

        // Place a dummy right after it so that it has enough space to render.
        int dummy_idx = _SFTE_GRID_IDX(ctx, ctx->term.cursor_x + 1, ctx->term.cursor_y);
        _sfte_utf8_stamp_cell(ctx, dummy_idx, rune, ATTR_DUMMY);

        ctx->term.cursor_x += 2;
        return;
    }
#endif  // SFTE_WIDE_CHARS

    // Write a normal, one-width character.
    int idx = _SFTE_GRID_IDX(ctx, ctx->term.cursor_x, ctx->term.cursor_y);
    _sfte_utf8_stamp_cell(ctx, idx, rune, 0);
    ctx->term.cursor_x++;
}

// =================================================================================================
// >>grid
// =================================================================================================

// -------------------------------------------------------------------------------------------------
// >grid helpers
// -------------------------------------------------------------------------------------------------

/*
    Retrieves a pointer to a specific cell in memory using logical grid coordinates.

    A negative `logical_r` seamlessly reaches back into the scrollback ring buffer.
    Assumes the caller has already validated that `logical_r` doesn't exceed scrollback length.
*/
static inline sfte_cell *_sfte_grid_get_cell(sfte_ctx *ctx, int c, int r) {
#if SFTE_SCROLLBACK_CAP
    if (r >= 0)
        return &ctx->term.cells[_SFTE_GRID_IDX(ctx, c, r)];
    else {
        // NOTE: The + (100 * sb_cap) prevents negative modulo results
        int ring_r = (ctx->term.sb_head + r + (100 * ctx->term.sb_cap)) % ctx->term.sb_cap;
        return &ctx->term.scrollback[ring_r * ctx->term.cols + c];
    }
#else   // !SFTE_SCROLLBACK_CAP
    return &ctx->term.cells[_SFTE_GRID_IDX(ctx, c, r)];
#endif  // !SFTE_SCROLLBACK_CAP
}

/*
    Converts physical pixel coordinates into discrete grid coordinates.
    `out_logical_r` includes scrollback offset (can be negative).
    `out_screen_r` is strictly clamped to the physical screen (0 to rows-1).
*/
static void _sfte_grid_from_px(sfte_ctx *ctx, int px_x, int px_y, int *out_c, int *out_logical_r,
                               int *out_screen_r) {
    int c = _SFTE_CLAMP((px_x - SFTE_PAD_X) / ctx->font.cell_width, 0, ctx->term.cols - 1);
    int r = _SFTE_CLAMP((px_y - SFTE_PAD_Y) / ctx->font.cell_height, 0, ctx->term.rows - 1);
    if (out_c) *out_c = c;
    if (out_screen_r) *out_screen_r = r;

    if (out_logical_r) {
#if SFTE_SCROLLBACK_CAP
        *out_logical_r = r - ctx->term.sb_offset;
#else   // !SFTE_SCROLLBACK_CAP
        *out_logical_r = r;
#endif  // !SFTE_SCROLLBACK_CAP
    }
}

/*
    Flags a range of logical rows as dirty to force a redraw.
    Automatically handles min/max sorting, scrollback offset mapping,
    and clamping to the visible physical screen.
*/
static inline void _sfte_grid_dirty_rows(sfte_ctx *ctx, int r1, int r2) {
    int min_r = r1 < r2 ? r1 : r2;
    int max_r = r1 > r2 ? r1 : r2;

#if SFTE_SCROLLBACK_CAP
    min_r += ctx->term.sb_offset;
    max_r += ctx->term.sb_offset;
#endif  // SFTE_SCROLLBACK_CAP

    min_r = _SFTE_CLAMP(min_r, 0, ctx->term.rows);
    max_r = _SFTE_CLAMP(max_r, 0, ctx->term.rows);

    if (min_r <= max_r)
        _sfte_grid_dirty_range(ctx, min_r * ctx->term.cols, (max_r - min_r + 1) * ctx->term.cols);
}

/*
    Flags a rectangular region of the grid as dirty, forcing a redraw on the next frame.
    Safely clips coordinates that fall outside the terminal boundaries.
*/
static inline void _sfte_grid_dirty_rect(sfte_ctx *ctx, int start_c, int start_r, int cols,
                                         int rows) {
    for (int r = start_r; r < start_r + rows; ++r) {
        if (r < 0 || r >= ctx->term.rows) continue;
        for (int c = start_c; c < start_c + cols; ++c) {
            if (c < 0 || c >= ctx->term.cols) continue;
            ctx->term.cells[_SFTE_GRID_IDX(ctx, c, r)].dirty = 1;
        }
    }
}

/*
    Flags a contiguous 1D range of grid cells [start_idx; start_idx + cnt) as dirty.
    Does NOT perform boundary checking, uses asserts for boundary safety
    to avoid branch-checking overhead in RELEASE builds.
*/
static inline void _sfte_grid_dirty_range(sfte_ctx *ctx, int start_idx, int cnt) {
    SFTE_ASSERT(start_idx >= 0, "dirty_range index underflow");
    SFTE_ASSERT(start_idx + cnt <= ctx->term.cols * ctx->term.rows, "dirty_range index overflow");

    for (int i = 0; i < cnt; ++i) ctx->term.cells[start_idx + i].dirty = 1;
}

/*
    Calculates how many terminal cells are spanned by a given pixel dimension,
    accounting for an arbitrary pixel offset within the starting cell.
*/
static inline int _sfte_grid_span(int px_len, int px_off, int cell_px) {
    return (px_len + px_off + cell_px - 1) / cell_px;
}

#if SFTE_SIXEL
/*
    Deletes Sixel image placements that intersect with a cleared grid region.
    Automatically frees Sixel image data if the reference count drops to 0.
    Kitty images are ignored as they require explicit terminal delete commands.
*/
static void _sfte_grid_clear_sixel(sfte_ctx *ctx, int start_idx, int cnt) {
    for (uint32_t i = 0; i < ctx->term.img_placements_len; ++i) {
        sfte_img_placement *p = &ctx->term.img_placements[i];
        if (!p->is_sixel) continue;
#if SFTE_ALT_SCREEN
        // Prevent active-screen clears from wiping out hidden-screen images
        if (p->alt_screen != ctx->term.alt_active) continue;
#endif  // SFTE_ALT_SCREEN

        sfte_img *img = _sfte_img_find(ctx, p->img_id);
        if (!img) continue;

        int cols = _sfte_grid_span(img->width, p->x_off, ctx->font.cell_width);
        int rows = _sfte_grid_span(img->height, p->y_off, ctx->font.cell_height);

        uint8_t overlap = 0;
        for (int r = p->start_row; r < p->start_row + rows && !overlap; ++r)
            for (int c = p->start_col; c < p->start_col + cols; ++c) {
                int cell_idx = r * ctx->term.cols + c;
                if (cell_idx >= start_idx && cell_idx < start_idx + cnt) {
                    overlap = 1;
                    break;
                }
            }

        if (overlap) {
            img->ref_cnt--;
            ctx->term.img_placements[i--] = ctx->term
                                                .img_placements[--ctx->term.img_placements_len];
        }
    }

    for (uint32_t i = 0; i < ctx->term.img_pool_len; ++i) {
        if (ctx->term.img_pool[i].ref_cnt || !ctx->term.img_pool[i].is_sixel) continue;
        if (ctx->term.img_pool[i].pxs) SFTE_FREE(ctx->term.img_pool[i].pxs);
        ctx->term.img_pool[i--] = ctx->term.img_pool[--ctx->term.img_pool_len];
    }
}
#endif  // SFTE_SIXEL

#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
/*
    Translates image placements up/down during screen scroll.
    Deletes images that scroll entirely out of the scrollback buffer.
*/
static void _sfte_grid_scroll_images(sfte_ctx *ctx, int lines, int top, int bot) {
    for (uint32_t i = 0; i < ctx->term.img_placements_len; ++i) {
        sfte_img_placement *p = &ctx->term.img_placements[i];
#if SFTE_ALT_SCREEN
        if (p->alt_screen != ctx->term.alt_active) continue;
#endif  // SFTE_ALT_SCREEN

        // An image should only scroll if it falls into one of two categories:
        // 1. it is inside the active scrolling margins (or sitting just on the edge at bot+1)
        // 2. It is in the scrollback buffer (<0), AND we are doing a full-screen scroll
        // (top==0) which pushes new lines into the scrollback.
        if ((p->start_row >= top && p->start_row <= bot + 1) || (top == 0 && p->start_row < 0))
            p->start_row -= lines;

        sfte_img *img = _sfte_img_find(ctx, p->img_id);
        if (!img) continue;

        int rows = _sfte_grid_span(img->height, p->y_off, ctx->font.cell_height);
        if (p->start_row + rows <= -SFTE_SCROLLBACK_CAP) {
            img->ref_cnt--;
            ctx->term.img_placements[i--] = ctx->term
                                                .img_placements[--ctx->term.img_placements_len];
        }
    }
}
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS

#if SFTE_SCROLLBACK_CAP
/*
    Pushes lines scrolling off the top of the screen into the ring buffer.
*/
static void _sfte_grid_push_scrollback(sfte_ctx *ctx, int lines) {
#if SFTE_ALT_SCREEN
    if (ctx->term.alt_active) return;
#endif  // SFTE_ALT_SCREEN

    int cols = ctx->term.cols;
    for (int i = 0; i < lines; ++i) {
        int ring_idx = ctx->term.sb_head * cols;
        int screen_idx = i * cols;
        memcpy(&ctx->term.scrollback[ring_idx], &ctx->term.cells[screen_idx],
               cols * sizeof(sfte_cell));

        ctx->term.sb_head = (ctx->term.sb_head + 1) % ctx->term.sb_cap;
        if (ctx->term.sb_len < ctx->term.sb_cap) ctx->term.sb_len++;
    }
}
#endif  // SFTE_SCROLLBACK_CAP

// -------------------------------------------------------------------------------------------------
// >grid core
// -------------------------------------------------------------------------------------------------

/*
    Wipes a 1D range of cells, resetting them to ' ' and applying the
    currently active terminal foreground, background and text attributes.
    Also triggers the deletion of any Sixel images that intersect that region.
    Does NOT move the cursor.
*/
static inline void _sfte_grid_clear_cells(sfte_ctx *ctx, int start_idx, int cnt) {
    for (int i = 0; i < cnt; ++i) {
        sfte_cell *c = &ctx->term.cells[start_idx + i];
        c->rune = ' ';
        c->fg = ctx->term.cur_fg;
        c->bg = ctx->term.cur_bg;
        c->attr = 0;
        c->dirty = 1;
#if SFTE_EXT_UNDERLINES
        c->ul_style = 0;
#endif  // SFTE_EXT_UNDERLINES
#if SFTE_COLOR_UNDERLINE
        c->ul_color = 0xFFFFFFFF;
#endif  // SFTE_COLOR_UNDERLINE
#if SFTE_REFLOW
        c->wrapped = 0;
#endif  // SFTE_REFLOW
#if SFTE_HYPERLINKS
        c->link_idx = 0;
#endif  // SFTE_HYPERLINKS
    }

#if SFTE_SIXEL
    _sfte_grid_clear_sixel(ctx, start_idx, cnt);
#endif  // SFTE_SIXEL
}

/*
    Shifts the terminal grid up or down by the specified number of lines.
    Positive values scroll the text UP (moving the viewport down).
    Negative values scroll the text DOWN (moving the viewport up).

    Strictly respects the active scroll margins (`ctx->term.scroll_top/bottom`).
    Pushes lines that fall off the top margin into the scrollback buffer
    (only if scrolling the primary screen and starting from row 0).

    Synchronizes image placements to scroll with the text.
*/
static void _sfte_grid_scroll(sfte_ctx *ctx, int lines) {
    int top = ctx->term.scroll_top;
    int bot = ctx->term.scroll_bottom;
    int height = bot - top + 1;
    int cols = ctx->term.cols;

    // Clamp the scroll amount to the region height while preserving direction.
    lines = _SFTE_CLAMP(lines, -height, height);

#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
    _sfte_grid_scroll_images(ctx, lines, top, bot);
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS

    if (lines > 0) {  // Scroll up
#if SFTE_SCROLLBACK_CAP
        if (top == 0) _sfte_grid_push_scrollback(ctx, lines);
#endif  // SFTE_SCROLLBACK_CAP

        int move_cnt = height - lines;
        if (move_cnt > 0)
            memmove(&ctx->term.cells[top * cols], &ctx->term.cells[(top + lines) * cols],
                    move_cnt * cols * sizeof(sfte_cell));

        _sfte_grid_clear_cells(ctx, (bot - lines + 1) * cols, lines * cols);
    } else if (lines < 0) {  // Scroll down
        lines = -lines;

        int move_cnt = height - lines;
        if (move_cnt > 0)
            memmove(&ctx->term.cells[(top + lines) * cols], &ctx->term.cells[top * cols],
                    move_cnt * cols * sizeof(sfte_cell));

        _sfte_grid_clear_cells(ctx, top * cols, lines * cols);
    }

    _sfte_grid_dirty_range(ctx, top * cols, height * cols);
}

/*
    Evaluates the cursor's X position against the terminal width and handles wrapping.
    Must be called BEFORE printing a character that might fall off the edge.

    If auto-wrap is ON, it wraps cursor to column 0 of the next line.
    If already at the bottom scroll margin, forces a 1-line scroll upwards.
    Marks the wrapped cell with a `wrapped = 1` flag (used by the reflow engine).

    If auto-wrap is OFF, it clamps the cursor to the final column, causing subsequent
    characters to overwrite each other.
*/
static inline void _sfte_grid_check_wrap(sfte_ctx *ctx) {
    if (ctx->term.cursor_x >= ctx->term.cols) {
        if (ctx->term.auto_wrap) {
#if SFTE_REFLOW
            ctx->term.cells[_SFTE_GRID_IDX(ctx, ctx->term.cols - 1, ctx->term.cursor_y)]
                .wrapped = 1;
#endif  // SFTE_REFLOW
            ctx->term.cursor_x = 0;
            if (ctx->term.cursor_y == ctx->term.scroll_bottom) {
                uint32_t saved_bg = ctx->term.cur_bg;
                ctx->term.cur_bg = SFTE_BG_COLOR;
                _sfte_grid_scroll(ctx, 1);
                ctx->term.cur_bg = saved_bg;
            } else if (ctx->term.cursor_y < ctx->term.rows - 1)
                ctx->term.cursor_y++;
        } else
            ctx->term.cursor_x = ctx->term.cols - 1;
    }
}

// -------------------------------------------------------------------------------------------------
// >resize
// -------------------------------------------------------------------------------------------------

/*
    Performs a simple 2D truncation/padding copy of a grid.
    Used for alt-screens (which don't reflow) and as the primary resizer in non-reflow builds.
    Allocates and returns a new `sfte_cell` array. Caller assumes ownership.
*/
static sfte_cell *_sfte_grid_resize_dumb_copy(sfte_cell *old_grid, int old_cols, int old_rows,
                                              int new_cols, int new_rows) {
    if (!old_grid) return NULL;
    sfte_cell *new_grid = (sfte_cell *)SFTE_CALLOC(new_cols * new_rows, sizeof(sfte_cell));
    SFTE_ASSERT(new_grid, "failed to allocate resized grid");

    int min_cols = new_cols < old_cols ? new_cols : old_cols;
    int min_rows = new_rows < old_rows ? new_rows : old_rows;

    for (int r = 0; r < min_rows; ++r)
        for (int c = 0; c < min_cols; ++c) new_grid[r * new_cols + c] = old_grid[r * old_cols + c];

    return new_grid;
}

/*
    Reallocates the tab stops array and populates new columns with default intervals.
*/
static void _sfte_grid_resize_tabs(sfte_ctx *ctx, int old_cols, int new_cols) {
    uint8_t *new_tabs = (uint8_t *)SFTE_MALLOC(new_cols);
    SFTE_ASSERT(new_tabs, "failed to allocate new tab stops");
    for (int i = 0; i < new_cols; ++i) {
        if (i < old_cols)
            new_tabs[i] = ctx->term.tab_stops[i];
        else
            new_tabs[i] = (i % SFTE_TAB_WIDTH == 0);
    }
    SFTE_FREE(ctx->term.tab_stops);
    ctx->term.tab_stops = new_tabs;
}

/*
    Reallocates all terminal buffers (main, alt, scrollback, tab stops) to match new dimensions.
    Assumes ownership of freeing the old ctx->term.cells arrays.

    if SFTE_REFLOW is enabled, performs a topological wrap/unwrap of the main screen text.
    If SFTE_REFLOW is disabled, performs a simple 2D truncation/padding copy.
    Alt-screen grids are always dumb-copied and never reflowed.
*/
static void _sfte_grid_resize(sfte_ctx *ctx, int new_cols, int new_rows) {
    if (new_cols < 1 || new_rows < 1) return;
    int old_cols = ctx->term.cols;
    int old_rows = ctx->term.rows;

#if SFTE_ALT_SCREEN
    sfte_cell *main_old = ctx->term.alt_active ? ctx->term.alt_cells : ctx->term.cells;
    sfte_cell *alt_old = ctx->term.alt_active ? ctx->term.cells : NULL;
    int target_cx = ctx->term.alt_active ? ctx->term.saved_x[0] : ctx->term.cursor_x;
    int target_cy = ctx->term.alt_active ? ctx->term.saved_y[0] : ctx->term.cursor_y;
#else
    sfte_cell *main_old = ctx->term.cells;
    int target_cx = ctx->term.cursor_x;
    int target_cy = ctx->term.cursor_y;
#endif  // !SFTE_ALT_SCREEN

    _sfte_resize_buffers out = {0};

#if SFTE_REFLOW
    out = _sfte_reflow_generate_buffers(ctx, main_old, new_cols, new_rows, target_cx, target_cy);
#else  // !SFTE_REFLOW
    out.main_grid = _sfte_grid_resize_dumb_copy(main_old, old_cols, old_rows, new_cols, new_rows);
    out.new_cx = _SFTE_CLAMP(target_cx, 0, new_cols - 1);
    out.new_cy = _SFTE_CLAMP(target_cy, 0, new_rows - 1);
#if SFTE_SCROLLBACK_CAP
    out.sb_grid = (sfte_cell *)SFTE_CALLOC(ctx->term.sb_cap * new_cols, sizeof(sfte_cell));
    out.sb_lines = 0;
#endif  // SFTE_SCROLLBACK_CAP
#endif  // !SFTE_REFLOW

#if SFTE_ALT_SCREEN
    // Alt screen uses dumb copy, since it's never reflowed
    sfte_cell *new_alt = NULL;
    if (alt_old)
        new_alt = _sfte_grid_resize_dumb_copy(alt_old, old_cols, old_rows, new_cols, new_rows);
#endif  // SFTE_ALT_SCREEN

    SFTE_FREE(ctx->term.cells);
#if SFTE_ALT_SCREEN
    if (ctx->term.alt_cells) SFTE_FREE(ctx->term.alt_cells);
    ctx->term.cells = ctx->term.alt_active ? new_alt : out.main_grid;
    ctx->term.alt_cells = ctx->term.alt_active ? out.main_grid : NULL;

    if (ctx->term.alt_active) {
        ctx->term.saved_x[0] = out.new_cx;
        ctx->term.saved_y[0] = out.new_cy;
        ctx->term.cursor_x = _SFTE_CLAMP(ctx->term.cursor_x, 0, new_cols - 1);
        ctx->term.cursor_y = _SFTE_CLAMP(ctx->term.cursor_y, 0, new_rows - 1);
    } else {
        ctx->term.cursor_x = out.new_cx;
        ctx->term.cursor_y = out.new_cy;
    }
#else   // !SFTE_ALT_SCREEN
    ctx->term.cells = out.main_grid;
    ctx->term.cursor_x = st.new_cx;
    ctx->term.cursor_y = st.new_cy;
#endif  // !SFTE_ALT_SCREEN

#if SFTE_SCROLLBACK_CAP
    if (ctx->term.scrollback) SFTE_FREE(ctx->term.scrollback);
    ctx->term.scrollback = out.sb_grid;
    ctx->term.sb_head = out.sb_lines % ctx->term.sb_cap;
    ctx->term.sb_offset = 0;
    ctx->term.sb_len = out.sb_lines;
#endif  // SFTE_SCROLLBACK_CAP

    ctx->term.cols = new_cols;
    ctx->term.rows = new_rows;
    ctx->term.scroll_top = 0;
    ctx->term.scroll_bottom = new_rows - 1;

    _sfte_grid_resize_tabs(ctx, old_cols, new_cols);
    _sfte_grid_dirty_range(ctx, 0, new_cols * new_rows);
    _SFTE_INFO(ctx, TERM_RESIZE, new_cols, new_rows);
}
// =================================================================================================
// >>img
// =================================================================================================
#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
/*
    Inserts a new image into the global image pool.

    Takes ownership of `img.pxs`.
    If the pool is at maximum capacity, it safely frees `img.pxs` and returns NULL.

    WARN: The returned pointer points directly into a dynamic array.
    It WILL be invalidated the next time an image is inserted. Do not store this pointer.
*/
static inline sfte_img *_sfte_img_pool_insert(sfte_ctx *ctx, sfte_img img) {
    uint8_t oom = 0;
    _SFTE_MEM_ENSURE_CAP(sfte_img, ctx->term.img_pool, ctx->term.img_pool_len,
                         ctx->term.img_pool_cap, 1, SFTE_IMG_POOL_INIT_CAP, SFTE_IMG_POOL_MAX_CAP,
                         oom);
    if (oom) {
        if (img.pxs) SFTE_FREE(img.pxs);
        return NULL;
    }

    ctx->term.img_pool[ctx->term.img_pool_len] = img;
    return &ctx->term.img_pool[ctx->term.img_pool_len++];
}

/*
    Inserts a new image placement instruction into the global placements list.

    WARN: The returned pointer points directly into a dynamic array.
    It WILL be invalidated the next time a placement is inserted. Do not store this pointer.
*/
static inline sfte_img_placement *_sfte_img_placement_insert(sfte_ctx *ctx, sfte_img_placement p) {
    uint8_t oom = 0;
    _SFTE_MEM_ENSURE_CAP(sfte_img_placement, ctx->term.img_placements, ctx->term.img_placements_len,
                         ctx->term.img_placements_cap, 1, SFTE_IMG_PLACEMENT_INIT_CAP,
                         SFTE_IMG_PLACEMENT_MAX_CAP, oom);
    if (oom) return NULL;

    ctx->term.img_placements[ctx->term.img_placements_len] = p;
    return &ctx->term.img_placements[ctx->term.img_placements_len++];
}

/*
    Locates an image in the global pool by its ID.
    Returns NULL if the image was deleted.

    WARN: The returned pointer points directly into a dynamic array.
    It WILL be invalidated the next time a placement is inserted. Do not store this pointer.
*/
static inline sfte_img *_sfte_img_find(sfte_ctx *ctx, uint32_t id) {
    for (uint32_t i = 0; i < ctx->term.img_pool_len; ++i)
        if (ctx->term.img_pool[i].id == id) return &ctx->term.img_pool[i];
    return NULL;
}
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS
// =================================================================================================
// >>view
// =================================================================================================

/*
    Fills the padding regions around the terminal grid with the background color.
*/
static void _sfte_view_clear_padding_rects(sfte_ctx *ctx, uint32_t *px_buf) {
    int w = ctx->width;
    int h = ctx->height;
    int grid_w = ctx->term.cols * ctx->font.cell_width;
    int grid_h = ctx->term.rows * ctx->font.cell_height;
    uint32_t bg = (SFTE_BG_OPACITY << 24) | SFTE_BG_COLOR;

#if SFTE_PAD_Y
    for (int y = 0; y < SFTE_PAD_Y && y < h; ++y)
        for (int x = 0; x < w; ++x) px_buf[y * w + x] = bg;

    for (int y = SFTE_PAD_Y + grid_h; y < h; ++y)
        for (int x = 0; x < w; ++x) px_buf[y * w + x] = bg;
#endif  // SFTE_PAD_Y

#if SFTE_PAD_X
    for (int y = SFTE_PAD_Y; y < SFTE_PAD_Y + grid_h && y < h; ++y) {
        for (int x = 0; x < SFTE_PAD_X && x < w; ++x) px_buf[y * w + x] = bg;
        for (int x = SFTE_PAD_X + grid_w; x < w; ++x) px_buf[y * w + x] = bg;
    }
#endif  // SFTE_PAD_X
}

// =================================================================================================
// >>input
// =================================================================================================

#if SFTE_SELECTION
/*
    Determines if a specific cell falls within the active selection bounds.
    Evaluates against logical rows, meaning selections correclty scroll with text history.
*/
static inline int _sfte_input_is_selected(sfte_ctx *ctx, int c, int r) {
    if (!ctx->term.mouse_sel_active) return 0;

    int sc = ctx->term.mouse_sel_start_c;
    int sr = ctx->term.mouse_sel_start_r;
    int ec = ctx->term.mouse_sel_end_c;
    int er = ctx->term.mouse_sel_end_r;

    // Normalize backward drags
    if (sr > er || (sr == er && sc > ec)) {
        int tmp = sr;
        sr = er, er = tmp;
        tmp = sc, sc = ec, ec = tmp;
    }

    if (r < sr || r > er) return 0;
    if (sr == er) return (c >= sc && c <= ec);  // same line
    if (r == sr) return c >= sc;                // first line
    if (r == er) return c <= ec;                // last line
    return 1;                                   // middle lines
}
#endif  // SFTE_SELECTION

#if SFTE_MOUSE
// Tracking modes (DECSET)
#define SFTE_MOUSE_MODE_CLICK 1000   // Report button press/release only
#define SFTE_MOUSE_MODE_DRAG 1002    // Report clicks and drag motion
#define SFTE_MOUSE_MODE_MOTION 1003  // Report all hover and drag motion

// Formatting extensions (DECSET)
#define SFTE_MOUSE_EXT_DEFAULT 0  // Legacy X10 encoding
#define SFTE_MOUSE_EXT_SGR 1006   // Modern SGR encoding

// Encoding offsets
#define SFTE_MOUSE_BTN_RELEASE 3     // The default "button released" state in legacy modes
#define SFTE_MOUSE_MOTION_OFFSET 32  // Added to the button state to indicate motion/dragging
#define SFTE_MOUSE_X10_OFFSET 32  // Added to coords to ensure they are printable ASCII characters
#define SFTE_MOUSE_X10_MAX_COORD 223  // 255 (max byte) - 32 (offset)

/*
    Encodes and flushes mouse events back to the host application via terminal escape sequences.
    Supports both legacy X10 (max coords 223) and modern SGR 1006 formats.
    Must be fed `screen_r` coordinates, never `logical_r` coordinates.
*/
static void _sfte_input_send_mouse_event(sfte_ctx *ctx, int btn, int is_release, int c, int r,
                                         int is_motion) {
    if (!ctx->term.mouse_mode) return;

    int encoded_btn = btn;
    // Legacy modes cannot encode which button was released
    if (is_release && ctx->term.mouse_ext != SFTE_MOUSE_EXT_SGR) {
        encoded_btn = SFTE_MOUSE_BTN_RELEASE;
    }

    if (is_motion) {
        if (ctx->term.mouse_mode == SFTE_MOUSE_MODE_DRAG &&
            ctx->term.mouse_btn_state != SFTE_MOUSE_BTN_RELEASE)
            encoded_btn = ctx->term.mouse_btn_state + SFTE_MOUSE_MOTION_OFFSET;  // Dragging
        else if (ctx->term.mouse_mode == SFTE_MOUSE_MODE_MOTION)
            encoded_btn = ctx->term.mouse_btn_state + SFTE_MOUSE_MOTION_OFFSET;  // Hover/dragging
        else
            return;  // Mode 1000 ignores motion
    }

    char buf[16];
    int len = 0;
    int tc = c + 1;
    int tr = r + 1;  // 1-based indexing for terminal escape sequences

    if (ctx->term.mouse_ext == SFTE_MOUSE_EXT_SGR) {
        // SGR: ESC [ < btn ; x ; y M/m
        char end_char = is_release ? 'm' : 'M';
        len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c", encoded_btn, tc, tr, end_char);
    } else {
        // X10: ESC [ <btn+32> <x+32> <y+32>
        if (tc > SFTE_MOUSE_X10_MAX_COORD || tr > SFTE_MOUSE_X10_MAX_COORD) return;
        len = snprintf(buf, sizeof(buf), "\033[M%c%c%c", encoded_btn + SFTE_MOUSE_X10_OFFSET,
                       tc + SFTE_MOUSE_X10_OFFSET, tr + SFTE_MOUSE_X10_OFFSET);
    }

    if (ctx->write_cb) ctx->write_cb(ctx->user_data, buf, len);
}
#endif  // SFTE_MOUSE

// =================================================================================================
// >>reflow
// =================================================================================================
#if SFTE_REFLOW
/*
    Pushes a single cell into the temporary linear buffer.
*/
static void _sfte_reflow_push(_sfte_reflow_state *st, sfte_cell c, int is_cursor) {
#if SFTE_WIDE_CHARS
    // If we're pushing a double-width character and we're at last column, wrap early.
    if ((c.attr & ATTR_WIDE) && st->tc == st->new_cols - 1) {
        sfte_cell space = c;
        space.rune = ' ';
        space.fg = 0xFFFFFF;
        space.bg = SFTE_BG_COLOR;
        space.attr = 0;
        space.wrapped = 1;
        st->temp_rows[st->tr * st->new_cols + st->tc] = space;
        st->tc = 0;
        st->tr++;
    }
#endif  // SFTE_WIDE_CHARS

    if (st->tc == st->new_cols) {
        st->temp_rows[st->tr * st->new_cols + st->new_cols - 1].wrapped = 1;
        st->tc = 0;
        st->tr++;
    }

    if (is_cursor) {
        st->new_cx = st->tc;
        st->new_cy = st->tr;
    }

    c.wrapped = 0;
    st->temp_rows[st->tr * st->new_cols + st->tc] = c;
    st->tc++;
}

/*
    Calculates the true length of a line by trimming empty trailing spaces.
    If the cursor is on this line (cursor_cx >= 0), it ensures the length includes the cursor.
*/
static inline int _sfte_reflow_get_len(sfte_cell *row, int cols, int cursor_cx) {
    if (row[cols - 1].wrapped) return cols;
    int len = cols;

    // Terminals pad lines with empty cells. When resizing, we must trim these
    // to prevent invisible padding from wrapping and creating artificial blank lines.
    // However, a cell is only truly empty if:
    // 1. it contains a space or null rune,
    // 2. its bg is the default color (colored spaces are used by TUIs to draw UI).
    // Finally, we must NEVER trim the cell where the cursor is currently sitting,
    // even if its a blank space.
    while (len > 0 && (row[len - 1].rune == ' ' || row[len - 1].rune == 0) &&
           row[len - 1].bg == SFTE_BG_COLOR) {
        if (cursor_cx >= 0 && len - 1 == cursor_cx) break;
        len--;
    }

    // Ensure the cursor isn't trimmed out.
    // Handles cases where cursor_cx is beyond the actual string length, e.g., wrap pending states.
    if (cursor_cx >= 0 && len <= cursor_cx) len = cursor_cx + 1;

    return len;
}

/*
    Processes a single row, extracting length, pushing cells, and handling cursor edge cases.
*/
static void _sfte_reflow_process_row(sfte_cell *row, int cols, int cursor_cx,
                                     _sfte_reflow_state *st) {
    int len = _sfte_reflow_get_len(row, cols, cursor_cx);

    for (int c = 0; c < len; ++c) _sfte_reflow_push(st, row[c], c == cursor_cx);

    if (cursor_cx >= 0 && cursor_cx >= len) {
        if (st->tc == st->new_cols) {
            st->temp_rows[st->tr * st->new_cols + st->new_cols - 1].wrapped = 1;
            st->tc = 0;
            st->tr++;
        }
        st->new_cx = st->tc;
        st->new_cy = st->tr;
    }

    if (!row[cols - 1].wrapped) {
        st->tc = 0;
        st->tr++;
    }
}

/*
    Flattens the terminals history (both the scrollback ring buffer and active 2D grid)
    into a single continuous 1D stream.

    To wrap text accurately across the boundaries of the screen and the scrollback, it
    must evaluate the text from the oldest recorded line down to the newest.
    Because the scrollback is a circular array, we must do modular rithmetic backwards
    from `sb_head` to read it in chronological order.
    The active cursor can only exist on the live screen, so we pass `-1`
    during scrollback iteration to explicitly trim trailing whitespace on all historical lines.
*/
static void _sfte_reflow_grid_into_linear(sfte_ctx *ctx, sfte_cell *main_old,
                                          _sfte_reflow_state *st) {
#if SFTE_SCROLLBACK_CAP
    for (int i = 0; i < ctx->term.sb_len; ++i) {
        int ring_idx = (ctx->term.sb_head - ctx->term.sb_len + i + ctx->term.sb_cap) %
                       ctx->term.sb_cap;
        sfte_cell *row = &ctx->term.scrollback[ring_idx * ctx->term.cols];
        _sfte_reflow_process_row(row, ctx->term.cols, -1, st);
    }
#endif  // SFTE_SCROLLBACK_CAP

    // reflow live grid
    st->is_live = 1;
    for (int r = 0; r < ctx->term.rows; ++r) {
        sfte_cell *row = &main_old[r * ctx->term.cols];
        int cursor_cx = (r == st->target_old_cy) ? st->target_old_cx : -1;

        _sfte_reflow_process_row(row, ctx->term.cols, cursor_cx, st);
    }
}

/*
    Allocates the temporary linear buffer and performs the topological text reflow.
*/
static sfte_cell *_sfte_reflow_linearize(sfte_ctx *ctx, sfte_cell *main_old, int new_cols,
                                         int new_rows, _sfte_reflow_state *st) {
    // ctx->term.cols / new_cols calculates the raw expansion factor.
    // We add +2 to to this multiplier:
    // +1 to account for integer division truncation, and
    // +1 as a safety margin for double-width characters that force early wraps.
    int max_temp_rows = (
#if SFTE_SCROLLBACK_CAP
                            ctx->term.sb_len +
#endif  // SFTE_SCROLLBACK_CAP
                            ctx->term.rows) *
                        (ctx->term.cols / new_cols + 2);

    if (max_temp_rows < new_rows) max_temp_rows = new_rows;

    sfte_cell *temp_rows = (sfte_cell *)SFTE_CALLOC(max_temp_rows * new_cols, sizeof(sfte_cell));
    SFTE_ASSERT(temp_rows, "failed to allocate temporary row data");

    st->temp_rows = temp_rows;
    st->new_cols = new_cols;
    st->tr = 0, st->tc = 0, st->new_cx = 0, st->new_cy = 0;
    st->is_live = 0;

    _sfte_reflow_grid_into_linear(ctx, main_old, st);
    return temp_rows;
}

/*
    Maps the linearized reflow buffer back into distinct 2D main and scrollback grids.
    Modifies `st->new_cy` to reflect its clamped viewport position.
*/
static void _sfte_reflow_extract_view(sfte_ctx *ctx, int new_cols, int new_rows, int target_cy,
                                      _sfte_reflow_state *st, _sfte_resize_buffers *out) {
    int total_lines = st->tr + (st->tc > 0 ? 1 : 0);

    out->main_grid = (sfte_cell *)SFTE_CALLOC(new_cols * new_rows, sizeof(sfte_cell));
    SFTE_ASSERT(out->main_grid, "failed to allocate resized terminal grid");

    // Clamp the cursors target row to the new screen height
    int cy = target_cy < new_rows ? target_cy : new_rows - 1;

    // Map the viewport to match the cursors visual row
    int screen_top = st->new_cy - cy;

    // Clamp the viewport to the absolute limits of the text buffer
    int max_top = total_lines - new_rows;
    if (max_top < 0) max_top = 0;

    screen_top = _SFTE_CLAMP(screen_top, 0, max_top);

#if SFTE_SCROLLBACK_CAP
    out->sb_grid = (sfte_cell *)SFTE_CALLOC(ctx->term.sb_cap * new_cols, sizeof(sfte_cell));
    SFTE_ASSERT(out->sb_grid, "failed to allocate resized scrollback");

    out->sb_lines = screen_top;
    if (out->sb_lines > ctx->term.sb_cap) out->sb_lines = ctx->term.sb_cap;
    int sb_start = screen_top - out->sb_lines;

    for (int i = 0; i < out->sb_lines; ++i)
        memcpy(&out->sb_grid[i * new_cols], &st->temp_rows[(sb_start + i) * new_cols],
               new_cols * sizeof(sfte_cell));
#endif  // SFTE_SCROLLBACK_CAP

    int copy_lines = _SFTE_CLAMP(total_lines - screen_top, 0, new_rows);
    for (int i = 0; i < copy_lines; ++i)
        memcpy(&out->main_grid[i * new_cols], &st->temp_rows[(screen_top + i) * new_cols],
               new_cols * sizeof(sfte_cell));

    for (int i = copy_lines * new_cols; i < new_rows * new_cols; ++i) {
        out->main_grid[i].rune = ' ';
        out->main_grid[i].bg = SFTE_BG_COLOR;
        out->main_grid[i].fg = 0xFFFFFF;
        out->main_grid[i].attr = 0;
        out->main_grid[i].wrapped = 0;
    }

    st->new_cy = _SFTE_CLAMP(st->new_cy - screen_top, 0, new_rows - 1);
}

/*
    Orchestrates the reflow pipeline and returns the newly allocated grid buffers.
*/
static _sfte_resize_buffers _sfte_reflow_generate_buffers(sfte_ctx *ctx, sfte_cell *main_old,
                                                          int new_cols, int new_rows, int target_cx,
                                                          int target_cy) {
    _sfte_reflow_state st = {
        .target_old_cx = target_cx,
        .target_old_cy = target_cy,
    };

    sfte_cell *temp = _sfte_reflow_linearize(ctx, main_old, new_cols, new_rows, &st);

    _sfte_resize_buffers out = {0};
    _sfte_reflow_extract_view(ctx, new_cols, new_rows, target_cy, &st, &out);

    SFTE_FREE(temp);

    out.new_cx = st.new_cx;
    out.new_cy = st.new_cy;

    return out;
}
#endif  // SFTE_REFLOW
// =================================================================================================
// >>sixel
// =================================================================================================
#if SFTE_SIXEL

#define _SFTE_SIXEL_OFFSET 63         // ASCII offset ('?' is 63)
#define _SFTE_SIXEL_BAND_HEIGHT 6     // Each sixel row represents 6 vertical pixels
#define _SFTE_SIXEL_MAX_PARAMS 5      // Max parameters in a color definition
#define _SFTE_SIXEL_RGB_MAX 100       // RGB values are set as percentages
#define _SFTE_SIXEL_COLORSPACE_HLS 1  // HLS color space identifier
#define _SFTE_SIXEL_COLORSPACE_RGB 2  // RGB color space identifier

/*
    Finalizes a completed sixel sequence.

    NOTE:
    Sixel requires the text cursor to be pushed to the beginning of next line
    after image finishes drawing, scrolling the screen if necessary.
*/
static inline void _sfte_sixel_commit(sfte_ctx *ctx) {
    if (ctx->sixel.width > 0 && ctx->sixel.height > 0) {
        uint32_t *final_pxs = (uint32_t *)SFTE_MALLOC(ctx->sixel.width * ctx->sixel.height *
                                                      sizeof(uint32_t));
        for (int y = 0; y < ctx->sixel.height; ++y)
            memcpy(&final_pxs[y * ctx->sixel.width], &ctx->sixel.pxs[y * ctx->sixel.cap_w],
                   ctx->sixel.width * sizeof(uint32_t));
        SFTE_FREE(ctx->sixel.pxs);

        sfte_img *img = _sfte_img_pool_insert(ctx, (sfte_img){
                                                       .id = ++ctx->term.next_img_id,
                                                       .width = ctx->sixel.width,
                                                       .height = ctx->sixel.height,
                                                       .pxs = final_pxs,
                                                       .is_sixel = 1,
                                                   });
        if (img) {
            sfte_img_placement *p = _sfte_img_placement_insert(
                ctx, (sfte_img_placement){
                         .img_id = img->id,
                         .start_col = ctx->sixel.start_col,
                         .start_row = ctx->sixel.start_row,
                         .z_idx = 1,
                         .is_sixel = 1,
                         .alt_screen = ctx->term.alt_active,
                     });
            if (p) {
                img->ref_cnt++;

                int rows = _sfte_grid_span(ctx->sixel.height, p->y_off, ctx->font.cell_height);
                if (ctx->sixel.height % ctx->font.cell_height != 0) rows++;

                // Force the text cursor below the placed image
                ctx->term.cursor_x = 0;
                ctx->term.cursor_y = ctx->sixel.start_row + rows;

                // If the image pushed the cursor off the screen, scroll down
                while (ctx->term.cursor_y > ctx->term.scroll_bottom) {
                    _sfte_grid_scroll(ctx, 1);
                    ctx->term.cursor_y--;
                }
            } else
                SFTE_FREE(final_pxs);
        } else
            SFTE_FREE(final_pxs);
    }

    ctx->sixel.pxs = NULL;
    ctx->sixel.cap_w = 0;
    ctx->sixel.cap_h = 0;
    ctx->sixel.width = 0;
    ctx->sixel.height = 0;
}

/*
    Ensures the image buffer is large enough for the incoming pixels.
    Size gets doubled on each dimension when the buffer is not big enough.
    Maxes out at `SFTE_SIXEL_MAX_SIZE` x `SFTE_SIXEL_MAX_SIZE`.
*/
static void _sfte_sixel_ensure_cap(sfte_ctx *ctx, int req_w, int req_h) {
    if (req_w < ctx->sixel.cap_w && req_h < ctx->sixel.cap_h) return;

    int new_w = ctx->sixel.cap_w == 0 ? SFTE_SIXEL_INIT_SIZE : ctx->sixel.cap_w;
    int new_h = ctx->sixel.cap_h == 0 ? SFTE_SIXEL_INIT_SIZE : ctx->sixel.cap_h;

    while (new_w >= req_w && new_w < SFTE_SIXEL_MAX_SIZE) new_w *= 2;
    while (new_h >= req_h && new_h < SFTE_SIXEL_MAX_SIZE) new_h *= 2;

    if (new_w > SFTE_SIXEL_MAX_SIZE) new_w = SFTE_SIXEL_MAX_SIZE;
    if (new_h > SFTE_SIXEL_MAX_SIZE) new_h = SFTE_SIXEL_MAX_SIZE;

    if (new_w <= ctx->sixel.cap_w && new_h <= ctx->sixel.cap_h) return;

    uint32_t *new_pxs = (uint32_t *)SFTE_CALLOC(new_w * new_h, sizeof(uint32_t));
    SFTE_ASSERT(new_pxs, "failed to allocate sixel buffer");

    if (ctx->sixel.pxs) {
        for (int r = 0; r < ctx->sixel.height; ++r)
            memcpy(&new_pxs[r * new_w], &ctx->sixel.pxs[r * ctx->sixel.cap_w],
                   ctx->sixel.width * sizeof(uint32_t));
        SFTE_FREE(ctx->sixel.pxs);
    }

    ctx->sixel.pxs = new_pxs;
    ctx->sixel.cap_w = new_w;
    ctx->sixel.cap_h = new_h;
}

/*
    Decodes a 6-bit bitmask and paints it onto the image buffer.
    Handles buffer resizing if it's too small.

    A sixel is a vertical column of 6 pixels encoded into a single ASCII character.
    The bits (from LSB to MSB) represent pixels from top to bottom.
    ASCII '?' (value 63) minus offset 63 = 000000 (all blank).
    ASCII '~' (value 126) minus offset 63 = 111111 (all solid).
*/
static void _sfte_sixel_draw_pattern(sfte_ctx *ctx, uint8_t pattern, int repeats) {
    int max_x = ctx->sixel.x + repeats - 1;
    int max_y = ctx->sixel.y + _SFTE_SIXEL_BAND_HEIGHT - 1;

    _sfte_sixel_ensure_cap(ctx, max_x, max_y);
    uint32_t col = ctx->sixel.palette[ctx->sixel.col_idx];

    for (int dx = 0; dx < repeats; ++dx) {
        int px_x = ctx->sixel.x + dx;

        for (uint8_t bit = 0; bit < _SFTE_SIXEL_BAND_HEIGHT; ++bit) {
            if (!(pattern & (1 << bit))) continue;
            int px_y = ctx->sixel.y + bit;

            // Skip invalid sequence requests
            if (px_x >= ctx->sixel.cap_w || px_y >= ctx->sixel.cap_h) continue;

            ctx->sixel.pxs[px_y * ctx->sixel.cap_w + px_x] = col;

            if (px_x >= ctx->sixel.width) ctx->sixel.width = px_x + 1;
            if (px_y >= ctx->sixel.height) ctx->sixel.height = px_y + 1;
        }
    }

    ctx->sixel.x += repeats;
}

/*
    Converts a hue angle into a single RGB channel.
*/
static inline float _sfte_sixel_hue_to_rgb(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

/*
    Converts HLS values to a packed 32-bit ARGB color.
    Unlike modern HSL, sixel uses integer degrees for hue (0-360) and percentages for L/S (0-100).
*/
static inline uint32_t _sfte_sixel_hls_to_rgb(uint16_t h_deg, uint16_t l_pct, uint16_t s_pct) {
    float h = h_deg / 360.0f;
    float l = l_pct / 100.0f;
    float s = s_pct / 100.0f;

    uint8_t r, g, b;

    if (s == 0.0f)
        r = g = b = (uint8_t)(l * 255.0f);
    else {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        r = (uint8_t)(_sfte_sixel_hue_to_rgb(p, q, h + 1.0f / 3.0f) * 255.0f);
        g = (uint8_t)(_sfte_sixel_hue_to_rgb(p, q, h) * 255.0f);
        b = (uint8_t)(_sfte_sixel_hue_to_rgb(p, q, h - 1.0f / 3.0f) * 255.0f);
    }

    return SFTE_COLOR_ALPHA_MASK | (r << 16) | (g << 8) | b;
}

/*
    Parses the accumulated numerical parameters to update the active color or palette.
    Initiated by the pound symbol (`#`).
    If one parameter is provided, the active color index gets changed to value of that parameter.
    If five parameters are provided, it defines a new color.
    Parameter values are as follows: #<idx>;<space>;<c1>;<c2>;<c3>.
    Color space is 1 for HLS, 2 for RGB.
*/
static void _sfte_sixel_apply_color(sfte_ctx *ctx) {
    if (ctx->sixel.param_idx == 0 && ctx->sixel.params[0] < 256)
        ctx->sixel.col_idx = ctx->sixel.params[0];
    else if (ctx->sixel.param_idx == 4) {
        int idx = ctx->sixel.params[0];
        int space = ctx->sixel.params[1];

        if (idx >= 0 && idx <= 256) {
            if (space == _SFTE_SIXEL_COLORSPACE_HLS)
                ctx->sixel.palette[idx] = _sfte_sixel_hls_to_rgb(
                    ctx->sixel.params[2], ctx->sixel.params[3], ctx->sixel.params[4]);
            else if (space == _SFTE_SIXEL_COLORSPACE_RGB) {
                uint8_t r = (ctx->sixel.params[2] * 255) / _SFTE_SIXEL_RGB_MAX;
                uint8_t g = (ctx->sixel.params[3] * 255) / _SFTE_SIXEL_RGB_MAX;
                uint8_t b = (ctx->sixel.params[4] * 255) / _SFTE_SIXEL_RGB_MAX;
                ctx->sixel.palette[idx] = SFTE_COLOR_ALPHA_MASK | (r << 16) | (g << 8) | b;
            }
        }
    }
}

/*
    The core state machine for the sixel byte stream.

    When parsing repeats (e.g. `!255`) or colors (e.g. `#1;2;100;0;0`), the terminal
    receives ASCII digits one at a time. To convert these characters into integers
    without external buffers, we use:
        `acc = acc * 10 + (b - '0')`
    `b - '0'` subtracts 48 to convert an ASCII character (e.g. `5`) to an integer (5).
    `acc * 10` shifts the previously parsed value one decimal place to the left.

    To avoid growing the call stack via recursion when a state
    needs to hand a byte back to SIXEL_GROUND, it uses a while-loop based on the `cont` variable.
*/
static void _sfte_sixel_parse_byte(sfte_ctx *ctx, uint8_t b) {
    uint8_t cont = 1;
    while (cont) {
        cont = 0;

        switch (ctx->sixel.state) {
        case SIXEL_GROUND:
            if (b >= '?' && b <= '~') {
                uint8_t pattern = b - _SFTE_SIXEL_OFFSET;
                int repeats = ctx->sixel.repeat_cnt > 0 ? ctx->sixel.repeat_cnt : 1;
                _sfte_sixel_draw_pattern(ctx, pattern, repeats);
                ctx->sixel.repeat_cnt = 0;
            } else if (b == '$')  // Carriage return
                ctx->sixel.x = 0;
            else if (b == '-') {  // Move down one band
                ctx->sixel.x = 0;
                ctx->sixel.y += _SFTE_SIXEL_BAND_HEIGHT;
            } else if (b == '!') {  // Start repeat sequence
                ctx->sixel.state = SIXEL_REPEAT;
                ctx->sixel.repeat_cnt = 0;
            } else if (b == '#') {  // Start color definition/selection
                ctx->sixel.state = SIXEL_COLOR_PARAM;
                ctx->sixel.param_idx = 0;
                memset(ctx->sixel.params, 0, sizeof(ctx->sixel.params));
            }
            break;
        case SIXEL_REPEAT:
            if (b >= '0' && b <= '9')
                ctx->sixel.repeat_cnt = ctx->sixel.repeat_cnt * 10 + (b - '0');
            else {
                // Done parsing repeat count, go back to ground state
                ctx->sixel.state = SIXEL_GROUND;
                cont = 1;
            }
            break;
        case SIXEL_COLOR_INTRO:
        case SIXEL_COLOR_PARAM:
            if (b >= '0' && b <= '9')
                ctx->sixel.params[ctx->sixel.param_idx] = ctx->sixel.params[ctx->sixel.param_idx] *
                                                              10 +
                                                          (b - '0');
            else if (b == ';' && ctx->sixel.param_idx < _SFTE_SIXEL_MAX_PARAMS - 1)
                // Move to next parameter
                ctx->sixel.param_idx++;
            else {  // Color sequence terminated by any non-digit/semicolon byte
                _sfte_sixel_apply_color(ctx);
                ctx->sixel.state = SIXEL_GROUND;
                cont = 1;
            }
            break;
        }
    }
}

/*
    Frees all sixel memory allocations.
*/
static void _sfte_sixel_deinit(sfte_ctx *ctx) {
    if (ctx->term.img_pool) {
        for (uint32_t i = 0; i < ctx->term.img_pool_len; ++i)
            if (ctx->term.img_pool[i].pxs) SFTE_FREE(ctx->term.img_pool[i].pxs);
        SFTE_FREE(ctx->term.img_pool);
        ctx->term.img_pool = NULL;
    }
    if (ctx->term.img_placements) {
        SFTE_FREE(ctx->term.img_placements);
        ctx->term.img_placements = NULL;
    }
    if (ctx->sixel.pxs) {
        SFTE_FREE(ctx->sixel.pxs);
        ctx->sixel.pxs = NULL;
    }
}
#endif  // SFTE_SIXEL
// =================================================================================================
// >>kitty
// =================================================================================================
#if SFTE_KITTY_GRAPHICS

/*
    Performs bilinear interpolation for image scaling.
    Kitty allows the terminal to dictate the final render size in rows/columns,
    this is the main purpose for this function.
*/
static uint32_t *_sfte_kitty_scale_image_bilinear(uint32_t *src, int sw, int sh, int dw, int dh) {
    uint32_t *dst = (uint32_t *)SFTE_MALLOC(dw * dh * sizeof(uint32_t));
    if (!dst) return NULL;

    float x_ratio = ((float)(sw - 1)) / dw;
    float y_ratio = ((float)(sh - 1)) / dh;

    for (int i = 0; i < dh; ++i)
        for (int j = 0; j < dw; ++j) {
            int x = (int)(x_ratio * j);
            int y = (int)(y_ratio * i);
            float x_diff = (x_ratio * j) - x;
            float y_diff = (y_ratio * i) - y;
            // 4 nearest pxs
            int idx = y * sw + x;
            uint32_t p1 = src[idx];
            uint32_t p2 = (x + 1 < sw) ? src[idx + 1] : p1;
            uint32_t p3 = (y + 1 < sh) ? src[idx + sw] : p1;
            uint32_t p4 = (x + 1 < sw && y + 1 < sh) ? src[idx + sw + 1] : p1;
            // weights
            float w1 = (1.0f - x_diff) * (1.0f - y_diff);
            float w2 = x_diff * (1.0f - y_diff);
            float w3 = (1.0f - x_diff) * y_diff;
            float w4 = x_diff * y_diff;
            // interpolate
            uint32_t r = (uint32_t)(((p1 >> 16) & 0xFF) * w1 + ((p2 >> 16) & 0xFF) * w2 +
                                    ((p3 >> 16) & 0xFF) * w3 + ((p4 >> 16) & 0xFF) * w4);
            uint32_t g = (uint32_t)(((p1 >> 8) & 0xFF) * w1 + ((p2 >> 8) & 0xFF) * w2 +
                                    ((p3 >> 8) & 0xFF) * w3 + ((p4 >> 8) & 0xFF) * w4);
            uint32_t b = (uint32_t)((p1 & 0xFF) * w1 + (p2 & 0xFF) * w2 + (p3 & 0xFF) * w3 +
                                    (p4 & 0xFF) * w4);
            uint32_t a = (uint32_t)(((p1 >> 24) & 0xFF) * w1 + ((p2 >> 24) & 0xFF) * w2 +
                                    ((p3 >> 24) & 0xFF) * w3 + ((p4 >> 24) & 0xFF) * w4);
            // store
            dst[i * dw + j] = (a << 24) | (r << 16) | (g << 8) | b;
        }

    return dst;
}

/*
    Parses raw pixel buffers or delegates to stb_image for PNG/JPEG decoding.
    This kitty protocol implementation supports:
    - direct base64 pixel streams (via 'd'),
    - reading from a local file path (via 'f', used by e.g. yazi),
    - reading from a temporary file that the terminal is expected to delete after reading (via 't').
*/
static uint32_t *_sfte_kitty_decode_payload(sfte_ctx *ctx, uint8_t *raw_data, size_t raw_len,
                                            uint8_t is_file, const char *file_path, int *w,
                                            int *h) {
    uint32_t *pxs = NULL;

    if (ctx->kitty.format == SFTE_KITTY_FMT_PNG_JPEG) {
        int channels = 0;
        uint8_t *stb_pxs = is_file ? stbi_load(file_path, w, h, &channels, 4)
                                   : stbi_load_from_memory(raw_data, raw_len, w, h, &channels, 4);

        if (stb_pxs && *w && *h) {
            pxs = (uint32_t *)SFTE_MALLOC(*w * *h * sizeof(uint32_t));
            for (int i = 0; i < *w * *h; ++i)
                pxs[i] = (stb_pxs[i * 4 + 3] << 24) | (stb_pxs[i * 4 + 0] << 16) |
                         (stb_pxs[i * 4 + 1] << 8) | stb_pxs[i * 4 + 2];
            stbi_image_free(stb_pxs);
        }
    } else if ((ctx->kitty.format == SFTE_KITTY_FMT_RGB ||
                ctx->kitty.format == SFTE_KITTY_FMT_RGBA) &&
               *w && *h) {
        int bpp = (ctx->kitty.format == SFTE_KITTY_FMT_RGB) ? 3 : 4;
        uint8_t *pixel_src = raw_data;
        size_t pixel_len = raw_len;

        if (is_file) {
            FILE *f = fopen(file_path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                pixel_len = ftell(f);
                fseek(f, 0, SEEK_SET);
                pixel_src = (uint8_t *)SFTE_MALLOC(pixel_len);
                fread(pixel_src, 1, pixel_len, f);
                fclose(f);
            } else
                pixel_src = NULL;
        }

        if (pixel_src && raw_len >= (size_t)(*w * *h * bpp)) {
            pxs = (uint32_t *)SFTE_MALLOC(*w * *h * sizeof(uint32_t));
            for (int i = 0; i < *w * *h; ++i) {
                uint8_t a = (bpp == 4) ? pixel_src[i * bpp + 3] : 255;
                pxs[i] = (a << 24) | (pixel_src[i * bpp + 0] << 16) |
                         (pixel_src[i * bpp + 1] << 8) | pixel_src[i * bpp + 2];
            }
        }
        if (is_file && pixel_src) SFTE_FREE(pixel_src);
    }
    return pxs;
}

/*
    Applies requested croppping limits before placing the image.
*/
static uint32_t *_sfte_kitty_apply_crop(sfte_ctx *ctx, uint32_t *pxs, int *w, int *h) {
    int cx = (unsigned int)_SFTE_CLAMP(ctx->kitty.crop_x, 0, *w);
    int cy = (unsigned int)_SFTE_CLAMP(ctx->kitty.crop_y, 0, *h);
    int cw = ctx->kitty.crop_w ? ctx->kitty.crop_w : (*w - cx);
    int ch = ctx->kitty.crop_h ? ctx->kitty.crop_h : (*h - cy);
    if (cx + cw > *w) cw = *w - cx;
    if (cy + ch > *h) ch = *h - cy;
    if (cw == *w && ch == *h && !cx && !cy) return pxs;
    if (cw <= 0 || ch <= 0) {
        SFTE_FREE(pxs);
        return NULL;
    }
    uint32_t *cropped = (uint32_t *)SFTE_MALLOC(cw * ch * sizeof(uint32_t));
    if (cropped) {
        for (int y = 0; y < ch; ++y)
            memcpy(&cropped[y * cw], &pxs[(cy + y) * *w + cx], cw * sizeof(uint32_t));
        *w = cw;
        *h = ch;
    }
    SFTE_FREE(pxs);
    return cropped;
}

/*
    Translates terminal cell bounds (columns/rows) into physical
    pixel dimensions, and scales the raster to match.
*/
static uint32_t *_sfte_kitty_apply_scale(sfte_ctx *ctx, uint32_t *pxs, int *w, int *h) {
    if (ctx->kitty.cols <= 0 && ctx->kitty.rows <= 0) return pxs;
    int target_w = *w;
    int target_h = *h;
    if (ctx->kitty.cols && !ctx->kitty.rows) {
        target_w = ctx->kitty.cols * ctx->font.cell_width;
        target_h = (target_w * *h) / *w;  // Maintain aspect ratio
    } else if (!ctx->kitty.cols && ctx->kitty.rows) {
        target_h = ctx->kitty.rows * ctx->font.cell_height;
        target_w = (target_h * *w) / *h;  // Maintain aspect ratio
    } else {
        target_w = ctx->kitty.cols * ctx->font.cell_width;
        target_h = ctx->kitty.rows * ctx->font.cell_height;
    }

    if (target_w <= 0 || target_h <= 0 || (target_w == *w && target_h == *h)) return pxs;

    uint32_t *scaled = _sfte_kitty_scale_image_bilinear(pxs, *w, *h, target_w, target_h);
    if (scaled) {
        SFTE_FREE(pxs);
        *w = target_w;
        *h = target_h;
        return scaled;
    }

    // Fall back to returning the unscaled image on 'scaled' allocation fail
    return pxs;
}

/*
    Evaluates the kitty deletion matrix to determine if a specific
    placement should be destroyed. This protocol implementation allows wiping by:
    - ID,
    - Z index,
    - viewport intersection,
    - specific cursor locations.
*/
static uint8_t _sfte_kitty_should_delete(sfte_ctx *ctx, sfte_img_placement *p, sfte_img *img) {
    uint8_t matches_id = (!ctx->kitty.id || ctx->kitty.id == p->img_id);
    if (!matches_id) return 0;
    int cols = _sfte_grid_span(img->width, p->x_off, ctx->font.cell_width);
    int rows = _sfte_grid_span(img->height, p->y_off, ctx->font.cell_height);
    int target_c = ctx->kitty.crop_x - 1;
    int target_r = ctx->kitty.crop_y - 1;
    uint8_t intersects_x = (target_c >= p->start_col && target_c < p->start_col + cols);
    uint8_t intersects_y = (target_r >= p->start_row && target_r < p->start_row + rows);
    switch (ctx->kitty.d_action) {
    case 'A':
    case 'a': return 1;  // Delete all
    case 'I':
    case 'i': return !ctx->kitty.placement_id || ctx->kitty.placement_id == p->placement_id;
    case 'C':
    case 'c':  // Delete if intersecting cursor
        return ctx->term.cursor_x >= p->start_col && ctx->term.cursor_x < p->start_col + cols &&
               ctx->term.cursor_y >= p->start_row && ctx->term.cursor_y < p->start_row + rows;
    case 'P':
    case 'p': return intersects_x && intersects_y;
    case 'X':
    case 'x': return intersects_x;
    case 'Y':
    case 'y': return intersects_y;
    case 'Z':
    case 'z': return p->z_idx == ctx->kitty.z_idx;
    case 'Q':
    case 'q': return intersects_x && intersects_y && p->z_idx == ctx->kitty.z_idx;
    default: return 0;
    }
}

/*
    Garbage collection via a simple reference counting.
    The image data is separated from image placements, we only free the image data
    when its `ref_cnt` drops to 0 (no placements are actively displaying it).
*/
static void _sfte_kitty_gc_pool(sfte_ctx *ctx) {
    for (uint32_t i = 0; i < ctx->term.img_pool_len; ++i) {
        sfte_img *img = &ctx->term.img_pool[i];
        if (img->is_sixel || img->ref_cnt) continue;  // Skip active and sixel
        uint8_t should_del = (ctx->kitty.d_action == 'A' || ctx->kitty.d_action == 'a') ||
                             ((ctx->kitty.d_action == 'I' || ctx->kitty.d_action == 'i') &&
                              img->id == ctx->kitty.id);
        if (!should_del) continue;
        if (img->pxs) SFTE_FREE(img->pxs);
        ctx->term.img_pool[i--] = ctx->term.img_pool[--ctx->term.img_pool_len];
    }
}

/*
    Registers a new viewport placement for an existing image buffer,
    incrementing its reference count.

    Returns a error message used for acknowledgements if 'img' is NULL or placement pool is OOM.
    Returns NULL on success.
*/
static const char *_sfte_kitty_apply_placement(sfte_ctx *ctx, sfte_img *img) {
    if (!img) return "EINVAL: cannot apply placement to NULL image";
    sfte_img_placement *p = _sfte_img_placement_insert(ctx,
                                                       (sfte_img_placement){
                                                           .img_id = img->id,
                                                           .placement_id = ctx->kitty.placement_id,
                                                           .start_col = ctx->term.cursor_x,
                                                           .start_row = ctx->term.cursor_y,
                                                           .x_off = ctx->kitty.x_off,
                                                           .y_off = ctx->kitty.y_off,
                                                           .z_idx = ctx->kitty.z_idx,
                                                           .alt_screen = ctx->term.alt_active,
                                                       });

    if (!p) return "ENOMEM: placement pool capacity reached";

    img->ref_cnt++;
    int cols = _sfte_grid_span(img->width, ctx->kitty.x_off, ctx->font.cell_width);
    int rows = _sfte_grid_span(img->height, ctx->kitty.y_off, ctx->font.cell_height);
    _sfte_grid_dirty_rect(ctx, ctx->term.cursor_x, ctx->term.cursor_y, cols, rows);
    return NULL;
}

/*
    Queries the terminals image support or interrogates the status of a specific image ID.
    If the terminal successfully parses the request, it replies with an OK status.

    Always returns NULL.
*/
static const char *_sfte_kitty_exec_query(sfte_ctx *ctx) {
    char reply[16];
    int len = snprintf(reply, sizeof(reply), "\033_Gi=%u;OK\033\\", ctx->kitty.id);
    if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, len);
    return NULL;
}

/*
    Iterates through active image placements and removes those that match the requested
    deletion criteria passed via a kitty sequence.

    Returns a error message used for acknowledgements if no image is found to delete.
    Returns NULL on success.
*/
static const char *_sfte_kitty_exec_delete(sfte_ctx *ctx) {
    for (uint32_t i = 0; i < ctx->term.img_placements_len; ++i) {
        sfte_img_placement *p = &ctx->term.img_placements[i];
        if (p->alt_screen != ctx->term.alt_active || p->is_sixel) continue;

        sfte_img *img = _sfte_img_find(ctx, p->img_id);
        if (!img) return "EINVAL: failed to find image to delete";

        if (_sfte_kitty_should_delete(ctx, p, img)) {
            img->ref_cnt--;
            int cols = _sfte_grid_span(img->width, p->x_off, ctx->font.cell_width);
            int rows = _sfte_grid_span(img->height, p->y_off, ctx->font.cell_height);
            _sfte_grid_dirty_rect(ctx, p->start_col, p->start_row, cols, rows);
            ctx->term.img_placements[i--] = ctx->term
                                                .img_placements[--ctx->term.img_placements_len];
        }
    }

    _sfte_kitty_gc_pool(ctx);
    return NULL;
}

/*
    Decodes the accumulated base64 payload into raw pixels, applies any requested
    cropping and scaling, and stores the final raster in the image pool.

    WARN:
    If the transmission medium is 't', the client sent a path to a temp file containing the pixels.
    The protocol dictates that the emulator assumes ownership of this file and MUST delete it
    after decoding to prevent disk leaks.

    Returns a error message used for acknowledgements if the base64 decode fails.
    Returns NULL on success.
*/
static const char *_sfte_kitty_exec_transmit(sfte_ctx *ctx, sfte_img **out_img) {
    size_t raw_len = 0;
    uint8_t *raw_data = _sfte_b64_decode((uint8_t *)ctx->kitty.b64_buf, ctx->kitty.b64_len,
                                         &raw_len);
    if (!raw_data) return "ENOMEM: base64 decode failed";

    int is_file = (ctx->kitty.t_medium == 'f' || ctx->kitty.t_medium == 't');
    char *file_path = NULL;
    if (is_file) {
        file_path = (char *)SFTE_MALLOC(raw_len + 1);
        memcpy(file_path, raw_data, raw_len);
        file_path[raw_len] = '\0';
    }

    int w = ctx->kitty.width;
    int h = ctx->kitty.height;
    uint32_t *pxs = _sfte_kitty_decode_payload(ctx, raw_data, raw_len, is_file, file_path, &w, &h);

    if (ctx->kitty.t_medium == 't' && is_file) remove(file_path);
    if (is_file) SFTE_FREE(file_path);
    SFTE_FREE(raw_data);
    if (!pxs) return "EBADFMT: failed to decode image data";

    pxs = _sfte_kitty_apply_crop(ctx, pxs, &w, &h);
    if (!pxs) return "EINVAL: invalid crop dimensions";

    pxs = _sfte_kitty_apply_scale(ctx, pxs, &w, &h);

    if (!ctx->kitty.id) ctx->kitty.id = ++ctx->term.next_img_id;
    sfte_img *new_img = _sfte_img_pool_insert(ctx, (sfte_img){
                                                       .id = ctx->kitty.id,
                                                       .width = w,
                                                       .height = h,
                                                       .pxs = pxs,
                                                   });

    if (!new_img) return "ENOMEM: image pool capacity reached";
    if (out_img) *out_img = new_img;
    return NULL;
}

/*
    Creates a new visual placement on the grid for an image already residing in the pool.

    Returns a error message used for acknowledgements if the requested image ID
    was never transmitted or has been garbage collected, OR if `_sfte_kitty_apply_placement` fails.
    Returns NULL on success.
*/
static const char *_sfte_kitty_exec_place(sfte_ctx *ctx) {
    for (uint32_t j = 0; j < ctx->term.img_pool_len; ++j)
        if (ctx->term.img_pool[j].id == ctx->kitty.id)
            return _sfte_kitty_apply_placement(ctx, &ctx->term.img_pool[j]);
    return "ENOENT: image id not found in pool";
}

/*
    Sends an acknowledgement response back to the client application.
    The `q` parameter dictates response verbosity.
    - q=0: Send a response for both success and failure.
    - q=1: Send a response ONLY on failure (defalut).
    - q=2: Silent.
*/
static void _sfte_kitty_send_ack(sfte_ctx *ctx, const char *err_msg) {
    if (err_msg && (ctx->kitty.quiet == 0 || ctx->kitty.quiet == 1)) {
        char reply[128];
        int len = ctx->kitty.id > 0 ? snprintf(reply, sizeof(reply), "\033_Gi=%u;%s\033\\",
                                               ctx->kitty.id, err_msg)
                                    : snprintf(reply, sizeof(reply), "\033_G;%s\033\\", err_msg);
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, len);
    } else if (!err_msg && !ctx->kitty.quiet) {
        char reply[16];
        int len = ctx->kitty.id > 0
                      ? snprintf(reply, sizeof(reply), "\033_Gi=%u;OK\033\\", ctx->kitty.id)
                      : snprintf(reply, sizeof(reply), "\033_G;OK\033\\");
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, len);
    }
}

/*
    The main entrypoint for kitty graphics sequences.
    Because terminal parsers usually have hard limits on escape sequence length,
    kitty protocol can send big image files across multiple escape sequences.

    The `m=1` parameter indicates that more data is coming.
    The base64 string is accumulated in `ctx->kitty.b64_buf` across multiple calls,
    and the evalutaion is triggered the moment a sequence with `m=0` is received.
*/
static void _sfte_kitty_parse_graphics(sfte_ctx *ctx, const char *payload) {
    const char *semi = strchr(payload, ';');
    // if there's no semicolon, the dictionary spans the entire payload
    const char *dict_end = semi ? semi : payload + strlen(payload);

    uint8_t more = 0;

    // lookahead to check if its a new transmission
    uint8_t is_new = 0;
    for (const char *p = payload; p < semi; ++p) {
        if (*p == 'a' || *p == 'f' || *p == 'i' || *p == 's' || *p == 'v' || *p == 'z')
            if (p + 1 < dict_end && p[1] == '=') {
                is_new = 1;
                break;
            }
    }

    // reset state if fresh
    if (is_new || !ctx->kitty.action) {
        char *saved_buf = ctx->kitty.b64_buf;
        size_t saved_cap = ctx->kitty.b64_cap;
        ctx->kitty = (sfte_kitty_state){
            .b64_buf = saved_buf,
            .b64_cap = saved_cap,
            .action = 'T',
            .format = SFTE_KITTY_FMT_RGBA,
            .t_medium = 'd',
            .quiet = 1,
            // rest 0-initialized
        };
    }

    // parse params into state
    const char *p = payload;
    while (p && p < dict_end) {
        char key = p[0];
        if (p + 1 >= dict_end || p[1] != '=') break;
        const char *val = p + 2;

        switch (key) {
        case 'a': ctx->kitty.action = val[0]; break;
        case 'i': ctx->kitty.id = (uint32_t)atoi(val); break;
        case 'z': ctx->kitty.z_idx = atoi(val); break;
        case 'f': ctx->kitty.format = atoi(val); break;
        case 's': ctx->kitty.width = atoi(val); break;
        case 'v': ctx->kitty.height = atoi(val); break;
        case 'm': more = atoi(val); break;
        case 't': ctx->kitty.t_medium = val[0]; break;
        case 'd': ctx->kitty.d_action = val[0]; break;
        case 'c': ctx->kitty.cols = atoi(val); break;
        case 'r': ctx->kitty.rows = atoi(val); break;
        case 'X': ctx->kitty.x_off = atoi(val); break;
        case 'Y': ctx->kitty.y_off = atoi(val); break;
        case 'x': ctx->kitty.crop_x = atoi(val); break;
        case 'y': ctx->kitty.crop_y = atoi(val); break;
        case 'w': ctx->kitty.crop_w = atoi(val); break;
        case 'h': ctx->kitty.crop_h = atoi(val); break;
        case 'p': ctx->kitty.placement_id = (uint32_t)atoi(val); break;
        case 'q': ctx->kitty.quiet = (uint8_t)atoi(val); break;
        default: break;
        }

        p = strchr(p, ',');
        if (!p || p >= dict_end) break;
        p++;  // skip comma
    }

    if (semi) {
        const char *b64_start = semi + 1;
        size_t b64_len = strlen(b64_start);

        uint8_t oom = 0;
        _SFTE_MEM_ENSURE_CAP(char, ctx->kitty.b64_buf, ctx->kitty.b64_len, ctx->kitty.b64_cap,
                             b64_len + 1, SFTE_KITTY_B64_INIT_CAP, SFTE_KITTY_B64_MAX_CAP, oom);

        if (oom) return;

        memcpy(ctx->kitty.b64_buf + ctx->kitty.b64_len, b64_start, b64_len);
        ctx->kitty.b64_len += b64_len;
    }

    if (more) return;  // Abort and wait for next sequence

    ctx->kitty.b64_buf[ctx->kitty.b64_len] = '\0';
    const char *err_msg = NULL;

    switch (ctx->kitty.action) {
    case 'q': err_msg = _sfte_kitty_exec_query(ctx); break;
    case 'd': err_msg = _sfte_kitty_exec_delete(ctx); break;
    case 't': err_msg = _sfte_kitty_exec_transmit(ctx, NULL); break;
    case 'T': {
        sfte_img *img = NULL;
        err_msg = _sfte_kitty_exec_transmit(ctx, &img);
        if (err_msg || !img) break;
        _sfte_kitty_apply_placement(ctx, img);
        break;
    }
    case 'p': _sfte_kitty_exec_place(ctx); break;
    default: err_msg = "EINVAL: unknown action"; break;
    }

    _sfte_kitty_send_ack(ctx, err_msg);

    ctx->kitty.b64_len = 0;
}

static void _sfte_kitty_deinit(sfte_ctx *ctx) {
    if (ctx->term.img_pool) {
        for (uint32_t i = 0; i < ctx->term.img_pool_len; ++i)
            if (ctx->term.img_pool[i].pxs) SFTE_FREE(ctx->term.img_pool[i].pxs);
        SFTE_FREE(ctx->term.img_pool);
        ctx->term.img_pool = NULL;
    }
    if (ctx->term.img_placements) {
        SFTE_FREE(ctx->term.img_placements);
        ctx->term.img_placements = NULL;
    }
    if (ctx->kitty.b64_buf) {
        SFTE_FREE(ctx->kitty.b64_buf);
        ctx->kitty.b64_buf = NULL;
    }
}
#endif  // SFTE_KITTY_GRAPHICS
// =================================================================================================
// >>csi
// =================================================================================================

/*
    VT500 parameters default to 1 if omitted or set to 0.
*/
#define _SFTE_P(val) ((val) > 0 ? (val) : 1)

/*
    Converts 1-based VT500 coordinates to 0-based array indices.
*/
#define _SFTE_P_IDX(val) (_SFTE_P(val) - 1)

#if SFTE_TRUE_COLOR
/*
    Unpacks a 24-bit TrueColor RGB sequence from the parameter array.
*/
static inline uint32_t _sfte_csi_parse_truecolor(int *p, int i) {
    return (p[i + 2] << 16) | (p[i + 3] << 8) | p[i + 4];
}
#endif  // SFTE_TRUE_COLOR

/*
    Handles Insert Character / CSI @.

    Inserts n spaces at the cursor, shifting text right.
*/
static inline void _sfte_csi_exec_ich(sfte_ctx *ctx, int *p, int cx) {
    int n = _SFTE_P(p[0]);
    int rem = ctx->term.cols - cx;
    if (n > rem) n = rem;
    int move_cnt = rem - n;
    int base_idx = _SFTE_GRID_IDX(ctx, 0, ctx->term.cursor_y);
    if (move_cnt > 0)
        memmove(&ctx->term.cells[base_idx + cx + n], &ctx->term.cells[base_idx + cx],
                move_cnt * sizeof(sfte_cell));
    _sfte_grid_clear_cells(ctx, base_idx + cx, n);
    _sfte_grid_dirty_range(ctx, base_idx + cx, rem);
}

/*
    Handles Cursor Next Line / CSI E.

    Moves cursor to the beginning of the line n lines down.
*/
static inline void _sfte_csi_exec_cnl(sfte_ctx *ctx, int *p) {
    ctx->term.cursor_x = 0;
    ctx->term.cursor_y = _SFTE_CLAMP(ctx->term.cursor_y + _SFTE_P(p[0]), 0, ctx->term.rows - 1);
}

/*
    Handles Cursor Previous Line / CSI F.

    Moves cursor to the beginning of the line n lines up.
*/
static inline void _sfte_csi_exec_cpl(sfte_ctx *ctx, int *p) {
    ctx->term.cursor_x = 0;
    ctx->term.cursor_y = _SFTE_CLAMP(ctx->term.cursor_y - _SFTE_P(p[0]), 0, ctx->term.rows - 1);
}

/*
    Handles Erase in Display / CSI J.
*/
static inline void _sfte_csi_exec_erase_display(sfte_ctx *ctx, int mode, int cx) {
    // If we clear entire screen and scrollback exists,
    // push the data to scrollback instead of erasing it in its entirety
    if (mode == 2 || (mode == 0 && ctx->term.cursor_x == 0 && ctx->term.cursor_y == 0)) {
#if SFTE_SCROLLBACK_CAP
        // Find last populated row
        int last_r = ctx->term.cursor_y;
        for (int r = ctx->term.rows - 1; r > last_r; --r) {
            for (int c = 0; c < ctx->term.cols; ++c) {
                sfte_cell *cell = &ctx->term.cells[r * ctx->term.cols + c];
                if (cell->rune != ' ' && cell->rune != '\0') {
                    last_r = r;
                    break;
                }
            }

            if (last_r == r) break;
        }

#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
        for (uint32_t i = 0; i < ctx->term.img_placements_len; ++i) {
            sfte_img_placement *p = &ctx->term.img_placements[i];
            if (p->alt_screen != ctx->term.alt_active) continue;

            sfte_img *img = _sfte_img_find(ctx, p->img_id);
            if (!img) continue;

            int rows = _sfte_grid_span(img->height, p->y_off, ctx->font.cell_height);
            int img_bot = p->start_row + rows - 1;
            if (img_bot > last_r) last_r = img_bot;
        }
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS

        if (last_r >= ctx->term.rows) last_r = ctx->term.rows - 1;
        int lines_to_push = last_r + 1;

        // Temporarily bypass scroll margins to ensure full-screen push
        int old_top = ctx->term.scroll_top;
        int old_bot = ctx->term.scroll_bottom;
        ctx->term.scroll_top = 0;
        ctx->term.scroll_bottom = ctx->term.rows - 1;

        if (lines_to_push > 0) _sfte_grid_scroll(ctx, lines_to_push);

        ctx->term.scroll_top = old_top;
        ctx->term.scroll_bottom = old_bot;
#endif  // SFTE_SCROLLBACK_CAP

        _sfte_grid_clear_cells(ctx, 0, ctx->term.rows * ctx->term.cols);
        return;
    }

    if (mode == 0) {
        int start_idx = _SFTE_GRID_IDX(ctx, cx, ctx->term.cursor_y);
        _sfte_grid_clear_cells(ctx, start_idx, (ctx->term.rows * ctx->term.cols) - start_idx);
    } else if (mode == 1) {
        int end_idx = _SFTE_GRID_IDX(ctx, ctx->term.cursor_x, ctx->term.cursor_y) + 1;
        _sfte_grid_clear_cells(ctx, 0, end_idx);
    } else if (mode == 3) {
#if SFTE_SCROLLBACK_CAP && SFTE_SCROLLBACK_ALLOW_CLEAR
        ctx->term.sb_len = 0;
        ctx->term.sb_head = 0;
        ctx->term.sb_offset = 0;
#endif  // SFTE_SCROLLBACK_CAP && SFTE_SCROLLBACK_ALLOW_CLEAR
    }
}

/*
    Handles Insert Line / CSI L.

    Inserts n blank lines at cursor position, pushing bottom lines off.
*/
static inline void _sfte_csi_exec_il(sfte_ctx *ctx, int *p) {
    int n = _SFTE_P(p[0]);
    int top = ctx->term.cursor_y;
    int bot = ctx->term.scroll_bottom;
    if (top < ctx->term.scroll_top || top > bot) return;
    int height = bot - top + 1;
    if (n > height) n = height;
    int move_cnt = height - n;
    int cols = ctx->term.cols;
    if (move_cnt > 0)
        memmove(&ctx->term.cells[(top + n) * cols], &ctx->term.cells[top * cols],
                move_cnt * cols * sizeof(sfte_cell));
    _sfte_grid_clear_cells(ctx, top * cols, n * cols);
    _sfte_grid_dirty_range(ctx, top * cols, height * cols);
}

/*
    Handles Delete Line / CSI M.

    Deletes n lines at the cursor, pulling bottom lines up.
*/
static inline void _sfte_csi_exec_dl(sfte_ctx *ctx, int *p) {
    int n = _SFTE_P(p[0]);
    int top = ctx->term.cursor_y;
    int bot = ctx->term.scroll_bottom;
    if (top < ctx->term.scroll_top || top > bot) return;
    int height = bot - top + 1;
    if (n > height) n = height;
    int move_cnt = height - n;
    int cols = ctx->term.cols;
    if (move_cnt > 0)
        memmove(&ctx->term.cells[top * cols], &ctx->term.cells[(top + n) * cols],
                move_cnt * cols * sizeof(sfte_cell));
    _sfte_grid_clear_cells(ctx, (bot - n + 1) * cols, n * cols);
    _sfte_grid_dirty_range(ctx, top * cols, height * cols);
}

/*
    Handles Delete Character / CSI P.

    Deletes n characters at the cursor, shifting right text left.
*/
static inline void _sfte_csi_exec_dch(sfte_ctx *ctx, int *p, int cx) {
    int n = _SFTE_P(p[0]);
    int rem = ctx->term.cols - cx;
    if (n > rem) n = rem;
    int move_cnt = rem - n;
    int base_idx = _SFTE_GRID_IDX(ctx, 0, ctx->term.cursor_y);
    if (move_cnt > 0)
        memmove(&ctx->term.cells[base_idx + cx], &ctx->term.cells[base_idx + cx + n],
                move_cnt * sizeof(sfte_cell));
    _sfte_grid_clear_cells(ctx, base_idx + ctx->term.cols - n, n);
    _sfte_grid_dirty_range(ctx, base_idx + cx, rem);
}

/*
    Handles Erase Character / CSI X.

    Replaces n characters with spaces starting at the cursor.
*/
static inline void _sfte_csi_exec_ech(sfte_ctx *ctx, int *p, int cx) {
    int n = _SFTE_P(p[0]);
    int rem = ctx->term.cols - cx;
    if (n > rem) n = rem;
    _sfte_grid_clear_cells(ctx, _SFTE_GRID_IDX(ctx, cx, ctx->term.cursor_y), n);
}

/*
    Handles Device Attributes / CSI c.

    Reports the terminals identity and capabilities to the host.

    TODO:
    Add identity to customization. This might be useful especially for custom backends and such.
*/
static inline void _sfte_csi_exec_da(sfte_ctx *ctx) {
    if (ctx->term.vt_dec_priv == 2) {
        const char *sda = "\033[>0;95;0c";
        ctx->write_cb(ctx->user_data, sda, strlen(sda));
    } else {
        const char *da = "\033[?62c";
        ctx->write_cb(ctx->user_data, da, strlen(da));
    }
}

/*
    Handles Vertical Position Absolute / CSI d.

    Moves cursor to the specific row n.

    NOTE:
    Respects origin mode.
*/
static inline void _sfte_csi_exec_vpa(sfte_ctx *ctx, int *p) {
    if (ctx->term.origin_mode)
        ctx->term.cursor_y = _SFTE_CLAMP(_SFTE_P_IDX(p[0]) + ctx->term.scroll_top,
                                         ctx->term.scroll_top, ctx->term.scroll_bottom);
    else
        ctx->term.cursor_y = _SFTE_CLAMP(_SFTE_P_IDX(p[0]), 0, ctx->term.rows - 1);
}

/*
    Handles Horizontal Vertical Position / CSI f.

    Moves cursor to row n, column m.

    NOTE:
    Respects origin mode.
*/
static inline void _sfte_csi_exec_hvp(sfte_ctx *ctx, int *p) {
    ctx->term.cursor_x = _SFTE_CLAMP(_SFTE_P_IDX(p[1]), 0, ctx->term.cols - 1);
    if (ctx->term.origin_mode)
        ctx->term.cursor_y = _SFTE_CLAMP(_SFTE_P_IDX(p[0]) + ctx->term.scroll_top,
                                         ctx->term.scroll_top, ctx->term.scroll_bottom);
    else
        ctx->term.cursor_y = _SFTE_CLAMP(_SFTE_P_IDX(p[0]), 0, ctx->term.rows - 1);
}

/*
    Handles Tab Clear / CSI g.

    Clears tab stops at current column (0) or all columns (3).
*/
static inline void _sfte_csi_exec_tbc(sfte_ctx *ctx, int *p) {
    if (p[0] == 0)
        ctx->term.tab_stops[ctx->term.cursor_x] = 0;
    else if (p[0] == 3)
        memset(ctx->term.tab_stops, 0, ctx->term.cols);
}

/*
    Handles Set Mode / CSI h.

    Enables various terminal modes.
    Supports DECTCEM (Cursor Show), DECAWM (Auto-Wrap),
    DECOM (Origin Mode), and alt screen buffer toggles.
*/
static inline void _sfte_csi_set_mode(sfte_ctx *ctx, int *p, int cnt, int cx) {
    if (!ctx->term.vt_dec_priv) return;

    for (int i = 0; i < cnt; ++i) {
        if (p[i] == 25) {
            ctx->term.hide_cursor = 0;
            ctx->term.cells[_SFTE_GRID_IDX(ctx, cx, ctx->term.cursor_y)].dirty = 1;
        } else if (p[i] == 2004)
            ctx->term.bracketed_paste = 1;
        else if (p[i] == 7)
            ctx->term.auto_wrap = 1;
        else if (p[i] == 6) {
            ctx->term.origin_mode = 1;
            ctx->term.cursor_x = 0;
            ctx->term.cursor_y = ctx->term.scroll_top;
        } else if (p[i] == 1047 || p[i] == 1048 || p[i] == 1049) {
            // 1048 / 1049 save cursor
            if (p[i] == 1048 || p[i] == 1049) {
                int s_idx = ctx->term.alt_active ? 1 : 0;
                ctx->term.saved_x[s_idx] = ctx->term.cursor_x;
                ctx->term.saved_y[s_idx] = ctx->term.cursor_y;
                ctx->term.saved_fg[s_idx] = ctx->term.cur_fg;
                ctx->term.saved_bg[s_idx] = ctx->term.cur_bg;
                ctx->term.saved_attr[s_idx] = ctx->term.cur_attr;
            }

#if SFTE_ALT_SCREEN
            // 1047 / 1049 switch to alt screen
            if ((p[i] == 1047 || p[i] == 1049) && !ctx->term.alt_active) {
                ctx->term.alt_active = 1;
#if SFTE_KITTY_KB
                ctx->term.kitty_kb_idx[1] = 0;
                ctx->term.kitty_kb_stack[1][0] = 0;
#endif  // SFTE_KITTY_KB
#if SFTE_CURSOR_TRAIL
                ctx->term.last_move_ms = 0;
#endif  // SFTE_CURSOR_TRAIL

                if (!ctx->term.alt_cells)
                    ctx->term.alt_cells = (sfte_cell *)SFTE_CALLOC(ctx->term.cols * ctx->term.rows,
                                                                   sizeof(sfte_cell));

                sfte_cell *tmp = ctx->term.cells;
                ctx->term.cells = ctx->term.alt_cells;
                ctx->term.alt_cells = tmp;
            }
#endif  // SFTE_ALT_SCREEN

            if (p[i] == 1049) {
                _sfte_grid_clear_cells(ctx, 0, ctx->term.cols * ctx->term.rows);
                ctx->term.cursor_x = 0;
                ctx->term.cursor_y = 0;
            } else if (p[i] == 1047) {
                _sfte_grid_dirty_range(ctx, 0, ctx->term.cols * ctx->term.rows);
            }
        }
#if SFTE_MOUSE
        else if (p[i] == 1000 || p[i] == 1002 || p[i] == 1003)
            ctx->term.mouse_mode = p[i];
        else if (p[i] == 1006)
            ctx->term.mouse_ext = 1006;
#endif  // SFTE_MOUSE
    }
}

/*
    Handles Reset Mode / CSI l

    Disables various terminal modes.
    Matches the implementations found in SM.
*/
static inline void _sfte_csi_reset_mode(sfte_ctx *ctx, int *p, int cnt, int cx) {
    if (!ctx->term.vt_dec_priv) return;

    for (int i = 0; i < cnt; ++i) {
        if (p[i] == 25) {
            ctx->term.hide_cursor = 1;
            ctx->term.cells[_SFTE_GRID_IDX(ctx, cx, ctx->term.cursor_y)].dirty = 1;
        } else if (p[i] == 2004)
            ctx->term.bracketed_paste = 0;
        else if (p[i] == 7)
            ctx->term.auto_wrap = 0;
        else if (p[i] == 6) {
            ctx->term.origin_mode = 0;
            ctx->term.cursor_x = 0;
            ctx->term.cursor_y = 0;
        } else if (p[i] == 1047 || p[i] == 1048 || p[i] == 1049) {
#if SFTE_ALT_SCREEN
            if ((p[i] == 1047 || p[i] == 1049) && ctx->term.alt_active) {
                ctx->term.alt_active = 0;
#if SFTE_KITTY_KB
                ctx->term.kitty_kb_idx[1] = 0;
                ctx->term.kitty_kb_stack[1][0] = 0;
#endif  // SFTE_KITTY_KB

                if (ctx->term.alt_cells) {
                    sfte_cell *tmp = ctx->term.cells;
                    ctx->term.cells = ctx->term.alt_cells;
                    ctx->term.alt_cells = tmp;
                    _sfte_grid_dirty_range(ctx, 0, ctx->term.cols * ctx->term.rows);
                }

#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
                // Destroy all images created on alt screen
                for (uint32_t j = 0; j < ctx->term.img_placements_len; ++j) {
                    if (!ctx->term.img_placements[j].alt_screen) continue;
                    for (uint32_t k = 0; k < ctx->term.img_pool_len; ++k)
                        if (ctx->term.img_pool[k].id == ctx->term.img_placements[j].img_id) {
                            ctx->term.img_pool[k].ref_cnt--;
                            break;
                        }
                    ctx->term
                        .img_placements[j--] = ctx->term
                                                   .img_placements[--ctx->term.img_placements_len];
                }
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS
            }
#endif  // SFTE_ALT_SCREEN

            if (p[i] == 1048 || p[i] == 1049) {
                int s_idx = ctx->term.alt_active ? 1 : 0;
                ctx->term.cursor_x = _SFTE_CLAMP(ctx->term.saved_x[s_idx], 0, ctx->term.cols - 1);
                ctx->term.cursor_y = _SFTE_CLAMP(ctx->term.saved_y[s_idx], 0, ctx->term.rows - 1);

                // force external output below prompt
                if (p[i] == 1049) {
                    ctx->term.cursor_x = 0;
                    if (ctx->term.cursor_y == ctx->term.scroll_bottom)
                        _sfte_grid_scroll(ctx, 1);
                    else if (ctx->term.cursor_y == ctx->term.rows - 1) {
                        int old_t = ctx->term.scroll_top;
                        int old_b = ctx->term.scroll_bottom;
                        ctx->term.scroll_top = 0;
                        ctx->term.scroll_bottom = ctx->term.rows - 1;
                        _sfte_grid_scroll(ctx, 1);
                        ctx->term.scroll_top = old_t;
                        ctx->term.scroll_bottom = old_b;
                    } else if (ctx->term.cursor_y < ctx->term.rows - 1)
                        ctx->term.cursor_y++;
                }

                ctx->term.cur_fg = ctx->term.saved_fg[s_idx];
                ctx->term.cur_bg = ctx->term.saved_bg[s_idx];
                ctx->term.cur_attr = ctx->term.saved_attr[s_idx];
                ctx->term.cells[_SFTE_GRID_IDX(ctx, ctx->term.cursor_x, ctx->term.cursor_y)]
                    .dirty = 1;

#if SFTE_CURSOR_TRAIL
                ctx->term.last_move_ms = 0;
                ctx->term.is_trailing = 0;
                ctx->term.tail_rx = ctx->term.cursor_x * ctx->font.cell_width;
                ctx->term.tail_ry = ctx->term.cursor_y * ctx->font.cell_height;
                ctx->term.trail_damage_w = 0;
#endif  // SFTE_CURSOR_TRAIL
            }
        }
#if SFTE_MOUSE
        else if (p[i] == 1000 || p[i] == 1002 || p[i] == 1003)
            ctx->term.mouse_mode = 0;
        else if (p[i] == 1006)
            ctx->term.mouse_ext = 0;
#endif  // SFTE_MOUSE
    }
}

/*
    Handles Select Graphic Rendition / CSI m

    Sets colors and style of the characters following this code.
*/
static inline void _sfte_csi_exec_sgr(sfte_ctx *ctx, int *p, int cnt) {
    for (int i = 0; i < cnt; ++i) {
        if (p[i] == 0) {
            ctx->term.cur_fg = 0xFFFFFFFF;
            ctx->term.cur_bg = SFTE_BG_COLOR;
            ctx->term.cur_attr = 0;
#if SFTE_COLOR_UNDERLINE
            ctx->term.cur_ul_color = 0xFFFFFFFF;
#endif  // SFTE_COLOR_UNDERLINE
#if SFTE_EXT_UNDERLINES
            ctx->term.cur_ul_style = 0;
#endif  // SFTE_EXT_UNDERLINES
        } else if (p[i] == 1)
            ctx->term.cur_attr |= ATTR_BOLD;
        else if (p[i] == 3)
            ctx->term.cur_attr |= ATTR_ITALIC;
        else if (p[i] == 4) {
            ctx->term.cur_attr |= ATTR_UNDERLINE;
#if SFTE_EXT_UNDERLINES
            if (i + 1 < cnt && (p[i + 1] >= 1 && p[i + 1] <= 5)) {
                ctx->term.cur_ul_style = p[i + 1];
                i++;  // Skip sub-parameters
            } else
                ctx->term.cur_ul_style = 1 /* Standard straight line */;
#endif  // SFTE_EXT_UNDERLINES
        } else if (p[i] == 7)
            ctx->term.cur_attr |= ATTR_REVERSE;
        else if (p[i] == 22)
            ctx->term.cur_attr &= ~ATTR_BOLD;
        else if (p[i] == 23)
            ctx->term.cur_attr &= ~ATTR_ITALIC;
        else if (p[i] == 24) {
            ctx->term.cur_attr &= ~ATTR_UNDERLINE;
#if SFTE_EXT_UNDERLINES
            ctx->term.cur_ul_style = 0;
#endif  // SFTE_EXT_UNDERLINES
        } else if (p[i] == 27)
            ctx->term.cur_attr &= ~ATTR_REVERSE;
        else if (p[i] >= 30 && p[i] <= 37)
            ctx->term.cur_fg = _sfte_ansi_palette[p[i] - 30];
        else if (p[i] == 39)  // Set default foreground
            ctx->term.cur_fg = 0xFFFFFFFF;
        else if (p[i] >= 40 && p[i] <= 47)
            ctx->term.cur_bg = _sfte_ansi_palette[p[i] - 40];
        else if (p[i] == 49)  // Set default background
            ctx->term.cur_bg = SFTE_BG_COLOR;
        // NOTE:
        // TrueColor sequences use 5 parameters.
        // We must manually advance the `i` iterator by 4
        // to prevent the parser from reading them as subsequent SGR commands.
        else if (p[i] == 38 && i + 4 < cnt && p[i + 1] == 2) {  // Set TrueColor-based foreground
#if SFTE_TRUE_COLOR
            ctx->term.cur_fg = _sfte_csi_parse_truecolor(p, i);
#endif  // SFTE_TRUE_COLOR
            i += 4;
        } else if (p[i] == 48 && i + 4 < cnt && p[i + 1] == 2) {  // Set TrueColor-based background
#if SFTE_TRUE_COLOR
            ctx->term.cur_bg = _sfte_csi_parse_truecolor(p, i);
#endif  // SFTE_TRUE_COLOR
            i += 4;
        }
#if SFTE_COLOR_UNDERLINE
        else if (p[i] == 58 && i + 4 < cnt && p[i + 1] == 2) {
            ctx->term.cur_ul_color = _sfte_csi_parse_truecolor(p, i);
            i += 4;
        } else if (p[i] == 59)
            ctx->term.cur_ul_color = 0xFFFFFFFF;
#endif  // SFTE_COLOR_UNDERLINE
    }
}

/*
    Handles Device Status Report / CSI n.

    Reports cursor position (6) or terminal status (5).
*/
static inline void _sfte_csi_exec_dsr(sfte_ctx *ctx, int *p) {
    if (p[0] == 5) {
        const char *reply = "\033[0n";
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, strlen(reply));
    } else if (p[0] == 6) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "\033[%d;%dR", ctx->term.cursor_y + 1,
                           ctx->term.cursor_x + 1);
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, buf, len);
    }
}

/*
    Handles Soft Terminal Reset / CSI p.

    Resets terminal state to default values.
*/
static inline void _sfte_csi_exec_decstr(sfte_ctx *ctx, int cx) {
#if SFTE_MOUSE
    ctx->term.mouse_mode = 0;
    ctx->term.mouse_ext = 0;
#endif  // SFTE_MOUSE
#if SFTE_KITTY_KB
    ctx->term.kitty_kb_idx[0] = 0;
    ctx->term.kitty_kb_idx[1] = 0;
    ctx->term.kitty_kb_stack[0][0] = 0;
    ctx->term.kitty_kb_stack[1][0] = 0;
#endif  // SFTE_KITTY_KB
#if SFTE_CURSOR_BLINK
    ctx->term.blink_enabled = 1;
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_TRAIL
    ctx->term.last_move_ms = 0;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_CURSOR_DYNAMIC
    ctx->term.cursor_style = SFTE_CURSOR_STYLE;
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_COLOR_UNDERLINE
    ctx->term.cur_ul_color = 0xFFFFFFFF;
#endif  // SFTE_COLOR_UNDERLINE
#if SFTE_EXT_UNDERLINES
    ctx->term.cur_ul_style = 0;
#endif  // SFTE_EXT_UNDERLINES
#if SFTE_HYPERLINKS
    ctx->term.cur_link_idx = 0;
#endif  // SFTE_HYPERLINKS
    ctx->term.scroll_top = 0;
    ctx->term.scroll_bottom = ctx->term.rows - 1;
    ctx->term.cur_fg = 0xFFFFFFFF;
    ctx->term.cur_bg = SFTE_BG_COLOR;
    ctx->term.cur_attr = 0;
    ctx->term.hide_cursor = 0;
    ctx->term.cells[_SFTE_GRID_IDX(ctx, cx, ctx->term.cursor_y)].dirty = 1;
}

/*
    Handles Set Cursor Style / CSI q.

    Changes the cursor shape and blinking style.
*/
static inline void _sfte_csi_exec_decscusr(sfte_ctx *ctx, int *p, int cx) {
#if SFTE_CURSOR_BLINK
    switch (p[0]) {
    case 0:
    case 1:
    case 3:
    case 5: ctx->term.blink_enabled = 1; break;
    case 2:
    case 4:
    case 6: ctx->term.blink_enabled = 0; break;
    }
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_DYNAMIC
    switch (p[0]) {
    case 0: ctx->term.cursor_style = SFTE_CURSOR_STYLE; break;
    case 1:
    case 2: ctx->term.cursor_style = SFTE_CURSOR_BLOCK; break;
    case 3:
    case 4: ctx->term.cursor_style = SFTE_CURSOR_UNDERLINE; break;
    case 5:
    case 6: ctx->term.cursor_style = SFTE_CURSOR_BAR; break;
    }
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_CURSOR_BLINK || SFTE_CURSOR_DYNAMIC
    ctx->term.cells[_SFTE_GRID_IDX(ctx, cx, ctx->term.cursor_y)].dirty = 1;
#endif  // SFTE_CURSOR_BLINK || SFTE_CURSOR_DYNAMIC
}

/*
    Handles Set Top and Bottom Margins / CSI r.

    Sets the scrolling region top and bottom margins.

    NOTE:
    Respects origin mode.
*/
static inline void _sfte_csi_exec_decstbm(sfte_ctx *ctx, int *p, int cnt) {
    int top = _SFTE_P_IDX(p[0]);
    if (top < 0) top = 0;
    int bot = (cnt > 1 && p[1] > 0 ? p[1] : ctx->term.rows) - 1;
    if (bot >= ctx->term.rows) bot = ctx->term.rows - 1;
    if (top < bot) {
        ctx->term.scroll_top = top;
        ctx->term.scroll_bottom = bot;
    }
    ctx->term.cursor_x = 0;
    ctx->term.cursor_y = ctx->term.origin_mode ? ctx->term.scroll_top : 0;
}

/*
    Handles Save Cursor / CSI s.

    Saves the current cursor position and attributes.
*/
static inline void _sfte_csi_exec_scosc(sfte_ctx *ctx, int *p) {
    if (p[0] != 0) return;  // Avoid colliding with kitty support command
    int s_idx = ctx->term.alt_active ? 1 : 0;
    ctx->term.saved_x[s_idx] = ctx->term.cursor_x;
    ctx->term.saved_y[s_idx] = ctx->term.cursor_y;
    ctx->term.saved_fg[s_idx] = ctx->term.cur_fg;
    ctx->term.saved_bg[s_idx] = ctx->term.cur_bg;
    ctx->term.saved_attr[s_idx] = ctx->term.cur_attr;
}

/*
    Handles Window Manipulation / CSI t.

    Xterm extension for querying or pushing/popping window titles.
*/
static inline void _sfte_csi_exec_xtwinops(sfte_ctx *ctx, int *p) {
    if (p[1] != 0 && p[1] != 2) return;
    if (p[0] == 22)
        snprintf(ctx->term.saved_title, sizeof(ctx->term.saved_title), "%s", ctx->term.title);
    else if (p[0] == 23) {
        snprintf(ctx->term.title, sizeof(ctx->term.title), "%s", ctx->term.saved_title);
        // TODO:
        // Make a callback that gets called here for custom backends support.
#if SFTE_WAYLAND
        sfte_wayland_app *app = (sfte_wayland_app *)ctx->user_data;
        xdg_toplevel_set_title(app->xdg_toplevel, ctx->term.title);
#endif  // SFTE_WAYLAND
    }
}

/*
    Handles Restore Cursor / CSI u.

    Restores the previously saved cursor position and attributes.
*/
static inline void _sfte_csi_exec_scorc(sfte_ctx *ctx, int *p) {
    if (ctx->term.vt_dec_priv != 0 || p[0] != 0) return;
    uint8_t s_idx = ctx->term.alt_active ? 1 : 0;
    ctx->term.cursor_x = _SFTE_CLAMP(ctx->term.saved_x[s_idx], 0, ctx->term.cols - 1);
    ctx->term.cursor_y = _SFTE_CLAMP(ctx->term.saved_y[s_idx], 0, ctx->term.rows - 1);
    ctx->term.cur_fg = ctx->term.saved_fg[s_idx];
    ctx->term.cur_bg = ctx->term.saved_bg[s_idx];
    ctx->term.cur_attr = ctx->term.saved_attr[s_idx];
}

#if SFTE_KITTY_KB
/*
    Handles Kitty Keyboard Protocol / CSI u extension.

    The VT500 standard defines 'u' as SCORC (Restore Cursor) if no modifiers are present.
    However, the modern kitty keyboard protocol overloads 'u' to manage
    the keyboard flag stack when preceded by >, =, < or ?.
*/
static inline void _sfte_csi_exec_kitty(sfte_ctx *ctx, int *p) {
    if (ctx->term.vt_dec_priv == 0) return;
    uint8_t s_idx = ctx->term.alt_active ? 1 : 0;
    if (ctx->term.vt_dec_priv == 1) {  // CSI ? u (query)
        char buf[32];
        int flags = ctx->term.kitty_kb_stack[s_idx][ctx->term.kitty_kb_idx[s_idx]];
        int len = snprintf(buf, sizeof(buf), "\033[?%du", flags);
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, buf, len);
    } else if (ctx->term.vt_dec_priv == 2) {  // CSI > flags u (push)
        if (ctx->term.kitty_kb_idx[s_idx] < 15) ctx->term.kitty_kb_idx[s_idx]++;
        ctx->term.kitty_kb_stack[s_idx][ctx->term.kitty_kb_idx[s_idx]] = p[0];
    } else if (ctx->term.vt_dec_priv == 3) {  // CSI < n u (pop)
        ctx->term.kitty_kb_idx[s_idx] -= (p[0] > 0) ? p[0] : 1;
        if (ctx->term.kitty_kb_idx[s_idx] < 0) ctx->term.kitty_kb_idx[s_idx] = 0;
    } else if (ctx->term.vt_dec_priv == 4) {  // CSI = flags u (set/overwrite)
        ctx->term.kitty_kb_stack[s_idx][ctx->term.kitty_kb_idx[s_idx]] = p[0];
    }
}
#endif  // SFTE_KITTY_KB

/*
    The main routing switch for Control Sequence Introducer events.
*/
static void _sfte_csi_dispatch(sfte_ctx *ctx, uint8_t cmd) {
    int *p = ctx->term.vt_params;
    int cnt = ctx->term.vt_param_idx + 1;
    int cx = ctx->term.cursor_x >= ctx->term.cols ? ctx->term.cols - 1 : ctx->term.cursor_x;

    switch (cmd) {
    case '@': _sfte_csi_exec_ich(ctx, p, cx); break;
    case 'A':  // CUU / Cursor Up
        ctx->term.cursor_y = _SFTE_CLAMP(ctx->term.cursor_y - _SFTE_P(p[0]), 0, ctx->term.rows - 1);
        break;
    case 'B':  // CUD / Cursor Down
        ctx->term.cursor_y = _SFTE_CLAMP(ctx->term.cursor_y + _SFTE_P(p[0]), 0, ctx->term.rows - 1);
        break;
    case 'C':  // CUF / Cursor Forward
        ctx->term.cursor_x = _SFTE_CLAMP(ctx->term.cursor_x + _SFTE_P(p[0]), 0, ctx->term.cols - 1);
        break;
    case 'D':  // CUB / Cursor Back
        ctx->term.cursor_x = _SFTE_CLAMP(ctx->term.cursor_x - _SFTE_P(p[0]), 0, ctx->term.cols - 1);
        break;
    case 'E': _sfte_csi_exec_cnl(ctx, p); break;
    case 'F': _sfte_csi_exec_cpl(ctx, p); break;
    case 'G':  // CHA / Cursor Horizontal Absolute
        ctx->term.cursor_x = _SFTE_CLAMP(_SFTE_P_IDX(p[0]), 0, ctx->term.cols - 1);
        break;
    case 'H':  // CUP / Cursor Position
        ctx->term.cursor_x = _SFTE_CLAMP(_SFTE_P_IDX(p[1]), 0, ctx->term.cols - 1);
        ctx->term.cursor_y = _SFTE_CLAMP(_SFTE_P_IDX(p[0]), 0, ctx->term.rows - 1);
        break;
    case 'J': _sfte_csi_exec_erase_display(ctx, p[0], cx); break;
    case 'K':  // EL / Erase in Line
        if (p[0] == 0)
            _sfte_grid_clear_cells(ctx, _SFTE_GRID_IDX(ctx, cx, ctx->term.cursor_y),
                                   ctx->term.cols - cx);
        else if (p[0] == 1)
            _sfte_grid_clear_cells(ctx, _SFTE_GRID_IDX(ctx, 0, ctx->term.cursor_y), cx + 1);
        else if (p[0] == 2)
            _sfte_grid_clear_cells(ctx, _SFTE_GRID_IDX(ctx, 0, ctx->term.cursor_y), ctx->term.cols);
        break;
    case 'L': _sfte_csi_exec_il(ctx, p); break;
    case 'M': _sfte_csi_exec_dl(ctx, p); break;
    case 'P': _sfte_csi_exec_dch(ctx, p, cx); break;
    case 'S': _sfte_grid_scroll(ctx, _SFTE_P(p[0])); break;
    case 'T': _sfte_grid_scroll(ctx, -_SFTE_P(p[0])); break;
    case 'X': _sfte_csi_exec_ech(ctx, p, cx); break;
    case 'c': _sfte_csi_exec_da(ctx); break;
    case 'd': _sfte_csi_exec_vpa(ctx, p); break;
    case 'f': _sfte_csi_exec_hvp(ctx, p); break;
    case 'g': _sfte_csi_exec_tbc(ctx, p); break;
    case 'h': _sfte_csi_set_mode(ctx, p, cnt, cx); break;
    case 'l': _sfte_csi_reset_mode(ctx, p, cnt, cx); break;
    case 'm': _sfte_csi_exec_sgr(ctx, p, cnt); break;
    case 'n': _sfte_csi_exec_dsr(ctx, p); break;
    case 'p': _sfte_csi_exec_decstr(ctx, cx); break;
    case 'q': _sfte_csi_exec_decscusr(ctx, p, cx); break;
    case 'r': _sfte_csi_exec_decstbm(ctx, p, cnt); break;
    case 's': _sfte_csi_exec_scosc(ctx, p); break;
    case 't': _sfte_csi_exec_xtwinops(ctx, p); break;
    case 'u': _sfte_csi_exec_scorc(ctx, p);
#if SFTE_KITTY_KB
        _sfte_csi_exec_kitty(ctx, p);
#endif  // SFTE_KITTY_KB
        break;
    default: _SFTE_WARN(ctx, UNHANDLED_CSI, cmd, cnt); break;
    }
}
// =================================================================================================
// >>parser
// =================================================================================================
typedef enum {
    VT_GROUND,     // normal
    VT_ESCAPE,     // \033
    VT_CSI_ENTRY,  // \033[
    VT_CSI_PARAM,  // nums
    VT_OSC,        // \033]
    VT_CHARSET,    // \033( \033)
    VT_HASH,       // #
    VT_DCS,        // P / _ / ^
#if SFTE_SIXEL
    VT_SIXEL,
#endif  // SFTE_SIXEL
} sfte_vt_state;

/*
    Appends a byte to the shared OSC/DCS payload buffer.
    If the buffer is too small, reallocates it with size doubled,
    unless it's already at its max capacity.
*/
static inline void _sfte_parser_append_payload(sfte_ctx *ctx, uint8_t b) {
    if (ctx->term.osc_len + 1 >= ctx->term.osc_cap && ctx->term.osc_cap < SFTE_OSC_MAX_CAP) {
        ctx->term.osc_cap *= 2;
        ctx->term.osc_payload = (char *)SFTE_REALLOC(ctx->term.osc_payload, ctx->term.osc_cap);
    }
    if (ctx->term.osc_len + 1 < ctx->term.osc_cap) ctx->term.osc_payload[ctx->term.osc_len++] = b;
}

/*
    Handles LF, VT, FF (Linefeed).

    Scrolls if at bottom margin.
*/
static inline void _sfte_parser_c0_lf(sfte_ctx *ctx) {
    if (ctx->term.cursor_y == ctx->term.scroll_bottom)
        _sfte_grid_scroll(ctx, 1);
    else if (ctx->term.cursor_y == ctx->term.rows - 1) {
        // Temporarily set scroll values to bottom to safely scroll
        int old_top = ctx->term.scroll_top;
        int old_bot = ctx->term.scroll_bottom;
        ctx->term.scroll_top = 0;
        ctx->term.scroll_bottom = ctx->term.rows - 1;
        _sfte_grid_scroll(ctx, 1);
        ctx->term.scroll_top = old_top;
        ctx->term.scroll_bottom = old_bot;
    } else if (ctx->term.cursor_y < ctx->term.rows - 1)
        ctx->term.cursor_y++;
}

/*
    Handles HT (Horizontal Tab).

    Advances to the next set tab stop.
*/
static inline void _sfte_parser_c0_ht(sfte_ctx *ctx) {
    while (ctx->term.cursor_x < ctx->term.cols - 1)
        if (ctx->term.tab_stops[ctx->term.cursor_x++]) break;
}

/*
    Handles RIS (Reset to Initial State) ESC c.

    Reuses DECSTR logic.
*/
static inline void _sfte_parser_esc_ris(sfte_ctx *ctx) {
    _sfte_csi_dispatch(ctx, 'p');
    ctx->term.cursor_x = 0, ctx->term.cursor_y = 0;
    _sfte_grid_clear_cells(ctx, 0, ctx->term.rows * ctx->term.cols);
}

/*
    Handles SC (Save Cursor) / ESC 7.

    Temporarily stores the data (cursor position, foreground/background color, attribute)
    to load it via RC later.

    The terminal can store one set of data per screen (main/alt).
*/
static inline void _sfte_parser_esc_sc(sfte_ctx *ctx) {
    int s_idx = ctx->term.alt_active ? 1 : 0;
    ctx->term.saved_x[s_idx] = ctx->term.cursor_x;
    ctx->term.saved_y[s_idx] = ctx->term.cursor_y;
    ctx->term.saved_fg[s_idx] = ctx->term.cur_fg;
    ctx->term.saved_bg[s_idx] = ctx->term.cur_bg;
    ctx->term.saved_attr[s_idx] = ctx->term.cur_attr;
}

/*
    Handles RC (Restore Cursor) / ESC 8.

    Loads the data (cursor position, foreground/background color, attribute)
    stored by SC in sequence.

    The terminal can store one set of data per screen (main/alt).
*/
static inline void _sfte_parser_esc_rc(sfte_ctx *ctx) {
    int s_idx = ctx->term.alt_active ? 1 : 0;
    // NOTE:
    // We need to clamp here since between SC and RC a resize could've occured.
    ctx->term.cursor_x = _SFTE_CLAMP(ctx->term.saved_x[s_idx], 0, ctx->term.cols - 1);
    ctx->term.cursor_y = _SFTE_CLAMP(ctx->term.saved_y[s_idx], 0, ctx->term.rows - 1);
    ctx->term.cur_fg = ctx->term.saved_fg[s_idx];
    ctx->term.cur_bg = ctx->term.saved_bg[s_idx];
    ctx->term.cur_attr = ctx->term.saved_attr[s_idx];
}

/*
    Handles IND (Index) / ESC D.

    Moves down, scrolling if at margin.
*/
static inline void _sfte_parser_esc_ind(sfte_ctx *ctx) {
    if (ctx->term.cursor_y == ctx->term.scroll_bottom)
        _sfte_grid_scroll(ctx, 1);
    else if (ctx->term.cursor_y < ctx->term.rows - 1)
        ctx->term.cursor_y++;
}

/*
    Handles RI (Reverse Index) / ESC M.

    Moves up, scrolling (up) if at margin.
*/
static inline void _sfte_parser_esc_ri(sfte_ctx *ctx) {
    if (ctx->term.cursor_y == ctx->term.scroll_top)
        _sfte_grid_scroll(ctx, -1);
    else if (ctx->term.cursor_y > 0)
        ctx->term.cursor_y--;
}

/*
    Handles NEL (Next Line) / ESC E.

    Moves to start of the next line.

    Reuses Index logic to move down.
*/
static inline void _sfte_parser_esc_nel(sfte_ctx *ctx) {
    _sfte_parser_esc_ind(ctx);
    ctx->term.cursor_x = 0;
}

/*
    Handles DECALN (Screen Alignment Pattern) / ESC # 8.
*/
static inline void _sfte_parser_hash_decaln(sfte_ctx *ctx) {
    for (int i = 0; i < ctx->term.cols * ctx->term.rows; ++i) {
        ctx->term.cells[i].rune = 'E';
        ctx->term.cells[i].fg = 0xFFFFFFFF;
        ctx->term.cells[i].bg = SFTE_BG_COLOR;
        ctx->term.cells[i].attr = 0;
        ctx->term.cells[i].dirty = 1;
    }
    ctx->term.cursor_x = 0, ctx->term.cursor_y = 0;
}

/*
    Parses and executes completed OSC strings.
*/
static inline void _sfte_parser_osc_dispatch(sfte_ctx *ctx, uint8_t terminator) {
    const char *term = (terminator == '\x1b') ? "\033\\" : "\x07";
    ctx->term.osc_payload[ctx->term.osc_len] = '\0';

    if (strncmp(ctx->term.osc_payload, "10;?", 4) == 0 ||
        strncmp(ctx->term.osc_payload, "11;?", 4) == 0) {
        int is_bg = ctx->term.osc_payload[1] == '1';
        uint32_t col = is_bg ? SFTE_BG_COLOR : 0xFFFFFF;
        uint8_t cr = (col >> 16) & 0xFF, cg = (col >> 8) & 0xFF, cb = col & 0xFF;

        char reply[64];
        int len = snprintf(reply, sizeof(reply), "\033]%d;rgb:%02x%02x/%02x%02x/%02x%02x%s",
                           is_bg ? 11 : 10, cr, cr, cg, cg, cb, cb, term);
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, len);
    }
#if SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD
    else if (strncmp(ctx->term.osc_payload, "52;", 3) == 0) {  // Remote Clipboard (OSC 52)
        char *p = ctx->term.osc_payload + (strlen("52;") - 1);
        char target = (*p && *p != ';') ? *p : 'c';
        while (*p && *p != ';') p++;
        if (*p == ';' && *++p != '?' /* Skips read requests */) {
            size_t b64_len = ctx->term.osc_len - (p - ctx->term.osc_payload);
            size_t raw_len = 0;
            uint8_t *raw_data = _sfte_b64_decode((uint8_t *)p, b64_len, &raw_len);
            if (raw_data) {
                char *data = (char *)SFTE_MALLOC(raw_len + 1);
                memcpy(data, raw_data, raw_len);
                data[raw_len] = '\0';
                if (ctx->osc52_clipboard_cb) ctx->osc52_clipboard_cb(ctx->user_data, target, data);
                SFTE_FREE(data);
                SFTE_FREE(raw_data);
            }
        }
    }
#endif  // SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD
#if SFTE_HYPERLINKS
    else if (strncmp(ctx->term.osc_payload, "8;", 2) == 0) {  // hyperlink
        char *p = ctx->term.osc_payload + (strlen("8;") - 1);
        while (*p && *p != ';') p++;
        if (*p++ == ';') {
            if (*p == '\0')  // Empty URI means close the link
                ctx->term.cur_link_idx = 0;
            else {  // Check if URI is already in pool
                uint16_t found_idx = 0;
                for (uint16_t i = 1; i < ctx->term.link_pool_len; ++i)
                    if (strcmp(ctx->term.link_pool[i], p) == 0) {
                        found_idx = i;
                        break;
                    }

                // Add new URI if not found
                if (found_idx == 0 && ctx->term.link_pool_len < SFTE_HYPERLINKS_MAX_CAP) {
                    if (ctx->term.link_pool_len >= ctx->term.link_pool_cap) {
                        ctx->term.link_pool_cap *= 2;
                        ctx->term.link_pool = (char **)SFTE_REALLOC(
                            ctx->term.link_pool, ctx->term.link_pool_cap * sizeof(char *));
                    }
                    size_t ulen = strlen(p);
                    char *uri = (char *)SFTE_MALLOC(ulen + 1);
                    memcpy(uri, p, ulen + 1);
                    found_idx = ctx->term.link_pool_len++;
                    ctx->term.link_pool[found_idx] = uri;
                }
                ctx->term.cur_link_idx = found_idx;
            }
        }
    }
#endif  // SFTE_HYPERLINKS
    else
        _SFTE_WARN(ctx, UNHANDLED_OSC, ctx->term.osc_payload);
}

/*
    Parses and executes completed DCS payloads.

    If payload begins with 'G', payload is passed to kitty graphics extension.
*/
static inline void _sfte_parser_dcs_dispatch(sfte_ctx *ctx, uint8_t terminator) {
    const char *term = (terminator == '\x1b') ? "\033\\" : "\x07";
    ctx->term.osc_payload[ctx->term.osc_len] = '\0';

#if SFTE_KITTY_GRAPHICS
    if (ctx->term.osc_payload[0] == 'G')
        _sfte_kitty_parse_graphics(ctx, ctx->term.osc_payload + (strlen("G") - 1));
    else
#endif  // SFTE_KITTY_GRAPHICS
        if (strncmp(ctx->term.osc_payload, "+q", 2) == 0) {
            char reply[128];
            int len = snprintf(reply, sizeof(reply), "\033P0+r%s%s",
                               ctx->term.osc_payload + (strlen("+q") - 1), term);
            if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, len);
        }
}

/*
    Core VT500 byte routing table.

    TODO:
    In-depth documentation of how this function behaves and why
*/
static void _sfte_parser_feed_byte(sfte_ctx *ctx, uint8_t b) {
    switch (b) {
    case '\n':
    case '\x0B':
    case '\x0C': _sfte_parser_c0_lf(ctx); return;
    case '\r': ctx->term.cursor_x = 0; return;
    case '\t': _sfte_parser_c0_ht(ctx); return;
    case '\b':
    case '\x7f':
        if (ctx->term.cursor_x > 0) ctx->term.cursor_x--;
        return;
    case '\a':
        if (ctx->bell_cb) ctx->bell_cb(ctx->user_data);
        return;
    default: break;
    }

    switch (ctx->term.vt_state) {
    case VT_GROUND:
        if (b == '\033' || b == '\x1b')
            ctx->term.vt_state = VT_ESCAPE;
        else if (b >= 0x20)
            if (_sfte_utf8_decode(ctx, b)) _sfte_utf8_insert_rune(ctx, ctx->term.utf8_rune);
        break;
    case VT_ESCAPE:
        if (b == '[') {
            ctx->term.vt_state = VT_CSI_ENTRY;
            ctx->term.vt_param_idx = 0;
            ctx->term.vt_dec_priv = 0;
            memset(ctx->term.vt_params, 0, sizeof(ctx->term.vt_params));
        } else if (b == ']') {
            ctx->term.vt_state = VT_OSC;
            ctx->term.osc_len = 0;
        } else if (b == 'c') {
            _sfte_parser_esc_ris(ctx);
            ctx->term.vt_state = VT_GROUND;
        } else if (b == '\\')
            ctx->term.vt_state = VT_GROUND;
        else if (b == 'P' || b == '_' || b == '^') {
            ctx->term.vt_state = VT_DCS;
            ctx->term.osc_len = 0;
        } else if (b == '(' || b == ')')
            ctx->term.vt_state = VT_CHARSET;
        else if (b == '7') {
            _sfte_parser_esc_sc(ctx);
            ctx->term.vt_state = VT_GROUND;
        } else if (b == '8') {
            _sfte_parser_esc_rc(ctx);
            ctx->term.vt_state = VT_GROUND;
        } else if (b == '#')
            ctx->term.vt_state = VT_HASH;
        else if (b == 'D') {
            _sfte_parser_esc_ind(ctx);
            ctx->term.vt_state = VT_GROUND;
        } else if (b == 'M') {
            _sfte_parser_esc_ri(ctx);
            ctx->term.vt_state = VT_GROUND;
        } else if (b == 'E') {
            _sfte_parser_esc_nel(ctx);
            ctx->term.vt_state = VT_GROUND;
        } else
            ctx->term.vt_state = VT_GROUND;
        break;
    case VT_HASH:
        if (b == '8') _sfte_parser_hash_decaln(ctx);
        ctx->term.vt_state = VT_GROUND;
        break;
    case VT_CHARSET: ctx->term.vt_state = VT_GROUND; break;
    case VT_OSC:
        if (b == '\x07' || b == '\x1b') {
            _sfte_parser_osc_dispatch(ctx, b);
            ctx->term.vt_state = (b == '\x1b') ? VT_ESCAPE : VT_GROUND;
        } else
            _sfte_parser_append_payload(ctx, b);
        break;
    case VT_DCS:
        if (b == '\x07' || b == '\x1b') {
            _sfte_parser_dcs_dispatch(ctx, b);
            ctx->term.vt_state = (b == '\x1b') ? VT_ESCAPE : VT_GROUND;
        }
#if SFTE_SIXEL
        else if (b == 'q') {
            // Detect if this dcs header is strictly sixel params (nums/semicols)
            int is_sixel = 1;
            for (size_t i = 0; i < ctx->term.osc_len; ++i) {
                char pb = ctx->term.osc_payload[i];
                if (pb != ';' && (pb < '0' || pb > '9')) {
                    is_sixel = 0;
                    break;
                }
            }
            if (is_sixel) {
                ctx->term.vt_state = VT_SIXEL;
                ctx->sixel = (sfte_sixel_state){
                    .state = SIXEL_GROUND,
                    .start_col = ctx->term.cursor_x,
                    .start_row = ctx->term.cursor_y,
                };
            } else
                _sfte_parser_append_payload(ctx, b);
        }
#endif  // SFTE_SIXEL
        else
            _sfte_parser_append_payload(ctx, b);
        break;
#if SFTE_SIXEL
    case VT_SIXEL:
        if (b == '\x1b' || b == '\x07') {
            _sfte_sixel_commit(ctx);
            ctx->term.vt_state = (b == '\x1b') ? VT_ESCAPE : VT_GROUND;
        } else
            _sfte_sixel_parse_byte(ctx, b);
        break;
#endif  // SFTE_SIXEL
    case VT_CSI_ENTRY:
    case VT_CSI_PARAM:
        if (b == '?') {  // private marker
            ctx->term.vt_state = VT_CSI_PARAM;
            ctx->term.vt_dec_priv = 1;
        } else if (b == '>') {
            ctx->term.vt_state = VT_CSI_PARAM;
            ctx->term.vt_dec_priv = 2;
        } else if (b >= '0' && b <= '9') {
            ctx->term.vt_state = VT_CSI_PARAM;
            ctx->term.vt_params[ctx->term.vt_param_idx] *= 10;
            ctx->term.vt_params[ctx->term.vt_param_idx] += (b - '0');
        } else if (b == ';' || b == ':') {
            if (ctx->term.vt_param_idx < 15) ctx->term.vt_param_idx++;  // Move to next parameter
        } else if (b >= 0x40 && b <= 0x7E) {
            _sfte_csi_dispatch(ctx, b);
            ctx->term.vt_state = VT_GROUND;
        }
    }
}
// =================================================================================================
// >>font
// =================================================================================================
#ifndef SFTE_CUSTOM_FONT_BACKEND
/*
    Default stb_truetype wrappers.
    Can be overriden by defining SFTE_CUSTOM_FONT_BACKEND.
*/
static inline void _sfte_stb_init(sfte_font_backend_info *info, const uint8_t *data) {
    stbtt_InitFont(info, data, 0);
}

static inline float _sfte_stb_get_scale(sfte_font_backend_info *info, float px_hei) {
    return stbtt_ScaleForPixelHeight(info, px_hei);
}

static inline void _sfte_stb_vmetrics(sfte_font_backend_info *info, int *ascent, int *descent,
                                      int *linegap) {
    stbtt_GetFontVMetrics(info, ascent, descent, linegap);
}

static inline int _sfte_stb_bounds(sfte_font_backend_info *info, uint32_t rune, float scale,
                                   int *adv, int *x0, int *y0, int *x1, int *y1) {
    int glyph_idx = stbtt_FindGlyphIndex(info, rune);
    if (glyph_idx == 0) return 0;

    int lsb;
    stbtt_GetGlyphHMetrics(info, glyph_idx, adv, &lsb);
    stbtt_GetGlyphBitmapBox(info, glyph_idx, scale, scale, x0, y0, x1, y1);

    return glyph_idx;
}

static inline void _sfte_stb_bake(sfte_font_backend_info *info, int glyph_idx, float scale,
                                  uint8_t *atlas_ptr, int gw, int gh, int atlas_stride) {
    stbtt_MakeGlyphBitmap(info, atlas_ptr, gw, gh, atlas_stride, scale, scale, glyph_idx);
}
#endif  // !SFTE_CUSTOM_FONT_BACKEND

/*
    Distance in pixels between each glyph in the atlas 2D texture.
*/
#define _SFTE_FONT_PADDING 1

/*
    Retrieves the active cache pool for a specific font style (regular/bold/italic/bold italic).
*/
static inline sfte_font_cache *_sfte_font_get_cache(sfte_ctx *ctx, int style) {
    if (style == SFTE_FONT_STYLE_REGULAR) return &ctx->font.regular;
#ifdef SFTE_FONT_BOLD
    if (style == SFTE_FONT_STYLE_BOLD) return &ctx->font.bold;
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    if (style == SFTE_FONT_STYLE_ITALIC) return &ctx->font.italic;
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    if (style == SFTE_FONT_STYLE_BOLD_ITALIC) return &ctx->font.bold_italic;
#endif  // SFTE_FONT_BOLD_ITALIC
    return NULL;
}

/*
    Clears a font cache's atlas texture and glyph hash map.
*/
static inline void _sfte_font_clear_cache(sfte_font_cache *cache) {
    if (cache->atlas_pxs) memset(cache->atlas_pxs, 0, SFTE_FONT_ATLAS_SIZE * SFTE_FONT_ATLAS_SIZE);
    if (cache->glyphs) memset(cache->glyphs, 0, SFTE_FONT_GLYPH_CAP * sizeof(sfte_glyph));
    cache->atlas_x = 0;
    cache->atlas_y = 0;
    cache->atlas_row_h = 0;
}

/*
    Recalculates font scales for the primary font and all fallbacks based on current size.
*/
static inline void _sfte_font_update_scales(sfte_ctx *ctx, sfte_font_cache *cache) {
    for (int i = 0; i < cache->num_fonts; ++i) {
        float tweak = _sfte_font_scales[i];
        if (tweak <= 0.0f) tweak = 1.0f;
        cache->scales[i] = SFTE_FONT_GET_SCALE(&cache->info[i], ctx->font.cur_size * tweak);
    }
}

/*
    Allocates space in the 2D texture atlas for a new glyph and bakes the pixels.
    Glyphs are packed sequentially into rows. If a row runs out of horizontal space,
    we step down by the height of the tallest glyph in that row.
*/
static inline void _sfte_font_pack_and_bake(sfte_font_cache *cache, sfte_glyph *g, int font_idx,
                                            int glyph_idx, int gw, int gh) {
    if (cache->atlas_x + gw >= SFTE_FONT_ATLAS_SIZE) {
        cache->atlas_x = 0;
        cache->atlas_y += cache->atlas_row_h + _SFTE_FONT_PADDING;
        cache->atlas_row_h = 0;
    }

    SFTE_ASSERT(cache->atlas_y + gh < SFTE_FONT_ATLAS_SIZE, "glyph atlas full");

    if (gh > cache->atlas_row_h) cache->atlas_row_h = gh;

    g->x0 = cache->atlas_x;
    g->y0 = cache->atlas_y;
    g->x1 = g->x0 + gw;
    g->y1 = g->y0 + gh;

    if (gw > 0 && gh > 0) {
        int atlas_idx = g->y0 * SFTE_FONT_ATLAS_SIZE + g->x0;
        SFTE_FONT_BAKE(&cache->info[font_idx], glyph_idx, cache->scales[font_idx],
                       &cache->atlas_pxs[atlas_idx], gw, gh, SFTE_FONT_ATLAS_SIZE);
    }

    cache->atlas_x += gw + _SFTE_FONT_PADDING;
}

/*
    Retrieves a cached glyph raster, or bakes a new one on cache miss.

    Handles cascading fallback fonts and cross-style fallbacks.
    So, for example, on request of a nerd symbol in bold,
    if it's not found in bold it falls back to regular,
    and if it's not found in main regular it fallbacks to lower-priority fonts,
    eventually finding the symbol.
*/
static sfte_glyph *_sfte_font_get_glyph(sfte_ctx *ctx, sfte_font_cache **cache_ptr, uint32_t rune) {
    sfte_font_cache *cache = *cache_ptr;
    if (rune == 0) rune = ' ';

    uint32_t h = rune % SFTE_FONT_GLYPH_CAP;

    // hash map logic
    for (int i = 0; i < SFTE_FONT_GLYPH_CAP; ++i) {
        int idx = (h + i) % SFTE_FONT_GLYPH_CAP;

        if (cache->glyphs[idx].rune == rune) return &cache->glyphs[idx];  // Cache hit
        if (cache->glyphs[idx].rune != 0) continue;                       // Collision

        // Cache miss, look up bounds across primary and fallback fonts
        int font_idx = 0;
        int glyph_idx = 0;
        int adv = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;

        // Start in primary font [0].
        // Then look through fallback fonts [1-SFTE_FONTS_MAX_COUNT]
        for (int f = 0; f < cache->num_fonts; ++f) {
            glyph_idx = SFTE_FONT_BOUNDS(&cache->info[f], rune, cache->scales[f], &adv, &x0, &y0,
                                         &x1, &y1);
            if (glyph_idx != 0) {
                font_idx = f;
                break;
            }
        }

        // If a glyph was not found and cache isn't regular, look in regular.
        // This is useful e.g. when a certain app tries to draw nerd symbols
        // in bold/italic/bold italic instead of regular.
        if (glyph_idx == 0 && cache != &ctx->font.regular) {
            *cache_ptr = &ctx->font.regular;
            return _sfte_font_get_glyph(ctx, cache_ptr, rune);
        }

        sfte_glyph *g = &cache->glyphs[idx];
        g->rune = rune;
        g->xadvance = (int)(adv * cache->scales[font_idx] + 0.5f);
        g->xoff = x0;
        g->yoff = y0;

        _sfte_font_pack_and_bake(cache, g, font_idx, glyph_idx, x1 - x0, y1 - y0);

        return g;
    }

    return NULL;
}

/*
    Purges all glyph atlases and recalculates strict terminal grid metrics.
    Must be called on startup, and whenever the DPI or font size changes.
*/
static void _sfte_font_reset_cache(sfte_ctx *ctx) {
    _sfte_font_clear_cache(&ctx->font.regular);
    _sfte_font_update_scales(ctx, &ctx->font.regular);

#ifdef SFTE_FONT_BOLD
    _sfte_font_clear_cache(&ctx->font.bold);
    _sfte_font_update_scales(ctx, &ctx->font.bold);
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    _sfte_font_clear_cache(&ctx->font.italic);
    _sfte_font_update_scales(ctx, &ctx->font.italic);
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    _sfte_font_clear_cache(&ctx->font.bold_italic);
    _sfte_font_update_scales(ctx, &ctx->font.bold_italic);
#endif  // SFTE_FONT_BOLD_ITALIC

    int ascent_u, descent_u, line_gap_u;
    SFTE_FONT_VMETRICS(&ctx->font.regular.info[0], &ascent_u, &descent_u, &line_gap_u);

    float primary_scale = ctx->font.regular.scales[0];
    ctx->font.ascent = (int)(ascent_u * primary_scale + 0.5f);
    ctx->font.descent = (int)(descent_u * primary_scale -
                              0.5f /* - instead of + because descent is natively negative */);
    ctx->font.line_gap = (int)(line_gap_u * primary_scale + 0.5f);
    ctx->font.cell_height = ctx->font.ascent - ctx->font.descent + ctx->font.line_gap;

    // NOTE:
    // Terminal column width is locked to advance of 'M'.
    sfte_font_cache *dummy = &ctx->font.regular;
    sfte_glyph *m = _sfte_font_get_glyph(ctx, &dummy, 'M');
    ctx->font.cell_width = m->xadvance;
}
// =================================================================================================
// >>render
// =================================================================================================
#if SFTE_CURSOR_DYNAMIC
#define _SFTE_CUR_STYLE(ctx) (ctx->term.cursor_style)
#else
#define _SFTE_CUR_STYLE(ctx) (SFTE_CURSOR_STYLE)
#endif  // !SFTE_CURSOR_DYNAMIC

#define SFTE_UL_STYLE_STRAIGHT 1
#if SFTE_EXT_UNDERLINES
#define SFTE_UL_STYLE_DOUBLE 2
#define SFTE_UL_STYLE_CURLED 3
#define SFTE_UL_STYLE_DOTTED 4
#define SFTE_UL_STYLE_DASHED 5
#endif  // SFTE_EXT_UNDERLINES

/*
    Safely expands a bounding box to encompass a new dirty region.
*/
static inline void _sfte_render_damage_add(int *x0, int *y0, int *x1, int *y1, int px, int py,
                                           int pw, int ph) {
    if (px < *x0) *x0 = px;
    if (py < *y0) *y0 = py;
    if (px + pw > *x1) *x1 = px + pw;
    if (py + ph > *y1) *y1 = py + ph;
}

/*
    Evaluates cursor movement and font bleed.

    Fonts often spill slightly outside their strict grid cell bounds.
    If we only redraw the specific cell that changed, we slice off the edges of adjacent letters.
    This pass detects damaged cells and intentionally bleeds the dirty flag
    to adjacent rows and columns to guarantee seamless redrawing.
*/
static inline void _sfte_render_propagate_damage(sfte_ctx *ctx, int vis_cx, int vis_cy) {
    if (ctx->term.dirty_saved_x != vis_cx || ctx->term.dirty_saved_y != vis_cy) {
        if (ctx->term.dirty_saved_x >= 0 && ctx->term.dirty_saved_x < ctx->term.cols &&
            ctx->term.dirty_saved_y >= 0 && ctx->term.dirty_saved_y < ctx->term.rows)
            ctx->term.cells[_SFTE_GRID_IDX(ctx, ctx->term.dirty_saved_x, ctx->term.dirty_saved_y)]
                .dirty = 1;

        if (vis_cx >= 0 && vis_cx < ctx->term.cols && vis_cy >= 0 && vis_cy < ctx->term.rows)
            ctx->term.cells[_SFTE_GRID_IDX(ctx, vis_cx, vis_cy)].dirty = 1;

        ctx->term.dirty_saved_x = vis_cx;
        ctx->term.dirty_saved_y = vis_cy;
    }

#if SFTE_FONT_BLEED
    for (int r = 0; r < ctx->term.rows; ++r) {
        int row_has_damage = 0;
        for (int c = 0; c < ctx->term.cols; ++c)
            if (ctx->term.cells[_SFTE_GRID_IDX(ctx, c, r)].dirty) {
                row_has_damage = 1;
                break;
            }

        if (row_has_damage) {
            int r_min = r > 0 ? r - 1 : 0;
            int r_max = r < ctx->term.rows - 1 ? r + 1 : ctx->term.rows - 1;

            // NOTE:
            // Mark padding as dirty to clear the bleed area.
            // it's not hidden behind an `if (r_min == 0 || r_max == ctx->term.rows - 1)`,
            // because if damage is at left or right edge, the bleed area will be visible there too.
            ctx->padding_dirty = 1;

            for (int y = r_min; y <= r_max; ++y)
                for (int x = 0; x < ctx->term.cols; ++x)
                    if (!ctx->term.cells[_SFTE_GRID_IDX(ctx, x, y)].dirty)
                        ctx->term.cells[_SFTE_GRID_IDX(ctx, x, y)].dirty = 2;  // bleed-dirty
        }
    }

    // Normalize bleed-dirty flags back to standard dirty flags
    for (int i = 0; i < ctx->term.rows * ctx->term.cols; ++i)
        if (ctx->term.cells[i].dirty == 2) ctx->term.cells[i].dirty = 1;
#endif  // SFTE_FONT_BLEED
}

#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
/*
    Sorts active image placements by z index using a fast insertion sort.
*/
static inline void _sfte_render_sort_images(sfte_ctx *ctx) {
    for (uint32_t i = 1; i < ctx->term.img_placements_len; ++i) {
        sfte_img_placement key = ctx->term.img_placements[i];
        int j = i - 1;
        while (j >= 0 && ctx->term.img_placements[j].z_idx > key.z_idx) {
            ctx->term.img_placements[j + 1] = ctx->term.img_placements[j];
            j--;
        }
        ctx->term.img_placements[j + 1] = key;
    }
}
/*
    Compositor pass for images.
    Images can be drawn behind text (is_bg_pass = 1) or in front of text (is_bg_pass = 0).
*/
static inline void _sfte_render_images(sfte_ctx *ctx, uint32_t *px_buf, int *b_x0, int *b_y0,
                                       int *b_x1, int *b_y1, uint8_t is_bg_pass, int base_y_off,
                                       uint8_t pad_was_dirty) {
    for (uint32_t i = 0; i < ctx->term.img_placements_len; ++i) {
        sfte_img_placement *p = &ctx->term.img_placements[i];
        if (p->alt_screen != ctx->term.alt_active) continue;

        uint8_t is_bg_img = (p->z_idx < 0);
        if (is_bg_img != is_bg_pass) continue;

        sfte_img *img = _sfte_img_find(ctx, p->img_id);
        if (!img) continue;

        int base_x = (p->start_col * ctx->font.cell_width) + SFTE_PAD_X + p->x_off;
        int base_y = (p->start_row * ctx->font.cell_height) + SFTE_PAD_Y + p->y_off + base_y_off;

        int draw_w = _SFTE_CLAMP(img->width, 0, ctx->width - base_x);
        int draw_h = _SFTE_CLAMP(img->height, 0, ctx->height - base_y);

        int dmg_x = base_x, dmg_y = base_y, dmg_w = draw_w, dmg_h = draw_h;
        if (dmg_x < 0) {
            dmg_w += dmg_x;
            dmg_x = 0;
        }
        if (dmg_y < 0) {
            dmg_h += dmg_y;
            dmg_y = 0;
        }

        if (dmg_w && dmg_h)
            _sfte_render_damage_add(b_x0, b_y0, b_x1, b_y1, dmg_x, dmg_y, dmg_w, dmg_h);

        for (int iy = 0; iy < img->height; ++iy) {
            int out_y = base_y + iy;
            if (out_y < 0 || out_y >= ctx->height) continue;

            for (int ix = 0; ix < img->width; ++ix) {
                int out_x = base_x + ix;
                if (out_x < 0 || out_x >= ctx->width) continue;

                uint8_t is_dirty = 0;
                if (out_x < SFTE_PAD_X || out_y < SFTE_PAD_Y || out_x >= ctx->width - SFTE_PAD_X ||
                    out_y >= ctx->height - SFTE_PAD_Y) {
                    is_dirty = pad_was_dirty;
                } else {
                    int grid_c = (out_x - SFTE_PAD_X) / ctx->font.cell_width;
                    int grid_r = (out_y - SFTE_PAD_Y) / ctx->font.cell_height;
                    is_dirty = ctx->term.cells[_SFTE_GRID_IDX(ctx, grid_c, grid_r)].dirty;
                }
                if (!is_dirty) continue;

                uint32_t img_pxs = img->pxs[iy * img->width + ix];
                if (!(img_pxs & SFTE_COLOR_ALPHA_MASK)) continue;

                px_buf[out_y * ctx->width + out_x] = _sfte_render_blend_argb(
                    px_buf[out_y * ctx->width + out_x], img_pxs, (uint8_t)(img_pxs >> 24));
            }
        }
    }
}
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS

/*
    Fast integer-based alpha blending.
    Used for cursor trails and antialiased font rendering to avoid slow floating-point math.
*/
static inline uint32_t _sfte_render_blend_argb(uint32_t dst, uint32_t src_col, uint8_t src_a) {
    if (src_a == 0) return dst;                                             // no trail
    if (src_a == 255) return SFTE_COLOR_ALPHA_MASK | (src_col & 0xFFFFFF);  // solid trail

    uint8_t da = (dst >> 24) & 0xFF;
    uint8_t dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    uint8_t sr = (src_col >> 16) & 0xFF, sg = (src_col >> 8) & 0xFF, sb = src_col & 0xFF;

    uint8_t out_r = (sr * src_a + dr * (255 - src_a)) >> 8;
    uint8_t out_g = (sg * src_a + dg * (255 - src_a)) >> 8;
    uint8_t out_b = (sb * src_a + db * (255 - src_a)) >> 8;
    uint8_t out_a = da + ((src_a * (255 - da)) >> 8);

    return (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

/*
    Paints the solid background color for a terminal cell.
*/
static void _sfte_render_bg_cell(sfte_ctx *ctx, uint32_t *px_buf, int col, int row, uint32_t bg) {
    int cx = col * ctx->font.cell_width + SFTE_PAD_X;
    int cy = row * ctx->font.cell_height + SFTE_PAD_Y;
    uint32_t final_bg = (SFTE_BG_OPACITY << 24) | (bg & ~SFTE_COLOR_ALPHA_MASK);

    for (int y = 0; y < ctx->font.cell_height; ++y) {
        for (int x = 0; x < ctx->font.cell_width; ++x) {
            int px_idx = (cy + y) * ctx->width + (cx + x);
            if (px_idx < ctx->width * ctx->height) px_buf[px_idx] = final_bg;
        }
    }
}

/*
    Samples the font atlas and paints a glyph.
    The texture atlas only stores alpha values.
    It blends the requested foreground color into the existing background using this alpha mask.
*/
static void _sfte_render_fg_cell(sfte_ctx *ctx, uint32_t *px_buf, int col, int row, uint32_t rune,
                                 uint32_t fg, sfte_font_cache *target_cache) {
    if (rune == ' ') return;

    sfte_font_cache *actual_cache = target_cache;
    sfte_glyph *g = _sfte_font_get_glyph(ctx, &actual_cache, rune);
    if (!g) return;

    int cx = col * ctx->font.cell_width + SFTE_PAD_X;
    int cy = row * ctx->font.cell_height + SFTE_PAD_Y;

    int glyph_width = g->x1 - g->x0;
    int glyph_height = g->y1 - g->y0;

    int draw_x = cx + (int)g->xoff;
    int draw_y = cy + ctx->font.ascent + (int)g->yoff;

    uint8_t fg_r = (fg >> 16) & 0xFF, fg_g = (fg >> 8) & 0xFF, fg_b = fg & 0xFF;

    for (int y = 0; y < glyph_height; ++y) {
        for (int x = 0; x < glyph_width; ++x) {
            int screen_x = draw_x + x;
            int screen_y = draw_y + y;
            if (screen_x < 0 || screen_x >= ctx->width || screen_y < 0 || screen_y >= ctx->height)
                continue;

            uint8_t alpha = actual_cache
                                ->atlas_pxs[(g->y0 + y) * SFTE_FONT_ATLAS_SIZE + (g->x0 + x)];
            if (alpha == 0) continue;

            int px_idx = screen_y * ctx->width + screen_x;

            if (alpha == 255)
                px_buf[px_idx] = SFTE_COLOR_ALPHA_MASK | (fg & ~SFTE_COLOR_ALPHA_MASK);
            else {
                uint32_t dst = px_buf[px_idx];
                uint8_t bg_r = (dst >> 16) & 0xFF, bg_g = (dst >> 8) & 0xFF, bg_b = dst & 0xFF;

                uint8_t col_r = (fg_r * alpha + bg_r * (255 - alpha)) >> 8;
                uint8_t col_g = (fg_g * alpha + bg_g * (255 - alpha)) >> 8;
                uint8_t col_b = (fg_b * alpha + bg_b * (255 - alpha)) >> 8;

                px_buf[px_idx] = (SFTE_BG_OPACITY << 24) | (col_r << 16) | (col_g << 8) | col_b;
            }
        }
    }
}

/*
    Renders extended underline styles (straight, double, undercurl, dotted, dotted, dashed).

    Undercurls require a periodic wave. To avoid slow math calls per-pixel, this implementation
    approximates a triangle wave using integer mod arithmetic.
*/
static inline void _sfte_render_underline_cell(sfte_ctx *ctx, uint32_t *px_buf, int cx, int cy,
                                               int render_w, sfte_cell *vcell) {
    uint32_t base_ul_col = vcell->fg;
#if SFTE_COLOR_UNDERLINE
    if (vcell->ul_color != 0xFFFFFFFF) base_ul_col = vcell->ul_color;
#endif  // SFTE_COLOR_UNDERLINE
    uint32_t underline_col = SFTE_COLOR_ALPHA_MASK | (base_ul_col & ~SFTE_COLOR_ALPHA_MASK);

    int thick = (int)(ctx->font.cell_height * SFTE_UNDERLINE_THICK_RATIO);
    if (thick < 1) thick = 1;

    int style = SFTE_UL_STYLE_STRAIGHT;
#if SFTE_EXT_UNDERLINES
    style = _SFTE_CLAMP(vcell->ul_style, SFTE_UL_STYLE_STRAIGHT, SFTE_UL_STYLE_DASHED);
#endif  // SFTE_EXT_UNDERLINES

    int offset = (int)(ctx->font.cell_height * SFTE_UNDERLINE_OFFSET_RATIO);
    if (offset < 1) offset = 1;
    int base_y = cy + ctx->font.ascent + offset;
    if (base_y + thick > cy + ctx->font.cell_height) base_y = cy + ctx->font.cell_height - thick;

    for (int x = cx; x < cx + render_w; ++x) {
        if (x >= ctx->width) break;

        int grid_x = x - SFTE_PAD_X;
        int local_x = grid_x % ctx->font.cell_width;

        switch (style) {
        case SFTE_UL_STYLE_CURLED: {
            int half_w = ctx->font.cell_width / 2;
            if (half_w == 0) half_w = 1;

            int amp = thick + 1;
            int dist = local_x > half_w ? local_x - half_w : half_w - local_x;
            int y_off = (dist * amp) / half_w - (amp / 2);

            for (int dy = 0; dy < thick; ++dy) {
                int py = base_y + y_off + dy;
                if (py >= 0 && py < ctx->height) px_buf[py * ctx->width + x] = underline_col;
            }
            break;
        }
        case SFTE_UL_STYLE_DOUBLE: {
            int half_thick = thick / 2;
            if (half_thick < 1) half_thick = 1;
            int gap = half_thick < 2 ? 1 : half_thick;
            for (int dy = 0; dy < half_thick; ++dy) {
                int py1 = base_y - half_thick + dy;
                int py2 = base_y + gap + dy;
                if (py1 >= 0 && py1 < ctx->height) px_buf[py1 * ctx->width + x] = underline_col;
                if (py2 >= 0 && py2 < ctx->height) px_buf[py2 * ctx->width + x] = underline_col;
            }
            break;
        }
        case SFTE_UL_STYLE_DOTTED:
            if ((grid_x / thick) % 2 != 0) break;
        case SFTE_UL_STYLE_DASHED:
            if (style == SFTE_UL_STYLE_DASHED && ((grid_x / thick) % 5 >= 3)) break;
        case SFTE_UL_STYLE_STRAIGHT:
        default:
            for (int dy = 0; dy < thick; ++dy) {
                int py = base_y + dy;
                if (py >= 0 && py < ctx->height) px_buf[py * ctx->width + x] = underline_col;
            }
        }
    }
}

/*
    Renders non-block cursors (bar/underline).
    Block cursors are rendered naturally by inverting the cells background/foreground colors.
*/
static inline void _sfte_render_cursor_shape(sfte_ctx *ctx, uint32_t *px_buf, int cx, int cy,
                                             int render_w) {
    uint32_t cur_col = SFTE_COLOR_ALPHA_MASK | (SFTE_CURSOR_COLOR & ~SFTE_COLOR_ALPHA_MASK);

    if (_SFTE_CUR_STYLE(ctx) == SFTE_CURSOR_UNDERLINE) {
        int thick = (int)(ctx->font.cell_height * SFTE_CURSOR_THICK_RATIO);
        if (thick < 1) thick = 1;

        for (int y = cy + ctx->font.cell_height - thick; y < cy + ctx->font.cell_height; ++y)
            for (int x = cx; x < cx + render_w; ++x)
                if (x < ctx->width && y < ctx->height) px_buf[y * ctx->width + x] = cur_col;
    } else if (_SFTE_CUR_STYLE(ctx) == SFTE_CURSOR_BAR) {
        int thick = (int)(ctx->font.cell_width * SFTE_CURSOR_THICK_RATIO);
        if (thick < 1) thick = 1;

        for (int y = cy; y < cy + ctx->font.cell_height; ++y)
            for (int x = cx; x < cx + thick; ++x)
                if (x < ctx->width && y < ctx->height) px_buf[y * ctx->width + x] = cur_col;
    }
}

/*
    Dispatcher for terminal text decorations (underlines, cursor).
*/
static void _sfte_render_decorations_cell(sfte_ctx *ctx, uint32_t *px_buf, int c, int r,
                                          sfte_cell *vcell, uint8_t is_cursor) {
    int cx = c * ctx->font.cell_width + SFTE_PAD_X;
    int cy = r * ctx->font.cell_height + SFTE_PAD_Y;

    int render_w = ctx->font.cell_width;
#if SFTE_WIDE_CHARS
    render_w *= (vcell->attr & ATTR_WIDE) ? 2 : 1;
#endif  // SFTE_WIDE_CHARS

    if (vcell->attr & ATTR_UNDERLINE)
        _sfte_render_underline_cell(ctx, px_buf, cx, cy, render_w, vcell);
    if (is_cursor && _SFTE_CUR_STYLE(ctx) != SFTE_CURSOR_BLOCK)
        _sfte_render_cursor_shape(ctx, px_buf, cx, cy, render_w);
}

/*
    Background rendering pass.
    Renders the whole grid, contrary to `_sfte_render_bg_cell`.
*/
static inline void _sfte_render_bg_grid(sfte_ctx *ctx, uint32_t *px_buf, int vis_cx, int vis_cy) {
    for (int r = 0; r < ctx->term.rows; ++r)
        for (int c = 0; c < ctx->term.cols; ++c) {
            int idx = _SFTE_GRID_IDX(ctx, c, r);
            if (!ctx->term.cells[idx].dirty) continue;

            sfte_cell *vcell = _sfte_grid_get_cell(ctx, c, r);
            uint32_t fg = vcell->fg ? vcell->fg : 0xFFFFFFFF;
            uint32_t bg = vcell->bg ? vcell->bg : SFTE_BG_COLOR;
            uint16_t attr = vcell->attr;

#if SFTE_SELECTION
            int logical_r = r;
#if SFTE_SCROLLBACK_CAP
            logical_r -= ctx->term.sb_offset;
#endif
            if (_sfte_input_is_selected(ctx, c, logical_r)) attr |= ATTR_REVERSE;
#endif

            if (attr & ATTR_REVERSE) {
                uint32_t tmp = fg;
                fg = bg;
                bg = tmp;
            }

            uint8_t is_cursor = (c == vis_cx && r == vis_cy && !ctx->term.hide_cursor);
#if SFTE_WIDE_CHARS
            if (!is_cursor && (attr & ATTR_DUMMY) && c > 0 && c - 1 == vis_cx && r == vis_cy &&
                !ctx->term.hide_cursor)
                is_cursor = 1;
#endif
#if SFTE_CURSOR_BLINK
            if (!ctx->term.blink_visible) is_cursor = 0;
#endif

            if (is_cursor && _SFTE_CUR_STYLE(ctx) == SFTE_CURSOR_BLOCK)
                _sfte_render_bg_cell(ctx, px_buf, c, r, fg);  // Invert colors
            else
                _sfte_render_bg_cell(ctx, px_buf, c, r, bg);
        }
}

/*
    Foreground rendering pass.
    Renders the whole grid, contrary to `_sfte_render_fg_cell`.
*/
static inline void _sfte_render_fg_grid(sfte_ctx *ctx, uint32_t *px_buf, int vis_cx, int vis_cy,
                                        int *bx0, int *by0, int *bx1, int *by1) {
    for (int r = 0; r < ctx->term.rows; ++r) {
        for (int c = 0; c < ctx->term.cols; ++c) {
            int idx = _SFTE_GRID_IDX(ctx, c, r);
            if (!ctx->term.cells[idx].dirty) continue;

            sfte_cell *vcell = _sfte_grid_get_cell(ctx, c, r);
#if SFTE_WIDE_CHARS
            if (vcell->attr & ATTR_DUMMY) {
                _sfte_render_damage_add(bx0, by0, bx1, by1, c * ctx->font.cell_width + SFTE_PAD_X,
                                        r * ctx->font.cell_height + SFTE_PAD_Y,
                                        ctx->font.cell_width, ctx->font.cell_height);
                ctx->term.cells[idx].dirty = 0;
                continue;
            }
#endif

            uint32_t rune = vcell->rune ? vcell->rune : ' ';
            uint32_t fg = vcell->fg ? vcell->fg : 0xFFFFFFFF;
            uint32_t bg = vcell->bg ? vcell->bg : SFTE_BG_COLOR;
            uint16_t attr = vcell->attr;

            if (attr & ATTR_REVERSE) {
                uint32_t tmp = fg;
                fg = bg;
                bg = tmp;
            }
#ifdef SFTE_BOLD_WHITE
            if (attr & ATTR_BOLD) fg = 0xFFFFFFFF;
#endif

            uint8_t is_cursor = (c == vis_cx && r == vis_cy && !ctx->term.hide_cursor);
#if SFTE_CURSOR_BLINK
            if (!ctx->term.blink_visible) is_cursor = 0;
#endif
            uint32_t draw_fg = (is_cursor && _SFTE_CUR_STYLE(ctx) == SFTE_CURSOR_BLOCK) ? bg : fg;

            sfte_font_cache *target_cache = &ctx->font.regular;
#ifdef SFTE_FONT_BOLD_ITALIC
            if ((attr & ATTR_BOLD) && (attr & ATTR_ITALIC)) target_cache = &ctx->font.bold_italic;
#endif
#ifdef SFTE_FONT_BOLD
            else if (attr & ATTR_BOLD)
                target_cache = &ctx->font.bold;
#endif
#ifdef SFTE_FONT_ITALIC
            else if (attr & ATTR_ITALIC)
                target_cache = &ctx->font.italic;
#endif

            _sfte_render_fg_cell(ctx, px_buf, c, r, rune, draw_fg, target_cache);
            _sfte_render_decorations_cell(ctx, px_buf, c, r, vcell, is_cursor);

            int dmg_cy = r * ctx->font.cell_height + SFTE_PAD_Y;
            int dmg_ch = ctx->font.cell_height;

            if (r == 0) {
                dmg_cy = 0;
                dmg_ch += SFTE_PAD_Y;
            } else if (r == ctx->term.rows - 1) {
                dmg_ch += ctx->height - (dmg_cy + dmg_ch);
            }
            _sfte_render_damage_add(bx0, by0, bx1, by1, 0, dmg_cy, ctx->width, dmg_ch);
        }
    }
}

// =================================================================================================
// >>wayland
// =================================================================================================
#if SFTE_WAYLAND

/*
    Maps Linux XKB keysyms to the emulators internal key enum.
*/
static sfte_key _sfte_xkb_to_sfte_key(xkb_keysym_t sym) {
    switch (sym) {
    case XKB_KEY_Tab:
    case XKB_KEY_ISO_Left_Tab: return SFTE_KEY_TAB;
    case XKB_KEY_Return:
    case XKB_KEY_Linefeed:
    case XKB_KEY_KP_Enter: return SFTE_KEY_ENTER;
    case XKB_KEY_BackSpace: return SFTE_KEY_BACKSPACE;
    case XKB_KEY_Escape: return SFTE_KEY_ESCAPE;
    case XKB_KEY_Up: return SFTE_KEY_UP;
    case XKB_KEY_Down: return SFTE_KEY_DOWN;
    case XKB_KEY_Left: return SFTE_KEY_LEFT;
    case XKB_KEY_Right: return SFTE_KEY_RIGHT;
    case XKB_KEY_Home: return SFTE_KEY_HOME;
    case XKB_KEY_End: return SFTE_KEY_END;
    case XKB_KEY_Page_Up: return SFTE_KEY_PAGE_UP;
    case XKB_KEY_Page_Down: return SFTE_KEY_PAGE_DOWN;
    case XKB_KEY_Insert: return SFTE_KEY_INSERT;
    case XKB_KEY_Delete: return SFTE_KEY_DELETE;
    case XKB_KEY_F1: return SFTE_KEY_F1;
    case XKB_KEY_F2: return SFTE_KEY_F2;
    case XKB_KEY_F3: return SFTE_KEY_F3;
    case XKB_KEY_F4: return SFTE_KEY_F4;
    case XKB_KEY_F5: return SFTE_KEY_F5;
    case XKB_KEY_F6: return SFTE_KEY_F6;
    case XKB_KEY_F7: return SFTE_KEY_F7;
    case XKB_KEY_F8: return SFTE_KEY_F8;
    case XKB_KEY_F9: return SFTE_KEY_F9;
    case XKB_KEY_F10: return SFTE_KEY_F10;
    case XKB_KEY_F11: return SFTE_KEY_F11;
    case XKB_KEY_F12: return SFTE_KEY_F12;
    default: return SFTE_KEY_NONE;
    }
}

/*
    Callback triggered by the emulator core to send bytes back to the shell (PTY).
*/
static void _sfte_wayland_write_cb(void *user_data, const char *data, size_t len) {
    sfte_wayland_app *app = (sfte_wayland_app *)user_data;
    if (app->pty_fd > 0) write(app->pty_fd, data, len);
}

static void _sfte_wayland_pty_spawn(sfte_wayland_app *app) {
#ifndef SFTE_NO_POSIX
    app->pty_pid = sfte_posix_pty_spawn(app->ctx, &app->pty_fd, app->width, app->height);
    SFTE_ASSERT(app->pty_pid != -1, "failed to forkpty");
#endif  // !SFTE_NO_POSIX
}

static void _sfte_wayland_pty_update(sfte_wayland_app *app) {
#ifndef SFTE_NO_POSIX
    sfte_posix_pty_resize(app->ctx, app->pty_fd, app->width, app->height);
#endif  // !SFTE_NO_POSIX
}

#if SFTE_FONT_ZOOM
static void _sfte_wayland_font_resize(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_zoom(ctx, arg->f);
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->user_data;
    _sfte_wayland_pty_update(app);
    app->needs_render = 1;
}

static void _sfte_wayland_font_reset(sfte_ctx *ctx, const sfte_arg *dummy) {
    (void)dummy;
    const sfte_arg arg = {.f = SFTE_FONT_DEFAULT_SIZE - ctx->font.cur_size};
    _sfte_wayland_font_resize(ctx, &arg);
}
#endif  // SFTE_FONT_ZOOM

#if SFTE_SCROLLBACK_CAP
static void _sfte_wayland_view_scroll(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_view_scroll(ctx, arg->i);
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->user_data;
    app->needs_render = 1;
}
#endif  // SFTE_SCROLLBACK_CAP

/*
    Allocates a SHared Memory (SHM) buffer that both the emulator and
    the Wayland compositor can access simultaneously.
*/
static void _sfte_wayland_create_buffer(sfte_wayland_app *app) {
    int stride = app->width * 4;  // 4 bytes per pixel (ARGB8888)
    app->shm_size = stride * app->height;

    // memfd_create provides an anonymous file descriptor backed by RAM, not disk
    int fd = memfd_create("sfte-buffer", MFD_CLOEXEC);
    SFTE_ASSERT(fd != -1, "failed to create memfd");
    SFTE_ASSERT(ftruncate(fd, app->shm_size) != -1, "failed to truncate memfd");

    app->shm_data = (uint32_t *)mmap(NULL, app->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                     0);
    SFTE_ASSERT(app->shm_data != MAP_FAILED, "failed to mmap shm data");

    // NOTE:
    // If enabled, we allocate a secondary heap buffer for the emulator to draw into.
    // Once drawing is complete, we memcpy the damaged regions into the Wayland SHM
    // buffer to prevent the compositor from displaying half-drawn frames.
#if SFTE_DOUBLE_BUFFER
    if (app->back_buffer) SFTE_FREE(app->back_buffer);
    app->back_buffer = (uint32_t *)SFTE_MALLOC(app->shm_size);
    SFTE_ASSERT(app->back_buffer, "failed to allocate back buffer");
#endif  // SFTE_DOUBLE_BUFFER

    struct wl_shm_pool *pool = wl_shm_create_pool(app->shm, fd, app->shm_size);
    app->buffer = wl_shm_pool_create_buffer(pool, 0, app->width, app->height, stride,
                                            WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
}

#if SFTE_CLIPBOARD
static void _sfte_wayland_data_offer_offer(void *data, struct wl_data_offer *offer,
                                           const char *mime_type) {
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (strcmp(mime_type, "text/plain;charset=utf-8") == 0 ||
        strcmp(mime_type, "text/plain") == 0) {
        wl_data_offer_accept(offer, app->serial, mime_type);
    }
}

static void _sfte_wayland_data_offer_source_actions(void *data, struct wl_data_offer *offer,
                                                    uint32_t actions) {
    (void)data, (void)offer, (void)actions;
}

static void _sfte_wayland_data_offer_action(void *data, struct wl_data_offer *offer,
                                            uint32_t action) {
    (void)data, (void)offer, (void)action;
}

static const struct wl_data_offer_listener _sfte_wayland_data_offer_listener = {
    .offer = _sfte_wayland_data_offer_offer,
    .source_actions = _sfte_wayland_data_offer_source_actions,
    .action = _sfte_wayland_data_offer_action,
};

static void _sfte_wayland_data_device_data_offer(void *data, struct wl_data_device *device,
                                                 struct wl_data_offer *offer) {
    (void)device;
    wl_data_offer_add_listener(offer, &_sfte_wayland_data_offer_listener, data);
}

static void _sfte_wayland_data_device_enter(void *data, struct wl_data_device *device,
                                            uint32_t serial, struct wl_surface *surface,
                                            wl_fixed_t x, wl_fixed_t y,
                                            struct wl_data_offer *offer) {
    (void)data, (void)device, (void)serial, (void)surface, (void)x, (void)y, (void)offer;
}

static void _sfte_wayland_data_device_leave(void *data, struct wl_data_device *device) {
    (void)data, (void)device;
}

static void _sfte_wayland_data_device_motion(void *data, struct wl_data_device *device,
                                             uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)data, (void)device, (void)time, (void)x, (void)y;
}

static void _sfte_wayland_data_device_drop(void *data, struct wl_data_device *device) {
    (void)data, (void)device;
}

static void _sfte_wayland_data_device_selection(void *data, struct wl_data_device *device,
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

static void _sfte_wayland_data_source_target(void *data, struct wl_data_source *src,
                                             const char *mime_type) {
    (void)data, (void)src, (void)mime_type;
}

static void _sfte_wayland_data_source_send(void *data, struct wl_data_source *src,
                                           const char *mime_type, int32_t fd) {
    (void)src, (void)mime_type;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (app->selection_text) write(fd, app->selection_text, strlen(app->selection_text));

    close(fd);
}

static void _sfte_wayland_data_source_cancelled(void *data, struct wl_data_source *src) {
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    wl_data_source_destroy(src);
    if (app->selection_text) {
        SFTE_FREE(app->selection_text);
        app->selection_text = NULL;
    }
    app->data_source = NULL;
}

static void _sfte_wayland_data_source_dnd_drop_performed(void *data, struct wl_data_source *src) {
    (void)data, (void)src;
}

static void _sfte_wayland_data_source_dnd_finished(void *data, struct wl_data_source *src) {
    (void)data, (void)src;
}

static void _sfte_wayland_data_source_action(void *data, struct wl_data_source *src,
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

#if SFTE_SELECTION
static void _sfte_wayland_pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
                                        struct wl_surface *surface, wl_fixed_t surface_x,
                                        wl_fixed_t surface_y) {
    (void)data, (void)pointer, (void)serial, (void)surface, (void)surface_x, (void)surface_y;
#if SFTE_MOUSE
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    sfte_mouse_move(app->ctx, wl_fixed_to_int(surface_x), wl_fixed_to_int(surface_y));
    app->needs_render = 1;
#endif  // SFTE_MOUSE
}

static void _sfte_wayland_pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial,
                                        struct wl_surface *surface) {
    (void)data, (void)pointer, (void)serial, (void)surface;
}

static void _sfte_wayland_pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time,
                                         wl_fixed_t surface_x, wl_fixed_t surface_y) {
    (void)data, (void)pointer, (void)time, (void)surface_x, (void)surface_y;
#if SFTE_MOUSE
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    sfte_mouse_move(app->ctx, wl_fixed_to_int(surface_x), wl_fixed_to_int(surface_y));
    app->needs_render = 1;
#endif  // SFTE_MOUSE
}

static void _sfte_wayland_pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                                         uint32_t time, uint32_t button, uint32_t state) {
    (void)data, (void)pointer, (void)serial, (void)time, (void)button, (void)state;
#if SFTE_MOUSE
    if (button != 0x110) return;  // lmb

    sfte_wayland_app *app = (sfte_wayland_app *)data;

#if SFTE_CLIPBOARD
    app->serial = serial;
#endif  // SFTE_CLIPBOARD

    sfte_mouse_click(app->ctx, 0, state == WL_POINTER_BUTTON_STATE_PRESSED,
                     app->ctx->term.mouse_hover_x * app->ctx->font.cell_width + SFTE_PAD_X,
                     app->ctx->term.mouse_hover_y * app->ctx->font.cell_height + SFTE_PAD_Y);
    app->needs_render = 1;

#if SFTE_CLIPBOARD
    if (state == WL_POINTER_BUTTON_STATE_RELEASED) _sfte_wayland_clipboard_copy(app->ctx, NULL);
#endif  // SFTE_CLIPBOARD
#endif  // SFTE_MOUSE
}

static void _sfte_wayland_pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time,
                                       uint32_t axis, wl_fixed_t value) {
    (void)data, (void)pointer, (void)time, (void)axis, (void)value;
#if SFTE_MOUSE
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) return;

    sfte_wayland_app *app = (sfte_wayland_app *)data;
    int dir = (wl_fixed_to_double(value) < 0) ? 1 : -1;
    sfte_mouse_scroll(app->ctx, dir,
                      app->ctx->term.mouse_hover_x * app->ctx->font.cell_width + SFTE_PAD_X,
                      app->ctx->term.mouse_hover_y * app->ctx->font.cell_height + SFTE_PAD_Y);
    app->needs_render = 1;
#endif  // SFTE_MOUSE
}

static void _sfte_wayland_pointer_frame(void *data, struct wl_pointer *pointer) {
    (void)data, (void)pointer;
}

static void _sfte_wayland_pointer_axis_source(void *data, struct wl_pointer *pointer,
                                              uint32_t axis_source) {
    (void)data, (void)pointer, (void)axis_source;
}

static void _sfte_wayland_pointer_axis_stop(void *data, struct wl_pointer *pointer, uint32_t time,
                                            uint32_t axis) {
    (void)data, (void)pointer, (void)time, (void)axis;
}

static void _sfte_wayland_pointer_axis_discrete(void *data, struct wl_pointer *pointer,
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
#endif  // SFTE_SELECTION

#if SFTE_HYPERLINKS
static void _sfte_wayland_open_link_cb(void *user_data, const char *uri) {
    (void)user_data;
    if (!uri) return;

    if (fork() == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execlp("xdg-open", "xdg-open", uri, NULL);
        exit(1);
    }
}
#endif  // SFTE_HYPERLINKS

static void _sfte_wayland_keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format,
                                          int32_t fd, uint32_t size) {
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

static void _sfte_wayland_keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                                         struct wl_surface *surface, struct wl_array *keys) {
    (void)data, (void)keyboard, (void)serial, (void)surface, (void)keys;
}

static void _sfte_wayland_keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                                         struct wl_surface *surface) {
    (void)data, (void)keyboard, (void)serial, (void)surface;
}

static void _sfte_wayland_keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                                       uint32_t time, uint32_t key, uint32_t state) {
    (void)data, (void)keyboard, (void)serial, (void)time;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
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

    // clear selection on key press
    if (app->ctx->term.mouse_sel_active) {
        app->ctx->term.mouse_sel_active = 0;
        _sfte_grid_dirty_range(app->ctx, 0, app->ctx->term.cols * app->ctx->term.rows);
        app->needs_render = 1;
    }

    if (app->repeat_rate > 0 && app->repeating_key != key) {
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

    xkb_keycode_t keycode = key + 8;  // WARN: evdev codes are offset by 8 from xkb keycodes
    xkb_keysym_t sym = xkb_state_key_get_one_sym(app->xkb_state, keycode);

    bool ctrl = xkb_state_mod_name_is_active(app->xkb_state, XKB_MOD_NAME_CTRL,
                                             XKB_STATE_MODS_EFFECTIVE);
    bool alt = xkb_state_mod_name_is_active(app->xkb_state, XKB_MOD_NAME_ALT,
                                            XKB_STATE_MODS_EFFECTIVE);
    bool shift = xkb_state_mod_name_is_active(app->xkb_state, XKB_MOD_NAME_SHIFT,
                                              XKB_STATE_MODS_EFFECTIVE);
    bool super = xkb_state_mod_name_is_active(app->xkb_state, XKB_MOD_NAME_LOGO,
                                              XKB_STATE_MODS_EFFECTIVE);

    uint32_t active_mods = SFTE_MOD_NONE;
    if (ctrl) active_mods |= SFTE_MOD_CTRL;
    if (alt) active_mods |= SFTE_MOD_ALT;
    if (shift) active_mods |= SFTE_MOD_SHIFT;
    if (super) active_mods |= SFTE_MOD_SUPER;

    for (size_t i = 0; i < _SFTE_ARRAY_LEN(_sfte_shortcuts); ++i) {
        if ((xkb_keysym_t)_sfte_shortcuts[i].keysym != sym ||
            _sfte_shortcuts[i].mod_mask != active_mods)
            continue;

        _sfte_shortcuts[i].func(app->ctx, &_sfte_shortcuts[i].arg);
        return;
    }

    char buf[128];
    int size = 0;

    // ignore standalone mod keys
    if (sym >= XKB_KEY_Shift_L && sym <= XKB_KEY_Hyper_R) return;

#if SFTE_KITTY_KB
    sfte_key key_id = _sfte_xkb_to_sfte_key(sym);
    uint32_t codepoint = xkb_keysym_to_utf32(sym);

    size = sfte_kitty_kb_encode(app->ctx, key_id, codepoint, active_mods, buf, sizeof(buf));
#endif  // SFTE_KITTY_KB

    // if unhandled by kitty
    if (size == 0) {
#define MAP_KEY(str)                                                                               \
    do {                                                                                           \
        size = sizeof(str) - 1;                                                                    \
        memcpy(buf, str, size);                                                                    \
    } while (0)

        switch (sym) {
        case XKB_KEY_Up: MAP_KEY("\033[A"); break;
        case XKB_KEY_Down: MAP_KEY("\033[B"); break;
        case XKB_KEY_Right: MAP_KEY("\033[C"); break;
        case XKB_KEY_Left: MAP_KEY("\033[D"); break;
        case XKB_KEY_BackSpace: MAP_KEY("\x7f"); break;
        case XKB_KEY_Delete: MAP_KEY("\033[3~"); break;
        case XKB_KEY_Home: MAP_KEY("\033[H"); break;
        case XKB_KEY_End: MAP_KEY("\033[F"); break;
        default:
            if (ctrl) {
                if (sym >= XKB_KEY_a && sym <= XKB_KEY_z) {
                    buf[0] = sym - XKB_KEY_a + 1;
                    size = 1;
                } else if (sym >= XKB_KEY_A && sym <= XKB_KEY_Z) {
                    buf[0] = sym - XKB_KEY_A + 1;
                    size = 1;
                } else if (sym == XKB_KEY_space) {
                    buf[0] = '\0';
                    size = 1;
                }
            }

            // if nothing intercepted the key, default to generating
            // standard terminal control chars or psasing through the raw utf8 string
            if (size == 0) size = xkb_state_key_get_utf8(app->xkb_state, keycode, buf, sizeof(buf));
        }

        // if alt is held, prepend esc byte
        if (alt && size > 0 && size < (int)(sizeof(buf) - 1)) {
            memmove(buf + 1, buf, size++);
            buf[0] = '\033';
        }
#undef MAP_KEY
    }

    if (size > 0) {
#if SFTE_SCROLLBACK_CAP
        sfte_term *term = &app->ctx->term;
        if (term->sb_offset > 0) {
            term->sb_offset = 0;
            _sfte_grid_dirty_range(app->ctx, 0, term->cols * term->rows);
            app->needs_render = 1;
        }
#endif  // SFTE_SCROLLBACK_CAP

        write(app->pty_fd, buf, size);
    }
}

static void _sfte_wayland_keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                                             uint32_t serial, uint32_t mods_depressed,
                                             uint32_t mods_latched, uint32_t mods_locked,
                                             uint32_t group) {
    (void)keyboard, (void)serial;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (!app->xkb_state) return;

    xkb_state_update_mask(app->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void _sfte_wayland_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
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

static void _sfte_wayland_seat_capabilities(void *data, struct wl_seat *seat,
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

#if SFTE_SELECTION
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) {
        app->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(app->pointer, &_sfte_wayland_pointer_listener, app);

    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && app->pointer) {
        wl_pointer_release(app->pointer);
        app->pointer = NULL;
    }
#endif  // SFTE_SELECTION

#if SFTE_CLIPBOARD
    if (app->data_device_manager && !app->data_device) {
        app->data_device = (struct wl_data_device *)wl_data_device_manager_get_data_device(
            app->data_device_manager, seat);
        wl_data_device_add_listener(app->data_device, &_sfte_wayland_data_device_listener, app);
    }
#endif  // SFTE_CLIPBOARD
}

static void _sfte_wayland_seat_name(void *data, struct wl_seat *seat, const char *name) {
    (void)data, (void)seat, (void)name;
}

static const struct wl_seat_listener _sfte_wayland_seat_listener = {
    .capabilities = _sfte_wayland_seat_capabilities,
    .name = _sfte_wayland_seat_name,
};

static void _sfte_wayland_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                                           uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(xdg_wm_base, serial);  // compositor pinged, pong back with same serial
}

static const struct xdg_wm_base_listener _sfte_wayland_xdg_wm_base_listener = {
    .ping = _sfte_wayland_xdg_wm_base_ping,
};

static void _sfte_wayland_registry_global(void *data, struct wl_registry *registry, uint32_t name,
                                          const char *interface, uint32_t version) {
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

static void _sfte_wayland_registry_global_remove(void *data, struct wl_registry *registry,
                                                 uint32_t name) {
    (void)data, (void)registry, (void)name;
}

static const struct wl_registry_listener _sfte_wayland_registry_listener = {
    .global = _sfte_wayland_registry_global,
    .global_remove = _sfte_wayland_registry_global_remove,
};

static void _sfte_wayland_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
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
    int needed_size = app->width * app->height * 4;
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

static void _sfte_wayland_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                                                 int32_t width, int32_t height,
                                                 struct wl_array *states) {
    (void)xdg_toplevel, (void)states;
    if (width <= 0 || height <= 0) return;
    sfte_wayland_app *app = (sfte_wayland_app *)data;

    app->pending_width = width;
    app->pending_height = height;
}

static void _sfte_wayland_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
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
static void _sfte_wayland_load(sfte_wayland_app *app) {
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
static void _sfte_wayland_unload(sfte_wayland_app *app) {
#if SFTE_DOUBLE_BUFFER
    SFTE_FREE(app->back_buffer);
#endif  // SFTE_DOUBLE_BUFFER

    if (app->buffer) wl_buffer_destroy(app->buffer);
    if (app->shm_data) munmap(app->shm_data, app->shm_size);
    if (app->xdg_toplevel) xdg_toplevel_destroy(app->xdg_toplevel);
    if (app->xdg_surface) xdg_surface_destroy(app->xdg_surface);
    if (app->surface) wl_surface_destroy(app->surface);
    if (app->xdg_wm_base) xdg_wm_base_destroy(app->xdg_wm_base);
    if (app->keyboard) wl_keyboard_release(app->keyboard);
    if (app->seat) wl_seat_release(app->seat);

    wl_registry_destroy(app->registry);
    wl_display_disconnect(app->display);

    xkb_state_unref(app->xkb_state);
    xkb_keymap_unref(app->xkb_keymap);
    xkb_context_unref(app->xkb_context);
}

#if SFTE_CLIPBOARD
#if SFTE_SELECTION
#if SFTE_OSC52_CLIPBOARD
static void _sfte_wayland_osc52_clipboard_cb(void *user_data, char target, const char *data) {
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
#endif  // SFTE_OSC52_CLIPBOARD

static void _sfte_wayland_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->user_data;

    if (app->data_source) {
        wl_data_source_destroy(app->data_source);
        app->data_source = NULL;
    }
    if (app->selection_text) {
        SFTE_FREE(app->selection_text);
        app->selection_text = NULL;
    }

    if (!app->ctx->term.mouse_sel_active || !app->data_device_manager || !app->data_device) return;

    size_t needed_bytes = sfte_get_selection(app->ctx, NULL, 0);
    if (needed_bytes == 0) {
        _SFTE_INFO(ctx, CLIPBOARD_EMPTY);
        return;
    }

    app->selection_text = (char *)SFTE_MALLOC(needed_bytes);
    sfte_get_selection(app->ctx, app->selection_text, needed_bytes);
    if (!app->selection_text) return;

    app->data_source = wl_data_device_manager_create_data_source(app->data_device_manager);
    wl_data_source_add_listener(app->data_source, &_sfte_wayland_data_source_listener, app);
    wl_data_source_offer(app->data_source, "text/plain;charset=utf-8");
    wl_data_source_offer(app->data_source, "text/plain");
    wl_data_device_set_selection(app->data_device, app->data_source, app->serial);
}
#endif  // SFTE_SELECTION

static void _sfte_wayland_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->user_data;

    if (!app->data_offer) return;

    int fds[2];
    if (pipe(fds) == 0) {
        wl_data_offer_receive(app->data_offer, "text/plain;charset=utf-8", fds[1]);
        close(fds[1]);

        wl_display_roundtrip(app->display);

        if (app->ctx->term.bracketed_paste) write(app->pty_fd, "\033[200~", 6);

        char buf[SFTE_CLIPBOARD_BUF_SIZE];
        ssize_t n;
        while ((n = read(fds[0], buf, sizeof(buf))) > 0) write(app->pty_fd, buf, n);

        if (app->ctx->term.bracketed_paste) write(app->pty_fd, "\033[201~", 6);

        close(fds[0]);
    }
}
#endif  // SFTE_CLIPBOARD

/*
    The primary event loop for the terminal emulator using Wayland.
    Uses `poll` to simultaneously wait for Wayland compositor events,
    shell output events (PTY data), and timer expirations (on cursor blink, key repeat, trail).
*/
static void _sfte_wayland_loop(sfte_wayland_app *app) {
    sfte_ctx *ctx = app->ctx;

    signal(SIGPIPE, SIG_IGN);
    setlocale(LC_ALL, "");

    app->repeat_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    int wl_fd = wl_display_get_fd(app->display);

    while (app->running) {
        // Dispatch pending Wayland events before polling to prevent deadlock
        wl_display_dispatch_pending(app->display);
        wl_display_flush(app->display);
        struct pollfd fds[] = {{.fd = wl_fd, .events = POLLIN},
                               {.fd = app->pty_fd, .events = POLLIN},
                               {.fd = app->repeat_timer_fd, .events = POLLIN}};
        int timeout = -1 /* wait indefinetely by default */;

#if SFTE_CURSOR_TRAIL
        // If the cursor is moving, cap the poll timeout to 16ms (60fps) to animate the trail
        if (ctx->term.is_trailing)
            if (timeout == -1 || timeout > 16) timeout = 16;
#endif  // SFTE_CURSOR_TRAIL

#if SFTE_CURSOR_BLINK
        uint64_t now = SFTE_TIME_MS();
        if (ctx->term.blink_enabled) {
            int time_to_next = (int)(ctx->term.next_blink_ms - now);
            if (time_to_next < 0) time_to_next = 0;

            if (timeout == -1 || time_to_next < timeout) timeout = time_to_next;
        }
#endif  // SFTE_CURSOR_BLINK

        if (app->needs_render) timeout = 0;  // Don't sleep if we already know we need to draw

        if (poll(fds, _SFTE_ARRAY_LEN(fds), timeout) == -1) break;

#if SFTE_CURSOR_BLINK
        if (ctx->term.blink_enabled) {
            now = SFTE_TIME_MS();
            if (now >= ctx->term.next_blink_ms) {
                ctx->term.blink_visible ^= 1;
                ctx->term.next_blink_ms = now + SFTE_CURSOR_BLINK_RATE;
                int vis_cx = ctx->term.cursor_x >= ctx->term.cols ? ctx->term.cols - 1
                                                                  : ctx->term.cursor_x;
                ctx->term.cells[_SFTE_GRID_IDX(ctx, vis_cx, ctx->term.cursor_y)].dirty = 1;
                app->needs_render = 1;
            }
        }
#endif  // SFTE_CURSOR_BLINK

        // Handle incoming Wayland events (keys, resizes)
        if (fds[0].revents & (POLLIN | POLLERR | POLLHUP))
            if (wl_display_dispatch(app->display) == -1) app->running = 0;

        // Handle incoming text from the shell
        if (fds[1].revents & (POLLIN | POLLERR | POLLHUP)) {
            uint8_t buf[SFTE_PTY_BUF_SIZE];
            ssize_t n = read(app->pty_fd, buf, SFTE_PTY_BUF_SIZE);

            if (n > 0) {
                sfte_parse(app->ctx, buf, n);
                app->needs_render = 1;
            } else
                app->running = 0;  // Shell exited
        }

        // Handle key repeat timer
        if (fds[2].revents & POLLIN) {
            uint64_t expirations;
            if (read(app->repeat_timer_fd, &expirations, sizeof(expirations)) == 0 ||
                app->repeating_key == 0)
                continue;
            // Simulate a key press to autorepeat
            _sfte_wayland_keyboard_key(app, app->keyboard, 0, 0, app->repeating_key,
                                       WL_KEYBOARD_KEY_STATE_PRESSED);
        }

#if SFTE_CURSOR_TRAIL
        if (ctx->term.is_trailing) {
            int vis_cx = ctx->term.cursor_x >= ctx->term.cols ? ctx->term.cols - 1
                                                              : ctx->term.cursor_x;
            float target_rx = vis_cx * ctx->font.cell_width;
            float target_ry = ctx->term.cursor_y * ctx->font.cell_height;

#if !SFTE_CURSOR_BLINK
            uint64_t
#endif  // !SFTE_CURSOR_BLINK
                now = SFTE_TIME_MS();
            if (ctx->term.last_trail_update_ms == 0) ctx->term.last_trail_update_ms = now;
            float dt_ms = (float)(now - ctx->term.last_trail_update_ms);
            ctx->term.last_trail_update_ms = now;

            float tx = target_rx - ctx->term.tail_rx;
            float ty = target_ry - ctx->term.tail_ry;

            // Snap to target if we're close enough to stop animating
            if (tx * tx + ty * ty <= 0.5f) {
                ctx->term.is_trailing = 0;
                ctx->term.tail_rx = target_rx;
                ctx->term.tail_ry = target_ry;
                ctx->term.last_trail_update_ms = 0;
            } else {
                float decay = dt_ms * SFTE_CURSOR_TRAIL_DECAY;
                if (decay > 1.0f) decay = 1.0f;

                ctx->term.tail_rx += tx * decay;
                ctx->term.tail_ry += ty * decay;
            }
            app->needs_render = 1;
        }
#endif  // SFTE_CURSOR_TRAIL

        // Dispatch render pass
        if (app->needs_render) {
            sfte_damage_rect dmg = {0};

            uint32_t *target_pxs = app->shm_data;
#if SFTE_DOUBLE_BUFFER
            target_pxs = app->back_buffer;
#endif  // SFTE_DOUBLE_BUFFER

            sfte_render(app->ctx, target_pxs, app->width, app->height, &dmg);

            if (dmg.w > 0 && dmg.h > 0) {
#if SFTE_DOUBLE_BUFFER
                for (int y = dmg.y; y < dmg.y + dmg.h; ++y)
                    memcpy(&app->shm_data[y * app->width + dmg.x],
                           &app->back_buffer[y * app->width + dmg.x], dmg.w * sizeof(uint32_t));
#endif  // SFTE_DOUBLE_BUFFER

                wl_surface_damage_buffer(app->surface, dmg.x, dmg.y, dmg.w, dmg.h);
                wl_surface_attach(app->surface, app->buffer, 0, 0);
                wl_surface_commit(app->surface);
            }

            app->needs_render = 0;
        }
    }
}
#endif  // SFTE_WAYLAND
// #################################################################################################
// >>>PUBLIC IMPLEMENTATION
// #################################################################################################

// =================================================================================================
// >>public api
// =================================================================================================

void sfte_render(sfte_ctx *ctx, uint32_t *px_buf, int w, int h, sfte_damage_rect *out_dmg) {
    int bx0 = w, by0 = h, bx1 = 0, by1 = 0;  // Bounds trackers

    ctx->width = w;
    ctx->height = h;
    uint8_t pad_was_dirty = ctx->padding_dirty;

    if (ctx->padding_dirty) {
        _sfte_view_clear_padding_rects(ctx, px_buf);
        ctx->padding_dirty--;
        _sfte_render_damage_add(&bx0, &by0, &bx1, &by1, 0, 0, w, h);
    }

    int new_cols = (w - (2 * SFTE_PAD_X)) / ctx->font.cell_width;
    int new_rows = (h - (2 * SFTE_PAD_Y)) / ctx->font.cell_height;
    if (new_cols != ctx->term.cols || new_rows != ctx->term.rows)
        _sfte_grid_resize(ctx, new_cols, new_rows);

    int vis_cx = _SFTE_CLAMP(ctx->term.cursor_x, 0, ctx->term.cols - 1);
    int vis_cy = ctx->term.cursor_y;
#if SFTE_SCROLLBACK_CAP
    vis_cy += ctx->term.sb_offset;
#endif  // SFTE_SCROLLBACK_CAP
#if SFTE_WIDE_CHARS
    if (vis_cx > 0 && (_sfte_grid_get_cell(ctx, vis_cx, vis_cy)->attr & ATTR_DUMMY)) vis_cx--;
#endif  // SFTE_WIDE_CHARS

    _sfte_render_propagate_damage(ctx, vis_cx, vis_cy);

#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
    _sfte_render_sort_images(ctx);
    int base_y_off = 0;
#if SFTE_SCROLLBACK_CAP
    base_y_off = ctx->term.sb_offset * ctx->font.cell_height;
#endif  // SFTE_SCROLLBACK_CAP
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS

    // Rendering order:
    // BG images -> BG grid -> FG grid -> FG images
#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
    _sfte_render_images(ctx, px_buf, &bx0, &by0, &bx1, &by1, 1, base_y_off, pad_was_dirty);
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS

    _sfte_render_bg_grid(ctx, px_buf, vis_cx, vis_cy);
    _sfte_render_fg_grid(ctx, px_buf, vis_cx, vis_cy, &bx0, &by0, &bx1, &by1);

#if SFTE_SIXEL || SFTE_KITTY_GRAPHICS
    _sfte_render_images(ctx, px_buf, &bx0, &by0, &bx1, &by1, 0, base_y_off, pad_was_dirty);
#endif  // SFTE_SIXEL || SFTE_KITTY_GRAPHICS

    if (bx0 < bx1 && by0 < by1) {
        out_dmg->x = _SFTE_CLAMP(bx0, 0, w);
        out_dmg->y = _SFTE_CLAMP(by0, 0, h);
        out_dmg->w = _SFTE_CLAMP(bx1, 0, w) - out_dmg->x;
        out_dmg->h = _SFTE_CLAMP(by1, 0, h) - out_dmg->y;
        for (int i = 0; i < ctx->term.rows * ctx->term.cols; ++i) ctx->term.cells[i].dirty = 0;
    } else
        out_dmg->w = 0, out_dmg->h = 0;
}

void sfte_resize(sfte_ctx *ctx, int w, int h) {
    if (w <= 0 || h <= 0) return;

    ctx->width = w;
    ctx->height = h;
    ctx->padding_dirty = 1;

    int new_cols = (w - (2 * SFTE_PAD_X)) / ctx->font.cell_width;
    if (new_cols < 1) new_cols = 1;
    int new_rows = (h - (2 * SFTE_PAD_Y)) / ctx->font.cell_height;
    if (new_rows < 1) new_rows = 1;

    if (new_cols != ctx->term.cols || new_rows != ctx->term.rows) {
        _sfte_grid_resize(ctx, new_cols, new_rows);

#if SFTE_CURSOR_TRAIL
        ctx->term.tail_rx = ctx->term.cursor_x * ctx->font.cell_width;
        ctx->term.tail_ry = ctx->term.cursor_y * ctx->font.cell_height;
        ctx->term.is_trailing = 0;
        ctx->term.trail_damage_w = 0;
#endif  // SFTE_CURSOR_TRAIL
    }
}

void sfte_get_ideal_size(sfte_ctx *ctx, int cols, int rows, int *out_w, int *out_h) {
    if (out_w) *out_w = cols * ctx->font.cell_width + (2 * SFTE_PAD_X);
    if (out_h) *out_h = rows * ctx->font.cell_height + (2 * SFTE_PAD_Y);
}

void sfte_parse(sfte_ctx *ctx, const uint8_t *data, size_t len) {
    if (len == 0 || !data) return;

#if SFTE_SCROLLBACK_CAP
    // snap view to bottom if new output arrives
    if (ctx->term.sb_offset > 0) {
        ctx->term.sb_offset = 0;
        _sfte_grid_dirty_range(ctx, 0, ctx->term.cols * ctx->term.rows);
    }
#endif  // SFTE_SCROLLBACK_CAP

#if SFTE_CURSOR_BLINK
    // reset blink timer when typing/outputting
    ctx->term.blink_visible = 1;
    ctx->term.next_blink_ms = SFTE_TIME_MS() + SFTE_CURSOR_BLINK_RATE;
#endif  // SFTE_CURSOR_BLINK

    // parse incoming stream
    for (size_t i = 0; i < len; ++i) _sfte_parser_feed_byte(ctx, data[i]);

#if SFTE_CURSOR_TRAIL
    int vis_cx = ctx->term.cursor_x >= ctx->term.cols ? ctx->term.cols - 1 : ctx->term.cursor_x;
    float target_rx = vis_cx * ctx->font.cell_width;
    float target_ry = ctx->term.cursor_y * ctx->font.cell_height;

    if (vis_cx != ctx->term.last_grid_x || ctx->term.cursor_y != ctx->term.last_grid_y) {
        uint64_t now = SFTE_TIME_MS();

        if (ctx->term.last_move_ms != 0 && (now - ctx->term.last_move_ms >= SFTE_CURSOR_TRAIL))
            ctx->term.is_trailing = 1;
        else if (!ctx->term.is_trailing) {
            ctx->term.tail_rx = target_rx;
            ctx->term.tail_ry = target_ry;
        }

        ctx->term.last_grid_x = vis_cx;
        ctx->term.last_grid_y = ctx->term.cursor_y;
        ctx->term.last_move_ms = now;
    }
#endif  // SFTE_CURSOR_TRAIL
}

sfte_ctx *sfte_init(sfte_write_cb write_fn, void *user_data) {
    sfte_ctx *ctx = (sfte_ctx *)SFTE_CALLOC(1, sizeof(sfte_ctx));
    SFTE_ASSERT(ctx, "failed to allocate core context");

    ctx->write_cb = write_fn;
    ctx->user_data = user_data;

#ifndef SFTE_NO_LOGGING
    ctx->logger.func = SFTE_LOG_FUNC;
#endif  // !SFTE_NO_LOGGING

#if SFTE_SIXEL
    for (size_t i = 0; i < _SFTE_ARRAY_LEN(_sfte_ansi_palette); ++i)
        ctx->sixel.palette[i] = SFTE_COLOR_ALPHA_MASK | _sfte_ansi_palette[i];
#endif  // SFTE_SIXEL

    ctx->term.cols = 80;
    ctx->term.rows = 24;
    ctx->term.auto_wrap = 1;
    ctx->term.origin_mode = 0;

#if SFTE_SCROLLBACK_CAP
    ctx->term.sb_cap = SFTE_SCROLLBACK_CAP;
    ctx->term.scrollback = (sfte_cell *)SFTE_CALLOC(ctx->term.sb_cap * ctx->term.cols,
                                                    sizeof(sfte_cell));
#endif  // SFTE_SCROLLBACK_CAP

    _sfte_grid_resize_tabs(ctx, 0, ctx->term.cols);

#if SFTE_MOUSE
    ctx->term.mouse_btn_state = 3;
#endif  // SFTE_MOUSE
#if SFTE_KITTY_KB
    ctx->term.kitty_kb_idx[0] = 0;
    ctx->term.kitty_kb_idx[1] = 0;
    ctx->term.kitty_kb_stack[0][0] = 0;
    ctx->term.kitty_kb_stack[1][0] = 0;
#endif  // SFTE_KITTY_KB

#if SFTE_CURSOR_BLINK
    ctx->term.blink_enabled = 1;
    ctx->term.blink_visible = 1;
    ctx->term.next_blink_ms = SFTE_TIME_MS() + SFTE_CURSOR_BLINK_RATE;
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_TRAIL
    ctx->term.tail_rx = 0.0f;
    ctx->term.tail_ry = 0.0f;
    ctx->term.trail_damage_x = 0.0f;
    ctx->term.trail_damage_y = 0.0f;
    ctx->term.trail_damage_w = 0.0f;
    ctx->term.trail_damage_h = 0.0f;
    ctx->term.last_grid_x = 0;
    ctx->term.last_grid_y = 0;
    ctx->term.last_move_ms = 0;
    ctx->term.is_trailing = 0;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_CURSOR_DYNAMIC
    ctx->term.cursor_style = SFTE_CURSOR_STYLE;
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_COLOR_UNDERLINE
    ctx->term.cur_ul_color = 0xFFFFFFFF;
#endif  // SFTE_COLOR_UNDERLINE
#if SFTE_EXT_UNDERLINES
    ctx->term.cur_ul_style = 0;
#endif  // SFTE_EXT_UNDERLINES
    ctx->term.scroll_top = 0;
    ctx->term.scroll_bottom = ctx->term.rows - 1;

    ctx->term.osc_cap = SFTE_OSC_INIT_CAP;
    ctx->term.osc_payload = (char *)SFTE_MALLOC(ctx->term.osc_cap);
    ctx->term.osc_len = 0;

#if SFTE_HYPERLINKS
    ctx->term.link_pool_cap = SFTE_HYPERLINKS_INIT_CAP;
    ctx->term.link_pool = (char **)SFTE_CALLOC(ctx->term.link_pool_cap, sizeof(char *));
    ctx->term.link_pool_len = 1;  // idx 0 is reserved for no link
    ctx->term.cur_link_idx = 0;
#endif  // SFTE_HYPERLINKS

    ctx->term.cells = (sfte_cell *)SFTE_MALLOC(ctx->term.cols * ctx->term.rows * sizeof(sfte_cell));
    SFTE_ASSERT(ctx->term.cells, "failed to allocate term grid");
    memset(ctx->term.cells, 0, ctx->term.cols * ctx->term.rows * sizeof(sfte_cell));

    return ctx;
}

void sfte_free(sfte_ctx *ctx) {
    if (!ctx) return;

    SFTE_FREE(ctx->term.tab_stops);

#define FREE_CACHE(type)                                                                           \
    do {                                                                                           \
        for (int i = 0; i < type.num_fonts; ++i)                                                   \
            if (type.owns_ttf_buf[i]) SFTE_FREE(type.ttf_buf[i]);                                  \
        SFTE_FREE(type.atlas_pxs);                                                                 \
        SFTE_FREE(type.glyphs);                                                                    \
    } while (0)

    FREE_CACHE(ctx->font.regular);
#ifdef SFTE_FONT_BOLD
    FREE_CACHE(ctx->font.bold);
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    FREE_CACHE(ctx->font.italic);
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    FREE_CACHE(ctx->font.bold_italic);
#endif  // SFTE_FONT_BOLD_ITALIC

#undef FREE_CACHE

    SFTE_FREE(ctx->term.osc_payload);
    SFTE_FREE(ctx->term.cells);
#if SFTE_ALT_SCREEN
    SFTE_FREE(ctx->term.alt_cells);
#endif  // SFTE_ALT_SCREEN
#if SFTE_SCROLLBACK_CAP
    SFTE_FREE(ctx->term.scrollback);
#endif  // SFTE_SCROLLBACK_CAP
#if SFTE_HYPERLINKS
    if (ctx->term.link_pool) {
        for (uint16_t i = 0; i < ctx->term.link_pool_len; ++i) SFTE_FREE(ctx->term.link_pool[i]);
        SFTE_FREE(ctx->term.link_pool);
    }
#endif  // SFTE_HYPERLINKS

#if SFTE_SIXEL
    _sfte_sixel_deinit(ctx);
#endif  // SFTE_SIXEL
#if SFTE_KITTY_GRAPHICS
    _sfte_kitty_deinit(ctx);
#endif  // SFTE_KITTY_GRAPHICS

    SFTE_FREE(ctx);
}

void sfte_font_load_mem(sfte_ctx *ctx, int style, const uint8_t *ttf_data) {
    sfte_font_cache *cache = _sfte_font_get_cache(ctx, style);
    if (!cache || !ttf_data || cache->num_fonts >= SFTE_FONT_MAX_COUNT) return;

    int idx = cache->num_fonts++;
    cache->ttf_buf[idx] = (uint8_t *)ttf_data;
    cache->owns_ttf_buf[idx] = 0;

    if (idx == 0) {
        if (style == SFTE_FONT_STYLE_REGULAR && idx == 0)
            ctx->font.cur_size = SFTE_FONT_DEFAULT_SIZE;

        if (!cache->atlas_pxs) {
            cache->atlas_pxs = (uint8_t *)SFTE_MALLOC(SFTE_FONT_ATLAS_SIZE * SFTE_FONT_ATLAS_SIZE);
            SFTE_ASSERT(cache->atlas_pxs, "failed to allocate font atlas");
        }

        if (!cache->glyphs) {
            cache->glyphs = (sfte_glyph *)SFTE_CALLOC(SFTE_FONT_GLYPH_CAP, sizeof(sfte_glyph));
            SFTE_ASSERT(cache->glyphs, "failed to allocate glyphs storage");
        }
    }

    SFTE_FONT_INIT(&cache->info[idx], cache->ttf_buf[idx]);

    if (style == SFTE_FONT_STYLE_REGULAR && idx == 0)
        _sfte_font_reset_cache(ctx);
    else {
        float tweak = _sfte_font_scales[idx];
        if (tweak <= 0.0f) tweak = 1.0f;
        cache->scales[idx] = SFTE_FONT_GET_SCALE(&cache->info[idx], ctx->font.cur_size * tweak);
    }

    _SFTE_INFO(ctx, FONT_LOADED);
}

void sfte_font_load_file(sfte_ctx *ctx, int style, const char *path) {
    sfte_font_cache *cache = _sfte_font_get_cache(ctx, style);
    if (!cache) return;

    FILE *f = fopen(path, "rb");
    SFTE_ASSERT(f, "failed to open font file");

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = (uint8_t *)SFTE_MALLOC(size);
    SFTE_ASSERT(fread(buf, 1, size, f) == size, "failed to read font file");
    fclose(f);

    sfte_font_load_mem(ctx, style, buf);
    cache->owns_ttf_buf[cache->num_fonts - 1] = 1;
}

#ifndef SFTE_NO_POSIX
pid_t sfte_posix_pty_spawn(sfte_ctx *ctx, int *out_fd, int px_w, int px_h) {
    struct winsize ws = {
        .ws_row = (unsigned short)ctx->term.rows,
        .ws_col = (unsigned short)ctx->term.cols,
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

    _SFTE_INFO(ctx, PTY_SPAWN);
    return pid;
}

void sfte_posix_pty_resize(sfte_ctx *ctx, int pty_fd, int px_w, int px_h) {
    if (pty_fd <= 0) return;
    struct winsize ws = {
        .ws_row = (unsigned short)ctx->term.rows,
        .ws_col = (unsigned short)ctx->term.cols,
        .ws_xpixel = (unsigned short)px_w,
        .ws_ypixel = (unsigned short)px_h,
    };
    ioctl(pty_fd, TIOCSWINSZ, &ws);
}
#endif  // !SFTE_NO_POSIX

#if SFTE_FONT_ZOOM
void sfte_zoom(sfte_ctx *ctx, float delta) {
    ctx->padding_dirty = 1;
    float new_size = ctx->font.cur_size + delta;
    if (new_size < 4.0f || new_size > 96.0f) return;
    ctx->font.cur_size = new_size;
    _sfte_font_reset_cache(ctx);
    sfte_resize(ctx, ctx->width, ctx->height);
}
#endif  // SFTE_FONT_ZOOM

#if SFTE_MOUSE
void sfte_mouse_move(sfte_ctx *ctx, int px_x, int px_y) {
    int c, r;
    _sfte_grid_from_px(ctx, px_x, px_y, &c, &r, NULL);

    if (ctx->term.mouse_hover_x == c && ctx->term.mouse_hover_y) return;

    ctx->term.mouse_hover_x = c;
    ctx->term.mouse_hover_y = r;

    if (ctx->term.mouse_mode) {
        _sfte_input_send_mouse_event(ctx, ctx->term.mouse_btn_state, 0, c, r, 1);
        return;
    }

#if SFTE_SELECTION
    if (!ctx->term.mouse_sel_dragging) return;
    sfte_term *term = &ctx->term;
    if (term->mouse_sel_end_c == term->mouse_hover_x &&
        term->mouse_sel_end_r == term->mouse_hover_y)
        return;

    _sfte_grid_dirty_rows(ctx, term->mouse_sel_start_r, term->mouse_sel_end_r);
    term->mouse_sel_end_c = term->mouse_hover_x;
    term->mouse_sel_end_r = term->mouse_hover_y;
    _sfte_grid_dirty_rows(ctx, term->mouse_sel_start_r, term->mouse_sel_end_r);
#endif  // SFTE_SELECTION
}

void sfte_mouse_click(sfte_ctx *ctx, int btn, int pressed, int px_x, int px_y) {
    int c, r;
    _sfte_grid_from_px(ctx, px_x, px_y, &c, &r, NULL);
    sfte_term *term = &ctx->term;

#if SFTE_HYPERLINKS
    if (pressed && btn == 0 && ctx->open_link_cb) {
        const char *uri = sfte_get_link_at(ctx, c, r);
        if (uri) {
            ctx->open_link_cb(ctx->user_data, uri);
            return;
        }
    }
#endif  // SFTE_HYPERLINKS

    if (term->mouse_mode) {
        if (pressed)
            term->mouse_btn_state = btn;
        else
            term->mouse_btn_state = 3;

        _sfte_input_send_mouse_event(ctx, btn, !pressed, c, r, 0);
        return;
    }

#if SFTE_SELECTION
    if (btn != 0) return;
    if (pressed) {
        if (term->mouse_sel_active)
            _sfte_grid_dirty_rows(ctx, term->mouse_sel_start_c, term->mouse_sel_end_r);
        term->mouse_sel_start_c = term->mouse_hover_x = c;
        term->mouse_sel_start_r = term->mouse_hover_y = r;
        term->mouse_sel_end_c = c;
        term->mouse_sel_end_r = r;
        term->mouse_sel_active = 1;
        term->mouse_sel_dragging = 1;
        _sfte_grid_dirty_rows(ctx, term->mouse_sel_start_r, term->mouse_sel_end_r);
    } else {
        term->mouse_sel_dragging = 0;
        if (term->mouse_sel_start_c == term->mouse_sel_end_c &&
            term->mouse_sel_start_r == term->mouse_sel_end_r) {
            term->mouse_sel_active = 0;
            _sfte_grid_dirty_rows(ctx, term->mouse_sel_start_r, term->mouse_sel_end_r);
        }
    }
#endif  // SFTE_SELECTION
}

void sfte_mouse_scroll(sfte_ctx *ctx, int dir, int px_x, int px_y) {
    int c, r;
    _sfte_grid_from_px(ctx, px_x, px_y, &c, &r, NULL);

    if (ctx->term.mouse_mode) {
        int btn = (dir > 0) ? 64 : 65;
        _sfte_input_send_mouse_event(ctx, btn, 0, c, r, 0);
    }

#if SFTE_SCROLLBACK_CAP
    sfte_view_scroll(ctx, (dir > 0) ? SFTE_SCROLL_STEP : -SFTE_SCROLL_STEP);
#endif  // SFTE_SCROLLBACK_CAP
}
#endif  // SFTE_MOUSE

#if SFTE_KITTY_KB
int sfte_kitty_kb_encode(sfte_ctx *ctx, sfte_key key, uint32_t codepoint, uint32_t mod_mask,
                         char *out_buf, size_t max_bytes) {
    int s_idx = ctx->term.alt_active ? 1 : 0;
    int flags = ctx->term.kitty_kb_stack[s_idx][ctx->term.kitty_kb_idx[s_idx]];
    if (flags == 0) return 0;

    uint32_t csi_mod = 1;
    if (mod_mask & SFTE_MOD_SHIFT) csi_mod += 1;
    if (mod_mask & SFTE_MOD_ALT) csi_mod += 2;
    if (mod_mask & SFTE_MOD_CTRL) csi_mod += 4;
    if (mod_mask & SFTE_MOD_SUPER) csi_mod += 8;

    // arrows, home, end, f1-f4 -> CSI 1 ; mods [char]
    char func_char = 0;
    switch (key) {
    case SFTE_KEY_UP: func_char = 'A'; break;
    case SFTE_KEY_DOWN: func_char = 'B'; break;
    case SFTE_KEY_RIGHT: func_char = 'C'; break;
    case SFTE_KEY_LEFT: func_char = 'D'; break;
    case SFTE_KEY_HOME: func_char = 'H'; break;
    case SFTE_KEY_END: func_char = 'F'; break;
    case SFTE_KEY_F1: func_char = 'P'; break;
    case SFTE_KEY_F2: func_char = 'Q'; break;
    case SFTE_KEY_F3: func_char = 'R'; break;
    case SFTE_KEY_F4: func_char = 'S'; break;
    default: break;
    }

    if (func_char) {
        if (csi_mod > 1) return snprintf(out_buf, max_bytes, "\033[1;%u%c", csi_mod, func_char);
        return snprintf(out_buf, max_bytes, "\033[%c", func_char);
    }

    // insert, delete, pgup, pgdn, f5-f12 -> CSI num ; mods ~
    int tilde_num = 0;
    switch (key) {
    case SFTE_KEY_INSERT: tilde_num = 2; break;
    case SFTE_KEY_DELETE: tilde_num = 3; break;
    case SFTE_KEY_PAGE_UP: tilde_num = 5; break;
    case SFTE_KEY_PAGE_DOWN: tilde_num = 6; break;
    case SFTE_KEY_F5: tilde_num = 15; break;
    case SFTE_KEY_F6: tilde_num = 17; break;
    case SFTE_KEY_F7: tilde_num = 18; break;
    case SFTE_KEY_F8: tilde_num = 19; break;
    case SFTE_KEY_F9: tilde_num = 20; break;
    case SFTE_KEY_F10: tilde_num = 21; break;
    case SFTE_KEY_F11: tilde_num = 22; break;
    case SFTE_KEY_F12: tilde_num = 23; break;
    default: break;
    }

    if (tilde_num) {
        if (csi_mod > 1) return snprintf(out_buf, max_bytes, "\033[%d;%u~", tilde_num, csi_mod);
        return snprintf(out_buf, max_bytes, "\033[%d~", tilde_num);
    }

    // text keys and control keys -> CSI codepoint ; mods u
    uint32_t target_cp = codepoint ? codepoint : (uint32_t)key;
    // we can safely use SFTE_KEY_* as ASCII values
    if (target_cp > 0 && (csi_mod > 1 || target_cp == SFTE_KEY_TAB || target_cp == SFTE_KEY_ENTER ||
                          target_cp == SFTE_KEY_ESCAPE || target_cp == SFTE_KEY_BACKSPACE))
        return snprintf(out_buf, max_bytes, "\033[%u;%uu", target_cp, csi_mod);

    return 0;
}
#endif  // SFTE_KITTY_KB

#if SFTE_HYPERLINKS
const char *sfte_get_link_at(sfte_ctx *ctx, int col, int row) {
    if (col < 0 || col >= ctx->term.cols || row < 0 || row >= ctx->term.rows) return NULL;

    sfte_cell *c = &ctx->term.cells[row * ctx->term.cols + col];
    if (!c || c->link_idx == 0 || c->link_idx >= ctx->term.link_pool_len) return NULL;

    return ctx->term.link_pool[c->link_idx];
}
#endif  // SFTE_HYPERLINKS

#if SFTE_SCROLLBACK_CAP
void sfte_view_scroll(sfte_ctx *ctx, int delta) {
#if SFTE_ALT_SCREEN
    if (ctx->term.alt_active) return;
#endif  // SFTE_ALT_SCREEN
    int new_off = ctx->term.sb_offset + delta;
    if (new_off < 0) new_off = 0;
    int max_scroll = ctx->term.sb_len < ctx->term.sb_cap ? ctx->term.sb_len : ctx->term.sb_cap;
    if (new_off > max_scroll) new_off = max_scroll;

    if (new_off != ctx->term.sb_offset) {
        ctx->term.sb_offset = new_off;
        _sfte_grid_dirty_range(ctx, 0, ctx->term.cols * ctx->term.rows);
    }
}
#endif  // SFTE_SCROLLBACK_CAP

#if SFTE_SELECTION
size_t sfte_get_selection(sfte_ctx *ctx, char *out_buf, size_t max_bytes) {
    if (!ctx->term.mouse_sel_active) return 0;

    int sx = ctx->term.mouse_sel_start_c, sy = ctx->term.mouse_sel_start_r;
    int ex = ctx->term.mouse_sel_end_c, ey = ctx->term.mouse_sel_end_r;

    // Normalize if dragging backwards
    if (sy > ey || (sy == ey && sx > ex)) {
        int tmp = sy;
        sy = ey;
        ey = tmp;
        tmp = sx;
        sx = ex;
        ex = tmp;
    }

    size_t pos = 0;

#define _SFTE_WRITE_CHAR(c)                                                                        \
    do {                                                                                           \
        if (out_buf && pos < max_bytes - 1) out_buf[pos] = (c);                                    \
        pos++;                                                                                     \
    } while (0)

    for (int r = sy; r <= ey; ++r) {
        int row_start = (r == sy) ? sx : 0;
        int row_end = (r == ey) ? ex : ctx->term.cols - 1;

        int phys_r = r;
#if SFTE_SCROLLBACK_CAP
        phys_r += ctx->term.sb_offset;
#endif

        // Trim trailing spaces if the user selected past the end of text
        int actual_end = row_end;
        if (actual_end == ctx->term.cols - 1) {
            while (actual_end >= row_start) {
                sfte_cell *vcell = _sfte_grid_get_cell(ctx, actual_end, phys_r);
                if (vcell->rune != ' ' && vcell->rune != 0) break;
                actual_end--;
            }
        }

        for (int c = row_start; c <= actual_end; ++c) {
            sfte_cell *vcell = _sfte_grid_get_cell(ctx, c, phys_r);
            uint32_t rune = (vcell->rune && vcell->rune != ' ') ? vcell->rune : ' ';

            // UTF-8 encoding
            if (rune < 0x80) {
                _SFTE_WRITE_CHAR(rune);
            } else if (rune < 0x800) {
                _SFTE_WRITE_CHAR(0xC0 | (rune >> 6));
                _SFTE_WRITE_CHAR(0x80 | (rune & 0x3F));
            } else if (rune < 0x10000) {
                _SFTE_WRITE_CHAR(0xE0 | (rune >> 12));
                _SFTE_WRITE_CHAR(0x80 | ((rune >> 6) & 0x3F));
                _SFTE_WRITE_CHAR(0x80 | (rune & 0x3F));
            } else {
                _SFTE_WRITE_CHAR(0xF0 | (rune >> 18));
                _SFTE_WRITE_CHAR(0x80 | ((rune >> 12) & 0x3F));
                _SFTE_WRITE_CHAR(0x80 | ((rune >> 6) & 0x3F));
                _SFTE_WRITE_CHAR(0x80 | (rune & 0x3F));
            }
        }

        // Inject newlines for multi-line selections, unless the line soft-wrapped
        if (r < ey) {
#if SFTE_REFLOW
            if (!_sfte_grid_get_cell(ctx, ctx->term.cols - 1, phys_r)->wrapped)
#endif
                _SFTE_WRITE_CHAR('\n');
        }
    }
#undef _SFTE_WRITE_CHAR

    if (out_buf && max_bytes > 0) out_buf[pos < max_bytes ? pos : max_bytes - 1] = '\0';
    return pos + 1;
}
#endif  // SFTE_SELECTION

// =================================================================================================
// >>wayland api
// =================================================================================================
#if SFTE_WAYLAND
sfte_wayland_app *sfte_wayland_init(void) {
    sfte_wayland_app *app = (sfte_wayland_app *)SFTE_CALLOC(1, sizeof(sfte_wayland_app));
    app->running = 1;
    app->ctx = sfte_init(_sfte_wayland_write_cb, app);

#if SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD
    app->ctx->osc52_clipboard_cb = _sfte_wayland_osc52_clipboard_cb;
#endif  // SFTE_CLIPBOARD && SFTE_OSC52_CLIPBOARD
#if SFTE_HYPERLINKS
    app->ctx->open_link_cb = _sfte_wayland_open_link_cb;
#endif  // SFTE_HYPERLINKS

    _sfte_wayland_pty_spawn(app);
    _sfte_wayland_load(app);

    return app;
}

sfte_ctx *sfte_wayland_get_ctx(sfte_wayland_app *app) {
    return app->ctx;
}

int sfte_wayland_run(sfte_wayland_app *app) {
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

    int ideal_w, ideal_h;
    sfte_get_ideal_size(app->ctx, 80, 24, &ideal_w, &ideal_h);
    app->width = ideal_w;
    app->height = ideal_h;
    sfte_resize(app->ctx, app->width, app->height);
    if (!app->buffer) _sfte_wayland_create_buffer(app);
    _sfte_wayland_pty_update(app);

    _sfte_wayland_loop(app);

    _sfte_wayland_unload(app);
    sfte_free(app->ctx);
    SFTE_FREE(app);
    return 0;
}
#endif  // SFTE_WAYLAND
#endif  // SFTE_IMPL
