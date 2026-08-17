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

#ifndef SFTE_FONT_PATH
#define SFTE_FONT_PATH "/usr/share/fonts/TTF/DejaVuSansMono.ttf"
#endif  // SFTE_FONT_PATH

#ifndef SFTE_DEFAULT_FONT_SIZE
#define SFTE_DEFAULT_FONT_SIZE 12.0f
#endif  // SFTE_DEFAULT_FONT_SIZE

#ifndef SFTE_ANSI_PALETTE
#define SFTE_ANSI_PALETTE                                                                          \
    {0x181818, 0xCC241D, 0x98971A, 0xD79921, 0x458588, 0xB16286, 0x689D6A, 0xA89984,               \
     0x928374, 0xFB4934, 0xB8BB26, 0xFABD2F, 0x83A598, 0xD3869B, 0x8EC07C, 0xEBDBB2}
#endif  // SFTE_ANSI_PALETTE

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

#ifndef SFTE_CURSOR_COLOR  // RGB
#define SFTE_CURSOR_COLOR 0xFFFFFF
#endif  // SFTE_CURSOR_COLOR

#ifndef SFTE_CURSOR_BLINK  // 0/1
#define SFTE_CURSOR_BLINK 1
#endif  // SFTE_CURSOR_BLINK

#ifndef SFTE_CURSOR_BLINK_RATE  // in ms
#define SFTE_CURSOR_BLINK_RATE 500
#endif  // SFTE_CURSOR_BLINK_RATE

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

#ifndef SFTE_SHORTCUTS
#define SFTE_SHORTCUTS                                                                             \
    {                                                                                              \
        {SFTE_MOD_CTRL, XKB_KEY_equal, _sfte_font_resize, {.f = 2.0f}},                            \
        {SFTE_MOD_CTRL, XKB_KEY_plus, _sfte_font_resize, {.f = 2.0f}},                             \
        {SFTE_MOD_CTRL, XKB_KEY_minus, _sfte_font_resize, {.f = -2.0f}},                           \
        {SFTE_MOD_CTRL, XKB_KEY_0, _sfte_font_reset, {.v = NULL}},                                 \
    }
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
typedef struct sfte_logger {
    void (*func)(const char *tag,              // always "sfte"
                 uint32_t log_level,           // 0=panic, 1=error, 2=warning, 3=info
                 const char *message_or_null,  // a message string, may be nullptr in release mode
                 uint32_t line_nr              // line number in sfte.h
    );
} sfte_logger;

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
} sfte_cell;

typedef struct {
    uint32_t rune;
    int x0, y0, x1, y1;  // atlas tex coords
    int xoff, yoff;      // render offsets
    int xadvance;
} sfte_glyph;

