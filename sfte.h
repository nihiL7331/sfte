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

// >>config
#ifndef SFTE_LOG_LEVEL
#define SFTE_LOG_LEVEL 0
#endif  // SFTE_LOG_LEVEL

#ifndef SFTE_LOGGER_FUNC
#define SFTE_LOGGER_FUNC _sfte_logger_default
#endif  // SFTE_LOGGER_FUNC

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

#ifndef SFTE_FONT_DEFAULT_SIZE
#define SFTE_FONT_DEFAULT_SIZE 12.0f
#endif  // SFTE_FONT_DEFAULT_SIZE

#ifndef SFTE_FONT_ZOOM
#define SFTE_FONT_ZOOM 1
#endif  // SFTE_FONT_ZOOM

#ifndef SFTE_ANSI_PALETTE
#define SFTE_ANSI_PALETTE                                                                          \
    {0x181818, 0xCC241D, 0x98971A, 0xD79921, 0x458588, 0xB16286, 0x689D6A, 0xA89984,               \
     0x928374, 0xFB4934, 0xB8BB26, 0xFABD2F, 0x83A598, 0xD3869B, 0x8EC07C, 0xEBDBB2}
#endif  // SFTE_ANSI_PALETTE

#ifndef SFTE_TRUE_COLOR
#define SFTE_TRUE_COLOR 1
#endif  // SFTE_TRUE_COLOR

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

#ifndef SFTE_SCROLLBACK_CAP
#define SFTE_SCROLLBACK_CAP 2000
#endif  // SFTE_SCROLLBACK_CAP

#ifndef SFTE_ALT_SCREEN
#define SFTE_ALT_SCREEN 1
#endif  // SFTE_ALT_SCREEN

#ifndef SFTE_DOUBLE_BUFFER
#define SFTE_DOUBLE_BUFFER 1
#endif  // SFTE_DOUBLE_BUFFER

#ifndef SFTE_REFLOW
#define SFTE_REFLOW 1
#endif  // SFTE_REFLOW

#ifndef SFTE_SELECTION
#define SFTE_SELECTION 1
#endif  // SFTE_SELECTION

// NOTE: SFTE_CLIPBOARD implicity enables SFTE_SELECTION
#ifndef SFTE_CLIPBOARD
#define SFTE_CLIPBOARD 1
#endif  // SFTE_CLIPBOARD

#ifndef SFTE_CLIPBOARD_BUF_SIZE
#define SFTE_CLIPBOARD_BUF_SIZE 4096
#endif  // SFTE_CLIPBOARD_BUF_SIZE

#if SFTE_CLIPBOARD
#define SFTE_SELECTION 1
#endif  // SFTE_CLIPBOARD

#define SFTE_MOD_CTRL 0b001
#define SFTE_MOD_ALT 0b010
#define SFTE_MOD_SHIFT 0b100
#define SFTE_MOD_NONE 0b000

typedef union {
    int i;
    float f;
    const void *v;
} sfte_arg;

typedef struct {
    uint32_t mod_mask;
    uint32_t /* xkb_keysym_t */ keysym;
    void (*func)(const sfte_arg *);
    const sfte_arg arg;
} sfte_shortcut;

static void _sfte_view_scroll(const sfte_arg *arg);
static void _sfte_clipboard_copy(const sfte_arg *arg);
static void _sfte_clipboard_paste(const sfte_arg *arg);

#if SFTE_FONT_ZOOM
#define _SFTE_ZOOM_BINDS                                                                           \
    {SFTE_MOD_CTRL, XKB_KEY_equal, _sfte_font_resize, {.f = 2.0f}},                                \
        {SFTE_MOD_CTRL, XKB_KEY_plus, _sfte_font_resize, {.f = 2.0f}},                             \
        {SFTE_MOD_CTRL, XKB_KEY_minus, _sfte_font_resize, {.f = -2.0f}},                           \
        {SFTE_MOD_CTRL, XKB_KEY_0, _sfte_font_reset, {.v = NULL}},
#else
#define _SFTE_ZOOM_BINDS
#endif  // !SFTE_FONT_ZOOM

#if SFTE_SCROLLBACK_CAP
#define _SFTE_SCROLL_BINDS                                                                         \
    {SFTE_MOD_SHIFT, XKB_KEY_Page_Up, _sfte_view_scroll, {.i = 10}},                               \
        {SFTE_MOD_SHIFT, XKB_KEY_Page_Down, _sfte_view_scroll, {.i = -10}},
#else
#define _SFTE_SCROLL_BINDS
#endif  // !SFTE_SCROLLBACK_CAP

#if SFTE_CLIPBOARD
#define _SFTE_CLIPBOARD_BINDS                                                                      \
    {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, XKB_KEY_C, _sfte_clipboard_copy, {.v = NULL}},                \
        {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, XKB_KEY_V, _sfte_clipboard_paste, {.v = NULL}},
#else
#define _SFTE_CLIPBOARD_BINDS
#endif

#ifndef SFTE_SHORTCUTS
#define SFTE_SHORTCUTS {_SFTE_ZOOM_BINDS _SFTE_SCROLL_BINDS _SFTE_CLIPBOARD_BINDS}
#endif  // SFTE_SHORTCUTS

// >>api
int sfte_run(void);

#define SFTE_IMPL
#ifdef SFTE_IMPL
/*=== IMPLEMENTATION =========================================================*/

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#include "xdg-shell.c"
#include "xdg-shell.h"

#include <fcntl.h>
#include <poll.h>
#include <pty.h>  // forkpty
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>  // getenv/setenv/malloc
#include <string.h>  // memset
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>  // exec/fork/env
#include <wayland-client.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>
#if SFTE_CURSOR_BLINK
#include <time.h>
#endif  // SFTE_CURSOR_BLINK

// >>structs
#ifndef SFTE_NO_LOGGING
typedef struct sfte_logger {
    void (*func)(const char *tag,              // always "sfte"
                 uint32_t log_level,           // 0=panic, 1=error, 2=warning, 3=info
                 const char *message_or_null,  // a message string, may be nullptr in release mode
                 uint32_t line_nr              // line number in sfte.h
    );
} sfte_logger;
#endif  // !SFTE_NO_LOGGING

typedef enum {
    ATTR_NONE = 0b0000,
    ATTR_BOLD = 0b0001,
    ATTR_ITALIC = 0b0010,
    ATTR_UNDERLINE = 0b0100,
    ATTR_REVERSE = 0b1000
} sfte_attr;

typedef struct {
    uint32_t rune;
    uint32_t fg;
    uint32_t bg;
    uint16_t attr;  // bitmask of sfte_attr
    uint8_t dirty;  // 1 if this cell changed
#if SFTE_REFLOW
    uint8_t wrapped;  // 1 if this cell caused a soft line-wrap
#endif                // SFTE_REFLOW
} sfte_cell;

typedef struct {
    uint32_t rune;
#if defined(SFTE_FONT_BOLD_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
    uint8_t is_bold;
#endif  // SFTE_FONT_BOLD_PATH || SFTE_FONT_BOLD_ITALIC_PATH
#if defined(SFTE_FONT_ITALIC_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
    uint8_t is_italic;
#endif  // SFTE_FONT_ITALIC_PATH || SFTE_FONT_BOLD_ITALIC_PATH

    int x0, y0, x1, y1;  // atlas tex coords
    int xoff, yoff;      // render offsets
    int xadvance;
} sfte_glyph;

typedef struct {
    sfte_cell *cells;
    int cols;
    int rows;
    char title[256];
    char saved_title[256];
    uint8_t auto_wrap;
    uint8_t origin_mode;
    uint8_t *tab_stops;
    // ansi save state
    int ansi_saved_x;
    int ansi_saved_y;
    uint32_t ansi_saved_fg;
    uint32_t ansi_saved_bg;
    uint32_t ansi_saved_attr;
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
    int alt_active;  // tracks if in alt buffer
    sfte_cell *alt_cells;
#endif  // SFTE_ALT_SCREEN
// mouse state
#if SFTE_SELECTION
    int sel_active;    // 1 if has selection
    int sel_dragging;  // 1 if lmb is held down
    int hover_x, hover_y;
    int sel_start_x, sel_start_y;  // abs grid coords
    int sel_end_x, sel_end_y;
#endif  // SFTE_SELECTION

    int scroll_top;
    int scroll_bottom;
    // osc
    char osc_payload[128];
    int osc_idx;
    // pen state
    uint32_t cur_fg;
    uint32_t cur_bg;
    uint16_t cur_attr;
    // parser
    int vt_state;
    int vt_params[16];  // stores nums from esc sequences
    int vt_param_idx;
    uint8_t vt_dec_priv;  // tracks if seq starts with ?
    // utf-8 state
    uint32_t utf8_rune;
    int utf8_bytes_left;
} sfte_term;

typedef struct {
    uint8_t *ttf_buf;
#ifdef SFTE_FONT_BOLD_PATH
    uint8_t *ttf_bold_buf;
#endif  // SFTE_FONT_BOLD_PATH
#ifdef SFTE_FONT_ITALIC_PATH
    uint8_t *ttf_italic_buf;
#endif  // SFTE_FONT_ITALIC_PATH
#ifdef SFTE_FONT_BOLD_ITALIC_PATH
    uint8_t *ttf_bold_italic_buf;
#endif  // SFTE_FONT_BOLD_ITALIC_PATH

    float cur_size;  // starts at SFTE_FONT_DEFAULT_SIZE

    uint8_t *atlas_pxs;
    int atlas_width;
    int atlas_height;

    stbtt_fontinfo stb_info;
#ifdef SFTE_FONT_BOLD_PATH
    stbtt_fontinfo stb_bold_info;
#endif  // SFTE_FONT_BOLD_PATH
#ifdef SFTE_FONT_ITALIC_PATH
    stbtt_fontinfo stb_italic_info;
#endif  // SFTE_FONT_ITALIC_PATH
#ifdef SFTE_FONT_BOLD_ITALIC_PATH
    stbtt_fontinfo stb_bold_italic_info;
#endif  // SFTE_FONT_BOLD_ITALIC_PATH
    float scale;

    // hash map
    sfte_glyph *glyphs;
    int glyph_cap;

    // allocator
    int atlas_x;
    int atlas_y;
    int atlas_bottom;

    int cell_width;   // width of a single mono char
    int cell_height;  // height of a single mono char
} sfte_font;

typedef struct {
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
    uint32_t serial;
#endif  // SFTE_SELECTION
#if SFTE_CLIPBOARD
    struct wl_data_device_manager *data_device_manager;
    struct wl_data_device *data_device;
    struct wl_data_source *data_source;
    struct wl_data_offer *data_offer;
    char *selection_text;
#endif  // SFTE_CLIPBOARD
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_buffer *buffer;
    uint32_t *shm_data;
#if SFTE_DOUBLE_BUFFER
    uint32_t *back_buffer;
#endif  // SFTE_DOUBLE_BUFFER
    int shm_size;

    int pty_fd;     // master fd to r/w from
    pid_t pty_pid;  // pid of shell

    int width;
    int height;
    uint8_t running;

    int repeat_timer_fd;
    int32_t repeat_rate;
    int32_t repeat_delay;
    uint32_t repeating_key;

#ifndef SFTE_NO_LOGGING
    sfte_logger logger;
#endif  // !SFTE_NO_LOGGING
    sfte_term term;
    sfte_font font;
} _sfte_state;
static _sfte_state _sfte;

// >>memory
#define _SFTE_ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#define _SFTE_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))
#define _SFTE_IDX(c, r) ((r) * _sfte.term.cols + (c))

static inline void _sfte_dirty_range(int start_idx, int cnt) {
    for (int i = 0; i < cnt; ++i) _sfte.term.cells[start_idx + i].dirty = 1;
}

// >>logging
#ifndef SFTE_ASSERT
#include <assert.h>
#define SFTE_ASSERT(c, m) assert(c &&m)
#endif  // SFTE_ASSERT

#ifndef SFTE_NO_LOGGING

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

#define _SFTE_LOG_ITEMS                                                                            \
    _SFTE_LOGITEM_XMACRO(OK, "ok")                                                                 \
    _SFTE_LOGITEM_XMACRO(FONT_LOADED, "font loaded and baked to atlas")                            \
    _SFTE_LOGITEM_XMACRO(PTY_SPAWN, "master/slave pair successfully spawned")                      \
    _SFTE_LOGITEM_XMACRO(UNHANDLED_CSI, "unhandled CSI command: '%c'")                             \
    _SFTE_LOGITEM_XMACRO(UNHANDLED_OSC, "unhandled OSC payload: '%s'")                             \
    _SFTE_LOGITEM_XMACRO(TERM_RESIZE, "resized grid to '%dx%d'")                                   \
    _SFTE_LOGITEM_XMACRO(WAYLAND_REGISTRY_BOUND, "wayland globals bound")                          \
    _SFTE_LOGITEM_XMACRO(KEYMAP_LOADED, "xkb keymap loaded from compositor")

#define _SFTE_LOGITEM_XMACRO(item, msg) item,
typedef enum { _SFTE_LOG_ITEMS } _sfte_log_item_t;
#undef _SFTE_LOGITEM_XMACRO

#define _SFTE_LOGITEM_XMACRO(item, msg) #item ": " msg,
static const char *_sfte_log_messages[] = {_SFTE_LOG_ITEMS};
#undef _SFTE_LOGITEM_XMACRO

static void _sfte_log(_sfte_log_item_t log_item, uint32_t log_level, uint32_t line_nr, ...) {
    if (log_level > SFTE_LOG_LEVEL) return;

    char buf[512];
    va_list args;
    va_start(args, line_nr);
    vsnprintf(buf, sizeof(buf), _sfte_log_messages[log_item], args);
    va_end(args);

    void (*log_func)(const char *, uint32_t, const char *,
                     uint32_t) = _sfte.logger.func ? _sfte.logger.func : _sfte_logger_default;

    log_func("sfte", log_level, buf, line_nr);

    // for log level PANIC it would be 'undefined behaviour' to continue
    if (log_level == 0) abort();
}

#define _SFTE_PANIC(code, ...) _sfte_log(code, 0, __LINE__, ##__VA_ARGS__)
#define _SFTE_ERROR(code, ...) _sfte_log(code, 1, __LINE__, ##__VA_ARGS__)
#define _SFTE_WARN(code, ...) _sfte_log(code, 2, __LINE__, ##__VA_ARGS__)
#define _SFTE_INFO(code, ...) _sfte_log(code, 3, __LINE__, ##__VA_ARGS__)

#else

#define _SFTE_PANIC(code, ...) abort()
#define _SFTE_ERROR(code, ...)                                                                     \
    do {                                                                                           \
    } while (0)
#define _SFTE_WARN(code, ...)                                                                      \
    do {                                                                                           \
    } while (0)
#define _SFTE_INFO(code, ...)                                                                      \
    do {                                                                                           \
    } while (0)

#endif  // !SFTE_NO_LOGGING

