#pragma once

#include "src/api.h"
#include "src/config.h"
#include <stddef.h>
#include <stdint.h>

typedef enum sfte_underline_style {
    _SFTE_UNDERLINE_STYLE_STRAIGHT = 1,
    _SFTE_UNDERLINE_STYLE_DOUBLE = 2,
    _SFTE_UNDERLINE_STYLE_CURLY = 3,
    _SFTE_UNDERLINE_STYLE_DOTTED = 4,
    _SFTE_UNDERLINE_STYLE_DASHED = 5,
} sfte_underline_style;

struct sfte_stack {
    uint8_t *buf;
    size_t cap;
    size_t off;
};

#ifndef SFTE_NO_LOGGING
typedef struct sfte_logger {
    void (*func)(const char *tag,  // Always "sfte"
                 sfte_log_level log_level,
                 const char *message_or_null,  // A message string, may be nullptr in release mode
                 uint32_t line_nr              // Line number in sfte.h
    );
} sfte_logger;
#endif  // !SFTE_NO_LOGGING

#if SFTE_COLOR_TRUECOLOR
typedef uint32_t sfte_color;
#define _SFTE_COLOR_FG_DEFAULT SFTE_COLOR_FG
#define _SFTE_COLOR_BG_DEFAULT SFTE_COLOR_BG
#else  // !SFTE_COLOR_TRUECOLOR
typedef uint8_t sfte_color;
#define _SFTE_COLOR_FG_DEFAULT 254
#define _SFTE_COLOR_BG_DEFAULT 255
#endif  // !SFTE_COLOR_TRUECOLOR

#if SFTE_TERM_ASCII_CHARSET
typedef uint8_t sfte_rune;
#else   // !SFTE_TERM_ASCII_CHARSET
typedef uint32_t sfte_rune;
#endif  // SFTE_TERM_ASCII_CHARSET

typedef enum {
    _SFTE_ATTR_NONE = 0,
    _SFTE_ATTR_BOLD = 1 << 0,
    _SFTE_ATTR_ITALIC = 1 << 1,
    _SFTE_ATTR_UNDERLINE = 1 << 2,
    _SFTE_ATTR_REVERSE = 1 << 3,
#if SFTE_FONT_WIDE_CHARS
    _SFTE_ATTR_WIDE = 1 << 4,
    _SFTE_ATTR_DUMMY = 1 << 5,  // Marks skipped trailing cell after wide rune
#endif                          // SFTE_FONT_WIDE_CHARS
} sfte_attr;
// This might be a little overkill, but only costs comptime
_SFTE_ENSURE_EQUAL(_SFTE_ATTR_NONE, SFTE_ATTR_NONE);
_SFTE_ENSURE_EQUAL(_SFTE_ATTR_BOLD, SFTE_ATTR_BOLD);
_SFTE_ENSURE_EQUAL(_SFTE_ATTR_ITALIC, SFTE_ATTR_ITALIC);
_SFTE_ENSURE_EQUAL(_SFTE_ATTR_UNDERLINE, SFTE_ATTR_UNDERLINE);
_SFTE_ENSURE_EQUAL(_SFTE_ATTR_REVERSE, SFTE_ATTR_REVERSE);

/*
    Represents a single cell on the terminal grid.
*/
typedef struct sfte_cell {
    sfte_rune rune;
#if SFTE_FONT_WIDE_CHARS && SFTE_FONT_MAX_COMBINING
    sfte_rune combining_runes[SFTE_FONT_MAX_COMBINING];
#endif  // SFTE_FONT_WIDE_CHARS && SFTE_FONT_MAX_COMBINING

#if SFTE_INPUT_HYPERLINKS
    uint16_t link_idx;  // 0=no link, >0=index to `term.link_pool`
#endif                  // SFTE_INPUT_HYPERLINKS

    sfte_color fg;
    sfte_color bg;
#if SFTE_UNDERLINE_COLORED
    sfte_color ul_color;
#endif             // SFTE_UNDERLINE_COLORED
    uint8_t attr;  // Bitmask of sfte_attr
#if SFTE_FONT_WIDE_CHARS
    uint8_t combining_cnt;
#endif  // SFTE_FONT_WIDE_CHARS
#if SFTE_UNDERLINE_EXTENDED
    uint8_t ul_style;  // 1=straight, 2=double, 3=curl, 4=dotted, 5=dashed
#endif                 // SFTE_UNDERLINE_EXTENDED
    uint8_t dirty;     // 1 if this cell changed and needs redraw
#if SFTE_TERM_REFLOW
    uint8_t wrapped;  // 1 if this cell caused a soft line-wrap
#endif                // SFTE_TERM_REFLOW
} sfte_cell;