typedef struct {
    sfte_cell *cells;
    sfte_cell *alt_cells;
    int cols;
    int rows;
    char title[256];
    char saved_title[256];
    uint8_t auto_wrap;
    uint8_t origin_mode;
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
    int cursor_x;
    int cursor_y;
    uint8_t hide_cursor;
    uint8_t cursor_style;  // block/underline/bar (lsb)
    // alt screen state
    int alt_active;  // tracks if in alt buffer

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
    float cur_size;  // starts at SFTE_DEFAULT_FONT_SIZE

    uint8_t *atlas_pxs;
    int atlas_width;
    int atlas_height;

    stbtt_fontinfo stb_info;
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
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_buffer *buffer;
    uint32_t *shm_data;
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

    sfte_logger logger;
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

// >>font
static sfte_glyph *_sfte_font_get_glyph(uint32_t rune) {
    if (rune == 0) rune = ' ';
    uint32_t h = rune % _sfte.font.glyph_cap;

    // hash map logic
    for (int i = 0; i < _sfte.font.glyph_cap; ++i) {
        int idx = (h + i) % _sfte.font.glyph_cap;

        if (_sfte.font.glyphs[idx].rune == rune) return &_sfte.font.glyphs[idx];  // cache hit

        if (_sfte.font.glyphs[idx].rune != 0) continue;  // cache miss, taken, continue

        // cache miss, free, take space
        sfte_glyph *g = &_sfte.font.glyphs[idx];
        g->rune = rune;

        int advance_width, left_side_bearing;
        stbtt_GetCodepointHMetrics(&_sfte.font.stb_info, rune, &advance_width, &left_side_bearing);
        g->xadvance = (int)(advance_width * _sfte.font.scale + 0.5f);

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&_sfte.font.stb_info, rune, _sfte.font.scale, _sfte.font.scale,
                                    &x0, &y0, &x1, &y1);

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
            stbtt_MakeCodepointBitmap(&_sfte.font.stb_info, &_sfte.font.atlas_pxs[byte_off],
                                      glyph_width, glyph_height, _sfte.font.atlas_width,
                                      _sfte.font.scale, _sfte.font.scale, rune);
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
    sfte_glyph *m = _sfte_font_get_glyph('M');
    _sfte.font.cell_width = m->xadvance;
    _sfte.font.cell_height = (int)(_sfte.font.cur_size * 1.2f + 0.5f);
}

static void _sfte_font_load(void) {
    _sfte.font.cur_size = SFTE_DEFAULT_FONT_SIZE;
    _sfte.font.atlas_width = 1024;
    _sfte.font.atlas_height = 1024;

    _sfte.font.atlas_pxs = (uint8_t *)malloc(_sfte.font.atlas_width * _sfte.font.atlas_height);
    SFTE_ASSERT(_sfte.font.atlas_pxs, "failed to allocate font atlas");

    FILE *f = fopen(SFTE_FONT_PATH, "rb");
    SFTE_ASSERT(f, "failed to open font file");
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    _sfte.font.ttf_buf = (uint8_t *)malloc(size);
    SFTE_ASSERT(fread(_sfte.font.ttf_buf, 1, size, f) == size, "failed to read font file");

    fclose(f);

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
    const sfte_arg arg = {.f = SFTE_DEFAULT_FONT_SIZE - _sfte.font.cur_size};
    _sfte_font_resize(&arg);
}

static const sfte_shortcut _sfte_shortcuts[] = SFTE_SHORTCUTS;

// >>render
static void _sfte_render_bg(int col, int row, uint32_t bg) {
    int cx = col * _sfte.font.cell_width + SFTE_PAD_X;
    int cy = row * _sfte.font.cell_height + SFTE_PAD_Y;
    uint32_t final_bg = (bg & 0x00FFFFFF) | (SFTE_BG_OPACITY << 24);

    for (int y = 0; y < _sfte.font.cell_height; ++y) {
        for (int x = 0; x < _sfte.font.cell_width; ++x) {
            int px_idx = (cy + y) * _sfte.width + (cx + x);
            if (px_idx < _sfte.width * _sfte.height) _sfte.shm_data[px_idx] = final_bg;
        }
    }
}

static void _sfte_render_fg(int col, int row, uint32_t rune, uint32_t fg) {
    if (rune == ' ') return;

    sfte_glyph *g = _sfte_font_get_glyph(rune);
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
                _sfte.shm_data[px_idx] = (0xFF << 24) | (fg & 0x00FFFFFF);
            else {
                uint32_t dst = _sfte.shm_data[px_idx];
                uint8_t bg_r = (dst >> 16) & 0xFF;
                uint8_t bg_g = (dst >> 8) & 0xFF;
                uint8_t bg_b = dst & 0xFF;

                uint8_t col_r = (fg_r * alpha + bg_r * (255 - alpha)) >> 8;
                uint8_t col_g = (fg_g * alpha + bg_g * (255 - alpha)) >> 8;
                uint8_t col_b = (fg_b * alpha + bg_b * (255 - alpha)) >> 8;

                _sfte.shm_data[px_idx] = (SFTE_BG_OPACITY << 24) | (col_r << 16) | (col_g << 8) |
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

    struct wl_shm_pool *pool = wl_shm_create_pool(_sfte.shm, fd, _sfte.shm_size);
    _sfte.buffer = wl_shm_pool_create_buffer(pool, 0, _sfte.width, _sfte.height, stride,
                                             WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);

    close(fd);
}

static void _sfte_term_resize(int new_cols, int new_rows);

static void _sfte_wayland_render(void) {
    int new_cols = (_sfte.width - (2 * SFTE_PAD_X)) / _sfte.font.cell_width;
    if (new_cols < 1) new_cols = 1;
    int new_rows = (_sfte.height - (2 * SFTE_PAD_Y)) / _sfte.font.cell_height;
    if (new_rows < 1) new_rows = 1;

    // if compositor OR font scaling changed physical dims,
    // reallocate the grid before attempting to draw
    if (new_cols != _sfte.term.cols || new_rows != _sfte.term.rows)
        _sfte_term_resize(new_cols, new_rows);

    // if cursor moved, dirtyy the old cell to erase it, and dirty the new cell to draw it
    if (_sfte.term.dirty_saved_x != _sfte.term.cursor_x ||
        _sfte.term.dirty_saved_y != _sfte.term.cursor_y) {
        _sfte.term.cells[_SFTE_IDX(_sfte.term.dirty_saved_x, _sfte.term.dirty_saved_y)].dirty = 1;
        _sfte.term.cells[_SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y)].dirty = 1;
        _sfte.term.dirty_saved_x = _sfte.term.cursor_x;
        _sfte.term.dirty_saved_y = _sfte.term.cursor_y;
    }

    int vis_cx = _sfte.term.cursor_x >= _sfte.term.cols ? _sfte.term.cols - 1 : _sfte.term.cursor_x;

    for (int r = 0; r < _sfte.term.rows; ++r) {
        for (int c = 0; c < _sfte.term.cols; ++c) {
            int idx = _SFTE_IDX(c, r);

            int is_dirty = _sfte.term.cells[idx].dirty;
            if (!is_dirty && c < _sfte.term.cols - 1 && _sfte.term.cells[idx + 1].dirty)
                is_dirty = 1;
            if (!is_dirty) continue;

            uint32_t fg = _sfte.term.cells[idx].fg ? _sfte.term.cells[idx].fg : 0xFFFFFF;
            uint32_t bg = _sfte.term.cells[idx].bg ? _sfte.term.cells[idx].bg : SFTE_BG_COLOR;

            int is_cursor = (c == vis_cx && r == _sfte.term.cursor_y && !_sfte.term.hide_cursor);

#if SFTE_CURSOR_BLINK
            if (!_sfte.term.blink_visible) is_cursor = 0;
#endif  // SFTE_CURSOR_BLINK

            if (is_cursor && _sfte.term.cursor_style == SFTE_CURSOR_BLOCK)
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

            uint32_t rune = _sfte.term.cells[idx].rune;
            if (rune == 0) rune = ' ';

            uint32_t fg = _sfte.term.cells[idx].fg ? _sfte.term.cells[idx].fg : 0xFFFFFF;
            uint32_t bg = _sfte.term.cells[idx].bg ? _sfte.term.cells[idx].bg : SFTE_BG_COLOR;

            int is_cursor = (c == vis_cx && r == _sfte.term.cursor_y && !_sfte.term.hide_cursor);

#if SFTE_CURSOR_BLINK
            if (!_sfte.term.blink_visible) is_cursor = 0;
#endif  // SFTE_CURSOR_BLINK

            if (is_cursor && _sfte.term.cursor_style == SFTE_CURSOR_BLOCK)
                _sfte_render_fg(c, r, rune, bg);  // inverse if under cursor block
            else {
                _sfte_render_fg(c, r, rune, fg);

                if (is_cursor && _sfte.term.cursor_style != SFTE_CURSOR_BLOCK) {  // bar/underline
                    int cx = c * _sfte.font.cell_width + SFTE_PAD_X;
                    int cy = r * _sfte.font.cell_height + SFTE_PAD_Y;

                    uint32_t cur_col = (SFTE_CURSOR_COLOR & 0x00FFFFFF) | (0xFF << 24);

                    if (_sfte.term.cursor_style == SFTE_CURSOR_UNDERLINE) {
                        int thickness = _sfte.font.cell_height / 10;
                        if (thickness < 1) thickness = 1;

                        for (int y = cy + _sfte.font.cell_height - thickness;
                             y < cy + _sfte.font.cell_height; ++y)
                            for (int x = cx; x < cx + _sfte.font.cell_width; ++x)
                                if (x < _sfte.width && y < _sfte.height)
                                    _sfte.shm_data[y * _sfte.width + x] = cur_col;
                    } else if (_sfte.term.cursor_style == SFTE_CURSOR_BAR) {
                        int thickness = _sfte.font.cell_width / 10;
                        if (thickness < 1) thickness = 1;

                        for (int y = cy; y < cy + _sfte.font.cell_height; ++y)
                            for (int x = cx; x < cx + thickness; ++x)
                                if (x < _sfte.width && y < _sfte.height)
                                    _sfte.shm_data[y * _sfte.width + x] = cur_col;
                    }
                }
            }

            // submit localized damage to compositor
            wl_surface_damage_buffer(_sfte.surface, c * _sfte.font.cell_width + SFTE_PAD_X,
                                     r * _sfte.font.cell_height + SFTE_PAD_Y, _sfte.font.cell_width,
                                     _sfte.font.cell_height);
            _sfte.term.cells[idx].dirty = 0;
        }
    }

    wl_surface_attach(_sfte.surface, _sfte.buffer, 0, 0);
    wl_surface_commit(_sfte.surface);
}

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
    if (state == WL_KEYBOARD_KEY_STATE_RELEASED && key == _sfte.repeating_key) {
        struct itimerspec its = {0};
        timerfd_settime(_sfte.repeat_timer_fd, 0, &its, NULL);
        _sfte.repeating_key = 0;
        return;
    }

    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !_sfte.xkb_state) return;

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

    if (size > 0) write(_sfte.pty_fd, buf, size);

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
    free(_sfte.font.ttf_buf);
    free(_sfte.font.atlas_pxs);
    free(_sfte.term.cells);
    free(_sfte.term.alt_cells);

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
    _sfte.logger.func = SFTE_LOGGER_FUNC;
    _sfte.term.cols = 80;
    _sfte.term.rows = 24;
    _sfte.term.auto_wrap = 1;
    _sfte.term.origin_mode = 0;
