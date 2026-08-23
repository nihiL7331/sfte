//
// example config file for sfte
//
#define SFTE_BG_OPACITY 0xEE
#define SFTE_CURSOR_TRAIL 10
#define SFTE_CURSOR_STYLE SFTE_CURSOR_BLOCK
#define SFTE_FONT_BOLD
#define SFTE_FONT_ITALIC
#define SFTE_FONT_BOLD_ITALIC
#define SFTE_LOG_LEVEL 3
#define SFTE_IMPL
#include "dejavusansmono-bold.h"
#include "dejavusansmono.h"
#include "sfte.h"

int main(void) {
    sfte_wayland_app *app = sfte_wayland_init();
    sfte_ctx *ctx = sfte_wayland_get_ctx(app);

    sfte_font_load_mem(ctx, SFTE_FONT_STYLE_REGULAR, _usr_share_fonts_TTF_DejaVuSansMono_ttf);
    sfte_font_load_mem(ctx, SFTE_FONT_STYLE_BOLD, _usr_share_fonts_TTF_DejaVuSansMono_Bold_ttf);
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_ITALIC,
                        "/usr/share/fonts/TTF/DejaVuSansMono-Oblique.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD_ITALIC,
                        "/usr/share/fonts/TTF/DejaVuSansMono-BoldOblique.ttf");

    return sfte_wayland_run(app);
}