/*
    Represents a baked texture atlas entry for a single character.
*/
typedef struct sfte_glyph {
    uint16_t glyph_id;
    int16_t xadvance;
    uint16_t x0, y0, x1, y1;  // Atlas texture coordinates
    int16_t xoff, yoff;       // Render offsets
    uint8_t font_idx;         // 0=primary, 1+=fallbacks
} sfte_glyph;

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
/*
    Shared image buffer data.
*/
typedef struct sfte_img {
    uint32_t *pixels;  // Format ARGB8888
    uint32_t id;
    uint32_t ref_cnt;  // How many placements are using this image
    int32_t width;     // In pixels
    int32_t height;    // In pixels
    uint8_t is_sixel;
} sfte_img;

/*
    Represents where and how an image is drawn on the screen.
*/
typedef struct sfte_img_placement {
    uint32_t img_id;
    uint32_t placement_id;
    int16_t start_col;
    int16_t start_row;
    int16_t x_off;
    int16_t y_off;
    int8_t z_idx;  // <0=below text, >=0=above text
    uint8_t is_sixel;
#if SFTE_TERM_ALT_SCREEN
    uint8_t alt_screen;  // 0=main, 1=alt
#endif                   // SFTE_TERM_ALT_SCREEN
} sfte_img_placement;
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

#if SFTE_IMG_SIXEL
typedef enum {
    SIXEL_GROUND,
    SIXEL_REPEAT,       // !
    SIXEL_COLOR_INTRO,  // #
    SIXEL_COLOR_PARAM   // Color definition
} sfte_sixel_state_enum;

typedef struct sfte_sixel_state {
    uint32_t *pixels;  // Temporary dynamic buffer for the in-progress image

    uint32_t palette[256];
    uint32_t repeat_cnt;

    int32_t x, y;           // In pixels
    int32_t width, height;  // Maximum bounds currently touched
    int32_t cap_w, cap_h;   // Allocated capacity of `pixels`

    int16_t start_col;   // Grid column where parsing started
    int16_t start_row;   // Grid row where parsing started
    uint16_t params[5];  // Parsed numbers for color registers/HLS/RGB

    sfte_sixel_state_enum state;
    uint8_t col_idx;    // Active palette register index
    uint8_t param_idx;  // Current parameter index
} sfte_sixel_state;
#endif  // SFTE_IMG_SIXEL

#if SFTE_IMG_KITTY
typedef struct sfte_kitty_state {
    char *b64_buf;
    size_t b64_len;
    size_t b64_cap;

    uint32_t id;
    uint32_t placement_id;
    int32_t width;   // Source image width in pixels
    int32_t height;  // Source image height in pixels
    int32_t crop_x;  // Crop X start in pixels
    int32_t crop_y;  // Crop Y start in pixels
    int32_t crop_w;  // Crop width in pixels
    int32_t crop_h;  // Crop height in pixels

    int16_t cols;   // Target grid columns
    int16_t rows;   // Target height columns
    int16_t x_off;  // Placement X offset
    int16_t y_off;  // Placement Y offset

    int8_t z_idx;    // <0=below text, >=0=above text
    uint8_t format;  // 24=RGB, 32=RGBA, 100=PNG/JPEG
    uint8_t quiet;   // 0=always, 1=error, 2=never
    char action;     // Protocol action
    char t_medium;   // Transmission medium
    char d_action;   // Delete action
} sfte_kitty_state;
#endif  // SFTE_IMG_KITTY

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
typedef struct sfte_image_state {
    sfte_img *pool;
    sfte_img_placement *placements;
    uint32_t pool_cap;
    uint32_t pool_len;
    uint32_t placements_cap;
    uint32_t placements_len;
    uint32_t next_id;
} sfte_image_state;
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

