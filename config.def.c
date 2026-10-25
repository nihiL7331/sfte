/*
    config.def.c - default configuration template for sfte

    NOTE:
    This file is automatically copied to config.c on first build.
    Edit config.c to customize your terminal settings and font paths.
    To apply a change, you need to recompile the project using `cc nob.c -o nob && ./nob install`.
*/

#define SFTE_FONT_BOLD
#define SFTE_FONT_ITALIC
#define SFTE_FONT_BOLD_ITALIC
// ... other options

#define SFTE_IMPL
#ifdef SFTE_DEV_ENV
#include "sfte_dev.h"
#else  // !SFTE_DEV_ENV
#include "sfte.h"
#endif  // !SFTE_DEV_ENV

/* More configuration options are available.
   For detailed descriptions of all options see sfte.h or src/config.h. */
static const sfte_config cfg = {
    .font =
        {
            .regular = {"/path/to/primary_font.ttf", "/path/to/nerd_symbols.ttf"},
            .bold = {"/path/to/bold_font.ttf"},
            .italic = {"/path/to/italic_font.ttf"},
            .bold_italic = {"/path/to/bold_italic_font.ttf"},
            .default_size = 16.0f,
            .min_size = 1.0f,
            .max_size = 96.0f,
            .scales = {1.0f, 0.8f, 1.0f, 1.0f},
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
            .pad_y = 4,
            .init_cols = 80,
            .init_rows = 24,
            .term_env = "xterm-256color",
        },
    .cursor =
        {
            .style = SFTE_CURSOR_STYLE_BLOCK,
        },
    .underline =
        {
            .thick_ratio = 0.1f,
            .off_ratio = 0.12f,
        },
    .scroll =
        {
            .scrollback_cap = 2000,
        },
};

int main(void) {
    sfte_wayland_app *app = sfte_wayland_init(&cfg);
    return sfte_wayland_run(app);
}