// >>font
static sfte_glyph *_sfte_font_get_glyph(uint32_t rune
#if defined(SFTE_FONT_BOLD_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
                                        ,
                                        uint8_t is_bold
#endif  // SFTE_FONT_BOLD_PATH || SFTE_FONT_BOLD_ITALIC_PATH
#if defined(SFTE_FONT_ITALIC_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
                                        ,
                                        uint8_t is_italic
#endif  // SFTE_FONT_ITALIC_PATH || SFTE_FONT_BOLD_ITALIC_PATH
) {
    if (rune == 0) rune = ' ';

    uint32_t h = rune;
// offset hash if bold is requested to avoid collisions
#ifdef SFTE_FONT_BOLD_PATH
    if (is_bold) h += 0x9E3779B9;
#endif  // SFTE_FONT_BOLD_PATH
#ifdef SFTE_FONT_ITALIC_PATH
    if (is_italic) h += 0x9E3779B9;
#endif  // SFTE_FONT_ITALIC_PATH
    h %= _sfte.font.glyph_cap;

    // hash map logic
    for (int i = 0; i < _sfte.font.glyph_cap; ++i) {
        int idx = (h + i) % _sfte.font.glyph_cap;

        if (_sfte.font.glyphs[idx].rune == rune
#if defined(SFTE_FONT_BOLD_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
            && _sfte.font.glyphs[idx].is_bold == is_bold
#endif  // SFTE_FONT_BOLD_PATH || SFTE_FONT_BOLD_ITALIC_PATH
#if defined(SFTE_FONT_ITALIC_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
            && _sfte.font.glyphs[idx].is_italic == is_italic
#endif  // SFTE_FONT_ITALIC_PATH || SFTE_FONT_BOLD_ITALIC_PATH
        )
            return &_sfte.font.glyphs[idx];

        if (_sfte.font.glyphs[idx].rune != 0) continue;  // cache miss, taken, continue

        stbtt_fontinfo *info = &_sfte.font.stb_info;

        // cache miss, free, take space
        sfte_glyph *g = &_sfte.font.glyphs[idx];
        g->rune = rune;
#ifdef SFTE_FONT_ITALIC_PATH
        g->is_italic = is_italic;
        if (is_italic) info = &_sfte.font.stb_italic_info;
#endif  // SFTE_FONT_ITALIC_PATH
#ifdef SFTE_FONT_BOLD_PATH
        g->is_bold = is_bold;
        if (is_bold) info = &_sfte.font.stb_bold_info;
#endif  // SFTE_FONT_BOLD_PATH
#ifdef SFTE_FONT_BOLD_ITALIC_PATH
        g->is_italic = is_italic;
        g->is_bold = is_bold;
        if (is_bold && is_italic) info = &_sfte.font.stb_bold_italic_info;
#endif  // SFTE_FONT_BOLD_ITALIC_PATH

        int advance_width, left_side_bearing;
        stbtt_GetCodepointHMetrics(info, rune, &advance_width, &left_side_bearing);
        g->xadvance = (int)(advance_width * _sfte.font.scale + 0.5f);

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(info, rune, _sfte.font.scale, _sfte.font.scale, &x0, &y0, &x1,
                                    &y1);

        int glyph_width = x1 - x0;
        int glyph_height = y1 - y0;

        // wrap to next row if out of horizontal space
        if (_sfte.font.atlas_x + glyph_width >= _sfte.font.atlas_width) {
            _sfte.font.atlas_x = 0;
            _sfte.font.atlas_y = _sfte.font.atlas_bottom;
        }

        SFTE_ASSERT(_sfte.font.atlas_y + glyph_height < _sfte.font.atlas_height,
                    "glyph atlas full");

        g->x0 = _sfte.font.atlas_x;
        g->y0 = _sfte.font.atlas_y;
        g->x1 = g->x0 + glyph_width;
        g->y1 = g->y0 + glyph_height;
        g->xoff = x0;
        g->yoff = y0;

        if (glyph_width > 0 && glyph_height > 0) {
            int byte_off = g->y0 * _sfte.font.atlas_width + g->x0;
            stbtt_MakeCodepointBitmap(info, &_sfte.font.atlas_pxs[byte_off], glyph_width,
                                      glyph_height, _sfte.font.atlas_width, _sfte.font.scale,
                                      _sfte.font.scale, rune);
        }

        _sfte.font.atlas_x += glyph_width + 1;  // padding to prevent bleeding
        if (g->y1 > _sfte.font.atlas_bottom) _sfte.font.atlas_bottom = g->y1;

        return g;
    }

    return NULL;  // out of space
}

static void _sfte_font_reset_cache(void) {
    memset(_sfte.font.atlas_pxs, 0, _sfte.font.atlas_width * _sfte.font.atlas_height);
    memset(_sfte.font.glyphs, 0, _sfte.font.glyph_cap * sizeof(sfte_glyph));
    _sfte.font.atlas_x = 0;
    _sfte.font.atlas_y = 0;
    _sfte.font.atlas_bottom = 0;

    _sfte.font.scale = stbtt_ScaleForPixelHeight(&_sfte.font.stb_info, _sfte.font.cur_size);

    // monospace grid using standard 'M' glyph
    sfte_glyph *m = _sfte_font_get_glyph('M'
#if defined(SFTE_FONT_BOLD_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
                                         ,
                                         0
#endif  // SFTE_FONT_BOLD_PATH || SFTE_FONT_BOLD_ITALIC_PATH
#if defined(SFTE_FONT_ITALIC_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
                                         ,
                                         0
#endif  // SFTE_FONT_ITALIC_PATH || SFTE_FONT_BOLD_ITALIC_PATH
    );
    _sfte.font.cell_width = m->xadvance;
    _sfte.font.cell_height = (int)(_sfte.font.cur_size * 1.2f + 0.5f);
}

static void _sfte_font_load(void) {
    _sfte.font.cur_size = SFTE_FONT_DEFAULT_SIZE;
    _sfte.font.atlas_width = 1024;
    _sfte.font.atlas_height = 1024;

    _sfte.font.atlas_pxs = (uint8_t *)malloc(_sfte.font.atlas_width * _sfte.font.atlas_height);
    SFTE_ASSERT(_sfte.font.atlas_pxs, "failed to allocate font atlas");

    // NOTE: explicitly not in #if SFTE_FONT_PATH, so that it only compiles if config is correct
    FILE *f = fopen(SFTE_FONT_PATH, "rb");
    SFTE_ASSERT(f, "failed to open font file");
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    _sfte.font.ttf_buf = (uint8_t *)malloc(size);
    SFTE_ASSERT(fread(_sfte.font.ttf_buf, 1, size, f) == size, "failed to read font file");
    fclose(f);

#ifdef SFTE_FONT_BOLD_PATH
    FILE *f_bold = fopen(SFTE_FONT_BOLD_PATH, "rb");
    SFTE_ASSERT(f_bold, "failed to open bold font file");

    fseek(f_bold, 0, SEEK_END);
    size_t size_bold = ftell(f_bold);
    fseek(f_bold, 0, SEEK_SET);

    _sfte.font.ttf_bold_buf = (uint8_t *)malloc(size_bold);
    SFTE_ASSERT(fread(_sfte.font.ttf_bold_buf, 1, size_bold, f_bold) == size_bold,
                "failed to read bold font file");
    fclose(f_bold);

    stbtt_InitFont(&_sfte.font.stb_bold_info, _sfte.font.ttf_bold_buf, 0);
#endif  // SFTE_FONT_BOLD_PATH

#ifdef SFTE_FONT_ITALIC_PATH
    FILE *f_italic = fopen(SFTE_FONT_ITALIC_PATH, "rb");
    SFTE_ASSERT(f_italic, "failed to open italic font file");

    fseek(f_italic, 0, SEEK_END);
    size_t size_italic = ftell(f_italic);
    fseek(f_italic, 0, SEEK_SET);

    _sfte.font.ttf_italic_buf = (uint8_t *)malloc(size_italic);
    SFTE_ASSERT(fread(_sfte.font.ttf_italic_buf, 1, size_italic, f_italic) == size_italic,
                "failed to read italic font file");
    fclose(f_italic);

    stbtt_InitFont(&_sfte.font.stb_italic_info, _sfte.font.ttf_italic_buf, 0);
#endif  // SFTE_FONT_ITALIC_PATH

#ifdef SFTE_FONT_BOLD_ITALIC_PATH
    FILE *f_bold_italic = fopen(SFTE_FONT_BOLD_ITALIC_PATH, "rb");
    SFTE_ASSERT(f_bold_italic, "failed to open bold italic font file");

    fseek(f_bold_italic, 0, SEEK_END);
    size_t size_bold_italic = ftell(f_bold_italic);
    fseek(f_bold_italic, 0, SEEK_SET);

    _sfte.font.ttf_bold_italic_buf = (uint8_t *)malloc(size_bold_italic);
    SFTE_ASSERT(fread(_sfte.font.ttf_bold_italic_buf, 1, size_bold_italic, f_bold_italic) ==
                    size_bold_italic,
                "failed to read bold italic font file");
    fclose(f_bold_italic);

    stbtt_InitFont(&_sfte.font.stb_bold_italic_info, _sfte.font.ttf_bold_italic_buf, 0);
#endif  // SFTE_FONT_BOLD_ITALIC_PATH

    _sfte.font.glyph_cap = 4096;
    _sfte.font.glyphs = (sfte_glyph *)calloc(_sfte.font.glyph_cap, sizeof(sfte_glyph));
    SFTE_ASSERT(_sfte.font.glyphs, "failed to allocate glyphs storage");
    stbtt_InitFont(&_sfte.font.stb_info, _sfte.font.ttf_buf, 0);
    _sfte_font_reset_cache();

    _sfte.width = _sfte.term.cols * _sfte.font.cell_width + (2 * SFTE_PAD_X);
    _sfte.height = _sfte.term.rows * _sfte.font.cell_height + (2 * SFTE_PAD_Y);

    _SFTE_INFO(FONT_LOADED);
}

static void _sfte_wayland_render(void);
static void _sfte_term_resize(int new_cols, int new_rows);

#if SFTE_FONT_ZOOM
static void _sfte_font_resize(const sfte_arg *arg) {
    float delta = arg->f;

    float new_size = _sfte.font.cur_size + delta;
    if (new_size < 4.0f || new_size > 96.0f) return;
    _sfte.font.cur_size = new_size;

    _sfte_font_reset_cache();

    int new_cols = (_sfte.width - (2 * SFTE_PAD_X)) / _sfte.font.cell_width;
    int new_rows = (_sfte.height - (2 * SFTE_PAD_Y)) / _sfte.font.cell_height;
    if (new_cols < 1) new_cols = 1;
    if (new_rows < 1) new_rows = 1;

    if (new_cols != _sfte.term.cols || new_rows != _sfte.term.rows)
        _sfte_term_resize(new_cols, new_rows);

    _sfte_wayland_render();
}

static void _sfte_font_reset(const sfte_arg *dummy) {
    (void)dummy;
    const sfte_arg arg = {.f = SFTE_FONT_DEFAULT_SIZE - _sfte.font.cur_size};
    _sfte_font_resize(&arg);
}
#endif  // SFTE_FONT_ZOOM

static const sfte_shortcut _sfte_shortcuts[] = SFTE_SHORTCUTS;

// >>render
#if SFTE_DOUBLE_BUFFER
#define _SFTE_RENDER_BUF (_sfte.back_buffer)
#else
#define _SFTE_RENDER_BUF (_sfte.shm_data)
#endif  // !SFTE_DOUBLE_BUFFER

static void _sfte_render_bg(int col, int row, uint32_t bg) {
    int cx = col * _sfte.font.cell_width + SFTE_PAD_X;
    int cy = row * _sfte.font.cell_height + SFTE_PAD_Y;
    uint32_t final_bg = (bg & 0x00FFFFFF) | (SFTE_BG_OPACITY << 24);

    for (int y = 0; y < _sfte.font.cell_height; ++y) {
        for (int x = 0; x < _sfte.font.cell_width; ++x) {
            int px_idx = (cy + y) * _sfte.width + (cx + x);
            if (px_idx < _sfte.width * _sfte.height) _SFTE_RENDER_BUF[px_idx] = final_bg;
        }
    }
}

static void _sfte_render_fg(int col, int row, uint32_t rune, uint32_t fg
#if defined(SFTE_FONT_BOLD_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
                            ,
                            uint8_t is_bold
#endif  // SFTE_FONT_BOLD_PATH || SFTE_FONT_BOLD_ITALIC_PATH
#if defined(SFTE_FONT_ITALIC_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
                            ,
                            uint8_t is_italic
#endif  // SFTE_FONT_ITALIC_PATH || SFTE_FONT_BOLD_ITALIC_PATH
) {
    if (rune == ' ') return;

    sfte_glyph *g = _sfte_font_get_glyph(rune
#if defined(SFTE_FONT_BOLD_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
                                         ,
                                         is_bold
#endif  // SFTE_FONT_BOLD_PATH || SFTE_FONT_BOLD_ITALIC_PATH
#if defined(SFTE_FONT_ITALIC_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
                                         ,
                                         is_italic
#endif  // SFTE_FONT_ITALIC_PATH || SFTE_FONT_BOLD_ITALIC_PATH
    );
    if (!g) return;

    int cx = col * _sfte.font.cell_width + SFTE_PAD_X;
    int cy = row * _sfte.font.cell_height + SFTE_PAD_Y;

    int glyph_width = g->x1 - g->x0;
    int glyph_height = g->y1 - g->y0;

    int baseline = (int)(_sfte.font.cell_height * 0.8f);
    int draw_x = cx + (int)g->xoff;
    int draw_y = cy + baseline + (int)g->yoff;

    uint8_t fg_r = (fg >> 16) & 0xFF, fg_g = (fg >> 8) & 0xFF, fg_b = fg & 0xFF;

    for (int y = 0; y < glyph_height; ++y) {
        for (int x = 0; x < glyph_width; ++x) {
            int screen_x = draw_x + x;
            int screen_y = draw_y + y;
            if (screen_x < 0 || screen_x >= _sfte.width || screen_y < 0 || screen_y >= _sfte.height)
                continue;

            uint8_t alpha = _sfte.font
                                .atlas_pxs[(g->y0 + y) * _sfte.font.atlas_width + (g->x0 + x)];
            if (alpha == 0) continue;

            int px_idx = screen_y * _sfte.width + screen_x;

            if (alpha == 255)
                _SFTE_RENDER_BUF[px_idx] = (0xFF << 24) | (fg & 0x00FFFFFF);
            else {
                uint32_t dst = _SFTE_RENDER_BUF[px_idx];
                uint8_t bg_r = (dst >> 16) & 0xFF;
                uint8_t bg_g = (dst >> 8) & 0xFF;
                uint8_t bg_b = dst & 0xFF;

                uint8_t col_r = (fg_r * alpha + bg_r * (255 - alpha)) >> 8;
                uint8_t col_g = (fg_g * alpha + bg_g * (255 - alpha)) >> 8;
                uint8_t col_b = (fg_b * alpha + bg_b * (255 - alpha)) >> 8;

                _SFTE_RENDER_BUF[px_idx] = (SFTE_BG_OPACITY << 24) | (col_r << 16) | (col_g << 8) |
                                           col_b;
            }
        }
    }
}

// >>wayland
static void _sfte_wayland_create_buffer(void) {
    int stride = _sfte.width * 4;  // 4B/px (ARGB)
    _sfte.shm_size = stride * _sfte.height;

    int fd = memfd_create("sfte-buffer", MFD_CLOEXEC);
    SFTE_ASSERT(fd != -1, "failed to create memfd");
    SFTE_ASSERT(ftruncate(fd, _sfte.shm_size) != -1, "failed to truncate memfd");

    _sfte.shm_data = (uint32_t *)mmap(NULL, _sfte.shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                      0);
    SFTE_ASSERT(_sfte.shm_data != MAP_FAILED, "failed to mmap shm data");

#if SFTE_DOUBLE_BUFFER
    if (_sfte.back_buffer) free(_sfte.back_buffer);
    _sfte.back_buffer = (uint32_t *)malloc(_sfte.shm_size);
    SFTE_ASSERT(_sfte.back_buffer, "failed to allocate back buffer");
#endif  // SFTE_DOUBLE_BUFFER

    struct wl_shm_pool *pool = wl_shm_create_pool(_sfte.shm, fd, _sfte.shm_size);
    _sfte.buffer = wl_shm_pool_create_buffer(pool, 0, _sfte.width, _sfte.height, stride,
                                             WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);

    close(fd);
}

static inline sfte_cell *_sfte_get_view_cell(int c, int r) {
#if SFTE_SCROLLBACK_CAP
    int logical_r = r - _sfte.term.sb_offset;
    if (logical_r >= 0)
        return &_sfte.term.cells[_SFTE_IDX(c, logical_r)];
    else {
        int hist_idx = -logical_r;
        int ring_r = (_sfte.term.sb_head - hist_idx + (100 * _sfte.term.sb_cap)) %
                     _sfte.term.sb_cap;
        return &_sfte.term.scrollback[ring_r * _sfte.term.cols + c];
    }
#else
    return &_sfte.term.cells[_SFTE_IDX(c, r)];
#endif  // !SFTE_SCROLLBACK_CAP
}

#if SFTE_SCROLLBACK_CAP
static void _sfte_view_scroll(const sfte_arg *arg) {
#if SFTE_ALT_SCREEN
    if (_sfte.term.alt_active) return;
#endif  // SFTE_ALT_SCREEN

    int delta = arg->i;
    int new_off = _sfte.term.sb_offset + delta;

    if (new_off < 0) new_off = 0;
    int max_scroll = _sfte.term.sb_len < _sfte.term.sb_cap ? _sfte.term.sb_len : _sfte.term.sb_cap;
    if (new_off > max_scroll) new_off = max_scroll;

    if (new_off != _sfte.term.sb_offset) {
        _sfte.term.sb_offset = new_off;
        _sfte_dirty_range(0, _sfte.term.cols * _sfte.term.rows);
        _sfte_wayland_render();
    }
}
#endif  // SFTE_SCROLLBACK_CAP