#if SFTE_INPUT_HYPERLINKS
typedef struct sfte_link_state {
    char **pool;
    uint16_t pool_len;
    uint16_t pool_cap;
    uint16_t cur_idx;
} sfte_link_state;
#endif  // SFTE_INPUT_HYPERLINKS

typedef struct sfte_mux_layout {
    int32_t grid_off_y;     // px the grid is pushed down (bar on top, not overlayed)
    int16_t reserved_rows;  // rows taken away from the grid (0 or 1)
    int32_t bar_y;          // absolute px y of the bars top edge
    uint8_t overlay;        // 1 if the bar draws on top of a grid row
} sfte_mux_layout;

typedef struct sfte_text_input_state {
    uint16_t len;
    uint16_t cursor_byte_idx;
    char buf[SFTE_INPUT_TEXT_MAX_LEN];
} sfte_text_input_state;

#if SFTE_INPUT_MOUSE
typedef struct sfte_mouse_state {
    uint16_t mode;  // 0=off, 1000=normal, 1002=button-event, 1003=any-event
    uint16_t ext;   // 0=off, 1006=SGR
#if SFTE_INPUT_SELECTION
    int16_t sel_start_col, sel_end_col;
    int32_t sel_start_row, sel_end_row;
    uint8_t sel_active;
    uint8_t sel_dragging;
#if SFTE_TERM_SCROLLBACK_CAP
    int8_t sel_scroll_dir;
    uint64_t sel_last_scroll_ms;
#endif  // SFTE_TERM_SCROLLBACK_CAP
#endif  // SFTE_INPUT_SELECTION
} sfte_mouse_state;
#endif  // SFTE_INPUT_MOUSE

#if SFTE_INPUT_KITTY
typedef struct sfte_keyboard_state {
    uint16_t stack[2][16];
    uint8_t stack_idx[2];
} sfte_keyboard_state;
#endif  // SFTE_INPUT_KITTY

#if SFTE_FONT_LIGATURES
typedef struct sfte_shaper_feature {
    uint16_t lookup_indices[SFTE_FONT_MAX_LIGATURE_LOOKUPS];
    uint32_t subtable_offs[SFTE_FONT_MAX_LIGATURE_LOOKUPS][SFTE_FONT_MAX_LIGATURE_SUBTABLES];
    uint16_t subtable_cnts[SFTE_FONT_MAX_LIGATURE_LOOKUPS];
    uint16_t lookup_types[SFTE_FONT_MAX_LIGATURE_LOOKUPS];
    uint16_t lookup_cnt;
} sfte_shaper_feature;

/*
    Font shaper data context.
*/
typedef struct sfte_shaper_ctx {
    uint32_t table_off;
    uint32_t script_list_off;
    uint32_t feat_list_off;
    uint32_t lookup_list_off;

    sfte_shaper_feature calt;  // Contextual alternates
    sfte_shaper_feature liga;  // Standard ligatures
} sfte_shaper_ctx;

typedef struct sfte_shaper_subst_record {
    uint16_t sequence_idx;
    uint16_t lookup_idx;
} sfte_shaper_subst_record;
#endif  // SFTE_FONT_LIGATURES

#if SFTE_SEARCH
/*
    Represents a single highlgihted match in the grid.
*/
typedef struct sfte_search_match {
    int32_t logical_r;
    int16_t start_c;
    int16_t len;
} sfte_search_match;

typedef struct sfte_search_state {
    sfte_text_input_state text_input;

    void *compiled_re;  // Regex context from SFTE_SEARCH_RE_INIT
    sfte_search_match *matches;
    size_t pre_off;  // Where to rewind the arena when search is cleared

    uint32_t match_cnt;         // Total # of matches found
    uint32_t match_cap;         // Current capacity of `matches`
    uint32_t active_match_idx;  // Currently focused match

    uint8_t is_active;  // Is search mode currently open
    uint8_t ignore_case;
    uint8_t ui_dirty;
} sfte_search_state;
#endif  // SFTE_SEARCH

