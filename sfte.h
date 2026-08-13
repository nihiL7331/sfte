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
#include <stdio.h>

// >>structs
typedef struct sfte_logger {
    void (*func)(const char *tag,              // always "sfte"
                 uint32_t log_level,           // 0=panic, 1=error, 2=warning, 3=info
                 const char *message_or_null,  // a message string, may be nullptr in release mode
                 uint32_t line_nr              // line number in sfte.h
    );
} sfte_logger;

typedef struct {
    int argc;            // currently unused
    char **argv;         // currently unused
    sfte_logger logger;  // optional
} sfte_desc;

// >>api
int sfte_run(const sfte_desc *desc);

/*=== IMPLEMENTATION =========================================================*/
#ifdef SFTE_IMPL

#include "xdg-shell.c"
#include "xdg-shell.h"
#include <string.h>

// >>memory

// >>logging
#ifndef SFTE_ASSERT
#include <assert.h>
#define SFTE_ASSERT(c, m) assert(c &&m)
#endif  // SFTE_ASSERT

#ifndef SFTE_LOG_LEVEL
#define SFTE_LOG_LEVEL 3
#endif  // SFTE_LOG_LEVEL

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

#define _SFTE_LOG_ITEMS _SFTE_LOGITEM_XMACRO(OK, "Ok")
#define _SFTE_LOGITEM_XMACRO(item, msg) item,
typedef enum { _SFTE_LOG_ITEMS } _sfte_log_item_t;
#undef _SFTE_LOGITEM_XMACRO
#define _SFTE_LOGITEM_XMACRO(item, msg) #item ": " msg,
static const char *_sfte_log_messages[] = {_SFTE_LOG_ITEMS};
#undef _SFTE_LOGITEM_XMACRO

static void _sfte_log(_sfte_log_item_t log_item, uint32_t log_level, uint32_t line_nr) {
    if (log_level > SFTE_LOG_LEVEL) return;
    void (*log_func)(const char *, uint32_t, const char *,
                     uint32_t) = _sfte.logger.func ? _sfte.logger.func : _sfte_logger_default;
    log_func("sfte", log_level, _sfte_log_messages[log_item], line_nr);

    // for log level PANIC it would be 'undefined behaviour' to continue
    if (log_level == 0) abort();
}

#define _SFTE_PANIC(code) _sfte_log(code, 0, __LINE__)
#define _SFTE_ERROR(code) _sfte_log(code, 1, __LINE__)
#define _SFTE_WARN(code) _sfte_log(code, 2, __LINE__)
#define _SFTE_INFO(code) _sfte_log(code, 3, __LINE__)

// >>api
int sfte_run(const sfte_desc *desc) {
    if (desc) _sfte.logger = desc->logger;
    _SFTE_INFO(OK);
    return 0;
}

#endif  // SFTE_IMPL
