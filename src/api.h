#pragma once

#include "src/config.h"
#include <stdint.h>
#ifdef SFTE_XKB_COMMON
#include <xkbcommon/xkbcommon.h>
#endif  // SFTE_XKB_COMMON

// =================================================================================================
// >>enums & structs
// =================================================================================================

typedef enum sfte_font_style {
    SFTE_FONT_STYLE_REGULAR = 0,
#ifdef SFTE_FONT_BOLD
    SFTE_FONT_STYLE_BOLD = 1,
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    SFTE_FONT_STYLE_ITALIC = 2,
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    SFTE_FONT_STYLE_BOLD_ITALIC = 3,
#endif  // SFTE_FONT_BOLD_ITALIC
} sfte_font_style;

typedef enum sfte_modifier {
    SFTE_MOD_NONE = 0b0000,
    SFTE_MOD_CTRL = 0b0001,
    SFTE_MOD_ALT = 0b0010,
    SFTE_MOD_SHIFT = 0b0100,
    SFTE_MOD_SUPER = 0b1000,
} sfte_modifier;

/*
    Used to report what regions of the pixel buffer have changed.
*/
typedef struct sfte_damage_rect {
    int32_t x, y, w, h;
} sfte_damage_rect;

typedef enum sfte_mouse_button {
    SFTE_MOUSE_BUTTON_LEFT = 0,
    SFTE_MOUSE_BUTTON_MIDDLE = 1,
    SFTE_MOUSE_BUTTON_RIGHT = 2,
    SFTE_MOUSE_BUTTON_NONE = 3,  // Used internally for release states in legacy protocol
} sfte_mouse_button;

// =================================================================================================
// >>callbacks
// =================================================================================================

/*
    Callback interface for the core to talk back to the host.
*/
typedef void (*sfte_write_cb)(void *user_data, const char *data, size_t len);

#if SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52
/*
    Callback interface for the core to request clipboard copy/paste operations.
    `target` is typically 'c' (clipboard) or 'p' (primary selection).
*/
typedef void (*sfte_osc52_clipboard_cb)(void *user_data, char target, const char *data);
#endif  // SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52

#if SFTE_INPUT_HYPERLINKS
/*
    Callback interface for the core to request opening a URI.
*/
typedef void (*sfte_open_link_cb)(void *user_data, const char *uri);
#endif  // SFTE_INPUT_HYPERLINKS

/*
    Fired when the terminal receives the BEL (\a) character.
*/
typedef void (*sfte_bell_cb)(void *user_data);

/*
    Fired when the terminal receives an OSC 9;4 progress bar sequence.
    `state` can either be:
        0 for remove,
        1 for normal,
        2 for error,
        3 for indeterminate,
        4 for warning.
    `progress` is a value from 0 to 100.
*/
typedef void (*sfte_progress_cb)(void *user_data, uint8_t state, uint8_t progress);

// =================================================================================================
// >>core initialization & lifecycle
// =================================================================================================

/*
    Context (de)initialization
*/
sfte_ctx *sfte_init(sfte_write_cb write_fn, void *user_data, const sfte_config *cfg);
void sfte_free(sfte_ctx *ctx);

/*
    Loads a TTF font from a raw memory buffer (e.g. compiled into the binary).
*/
void sfte_font_load_mem(sfte_ctx *ctx, sfte_font_style style, const uint8_t *ttf_data);

/*
    Convenience function to load a TTF font from the disk.
    Useful for runtime font swap functionality.
*/
void sfte_font_load_file(sfte_ctx *ctx, sfte_font_style style, const char *path);

#ifndef SFTE_NO_POSIX
/*
    Spawns a shell and populates `out_fd` with the master PTY descriptor.
    Returns the shell PID.
*/
pid_t sfte_posix_pty_spawn(sfte_ctx *ctx, int32_t *out_fd, uint16_t cols, uint16_t rows,
                           uint16_t px_w, uint16_t px_h);