#if SFTE_SELECTION
static char *_sfte_extract_selection(void) {
    if (!_sfte.term.sel_active) return NULL;

    int sx = _sfte.term.sel_start_x;
    int sy = _sfte.term.sel_start_y;
    int ex = _sfte.term.sel_end_x;
    int ey = _sfte.term.sel_end_y;

    // if dragging backwards, flip start/end pnts
    if (sy > ey || (sy == ey && sx > ex)) {
        int tmp = sy;
        sy = ey;
        ey = tmp;
        tmp = sx;
        sx = ex;
        ex = tmp;
    }

    int max_bytes = (ey - sy + 1) * (_sfte.term.cols * 4 + 1) + 1;
    char *buf = (char *)malloc(max_bytes);
    SFTE_ASSERT(buf, "failed to allocate temporary clipboard buffer");
    int pos = 0;

    for (int r = sy; r <= ey; ++r) {
        int row_start = (r == sy) ? sx : 0;
        int row_end = (r == ey) ? ex : _sfte.term.cols - 1;

        int phys_r = r;
#if SFTE_SCROLLBACK_CAP
        phys_r += _sfte.term.sb_offset;
#endif  // SFTE_SCROLLBACK_CAP

        // trim trailing spaces if selecting to edge
        int actual_end = row_end;
        if (actual_end == _sfte.term.cols - 1) {
            while (actual_end >= row_start) {
                sfte_cell *vcell = _sfte_get_view_cell(actual_end, phys_r);
                if (vcell->rune != ' ' && vcell->rune != 0) break;
                actual_end--;
            }
        }

        for (int c = row_start; c <= actual_end; ++c) {
            sfte_cell *vcell = _sfte_get_view_cell(c, phys_r);
            uint32_t rune = (vcell->rune && vcell->rune != ' ') ? vcell->rune : ' ';

            // convert 32b to UTF-8
            if (rune < 0x80)
                buf[pos++] = rune;
            else if (rune < 0x800) {
                buf[pos++] = 0xC0 | (rune >> 6);
                buf[pos++] = 0x80 | (rune & 0x3F);
            } else if (rune < 0x10000) {
                buf[pos++] = 0xE0 | (rune >> 12);
                buf[pos++] = 0x80 | ((rune >> 6) & 0x3F);
                buf[pos++] = 0x80 | (rune & 0x3F);
            } else {
                buf[pos++] = 0xF0 | (rune >> 18);
                buf[pos++] = 0x80 | ((rune >> 12) & 0x3F);
                buf[pos++] = 0x80 | ((rune >> 6) & 0x3F);
                buf[pos++] = 0x80 | (rune & 0x3F);
            }
        }

        if (r < ey) {
#if SFTE_REFLOW
            if (!_sfte_get_view_cell(_sfte.term.cols - 1, phys_r)->wrapped)
#endif  // SFTE_REFLOW
                buf[pos++] = '\n';
        }
    }

    buf[pos] = '\0';
    return buf;
}

static void _sfte_dirty_selection_rows(int y1, int y2) {
    int min_y = y1 < y2 ? y1 : y2;
    int max_y = y1 > y2 ? y1 : y2;

#if SFTE_SCROLLBACK_CAP
    min_y += _sfte.term.sb_offset;
    max_y += _sfte.term.sb_offset;
#endif  // SFTE_SCROLLBACK_CAP

    if (min_y < 0) min_y = 0;
    if (max_y >= _sfte.term.rows) max_y = _sfte.term.rows - 1;

    if (min_y <= max_y) {
        _sfte_dirty_range(min_y * _sfte.term.cols, (max_y - min_y + 1) * _sfte.term.cols);
    }
}

static inline int _sfte_is_selected(int c, int logical_r) {
    if (!_sfte.term.sel_active) return 0;

    int sx = _sfte.term.sel_start_x;
    int sy = _sfte.term.sel_start_y;
    int ex = _sfte.term.sel_end_x;
    int ey = _sfte.term.sel_end_y;

    // if dragging backwards, flip start/end pnts
    if (sy > ey || (sy == ey && sx > ex)) {
        int tmp = sy;
        sy = ey;
        ey = tmp;
        tmp = sx;
        sx = ex;
        ex = tmp;
    }

    if (logical_r < sy || logical_r > ey) return 0;
    if (sy == ey) return (c >= sx && c <= ex);  // same line
    if (logical_r == sy) return c >= sx;        // first line
    if (logical_r == ey) return c <= ex;        // last line
    return 1;                                   // middle lines
}
#endif  // SFTE_SELECTION

#if SFTE_CURSOR_DYNAMIC
#define _SFTE_CUR_STYLE (_sfte.term.cursor_style)
#else
#define _SFTE_CUR_STYLE (SFTE_CURSOR_STYLE)
#endif  // !SFTE_CURSOR_DYNAMIC

static void _sfte_wayland_render(void) {
    int new_cols = (_sfte.width - (2 * SFTE_PAD_X)) / _sfte.font.cell_width;
    if (new_cols < 1) new_cols = 1;
    int new_rows = (_sfte.height - (2 * SFTE_PAD_Y)) / _sfte.font.cell_height;
    if (new_rows < 1) new_rows = 1;

    // if compositor OR font scaling changed physical dims,
    // reallocate the grid before attempting to draw
    if (new_cols != _sfte.term.cols || new_rows != _sfte.term.rows)
        _sfte_term_resize(new_cols, new_rows);

    int vis_cx = _sfte.term.cursor_x >= _sfte.term.cols ? _sfte.term.cols - 1 : _sfte.term.cursor_x;
    int vis_cy = _sfte.term.cursor_y;
#if SFTE_SCROLLBACK_CAP
    vis_cy += _sfte.term.sb_offset;
#endif  // SFTE_SCROLLBACK_CAP

    // if cursor moved, dirtyy the old cell to erase it, and dirty the new cell to draw it
    if (_sfte.term.dirty_saved_x != vis_cx || _sfte.term.dirty_saved_y != vis_cy) {
        _sfte.term.cells[_SFTE_IDX(_sfte.term.dirty_saved_x, _sfte.term.dirty_saved_y)].dirty = 1;
#if SFTE_SCROLLBACK_CAP  // NOTE: this check is only needed if scrollback buffer is on
        if (vis_cy >= 0 && vis_cy < _sfte.term.rows)
#endif  // SFTE_SCROLLBACK_CAP
            _sfte.term.cells[_SFTE_IDX(vis_cx, vis_cy)].dirty = 1;
        _sfte.term.dirty_saved_x = vis_cx;
        _sfte.term.dirty_saved_y = vis_cy;
    }

    for (int r = 0; r < _sfte.term.rows; ++r) {
        for (int c = 0; c < _sfte.term.cols; ++c) {
            int idx = _SFTE_IDX(c, r);

            int is_dirty = _sfte.term.cells[idx].dirty;
            if (!is_dirty && c < _sfte.term.cols - 1 && _sfte.term.cells[idx + 1].dirty)
                is_dirty = 1;
            if (!is_dirty) continue;

            sfte_cell *vcell = _sfte_get_view_cell(c, r);
            uint32_t fg = vcell->fg ? vcell->fg : 0xFFFFFF;
            uint32_t bg = vcell->bg ? vcell->bg : SFTE_BG_COLOR;
            uint16_t attr = vcell->attr;

#if SFTE_SELECTION
            if (_sfte_is_selected(c, r
#if SFTE_SCROLLBACK_CAP
                                         - _sfte.term.sb_offset
#endif  // SFTE_SCROLLBACK_CAP
                                  ))
                attr |= ATTR_REVERSE;
#endif  // SFTE_SELECTION

            if (attr & ATTR_REVERSE) {
                uint32_t tmp = fg;
                fg = bg;
                bg = tmp;
            }

            int is_cursor = (c == vis_cx && r == vis_cy && !_sfte.term.hide_cursor);

#if SFTE_CURSOR_BLINK
            if (!_sfte.term.blink_visible) is_cursor = 0;
#endif  // SFTE_CURSOR_BLINK

            if (is_cursor && _SFTE_CUR_STYLE == SFTE_CURSOR_BLOCK)
                _sfte_render_bg(c, r, fg);  // inverse if under cursor block
            else
                _sfte_render_bg(c, r, bg);
        }
    }

    for (int r = 0; r < _sfte.term.rows; ++r) {
        for (int c = 0; c < _sfte.term.cols; ++c) {
            int idx = _SFTE_IDX(c, r);

            // certain nerd font symbols can have width greater than one cell. that's why if cell to
            // our right is dirty, its bg got repainted, so we redraw our foreground.
            int is_dirty = _sfte.term.cells[idx].dirty;
            if (!is_dirty && c < _sfte.term.cols - 1 && _sfte.term.cells[idx + 1].dirty)
                is_dirty = 1;
            if (!is_dirty) continue;

            sfte_cell *vcell = _sfte_get_view_cell(c, r);
            uint32_t rune = vcell->rune;
            if (rune == 0) rune = ' ';

            uint32_t fg = vcell->fg ? vcell->fg : 0xFFFFFF;
            uint32_t bg = vcell->bg ? vcell->bg : SFTE_BG_COLOR;
            uint16_t attr = vcell->attr;

            if (attr & ATTR_REVERSE) {
                uint32_t tmp = fg;
                fg = bg;
                bg = tmp;
            }

#ifdef SFTE_BOLD_WHITE
            if ((attr & ATTR_BOLD)) fg = 0xFFFFFF;
#endif  // SFTE_BOLD_WHITE

            int is_cursor = (c == vis_cx && r == vis_cy && !_sfte.term.hide_cursor);

#if SFTE_CURSOR_BLINK
            if (!_sfte.term.blink_visible) is_cursor = 0;
#endif  // SFTE_CURSOR_BLINK

            uint32_t draw_fg = fg;
            if (is_cursor && _SFTE_CUR_STYLE == SFTE_CURSOR_BLOCK) draw_fg = bg;

            _sfte_render_fg(c, r, rune, draw_fg
#if defined(SFTE_FONT_BOLD_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
                            ,
                            attr & ATTR_BOLD
#endif  // SFTE_FONT_BOLD_PATH || SFTE_FONT_BOLD_ITALIC_PATH
#if defined(SFTE_FONT_ITALIC_PATH) || defined(SFTE_FONT_BOLD_ITALIC_PATH)
                            ,
                            attr & ATTR_ITALIC
#endif  // SFTE_FONT_ITALIC_PATH || SFTE_FONT_BOLD_ITALIC_PATH
            );

            if (attr & ATTR_UNDERLINE) {
                int cx = c * _sfte.font.cell_width + SFTE_PAD_X;
                int cy = r * _sfte.font.cell_height + SFTE_PAD_Y;

                uint32_t underline_col = (draw_fg & 0x00FFFFFF) | (0xFF << 24);

                int thickness = _sfte.font.cell_height / 10;
                if (thickness < 1) thickness = 1;

                for (int y = cy + _sfte.font.cell_height - thickness;
                     y < cy + _sfte.font.cell_height; ++y)
                    for (int x = cx; x < cx + _sfte.font.cell_width; ++x)
                        if (x < _sfte.width && y < _sfte.height)
                            _SFTE_RENDER_BUF[y * _sfte.width + x] = underline_col;
            }

            if (is_cursor && _SFTE_CUR_STYLE != SFTE_CURSOR_BLOCK) {  // bar / underline
                int cx = c * _sfte.font.cell_width + SFTE_PAD_X;
                int cy = r * _sfte.font.cell_height + SFTE_PAD_Y;

                uint32_t cur_col = (SFTE_CURSOR_COLOR & 0x00FFFFFF) | (0xFF << 24);

                if (_SFTE_CUR_STYLE == SFTE_CURSOR_UNDERLINE) {
                    int thickness = _sfte.font.cell_height / 10;
                    if (thickness < 1) thickness = 1;

                    for (int y = cy + _sfte.font.cell_height - thickness;
                         y < cy + _sfte.font.cell_height; ++y)
                        for (int x = cx; x < cx + _sfte.font.cell_width; ++x)
                            if (x < _sfte.width && y < _sfte.height)
                                _SFTE_RENDER_BUF[y * _sfte.width + x] = cur_col;
                } else if (_SFTE_CUR_STYLE == SFTE_CURSOR_BAR) {
                    int thickness = _sfte.font.cell_width / 10;
                    if (thickness < 1) thickness = 1;

                    for (int y = cy; y < cy + _sfte.font.cell_height; ++y)
                        for (int x = cx; x < cx + thickness; ++x)
                            if (x < _sfte.width && y < _sfte.height)
                                _SFTE_RENDER_BUF[y * _sfte.width + x] = cur_col;
                }
            }

            // submit localized damage to compositor
            wl_surface_damage_buffer(_sfte.surface, c * _sfte.font.cell_width + SFTE_PAD_X,
                                     r * _sfte.font.cell_height + SFTE_PAD_Y, _sfte.font.cell_width,
                                     _sfte.font.cell_height);
            _sfte.term.cells[idx].dirty = 0;
        }
    }

#if SFTE_DOUBLE_BUFFER
    memcpy(_sfte.shm_data, _sfte.back_buffer, _sfte.shm_size);
#endif  // SFTE_DOUBLE_BUFFER

#if SFTE_CURSOR_TRAIL
    if (_sfte.term.trail_damage_w > 0)
        wl_surface_damage_buffer(_sfte.surface, _sfte.term.trail_damage_x,
                                 _sfte.term.trail_damage_y, _sfte.term.trail_damage_w,
                                 _sfte.term.trail_damage_h);

    if (!_sfte.term.hide_cursor && _sfte.term.is_trailing) {
        int vis_cx = _sfte.term.cursor_x >= _sfte.term.cols ? _sfte.term.cols - 1
                                                            : _sfte.term.cursor_x;
        float target_rx = vis_cx * _sfte.font.cell_width;
        float target_ry = _sfte.term.cursor_y * _sfte.font.cell_height;

        float min_x = target_rx < _sfte.term.tail_rx ? target_rx : _sfte.term.tail_rx;
        float max_x = (target_rx > _sfte.term.tail_rx ? target_rx : _sfte.term.tail_rx) +
                      _sfte.font.cell_width;
        float min_y = target_ry < _sfte.term.tail_ry ? target_ry : _sfte.term.tail_ry;
        float max_y = (target_ry > _sfte.term.tail_ry ? target_ry : _sfte.term.tail_ry) +
                      _sfte.font.cell_height;

        int px_min_x = (int)min_x + SFTE_PAD_X;
        int px_max_x = (int)max_x + SFTE_PAD_X;
        int px_min_y = (int)min_y + SFTE_PAD_Y;
        int px_max_y = (int)max_y + SFTE_PAD_Y;

        uint8_t cr = (SFTE_CURSOR_COLOR >> 16) & 0xFF;
        uint8_t cg = (SFTE_CURSOR_COLOR >> 8) & 0xFF;
        uint8_t cb = SFTE_CURSOR_COLOR & 0xFF;

        float w = _sfte.font.cell_width;
        float h = _sfte.font.cell_height;
        float cx0 = _sfte.term.tail_rx + w * 0.5f;
        float cy0 = _sfte.term.tail_ry + h * 0.5f;
        float cx1 = target_rx + w * 0.5f;
        float cy1 = target_ry + h * 0.5f;

        float ab_x = cx1 - cx0;
        float ab_y = cy1 - cy0;
        float l2 = ab_x * ab_x + ab_y * ab_y;

        for (int y = px_min_y; y < px_max_y; ++y) {
            for (int x = px_min_x; x < px_max_x; ++x) {
                if (x < 0 || x >= _sfte.width || y < 0 || y >= _sfte.height) continue;

                int hx = (int)target_rx + SFTE_PAD_X;
                int hy = (int)target_ry + SFTE_PAD_Y;
                if (x >= hx && x < hx + _sfte.font.cell_width && y >= hy &&
                    y < hy + _sfte.font.cell_height)
                    continue;

                float ap_x = (float)(x - SFTE_PAD_X) - cx0 + 0.5f;
                float ap_y = (float)(y - SFTE_PAD_Y) - cy0 + 0.5f;

                float t = 0.0f;
                if (l2 > 0.001f) {
                    t = (ap_x * ab_x + ap_y * ab_y) / l2;
                    if (t < 0.0f) t = 0.0f;
                    if (t > 1.0f) t = 1.0f;
                }

                float proj_x = cx0 + t * ab_x;
                float proj_y = cy0 + t * ab_y;

                float dist_x = (float)(x - SFTE_PAD_X) + 0.5f - proj_x;
                float dist_y = (float)(y - SFTE_PAD_Y) + 0.5f - proj_y;
                if (dist_x < 0) dist_x = -dist_x;
                if (dist_y < 0) dist_y = -dist_y;

                if (dist_x <= w * 0.5f && dist_y <= h * 0.5f) {
                    int alpha = (int)(128.0f * t);
                    if (alpha == 0) continue;
                    int inv_alpha = 255 - alpha;

                    uint32_t bg = _sfte.shm_data[y * _sfte.width + x];
                    uint8_t bg_r = (bg >> 16) & 0xFF;
                    uint8_t bg_g = (bg >> 8) & 0xFF;
                    uint8_t bg_b = bg & 0xFF;

                    uint8_t r = (cr * alpha + bg_r * inv_alpha) >> 8;
                    uint8_t g = (cg * alpha + bg_g * inv_alpha) >> 8;
                    uint8_t b = (cb * alpha + bg_b * inv_alpha) >> 8;

                    _sfte.shm_data[y * _sfte.width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                }
            }
        }

        _sfte.term.trail_damage_x = px_min_x;
        _sfte.term.trail_damage_y = px_min_y;
        _sfte.term.trail_damage_w = px_max_x - px_min_x;
        _sfte.term.trail_damage_h = px_max_y - px_min_y;

        wl_surface_damage_buffer(_sfte.surface, _sfte.term.trail_damage_x,
                                 _sfte.term.trail_damage_y, _sfte.term.trail_damage_w,
                                 _sfte.term.trail_damage_h);
    } else
        _sfte.term.trail_damage_w = 0;
#endif  // SFTE_CURSOR_TRAIL

    wl_surface_attach(_sfte.surface, _sfte.buffer, 0, 0);
    wl_surface_commit(_sfte.surface);
}

#if SFTE_CLIPBOARD
static void _sfte_data_offer_offer(void *data, struct wl_data_offer *offer, const char *mime_type) {
    (void)data;
    if (strcmp(mime_type, "text/plain;charset=utf-8") == 0 ||
        strcmp(mime_type, "text/plain") == 0) {
        wl_data_offer_accept(offer, _sfte.serial, mime_type);
    }
}

static void _sfte_data_offer_source_actions(void *data, struct wl_data_offer *offer,
                                            uint32_t actions) {
    (void)data, (void)offer, (void)actions;
}

static void _sfte_data_offer_action(void *data, struct wl_data_offer *offer, uint32_t action) {
    (void)data, (void)offer, (void)action;
}

static const struct wl_data_offer_listener _sfte_data_offer_listener = {
    .offer = _sfte_data_offer_offer,
    .source_actions = _sfte_data_offer_source_actions,
    .action = _sfte_data_offer_action,
};

static void _sfte_data_device_data_offer(void *data, struct wl_data_device *device,
                                         struct wl_data_offer *offer) {
    (void)data, (void)device;
    wl_data_offer_add_listener(offer, &_sfte_data_offer_listener, NULL);
}

static void _sfte_data_device_enter(void *data, struct wl_data_device *device, uint32_t serial,
                                    struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y,
                                    struct wl_data_offer *offer) {
    (void)data, (void)device, (void)serial, (void)surface, (void)x, (void)y, (void)offer;
}

static void _sfte_data_device_leave(void *data, struct wl_data_device *device) {
    (void)data, (void)device;
}

static void _sfte_data_device_motion(void *data, struct wl_data_device *device, uint32_t time,
                                     wl_fixed_t x, wl_fixed_t y) {
    (void)data, (void)device, (void)time, (void)x, (void)y;
}

static void _sfte_data_device_drop(void *data, struct wl_data_device *device) {
    (void)data, (void)device;
}

static void _sfte_data_device_selection(void *data, struct wl_data_device *device,
                                        struct wl_data_offer *offer) {
    if (_sfte.data_offer && _sfte.data_offer != offer) wl_data_offer_destroy(_sfte.data_offer);
    _sfte.data_offer = offer;
}

static const struct wl_data_device_listener _sfte_data_device_listener = {
    .data_offer = _sfte_data_device_data_offer,
    .enter = _sfte_data_device_enter,
    .leave = _sfte_data_device_leave,
    .motion = _sfte_data_device_motion,
    .drop = _sfte_data_device_drop,
    .selection = _sfte_data_device_selection,
};

static void _sfte_data_source_target(void *data, struct wl_data_source *src,
                                     const char *mime_type) {
    (void)data, (void)src, (void)mime_type;
}

static void _sfte_data_source_send(void *data, struct wl_data_source *src, const char *mime_type,
                                   int32_t fd) {
    if (_sfte.selection_text) write(fd, _sfte.selection_text, strlen(_sfte.selection_text));

    close(fd);
}

static void _sfte_data_source_cancelled(void *data, struct wl_data_source *src) {
    wl_data_source_destroy(src);
    if (_sfte.selection_text) {
        free(_sfte.selection_text);
        _sfte.selection_text = NULL;
    }
    _sfte.data_source = NULL;
}

static void _sfte_data_source_dnd_drop_performed(void *data, struct wl_data_source *src) {
    (void)data, (void)src;
}

static void _sfte_data_source_dnd_finished(void *data, struct wl_data_source *src) {
    (void)data, (void)src;
}

static void _sfte_data_source_action(void *data, struct wl_data_source *src, uint32_t action) {
    (void)data, (void)src, (void)action;
}

static const struct wl_data_source_listener _sfte_data_source_listener = {
    .target = _sfte_data_source_target,
    .send = _sfte_data_source_send,
    .cancelled = _sfte_data_source_cancelled,
    .dnd_drop_performed = _sfte_data_source_dnd_drop_performed,
    .dnd_finished = _sfte_data_source_dnd_finished,
    .action = _sfte_data_source_action,
};
#endif  // SFTE_CLIPBOARD

#if SFTE_SELECTION
static void _sfte_update_hover(wl_fixed_t surface_x, wl_fixed_t surface_y) {
    int px_x = wl_fixed_to_int(surface_x) - SFTE_PAD_X;
    int px_y = wl_fixed_to_int(surface_y) - SFTE_PAD_Y;

    int grid_x = px_x / _sfte.font.cell_width;
    int grid_y = px_y / _sfte.font.cell_height;

    grid_x = _SFTE_CLAMP(grid_x, 0, _sfte.term.cols - 1);
    grid_y = _SFTE_CLAMP(grid_y, 0, _sfte.term.rows - 1);

#if SFTE_SCROLLBACK_CAP
    grid_y -= _sfte.term.sb_offset;
#endif  // SFTE_SCROLLBACK_CAP

    _sfte.term.hover_x = grid_x;
    _sfte.term.hover_y = grid_y;
}

static void _sfte_wayland_pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
                                        struct wl_surface *surface, wl_fixed_t surface_x,
                                        wl_fixed_t surface_y) {
    (void)data, (void)pointer, (void)serial, (void)surface;
    _sfte_update_hover(surface_x, surface_y);
}