#if SFTE_CURSOR_BLINK
    _sfte.term.blink_enabled = 1;
    _sfte.term.blink_visible = 1;
    _sfte.term.next_blink_ms = _sfte_time_ms() + SFTE_CURSOR_BLINK_RATE;
#endif  // SFTE_CURSOR_BLINK
    _sfte.term.cursor_style = SFTE_CURSOR_STYLE;
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
        .ws_row = (uint8_t)_sfte.term.rows,
        .ws_col = (uint8_t)_sfte.term.cols,
        .ws_xpixel = (uint8_t)_sfte.width,
        .ws_ypixel = (uint8_t)_sfte.height,
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

static void _sfte_term_resize(int new_cols, int new_rows) {
    sfte_cell *new_cells = (sfte_cell *)calloc(new_cols * new_rows, sizeof(sfte_cell));
    SFTE_ASSERT(new_cells, "failed to allocate resized terminal grid");

    sfte_cell *new_alt_cells = NULL;
    if (_sfte.term.alt_cells) {
        new_alt_cells = (sfte_cell *)calloc(new_cols * new_rows, sizeof(sfte_cell));
        SFTE_ASSERT(new_alt_cells, "failed to allocate resized alt grid");
    }

    int min_cols = new_cols < _sfte.term.cols ? new_cols : _sfte.term.cols;
    int min_rows = new_rows < _sfte.term.rows ? new_rows : _sfte.term.rows;

    for (int r = 0; r < min_rows; ++r)
        for (int c = 0; c < min_cols; ++c) {
            new_cells[r * new_cols + c] = _sfte.term.cells[r * _sfte.term.cols + c];
            if (new_alt_cells)
                new_alt_cells[r * new_cols + c] = _sfte.term.alt_cells[r * _sfte.term.cols + c];
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

    struct winsize ws = {.ws_row = (uint8_t)new_rows,
                         .ws_col = (uint8_t)new_cols,
                         .ws_xpixel = (uint8_t)_sfte.width,
                         .ws_ypixel = (uint8_t)_sfte.height};

    ioctl(_sfte.pty_fd, TIOCSWINSZ, &ws);

    uint32_t clear_col = (SFTE_BG_COLOR & 0x00FFFFFF) | (SFTE_BG_OPACITY << 24);
    for (int i = 0; i < _sfte.width * _sfte.height; ++i) _sfte.shm_data[i] = clear_col;

    _SFTE_INFO(TERM_RESIZE, new_cols, new_rows);
}

static inline void _sfte_clear_cells(int start_idx, int cnt) {
    for (int i = 0; i < cnt; ++i) {
        _sfte.term.cells[start_idx + i].rune = ' ';
        _sfte.term.cells[start_idx + i].fg = _sfte.term.cur_fg;
        _sfte.term.cells[start_idx + i].bg = _sfte.term.cur_bg;
        _sfte.term.cells[start_idx + i].attr = 0;
        _sfte.term.cells[start_idx + i].dirty = 1;
    }
}

static void _sfte_scroll(int lines) {
    int top = _sfte.term.scroll_top;
    int bot = _sfte.term.scroll_bottom;
    int height = bot - top + 1;
    int cols = _sfte.term.cols;

    if (lines > 0) {  // scroll up
        if (lines > height) lines = height;

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
            _sfte.term.cursor_x = 0;
            if (_sfte.term.cursor_y == _sfte.term.scroll_bottom)
                _sfte_scroll(1);
            else if (_sfte.term.cursor_y < _sfte.term.rows - 1)
                _sfte.term.cursor_y++;
        } else
            _sfte.term.cursor_x = _sfte.term.cols - 1;
    }
}