/*
    Font variant texture cache.
*/
typedef struct sfte_font_cache {
#if SFTE_FONT_LIGATURES
    sfte_shaper_ctx shaper[SFTE_FONT_MAX_COUNT];
#endif  // SFTE_FONT_LIGATURES
    sfte_font_backend_info info[SFTE_FONT_MAX_COUNT];
    uint8_t *ttf_buf[SFTE_FONT_MAX_COUNT];
    uint8_t *atlas_pxs;
    sfte_glyph *glyphs;

    float scales[SFTE_FONT_MAX_COUNT];

    int16_t atlas_x;
    int16_t atlas_y;
    int16_t atlas_row_h;

    uint8_t owns_ttf_buf[SFTE_FONT_MAX_COUNT];  // 1 if sfte allocated it via fopen, 0 if user
                                                // provided it
    int8_t num_fonts;
} sfte_font_cache;

typedef struct sfte_parser_state {
    uint16_t state;
    uint16_t params[16];
    uint8_t param_idx;
    uint8_t dec_priv;
    uint8_t csi_inter;

    char *osc_payload;
    size_t osc_len;
    size_t osc_cap;

#if !SFTE_TERM_ASCII_CHARSET
    uint32_t utf8_rune_acc;
#endif  // !SFTE_TERM_ASCII_CHARSET
#if SFTE_INPUT_MOUSE
    int16_t mouse_hover_col, mouse_hover_row;
    uint8_t mouse_btn_state;  // 0=LMB, 1=MMB, 2=RMB, 3=none
#endif                        // SFTE_INPUT_MOUSE
    uint8_t utf8_bytes_left;
} sfte_parser_state;

#if SFTE_CURSOR_TRAIL
typedef struct sfte_trail_state {
    float x, y;
    uint64_t last_move_ms;
    uint64_t last_update_ms;
    int16_t last_grid_col, last_grid_row;
    uint8_t is_trailing;
    uint8_t warp_tail;
    sfte_damage_rect dmg;
} sfte_trail_state;
#endif  // SFTE_CURSOR_TRAIL

typedef struct sfte_cursor_state {
    int16_t col;
    int16_t row;
    uint32_t color;
    uint8_t hidden;
#if SFTE_CURSOR_DYNAMIC
    uint8_t style;
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_CURSOR_BLINK
    uint8_t blink_enabled;
    uint8_t blink_visible;
    uint64_t next_blink_ms;
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_TRAIL
    sfte_trail_state trail;
#endif  // SFTE_CURSOR_TRAIL
} sfte_cursor_state;

typedef struct sfte_viewport_state {
    int16_t cols;
    int16_t rows;
    int16_t cell_width;
    int16_t cell_height;
    int16_t scroll_top;
    int16_t scroll_bot;
    int16_t grid_off;

#if SFTE_TERM_SCROLLBACK_CAP
    int32_t sb_cap;
    int32_t sb_len;
    int32_t sb_off;  // 0 = live, >0 = history
    int32_t sb_head;
#endif  // SFTE_TERM_SCROLLBACK_CAP
#if SFTE_TERM_SCROLL_SMOOTH
    int32_t last_sb_off;
    float scroll_y_off;
    uint64_t last_scroll_ms;
    uint8_t is_scrolling;
#endif  // SFTE_TERM_SCROLL_SMOOTH
} sfte_viewport_state;

typedef struct sfte_sgr_state {
    sfte_color fg;
    sfte_color bg;
    uint16_t attr;

#if SFTE_UNDERLINE_COLORED
    sfte_color ul_color;
#endif  // SFTE_UNDERLINE_COLORED
#if SFTE_UNDERLINE_EXTENDED
    uint8_t ul_style;
#endif  // SFTE_UNDERLINE_EXTENDED
} sfte_sgr_state;

/*
    0=main, 1=alt
*/
typedef struct sfte_saved_state {
    uint32_t fg[2];
    uint32_t bg[2];
    int16_t col[2];
    int16_t row[2];
    int16_t grid_off[2];
    uint8_t attr[2];
    char title[2][256];
} sfte_saved_state;