static void _sfte_wayland_pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial,
                                        struct wl_surface *surface) {
    (void)data, (void)pointer, (void)serial, (void)surface;
}

static void _sfte_wayland_pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time,
                                         wl_fixed_t surface_x, wl_fixed_t surface_y) {
    (void)data, (void)pointer, (void)time;
    _sfte_update_hover(surface_x, surface_y);

    if (!_sfte.term.sel_dragging) return;

    if (_sfte.term.sel_end_x != _sfte.term.hover_x || _sfte.term.sel_end_y != _sfte.term.hover_y) {
        // dirty old bounds to erase prev highlight
        _sfte_dirty_selection_rows(_sfte.term.sel_start_y, _sfte.term.sel_end_y);

        _sfte.term.sel_end_x = _sfte.term.hover_x;
        _sfte.term.sel_end_y = _sfte.term.hover_y;

        // dirty new bounds to draw new highlight
        _sfte_dirty_selection_rows(_sfte.term.sel_start_y, _sfte.term.sel_end_y);

        _sfte_wayland_render();
    }
}

static void _sfte_wayland_pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                                         uint32_t time, uint32_t button, uint32_t state) {
    if (button != 0x110) return;  // lmb

#if SFTE_CLIPBOARD
    _sfte.serial = serial;
#endif  // SFTE_CLIPBOARD

    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        if (_sfte.term.sel_active)
            _sfte_dirty_selection_rows(_sfte.term.sel_start_y, _sfte.term.sel_end_y);

        _sfte.term.sel_start_x = _sfte.term.hover_x;
        _sfte.term.sel_start_y = _sfte.term.hover_y;
        _sfte.term.sel_end_x = _sfte.term.hover_x;
        _sfte.term.sel_end_y = _sfte.term.hover_y;
        _sfte.term.sel_active = 1;
        _sfte.term.sel_dragging = 1;

        _sfte_dirty_selection_rows(_sfte.term.sel_start_y, _sfte.term.sel_end_y);
        _sfte_wayland_render();
    } else if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
        _sfte.term.sel_dragging = 0;

        if (_sfte.term.sel_start_x == _sfte.term.sel_end_x &&
            _sfte.term.sel_start_y == _sfte.term.sel_end_y) {
            _sfte.term.sel_active = 0;

            _sfte_dirty_selection_rows(_sfte.term.sel_start_y, _sfte.term.sel_end_y);
            _sfte_wayland_render();
        }

#if SFTE_CLIPBOARD
        _sfte_clipboard_copy(NULL);
#endif  // SFTE_CLIPBOARD
    }
}

static void _sfte_wayland_pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time,
                                       uint32_t axis, wl_fixed_t value) {
    (void)data, (void)pointer, (void)time, (void)axis, (void)value;
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

static void _sfte_wayland_keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format,
                                          int32_t fd, uint32_t size) {
    (void)data, (void)keyboard;
    SFTE_ASSERT(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, "unsupported keymap format");

    char *map_str = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    SFTE_ASSERT(map_str != MAP_FAILED, "failed to mmap keyboard");

    if (_sfte.xkb_keymap) xkb_keymap_unref(_sfte.xkb_keymap);
    if (_sfte.xkb_state) xkb_state_unref(_sfte.xkb_state);

    _sfte.xkb_keymap = xkb_keymap_new_from_string(
        _sfte.xkb_context, map_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    _sfte.xkb_state = xkb_state_new(_sfte.xkb_keymap);

    _SFTE_INFO(KEYMAP_LOADED);
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
#if SFTE_CLIPBOARD
    _sfte.serial = serial;
#endif  // SFTE_CLIPBOARD

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED && key == _sfte.repeating_key) {
        struct itimerspec its = {0};
        timerfd_settime(_sfte.repeat_timer_fd, 0, &its, NULL);
        _sfte.repeating_key = 0;
        return;
    }

    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !_sfte.xkb_state) return;

    if (_sfte.repeat_rate > 0 && _sfte.repeating_key != key) {
        struct itimerspec its;
        its.it_value.tv_sec = _sfte.repeat_delay / 1000;
        its.it_value.tv_nsec = (_sfte.repeat_delay % 1000) * 1000000;
        its.it_interval.tv_sec = 0;
        if (_sfte.repeat_rate > 0)
            its.it_interval.tv_nsec = 1000000000 / _sfte.repeat_rate;
        else
            its.it_interval.tv_nsec = 0;

        timerfd_settime(_sfte.repeat_timer_fd, 0, &its, NULL);
        _sfte.repeating_key = key;
    }

    xkb_keycode_t keycode = key + 8;  // WARN: evdev codes are offset by 8 from xkb keycodes
    xkb_keysym_t sym = xkb_state_key_get_one_sym(_sfte.xkb_state, keycode);

    bool ctrl = xkb_state_mod_name_is_active(_sfte.xkb_state, XKB_MOD_NAME_CTRL,
                                             XKB_STATE_MODS_EFFECTIVE);
    bool alt = xkb_state_mod_name_is_active(_sfte.xkb_state, XKB_MOD_NAME_ALT,
                                            XKB_STATE_MODS_EFFECTIVE);
    bool shift = xkb_state_mod_name_is_active(_sfte.xkb_state, XKB_MOD_NAME_SHIFT,
                                              XKB_STATE_MODS_EFFECTIVE);

    uint32_t active_mods = SFTE_MOD_NONE;
    if (ctrl) active_mods |= SFTE_MOD_CTRL;
    if (alt) active_mods |= SFTE_MOD_ALT;
    if (shift) active_mods |= SFTE_MOD_SHIFT;

    for (size_t i = 0; i < _SFTE_ARRAY_LEN(_sfte_shortcuts); ++i) {
        if ((xkb_keysym_t)_sfte_shortcuts[i].keysym != sym ||
            _sfte_shortcuts[i].mod_mask != active_mods)
            continue;

        _sfte_shortcuts[i].func(&_sfte_shortcuts[i].arg);
        return;
    }

    char buf[128];
    int size = 0;

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
        if (size == 0) size = xkb_state_key_get_utf8(_sfte.xkb_state, keycode, buf, sizeof(buf));
    }
    // if alt is held, prepend esc byte
    if (alt && size > 0 && size < (int)(sizeof(buf) - 1)) {
        memmove(buf + 1, buf, size++);
        buf[0] = '\033';
    }

    if (size > 0) {
#if SFTE_SCROLLBACK_CAP
        if (_sfte.term.sb_offset > 0) {
            _sfte.term.sb_offset = 0;
            _sfte_dirty_range(0, _sfte.term.cols * _sfte.term.rows);
            _sfte_wayland_render();
        }
#endif  // SFTE_SCROLLBACK_CAP

        write(_sfte.pty_fd, buf, size);
    }
}

static void _sfte_wayland_keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                                             uint32_t serial, uint32_t mods_depressed,
                                             uint32_t mods_latched, uint32_t mods_locked,
                                             uint32_t group) {
    (void)data, (void)keyboard, (void)serial;
    if (!_sfte.xkb_state) return;
    xkb_state_update_mask(_sfte.xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void _sfte_wayland_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
                                               int32_t rate, int32_t delay) {
    (void)data, (void)keyboard;
    _sfte.repeat_rate = rate;
    _sfte.repeat_delay = delay;
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
    (void)data;
    // if seat has a keyboard and we haven't grabbed it yet
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !_sfte.keyboard) {
        _sfte.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(_sfte.keyboard, &_sfte_wayland_keyboard_listener, &_sfte);
    }
    // if seat lost keyboard and we still hold the ptr
    else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && _sfte.keyboard) {
        wl_keyboard_release(_sfte.keyboard);
        _sfte.keyboard = NULL;
    }

#if SFTE_SELECTION
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !_sfte.pointer) {
        _sfte.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(_sfte.pointer, &_sfte_wayland_pointer_listener, &_sfte);

#if SFTE_CLIPBOARD
        if (_sfte.data_device_manager && !_sfte.data_device) {
            _sfte.data_device = (struct wl_data_device *)wl_data_device_manager_get_data_device(
                _sfte.data_device_manager, seat);
            wl_data_device_add_listener(_sfte.data_device, &_sfte_data_device_listener, NULL);
        }
#endif  // SFTE_CLIPBOARD
    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && _sfte.pointer) {
        wl_pointer_release(_sfte.pointer);
        _sfte.pointer = NULL;
    }
#endif  // SFTE_SELECTION
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
    (void)data, (void)version;
    if (strcmp(interface, wl_compositor_interface.name) == 0)
        _sfte.compositor = (struct wl_compositor *)wl_registry_bind(
            registry, name, &wl_compositor_interface, 4 /* wl compositor version */);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        _sfte.shm = (struct wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface,
                                                      1 /* wl shm version */);
    else if (strcmp(interface, wl_seat_interface.name) == 0) {
        _sfte.seat = (struct wl_seat *)wl_registry_bind(registry, name, &wl_seat_interface,
                                                        7 /* wl seat version */);
        wl_seat_add_listener(_sfte.seat, &_sfte_wayland_seat_listener, &_sfte);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        _sfte.xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(
            registry, name, &xdg_wm_base_interface, 1 /* xdg wm base version */);
        xdg_wm_base_add_listener(_sfte.xdg_wm_base, &_sfte_wayland_xdg_wm_base_listener, &_sfte);
    }
#if SFTE_CLIPBOARD
    else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
        _sfte.data_device_manager = (struct wl_data_device_manager *)wl_registry_bind(
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
    (void)data;
    xdg_surface_ack_configure(xdg_surface, serial);

    // resize recalc
    int needed_size = _sfte.width * _sfte.height * 4;
    if (_sfte.shm_size != needed_size) {
        if (_sfte.buffer) wl_buffer_destroy(_sfte.buffer);
        if (_sfte.shm_data) munmap(_sfte.shm_data, _sfte.shm_size);
        _sfte_wayland_create_buffer();
    }

    _sfte_wayland_render();
}

static const struct xdg_surface_listener _sfte_wayland_xdg_surface_listener = {
    .configure = _sfte_wayland_xdg_surface_configure,
};

static void _sfte_wayland_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                                                 int32_t width, int32_t height,
                                                 struct wl_array *states) {
    (void)data, (void)xdg_toplevel, (void)states;
    if (width <= 0 || height <= 0) return;

    _sfte.width = width;
    _sfte.height = height;
}

static void _sfte_wayland_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    (void)data, (void)xdg_toplevel;
    _sfte.running = 0;
}

static const struct xdg_toplevel_listener _sfte_wayland_xdg_toplevel_listener = {
    .configure = _sfte_wayland_xdg_toplevel_configure,
    .close = _sfte_wayland_xdg_toplevel_close,
};