static inline uint32_t _sfte_parse_truecolor(int *p, int i) {
    return (p[i + 2] << 16) | (p[i + 3] << 8) | p[i + 4];
}

static void _sfte_dispatch_csi(uint8_t cmd) {
    int *p = _sfte.term.vt_params;
    int cnt = _sfte.term.vt_param_idx + 1;

    if (_sfte.term.cursor_x >= _sfte.term.cols) _sfte.term.cursor_x = _sfte.term.cols - 1;

    switch (cmd) {
    case 'm':
        if (cnt == 1 && p[0] == 0) {
            _sfte.term.cur_fg = 0xFFFFFF;
            _sfte.term.cur_bg = SFTE_BG_COLOR;
            _sfte.term.cur_attr = 0;
            break;
        }

        for (int i = 0; i < cnt; ++i) {
            if (p[i] == 0) {
                _sfte.term.cur_fg = 0xFFFFFF;
                _sfte.term.cur_bg = SFTE_BG_COLOR;
                _sfte.term.cur_attr = 0;
            } else if (p[i] == 1)
                _sfte.term.cur_attr |= ATTR_BOLD;
            else if (p[i] >= 30 && p[i] <= 37)
                _sfte.term.cur_fg = _sfte_ansi_palette[p[i] - 30];
            else if (p[i] == 39)  // default fg
                _sfte.term.cur_fg = 0xFFFFFF;
            else if (p[i] >= 40 && p[i] <= 47)
                _sfte.term.cur_bg = _sfte_ansi_palette[p[i] - 40];
            else if (p[i] == 49)  // default bg
                _sfte.term.cur_bg = SFTE_BG_COLOR;
            else if (p[i] == 38 && i + 4 < cnt && p[i + 1] == 2) {  // true fg
                _sfte.term.cur_fg = _sfte_parse_truecolor(p, i);
                i += 4;
            } else if (p[i] == 48 && i + 4 < cnt && p[i + 1] == 2) {  // true bg
                _sfte.term.cur_bg = _sfte_parse_truecolor(p, i);
                i += 4;
            }
        }
        break;
    case 'H':  // cursor position
    case 'f': {
        // NOTE: vt coords are 1-idxd
        int r = (p[0] > 0 ? p[0] : 1) - 1;
        int c = (cnt > 1 && p[1] > 0 ? p[1] : 1) - 1;
        c = _SFTE_CLAMP(c, 0, _sfte.term.cols - 1);

        if (_sfte.term.origin_mode) {  // relative bounds
            r += _sfte.term.scroll_top;
            r = _SFTE_CLAMP(r, _sfte.term.scroll_top, _sfte.term.scroll_bottom);
        } else
            r = _SFTE_CLAMP(r, 0, _sfte.term.rows - 1);

        _sfte.term.cursor_y = r;
        _sfte.term.cursor_x = c;

        break;
    }
    case 'J':  // clear screen
    {
        int p0 = p[0];
        if (p[0] == 0) {  // 0J / cursor to end of screen
            int start_idx = _SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y);
            _sfte_clear_cells(start_idx, (_sfte.term.rows * _sfte.term.cols) - start_idx);
        } else if (p[0] == 1)  // 1J / start of screen to cursor
            _sfte_clear_cells(0, _SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y) + 1);
        else if (p[0] == 2) {  // 2J / entire screen
            _sfte_clear_cells(0, _sfte.term.rows * _sfte.term.cols);
            _sfte.term.cursor_x = 0;
            _sfte.term.cursor_y = 0;
        }
        break;
    }
    case 'K':  // erase in line
    {
        if (p[0] == 0)  // 0K / clear to eol
            _sfte_clear_cells(_SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y),
                              _sfte.term.cols - _sfte.term.cursor_x);
        if (p[0] == 1)  // clear to start of line
            _sfte_clear_cells(_SFTE_IDX(0, _sfte.term.cursor_y), _sfte.term.cursor_x + 1);
        else if (p[0] == 2)  // 2K / clear entire line
            _sfte_clear_cells(_SFTE_IDX(0, _sfte.term.cursor_y), _sfte.term.cols);
        break;
    }
    case 'n': {  // device status report
        if (p[0] != 6) break;
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "\033[%d;%dR", _sfte.term.cursor_y + 1,
                           _sfte.term.cursor_x + 1);
        write(_sfte.pty_fd, buf, len);
        break;
    }
    case 'A':  // cursor up
    {
        _sfte.term.cursor_y -= (p[0] > 0 ? p[0] : 1);
        _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_y, 0, _sfte.term.rows - 1);
        break;
    }
    case 'B':  // cursor down
    {
        _sfte.term.cursor_y += (p[0] > 0 ? p[0] : 1);
        _sfte.term.cursor_y = _SFTE_CLAMP(_sfte.term.cursor_y, 0, _sfte.term.rows - 1);
        break;
    }
    case 'C':  // cursor forward
    {
        _sfte.term.cursor_x += (p[0] > 0 ? p[0] : 1);
        _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.cursor_x, 0, _sfte.term.cols - 1);
        break;
    }
    case 'D':  // cursor backward
    {
        _sfte.term.cursor_x -= (p[0] > 0 ? p[0] : 1);
        _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.cursor_x, 0, _sfte.term.cols - 1);
        break;
    }
    case 'G':  // cursor horizontal abs
    {
        _sfte.term.cursor_x = (p[0] > 0 ? p[0] : 1) - 1;
        _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.cursor_x, 0, _sfte.term.cols - 1);
        break;
    }
    case 'h':  // set mode
    {
        if (!_sfte.term.vt_dec_priv) break;
        if (p[0] == 25) {
            _sfte.term.hide_cursor = 0;  // ?25h / show cursor
            _sfte.term.cells[_SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y)].dirty = 1;
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

            // 1047 / 1049 switch to alt screen
            if ((p[0] == 1047 || p[0] == 1049) && !_sfte.term.alt_active) {
                _sfte.term.alt_active = 1;

                if (!_sfte.term.alt_cells)
                    _sfte.term.alt_cells = (sfte_cell *)calloc(_sfte.term.cols * _sfte.term.rows,
                                                               sizeof(sfte_cell));

                sfte_cell *tmp = _sfte.term.cells;  // swap buffer ptrs
                _sfte.term.cells = _sfte.term.alt_cells;
                _sfte.term.alt_cells = tmp;
            }

            // 1049 clear screen
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
    case 'l':  // reset mode
    {
        if (!_sfte.term.vt_dec_priv) break;
        if (p[0] == 25) {
            _sfte.term.hide_cursor = 1;  // ?25l / hide cursor
            _sfte.term.cells[_SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y)].dirty = 1;
        } else if (p[0] == 7)
            _sfte.term.auto_wrap = 0;
        else if (p[0] == 6) {
            _sfte.term.origin_mode = 0;
            _sfte.term.cursor_x = 0;
            _sfte.term.cursor_y = 0;
        } else if (p[0] == 1047 || p[0] == 1048 || p[0] == 1049) {
            // 1047 / 1049 switch to main screen
            if ((p[0] == 1047 || p[0] == 1049) && _sfte.term.alt_active) {
                _sfte.term.alt_active = 0;
                if (_sfte.term.alt_cells) {
                    sfte_cell *tmp = _sfte.term.cells;
                    _sfte.term.cells = _sfte.term.alt_cells;
                    _sfte.term.alt_cells = tmp;
                    _sfte_dirty_range(0, _sfte.term.cols * _sfte.term.rows);
                }
            }

            // 1048 / 1049 restore cursor
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
    case 'r':  // set scroll region
    {
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
    case 'S':  // scroll up
    {
        _sfte_scroll(p[0] > 0 ? p[0] : 1);
        break;
    }
    case 'T':  // scroll down
    {
        _sfte_scroll(-(p[0] > 0 ? p[0] : 1));
        break;
    }
    case 'd':  // line pos abs / VPA
    {
        // move to specific row, keep column same
        int r = (p[0] > 0 ? p[0] : 1) - 1;
        if (_sfte.term.origin_mode) {
            r += _sfte.term.scroll_top;
            r = _SFTE_CLAMP(r, _sfte.term.scroll_top, _sfte.term.scroll_bottom);
        } else
            r = _SFTE_CLAMP(r, 0, _sfte.term.rows - 1);
        _sfte.term.cursor_y = r;
        break;
    }
    case 'X':  // erase char / ECH
    {
        // replace n chars with spaces from cursor
        int n = p[0] > 0 ? p[0] : 1;
        int rem = _sfte.term.cols - _sfte.term.cursor_x;
        if (n > rem) n = rem;

        int start_idx = _SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y);
        _sfte_clear_cells(start_idx, n);
        break;
    }
    case 'P':  // delete char / DCH
    {
        // deletes n chars, text to the right shifts left, eol blanked
        int n = p[0] > 0 ? p[0] : 1;
        int rem = _sfte.term.cols - _sfte.term.cursor_x;
        if (n > rem) n = rem;

        int move_cnt = rem - n;
        int base_idx = _SFTE_IDX(0, _sfte.term.cursor_y);
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[base_idx + _sfte.term.cursor_x],
                    &_sfte.term.cells[base_idx + _sfte.term.cursor_x + n],
                    move_cnt * sizeof(sfte_cell));

        int start_idx = base_idx + _sfte.term.cols - n;
        _sfte_clear_cells(start_idx, n);
        _sfte_dirty_range(base_idx + _sfte.term.cursor_x, rem);
        break;
    }
    case '@':  // insert char / ICH
    {
        // inserts n spaces, text shifts right, text pushed off edge is lost
        int n = p[0] > 0 ? p[0] : 1;
        int rem = _sfte.term.cols - _sfte.term.cursor_x;
        if (n > rem) n = rem;

        int move_cnt = rem - n;
        int base_idx = _SFTE_IDX(0, _sfte.term.cursor_y);
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[base_idx + _sfte.term.cursor_x + n],
                    &_sfte.term.cells[base_idx + _sfte.term.cursor_x],
                    move_cnt * sizeof(sfte_cell));

        int start_idx = base_idx + _sfte.term.cursor_x;
        _sfte_clear_cells(start_idx, n);
        _sfte_dirty_range(base_idx + _sfte.term.cursor_x, rem);
        break;
    }
    case 'L':  // insert line / IL
    {
        // inserts n blank lines at cursor, lines below get pushed
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
    case 'M':  // delete line / DL
    {
        // deletes n lines at cursor, lines below are pulled up
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
        _sfte_clear_cells(start_idx, n);
        _sfte_dirty_range(top * cols, height * cols);
        break;
    }
    case 'c':  // device attributes
    {
        if (p[0] != 0) break;
        const char *da = "\033[?6c";
        write(_sfte.pty_fd, da, strlen(da));
        break;
    }
    case 'q':  // dynamic cursor style
    {
        int style = p[0] ? p[0] : 0;
// two separate switch cases, not real performance difference and cleaner codebase-wise
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

        switch (style) {
        case 0: _sfte.term.cursor_style = SFTE_CURSOR_STYLE; break;
        case 1:
        case 2: _sfte.term.cursor_style = SFTE_CURSOR_BLOCK; break;
        case 3:
        case 4: _sfte.term.cursor_style = SFTE_CURSOR_UNDERLINE; break;
        case 5:
        case 6: _sfte.term.cursor_style = SFTE_CURSOR_BAR; break;
        }

        _sfte.term.cells[_SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y)].dirty = 1;

        break;
    }
    case 't':  // window title swap
    {
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
    case 'p':  // soft terminal reset
    {
#if SFTE_CURSOR_BLINK
        _sfte.term.blink_enabled = 1;
#endif  // SFTE_CURSOR_BLINK
        _sfte.term.cursor_style = SFTE_CURSOR_STYLE;
        _sfte.term.scroll_top = 0;
        _sfte.term.scroll_bottom = _sfte.term.rows - 1;
        _sfte.term.cur_fg = 0xFFFFFF;
        _sfte.term.cur_bg = SFTE_BG_COLOR;
        _sfte.term.cur_attr = 0;
        _sfte.term.hide_cursor = 0;

        _sfte.term.cells[_SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y)].dirty = 1;

        break;
    }
    case 's':  // save cursor
    {
        if (p[0] != 0) break;  // avoid kitty support command
        _sfte.term.ansi_saved_x = _sfte.term.cursor_x;
        _sfte.term.ansi_saved_y = _sfte.term.cursor_y;
        _sfte.term.ansi_saved_fg = _sfte.term.cur_fg;
        _sfte.term.ansi_saved_bg = _sfte.term.cur_bg;
        _sfte.term.ansi_saved_attr = _sfte.term.cur_attr;
        break;
    }
    case 'u': {
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
    VT_HASH        // #
} sfte_vt_state;