/*
    Sends the TIOCSWINSZ ioctl to keep the OS shell in sync with the engine grid.
*/
void sfte_posix_pty_resize(const sfte_term *term, int32_t pty_fd, uint16_t px_w, uint16_t px_h);
#endif  // SFTE_NO_POSIX

/*
    Returns the maximum time (in milliseconds) the host event loop
    should sleep before the terminal requires a redraw.
    Returns -1 if the terminal is fully static and can sleep indefinitely.
*/
int32_t sfte_get_timeout_ms(sfte_ctx *ctx);

/*
    Updates time-based internal state (animations, cursor trail, blinking).
    Returns 1 if the terminal visual state changed and requires a redraw.
*/
uint8_t sfte_tick(sfte_ctx *ctx);

/*
    Returns the terminal of a given index.
    If SFTE_MUX is not defined, returns the only (main) terminal struct.
    Returns NULL if terminal under the provided index is not allocated/out of bounds.
*/
sfte_term *sfte_term_get_at(sfte_ctx *ctx, uint8_t idx);

/*
    Returns the currently active terminal.
    If SFTE_MUX is not defined, returns the only (main) terminal struct.
*/
sfte_term *sfte_term_get_active(sfte_ctx *ctx);

/*
    Returns the index of provided `term` in the multiplexer array.
    If `ctx` or `term` is NULL, returns -1.
    If SFTE_MUX is not defined, returns 0.
*/
int8_t sfte_term_get_idx(sfte_ctx *ctx, sfte_term *term);

/*
    Allocates a new terminal window struct and returns its handle.
    If SFTE_MUX is defined, returns NULL if the multiplexer is full.
    If it is not defined, returns NULL if already allocated.
*/
sfte_term *sfte_term_spawn(sfte_ctx *ctx);

/*
    Closes a terminal window.
    If SFTE_MUX is defined, cleans up the provided `term`
    and shifts focus to another available window.
    If SFTE_MUX is NOT defined, just cleans up the provided `term`.

    Returns 0 if no panes remain active.
*/
uint8_t sfte_term_close(sfte_ctx *ctx, sfte_term *term);

// =================================================================================================
// >>rendering & parsing
// =================================================================================================

/*
    Ask engine how many pixels it needs to display a specified grid.
*/
void sfte_get_ideal_size(sfte_ctx *ctx, int16_t cols, int16_t rows, int32_t *out_w, int32_t *out_h);

/*
    Feed bytes from the shell/PTY to the terminal state machine.
*/
void sfte_parse(sfte_ctx *ctx, sfte_term *term, const uint8_t *data, size_t len);

/*
    Render the grid to the provided `px_buf` buffer
    and populate the `out_dmg` damage rectangle.

*/
void sfte_render(sfte_ctx *ctx, void *px_buf, int32_t w, int32_t h, sfte_damage_rect *out_dmg);

/*
    Explicitly tell the terminal engine its canvas size has changed.
*/
void sfte_resize(sfte_ctx *ctx, int32_t w, int32_t h);

#if SFTE_FONT_ZOOM
/*
    Zooms in/out changing the font size, rebaking and reallocating the terminal grid data.
    `delta` > 0 - zoom in
    `delta` < 0 - zoom out
    `delta` = 0 - noop
*/
void sfte_zoom(sfte_ctx *ctx, float delta);
#endif  // SFTE_FONT_ZOOM

// =================================================================================================
// >>input & interaction
// =================================================================================================

/*
    Feed raw UTF-8 text into the terminal.
    Used for both keyboard typing and streaming clipboard chunks.
*/
void sfte_input_text(sfte_ctx *ctx, const char *text, size_t len);

/*
    Feed a keyboard shortcut into the terminal.
    Used for virtual shortcuts (font zoom, multiplexer, search, etc.).
    Similarly to shortcut functions themselves:
    - returns 1 if any shortcut was triggered and the input should be consumed,
    - retuns 0 otherwise.
*/
uint8_t sfte_input_shortcut(sfte_ctx *ctx, sfte_key key, uint32_t mod_mask);