static void _sfte_wayland_load(void) {
    _sfte.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    SFTE_ASSERT(_sfte.xkb_context, "failed to create xkb context");

    _sfte.display = wl_display_connect(NULL);
    SFTE_ASSERT(_sfte.display, "failed to connect to Wayland display\n");

    _sfte.registry = wl_display_get_registry(_sfte.display);
    wl_registry_add_listener(_sfte.registry, &_sfte_wayland_registry_listener, &_sfte);

    wl_display_roundtrip(_sfte.display);
    SFTE_ASSERT(_sfte.compositor, "failed to initialize compositor\n");
    SFTE_ASSERT(_sfte.shm, "compositor missing required interfaces\n");
    SFTE_ASSERT(_sfte.xdg_wm_base, "failed to bind xdg_wm_base\n");

    _sfte.surface = wl_compositor_create_surface(_sfte.compositor);
    _sfte.xdg_surface = xdg_wm_base_get_xdg_surface(_sfte.xdg_wm_base, _sfte.surface);
    xdg_surface_add_listener(_sfte.xdg_surface, &_sfte_wayland_xdg_surface_listener, &_sfte);

    _sfte.xdg_toplevel = xdg_surface_get_toplevel(_sfte.xdg_surface);
    xdg_toplevel_add_listener(_sfte.xdg_toplevel, &_sfte_wayland_xdg_toplevel_listener, &_sfte);
    xdg_toplevel_set_title(_sfte.xdg_toplevel, "sfte");
    xdg_toplevel_set_app_id(_sfte.xdg_toplevel, "sfte");

    wl_surface_commit(_sfte.surface);
    wl_display_roundtrip(_sfte.display);
    _SFTE_INFO(WAYLAND_REGISTRY_BOUND);
}

static void _sfte_wayland_unload(void) {
#if SFTE_DOUBLE_BUFFER
    free(_sfte.back_buffer);
#endif  // SFTE_DOUBLE_BUFFER
    free(_sfte.term.tab_stops);
    free(_sfte.font.ttf_buf);
#ifdef SFTE_FONT_BOLD_PATH
    free(_sfte.font.ttf_bold_buf);
#endif  // SFTE_FONT_BOLD_PATH
#ifdef SFTE_FONT_ITALIC_PATH
    free(_sfte.font.ttf_italic_buf);
#endif  // SFTE_FONT_ITALIC_PATH
    free(_sfte.font.atlas_pxs);
    free(_sfte.term.cells);
#if SFTE_ALT_SCREEN
    free(_sfte.term.alt_cells);
#endif  // SFTE_ALT_SCREEN
#if SFTE_SCROLLBACK_CAP
    free(_sfte.term.scrollback);
#endif  // SFTE_SCROLLBACK_CAP

    if (_sfte.buffer) wl_buffer_destroy(_sfte.buffer);
    if (_sfte.shm_data) munmap(_sfte.shm_data, _sfte.shm_size);
    if (_sfte.xdg_toplevel) xdg_toplevel_destroy(_sfte.xdg_toplevel);
    if (_sfte.xdg_surface) xdg_surface_destroy(_sfte.xdg_surface);
    if (_sfte.surface) wl_surface_destroy(_sfte.surface);
    if (_sfte.xdg_wm_base) xdg_wm_base_destroy(_sfte.xdg_wm_base);
    if (_sfte.keyboard) wl_keyboard_release(_sfte.keyboard);
    if (_sfte.seat) wl_seat_release(_sfte.seat);

    wl_registry_destroy(_sfte.registry);
    wl_display_disconnect(_sfte.display);

    xkb_state_unref(_sfte.xkb_state);
    xkb_keymap_unref(_sfte.xkb_keymap);
    xkb_context_unref(_sfte.xkb_context);
}

#if SFTE_CLIPBOARD
static void _sfte_clipboard_copy(const sfte_arg *arg) {
    (void)arg;

    if (_sfte.data_source) {
        wl_data_source_destroy(_sfte.data_source);
        _sfte.data_source = NULL;
    }
    if (_sfte.selection_text) {
        free(_sfte.selection_text);
        _sfte.selection_text = NULL;
    }

    if (_sfte.term.sel_active && _sfte.data_device_manager && _sfte.data_device) {
        _sfte.selection_text = _sfte_extract_selection();

        if (_sfte.selection_text) {
            _sfte.data_source = wl_data_device_manager_create_data_source(
                _sfte.data_device_manager);
            wl_data_source_add_listener(_sfte.data_source, &_sfte_data_source_listener, NULL);
            wl_data_source_offer(_sfte.data_source, "text/plain;charset=utf-8");
            wl_data_source_offer(_sfte.data_source, "text/plain");
            wl_data_device_set_selection(_sfte.data_device, _sfte.data_source, _sfte.serial);
        }
    }
}

static void _sfte_clipboard_paste(const sfte_arg *arg) {
    (void)arg;
    if (!_sfte.data_offer) return;

    int fds[2];
    if (pipe(fds) == 0) {
        wl_data_offer_receive(_sfte.data_offer, "text/plain;charset=utf-8", fds[1]);
        close(fds[1]);

        wl_display_roundtrip(_sfte.display);

        char buf[SFTE_CLIPBOARD_BUF_SIZE];
        ssize_t n;
        while ((n = read(fds[0], buf, sizeof(buf))) > 0) write(_sfte.pty_fd, buf, n);

        close(fds[0]);
    }
}
#endif  // SFTE_CLIPBOARD

// >>state
#if SFTE_CURSOR_BLINK
static inline uint64_t _sfte_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif  // SFTE_CURSOR_BLINK

static void _sfte_state_load(void) {
    memset(&_sfte, 0, sizeof(_sfte));

    _sfte.running = 1;
#ifndef SFTE_NO_LOGGING
    _sfte.logger.func = SFTE_LOGGER_FUNC;
#endif  // !SFTE_NO_LOGGING
    _sfte.term.cols = 80;
    _sfte.term.rows = 24;
    _sfte.term.auto_wrap = 1;
    _sfte.term.origin_mode = 0;

#if SFTE_SCROLLBACK_CAP
    _sfte.term.sb_cap = SFTE_SCROLLBACK_CAP;
    _sfte.term.scrollback = (sfte_cell *)calloc(_sfte.term.sb_cap * _sfte.term.cols,
                                                sizeof(sfte_cell));
#endif  // SFTE_SCROLLBACK_CAP

    _sfte.term.tab_stops = (uint8_t *)malloc(_sfte.term.cols);
    for (int i = 0; i < _sfte.term.cols; ++i) _sfte.term.tab_stops[i] = (i > 0 && i % 8 == 0);

#if SFTE_CURSOR_BLINK
    _sfte.term.blink_enabled = 1;
    _sfte.term.blink_visible = 1;
    _sfte.term.next_blink_ms = _sfte_time_ms() + SFTE_CURSOR_BLINK_RATE;
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_TRAIL
    _sfte.term.tail_rx = 0.0f;
    _sfte.term.tail_ry = 0.0f;
    _sfte.term.trail_damage_x = 0.0f;
    _sfte.term.trail_damage_y = 0.0f;
    _sfte.term.trail_damage_w = 0.0f;
    _sfte.term.trail_damage_h = 0.0f;
    _sfte.term.last_grid_x = 0;
    _sfte.term.last_grid_y = 0;
    _sfte.term.last_move_ms = 0;
    _sfte.term.is_trailing = 0;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_CURSOR_DYNAMIC
    _sfte.term.cursor_style = SFTE_CURSOR_STYLE;
#endif  // SFTE_CURSOR_DYNAMIC
    _sfte.term.scroll_top = 0;
    _sfte.term.scroll_bottom = _sfte.term.rows - 1;

    _sfte.term.cells = (sfte_cell *)malloc(_sfte.term.cols * _sfte.term.rows * sizeof(sfte_cell));
    SFTE_ASSERT(_sfte.term.cells, "failed to allocate term grid");
    memset(_sfte.term.cells, 0, _sfte.term.cols * _sfte.term.rows * sizeof(sfte_cell));
}

// >>pty
static void _sfte_pty_spawn(void) {
    // NOTE: ws_col/ws_row will not be hardcoded when font rendering will be present
    struct winsize ws = {
        .ws_row = (unsigned short)_sfte.term.rows,
        .ws_col = (unsigned short)_sfte.term.cols,
        .ws_xpixel = (unsigned short)_sfte.width,
        .ws_ypixel = (unsigned short)_sfte.height,
    };

    _sfte.pty_pid = forkpty(&_sfte.pty_fd, NULL, NULL, &ws);
    SFTE_ASSERT(_sfte.pty_pid != -1, "failed to forkpty");

    if (_sfte.pty_pid == 0) {
        setenv("TERM", SFTE_TERM_ENV, 1);
        char *shell = getenv("SHELL");
        if (!shell) shell = (char *)"/bin/sh";
        execlp(shell, shell, NULL);  // replace current pimg with shell
        abort();                     // if execlp returns, it failed to exec the shell
    }

    _SFTE_INFO(PTY_SPAWN);
}

// >>vt
static const uint32_t _sfte_ansi_palette[16] = SFTE_ANSI_PALETTE;

#if SFTE_REFLOW
typedef struct {
    sfte_cell *temp_rows;
    int new_cols;
    int tr, tc;
    int new_cx, new_cy;
    int target_old_cx, target_old_cy;
    int is_live;
} sfte_reflow_state;