static void _sfte_parse_byte(uint8_t b) {
    switch (_sfte.term.vt_state) {
    case VT_GROUND:
        if (b == '\033' || b == '\x1b') {
            if (_sfte.term.cursor_x >= _sfte.term.cols) _sfte.term.cursor_x = _sfte.term.cols - 1;
            _sfte.term.vt_state = VT_ESCAPE;
        } else if (b == '\n') {
            if (_sfte.term.cursor_x >= _sfte.term.cols) _sfte.term.cursor_x = _sfte.term.cols - 1;
            if (_sfte.term.cursor_y == _sfte.term.scroll_bottom)
                _sfte_scroll(1);  // at bot margin, scroll text up
            else if (_sfte.term.cursor_y < _sfte.term.rows - 1)
                _sfte.term.cursor_y++;  // not at bot, move cursor down
        } else if (b == '\r')
            _sfte.term.cursor_x = 0;
        else if (b == '\t') {
            if (_sfte.term.cursor_x >= _sfte.term.cols) _sfte.term.cursor_x = _sfte.term.cols - 1;
            _sfte.term.cursor_x = (_sfte.term.cursor_x / 8 + 1) * 8;
            _sfte.term.cursor_x = _SFTE_CLAMP(_sfte.term.cursor_x, 0, _sfte.term.cols - 1);
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
            if (strncmp(_sfte.term.osc_payload, "11;?", 4) == 0) {
                const char *reply = "\033]11;rgb:0000/0000/0000\x07";
                write(_sfte.pty_fd, reply, strlen(reply));
            } else
                _SFTE_WARN(UNHANDLED_OSC, _sfte.term.osc_payload);

            _sfte.term.vt_state = VT_GROUND;
        } else if (_sfte.term.osc_idx < (int)sizeof(_sfte.term.osc_payload) - 1)
            _sfte.term.osc_payload[_sfte.term.osc_idx++] = b;
        break;
    case VT_CSI_ENTRY:
    case VT_CSI_PARAM:
        if (b == '?') {  // private marker
            _sfte.term.vt_state = VT_CSI_PARAM;

            _sfte.term.vt_dec_priv = 1;
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
    _sfte.repeat_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    int wl_fd = wl_display_get_fd(_sfte.display);

    while (_sfte.running) {
        wl_display_dispatch_pending(_sfte.display);
        wl_display_flush(_sfte.display);
        struct pollfd fds[] = {{.fd = wl_fd, .events = POLLIN},
                               {.fd = _sfte.pty_fd, .events = POLLIN},
                               {.fd = _sfte.repeat_timer_fd, .events = POLLIN}};
        int timeout = -1;

#if SFTE_CURSOR_BLINK
        uint64_t now = _sfte_time_ms();
        if (_sfte.term.blink_enabled) {
            int time_to_next = (int)(_sfte.term.next_blink_ms - now);
            if (time_to_next < 0) time_to_next = 0;
            timeout = time_to_next;
        }
#endif  // SFTE_CURSOR_BLINK

        if (poll(fds, _SFTE_ARRAY_LEN(fds), timeout /* default infinite timeout */) == -1) break;

#if SFTE_CURSOR_BLINK
        if (_sfte.term.blink_enabled) {
            now = _sfte_time_ms();
            if (now >= _sfte.term.next_blink_ms) {
                _sfte.term.blink_visible = !_sfte.term.blink_visible;
                _sfte.term.next_blink_ms = now + SFTE_CURSOR_BLINK_RATE;
                _sfte.term.cells[_SFTE_IDX(_sfte.term.cursor_x, _sfte.term.cursor_y)].dirty = 1;
                _sfte_wayland_render();
            }
        }
#endif  // SFTE_CURSOR_BLINK

        if (fds[0].revents & (POLLIN | POLLERR | POLLHUP))
            if (wl_display_dispatch(_sfte.display) == -1) _sfte.running = 0;
        if (fds[1].revents & (POLLIN | POLLERR | POLLHUP)) {
            uint8_t buf[SFTE_PTY_BUF_SIZE];
            ssize_t n = read(_sfte.pty_fd, buf, SFTE_PTY_BUF_SIZE);

            if (n > 0) {
#if SFTE_CURSOR_BLINK
                _sfte.term.blink_visible = 1;
                _sfte.term.next_blink_ms = _sfte_time_ms() + SFTE_CURSOR_BLINK_RATE;
#endif  // SFTE_CURSOR_BLINK

                for (ssize_t i = 0; i < n; ++i) _sfte_parse_byte(buf[i]);

                _sfte_wayland_render();
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