typedef struct sfte_modes_state {
    uint8_t auto_wrap;
    uint8_t origin_mode;
    uint8_t bracketed_paste;
#if SFTE_TERM_ALT_SCREEN
    uint8_t alt_active;
#endif  // SFTE_TERM_ALT_SCREEN;
#if SFTE_TERM_FOCUS
    uint8_t report_focus;
#endif  // SFTE_TERM_FOCUS
} sfte_modes_state;

typedef struct sfte_callbacks {
    sfte_write_cb write;
    sfte_bell_cb bell;
    sfte_progress_cb progress;
#if SFTE_CLIPBOARD_OSC52
    sfte_osc52_clipboard_cb osc52_clipboard;
#endif  // SFTE_CLIPBOARD_OSC52
#if SFTE_INPUT_HYPERLINKS
    sfte_open_link_cb open_link;
#endif  // SFTE_INPUT_HYPERLINKS
    void *user_data;
} sfte_callbacks;

typedef struct sfte_render_buffers {
#if SFTE_FONT_LIGATURES
    uint16_t *shaper_ids;
    uint64_t *row_hashes;
    uint16_t *row_shaper_ids;
#endif  // SFTE_FONT_LIGATURES
    uint16_t *ids;
    uint8_t *font_indices;
    sfte_font_cache **target_caches;
} sfte_render_buffers;

/*
    Core terminal emulation state machine and grid bounds.
*/
struct sfte_term {
    sfte_cell *cells;
    uint8_t *tab_stops;
    uint8_t tab_width;
#if SFTE_TERM_SCROLLBACK_CAP
    sfte_cell *scrollback;
#endif  // SFTE_TERM_SCROLLBACK_CAP
#if SFTE_TERM_ALT_SCREEN
    sfte_cell *alt_cells;
#endif  // SFTE_TERM_ALT_SCREEN
    const sfte_callbacks *cb;

    sfte_stack stack;
    sfte_render_buffers render;
    sfte_parser_state parser;
    sfte_cursor_state cursor;
    sfte_viewport_state viewport;
    sfte_sgr_state sgr;
    sfte_saved_state saved;
    sfte_modes_state modes;

#if SFTE_SEARCH
    sfte_search_state search;
#endif  // SFTE_SEARCH
#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    sfte_image_state images;
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
#if SFTE_INPUT_HYPERLINKS
    sfte_link_state links;
#endif  // SFTE_INPUT_HYPERLINKS
#if SFTE_INPUT_MOUSE
    sfte_mouse_state mouse;
#endif  // SFTE_INPUT_MOUSE
#if SFTE_INPUT_KITTY
    sfte_keyboard_state keyboard;
#endif  // SFTE_INPUT_KITTY
#if SFTE_IMG_SIXEL
    sfte_sixel_state sixel;
#endif  // SFTE_IMG_SIXEL
#if SFTE_IMG_KITTY
    sfte_kitty_state kitty;
#endif  // SFTE_IMG_KITTY

    char title[256];
    uint8_t title_locked;  // Toggled when a user-defined title is set
    int16_t last_drawn_col, last_drawn_row;

#if SFTE_TERM_ANIMATE_SCREEN
    uint64_t anim_start_ms;
    uint8_t is_animating;
    uint8_t anim_dir;
#endif  // SFTE_TERM_ANIMATE_SCREEN

    uint8_t is_allocated;
};

/*
    Terminal windows state for the multiplexer.
*/
typedef struct sfte_mux {
    sfte_term terms[SFTE_MUX_MAX_WINDOWS];
    uint8_t tab_order[SFTE_MUX_MAX_WINDOWS];  // Vis ord -> slot idx
    uint8_t tab_cnt;                          // # of curr open tabs

    sfte_text_input_state text_input;
    int16_t rename_cursor_col;
    uint8_t is_renaming;

    // Hitbox for mouse inputs
    int32_t tab_px_x[SFTE_MUX_MAX_WINDOWS];
    int32_t tab_px_w[SFTE_MUX_MAX_WINDOWS];

    uint8_t active_idx;
    int16_t hovered_idx;
    uint8_t is_prefix_active;
#if SFTE_MUX_PREFIX_PREVIEW
    uint8_t was_prefix_active;
    uint8_t preview_selected_idx;
#endif  // SFTE_MUX_PREFIX_PREVIEW
    uint8_t pending_spawn;
    uint8_t pending_close;
    int8_t pending_reorder;  // -1 for left, 1 for right
    uint8_t ui_dirty;
} sfte_mux;