static void _sfte_reflow_push(sfte_reflow_state *st, sfte_cell c, int is_cursor) {
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

static void _sfte_term_resize(int new_cols, int new_rows) {
#if SFTE_ALT_SCREEN
    sfte_cell *main_old = _sfte.term.alt_active ? _sfte.term.alt_cells : _sfte.term.cells;
    sfte_cell *alt_old = _sfte.term.alt_active ? _sfte.term.cells : NULL;

    int target_cx = _sfte.term.alt_active ? _sfte.term.ansi_saved_x : _sfte.term.cursor_x;
    int target_cy = _sfte.term.alt_active ? _sfte.term.ansi_saved_y : _sfte.term.cursor_y;
#else
    sfte_cell *main_old = _sfte.term.cells;
    sfte_cell *alt_old = NULL;

    int target_cx = _sfte.term.cursor_x;
    int target_cy = _sfte.term.cursor_y;
#endif  // !SFTE_ALT_SCREEN

    int max_temp_rows = (
#if SFTE_SCROLLBACK_CAP
                            _sfte.term.sb_len +
#endif  // SFTE_SCROLLBACK_CAP
                            _sfte.term.rows) *
                        (_sfte.term.cols / new_cols + 2);
    if (max_temp_rows < new_rows) max_temp_rows = new_rows;
    sfte_cell *temp_rows = (sfte_cell *)calloc(max_temp_rows * new_cols, sizeof(sfte_cell));
    SFTE_ASSERT(temp_rows, "failed to allocate temporary row data");

    sfte_reflow_state st = {.temp_rows = temp_rows,
                            .new_cols = new_cols,
                            .tr = 0,
                            .tc = 0,
                            .new_cx = 0,
                            .new_cy = 0,
                            .target_old_cx = target_cx,
                            .target_old_cy = target_cy,
                            .is_live = 0};

#if SFTE_SCROLLBACK_CAP
    for (int i = 0; i < _sfte.term.sb_len; ++i) {
        int ring_idx = (_sfte.term.sb_head - _sfte.term.sb_len + i + _sfte.term.sb_cap) %
                       _sfte.term.sb_cap;
        sfte_cell *row = &_sfte.term.scrollback[ring_idx * _sfte.term.cols];
        int is_wrapped = row[_sfte.term.cols - 1].wrapped;

        int len = _sfte.term.cols;
        if (!is_wrapped)
            while (len > 0 && (row[len - 1].rune == ' ' || row[len - 1].rune == 0) &&
                   row[len - 1].bg == SFTE_BG_COLOR)
                len--;

        for (int c = 0; c < len; ++c) _sfte_reflow_push(&st, row[c], 0);
        if (!is_wrapped) {
            st.tc = 0;
            st.tr++;
        }
    }
#endif  // SFTE_SCROLLBACK_CAP

    // reflow live grid
    st.is_live = 1;
    for (int r = 0; r < _sfte.term.rows; ++r) {
        sfte_cell *row = &main_old[r * _sfte.term.cols];
        int is_wrapped = row[_sfte.term.cols - 1].wrapped;

        int len = _sfte.term.cols;
        if (!is_wrapped) {
            while (len > 0 && (row[len - 1].rune == ' ' || row[len - 1].rune == 0) &&
                   row[len - 1].bg == SFTE_BG_COLOR) {
                if (r == st.target_old_cy && len - 1 == st.target_old_cx) break;
                len--;
            }
            if (r == st.target_old_cy && len <= st.target_old_cx) len = st.target_old_cx + 1;
        }

        for (int c = 0; c < len; ++c) {
            int is_cursor = (r == st.target_old_cy && c == st.target_old_cx);
            _sfte_reflow_push(&st, row[c], is_cursor);
        }

        if (r == st.target_old_cy && st.target_old_cx >= len) {
            if (st.tc == new_cols) {
                st.temp_rows[st.tr * new_cols + new_cols - 1].wrapped = 1;
                st.tc = 0;
                st.tr++;
            }
            st.new_cx = st.tc;
            st.new_cy = st.tr;
        }

        if (!is_wrapped) {
            st.tc = 0;
            st.tr++;
        }
    }

    int total_lines = st.tr + (st.tc > 0 ? 1 : 0);

    // map into new layout arrays
    sfte_cell *new_main = (sfte_cell *)calloc(new_cols * new_rows, sizeof(sfte_cell));
    SFTE_ASSERT(new_main, "failed to allocate resized terminal grid");

    int screen_top = total_lines - new_rows;
    if (st.new_cy >= screen_top + new_rows) screen_top = st.new_cy - new_rows + 1;
    screen_top = _SFTE_CLAMP(screen_top, 0, st.new_cy);

#if SFTE_SCROLLBACK_CAP
    sfte_cell *new_sb = (sfte_cell *)calloc(_sfte.term.sb_cap * new_cols, sizeof(sfte_cell));
    SFTE_ASSERT(new_sb, "failed to allocate resized scrollback");

    int sb_lines = screen_top;
    if (sb_lines > _sfte.term.sb_cap) sb_lines = _sfte.term.sb_cap;
    int sb_start = screen_top - sb_lines;

    for (int i = 0; i < sb_lines; ++i)
        memcpy(&new_sb[i * new_cols], &temp_rows[(sb_start + i) * new_cols],
               new_cols * sizeof(sfte_cell));
#endif  // SFTE_SCROLLBACK_CAP

    int copy_lines = total_lines - screen_top;
    if (copy_lines > new_rows) copy_lines = new_rows;
    for (int i = 0; i < copy_lines; ++i)
        memcpy(&new_main[i * new_cols], &temp_rows[(screen_top + i) * new_cols],
               new_cols * sizeof(sfte_cell));

// anchor cursors
#if SFTE_ALT_SCREEN
    if (_sfte.term.alt_active) {
        _sfte.term.ansi_saved_x = st.new_cx;
        _sfte.term.ansi_saved_y = _SFTE_CLAMP(st.new_cy - screen_top, 0, new_rows - 1);
    } else {
#endif  // SFTE_ALT_SCREEN
        _sfte.term.cursor_x = st.new_cx;
        _sfte.term.cursor_y = _SFTE_CLAMP(st.new_cy - screen_top, 0, new_rows - 1);
#if SFTE_ALT_SCREEN
    }
#endif  // SFTE_ALT_SCREEN

#if SFTE_ALT_SCREEN
    // hard copy alt grid
    // NOTE: alt grid gets no reflow, it destroys visuals of alt-screen based interfaces
    sfte_cell *new_alt = NULL;
    if (alt_old) {
        new_alt = (sfte_cell *)calloc(new_cols * new_rows, sizeof(sfte_cell));
        SFTE_ASSERT(new_alt, "failed to allocate resized alt grid");

        int min_cols = new_cols < _sfte.term.cols ? new_cols : _sfte.term.cols;
        int min_rows = new_rows < _sfte.term.rows ? new_rows : _sfte.term.rows;
        for (int r = 0; r < min_rows; ++r)
            for (int c = 0; c < min_cols; ++c)
                new_alt[r * new_cols + c] = alt_old[r * _sfte.term.cols + c];

        if (_sfte.term.cursor_x >= new_cols) _sfte.term.cursor_x = new_cols - 1;
        if (_sfte.term.cursor_y >= new_rows) _sfte.term.cursor_y = new_rows - 1;
    }
#endif  // SFTE_ALT_SCREEN

    free(_sfte.term.cells);
    free(temp_rows);

#if SFTE_ALT_SCREEN
    if (_sfte.term.alt_cells) free(_sfte.term.alt_cells);
    _sfte.term.cells = _sfte.term.alt_active ? new_alt : new_main;
    _sfte.term.alt_cells = _sfte.term.alt_active ? new_main : NULL;
#else
    _sfte.term.cells = new_main;
#endif  // !SFTE_ALT_SCREEN

#if SFTE_SCROLLBACK_CAP
    if (_sfte.term.scrollback) free(_sfte.term.scrollback);
    _sfte.term.scrollback = new_sb;
    _sfte.term.sb_head = sb_lines % _sfte.term.sb_cap;
    _sfte.term.sb_offset = 0;
    _sfte.term.sb_len = sb_lines;
#endif  // SFTE_SCROLLBACK_CAP

    _sfte.term.cols = new_cols;
    _sfte.term.rows = new_rows;
    _sfte.term.scroll_top = 0;
    _sfte.term.scroll_bottom = new_rows - 1;

    _sfte_dirty_range(0, new_cols * new_rows);

    uint8_t *new_tabs = (uint8_t *)malloc(new_cols);
    SFTE_ASSERT(new_tabs, "failed to allocate new tab stops");
    for (int i = 0; i < new_cols; ++i)
        if (i < _sfte.term.cols)
            new_tabs[i] = _sfte.term.tab_stops[i];
        else
            new_tabs[i] = (i % 8 == 0);
    free(_sfte.term.tab_stops);
    _sfte.term.tab_stops = new_tabs;

    struct winsize ws = {.ws_row = (unsigned short)new_rows,
                         .ws_col = (unsigned short)new_cols,
                         .ws_xpixel = (unsigned short)_sfte.width,
                         .ws_ypixel = (unsigned short)_sfte.height};

    ioctl(_sfte.pty_fd, TIOCSWINSZ, &ws);

    uint32_t clear_col = (SFTE_BG_COLOR & 0x00FFFFFF) | (SFTE_BG_OPACITY << 24);
    for (int i = 0; i < _sfte.width * _sfte.height; ++i) _sfte.shm_data[i] = clear_col;
#if SFTE_DOUBLE_BUFFER
    for (int i = 0; i < _sfte.width * _sfte.height; ++i) _sfte.back_buffer[i] = clear_col;
#endif  // SFTE_DOUBLE_BUFFER

    _SFTE_INFO(TERM_RESIZE, new_cols, new_rows);
}
#else  // !SFTE_REFLOW
static void _sfte_term_resize(int new_cols, int new_rows) {
    sfte_cell *new_cells = (sfte_cell *)calloc(new_cols * new_rows, sizeof(sfte_cell));
    SFTE_ASSERT(new_cells, "failed to allocate resized terminal grid");

    sfte_cell *new_alt_cells = NULL;
#if SFTE_ALT_SCREEN
    if (_sfte.term.alt_cells) {
        new_alt_cells = (sfte_cell *)calloc(new_cols * new_rows, sizeof(sfte_cell));
        SFTE_ASSERT(new_alt_cells, "failed to allocate resized alt grid");
    }
#endif  // SFTE_ALT_SCREEN

#if SFTE_SCROLLBACK_CAP
    if (_sfte.term.scrollback) free(_sfte.term.scrollback);
    _sfte.term.scrollback = (sfte_cell *)calloc(_sfte.term.sb_cap * new_cols, sizeof(sfte_cell));
    _sfte.term.sb_head = 0;
    _sfte.term.sb_offset = 0;
    _sfte.term.sb_len = 0;
#endif  // SFTE_SCROLLBACK_CAP

    uint8_t *new_tabs = (uint8_t *)malloc(new_cols);
    SFTE_ASSERT(new_tabs, "failed to allocate new tab stops");
    for (int i = 0; i < new_cols; ++i)
        if (i < _sfte.term.cols)
            new_tabs[i] = _sfte.term.tab_stops[i];
        else
            new_tabs[i] = (i % 8 == 0);
    free(_sfte.term.tab_stops);
    _sfte.term.tab_stops = new_tabs;

    int min_cols = new_cols < _sfte.term.cols ? new_cols : _sfte.term.cols;
    int min_rows = new_rows < _sfte.term.rows ? new_rows : _sfte.term.rows;

    for (int r = 0; r < min_rows; ++r) {
        for (int c = 0; c < min_cols; ++c) {
            new_cells[r * new_cols + c] = _sfte.term.cells[r * _sfte.term.cols + c];
#if SFTE_ALT_SCREEN
            if (new_alt_cells)
                new_alt_cells[r * new_cols + c] = _sfte.term.alt_cells[r * _sfte.term.cols + c];
#endif  // SFTE_ALT_SCREEN
        }
    }

    free(_sfte.term.cells);
    _sfte.term.cells = new_cells;
    if (_sfte.term.alt_cells) {
        free(_sfte.term.alt_cells);
        _sfte.term.alt_cells = new_alt_cells;
    }

    _sfte.term.cols = new_cols;
    _sfte.term.rows = new_rows;

    _sfte.term.scroll_top = 0;
    _sfte.term.scroll_bottom = new_rows - 1;

    if (_sfte.term.cursor_x >= new_cols) _sfte.term.cursor_x = new_cols - 1;
    if (_sfte.term.cursor_y >= new_rows) _sfte.term.cursor_y = new_rows - 1;

    _sfte_dirty_range(0, new_cols * new_rows);

    struct winsize ws = {.ws_row = (unsigned short)new_rows,
                         .ws_col = (unsigned short)new_cols,
                         .ws_xpixel = (unsigned short)_sfte.width,
                         .ws_ypixel = (unsigned short)_sfte.height};

    ioctl(_sfte.pty_fd, TIOCSWINSZ, &ws);

    uint32_t clear_col = (SFTE_BG_COLOR & 0x00FFFFFF) | (SFTE_BG_OPACITY << 24);
    for (int i = 0; i < _sfte.width * _sfte.height; ++i) _sfte.shm_data[i] = clear_col;
#if SFTE_DOUBLE_BUFFER
    for (int i = 0; i < _sfte.width * _sfte.height; ++i) _sfte.back_buffer[i] = clear_col;
#endif  // SFTE_DOUBLE_BUFFER

    _SFTE_INFO(TERM_RESIZE, new_cols, new_rows);
}
#endif  // !SFTE_REFLOW

static inline void _sfte_clear_cells(int start_idx, int cnt) {
    for (int i = 0; i < cnt; ++i) {
        _sfte.term.cells[start_idx + i].rune = ' ';
        _sfte.term.cells[start_idx + i].fg = _sfte.term.cur_fg;
        _sfte.term.cells[start_idx + i].bg = _sfte.term.cur_bg;
        _sfte.term.cells[start_idx + i].attr = 0;
        _sfte.term.cells[start_idx + i].dirty = 1;
#if SFTE_REFLOW
        _sfte.term.cells[start_idx + i].wrapped = 0;
#endif  // SFTE_REFLOW
    }
}

static void _sfte_scroll(int lines) {
    int top = _sfte.term.scroll_top;
    int bot = _sfte.term.scroll_bottom;
    int height = bot - top + 1;
    int cols = _sfte.term.cols;

    if (lines > 0) {  // scroll up
        if (lines > height) lines = height;

#if SFTE_SCROLLBACK_CAP
        if (top == 0
#if SFTE_ALT_SCREEN
            && !_sfte.term.alt_active
#endif  // SFTE_ALT_SCREEN
        ) {
            for (int i = 0; i < lines; ++i) {
                int ring_idx = _sfte.term.sb_head * cols;
                int screen_idx = i * cols;
                memcpy(&_sfte.term.scrollback[ring_idx], &_sfte.term.cells[screen_idx],
                       cols * sizeof(sfte_cell));

                _sfte.term.sb_head = (_sfte.term.sb_head + 1) % _sfte.term.sb_cap;
                if (_sfte.term.sb_len < _sfte.term.sb_cap) _sfte.term.sb_len++;
            }
        }
#endif  // SFTE_SCROLLBACK_CAP

        int move_cnt = height - lines;
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[top * cols], &_sfte.term.cells[(top + lines) * cols],
                    move_cnt * cols * sizeof(sfte_cell));

        int start_idx = (bot - lines + 1) * cols;
        _sfte_clear_cells(start_idx, lines * cols);  // clear lines at bot
    } else if (lines < 0) {                          // scroll down
        lines = -lines;
        if (lines > height) lines = height;

        int move_cnt = height - lines;
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[(top + lines) * cols], &_sfte.term.cells[top * cols],
                    move_cnt * cols * sizeof(sfte_cell));

        int start_idx = top * cols;
        _sfte_clear_cells(start_idx, lines * cols);
    }

    _sfte_dirty_range(top * cols, height * cols);
}

static inline void _sfte_check_wrap(void) {
    if (_sfte.term.cursor_x >= _sfte.term.cols) {
        if (_sfte.term.auto_wrap) {
#if SFTE_REFLOW
            _sfte.term.cells[_SFTE_IDX(_sfte.term.cols - 1, _sfte.term.cursor_y)].wrapped = 1;
#endif  // SFTE_REFLOW
            _sfte.term.cursor_x = 0;
            if (_sfte.term.cursor_y == _sfte.term.scroll_bottom)
                _sfte_scroll(1);
            else if (_sfte.term.cursor_y < _sfte.term.rows - 1)
                _sfte.term.cursor_y++;
        } else
            _sfte.term.cursor_x = _sfte.term.cols - 1;
    }
}

#if SFTE_TRUE_COLOR
static inline uint32_t _sfte_parse_truecolor(int *p, int i) {
    return (p[i + 2] << 16) | (p[i + 3] << 8) | p[i + 4];
}
#endif  // SFTE_TRUE_COLOR

