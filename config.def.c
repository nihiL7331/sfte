/*
    config.def.c - default configuration template for sfte

    NOTE:
    This file is automatically copied to config.c on first build.
    Edit config.c to customize your terminal settings and font paths.
    To apply a change, you need to recompile the project using `cc nob.c -o nob && ./nob`.
*/

// #define SFTE_COLOR_BG_OPACITY 0xEE
// #define SFTE_CURSOR_TRAIL 10
#define SFTE_FONT_BOLD
#define SFTE_FONT_ITALIC
#define SFTE_FONT_BOLD_ITALIC
// ... other options

#define SFTE_IMPL
#include "sfte.h"

int main(void) {
    sfte_wayland_app *app = sfte_wayland_init();
    sfte_ctx *ctx = sfte_wayland_get_ctx(app);

    sfte_font_load_file(ctx, SFTE_FONT_STYLE_REGULAR, "/path/to/primary_font.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_REGULAR, "/path/to/nerd_symbols.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD, "/path/to/bold_font.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_ITALIC, "/path/to/italic_font.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD_ITALIC, "/path/to/bold_italic_font.ttf");

    return sfte_wayland_run(app);
}
