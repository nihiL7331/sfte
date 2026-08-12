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
#include <stdio.h>

// >>api
int sfte_run(int argc, char **argv);

/*=== IMPLEMENTATION =========================================================*/
#ifdef SFTE_IMPL

#include "xdg-shell.c"
#include "xdg-shell.h"

// >>memory

// >>logging

// >>api
int sfte_run(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("[sfte] starting...\n");
    return 0;
}

#endif  // SFTE_IMPL