static void _sfte_dispatch_csi(uint8_t cmd) {
    int *p = _sfte.term.vt_params;
    int cnt = _sfte.term.vt_param_idx + 1;

    int cx = _sfte.term.cursor_x >= _sfte.term.cols ? _sfte.term.cols - 1 : _sfte.term.cursor_x;

    switch (cmd) {
    case '@':  // ICH / Insert Character
    {
        /*
          Inserts n (default 1) spaces at the cursor position.
          Text shifts right.
          Text pushed off the right edge is lost.
         */
        int n = p[0] > 0 ? p[0] : 1;
        int rem = _sfte.term.cols - cx;
        if (n > rem) n = rem;

        int move_cnt = rem - n;
        int base_idx = _SFTE_IDX(0, _sfte.term.cursor_y);
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[base_idx + cx + n], &_sfte.term.cells[base_idx + cx],
                    move_cnt * sizeof(sfte_cell));

        int start_idx = base_idx + cx;
        _sfte_clear_cells(start_idx, n);
        _sfte_dirty_range(base_idx + cx, rem);
        break;
    }
    case 'A':  // CUU / Cursor Up
    {
        /*
          Moves the cursor n (default 1) cells up.
          If the cursor is already at the edge of the screen, this has no effect.
         */
        _sfte.term.cursor_y -= (p[0] > 0 ? p[0] : 1);
        _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_y, 0, _sfte.term.rows - 1);
        break;
    }
    case 'B':  // CUD / Cursor Down
    {
        /*
          Moves the cursor n (default 1) cells down.
          If the cursor is already at the edge of the screen, this has no effect.
         */
        _sfte.term.cursor_y += (p[0] > 0 ? p[0] : 1);
        _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_y, 0, _sfte.term.rows - 1);
        break;
    }
    case 'C':  // CUF / Cursor Forward
    {
        /*
          Moves the cursor n (default 1) cells forward.
          If the cursor is already at the edge of the screen, this has no effect.
         */
        _sfte.term.cursor_x += (p[0] > 0 ? p[0] : 1);
        _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.cursor_x, 0, _sfte.term.cols - 1);
        break;
    }
    case 'D':  // CUB / Cursor Back
    {
        /*
          Moves the cursor n (default 1) cells back.
          If the cursor is already at the edge of the screen, this has no effect.
         */
        _sfte.term.cursor_x -= (p[0] > 0 ? p[0] : 1);
        _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.cursor_x, 0, _sfte.term.cols - 1);
        break;
    }
    case 'E':  // CNL / Cursor Next Line
    {
        /*
          Moves cursor to the beginning of the line n (default 1) lines down.
         */
        _sfte.term.cursor_x = 0;
        _sfte.term.cursor_y += (p[0] > 0 ? p[0] : 1);
        _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_y, 0, _sfte.term.rows - 1);
        break;
    }
    case 'F':  // CPL / Cursor Next Line
    {
        /*
          Moves cursor to the beginning of the line n (default 1) lines up.
         */
        _sfte.term.cursor_x = 0;
        _sfte.term.cursor_y -= (p[0] > 0 ? p[0] : 1);
        _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_y, 0, _sfte.term.rows - 1);
        break;
    }
    case 'G':  // CHA / Cursor Horizontal Absolute
    {
        /*
          Moves the cursor to column n (default 1).
         */
        _sfte.term.cursor_x = (p[0] > 0 ? p[0] : 1) - 1;
        _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.cursor_x, 0, _sfte.term.cols - 1);
        break;
    }
    case 'H':  // CUP / Cursor Position
    {
        /*
          Moves the cursor to row n, column m.
          The values are 1-based, and default to 1 (top left corner) if omitted.
          A sequence such as CSI ;5H is a synonym for CSI 1;5H as well as
          CSI 17;H is the same as CSI 17H and CSI 17;1H.
         */
        _sfte.term.cursor_x = (cnt > 1 && p[1] > 0 ? p[1] : 1) - 1;
        _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.cursor_x, 0, _sfte.term.cols - 1);
        _sfte.term.cursor_y = (p[0] > 0 ? p[0] : 1) - 1;
        _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_y, 0, _sfte.term.rows - 1);
        break;
    }
    case 'J':  // ED / Erase in Display
    {
        /*
          Clears part of the screen.
          If n is 0 (or missing), clear from cursor to end of screen.
          If n is 1, clear from cursor to beginning of the screen.
          If n is 2, clear entire screen (and moves cursor to upper left on DOS ANSI.SYS)
          If n is 3, clear entire screen and delete all lines saved in the scrollback buffer.
         */
        if (p[0] == 0) {
            int start_idx = _SFTE_IDX(cx, _sfte.term.cursor_y);
            _sfte_clear_cells(start_idx, (_sfte.term.rows * _sfte.term.cols) - start_idx);
        } else if (p[0] == 1)
            _sfte_clear_cells(0, _SFTE_IDX(cx, _sfte.term.cursor_y) + 1);
        else if (p[0] == 2)
            _sfte_clear_cells(0, _sfte.term.rows * _sfte.term.cols);
        /* NOTE: n = 3 unhandled since scrollback buffer is missing */
        break;
    }
    case 'K':  // EL / Erase in Line
    {
        /*
          Erases part of the line.
          If n is 0 (or missing), clear from cursor to the end of the line.
          If n is 1, clear from cursor to beginning of the line.
          If n is 2, clear entire line.
          Cursor position does not change.
         */
        if (p[0] == 0)
            _sfte_clear_cells(_SFTE_IDX(cx, _sfte.term.cursor_y), _sfte.term.cols - cx);
        else if (p[0] == 1)
            _sfte_clear_cells(_SFTE_IDX(0, _sfte.term.cursor_y), cx + 1);
        else if (p[0] == 2)
            _sfte_clear_cells(_SFTE_IDX(0, _sfte.term.cursor_y), _sfte.term.cols);
        break;
    }
    case 'L':  // IL / Insert Line
    {
        /*
          Inserts n (default 1) blank lines at cursor position.
          Lines below the cursor are pushed down.
          Bottom lines are lost.
         */
        int n = p[0] > 0 ? p[0] : 1;
        int top = _sfte.term.cursor_y;
        int bot = _sfte.term.scroll_bottom;
        if (top < _sfte.term.scroll_top || top > bot) break;  // oob

        int height = bot - top + 1;
        if (n > height) n = height;

        int move_cnt = height - n;
        int cols = _sfte.term.cols;
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[(top + n) * cols], &_sfte.term.cells[top * cols],
                    move_cnt * cols * sizeof(sfte_cell));

        int start_idx = top * cols;
        _sfte_clear_cells(start_idx, n * cols);
        _sfte_dirty_range(top * cols, height * cols);
        break;
    }
    case 'M':  // DL / Delete Line
    {
        /*
          Deletes n (default 1) lines at the cursor position.
          Lines below the cursor are pulled up.
          Bottom lines are blanked.
         */
        int n = p[0] > 0 ? p[0] : 1;
        int top = _sfte.term.cursor_y;
        int bot = _sfte.term.scroll_bottom;
        if (top < _sfte.term.scroll_top || top > bot) break;  // oob

        int height = bot - top + 1;
        if (n > height) n = height;

        int move_cnt = height - n;
        int cols = _sfte.term.cols;
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[top * cols], &_sfte.term.cells[(top + n) * cols],
                    move_cnt * cols * sizeof(sfte_cell));

        int start_idx = (bot - n + 1) * cols;
        _sfte_clear_cells(start_idx, n * cols);
        _sfte_dirty_range(top * cols, height * cols);
        break;
    }
    case 'P':  // DCH / Delete Character
    {
        /*
          Deletes n (default 1) characters at the cursor position.
          Text to the right shifts left.
          End of line is blanked.
         */
        int n = p[0] > 0 ? p[0] : 1;
        int rem = _sfte.term.cols - cx;
        if (n > rem) n = rem;

        int move_cnt = rem - n;
        int base_idx = _SFTE_IDX(0, _sfte.term.cursor_y);
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[base_idx + cx], &_sfte.term.cells[base_idx + cx + n],
                    move_cnt * sizeof(sfte_cell));

        int start_idx = base_idx + _sfte.term.cols - n;
        _sfte_clear_cells(start_idx, n);
        _sfte_dirty_range(base_idx + cx, rem);
        break;
    }
    case 'S':  // SU / Scroll Up
    {
        /*
          Scroll whole page up by n (default 1) lines.
          New lines are added at the bottom.
         */
        _sfte_scroll(p[0] > 0 ? p[0] : 1);
        break;
    }
    case 'T':  // SD / Scroll Down
    {
        /*
          Scroll whole page down by n (default 1) lines.
          New lines are added at the top.
         */
        _sfte_scroll(-(p[0] > 0 ? p[0] : 1));
        break;
    }
    case 'X':  // ECH / Erase Character
    {
        /*
          Replaces n (default 1) characters with spaces starting at the cursor.
         */
        int n = p[0] > 0 ? p[0] : 1;
        int rem = _sfte.term.cols - cx;
        if (n > rem) n = rem;

        _sfte_clear_cells(_SFTE_IDX(cx, _sfte.term.cursor_y), n);
        break;
    }
    case 'c':  // DA / Device Attributes
    {
        /*
          Reports the terminal's identity and capabilities to the host.
         */
        if (_sfte.term.vt_dec_priv == 2) {
            // terminal type and version
            // 0 = VT100, 95 = xterm version, 0 = ROM
            const char *sda = "\033[>0;95;0c";
            write(_sfte.pty_fd, sda, strlen(sda));
        } else {
            const char *da = "\033[?62c";  // VT220
            write(_sfte.pty_fd, da, strlen(da));
        }
        break;
    }
    case 'd':  // VPA / Vertical Position Absolute
    {
        /*
          Moves cursor to the specific row n (default 1).
          Column remains the same.
          Format effector function, leads to different handling in certain terminal modes.
         */
        _sfte.term.cursor_y = (p[0] > 0 ? p[0] : 1) - 1;

        if (_sfte.term.origin_mode) {
            _sfte.term.cursor_y += _sfte.term.scroll_top;
            _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_y, _sfte.term.scroll_top,
                                              _sfte.term.scroll_bottom);
        } else
            _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_y, 0, _sfte.term.rows - 1);

        break;
    }
    case 'f':  // HVP / Horizontal Vertical Position
    {
        /*
          Same as CUP, but counts as a format effector function (like CR or LF)
          rather than an editor function (like CUD or CNL).
          This leads to different handling in certain terminal modes.
         */
        _sfte.term.cursor_x = (cnt > 1 && p[1] > 0 ? p[1] : 1) - 1;
        _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.cursor_x, 0, _sfte.term.cols - 1);
        _sfte.term.cursor_y = (p[0] > 0 ? p[0] : 1) - 1;

        if (_sfte.term.origin_mode) {  // relative bounds
            _sfte.term.cursor_y += _sfte.term.scroll_top;
            _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_x, _sfte.term.scroll_top,
                                              _sfte.term.scroll_bottom);
        } else
            _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_y, 0, _sfte.term.rows - 1);

        break;
    }
    case 'g':  // TBC / Tab Clear
    {
        /*
          Clears tab stops.
          If n is 0, clear stop at the current column.
          If n is 3, clear all stops.

          NOTE: ECMA-48 defines additional parameters (1, 2, 4, 5) for managing
          vertical tab stops and single-line clears. These were never supported
          by original VT100, so they're intentionally left unhandled.
         */
        if (p[0] == 0)
            _sfte.term.tab_stops[_sfte.term.cursor_x] = 0;
        else if (p[0] == 3)
            memset(_sfte.term.tab_stops, 0, _sfte.term.cols);
        break;
    }
    case 'h':  // SM / Set Mode
    {
        /*
          Enables various terminal modes.
          Supports DECTCEM (Cursor Show), DECAWM (Auto-Wrap),
          DECOM (Origin Mode), and alt screen buffer toggles.
         */
        if (!_sfte.term.vt_dec_priv) break;

        if (p[0] == 25) {
            _sfte.term.hide_cursor = 0;
            _sfte.term.cells[_SFTE_IDX(cx, _sfte.term.cursor_y)].dirty = 1;
        } else if (p[0] == 7)
            _sfte.term.auto_wrap = 1;
        else if (p[0] == 6) {
            _sfte.term.origin_mode = 1;
            _sfte.term.cursor_x = 0;
            _sfte.term.cursor_y = _sfte.term.scroll_top;
        } else if (p[0] == 1047 || p[0] == 1048 || p[0] == 1049) {
            // 1048 / 1049 save cursor
            if (p[0] == 1048 || p[0] == 1049) {
                _sfte.term.ansi_saved_x = _sfte.term.cursor_x;
                _sfte.term.ansi_saved_y = _sfte.term.cursor_y;
                _sfte.term.ansi_saved_fg = _sfte.term.cur_fg;
                _sfte.term.ansi_saved_bg = _sfte.term.cur_bg;
                _sfte.term.ansi_saved_attr = _sfte.term.cur_attr;
            }

#if SFTE_ALT_SCREEN
            // 1047 / 1049 switch to alt screen
            if ((p[0] == 1047 || p[0] == 1049) && !_sfte.term.alt_active) {
                _sfte.term.alt_active = 1;
#if SFTE_CURSOR_TRAIL
                _sfte.term.last_move_ms = 0;
#endif  // SFTE_CURSOR_TRAIL

                if (!_sfte.term.alt_cells)
                    _sfte.term.alt_cells = (sfte_cell *)calloc(_sfte.term.cols * _sfte.term.rows,
                                                               sizeof(sfte_cell));

                sfte_cell *tmp = _sfte.term.cells;
                _sfte.term.cells = _sfte.term.alt_cells;
                _sfte.term.alt_cells = tmp;
            }
#endif  // SFTE_ALT_SCREEN

            if (p[0] == 1049) {
                _sfte_clear_cells(0, _sfte.term.cols * _sfte.term.rows);
                _sfte.term.cursor_x = 0;
                _sfte.term.cursor_y = 0;
            } else if (p[0] == 1047) {
                _sfte_dirty_range(0, _sfte.term.cols * _sfte.term.rows);
            }
        }
        break;
    }
    case 'l':  // RM / Reset Mode
    {
        /*
          Disables various terminal modes.
          Matches the implementations found in SM.
         */
        if (!_sfte.term.vt_dec_priv) break;

        if (p[0] == 25) {
            _sfte.term.hide_cursor = 1;
            _sfte.term.cells[_SFTE_IDX(cx, _sfte.term.cursor_y)].dirty = 1;
        } else if (p[0] == 7)
            _sfte.term.auto_wrap = 0;
        else if (p[0] == 6) {
            _sfte.term.origin_mode = 0;
            _sfte.term.cursor_x = 0;
            _sfte.term.cursor_y = 0;
        } else if (p[0] == 1047 || p[0] == 1048 || p[0] == 1049) {
#if SFTE_ALT_SCREEN
            if ((p[0] == 1047 || p[0] == 1049) && _sfte.term.alt_active) {
                _sfte.term.alt_active = 0;

                if (_sfte.term.alt_cells) {
                    sfte_cell *tmp = _sfte.term.cells;
                    _sfte.term.cells = _sfte.term.alt_cells;
                    _sfte.term.alt_cells = tmp;
                    _sfte_dirty_range(0, _sfte.term.cols * _sfte.term.rows);
                }
            }
#endif  // SFTE_ALT_SCREEN

            if (p[0] == 1048 || p[0] == 1049) {
                _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.ansi_saved_x, 0, _sfte.term.cols - 1);
                _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.ansi_saved_y, 0, _sfte.term.rows - 1);
                _sfte.term.cur_fg = _sfte.term.ansi_saved_fg;
                _sfte.term.cur_bg = _sfte.term.ansi_saved_bg;
                _sfte.term.cur_attr = _sfte.term.ansi_saved_attr;
                _sfte.term.cells[_SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y)].dirty = 1;
            }
        }
        break;
    }
    case 'm':  // SGR / Select Graphic Rendition
    {
        /*
          Sets colors and style of the characters following this code
         */
        for (int i = 0; i < cnt; ++i) {
            if (p[i] == 0) {
                _sfte.term.cur_fg = 0xFFFFFF;
                _sfte.term.cur_bg = SFTE_BG_COLOR;
                _sfte.term.cur_attr = 0;
            } else if (p[i] == 1)
                _sfte.term.cur_attr |= ATTR_BOLD;
            else if (p[i] == 3)
                _sfte.term.cur_attr |= ATTR_ITALIC;
            else if (p[i] == 4)
                _sfte.term.cur_attr |= ATTR_UNDERLINE;
            else if (p[i] == 7)
                _sfte.term.cur_attr |= ATTR_REVERSE;
            else if (p[i] == 22)
                _sfte.term.cur_attr &= ~ATTR_BOLD;
            else if (p[i] == 23)
                _sfte.term.cur_attr &= ~ATTR_ITALIC;
            else if (p[i] == 24)
                _sfte.term.cur_attr &= ~ATTR_UNDERLINE;
            else if (p[i] == 27)
                _sfte.term.cur_attr &= ~ATTR_REVERSE;
            else if (p[i] >= 30 && p[i] <= 37)
                _sfte.term.cur_fg = _sfte_ansi_palette[p[i] - 30];
            else if (p[i] == 39)  // default fg
                _sfte.term.cur_fg = 0xFFFFFF;
            else if (p[i] >= 40 && p[i] <= 47)
                _sfte.term.cur_bg = _sfte_ansi_palette[p[i] - 40];
            else if (p[i] == 49)  // default bg
                _sfte.term.cur_bg = SFTE_BG_COLOR;
            else if (p[i] == 38 && i + 4 < cnt && p[i + 1] == 2) {  // true fg
#if SFTE_TRUE_COLOR
                _sfte.term.cur_fg = _sfte_parse_truecolor(p, i);
#endif  // SFTE_TRUE_COLOR
                i += 4;
            } else if (p[i] == 48 && i + 4 < cnt && p[i + 1] == 2) {  // true bg
#if SFTE_TRUE_COLOR
                _sfte.term.cur_bg = _sfte_parse_truecolor(p, i);
#endif  // SFTE_TRUE_COLOR
                i += 4;
            }
        }
        break;
    }
    case 'n': {  // DSR / Device Status Report
        /*
          Reports the cursor position (CPR) by transmitting ESC[n;mR,
          where n is the row and m is the column.
          Also reports terminal status (5) with OK (0n).
         */
        if (p[0] == 6) {
            char buf[32];
            int len = snprintf(buf, sizeof(buf), "\033[%d;%dR", _sfte.term.cursor_y + 1,
                               _sfte.term.cursor_x + 1);
            write(_sfte.pty_fd, buf, len);
        } else if (p[0] == 5) {
            const char *reply = "\033[0n";
            write(_sfte.pty_fd, reply, strlen(reply));
        }
        break;
    }
    case 'p':  // DECSTR / Soft Terminal Reset
    {
        /*
          Resets terminal state to default values.
         */
#if SFTE_CURSOR_BLINK
        _sfte.term.blink_enabled = 1;
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_TRAIL
        _sfte.term.last_move_ms = 0;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_CURSOR_DYNAMIC
        _sfte.term.cursor_style = SFTE_CURSOR_STYLE;
#endif  // SFTE_CURSOR_DYNAMIC
        _sfte.term.scroll_top = 0;
        _sfte.term.scroll_bottom = _sfte.term.rows - 1;
        _sfte.term.cur_fg = 0xFFFFFF;
        _sfte.term.cur_bg = SFTE_BG_COLOR;
        _sfte.term.cur_attr = 0;
        _sfte.term.hide_cursor = 0;

        _sfte.term.cells[_SFTE_IDX(cx, _sfte.term.cursor_y)].dirty = 1;
        break;
    }
    case 'q':  // DECSCUSR / Set Cursor Style
    {
        /*
          Changes the cursor shape and blinking style.
         */
        int style = p[0] ? p[0] : 0;
#if SFTE_CURSOR_BLINK
        switch (style) {
        case 0:
        case 1:
        case 3:
        case 5: _sfte.term.blink_enabled = 1; break;
        case 2:
        case 4:
        case 6: _sfte.term.blink_enabled = 0; break;
        }
#endif  // SFTE_CURSOR_BLINK

#if SFTE_CURSOR_DYNAMIC
        switch (style) {
        case 0: _sfte.term.cursor_style = SFTE_CURSOR_STYLE; break;
        case 1:
        case 2: _sfte.term.cursor_style = SFTE_CURSOR_BLOCK; break;
        case 3:
        case 4: _sfte.term.cursor_style = SFTE_CURSOR_UNDERLINE; break;
        case 5:
        case 6: _sfte.term.cursor_style = SFTE_CURSOR_BAR; break;
        }
#endif  // SFTE_CURSOR_DYNAMIC

        _sfte.term.cells[_SFTE_IDX(cx, _sfte.term.cursor_y)].dirty = 1;
        break;
    }
    case 'r':  // DECSTBM / Set Top and Bottom Margins
    {
        /*
          Sets the scrolling region.
          n (default 1) is top margin, m (default 1) is bottom margin.
          Cursor is repositioned dependeing on Origin Mode state.
         */
        int top = (p[0] > 0 ? p[0] : 1) - 1;
        if (top < 0) top = 0;

        int bot = (cnt > 1 && p[1] > 0 ? p[1] : _sfte.term.rows) - 1;
        if (bot >= _sfte.term.rows) bot = _sfte.term.rows - 1;

        if (top < bot) {
            _sfte.term.scroll_top = top;
            _sfte.term.scroll_bottom = bot;
        }

        _sfte.term.cursor_x = 0;
        _sfte.term.cursor_y = _sfte.term.origin_mode ? _sfte.term.scroll_top : 0;
        break;
    }
    case 's':  // SCOSC / Save Cursor
    {
        /*
          Saves the current cursor position and attributes.
         */
        if (p[0] != 0) break;  // avoid kitty support command
        _sfte.term.ansi_saved_x = _sfte.term.cursor_x;
        _sfte.term.ansi_saved_y = _sfte.term.cursor_y;
        _sfte.term.ansi_saved_fg = _sfte.term.cur_fg;
        _sfte.term.ansi_saved_bg = _sfte.term.cur_bg;
        _sfte.term.ansi_saved_attr = _sfte.term.cur_attr;
        break;
    }
    case 't':  // XTWINOPS / Window Manipulation
    {
        /*
          Xterm extension for querying or changing window properties.
          Used here to push/pop window titles.
         */
        int op = p[0];
        if (op == 22) {                  // push title to stack
            if (p[1] == 0 || p[1] == 2)  // p[1] == 0 (icon+title), 1 (icon), 2 (title)
                snprintf(_sfte.term.saved_title, sizeof(_sfte.term.saved_title), "%s",
                         _sfte.term.title);
        } else if (op == 23) {  // pop title from stack
            if (p[1] == 0 || p[1] == 2) {
                snprintf(_sfte.term.title, sizeof(_sfte.term.title), "%s", _sfte.term.saved_title);
                // update window border text
                xdg_toplevel_set_title(_sfte.xdg_toplevel, _sfte.term.title);
            }
        }
        break;
    }
    case 'u':  // SCORC / Restore Cursor
    {
        /*
          Restores the previously saved cursor position and attributes.
         */
        if (p[0] != 0) break;  // avoid kitty support command
        _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.ansi_saved_x, 0, _sfte.term.cols - 1);
        _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.ansi_saved_y, 0, _sfte.term.rows - 1);
        _sfte.term.cur_fg = _sfte.term.ansi_saved_fg;
        _sfte.term.cur_bg = _sfte.term.ansi_saved_bg;
        _sfte.term.cur_attr = _sfte.term.ansi_saved_attr;
        break;
    }
    default: _SFTE_WARN(UNHANDLED_CSI, cmd); break;
    }
}

