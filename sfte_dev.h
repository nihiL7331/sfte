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
