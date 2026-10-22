// clang-format off
/*
    sfte -- single-file terminal emulator (library)
    v2.0.0

    Project URL: https://github.com/nihiL7331/sfte

    Do this:
        #define SFTE_IMPL
    before you include this file in the `config.c` file.

    You can #define SFTE_ASSERT(x) before the #include to avoid using assert.h.
    And #define SFTE_MALLOC, SFTE_REALLOC, SFTE_CALLOC and SFTE_FREE
    to avoid using malloc, realloc, free.

    PURPOSE
    =======
    Primarily of interest to: Wayland users, since it's the only backend currently supported
                              Embedded tinkerers, due to it's possible low memory usage and
                                                  performance-altering customizability
                              Game engine developers, who need an in-game terminal that works.

    Both the font backend (by default, stb_truetype) and rendering/I/O backend (by default, Wayland)
    can be changed by #define SFTE_CUSTOM_BACKEND, #define SFTE_NO_POSIX
                                                               and #define SFTE_FONT_CUSTOM_BACKEND.


    Full customization macros documentation below, in the `>>>CONFIGURATION` section.
    *sfte* requires explicit dependency enabling.

    EYE CANDY
    =========

    If we're not working on a low-power machine, we can easily purpose 0.1% of our CPU for some
    eye candy. Over time, terminal emulators started implementing different eye-catching 'candies'.
    *sfte* has support for a cursor trail (similarly to kitty and neovide), it can be enabled using:

    #define SFTE_CURSOR_TRAIL 10


    Another, similar feature implemented are screen buffer transitions.
    It's a simple scroll animation that happens when terminal transitions from main to alt grid.
    That occurs when a TUI like nvim, yazi, btop gets opened.
    It can be seen in the README.md gif. This can be enabled by using:

    #define SFTE_TERM_ANIMATE_SCREEN 1

    NOTE:
    Due to how tmux works, even if enabled, this function doesn't work when using it.
    This *technically* could be 'fixed' by intercepting the SM/RM calls, but it's hacky and probably
    will never be apart of the *sfte* core.

    FONTS
    =====

    There's a lot of customization options regarding fonts.
    50% of API calls with a default setup are also font-related.

    To set them up properly, first consider what font types you want to use.
    If you want to use a bold font,        add #define SFTE_FONT_BOLD.
    If you want to use an italic font,     add #define SFTE_FONT_ITALIC.
    If you want to use a bold-italic font, add #define SFTE_FONT_BOLD_ITALIC.

    If a certain font type is enabled, at least one font of that type must be loaded, using either:

    void sfte_font_load_mem(sfte_ctx *ctx, sfte_font_style style, const uint8_t *ttf_data)

    or

    void sfte_font_load_file(sfte_ctx *ctx, sfte_font_style style, const char *path)

    They both achieve very different things (but both can be used at once).
    `sfte_font_load_mem` expects the font data as a pointer to a const array.
    This means, that the data lives inside the binary, bloating the binary size significantly
    in exchange for having the font 'baked' into the binary.
    That means, that the terminal becomes a single unit, independent from system fonts.
    To get a TTF font in form of a C buffer, you can use xxd:

    xxd -i /path/to/my/font.ttf > font_data.h

    Then, you just include the font data header inside `config.c`, and pass the array from inside
    that file into the `sfte_font_load_mem` function:

    sfte_font_load_mem(ctx, SFTE_FONT_STYLE_REGULAR, _path_to_my_font_ttf);

    `sfte_font_load_file`, on the other hand loads the font from a provided file path on startup.
    This means that the font data is saved to RAM, not changing the binary size.
    However, it requires the font to live in provided directory DURING THE STARTUP.
    It's a less portable, but more lightweight solution.
    It can be used like so:

    sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD, "/path/to/my/font_bold.ttf");


    *sfte* supports fallback fonts. That's especially useful for cases where we have a primary font
    and a nerd symbols font. They can be loaded using two sfte_font_load_file/mem calls:

    sfte_font_load_file(ctx, SFTE_FONT_STYLE_REGULAR, "/path/to/my/main_font.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_REGULAR, "/path/to/my/nerd_symbols_font.ttf");

    Because the nerd symbols font is loaded second, it's a 'second priority' font.


    Often times, the nerd symbols are too big, causing weird visual overlaps.
    To scale down the nerd symbols font, you can use:

    #define SFTE_FONT_SCALES {1.0f, 0.8f}

    SHORTCUTS
    =========

    It is possible to create own shortcuts.
    To create a shortcut, first create its callback following this definition:

    uint8_t my_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
        // my shortcut logic... e.g.,
        (void)arg;
        printf("Current cursor position: %d x %d\n", ctx->term.cursor.col, ctx->term.cursor.row);
        return 1;
    }

    Inside this function write the wanted logic that should happen when a shortcut is toggled.
    The returned value should be either 0 or 1:
    - 0, if the callback shouldn't consume the keyboard input required to toggle that shortcut,
    - 1, if it should.
    `sfte_arg` is a type that's a union:

    typedef union {
        int i;
        float f;
        const void *v;
    } sfte_arg;

    Then, add it to your SFTE_SHORTCUTS macro override:

    #define SFTE_SHORTCUTS {SFTE_BASE_SHORTCUTS,                                                   \
        {SFTE_MOD_CTRL | SFTE_MOD_ALT, XKB_KEY_C, my_shortcut, {.v = NULL}}}

    NOTE:
    Remember to use SFTE_BASE_SHORTCUTS if you want to keep default shortcuts.

    SEARCH
    ======

    sfte comes with a simple scrollback buffer searchbar.
    It supports regex via regex.h on Linux and tiny-regex-c on other (custom) platforms.
    It works with smart case rules, that is:
    - if the entire query is in lowercase, the case is ignored,
    - otherwise the search is case-sensitive.

    Search support can be enabled/disabled by defining the SFTE_SEARCH macro.
    By default, searching is enabled.

    The default shortcut to toggle the search bar is Ctrl+Shift+/.
    To exit the search mode, either click Escape or Backspace when the query is empty.
    Use Arrow Up/Down to change the currently active match.

    Both the search bar and search results can be heavily customized.

    Available options:

    SFTE_SEARCH                 (default: 1)
    SFTE_SEARCH_PREFIX          (default: " search: ")
    SFTE_SEARCH_PLACEMENT_TOP   (default: 0)
    SFTE_SEARCH_BG_BAR          (default: 0x000000)
    SFTE_SEARCH_BG_BAR_OPACITY  (default: 0x7F)
    SFTE_SEARCH_BG_ACTIVE       (default: SFTE_COLOR_FG)
    SFTE_SEARCH_BG_MATCH        (default: 0x7F7F7F)
    SFTE_SEARCH_FG_ACTIVE       (default: SFTE_COLOR_BG)
    SFTE_SEARCH_FG_MATCH        (default: SFTE_COLOR_BG)
    SFTE_SEARCH_ATTR_ACTIVE     (default: SFTE_ATTR_BOLD)
    SFTE_SEARCH_ATTR_MATCH      (default: SFTE_ATTR_NONE)
    SFTE_SEARCH_MATCH_MAX_CAP   (default: 4096)
    SFTE_SEARCH_MATCH_INIT_CAP  (default: 16)
    SFTE_SEARCH_ROW_BUF_CAP     (default: 8192)

    For an in-depth explanation of configuration options, check the '>>search macros' section below.

    TODO:
    Sections explaining: custom backend support, procedural boxes, cursor macros, images, input,
    clipboard, minimal setup, SFTE_COLOR_DRAW/BLEND_PIXEL overrides, ligatures.
    Also match the style of all the sections to 'SEARCH'.

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
#include "src/api.h"
#include "src/config.h"
#define SFTE_IMPL
#ifdef SFTE_IMPL
#include "src/types.h"
#include "src/util.h"
#include "src/font.h"
#include "src/grid.h"
#include "src/sixel.h"
#include "src/kitty.h"
#include "src/mux.h"
#include "src/search.h"
#include "src/parser.h"
#include "src/input.h"
#include "src/render.h"
#include "src/core.h"
#include "src/wayland_backend.h"
#endif  // SFTE_IMPL