typedef enum {
    VT_GROUND,     // normal
    VT_ESCAPE,     // \033
    VT_CSI_ENTRY,  // \033[
    VT_CSI_PARAM,  // nums
    VT_OSC,        // \033]
    VT_CHARSET,    // \033( \033)
    VT_HASH,       // #
    VT_DCS         // P / _ / ^
} sfte_vt_state;

static void _sfte_parse_byte(uint8_t b) {
    switch (_sfte.term.vt_state) {
    case VT_GROUND:
        if (b == '\033' || b == '\x1b') {
            _sfte.term.vt_state = VT_ESCAPE;
        } else if (b == '\n') {
            if (_sfte.term.cursor_y == _sfte.term.scroll_bottom)
                _sfte_scroll(1);  // at bot margin, scroll text up
            else if (_sfte.term.cursor_y < _sfte.term.rows - 1)
                _sfte.term.cursor_y++;  // not at bot, move cursor down
        } else if (b == '\r')
            _sfte.term.cursor_x = 0;
        else if (b == '\t') {
            while (_sfte.term.cursor_x < _sfte.term.cols - 1) {
                _sfte.term.cursor_x++;
                if (_sfte.term.tab_stops[_sfte.term.cursor_x]) break;
            }
        } else if ((b == '\b' || b == '\x7f') && _sfte.term.cursor_x > 0)
            _sfte.term.cursor_x--;
        else if (b >= 0x20) {
            if (_sfte.term.utf8_bytes_left > 0) {
                if ((b & 0xC0) == 0x80) {  // continuation byte
                    _sfte.term.utf8_rune = (_sfte.term.utf8_rune << 6) | (b & 0x3F);
                    _sfte.term.utf8_bytes_left--;
                } else
                    _sfte.term.utf8_bytes_left = 0;  // invalid sequence, abort
            } else {                                 // start of a new rune
                if ((b & 0x80) == 0) {
                    _sfte.term.utf8_rune = b;
                    _sfte.term.utf8_bytes_left = 0;
                } else if ((b & 0xE0) == 0xC0) {
                    _sfte.term.utf8_rune = b & 0x1F;
                    _sfte.term.utf8_bytes_left = 1;
                } else if ((b & 0xF0) == 0xE0) {
                    _sfte.term.utf8_rune = b & 0x0F;
                    _sfte.term.utf8_bytes_left = 2;
                } else if ((b & 0xF8) == 0xF0) {
                    _sfte.term.utf8_rune = b & 0x07;
                    _sfte.term.utf8_bytes_left = 3;
                }
            }

            // only write to grid when multibyte sequence is ready
            if (_sfte.term.utf8_bytes_left != 0) break;

            // evaluate line wrapping before drawing
            // ensures chars placed in the final col enter a pending wrap state
            // instead of immediately dropping to the next line
            _sfte_check_wrap();
            int idx = _SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y);
            _sfte.term.cells[idx].rune = _sfte.term.utf8_rune;
            _sfte.term.cells[idx].fg = _sfte.term.cur_fg;
            _sfte.term.cells[idx].bg = _sfte.term.cur_bg;
            _sfte.term.cells[idx].attr = _sfte.term.cur_attr;
            _sfte.term.cells[idx].dirty = 1;
            _sfte.term.cursor_x++;
        }
        break;
    case VT_ESCAPE:
        if (b == '[') {
            _sfte.term.vt_state = VT_CSI_ENTRY;
            _sfte.term.vt_param_idx = 0;
            _sfte.term.vt_dec_priv = 0;

            memset(_sfte.term.vt_params, 0, sizeof(_sfte.term.vt_params));
        } else if (b == ']') {
            _sfte.term.vt_state = VT_OSC;
            _sfte.term.osc_idx = 0;

            memset(_sfte.term.osc_payload, 0, sizeof(_sfte.term.osc_payload));
        } else if (b == '\\')
            _sfte.term.vt_state = VT_GROUND;
        else if (b == 'P' || b == '_' || b == '^') {
            _sfte.term.vt_state = VT_DCS;
            _sfte.term.osc_idx = 0;
            memset(_sfte.term.osc_payload, 0, sizeof(_sfte.term.osc_payload));
        } else if (b == '(' || b == ')')
            _sfte.term.vt_state = VT_CHARSET;
        else if (b == '7') {  // save cursor
            _sfte.term.ansi_saved_x = _sfte.term.cursor_x;
            _sfte.term.ansi_saved_y = _sfte.term.cursor_y;
            _sfte.term.ansi_saved_fg = _sfte.term.cur_fg;
            _sfte.term.ansi_saved_bg = _sfte.term.cur_bg;
            _sfte.term.ansi_saved_attr = _sfte.term.cur_attr;
            _sfte.term.vt_state = VT_GROUND;
        } else if (b == '8') {  // restore cursor
            _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.ansi_saved_x, 0, _sfte.term.cols - 1);
            _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.ansi_saved_y, 0, _sfte.term.rows - 1);
            _sfte.term.cur_fg = _sfte.term.ansi_saved_fg;
            _sfte.term.cur_bg = _sfte.term.ansi_saved_bg;
            _sfte.term.cur_attr = _sfte.term.ansi_saved_attr;
            _sfte.term.vt_state = VT_GROUND;
        } else if (b == '#')
            _sfte.term.vt_state = VT_HASH;
        else if (b == 'D') {  // index move down
            if (_sfte.term.cursor_y == _sfte.term.scroll_bottom)
                _sfte_scroll(1);
            else if (_sfte.term.cursor_y < _sfte.term.rows - 1)
                _sfte.term.cursor_y++;
            _sfte.term.vt_state = VT_GROUND;
        } else if (b == 'M') {  // reverse index move up
            if (_sfte.term.cursor_y == _sfte.term.scroll_top)
                _sfte_scroll(-1);
            else if (_sfte.term.cursor_y > 0)
                _sfte.term.cursor_y--;
            _sfte.term.vt_state = VT_GROUND;
        } else if (b == 'E') {  // next line
            if (_sfte.term.cursor_y == _sfte.term.scroll_bottom)
                _sfte_scroll(1);
            else if (_sfte.term.cursor_y < _sfte.term.rows - 1)
                _sfte.term.cursor_y++;
            _sfte.term.cursor_x = 0;
            _sfte.term.vt_state = VT_GROUND;
        } else
            _sfte.term.vt_state = VT_GROUND;
        break;
    case VT_HASH:
        if (b == '8') {  // ESC # 8 / DECALN
            for (int i = 0; i < _sfte.term.cols * _sfte.term.rows; ++i) {
                _sfte.term.cells[i].rune = 'E';
                _sfte.term.cells[i].fg = 0xFFFFFF;
                _sfte.term.cells[i].bg = SFTE_BG_COLOR;
                _sfte.term.cells[i].attr = 0;
                _sfte.term.cells[i].dirty = 1;
            }
            _sfte.term.cursor_x = 0;
            _sfte.term.cursor_y = 0;
        }
        _sfte.term.vt_state = VT_GROUND;
        break;
    case VT_CHARSET:  // absorb charset specifier
        _sfte.term.vt_state = VT_GROUND;
        break;
    case VT_OSC:
        if (b == '\x07' || b == '\x1b') {
            const char *term = (b == '\x1b') ? "\033\\" : "\x07";

            if (strncmp(_sfte.term.osc_payload, "10;?", 4) == 0 ||
                strncmp(_sfte.term.osc_payload, "11;?", 4) == 0) {
                int is_bg = _sfte.term.osc_payload[1] == '1';

                uint32_t color = is_bg ? SFTE_BG_COLOR : 0xFFFFFF;

                uint8_t cr = (color >> 16) & 0xFF;
                uint8_t cg = (color >> 8) & 0xFF;
                uint8_t cb = color & 0xFF;

                char reply[64];
                int len = snprintf(reply, sizeof(reply), "\033]%d;rgb:%02x%02x/%02x%02x/%02x%02x%s",
                                   is_bg ? 11 : 10, cr, cr, cg, cg, cb, cb, term);
                write(_sfte.pty_fd, reply, len);
            } else
                _SFTE_WARN(UNHANDLED_OSC, _sfte.term.osc_payload);

            if (b == '\x1b')
                _sfte.term.vt_state = VT_ESCAPE;
            else
                _sfte.term.vt_state = VT_GROUND;

        } else if (_sfte.term.osc_idx < (int)sizeof(_sfte.term.osc_payload) - 1)
            _sfte.term.osc_payload[_sfte.term.osc_idx++] = b;
        break;
    case VT_DCS:
        if (b == '\x07' || b == '\x1b') {
            const char *term = (b == '\x1b') ? "\033\\" : "\x07";

            if (strncmp(_sfte.term.osc_payload, "+q", 2) == 0) {
                char reply[128];
                int len = snprintf(reply, sizeof(reply), "\033P0+r%s%s", _sfte.term.osc_payload + 2,
                                   term);
                write(_sfte.pty_fd, reply, len);
            }

            if (b == '\x1b')
                _sfte.term.vt_state = VT_ESCAPE;
            else if (b == '\x07')
                _sfte.term.vt_state = VT_GROUND;

        } else if (_sfte.term.osc_idx < (int)sizeof(_sfte.term.osc_payload) - 1)
            _sfte.term.osc_payload[_sfte.term.osc_idx++] = b;
        break;
    case VT_CSI_ENTRY:
    case VT_CSI_PARAM:
        if (b == '?') {  // private marker
            _sfte.term.vt_state = VT_CSI_PARAM;
            _sfte.term.vt_dec_priv = 1;
        } else if (b == '>') {
            _sfte.term.vt_state = VT_CSI_PARAM;
            _sfte.term.vt_dec_priv = 2;
        } else if (b >= '0' && b <= '9') {
            _sfte.term.vt_state = VT_CSI_PARAM;

            _sfte.term.vt_params[_sfte.term.vt_param_idx] *= 10;
            _sfte.term.vt_params[_sfte.term.vt_param_idx] += (b - '0');
        } else if (b == ';')
            _sfte.term.vt_param_idx++;  // move to next param
        else if (b >= 0x40 && b <= 0x7E) {
            _sfte_dispatch_csi(b);
            _sfte.term.vt_state = VT_GROUND;
        }
    }
}

// >>loop
static void _sfte_loop(void) {
    signal(SIGPIPE, SIG_IGN);

    _sfte.repeat_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    int wl_fd = wl_display_get_fd(_sfte.display);

    while (_sfte.running) {
        wl_display_dispatch_pending(_sfte.display);
        wl_display_flush(_sfte.display);
        struct pollfd fds[] = {{.fd = wl_fd, .events = POLLIN},
                               {.fd = _sfte.pty_fd, .events = POLLIN},
                               {.fd = _sfte.repeat_timer_fd, .events = POLLIN}};
        int timeout = -1;
        int needs_render = 0;

#if SFTE_CURSOR_TRAIL
        if (_sfte.term.is_trailing)
            if (timeout == -1 || timeout > 16) timeout = 16;
#endif  // SFTE_CURSOR_TRAIL

#if SFTE_CURSOR_BLINK
        uint64_t now = _sfte_time_ms();
        if (_sfte.term.blink_enabled) {
            int time_to_next = (int)(_sfte.term.next_blink_ms - now);
            if (time_to_next < 0) time_to_next = 0;

            if (timeout == -1 || time_to_next < timeout) timeout = time_to_next;
        }
#endif  // SFTE_CURSOR_BLINK

        if (poll(fds, _SFTE_ARRAY_LEN(fds), timeout /* default infinite timeout */) == -1) break;

#if SFTE_CURSOR_BLINK
        if (_sfte.term.blink_enabled) {
            now = _sfte_time_ms();
            if (now >= _sfte.term.next_blink_ms) {
                _sfte.term.blink_visible = !_sfte.term.blink_visible;
                _sfte.term.next_blink_ms = now + SFTE_CURSOR_BLINK_RATE;
                int vis_cx = _sfte.term.cursor_x >= _sfte.term.cols ? _sfte.term.cols - 1
                                                                    : _sfte.term.cursor_x;
                _sfte.term.cells[_SFTE_IDX(vis_cx, _sfte.term.cursor_y)].dirty = 1;
                needs_render = 1;
            }
        }
#endif  // SFTE_CURSOR_BLINK

        if (fds[0].revents & (POLLIN | POLLERR | POLLHUP))
            if (wl_display_dispatch(_sfte.display) == -1) _sfte.running = 0;

        if (fds[1].revents & (POLLIN | POLLERR | POLLHUP)) {
            uint8_t buf[SFTE_PTY_BUF_SIZE];
            ssize_t n = read(_sfte.pty_fd, buf, SFTE_PTY_BUF_SIZE);

            if (n > 0) {
#if SFTE_SCROLLBACK_CAP
                if (_sfte.term.sb_offset > 0) {
                    _sfte.term.sb_offset = 0;
                    _sfte_dirty_range(0, _sfte.term.cols * _sfte.term.rows);
                    needs_render = 1;
                }
#endif  // SFTE_SCROLLBACK_CAP

#if SFTE_CURSOR_BLINK
                _sfte.term.blink_visible = 1;
                _sfte.term.next_blink_ms = _sfte_time_ms() + SFTE_CURSOR_BLINK_RATE;
#endif  // SFTE_CURSOR_BLINK

                for (ssize_t i = 0; i < n; ++i) _sfte_parse_byte(buf[i]);

#if SFTE_CURSOR_TRAIL
                int vis_cx = _sfte.term.cursor_x >= _sfte.term.cols ? _sfte.term.cols - 1
                                                                    : _sfte.term.cursor_x;
                float target_rx = vis_cx * _sfte.font.cell_width;
                float target_ry = _sfte.term.cursor_y * _sfte.font.cell_height;

                if (vis_cx != _sfte.term.last_grid_x ||
                    _sfte.term.cursor_y != _sfte.term.last_grid_y) {
                    now = _sfte_time_ms();

                    if (_sfte.term.last_move_ms != 0 &&
                        (now - _sfte.term.last_move_ms >= SFTE_CURSOR_TRAIL)) {
                        _sfte.term.is_trailing = 1;
                    } else if (!_sfte.term.is_trailing) {
                        _sfte.term.tail_rx = target_rx;
                        _sfte.term.tail_ry = target_ry;
                    }

                    _sfte.term.last_grid_x = vis_cx;
                    _sfte.term.last_grid_y = _sfte.term.cursor_y;
                    _sfte.term.last_move_ms = now;
                }
#endif  // SFTE_CURSOR_TRAIL

                needs_render = 1;
            } else
                _sfte.running = 0;
        }

        if (fds[2].revents & POLLIN) {
            uint64_t expirations;
            if (read(_sfte.repeat_timer_fd, &expirations, sizeof(expirations)) == 0 ||
                _sfte.repeating_key == 0)
                continue;
            // simulate a key press to get autorepeat
            _sfte_wayland_keyboard_key(NULL, _sfte.keyboard, 0, 0, _sfte.repeating_key,
                                       WL_KEYBOARD_KEY_STATE_PRESSED);
        }

#if SFTE_CURSOR_TRAIL
        if (_sfte.term.is_trailing) {
            int vis_cx = _sfte.term.cursor_x >= _sfte.term.cols ? _sfte.term.cols - 1
                                                                : _sfte.term.cursor_x;
            float target_rx = vis_cx * _sfte.font.cell_width;
            float target_ry = _sfte.term.cursor_y * _sfte.font.cell_height;

            now = _sfte_time_ms();
            if (_sfte.term.last_trail_update_ms == 0) _sfte.term.last_trail_update_ms = now;
            float dt_ms = (float)(now - _sfte.term.last_trail_update_ms);
            _sfte.term.last_trail_update_ms = now;

            float tx = target_rx - _sfte.term.tail_rx;
            float ty = target_ry - _sfte.term.tail_ry;

            if (tx * tx + ty * ty <= 0.5f) {
                _sfte.term.is_trailing = 0;
                _sfte.term.tail_rx = target_rx;
                _sfte.term.tail_ry = target_ry;
                _sfte.term.last_trail_update_ms = 0;
            } else {
                float decay = dt_ms * SFTE_CURSOR_TRAIL_DECAY;
                if (decay > 1.0f) decay = 1.0f;

                _sfte.term.tail_rx += tx * decay;
                _sfte.term.tail_ry += ty * decay;
            }
            needs_render = 1;
        }
#endif  // SFTE_CURSOR_TRAIL

        if (needs_render) _sfte_wayland_render();
    }
}

// >>api
int sfte_run(void) {
    _sfte_state_load();
    _sfte_font_load();
    _sfte_wayland_load();
    _sfte_pty_spawn();
    _sfte_loop();
    _sfte_wayland_unload();
    return 0;
}

#endif  // SFTE_IMPL