/*
    Central typography metrics and caching.
*/
typedef struct sfte_font_state {
    sfte_font_cache regular;
#ifdef SFTE_FONT_BOLD
    sfte_font_cache bold;
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    sfte_font_cache italic;
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    sfte_font_cache bold_italic;
#endif               // SFTE_FONT_BOLD_ITALIC
    float cur_size;  // Starts at SFTE_FONT_DEFAULT_SIZE

    int16_t ascent;    // Distance from cell top to the baseline
    int16_t descent;   // Distance from baseline to cell bottom
    int16_t line_gap;  // Recommended empty space between lines
} sfte_font_state;

typedef struct sfte_window_state {
    int32_t width;
    int32_t height;
    uint8_t pad_dirty;
#if SFTE_TERM_FOCUS
    uint8_t is_focused;
#endif  // SFTE_TERM_FOCUS
} sfte_window_state;

/*
    Main context for the emulator core.
*/
struct sfte_ctx {
    sfte_mux mux;
    sfte_font_state font;
    sfte_callbacks cb;
    sfte_window_state window;
#ifndef SFTE_NO_LOGGING
    sfte_logger logger;
#endif  // !SFTE_NO_LOGGING
};

#if SFTE_WAYLAND
typedef struct sfte_wayland_window_backend {
    int32_t pty_fd;  // Master file descriptor to read/write from
    pid_t pty_pid;   // PID of shell
#if SFTE_TERM_RESURRECT
    int32_t log_fd;
#endif  // SFTE_TERM_RESURRECT
} sfte_wayland_window_backend;

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
#if SFTE_INPUT_SELECTION
    struct wl_pointer *pointer;
#endif  // SFTE_INPUT_SELECTION
#if SFTE_CLIPBOARD
    struct wl_data_device_manager *data_device_manager;
    struct wl_data_device *data_device;
    struct wl_data_source *data_source;
    struct wl_data_offer *data_offer;
    char data_offer_mime[64];
    char *selection_text;
#endif  // SFTE_CLIPBOARD
#if SFTE_INPUT_SELECTION || SFTE_CLIPBOARD
    uint32_t serial;
#endif  // SFTE_INPUT_SELECTION || SFTE_CLIPBOARD
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_buffer *buffer;
    uint32_t *shm_data;
    size_t shm_size;
#if SFTE_TERM_DOUBLE_BUFFER
    uint32_t *back_buffer;
#endif  // SFTE_TERM_DOUBLE_BUFFER

    int32_t width, height;
    int32_t pending_width, pending_height;
    int32_t mouse_x, mouse_y;
    sfte_wayland_window_backend win_backends[SFTE_MUX_MAX_WINDOWS];

    int32_t repeat_timer_fd;
    int32_t repeat_rate;
    int32_t repeat_delay;
    uint32_t repeating_key;

    uint8_t running;
    uint8_t needs_render;
    uint8_t frame_pending;
};
#endif  // SFTE_WAYLAND

typedef struct sfte_resize_buffers {
    sfte_cell *main_grid;
#if SFTE_TERM_SCROLLBACK_CAP
    sfte_cell *sb_grid;
    int32_t sb_lines;
#endif  // SFTE_TERM_SCROLLBACK_CAP
    int16_t new_col, new_row;
} sfte_resize_buffers;

#if SFTE_TERM_REFLOW
typedef struct sfte_reflow_state {
    sfte_cell *temp_rows;

    int32_t reflow_row;

    int16_t new_cols;
    int16_t reflow_col;
    int16_t new_col, new_row;
    int16_t target_old_col, target_old_row;

    uint8_t is_live;
} sfte_reflow_state;
#endif  // SFTE_TERM_REFLOW

typedef struct sfte_pass_info {
    int32_t y_off;
    sfte_cell *grid;
    uint8_t hide_cursor;
} sfte_pass_info;