/*
    Signals the start of a clipboard paste operation.
    Generates \033[200~ (bracketed paste) if enabled by the shell.
*/
void sfte_input_paste_begin(const sfte_term *term);

/*
    Signals the end of a clipboard paste operation.
    Generates \033[201~ (bracketed paste) if enabled by the shell.
*/
void sfte_input_paste_end(const sfte_term *term);

/*
    Feed a physical control key into the terminal.
    Automatically generates VT100/ANSI escape codes (arrows, F-keys).
    Routes through `sfte_write_cb` internally.
*/
void sfte_input_key(sfte_ctx *ctx, sfte_key key, uint32_t mod_mask);

#if SFTE_INPUT_MOUSE
/*
    Feed raw OS mouse events to the terminal engine for hover events and drag selections.
*/
void sfte_mouse_move(sfte_ctx *ctx, int32_t px_x, int32_t px_y);

/*
    Feed mouse clicks to the engine.
    `pressed` is a boolean: 1 for button down, 0 for button release.
*/
void sfte_mouse_click(sfte_ctx *ctx, sfte_mouse_button btn, uint8_t pressed, int32_t px_x,
                      int32_t px_y);

/*
    Feed scroll wheel events to the terminal engine.
    `dir` is the scroll direction: positive (>0) for up, negative (<0) for down.
*/
void sfte_mouse_scroll(sfte_ctx *ctx, int8_t dir, int32_t px_x, int32_t px_y);
#endif  // SFTE_INPUT_MOUSE

#if SFTE_INPUT_HYPERLINKS
/*
    Returns the URL at the given cell coords.
    Returns NULL if no link is present.
*/
const char *sfte_get_link_at(sfte_ctx *ctx, int16_t col, int16_t row);
#endif  // SFTE_INPUT_HYPERLINKS

#if SFTE_INPUT_SELECTION
/*
    Copies the UTF-8 selection into `out_buf`, up to `max_bytes`.
    If `out_buf` is NULL, performs a dry-run and returns the required byte size.
    Returns 0 if nothing is selected.
*/
size_t sfte_get_selection(sfte_ctx *ctx, char *out_buf, size_t max_bytes);
#endif  // SFTE_INPUT_SELECTION

#if SFTE_TERM_SCROLLBACK
/*
    Shift viewport up or down in the scrollback buffer.
*/
void sfte_view_scroll(sfte_ctx *ctx, int32_t delta);
#endif  // SFTE_TERM_SCROLLBACK

#ifdef SFTE_XKB_COMMON
/*
    Input entrypoint for XKB (Linux) implementations.
    Handles modifiers, control keys, kitty keyboard support (if enabled) and shortcuts.
*/
void sfte_xkb_process_key(sfte_ctx *ctx, struct xkb_state *state, uint32_t keycode);
#endif  // SFTE_XKB_COMMON

#if SFTE_TERM_FOCUS
/*
    Set window focus to `focused`.
    Toggles the state, dirties the cursor cell and sends I/O escape sequences to PTY.
*/
void sfte_set_focus(sfte_ctx *ctx, uint8_t focused);
#endif  // SFTE_TERM_FOCUS

// =================================================================================================
// >>wayland backend
// =================================================================================================

#if SFTE_WAYLAND
typedef struct sfte_wayland_app sfte_wayland_app;

/*
    Initializes Wayland, PTY and `sfte_ctx`.
*/
sfte_wayland_app *sfte_wayland_init(const sfte_config *cfg);

/*
    Exposes the context for runtime configuration.
*/
sfte_ctx *sfte_wayland_get_ctx(sfte_wayland_app *app);

/*
    Enters the blocking event loop, run it last.
*/
int sfte_wayland_run(sfte_wayland_app *app);
#endif  // SFTE_WAYLAND
