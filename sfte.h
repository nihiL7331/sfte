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

// >>config
#ifndef SFTE_LOG_LEVEL
#define SFTE_LOG_LEVEL 3
#endif  // SFTE_LOG_LEVEL

#ifndef SFTE_STARTUP_WIDTH
#define SFTE_STARTUP_WIDTH 800
#endif  // SFTE_STARTUP_WIDTH

#ifndef SFTE_STARTUP_HEIGHT
#define SFTE_STARTUP_HEIGHT 600
#endif  // SFTE_STARTUP_HEIGHT

#ifndef SFTE_LOGGER_FUNC
#define SFTE_LOGGER_FUNC _sfte_logger_default
#endif  // SFTE_LOGGER_FUNC

#ifndef SFTE_TERM_ENV
#define SFTE_TERM_ENV "xterm-256color"
#endif  // SFTE_TERM_ENV

#ifndef SFTE_PTY_BUF_SIZE
#define SFTE_PTY_BUF_SIZE 4096
#endif  // SFTE_PTY_BUF_SIZE

#ifndef SFTE_BG_COLOR
#define SFTE_BG_COLOR 0x000000
#endif  // SFTE_BG_COLOR

#ifndef SFTE_FONT_PATH
#define SFTE_FONT_PATH "/usr/share/fonts/TTF/DejaVuSansMono.ttf"
#endif  // SFTE_FONT_PATH

#ifndef SFTE_DEFAULT_FONT_SIZE
#define SFTE_DEFAULT_FONT_SIZE 12.0f
#endif  // SFTE_DEFAULT_FONT_SIZE

#ifndef SFTE_FONT_RESIZE_SPEED
#define SFTE_FONT_RESIZE_SPEED 2.0f
#endif  // SFTE_FONT_RESIZE_SPEED

// >>api
int sfte_run(void);

#define SFTE_IMPL
#ifdef SFTE_IMPL
/*=== stb_truetype.h =========================================================*/
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#ifdef STB_TRUETYPE_IMPLEMENTATION

typedef uint8_t stbtt_uint8;
typedef int8_t stbtt_int8;
typedef uint16_t stbtt_uint16;
typedef int16_t stbtt_int16;
typedef uint32_t stbtt_uint32;
typedef int32_t stbtt_int32;

typedef char stbtt__check_size32[sizeof(stbtt_int32) == 4 ? 1 : -1];
typedef char stbtt__check_size16[sizeof(stbtt_int16) == 2 ? 1 : -1];

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define STBTT_ifloor(x) ((int)floor(x))
#define STBTT_iceil(x) ((int)ceil(x))
#define STBTT_sqrt(x) sqrt(x)
#define STBTT_pow(x, y) pow(x, y)
#define STBTT_fmod(x, y) fmod(x, y)
#define STBTT_cos(x) cos(x)
#define STBTT_acos(x) acos(x)
#define STBTT_fabs(x) fabs(x)

#define STBTT_malloc(x, u) ((void)(u), malloc(x))
#define STBTT_free(x, u) ((void)(u), free(x))
#define STBTT_assert(x) assert(x)
#define STBTT_strlen(x) strlen(x)
#define STBTT_memcpy memcpy
#define STBTT_memset memset
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////
////   INTERFACE
////
////

#ifndef __STB_INCLUDE_STB_TRUETYPE_H__
#define __STB_INCLUDE_STB_TRUETYPE_H__

#define STBTT_DEF static

// private structure
typedef struct {
    unsigned char *data;
    int cursor;
    int size;
} stbtt__buf;

//////////////////////////////////////////////////////////////////////////////
//
// TEXTURE BAKING API
//
// If you use this API, you only have to call two functions ever.
//

typedef struct {
    unsigned short x0, y0, x1, y1;  // coordinates of bbox in bitmap
    float xoff, yoff, xadvance;
} stbtt_bakedchar;

STBTT_DEF int
stbtt_BakeFontBitmap(const unsigned char *data,
                     int offset,          // font location (use offset=0 for plain .ttf)
                     float pixel_height,  // height of font in pixels
                     unsigned char *pixels, int pw, int ph,  // bitmap to be filled in
                     int first_char, int num_chars,          // characters to bake
                     stbtt_bakedchar *chardata);  // you allocate this, it's num_chars long
// if return is positive, the first unused row of the bitmap
// if return is negative, returns the negative of the number of characters that fit
// if return is 0, no characters fit and no rows were used
// This uses a very crappy packing.

typedef struct {
    float x0, y0, s0, t0;  // top-left
    float x1, y1, s1, t1;  // bottom-right
} stbtt_aligned_quad;

//////////////////////////////////////////////////////////////////////////////
//
// NEW TEXTURE BAKING API
//
// This provides options for packing multiple fonts into one atlas, not
// perfectly but better than nothing.

typedef struct {
    unsigned short x0, y0, x1, y1;  // coordinates of bbox in bitmap
    float xoff, yoff, xadvance;
    float xoff2, yoff2;
} stbtt_packedchar;

typedef struct stbtt_pack_context stbtt_pack_context;
typedef struct stbtt_fontinfo stbtt_fontinfo;
#ifndef STB_RECT_PACK_VERSION
typedef struct stbrp_rect stbrp_rect;
#endif

#define STBTT_POINT_SIZE(x) (-(x))

typedef struct {
    float font_size;
    int first_unicode_codepoint_in_range;  // if non-zero, then the chars are continuous, and this
                                           // is the first codepoint
    int *array_of_unicode_codepoints;  // if non-zero, then this is an array of unicode codepoints
    int num_chars;
    stbtt_packedchar *chardata_for_range;      // output
    unsigned char h_oversample, v_oversample;  // don't set these, they're used internally
} stbtt_pack_range;

// this is an opaque structure that you shouldn't mess with which holds
// all the context needed from PackBegin to PackEnd.
struct stbtt_pack_context {
    void *user_allocator_context;
    void *pack_info;
    int width;
    int height;
    int stride_in_bytes;
    int padding;
    int skip_missing;
    unsigned int h_oversample, v_oversample;
    unsigned char *pixels;
    void *nodes;
};

//////////////////////////////////////////////////////////////////////////////
//
// FONT LOADING
//
//

// The following structure is defined publicly so you can declare one on
// the stack or as a global or etc, but you should treat it as opaque.
struct stbtt_fontinfo {
    void *userdata;
    unsigned char *data;  // pointer to .ttf file
    int fontstart;        // offset of start of font

    int numGlyphs;  // number of glyphs, needed for range checking

    int loca, head, glyf, hhea, hmtx, kern, gpos,
        svg;               // table locations as offset from start of .ttf
    int index_map;         // a cmap mapping for our chosen character encoding
    int indexToLocFormat;  // format needed to map from glyph index to glyph

    stbtt__buf cff;          // cff font data
    stbtt__buf charstrings;  // the charstring index
    stbtt__buf gsubrs;       // global charstring subroutines index
    stbtt__buf subrs;        // private charstring subroutines index
    stbtt__buf fontdicts;    // array of font dicts
    stbtt__buf fdselect;     // map from glyph to fontdict
};

STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset);
// Given an offset into the file that defines a font, this function builds
// the necessary cached info for the rest of the system. You must allocate
// the stbtt_fontinfo yourself, and stbtt_InitFont will fill it out. You don't
// need to do anything special to free it, because the contents are pure
// value data with no additional data structures. Returns 0 on failure.

//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER TO GLYPH-INDEX CONVERSIOn

STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint);
// If you're going to perform multiple operations on the same character
// and you want a speed-up, call this function with the character you're
// going to process, then use glyph-based functions instead of the
// codepoint-based functions.
// Returns 0 if the character codepoint is not defined in the font.

//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER PROPERTIES
//

STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels);
// computes a scale factor to produce a font whose "height" is 'pixels' tall.
// Height is measured as the distance from the highest ascender to the lowest
// descender; in other words, it's equivalent to calling stbtt_GetFontVMetrics
// and computing:
//       scale = pixels / (ascent - descent)
// so if you prefer to measure height by the ascent only, use a similar calculation.

STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index,
                                      int *advanceWidth, int *leftSideBearing);
STBTT_DEF int stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0,
                                int *x1, int *y1);
// as above, but takes one or more glyph indices for greater efficiency

typedef struct stbtt_kerningentry {
    int glyph1;  // use stbtt_FindGlyphIndex
    int glyph2;
    int advance;
} stbtt_kerningentry;

//////////////////////////////////////////////////////////////////////////////
//
// GLYPH SHAPES (you probably don't need these, but they have to go before
// the bitmaps for C declaration-order reasons)
//

#ifndef STBTT_vmove  // you can predefine these to use different values (but why?)
enum { STBTT_vmove = 1, STBTT_vline, STBTT_vcurve, STBTT_vcubic };
#endif

#ifndef stbtt_vertex  // you can predefine this to use different values
                      // (we share this with other code at RAD)
#define stbtt_vertex_type                                                                          \
    short  // can't use stbtt_int16 because that's not visible in the header file
typedef struct {
    stbtt_vertex_type x, y, cx, cy, cx1, cy1;
    unsigned char type, padding;
} stbtt_vertex;
#endif

STBTT_DEF int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index,
                                  stbtt_vertex **vertices);
// returns # of vertices and fills *vertices with the pointer to them
//   these are expressed in "unscaled" coordinates
//
// The shape is a series of contours. Each one starts with
// a STBTT_moveto, then consists of a series of mixed
// STBTT_lineto and STBTT_curveto segments. A lineto
// draws a line from previous endpoint to its x,y; a curveto
// draws a quadratic bezier from previous endpoint to
// its x,y, using cx,cy as the bezier control point.

//////////////////////////////////////////////////////////////////////////////
//
// BITMAP RENDERING
//

STBTT_DEF void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w,
                                     int out_h, int out_stride, float scale_x, float scale_y,
                                     int glyph);
STBTT_DEF void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output,
                                             int out_w, int out_h, int out_stride, float scale_x,
                                             float scale_y, float shift_x, float shift_y,
                                             int glyph);
STBTT_DEF void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x,
                                       float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
STBTT_DEF void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x,
                                               float scale_y, float shift_x, float shift_y,
                                               int *ix0, int *iy0, int *ix1, int *iy1);

// @TODO: don't expose this structure
typedef struct {
    int w, h, stride;
    unsigned char *pixels;
} stbtt__bitmap;

// rasterize a shape with quadratic beziers into a bitmap
STBTT_DEF void stbtt_Rasterize(stbtt__bitmap *result,         // 1-channel bitmap to draw into
                               float flatness_in_pixels,      // allowable error of curve in pixels
                               stbtt_vertex *vertices,        // array of vertices defining shape
                               int num_verts,                 // number of vertices in above array
                               float scale_x, float scale_y,  // scale applied to input vertices
                               float shift_x,
                               float shift_y,         // translation applied to input vertices
                               int x_off, int y_off,  // another translation applied to input
                               int invert,            // if non-zero, vertically flip shape
                               void *userdata);       // context for to STBTT_MALLOC

//////////////////////////////////////////////////////////////////////////////
//
// Signed Distance Function (or Field) rendering

//////////////////////////////////////////////////////////////////////////////
//
// Finding the right font...
//
// You should really just solve this offline, keep your own tables
// of what font is what, and don't try to get it out of the .ttf file.
// That's because getting it out of the .ttf file is really hard, because
// the names in the file can appear in many possible encodings, in many
// possible languages, and e.g. if you need a case-insensitive comparison,
// the details of that depend on the encoding & language in a complex way
// (actually underspecified in truetype, but also gigantic).
//
// But you can use the provided functions in two possible ways:
//     stbtt_FindMatchingFont() will use *case-sensitive* comparisons on
//             unicode-encoded names to try to find the font you want;
//             you can run this before calling stbtt_InitFont()
//
//     stbtt_GetFontNameString() lets you get any of the various strings
//             from the file yourself and do your own comparisons on them.
//             You have to have called stbtt_InitFont() first.

#define STBTT_MACSTYLE_DONTCARE 0
#define STBTT_MACSTYLE_BOLD 1
#define STBTT_MACSTYLE_ITALIC 2
#define STBTT_MACSTYLE_UNDERSCORE 4
#define STBTT_MACSTYLE_NONE 8  // <= not same as 0, this makes us check the bitfield is 0

enum {  // platformID
    STBTT_PLATFORM_ID_UNICODE = 0,
    STBTT_PLATFORM_ID_MAC = 1,
    STBTT_PLATFORM_ID_ISO = 2,
    STBTT_PLATFORM_ID_MICROSOFT = 3
};

enum {  // encodingID for STBTT_PLATFORM_ID_UNICODE
    STBTT_UNICODE_EID_UNICODE_1_0 = 0,
    STBTT_UNICODE_EID_UNICODE_1_1 = 1,
    STBTT_UNICODE_EID_ISO_10646 = 2,
    STBTT_UNICODE_EID_UNICODE_2_0_BMP = 3,
    STBTT_UNICODE_EID_UNICODE_2_0_FULL = 4
};

enum {  // encodingID for STBTT_PLATFORM_ID_MICROSOFT
    STBTT_MS_EID_SYMBOL = 0,
    STBTT_MS_EID_UNICODE_BMP = 1,
    STBTT_MS_EID_SHIFTJIS = 2,
    STBTT_MS_EID_UNICODE_FULL = 10
};

enum {  // encodingID for STBTT_PLATFORM_ID_MAC; same as Script Manager codes
    STBTT_MAC_EID_ROMAN = 0,
    STBTT_MAC_EID_ARABIC = 4,
    STBTT_MAC_EID_JAPANESE = 1,
    STBTT_MAC_EID_HEBREW = 5,
    STBTT_MAC_EID_CHINESE_TRAD = 2,
    STBTT_MAC_EID_GREEK = 6,
    STBTT_MAC_EID_KOREAN = 3,
    STBTT_MAC_EID_RUSSIAN = 7
};

enum {  // languageID for STBTT_PLATFORM_ID_MICROSOFT; same as LCID...
        // problematic because there are e.g. 16 english LCIDs and 16 arabic LCIDs
    STBTT_MS_LANG_ENGLISH = 0x0409,
    STBTT_MS_LANG_ITALIAN = 0x0410,
    STBTT_MS_LANG_CHINESE = 0x0804,
    STBTT_MS_LANG_JAPANESE = 0x0411,
    STBTT_MS_LANG_DUTCH = 0x0413,
    STBTT_MS_LANG_KOREAN = 0x0412,
    STBTT_MS_LANG_FRENCH = 0x040c,
    STBTT_MS_LANG_RUSSIAN = 0x0419,
    STBTT_MS_LANG_GERMAN = 0x0407,
    STBTT_MS_LANG_SPANISH = 0x0409,
    STBTT_MS_LANG_HEBREW = 0x040d,
    STBTT_MS_LANG_SWEDISH = 0x041D
};

enum {  // languageID for STBTT_PLATFORM_ID_MAC
    STBTT_MAC_LANG_ENGLISH = 0,
    STBTT_MAC_LANG_JAPANESE = 11,
    STBTT_MAC_LANG_ARABIC = 12,
    STBTT_MAC_LANG_KOREAN = 23,
    STBTT_MAC_LANG_DUTCH = 4,
    STBTT_MAC_LANG_RUSSIAN = 32,
    STBTT_MAC_LANG_FRENCH = 1,
    STBTT_MAC_LANG_SPANISH = 6,
    STBTT_MAC_LANG_GERMAN = 2,
    STBTT_MAC_LANG_SWEDISH = 5,
    STBTT_MAC_LANG_HEBREW = 10,
    STBTT_MAC_LANG_CHINESE_SIMPLIFIED = 33,
    STBTT_MAC_LANG_ITALIAN = 3,
    STBTT_MAC_LANG_CHINESE_TRAD = 19
};

#endif  // __STB_INCLUDE_STB_TRUETYPE_H__

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////
////   IMPLEMENTATION
////
////

#ifdef STB_TRUETYPE_IMPLEMENTATION

#ifndef STBTT_MAX_OVERSAMPLE
#define STBTT_MAX_OVERSAMPLE 8
#endif

#if STBTT_MAX_OVERSAMPLE > 255
#error "STBTT_MAX_OVERSAMPLE cannot be > 255"
#endif

typedef int
    stbtt__test_oversample_pow2[(STBTT_MAX_OVERSAMPLE & (STBTT_MAX_OVERSAMPLE - 1)) == 0 ? 1 : -1];

#ifndef STBTT_RASTERIZER_VERSION
#define STBTT_RASTERIZER_VERSION 2
#endif

#ifdef _MSC_VER
#define STBTT__NOTUSED(v) (void)(v)
#else
#define STBTT__NOTUSED(v) (void)sizeof(v)
#endif

//////////////////////////////////////////////////////////////////////////
//
// stbtt__buf helpers to parse data from file
//

static stbtt_uint8 stbtt__buf_get8(stbtt__buf *b) {
    if (b->cursor >= b->size) return 0;
    return b->data[b->cursor++];
}

static stbtt_uint8 stbtt__buf_peek8(stbtt__buf *b) {
    if (b->cursor >= b->size) return 0;
    return b->data[b->cursor];
}

static void stbtt__buf_seek(stbtt__buf *b, int o) {
    STBTT_assert(!(o > b->size || o < 0));
    b->cursor = (o > b->size || o < 0) ? b->size : o;
}

static void stbtt__buf_skip(stbtt__buf *b, int o) {
    stbtt__buf_seek(b, b->cursor + o);
}

static stbtt_uint32 stbtt__buf_get(stbtt__buf *b, int n) {
    stbtt_uint32 v = 0;
    int i;
    STBTT_assert(n >= 1 && n <= 4);
    for (i = 0; i < n; i++) v = (v << 8) | stbtt__buf_get8(b);
    return v;
}

static stbtt__buf stbtt__new_buf(const void *p, size_t size) {
    stbtt__buf r;
    STBTT_assert(size < 0x40000000);
    r.data = (stbtt_uint8 *)p;
    r.size = (int)size;
    r.cursor = 0;
    return r;
}

#define stbtt__buf_get16(b) stbtt__buf_get((b), 2)
#define stbtt__buf_get32(b) stbtt__buf_get((b), 4)

static stbtt__buf stbtt__buf_range(const stbtt__buf *b, int o, int s) {
    stbtt__buf r = stbtt__new_buf(NULL, 0);
    if (o < 0 || s < 0 || o > b->size || s > b->size - o) return r;
    r.data = b->data + o;
    r.size = s;
    return r;
}

static stbtt__buf stbtt__cff_get_index(stbtt__buf *b) {
    int count, start, offsize;
    start = b->cursor;
    count = stbtt__buf_get16(b);
    if (count) {
        offsize = stbtt__buf_get8(b);
        STBTT_assert(offsize >= 1 && offsize <= 4);
        stbtt__buf_skip(b, offsize * count);
        stbtt__buf_skip(b, stbtt__buf_get(b, offsize) - 1);
    }
    return stbtt__buf_range(b, start, b->cursor - start);
}

static stbtt_uint32 stbtt__cff_int(stbtt__buf *b) {
    int b0 = stbtt__buf_get8(b);
    if (b0 >= 32 && b0 <= 246)
        return b0 - 139;
    else if (b0 >= 247 && b0 <= 250)
        return (b0 - 247) * 256 + stbtt__buf_get8(b) + 108;
    else if (b0 >= 251 && b0 <= 254)
        return -(b0 - 251) * 256 - stbtt__buf_get8(b) - 108;
    else if (b0 == 28)
        return stbtt__buf_get16(b);
    else if (b0 == 29)
        return stbtt__buf_get32(b);
    STBTT_assert(0);
    return 0;
}

static void stbtt__cff_skip_operand(stbtt__buf *b) {
    int v, b0 = stbtt__buf_peek8(b);
    STBTT_assert(b0 >= 28);
    if (b0 == 30) {
        stbtt__buf_skip(b, 1);
        while (b->cursor < b->size) {
            v = stbtt__buf_get8(b);
            if ((v & 0xF) == 0xF || (v >> 4) == 0xF) break;
        }
    } else {
        stbtt__cff_int(b);
    }
}

static stbtt__buf stbtt__dict_get(stbtt__buf *b, int key) {
    stbtt__buf_seek(b, 0);
    while (b->cursor < b->size) {
        int start = b->cursor, end, op;
        while (stbtt__buf_peek8(b) >= 28) stbtt__cff_skip_operand(b);
        end = b->cursor;
        op = stbtt__buf_get8(b);
        if (op == 12) op = stbtt__buf_get8(b) | 0x100;
        if (op == key) return stbtt__buf_range(b, start, end - start);
    }
    return stbtt__buf_range(b, 0, 0);
}

static void stbtt__dict_get_ints(stbtt__buf *b, int key, int outcount, stbtt_uint32 *out) {
    int i;
    stbtt__buf operands = stbtt__dict_get(b, key);
    for (i = 0; i < outcount && operands.cursor < operands.size; i++)
        out[i] = stbtt__cff_int(&operands);
}

static int stbtt__cff_index_count(stbtt__buf *b) {
    stbtt__buf_seek(b, 0);
    return stbtt__buf_get16(b);
}

static stbtt__buf stbtt__cff_index_get(stbtt__buf b, int i) {
    int count, offsize, start, end;
    stbtt__buf_seek(&b, 0);
    count = stbtt__buf_get16(&b);
    offsize = stbtt__buf_get8(&b);
    STBTT_assert(i >= 0 && i < count);
    STBTT_assert(offsize >= 1 && offsize <= 4);
    stbtt__buf_skip(&b, i * offsize);
    start = stbtt__buf_get(&b, offsize);
    end = stbtt__buf_get(&b, offsize);
    return stbtt__buf_range(&b, 2 + (count + 1) * offsize + start, end - start);
}

//////////////////////////////////////////////////////////////////////////
//
// accessors to parse data from file
//

// on platforms that don't allow misaligned reads, if we want to allow
// truetype fonts that aren't padded to alignment, define ALLOW_UNALIGNED_TRUETYPE

#define ttBYTE(p) (*(stbtt_uint8 *)(p))
#define ttCHAR(p) (*(stbtt_int8 *)(p))
#define ttFixed(p) ttLONG(p)

static stbtt_uint16 ttUSHORT(stbtt_uint8 *p) {
    return p[0] * 256 + p[1];
}
static stbtt_int16 ttSHORT(stbtt_uint8 *p) {
    return p[0] * 256 + p[1];
}
static stbtt_uint32 ttULONG(stbtt_uint8 *p) {
    return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}
#define stbtt_tag4(p, c0, c1, c2, c3)                                                              \
    ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define stbtt_tag(p, str) stbtt_tag4(p, str[0], str[1], str[2], str[3])

// @OPTIMIZE: binary search
static stbtt_uint32 stbtt__find_table(stbtt_uint8 *data, stbtt_uint32 fontstart, const char *tag) {
    stbtt_int32 num_tables = ttUSHORT(data + fontstart + 4);
    stbtt_uint32 tabledir = fontstart + 12;
    stbtt_int32 i;
    for (i = 0; i < num_tables; ++i) {
        stbtt_uint32 loc = tabledir + 16 * i;
        if (stbtt_tag(data + loc + 0, tag)) return ttULONG(data + loc + 8);
    }
    return 0;
}

static stbtt__buf stbtt__get_subrs(stbtt__buf cff, stbtt__buf fontdict) {
    stbtt_uint32 subrsoff = 0, private_loc[2] = {0, 0};
    stbtt__buf pdict;
    stbtt__dict_get_ints(&fontdict, 18, 2, private_loc);
    if (!private_loc[1] || !private_loc[0]) return stbtt__new_buf(NULL, 0);
    pdict = stbtt__buf_range(&cff, private_loc[1], private_loc[0]);
    stbtt__dict_get_ints(&pdict, 19, 1, &subrsoff);
    if (!subrsoff) return stbtt__new_buf(NULL, 0);
    stbtt__buf_seek(&cff, private_loc[1] + subrsoff);
    return stbtt__cff_get_index(&cff);
}

static int stbtt_InitFont_internal(stbtt_fontinfo *info, unsigned char *data, int fontstart) {
    stbtt_uint32 cmap, t;
    stbtt_int32 i, numTables;

    info->data = data;
    info->fontstart = fontstart;
    info->cff = stbtt__new_buf(NULL, 0);

    cmap = stbtt__find_table(data, fontstart, "cmap");        // required
    info->loca = stbtt__find_table(data, fontstart, "loca");  // required
    info->head = stbtt__find_table(data, fontstart, "head");  // required
    info->glyf = stbtt__find_table(data, fontstart, "glyf");  // required
    info->hhea = stbtt__find_table(data, fontstart, "hhea");  // required
    info->hmtx = stbtt__find_table(data, fontstart, "hmtx");  // required
    info->kern = stbtt__find_table(data, fontstart, "kern");  // not required
    info->gpos = stbtt__find_table(data, fontstart, "GPOS");  // not required

    if (!cmap || !info->head || !info->hhea || !info->hmtx) return 0;
    if (info->glyf) {
        // required for truetype
        if (!info->loca) return 0;
    } else {
        // initialization for CFF / Type2 fonts (OTF)
        stbtt__buf b, topdict, topdictidx;
        stbtt_uint32 cstype = 2, charstrings = 0, fdarrayoff = 0, fdselectoff = 0;
        stbtt_uint32 cff;

        cff = stbtt__find_table(data, fontstart, "CFF ");
        if (!cff) return 0;

        info->fontdicts = stbtt__new_buf(NULL, 0);
        info->fdselect = stbtt__new_buf(NULL, 0);

        // @TODO this should use size from table (not 512MB)
        info->cff = stbtt__new_buf(data + cff, 512 * 1024 * 1024);
        b = info->cff;

        // read the header
        stbtt__buf_skip(&b, 2);
        stbtt__buf_seek(&b, stbtt__buf_get8(&b));  // hdrsize

        // @TODO the name INDEX could list multiple fonts,
        // but we just use the first one.
        stbtt__cff_get_index(&b);  // name INDEX
        topdictidx = stbtt__cff_get_index(&b);
        topdict = stbtt__cff_index_get(topdictidx, 0);
        stbtt__cff_get_index(&b);  // string INDEX
        info->gsubrs = stbtt__cff_get_index(&b);

        stbtt__dict_get_ints(&topdict, 17, 1, &charstrings);
        stbtt__dict_get_ints(&topdict, 0x100 | 6, 1, &cstype);
        stbtt__dict_get_ints(&topdict, 0x100 | 36, 1, &fdarrayoff);
        stbtt__dict_get_ints(&topdict, 0x100 | 37, 1, &fdselectoff);
        info->subrs = stbtt__get_subrs(b, topdict);

        // we only support Type 2 charstrings
        if (cstype != 2) return 0;
        if (charstrings == 0) return 0;

        if (fdarrayoff) {
            // looks like a CID font
            if (!fdselectoff) return 0;
            stbtt__buf_seek(&b, fdarrayoff);
            info->fontdicts = stbtt__cff_get_index(&b);
            info->fdselect = stbtt__buf_range(&b, fdselectoff, b.size - fdselectoff);
        }

        stbtt__buf_seek(&b, charstrings);
        info->charstrings = stbtt__cff_get_index(&b);
    }

    t = stbtt__find_table(data, fontstart, "maxp");
    if (t)
        info->numGlyphs = ttUSHORT(data + t + 4);
    else
        info->numGlyphs = 0xffff;

    info->svg = -1;

    // find a cmap encoding table we understand *now* to avoid searching
    // later. (todo: could make this installable)
    // the same regardless of glyph.
    numTables = ttUSHORT(data + cmap + 2);
    info->index_map = 0;
    for (i = 0; i < numTables; ++i) {
        stbtt_uint32 encoding_record = cmap + 4 + 8 * i;
        // find an encoding we understand:
        switch (ttUSHORT(data + encoding_record)) {
        case STBTT_PLATFORM_ID_MICROSOFT:
            switch (ttUSHORT(data + encoding_record + 2)) {
            case STBTT_MS_EID_UNICODE_BMP:
            case STBTT_MS_EID_UNICODE_FULL:
                // MS/Unicode
                info->index_map = cmap + ttULONG(data + encoding_record + 4);
                break;
            }
            break;
        case STBTT_PLATFORM_ID_UNICODE:
            // Mac/iOS has these
            // all the encodingIDs are unicode, so we don't bother to check it
            info->index_map = cmap + ttULONG(data + encoding_record + 4);
            break;
        }
    }
    if (info->index_map == 0) return 0;

    info->indexToLocFormat = ttUSHORT(data + info->head + 50);
    return 1;
}

STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint) {
    stbtt_uint8 *data = info->data;
    stbtt_uint32 index_map = info->index_map;

    stbtt_uint16 format = ttUSHORT(data + index_map + 0);
    if (format == 0) {  // apple byte encoding
        stbtt_int32 bytes = ttUSHORT(data + index_map + 2);
        if (unicode_codepoint < bytes - 6) return ttBYTE(data + index_map + 6 + unicode_codepoint);
        return 0;
    } else if (format == 6) {
        stbtt_uint32 first = ttUSHORT(data + index_map + 6);
        stbtt_uint32 count = ttUSHORT(data + index_map + 8);
        if ((stbtt_uint32)unicode_codepoint >= first &&
            (stbtt_uint32)unicode_codepoint < first + count)
            return ttUSHORT(data + index_map + 10 + (unicode_codepoint - first) * 2);
        return 0;
    } else if (format == 2) {
        STBTT_assert(0);  // @TODO: high-byte mapping for japanese/chinese/korean
        return 0;
    } else if (format ==
               4) {  // standard mapping for windows fonts: binary search collection of ranges
        stbtt_uint16 segcount = ttUSHORT(data + index_map + 6) >> 1;
        stbtt_uint16 searchRange = ttUSHORT(data + index_map + 8) >> 1;
        stbtt_uint16 entrySelector = ttUSHORT(data + index_map + 10);
        stbtt_uint16 rangeShift = ttUSHORT(data + index_map + 12) >> 1;

        // do a binary search of the segments
        stbtt_uint32 endCount = index_map + 14;
        stbtt_uint32 search = endCount;

        if (unicode_codepoint > 0xffff) return 0;

        // they lie from endCount .. endCount + segCount
        // but searchRange is the nearest power of two, so...
        if (unicode_codepoint >= ttUSHORT(data + search + rangeShift * 2)) search += rangeShift * 2;

        // now decrement to bias correctly to find smallest
        search -= 2;
        while (entrySelector) {
            stbtt_uint16 end;
            searchRange >>= 1;
            end = ttUSHORT(data + search + searchRange * 2);
            if (unicode_codepoint > end) search += searchRange * 2;
            --entrySelector;
        }
        search += 2;

        {
            stbtt_uint16 offset, start, last;
            stbtt_uint16 item = (stbtt_uint16)((search - endCount) >> 1);

            start = ttUSHORT(data + index_map + 14 + segcount * 2 + 2 + 2 * item);
            last = ttUSHORT(data + endCount + 2 * item);
            if (unicode_codepoint < start || unicode_codepoint > last) return 0;

            offset = ttUSHORT(data + index_map + 14 + segcount * 6 + 2 + 2 * item);
            if (offset == 0)
                return (stbtt_uint16)(unicode_codepoint +
                                      ttSHORT(data + index_map + 14 + segcount * 4 + 2 + 2 * item));

            return ttUSHORT(data + offset + (unicode_codepoint - start) * 2 + index_map + 14 +
                            segcount * 6 + 2 + 2 * item);
        }
    } else if (format == 12 || format == 13) {
        stbtt_uint32 ngroups = ttULONG(data + index_map + 12);
        stbtt_int32 low, high;
        low = 0;
        high = (stbtt_int32)ngroups;
        // Binary search the right group.
        while (low < high) {
            stbtt_int32 mid = low + ((high - low) >> 1);  // rounds down, so low <= mid < high
            stbtt_uint32 start_char = ttULONG(data + index_map + 16 + mid * 12);
            stbtt_uint32 end_char = ttULONG(data + index_map + 16 + mid * 12 + 4);
            if ((stbtt_uint32)unicode_codepoint < start_char)
                high = mid;
            else if ((stbtt_uint32)unicode_codepoint > end_char)
                low = mid + 1;
            else {
                stbtt_uint32 start_glyph = ttULONG(data + index_map + 16 + mid * 12 + 8);
                if (format == 12)
                    return start_glyph + unicode_codepoint - start_char;
                else  // format == 13
                    return start_glyph;
            }
        }
        return 0;  // not found
    }
    // @TODO
    STBTT_assert(0);
    return 0;
}

static void stbtt_setvertex(stbtt_vertex *v, stbtt_uint8 type, stbtt_int32 x, stbtt_int32 y,
                            stbtt_int32 cx, stbtt_int32 cy) {
    v->type = type;
    v->x = (stbtt_int16)x;
    v->y = (stbtt_int16)y;
    v->cx = (stbtt_int16)cx;
    v->cy = (stbtt_int16)cy;
}

static int stbtt__GetGlyfOffset(const stbtt_fontinfo *info, int glyph_index) {
    int g1, g2;

    STBTT_assert(!info->cff.size);

    if (glyph_index >= info->numGlyphs) return -1;  // glyph index out of range
    if (info->indexToLocFormat >= 2) return -1;     // unknown index->glyph map format

    if (info->indexToLocFormat == 0) {
        g1 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2) * 2;
        g2 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2 + 2) * 2;
    } else {
        g1 = info->glyf + ttULONG(info->data + info->loca + glyph_index * 4);
        g2 = info->glyf + ttULONG(info->data + info->loca + glyph_index * 4 + 4);
    }

    return g1 == g2 ? -1 : g1;  // if length is 0, return -1
}

static int stbtt__GetGlyphInfoT2(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0,
                                 int *x1, int *y1);

STBTT_DEF int stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0,
                                int *x1, int *y1) {
    if (info->cff.size) {
        stbtt__GetGlyphInfoT2(info, glyph_index, x0, y0, x1, y1);
    } else {
        int g = stbtt__GetGlyfOffset(info, glyph_index);
        if (g < 0) return 0;

        if (x0) *x0 = ttSHORT(info->data + g + 2);
        if (y0) *y0 = ttSHORT(info->data + g + 4);
        if (x1) *x1 = ttSHORT(info->data + g + 6);
        if (y1) *y1 = ttSHORT(info->data + g + 8);
    }
    return 1;
}

static int stbtt__close_shape(stbtt_vertex *vertices, int num_vertices, int was_off, int start_off,
                              stbtt_int32 sx, stbtt_int32 sy, stbtt_int32 scx, stbtt_int32 scy,
                              stbtt_int32 cx, stbtt_int32 cy) {
    if (start_off) {
        if (was_off)
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx + scx) >> 1,
                            (cy + scy) >> 1, cx, cy);
        stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, sx, sy, scx, scy);
    } else {
        if (was_off)
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, sx, sy, cx, cy);
        else
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vline, sx, sy, 0, 0);
    }
    return num_vertices;
}

static int stbtt__GetGlyphShapeTT(const stbtt_fontinfo *info, int glyph_index,
                                  stbtt_vertex **pvertices) {
    stbtt_int16 numberOfContours;
    stbtt_uint8 *endPtsOfContours;
    stbtt_uint8 *data = info->data;
    stbtt_vertex *vertices = 0;
    int num_vertices = 0;
    int g = stbtt__GetGlyfOffset(info, glyph_index);

    *pvertices = NULL;

    if (g < 0) return 0;

    numberOfContours = ttSHORT(data + g);

    if (numberOfContours > 0) {
        stbtt_uint8 flags = 0, flagcount;
        stbtt_int32 ins, i, j = 0, m, n, next_move, was_off = 0, off, start_off = 0;
        stbtt_int32 x, y, cx, cy, sx, sy, scx, scy;
        stbtt_uint8 *points;
        endPtsOfContours = (data + g + 10);
        ins = ttUSHORT(data + g + 10 + numberOfContours * 2);
        points = data + g + 10 + numberOfContours * 2 + 2 + ins;

        n = 1 + ttUSHORT(endPtsOfContours + numberOfContours * 2 - 2);

        m = n + 2 * numberOfContours;  // a loose bound on how many vertices we might need
        vertices = (stbtt_vertex *)STBTT_malloc(m * sizeof(vertices[0]), info->userdata);
        if (vertices == 0) return 0;

        next_move = 0;
        flagcount = 0;

        // in first pass, we load uninterpreted data into the allocated array
        // above, shifted to the end of the array so we won't overwrite it when
        // we create our final data starting from the front

        off = m - n;  // starting offset for uninterpreted data, regardless of how m ends up being
                      // calculated

        // first load flags

        for (i = 0; i < n; ++i) {
            if (flagcount == 0) {
                flags = *points++;
                if (flags & 8) flagcount = *points++;
            } else
                --flagcount;
            vertices[off + i].type = flags;
        }

        // now load x coordinates
        x = 0;
        for (i = 0; i < n; ++i) {
            flags = vertices[off + i].type;
            if (flags & 2) {
                stbtt_int16 dx = *points++;
                x += (flags & 16) ? dx : -dx;  // ???
            } else {
                if (!(flags & 16)) {
                    x = x + (stbtt_int16)(points[0] * 256 + points[1]);
                    points += 2;
                }
            }
            vertices[off + i].x = (stbtt_int16)x;
        }

        // now load y coordinates
        y = 0;
        for (i = 0; i < n; ++i) {
            flags = vertices[off + i].type;
            if (flags & 4) {
                stbtt_int16 dy = *points++;
                y += (flags & 32) ? dy : -dy;  // ???
            } else {
                if (!(flags & 32)) {
                    y = y + (stbtt_int16)(points[0] * 256 + points[1]);
                    points += 2;
                }
            }
            vertices[off + i].y = (stbtt_int16)y;
        }

        // now convert them to our format
        num_vertices = 0;
        sx = sy = cx = cy = scx = scy = 0;
        for (i = 0; i < n; ++i) {
            flags = vertices[off + i].type;
            x = (stbtt_int16)vertices[off + i].x;
            y = (stbtt_int16)vertices[off + i].y;

            if (next_move == i) {
                if (i != 0)
                    num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off,
                                                      sx, sy, scx, scy, cx, cy);

                // now start the new one
                start_off = !(flags & 1);
                if (start_off) {
                    // if we start off with an off-curve point, then when we need to find a point on
                    // the curve where we can start, and we need to save some state for when we
                    // wraparound.
                    scx = x;
                    scy = y;
                    if (!(vertices[off + i + 1].type & 1)) {
                        // next point is also a curve point, so interpolate an on-point curve
                        sx = (x + (stbtt_int32)vertices[off + i + 1].x) >> 1;
                        sy = (y + (stbtt_int32)vertices[off + i + 1].y) >> 1;
                    } else {
                        // otherwise just use the next point as our start point
                        sx = (stbtt_int32)vertices[off + i + 1].x;
                        sy = (stbtt_int32)vertices[off + i + 1].y;
                        ++i;  // we're using point i+1 as the starting point, so skip it
                    }
                } else {
                    sx = x;
                    sy = y;
                }
                stbtt_setvertex(&vertices[num_vertices++], STBTT_vmove, sx, sy, 0, 0);
                was_off = 0;
                next_move = 1 + ttUSHORT(endPtsOfContours + j * 2);
                ++j;
            } else {
                if (!(flags & 1)) {  // if it's a curve
                    if (was_off)     // two off-curve control points in a row means interpolate an
                                     // on-curve midpoint
                        stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx + x) >> 1,
                                        (cy + y) >> 1, cx, cy);
                    cx = x;
                    cy = y;
                    was_off = 1;
                } else {
                    if (was_off)
                        stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, x, y, cx, cy);
                    else
                        stbtt_setvertex(&vertices[num_vertices++], STBTT_vline, x, y, 0, 0);
                    was_off = 0;
                }
            }
        }
        num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx, sy, scx,
                                          scy, cx, cy);
    } else if (numberOfContours < 0) {
        // Compound shapes.
        int more = 1;
        stbtt_uint8 *comp = data + g + 10;
        num_vertices = 0;
        vertices = 0;
        while (more) {
            stbtt_uint16 flags, gidx;
            int comp_num_verts = 0, i;
            stbtt_vertex *comp_verts = 0, *tmp = 0;
            float mtx[6] = {1, 0, 0, 1, 0, 0}, m, n;

            flags = ttSHORT(comp);
            comp += 2;
            gidx = ttSHORT(comp);
            comp += 2;

            if (flags & 2) {      // XY values
                if (flags & 1) {  // shorts
                    mtx[4] = ttSHORT(comp);
                    comp += 2;
                    mtx[5] = ttSHORT(comp);
                    comp += 2;
                } else {
                    mtx[4] = ttCHAR(comp);
                    comp += 1;
                    mtx[5] = ttCHAR(comp);
                    comp += 1;
                }
            } else {
                // @TODO handle matching point
                STBTT_assert(0);
            }
            if (flags & (1 << 3)) {  // WE_HAVE_A_SCALE
                mtx[0] = mtx[3] = ttSHORT(comp) / 16384.0f;
                comp += 2;
                mtx[1] = mtx[2] = 0;
            } else if (flags & (1 << 6)) {  // WE_HAVE_AN_X_AND_YSCALE
                mtx[0] = ttSHORT(comp) / 16384.0f;
                comp += 2;
                mtx[1] = mtx[2] = 0;
                mtx[3] = ttSHORT(comp) / 16384.0f;
                comp += 2;
            } else if (flags & (1 << 7)) {  // WE_HAVE_A_TWO_BY_TWO
                mtx[0] = ttSHORT(comp) / 16384.0f;
                comp += 2;
                mtx[1] = ttSHORT(comp) / 16384.0f;
                comp += 2;
                mtx[2] = ttSHORT(comp) / 16384.0f;
                comp += 2;
                mtx[3] = ttSHORT(comp) / 16384.0f;
                comp += 2;
            }

            // Find transformation scales.
            m = (float)STBTT_sqrt(mtx[0] * mtx[0] + mtx[1] * mtx[1]);
            n = (float)STBTT_sqrt(mtx[2] * mtx[2] + mtx[3] * mtx[3]);

            // Get indexed glyph.
            comp_num_verts = stbtt_GetGlyphShape(info, gidx, &comp_verts);
            if (comp_num_verts > 0) {
                // Transform vertices.
                for (i = 0; i < comp_num_verts; ++i) {
                    stbtt_vertex *v = &comp_verts[i];
                    stbtt_vertex_type x, y;
                    x = v->x;
                    y = v->y;
                    v->x = (stbtt_vertex_type)(m * (mtx[0] * x + mtx[2] * y + mtx[4]));
                    v->y = (stbtt_vertex_type)(n * (mtx[1] * x + mtx[3] * y + mtx[5]));
                    x = v->cx;
                    y = v->cy;
                    v->cx = (stbtt_vertex_type)(m * (mtx[0] * x + mtx[2] * y + mtx[4]));
                    v->cy = (stbtt_vertex_type)(n * (mtx[1] * x + mtx[3] * y + mtx[5]));
                }
                // Append vertices.
                tmp = (stbtt_vertex *)STBTT_malloc(
                    (num_vertices + comp_num_verts) * sizeof(stbtt_vertex), info->userdata);
                if (!tmp) {
                    if (vertices) STBTT_free(vertices, info->userdata);
                    if (comp_verts) STBTT_free(comp_verts, info->userdata);
                    return 0;
                }
                if (num_vertices > 0 && vertices)
                    STBTT_memcpy(tmp, vertices, num_vertices * sizeof(stbtt_vertex));
                STBTT_memcpy(tmp + num_vertices, comp_verts, comp_num_verts * sizeof(stbtt_vertex));
                if (vertices) STBTT_free(vertices, info->userdata);
                vertices = tmp;
                STBTT_free(comp_verts, info->userdata);
                num_vertices += comp_num_verts;
            }
            // More components ?
            more = flags & (1 << 5);
        }
    } else {
        // numberOfCounters == 0, do nothing
    }

    *pvertices = vertices;
    return num_vertices;
}

typedef struct {
    int bounds;
    int started;
    float first_x, first_y;
    float x, y;
    stbtt_int32 min_x, max_x, min_y, max_y;

    stbtt_vertex *pvertices;
    int num_vertices;
} stbtt__csctx;

#define STBTT__CSCTX_INIT(bounds) {bounds, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0}

static void stbtt__track_vertex(stbtt__csctx *c, stbtt_int32 x, stbtt_int32 y) {
    if (x > c->max_x || !c->started) c->max_x = x;
    if (y > c->max_y || !c->started) c->max_y = y;
    if (x < c->min_x || !c->started) c->min_x = x;
    if (y < c->min_y || !c->started) c->min_y = y;
    c->started = 1;
}

static void stbtt__csctx_v(stbtt__csctx *c, stbtt_uint8 type, stbtt_int32 x, stbtt_int32 y,
                           stbtt_int32 cx, stbtt_int32 cy, stbtt_int32 cx1, stbtt_int32 cy1) {
    if (c->bounds) {
        stbtt__track_vertex(c, x, y);
        if (type == STBTT_vcubic) {
            stbtt__track_vertex(c, cx, cy);
            stbtt__track_vertex(c, cx1, cy1);
        }
    } else {
        stbtt_setvertex(&c->pvertices[c->num_vertices], type, x, y, cx, cy);
        c->pvertices[c->num_vertices].cx1 = (stbtt_int16)cx1;
        c->pvertices[c->num_vertices].cy1 = (stbtt_int16)cy1;
    }
    c->num_vertices++;
}

static void stbtt__csctx_close_shape(stbtt__csctx *ctx) {
    if (ctx->first_x != ctx->x || ctx->first_y != ctx->y)
        stbtt__csctx_v(ctx, STBTT_vline, (int)ctx->first_x, (int)ctx->first_y, 0, 0, 0, 0);
}

static void stbtt__csctx_rmove_to(stbtt__csctx *ctx, float dx, float dy) {
    stbtt__csctx_close_shape(ctx);
    ctx->first_x = ctx->x = ctx->x + dx;
    ctx->first_y = ctx->y = ctx->y + dy;
    stbtt__csctx_v(ctx, STBTT_vmove, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void stbtt__csctx_rline_to(stbtt__csctx *ctx, float dx, float dy) {
    ctx->x += dx;
    ctx->y += dy;
    stbtt__csctx_v(ctx, STBTT_vline, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void stbtt__csctx_rccurve_to(stbtt__csctx *ctx, float dx1, float dy1, float dx2, float dy2,
                                    float dx3, float dy3) {
    float cx1 = ctx->x + dx1;
    float cy1 = ctx->y + dy1;
    float cx2 = cx1 + dx2;
    float cy2 = cy1 + dy2;
    ctx->x = cx2 + dx3;
    ctx->y = cy2 + dy3;
    stbtt__csctx_v(ctx, STBTT_vcubic, (int)ctx->x, (int)ctx->y, (int)cx1, (int)cy1, (int)cx2,
                   (int)cy2);
}

static stbtt__buf stbtt__get_subr(stbtt__buf idx, int n) {
    int count = stbtt__cff_index_count(&idx);
    int bias = 107;
    if (count >= 33900)
        bias = 32768;
    else if (count >= 1240)
        bias = 1131;
    n += bias;
    if (n < 0 || n >= count) return stbtt__new_buf(NULL, 0);
    return stbtt__cff_index_get(idx, n);
}

static stbtt__buf stbtt__cid_get_glyph_subrs(const stbtt_fontinfo *info, int glyph_index) {
    stbtt__buf fdselect = info->fdselect;
    int nranges, start, end, v, fmt, fdselector = -1, i;

    stbtt__buf_seek(&fdselect, 0);
    fmt = stbtt__buf_get8(&fdselect);
    if (fmt == 0) {
        // untested
        stbtt__buf_skip(&fdselect, glyph_index);
        fdselector = stbtt__buf_get8(&fdselect);
    } else if (fmt == 3) {
        nranges = stbtt__buf_get16(&fdselect);
        start = stbtt__buf_get16(&fdselect);
        for (i = 0; i < nranges; i++) {
            v = stbtt__buf_get8(&fdselect);
            end = stbtt__buf_get16(&fdselect);
            if (glyph_index >= start && glyph_index < end) {
                fdselector = v;
                break;
            }
            start = end;
        }
    }
    if (fdselector == -1) stbtt__new_buf(NULL, 0);
    return stbtt__get_subrs(info->cff, stbtt__cff_index_get(info->fontdicts, fdselector));
}

static int stbtt__run_charstring(const stbtt_fontinfo *info, int glyph_index, stbtt__csctx *c) {
    int in_header = 1, maskbits = 0, subr_stack_height = 0, sp = 0, v, i, b0;
    int has_subrs = 0, clear_stack;
    float s[48];
    stbtt__buf subr_stack[10], subrs = info->subrs, b;
    float f;

#define STBTT__CSERR(s) (0)

    // this currently ignores the initial width value, which isn't needed if we have hmtx
    b = stbtt__cff_index_get(info->charstrings, glyph_index);
    while (b.cursor < b.size) {
        i = 0;
        clear_stack = 1;
        b0 = stbtt__buf_get8(&b);
        switch (b0) {
        // @TODO implement hinting
        case 0x13:                                // hintmask
        case 0x14:                                // cntrmask
            if (in_header) maskbits += (sp / 2);  // implicit "vstem"
            in_header = 0;
            stbtt__buf_skip(&b, (maskbits + 7) / 8);
            break;

        case 0x01:  // hstem
        case 0x03:  // vstem
        case 0x12:  // hstemhm
        case 0x17:  // vstemhm
            maskbits += (sp / 2);
            break;

        case 0x15:  // rmoveto
            in_header = 0;
            if (sp < 2) return STBTT__CSERR("rmoveto stack");
            stbtt__csctx_rmove_to(c, s[sp - 2], s[sp - 1]);
            break;
        case 0x04:  // vmoveto
            in_header = 0;
            if (sp < 1) return STBTT__CSERR("vmoveto stack");
            stbtt__csctx_rmove_to(c, 0, s[sp - 1]);
            break;
        case 0x16:  // hmoveto
            in_header = 0;
            if (sp < 1) return STBTT__CSERR("hmoveto stack");
            stbtt__csctx_rmove_to(c, s[sp - 1], 0);
            break;

        case 0x05:  // rlineto
            if (sp < 2) return STBTT__CSERR("rlineto stack");
            for (; i + 1 < sp; i += 2) stbtt__csctx_rline_to(c, s[i], s[i + 1]);
            break;

            // hlineto/vlineto and vhcurveto/hvcurveto alternate horizontal and vertical
            // starting from a different place.

        case 0x07:  // vlineto
            if (sp < 1) return STBTT__CSERR("vlineto stack");
            goto vlineto;
        case 0x06:  // hlineto
            if (sp < 1) return STBTT__CSERR("hlineto stack");
            for (;;) {
                if (i >= sp) break;
                stbtt__csctx_rline_to(c, s[i], 0);
                i++;
            vlineto:
                if (i >= sp) break;
                stbtt__csctx_rline_to(c, 0, s[i]);
                i++;
            }
            break;

        case 0x1F:  // hvcurveto
            if (sp < 4) return STBTT__CSERR("hvcurveto stack");
            goto hvcurveto;
        case 0x1E:  // vhcurveto
            if (sp < 4) return STBTT__CSERR("vhcurveto stack");
            for (;;) {
                if (i + 3 >= sp) break;
                stbtt__csctx_rccurve_to(c, 0, s[i], s[i + 1], s[i + 2], s[i + 3],
                                        (sp - i == 5) ? s[i + 4] : 0.0f);
                i += 4;
            hvcurveto:
                if (i + 3 >= sp) break;
                stbtt__csctx_rccurve_to(c, s[i], 0, s[i + 1], s[i + 2],
                                        (sp - i == 5) ? s[i + 4] : 0.0f, s[i + 3]);
                i += 4;
            }
            break;

        case 0x08:  // rrcurveto
            if (sp < 6) return STBTT__CSERR("rcurveline stack");
            for (; i + 5 < sp; i += 6)
                stbtt__csctx_rccurve_to(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            break;

        case 0x18:  // rcurveline
            if (sp < 8) return STBTT__CSERR("rcurveline stack");
            for (; i + 5 < sp - 2; i += 6)
                stbtt__csctx_rccurve_to(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            if (i + 1 >= sp) return STBTT__CSERR("rcurveline stack");
            stbtt__csctx_rline_to(c, s[i], s[i + 1]);
            break;

        case 0x19:  // rlinecurve
            if (sp < 8) return STBTT__CSERR("rlinecurve stack");
            for (; i + 1 < sp - 6; i += 2) stbtt__csctx_rline_to(c, s[i], s[i + 1]);
            if (i + 5 >= sp) return STBTT__CSERR("rlinecurve stack");
            stbtt__csctx_rccurve_to(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            break;

        case 0x1A:  // vvcurveto
        case 0x1B:  // hhcurveto
            if (sp < 4) return STBTT__CSERR("(vv|hh)curveto stack");
            f = 0.0;
            if (sp & 1) {
                f = s[i];
                i++;
            }
            for (; i + 3 < sp; i += 4) {
                if (b0 == 0x1B)
                    stbtt__csctx_rccurve_to(c, s[i], f, s[i + 1], s[i + 2], s[i + 3], 0.0);
                else
                    stbtt__csctx_rccurve_to(c, f, s[i], s[i + 1], s[i + 2], 0.0, s[i + 3]);
                f = 0.0;
            }
            break;

        case 0x0A:  // callsubr
            if (!has_subrs) {
                if (info->fdselect.size) subrs = stbtt__cid_get_glyph_subrs(info, glyph_index);
                has_subrs = 1;
            }
            // FALLTHROUGH
        case 0x1D:  // callgsubr
            if (sp < 1) return STBTT__CSERR("call(g|)subr stack");
            v = (int)s[--sp];
            if (subr_stack_height >= 10) return STBTT__CSERR("recursion limit");
            subr_stack[subr_stack_height++] = b;
            b = stbtt__get_subr(b0 == 0x0A ? subrs : info->gsubrs, v);
            if (b.size == 0) return STBTT__CSERR("subr not found");
            b.cursor = 0;
            clear_stack = 0;
            break;

        case 0x0B:  // return
            if (subr_stack_height <= 0) return STBTT__CSERR("return outside subr");
            b = subr_stack[--subr_stack_height];
            clear_stack = 0;
            break;

        case 0x0E:  // endchar
            stbtt__csctx_close_shape(c);
            return 1;

        case 0x0C: {  // two-byte escape
            float dx1, dx2, dx3, dx4, dx5, dx6, dy1, dy2, dy3, dy4, dy5, dy6;
            float dx, dy;
            int b1 = stbtt__buf_get8(&b);
            switch (b1) {
            // @TODO These "flex" implementations ignore the flex-depth and resolution,
            // and always draw beziers.
            case 0x22:  // hflex
                if (sp < 7) return STBTT__CSERR("hflex stack");
                dx1 = s[0];
                dx2 = s[1];
                dy2 = s[2];
                dx3 = s[3];
                dx4 = s[4];
                dx5 = s[5];
                dx6 = s[6];
                stbtt__csctx_rccurve_to(c, dx1, 0, dx2, dy2, dx3, 0);
                stbtt__csctx_rccurve_to(c, dx4, 0, dx5, -dy2, dx6, 0);
                break;

            case 0x23:  // flex
                if (sp < 13) return STBTT__CSERR("flex stack");
                dx1 = s[0];
                dy1 = s[1];
                dx2 = s[2];
                dy2 = s[3];
                dx3 = s[4];
                dy3 = s[5];
                dx4 = s[6];
                dy4 = s[7];
                dx5 = s[8];
                dy5 = s[9];
                dx6 = s[10];
                dy6 = s[11];
                // fd is s[12]
                stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
                stbtt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
                break;

            case 0x24:  // hflex1
                if (sp < 9) return STBTT__CSERR("hflex1 stack");
                dx1 = s[0];
                dy1 = s[1];
                dx2 = s[2];
                dy2 = s[3];
                dx3 = s[4];
                dx4 = s[5];
                dx5 = s[6];
                dy5 = s[7];
                dx6 = s[8];
                stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, 0);
                stbtt__csctx_rccurve_to(c, dx4, 0, dx5, dy5, dx6, -(dy1 + dy2 + dy5));
                break;

            case 0x25:  // flex1
                if (sp < 11) return STBTT__CSERR("flex1 stack");
                dx1 = s[0];
                dy1 = s[1];
                dx2 = s[2];
                dy2 = s[3];
                dx3 = s[4];
                dy3 = s[5];
                dx4 = s[6];
                dy4 = s[7];
                dx5 = s[8];
                dy5 = s[9];
                dx6 = dy6 = s[10];
                dx = dx1 + dx2 + dx3 + dx4 + dx5;
                dy = dy1 + dy2 + dy3 + dy4 + dy5;
                if (STBTT_fabs(dx) > STBTT_fabs(dy))
                    dy6 = -dy;
                else
                    dx6 = -dx;
                stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
                stbtt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
                break;

            default: return STBTT__CSERR("unimplemented");
            }
        } break;

        default:
            if (b0 != 255 && b0 != 28 && b0 < 32) return STBTT__CSERR("reserved operator");

            // push immediate
            if (b0 == 255) {
                f = (float)(stbtt_int32)stbtt__buf_get32(&b) / 0x10000;
            } else {
                stbtt__buf_skip(&b, -1);
                f = (float)(stbtt_int16)stbtt__cff_int(&b);
            }
            if (sp >= 48) return STBTT__CSERR("push stack overflow");
            s[sp++] = f;
            clear_stack = 0;
            break;
        }
        if (clear_stack) sp = 0;
    }
    return STBTT__CSERR("no endchar");

#undef STBTT__CSERR
}

static int stbtt__GetGlyphShapeT2(const stbtt_fontinfo *info, int glyph_index,
                                  stbtt_vertex **pvertices) {
    // runs the charstring twice, once to count and once to output (to avoid realloc)
    stbtt__csctx count_ctx = STBTT__CSCTX_INIT(1);
    stbtt__csctx output_ctx = STBTT__CSCTX_INIT(0);
    if (stbtt__run_charstring(info, glyph_index, &count_ctx)) {
        *pvertices = (stbtt_vertex *)STBTT_malloc(count_ctx.num_vertices * sizeof(stbtt_vertex),
                                                  info->userdata);
        output_ctx.pvertices = *pvertices;
        if (stbtt__run_charstring(info, glyph_index, &output_ctx)) {
            STBTT_assert(output_ctx.num_vertices == count_ctx.num_vertices);
            return output_ctx.num_vertices;
        }
    }
    *pvertices = NULL;
    return 0;
}

static int stbtt__GetGlyphInfoT2(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0,
                                 int *x1, int *y1) {
    stbtt__csctx c = STBTT__CSCTX_INIT(1);
    int r = stbtt__run_charstring(info, glyph_index, &c);
    if (x0) *x0 = r ? c.min_x : 0;
    if (y0) *y0 = r ? c.min_y : 0;
    if (x1) *x1 = r ? c.max_x : 0;
    if (y1) *y1 = r ? c.max_y : 0;
    return r ? c.num_vertices : 0;
}

STBTT_DEF int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index,
                                  stbtt_vertex **pvertices) {
    if (!info->cff.size)
        return stbtt__GetGlyphShapeTT(info, glyph_index, pvertices);
    else
        return stbtt__GetGlyphShapeT2(info, glyph_index, pvertices);
}

STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index,
                                      int *advanceWidth, int *leftSideBearing) {
    stbtt_uint16 numOfLongHorMetrics = ttUSHORT(info->data + info->hhea + 34);
    if (glyph_index < numOfLongHorMetrics) {
        if (advanceWidth) *advanceWidth = ttSHORT(info->data + info->hmtx + 4 * glyph_index);
        if (leftSideBearing)
            *leftSideBearing = ttSHORT(info->data + info->hmtx + 4 * glyph_index + 2);
    } else {
        if (advanceWidth)
            *advanceWidth = ttSHORT(info->data + info->hmtx + 4 * (numOfLongHorMetrics - 1));
        if (leftSideBearing)
            *leftSideBearing = ttSHORT(info->data + info->hmtx + 4 * numOfLongHorMetrics +
                                       2 * (glyph_index - numOfLongHorMetrics));
    }
}

// Define to STBTT_assert(x) if you want to break on unimplemented formats.
#define STBTT_GPOS_TODO_assert(x)

STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float height) {
    int fheight = ttSHORT(info->data + info->hhea + 4) - ttSHORT(info->data + info->hhea + 6);
    return (float)height / fheight;
}

//////////////////////////////////////////////////////////////////////////////
//
// antialiasing software rasterizer
//

STBTT_DEF void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x,
                                               float scale_y, float shift_x, float shift_y,
                                               int *ix0, int *iy0, int *ix1, int *iy1) {
    int x0 = 0, y0 = 0, x1, y1;  // =0 suppresses compiler warning
    if (!stbtt_GetGlyphBox(font, glyph, &x0, &y0, &x1, &y1)) {
        // e.g. space character
        if (ix0) *ix0 = 0;
        if (iy0) *iy0 = 0;
        if (ix1) *ix1 = 0;
        if (iy1) *iy1 = 0;
    } else {
        // move to integral bboxes (treating pixels as little squares, what pixels get touched)?
        if (ix0) *ix0 = STBTT_ifloor(x0 * scale_x + shift_x);
        if (iy0) *iy0 = STBTT_ifloor(-y1 * scale_y + shift_y);
        if (ix1) *ix1 = STBTT_iceil(x1 * scale_x + shift_x);
        if (iy1) *iy1 = STBTT_iceil(-y0 * scale_y + shift_y);
    }
}

STBTT_DEF void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x,
                                       float scale_y, int *ix0, int *iy0, int *ix1, int *iy1) {
    stbtt_GetGlyphBitmapBoxSubpixel(font, glyph, scale_x, scale_y, 0.0f, 0.0f, ix0, iy0, ix1, iy1);
}

//////////////////////////////////////////////////////////////////////////////
//
//  Rasterizer

typedef struct stbtt__hheap_chunk {
    struct stbtt__hheap_chunk *next;
} stbtt__hheap_chunk;

typedef struct stbtt__hheap {
    struct stbtt__hheap_chunk *head;
    void *first_free;
    int num_remaining_in_head_chunk;
} stbtt__hheap;

static void *stbtt__hheap_alloc(stbtt__hheap *hh, size_t size, void *userdata) {
    if (hh->first_free) {
        void *p = hh->first_free;
        hh->first_free = *(void **)p;
        return p;
    } else {
        if (hh->num_remaining_in_head_chunk == 0) {
            int count = (size < 32 ? 2000 : size < 128 ? 800 : 100);
            stbtt__hheap_chunk *c = (stbtt__hheap_chunk *)STBTT_malloc(
                sizeof(stbtt__hheap_chunk) + size * count, userdata);
            if (c == NULL) return NULL;
            c->next = hh->head;
            hh->head = c;
            hh->num_remaining_in_head_chunk = count;
        }
        --hh->num_remaining_in_head_chunk;
        return (char *)(hh->head) + sizeof(stbtt__hheap_chunk) +
               size * hh->num_remaining_in_head_chunk;
    }
}

static void stbtt__hheap_free(stbtt__hheap *hh, void *p) {
    *(void **)p = hh->first_free;
    hh->first_free = p;
}

static void stbtt__hheap_cleanup(stbtt__hheap *hh, void *userdata) {
    stbtt__hheap_chunk *c = hh->head;
    while (c) {
        stbtt__hheap_chunk *n = c->next;
        STBTT_free(c, userdata);
        c = n;
    }
}

typedef struct stbtt__edge {
    float x0, y0, x1, y1;
    int invert;
} stbtt__edge;

typedef struct stbtt__active_edge {
    struct stbtt__active_edge *next;
#if STBTT_RASTERIZER_VERSION == 1
    int x, dx;
    float ey;
    int direction;
#elif STBTT_RASTERIZER_VERSION == 2
    float fx, fdx, fdy;
    float direction;
    float sy;
    float ey;
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif
} stbtt__active_edge;

#if STBTT_RASTERIZER_VERSION == 1
#define STBTT_FIXSHIFT 10
#define STBTT_FIX (1 << STBTT_FIXSHIFT)
#define STBTT_FIXMASK (STBTT_FIX - 1)

static stbtt__active_edge *stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x,
                                             float start_point, void *userdata) {
    stbtt__active_edge *z = (stbtt__active_edge *)stbtt__hheap_alloc(hh, sizeof(*z), userdata);
    float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
    STBTT_assert(z != NULL);
    if (!z) return z;

    // round dx down to avoid overshooting
    if (dxdy < 0)
        z->dx = -STBTT_ifloor(STBTT_FIX * -dxdy);
    else
        z->dx = STBTT_ifloor(STBTT_FIX * dxdy);

    z->x = STBTT_ifloor(
        STBTT_FIX * e->x0 +
        z->dx *
            (start_point - e->y0));  // use z->dx so when we offset later it's by the same amount
    z->x -= off_x * STBTT_FIX;

    z->ey = e->y1;
    z->next = 0;
    z->direction = e->invert ? 1 : -1;
    return z;
}
#elif STBTT_RASTERIZER_VERSION == 2
static stbtt__active_edge *stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x,
                                             float start_point, void *userdata) {
    stbtt__active_edge *z = (stbtt__active_edge *)stbtt__hheap_alloc(hh, sizeof(*z), userdata);
    float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
    STBTT_assert(z != NULL);
    // STBTT_assert(e->y0 <= start_point);
    if (!z) return z;
    z->fdx = dxdy;
    z->fdy = dxdy != 0.0f ? (1.0f / dxdy) : 0.0f;
    z->fx = e->x0 + dxdy * (start_point - e->y0);
    z->fx -= off_x;
    z->direction = e->invert ? 1.0f : -1.0f;
    z->sy = e->y0;
    z->ey = e->y1;
    z->next = 0;
    return z;
}
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif

#if STBTT_RASTERIZER_VERSION == 1
// note: this routine clips fills that extend off the edges... ideally this
// wouldn't happen, but it could happen if the truetype glyph bounding boxes
// are wrong, or if the user supplies a too-small bitmap
static void stbtt__fill_active_edges(unsigned char *scanline, int len, stbtt__active_edge *e,
                                     int max_weight) {
    // non-zero winding fill
    int x0 = 0, w = 0;

    while (e) {
        if (w == 0) {
            // if we're currently at zero, we need to record the edge start point
            x0 = e->x;
            w += e->direction;
        } else {
            int x1 = e->x;
            w += e->direction;
            // if we went to zero, we need to draw
            if (w == 0) {
                int i = x0 >> STBTT_FIXSHIFT;
                int j = x1 >> STBTT_FIXSHIFT;

                if (i < len && j >= 0) {
                    if (i == j) {
                        // x0,x1 are the same pixel, so compute combined coverage
                        scanline[i] = scanline[i] +
                                      (stbtt_uint8)((x1 - x0) * max_weight >> STBTT_FIXSHIFT);
                    } else {
                        if (i >= 0)  // add antialiasing for x0
                            scanline[i] = scanline[i] +
                                          (stbtt_uint8)(((STBTT_FIX - (x0 & STBTT_FIXMASK)) *
                                                         max_weight) >>
                                                        STBTT_FIXSHIFT);
                        else
                            i = -1;  // clip

                        if (j < len)  // add antialiasing for x1
                            scanline[j] = scanline[j] +
                                          (stbtt_uint8)(((x1 & STBTT_FIXMASK) * max_weight) >>
                                                        STBTT_FIXSHIFT);
                        else
                            j = len;  // clip

                        for (++i; i < j; ++i)  // fill pixels between x0 and x1
                            scanline[i] = scanline[i] + (stbtt_uint8)max_weight;
                    }
                }
            }
        }

        e = e->next;
    }
}

static void stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e, int n,
                                          int vsubsample, int off_x, int off_y, void *userdata) {
    stbtt__hheap hh = {0, 0, 0};
    stbtt__active_edge *active = NULL;
    int y, j = 0;
    int max_weight = (255 / vsubsample);  // weight per vertical scanline
    int s;                                // vertical subsample index
    unsigned char scanline_data[512], *scanline;

    if (result->w > 512)
        scanline = (unsigned char *)STBTT_malloc(result->w, userdata);
    else
        scanline = scanline_data;

    y = off_y * vsubsample;
    e[n].y0 = (off_y + result->h) * (float)vsubsample + 1;

    while (j < result->h) {
        STBTT_memset(scanline, 0, result->w);
        for (s = 0; s < vsubsample; ++s) {
            // find center of pixel for this scanline
            float scan_y = y + 0.5f;
            stbtt__active_edge **step = &active;

            // update all active edges;
            // remove all active edges that terminate before the center of this scanline
            while (*step) {
                stbtt__active_edge *z = *step;
                if (z->ey <= scan_y) {
                    *step = z->next;  // delete from list
                    STBTT_assert(z->direction);
                    z->direction = 0;
                    stbtt__hheap_free(&hh, z);
                } else {
                    z->x += z->dx;            // advance to position for current scanline
                    step = &((*step)->next);  // advance through list
                }
            }

            // resort the list if needed
            for (;;) {
                int changed = 0;
                step = &active;
                while (*step && (*step)->next) {
                    if ((*step)->x > (*step)->next->x) {
                        stbtt__active_edge *t = *step;
                        stbtt__active_edge *q = t->next;

                        t->next = q->next;
                        q->next = t;
                        *step = q;
                        changed = 1;
                    }
                    step = &(*step)->next;
                }
                if (!changed) break;
            }

            // insert all edges that start before the center of this scanline -- omit ones that also
            // end on this scanline
            while (e->y0 <= scan_y) {
                if (e->y1 > scan_y) {
                    stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y, userdata);
                    if (z != NULL) {
                        // find insertion point
                        if (active == NULL)
                            active = z;
                        else if (z->x < active->x) {
                            // insert at front
                            z->next = active;
                            active = z;
                        } else {
                            // find thing to insert AFTER
                            stbtt__active_edge *p = active;
                            while (p->next && p->next->x < z->x) p = p->next;
                            // at this point, p->next->x is NOT < z->x
                            z->next = p->next;
                            p->next = z;
                        }
                    }
                }
                ++e;
            }

            // now process all active edges in XOR fashion
            if (active) stbtt__fill_active_edges(scanline, result->w, active, max_weight);

            ++y;
        }
        STBTT_memcpy(result->pixels + j * result->stride, scanline, result->w);
        ++j;
    }

    stbtt__hheap_cleanup(&hh, userdata);

    if (scanline != scanline_data) STBTT_free(scanline, userdata);
}

#elif STBTT_RASTERIZER_VERSION == 2

// the edge passed in here does not cross the vertical line at x or the vertical line at x+1
// (i.e. it has already been clipped to those)
static void stbtt__handle_clipped_edge(float *scanline, int x, stbtt__active_edge *e, float x0,
                                       float y0, float x1, float y1) {
    if (y0 == y1) return;
    STBTT_assert(y0 < y1);
    STBTT_assert(e->sy <= e->ey);
    if (y0 > e->ey) return;
    if (y1 < e->sy) return;
    if (y0 < e->sy) {
        x0 += (x1 - x0) * (e->sy - y0) / (y1 - y0);
        y0 = e->sy;
    }
    if (y1 > e->ey) {
        x1 += (x1 - x0) * (e->ey - y1) / (y1 - y0);
        y1 = e->ey;
    }

    if (x0 == x)
        STBTT_assert(x1 <= x + 1);
    else if (x0 == x + 1)
        STBTT_assert(x1 >= x);
    else if (x0 <= x)
        STBTT_assert(x1 <= x);
    else if (x0 >= x + 1)
        STBTT_assert(x1 >= x + 1);
    else
        STBTT_assert(x1 >= x && x1 <= x + 1);

    if (x0 <= x && x1 <= x)
        scanline[x] += e->direction * (y1 - y0);
    else if (x0 >= x + 1 && x1 >= x + 1)
        ;
    else {
        STBTT_assert(x0 >= x && x0 <= x + 1 && x1 >= x && x1 <= x + 1);
        scanline[x] += e->direction * (y1 - y0) *
                       (1 - ((x0 - x) + (x1 - x)) / 2);  // coverage = 1 - average x position
    }
}

static float stbtt__sized_trapezoid_area(float height, float top_width, float bottom_width) {
    STBTT_assert(top_width >= 0);
    STBTT_assert(bottom_width >= 0);
    return (top_width + bottom_width) / 2.0f * height;
}

static float stbtt__position_trapezoid_area(float height, float tx0, float tx1, float bx0,
                                            float bx1) {
    return stbtt__sized_trapezoid_area(height, tx1 - tx0, bx1 - bx0);
}

static float stbtt__sized_triangle_area(float height, float width) {
    return height * width / 2;
}

static void stbtt__fill_active_edges_new(float *scanline, float *scanline_fill, int len,
                                         stbtt__active_edge *e, float y_top) {
    float y_bottom = y_top + 1;

    while (e) {
        // brute force every pixel

        // compute intersection points with top & bottom
        STBTT_assert(e->ey >= y_top);

        if (e->fdx == 0) {
            float x0 = e->fx;
            if (x0 < len) {
                if (x0 >= 0) {
                    stbtt__handle_clipped_edge(scanline, (int)x0, e, x0, y_top, x0, y_bottom);
                    stbtt__handle_clipped_edge(scanline_fill - 1, (int)x0 + 1, e, x0, y_top, x0,
                                               y_bottom);
                } else {
                    stbtt__handle_clipped_edge(scanline_fill - 1, 0, e, x0, y_top, x0, y_bottom);
                }
            }
        } else {
            float x0 = e->fx;
            float dx = e->fdx;
            float xb = x0 + dx;
            float x_top, x_bottom;
            float sy0, sy1;
            float dy = e->fdy;
            STBTT_assert(e->sy <= y_bottom && e->ey >= y_top);

            // compute endpoints of line segment clipped to this scanline (if the
            // line segment starts on this scanline. x0 is the intersection of the
            // line with y_top, but that may be off the line segment.
            if (e->sy > y_top) {
                x_top = x0 + dx * (e->sy - y_top);
                sy0 = e->sy;
            } else {
                x_top = x0;
                sy0 = y_top;
            }
            if (e->ey < y_bottom) {
                x_bottom = x0 + dx * (e->ey - y_top);
                sy1 = e->ey;
            } else {
                x_bottom = xb;
                sy1 = y_bottom;
            }

            if (x_top >= 0 && x_bottom >= 0 && x_top < len && x_bottom < len) {
                // from here on, we don't have to range check x values

                if ((int)x_top == (int)x_bottom) {
                    float height;
                    // simple case, only spans one pixel
                    int x = (int)x_top;
                    height = (sy1 - sy0) * e->direction;
                    STBTT_assert(x >= 0 && x < len);
                    scanline[x] += stbtt__position_trapezoid_area(height, x_top, x + 1.0f, x_bottom,
                                                                  x + 1.0f);
                    scanline_fill[x] += height;  // everything right of this pixel is filled
                } else {
                    int x, x1, x2;
                    float y_crossing, y_final, step, sign, area;
                    // covers 2+ pixels
                    if (x_top > x_bottom) {
                        // flip scanline vertically; signed area is the same
                        float t;
                        sy0 = y_bottom - (sy0 - y_top);
                        sy1 = y_bottom - (sy1 - y_top);
                        t = sy0, sy0 = sy1, sy1 = t;
                        t = x_bottom, x_bottom = x_top, x_top = t;
                        dx = -dx;
                        dy = -dy;
                        t = x0, x0 = xb, xb = t;
                    }
                    STBTT_assert(dy >= 0);
                    STBTT_assert(dx >= 0);

                    x1 = (int)x_top;
                    x2 = (int)x_bottom;
                    // compute intersection with y axis at x1+1
                    y_crossing = y_top + dy * (x1 + 1 - x0);

                    // compute intersection with y axis at x2
                    y_final = y_top + dy * (x2 - x0);

                    //           x1    x_top                            x2    x_bottom
                    //     y_top  +------|-----+------------+------------+--------|---+------------+
                    //            |            |            |            |            |            |
                    //            |            |            |            |            |            |
                    //       sy0  |      Txxxxx|............|............|............|............|
                    // y_crossing |            *xxxxx.......|............|............|............|
                    //            |            |     xxxxx..|............|............|............|
                    //            |            |     /-   xx*xxxx........|............|............|
                    //            |            | dy <       |    xxxxxx..|............|............|
                    //   y_final  |            |     \-     |          xx*xxx.........|............|
                    //       sy1  |            |            |            |   xxxxxB...|............|
                    //            |            |            |            |            |            |
                    //            |            |            |            |            |            |
                    //  y_bottom  +------------+------------+------------+------------+------------+
                    //
                    // goal is to measure the area covered by '.' in each pixel

                    // if x2 is right at the right edge of x1, y_crossing can blow up, github #1057
                    // @TODO: maybe test against sy1 rather than y_bottom?
                    if (y_crossing > y_bottom) y_crossing = y_bottom;

                    sign = e->direction;

                    // area of the rectangle covered from sy0..y_crossing
                    area = sign * (y_crossing - sy0);

                    // area of the triangle (x_top,sy0), (x1+1,sy0), (x1+1,y_crossing)
                    scanline[x1] += stbtt__sized_triangle_area(area, x1 + 1 - x_top);

                    // check if final y_crossing is blown up; no test case for this
                    if (y_final > y_bottom) {
                        y_final = y_bottom;
                        dy = (y_final - y_crossing) /
                             (x2 -
                              (x1 +
                               1));  // if denom=0, y_final = y_crossing, so y_final <= y_bottom
                    }

                    // in second pixel, area covered by line segment found in first pixel
                    // is always a rectangle 1 wide * the height of that line segment; this
                    // is exactly what the variable 'area' stores. it also gets a contribution
                    // from the line segment within it. the THIRD pixel will get the first
                    // pixel's rectangle contribution, the second pixel's rectangle contribution,
                    // and its own contribution. the 'own contribution' is the same in every pixel
                    // except the leftmost and rightmost, a trapezoid that slides down in each
                    // pixel. the second pixel's contribution to the third pixel will be the
                    // rectangle 1 wide times the height change in the second pixel, which is dy.

                    step = sign * dy * 1;  // dy is dy/dx, change in y for every 1 change in x,
                    // which multiplied by 1-pixel-width is how much pixel area changes for each
                    // step in x so the area advances by 'step' every time

                    for (x = x1 + 1; x < x2; ++x) {
                        scanline[x] += area + step / 2;  // area of trapezoid is 1*step/2
                        area += step;
                    }
                    STBTT_assert(
                        STBTT_fabs(area) <=
                        1.01f);  // accumulated error from area += step unless we round step down
                    STBTT_assert(sy1 > y_final - 0.01f);

                    // area covered in the last pixel is the rectangle from all the pixels to the
                    // left, plus the trapezoid filled by the line segment in this pixel all the way
                    // to the right edge
                    scanline[x2] += area + sign * stbtt__position_trapezoid_area(
                                                      sy1 - y_final, (float)x2, x2 + 1.0f, x_bottom,
                                                      x2 + 1.0f);

                    // the rest of the line is filled based on the total height of the line segment
                    // in this pixel
                    scanline_fill[x2] += sign * (sy1 - sy0);
                }
            } else {
                // if edge goes outside of box we're drawing, we require
                // clipping logic. since this does not match the intended use
                // of this library, we use a different, very slow brute
                // force implementation
                // note though that this does happen some of the time because
                // x_top and x_bottom can be extrapolated at the top & bottom of
                // the shape and actually lie outside the bounding box
                int x;
                for (x = 0; x < len; ++x) {
                    // cases:
                    //
                    // there can be up to two intersections with the pixel. any intersection
                    // with left or right edges can be handled by splitting into two (or three)
                    // regions. intersections with top & bottom do not necessitate case-wise logic.
                    //
                    // the old way of doing this found the intersections with the left & right
                    // edges, then used some simple logic to produce up to three segments in sorted
                    // order from top-to-bottom. however, this had a problem: if an x edge was
                    // epsilon across the x border, then the corresponding y position might not be
                    // distinct from the other y segment, and it might ignored as an empty segment.
                    // to avoid that, we need to explicitly produce segments based on x positions.

                    // rename variables to clearly-defined pairs
                    float y0 = y_top;
                    float x1 = (float)(x);
                    float x2 = (float)(x + 1);
                    float x3 = xb;
                    float y3 = y_bottom;

                    // x = e->x + e->dx * (y-y_top)
                    // (y-y_top) = (x - e->x) / e->dx
                    // y = (x - e->x) / e->dx + y_top
                    float y1 = (x - x0) / dx + y_top;
                    float y2 = (x + 1 - x0) / dx + y_top;

                    if (x0 < x1 && x3 > x2) {  // three segments descending down-right
                        stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
                        stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x2, y2);
                        stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
                    } else if (x3 < x1 && x0 > x2) {  // three segments descending down-left
                        stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
                        stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x1, y1);
                        stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
                    } else if (x0 < x1 && x3 > x1) {  // two segments across x, down-right
                        stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
                        stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
                    } else if (x3 < x1 && x0 > x1) {  // two segments across x, down-left
                        stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
                        stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
                    } else if (x0 < x2 && x3 > x2) {  // two segments across x+1, down-right
                        stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
                        stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
                    } else if (x3 < x2 && x0 > x2) {  // two segments across x+1, down-left
                        stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
                        stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
                    } else {  // one segment
                        stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x3, y3);
                    }
                }
            }
        }
        e = e->next;
    }
}

// directly AA rasterize edges w/o supersampling
static void stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e, int n,
                                          int vsubsample, int off_x, int off_y, void *userdata) {
    stbtt__hheap hh = {0, 0, 0};
    stbtt__active_edge *active = NULL;
    int y, j = 0, i;
    float scanline_data[129], *scanline, *scanline2;

    STBTT__NOTUSED(vsubsample);

    if (result->w > 64)
        scanline = (float *)STBTT_malloc((result->w * 2 + 1) * sizeof(float), userdata);
    else
        scanline = scanline_data;

    scanline2 = scanline + result->w;

    y = off_y;
    e[n].y0 = (float)(off_y + result->h) + 1;

    while (j < result->h) {
        // find center of pixel for this scanline
        float scan_y_top = y + 0.0f;
        float scan_y_bottom = y + 1.0f;
        stbtt__active_edge **step = &active;

        STBTT_memset(scanline, 0, result->w * sizeof(scanline[0]));
        STBTT_memset(scanline2, 0, (result->w + 1) * sizeof(scanline[0]));

        // update all active edges;
        // remove all active edges that terminate before the top of this scanline
        while (*step) {
            stbtt__active_edge *z = *step;
            if (z->ey <= scan_y_top) {
                *step = z->next;  // delete from list
                STBTT_assert(z->direction);
                z->direction = 0;
                stbtt__hheap_free(&hh, z);
            } else {
                step = &((*step)->next);  // advance through list
            }
        }

        // insert all edges that start before the bottom of this scanline
        while (e->y0 <= scan_y_bottom) {
            if (e->y0 != e->y1) {
                stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y_top, userdata);
                if (z != NULL) {
                    if (j == 0 && off_y != 0) {
                        if (z->ey < scan_y_top) {
                            // this can happen due to subpixel positioning and some kind of fp
                            // rounding error i think
                            z->ey = scan_y_top;
                        }
                    }
                    STBTT_assert(z->ey >= scan_y_top);  // if we get really unlucky a tiny bit of an
                                                        // edge can be out of bounds
                    // insert at front
                    z->next = active;
                    active = z;
                }
            }
            ++e;
        }

        // now process all active edges
        if (active)
            stbtt__fill_active_edges_new(scanline, scanline2 + 1, result->w, active, scan_y_top);

        {
            float sum = 0;
            for (i = 0; i < result->w; ++i) {
                float k;
                int m;
                sum += scanline2[i];
                k = scanline[i] + sum;
                k = (float)STBTT_fabs(k) * 255 + 0.5f;
                m = (int)k;
                if (m > 255) m = 255;
                result->pixels[j * result->stride + i] = (unsigned char)m;
            }
        }
        // advance all the edges
        step = &active;
        while (*step) {
            stbtt__active_edge *z = *step;
            z->fx += z->fdx;          // advance to position for current scanline
            step = &((*step)->next);  // advance through list
        }

        ++y;
        ++j;
    }

    stbtt__hheap_cleanup(&hh, userdata);

    if (scanline != scanline_data) STBTT_free(scanline, userdata);
}
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif

#define STBTT__COMPARE(a, b) ((a)->y0 < (b)->y0)

static void stbtt__sort_edges_ins_sort(stbtt__edge *p, int n) {
    int i, j;
    for (i = 1; i < n; ++i) {
        stbtt__edge t = p[i], *a = &t;
        j = i;
        while (j > 0) {
            stbtt__edge *b = &p[j - 1];
            int c = STBTT__COMPARE(a, b);
            if (!c) break;
            p[j] = p[j - 1];
            --j;
        }
        if (i != j) p[j] = t;
    }
}

static void stbtt__sort_edges_quicksort(stbtt__edge *p, int n) {
    /* threshold for transitioning to insertion sort */
    while (n > 12) {
        stbtt__edge t;
        int c01, c12, c, m, i, j;

        /* compute median of three */
        m = n >> 1;
        c01 = STBTT__COMPARE(&p[0], &p[m]);
        c12 = STBTT__COMPARE(&p[m], &p[n - 1]);
        /* if 0 >= mid >= end, or 0 < mid < end, then use mid */
        if (c01 != c12) {
            /* otherwise, we'll need to swap something else to middle */
            int z;
            c = STBTT__COMPARE(&p[0], &p[n - 1]);
            /* 0>mid && mid<n:  0>n => n; 0<n => 0 */
            /* 0<mid && mid>n:  0>n => 0; 0<n => n */
            z = (c == c12) ? 0 : n - 1;
            t = p[z];
            p[z] = p[m];
            p[m] = t;
        }
        /* now p[m] is the median-of-three */
        /* swap it to the beginning so it won't move around */
        t = p[0];
        p[0] = p[m];
        p[m] = t;

        /* partition loop */
        i = 1;
        j = n - 1;
        for (;;) {
            /* handling of equality is crucial here */
            /* for sentinels & efficiency with duplicates */
            for (;; ++i) {
                if (!STBTT__COMPARE(&p[i], &p[0])) break;
            }
            for (;; --j) {
                if (!STBTT__COMPARE(&p[0], &p[j])) break;
            }
            /* make sure we haven't crossed */
            if (i >= j) break;
            t = p[i];
            p[i] = p[j];
            p[j] = t;

            ++i;
            --j;
        }
        /* recurse on smaller side, iterate on larger */
        if (j < (n - i)) {
            stbtt__sort_edges_quicksort(p, j);
            p = p + i;
            n = n - i;
        } else {
            stbtt__sort_edges_quicksort(p + i, n - i);
            n = j;
        }
    }
}

static void stbtt__sort_edges(stbtt__edge *p, int n) {
    stbtt__sort_edges_quicksort(p, n);
    stbtt__sort_edges_ins_sort(p, n);
}

typedef struct {
    float x, y;
} stbtt__point;

static void stbtt__rasterize(stbtt__bitmap *result, stbtt__point *pts, int *wcount, int windings,
                             float scale_x, float scale_y, float shift_x, float shift_y, int off_x,
                             int off_y, int invert, void *userdata) {
    float y_scale_inv = invert ? -scale_y : scale_y;
    stbtt__edge *e;
    int n, i, j, k, m;
#if STBTT_RASTERIZER_VERSION == 1
    int vsubsample = result->h < 8 ? 15 : 5;
#elif STBTT_RASTERIZER_VERSION == 2
    int vsubsample = 1;
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif
    // vsubsample should divide 255 evenly; otherwise we won't reach full opacity

    // now we have to blow out the windings into explicit edge lists
    n = 0;
    for (i = 0; i < windings; ++i) n += wcount[i];

    e = (stbtt__edge *)STBTT_malloc(sizeof(*e) * (n + 1),
                                    userdata);  // add an extra one as a sentinel
    if (e == 0) return;
    n = 0;

    m = 0;
    for (i = 0; i < windings; ++i) {
        stbtt__point *p = pts + m;
        m += wcount[i];
        j = wcount[i] - 1;
        for (k = 0; k < wcount[i]; j = k++) {
            int a = k, b = j;
            // skip the edge if horizontal
            if (p[j].y == p[k].y) continue;
            // add edge from j to k to the list
            e[n].invert = 0;
            if (invert ? p[j].y > p[k].y : p[j].y < p[k].y) {
                e[n].invert = 1;
                a = j, b = k;
            }
            e[n].x0 = p[a].x * scale_x + shift_x;
            e[n].y0 = (p[a].y * y_scale_inv + shift_y) * vsubsample;
            e[n].x1 = p[b].x * scale_x + shift_x;
            e[n].y1 = (p[b].y * y_scale_inv + shift_y) * vsubsample;
            ++n;
        }
    }

    // now sort the edges by their highest point (should snap to integer, and then by x)
    // STBTT_sort(e, n, sizeof(e[0]), stbtt__edge_compare);
    stbtt__sort_edges(e, n);

    // now, traverse the scanlines and find the intersections on each scanline, use xor winding rule
    stbtt__rasterize_sorted_edges(result, e, n, vsubsample, off_x, off_y, userdata);

    STBTT_free(e, userdata);
}

static void stbtt__add_point(stbtt__point *points, int n, float x, float y) {
    if (!points) return;  // during first pass, it's unallocated
    points[n].x = x;
    points[n].y = y;
}

// tessellate until threshold p is happy... @TODO warped to compensate for non-linear stretching
static int stbtt__tesselate_curve(stbtt__point *points, int *num_points, float x0, float y0,
                                  float x1, float y1, float x2, float y2,
                                  float objspace_flatness_squared, int n) {
    // midpoint
    float mx = (x0 + 2 * x1 + x2) / 4;
    float my = (y0 + 2 * y1 + y2) / 4;
    // versus directly drawn line
    float dx = (x0 + x2) / 2 - mx;
    float dy = (y0 + y2) / 2 - my;
    if (n > 16)  // 65536 segments on one curve better be enough!
        return 1;
    if (dx * dx + dy * dy >
        objspace_flatness_squared) {  // half-pixel error allowed... need to be smaller if AA
        stbtt__tesselate_curve(points, num_points, x0, y0, (x0 + x1) / 2.0f, (y0 + y1) / 2.0f, mx,
                               my, objspace_flatness_squared, n + 1);
        stbtt__tesselate_curve(points, num_points, mx, my, (x1 + x2) / 2.0f, (y1 + y2) / 2.0f, x2,
                               y2, objspace_flatness_squared, n + 1);
    } else {
        stbtt__add_point(points, *num_points, x2, y2);
        *num_points = *num_points + 1;
    }
    return 1;
}

static void stbtt__tesselate_cubic(stbtt__point *points, int *num_points, float x0, float y0,
                                   float x1, float y1, float x2, float y2, float x3, float y3,
                                   float objspace_flatness_squared, int n) {
    // @TODO this "flatness" calculation is just made-up nonsense that seems to work well enough
    float dx0 = x1 - x0;
    float dy0 = y1 - y0;
    float dx1 = x2 - x1;
    float dy1 = y2 - y1;
    float dx2 = x3 - x2;
    float dy2 = y3 - y2;
    float dx = x3 - x0;
    float dy = y3 - y0;
    float longlen = (float)(STBTT_sqrt(dx0 * dx0 + dy0 * dy0) + STBTT_sqrt(dx1 * dx1 + dy1 * dy1) +
                            STBTT_sqrt(dx2 * dx2 + dy2 * dy2));
    float shortlen = (float)STBTT_sqrt(dx * dx + dy * dy);
    float flatness_squared = longlen * longlen - shortlen * shortlen;

    if (n > 16)  // 65536 segments on one curve better be enough!
        return;

    if (flatness_squared > objspace_flatness_squared) {
        float x01 = (x0 + x1) / 2;
        float y01 = (y0 + y1) / 2;
        float x12 = (x1 + x2) / 2;
        float y12 = (y1 + y2) / 2;
        float x23 = (x2 + x3) / 2;
        float y23 = (y2 + y3) / 2;

        float xa = (x01 + x12) / 2;
        float ya = (y01 + y12) / 2;
        float xb = (x12 + x23) / 2;
        float yb = (y12 + y23) / 2;

        float mx = (xa + xb) / 2;
        float my = (ya + yb) / 2;

        stbtt__tesselate_cubic(points, num_points, x0, y0, x01, y01, xa, ya, mx, my,
                               objspace_flatness_squared, n + 1);
        stbtt__tesselate_cubic(points, num_points, mx, my, xb, yb, x23, y23, x3, y3,
                               objspace_flatness_squared, n + 1);
    } else {
        stbtt__add_point(points, *num_points, x3, y3);
        *num_points = *num_points + 1;
    }
}

// returns number of contours
static stbtt__point *stbtt_FlattenCurves(stbtt_vertex *vertices, int num_verts,
                                         float objspace_flatness, int **contour_lengths,
                                         int *num_contours, void *userdata) {
    stbtt__point *points = 0;
    int num_points = 0;

    float objspace_flatness_squared = objspace_flatness * objspace_flatness;
    int i, n = 0, start = 0, pass;

    // count how many "moves" there are to get the contour count
    for (i = 0; i < num_verts; ++i)
        if (vertices[i].type == STBTT_vmove) ++n;

    *num_contours = n;
    if (n == 0) return 0;

    *contour_lengths = (int *)STBTT_malloc(sizeof(**contour_lengths) * n, userdata);

    if (*contour_lengths == 0) {
        *num_contours = 0;
        return 0;
    }

    // make two passes through the points so we don't need to realloc
    for (pass = 0; pass < 2; ++pass) {
        float x = 0, y = 0;
        if (pass == 1) {
            points = (stbtt__point *)STBTT_malloc(num_points * sizeof(points[0]), userdata);
            if (points == NULL) goto error;
        }
        num_points = 0;
        n = -1;
        for (i = 0; i < num_verts; ++i) {
            switch (vertices[i].type) {
            case STBTT_vmove:
                // start the next contour
                if (n >= 0) (*contour_lengths)[n] = num_points - start;
                ++n;
                start = num_points;

                x = vertices[i].x, y = vertices[i].y;
                stbtt__add_point(points, num_points++, x, y);
                break;
            case STBTT_vline:
                x = vertices[i].x, y = vertices[i].y;
                stbtt__add_point(points, num_points++, x, y);
                break;
            case STBTT_vcurve:
                stbtt__tesselate_curve(points, &num_points, x, y, vertices[i].cx, vertices[i].cy,
                                       vertices[i].x, vertices[i].y, objspace_flatness_squared, 0);
                x = vertices[i].x, y = vertices[i].y;
                break;
            case STBTT_vcubic:
                stbtt__tesselate_cubic(points, &num_points, x, y, vertices[i].cx, vertices[i].cy,
                                       vertices[i].cx1, vertices[i].cy1, vertices[i].x,
                                       vertices[i].y, objspace_flatness_squared, 0);
                x = vertices[i].x, y = vertices[i].y;
                break;
            }
        }
        (*contour_lengths)[n] = num_points - start;
    }

    return points;
error:
    STBTT_free(points, userdata);
    STBTT_free(*contour_lengths, userdata);
    *contour_lengths = 0;
    *num_contours = 0;
    return NULL;
}

STBTT_DEF void stbtt_Rasterize(stbtt__bitmap *result, float flatness_in_pixels,
                               stbtt_vertex *vertices, int num_verts, float scale_x, float scale_y,
                               float shift_x, float shift_y, int x_off, int y_off, int invert,
                               void *userdata) {
    float scale = scale_x > scale_y ? scale_y : scale_x;
    int winding_count = 0;
    int *winding_lengths = NULL;
    stbtt__point *windings = stbtt_FlattenCurves(vertices, num_verts, flatness_in_pixels / scale,
                                                 &winding_lengths, &winding_count, userdata);
    if (windings) {
        stbtt__rasterize(result, windings, winding_lengths, winding_count, scale_x, scale_y,
                         shift_x, shift_y, x_off, y_off, invert, userdata);
        STBTT_free(winding_lengths, userdata);
        STBTT_free(windings, userdata);
    }
}

STBTT_DEF void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output,
                                             int out_w, int out_h, int out_stride, float scale_x,
                                             float scale_y, float shift_x, float shift_y,
                                             int glyph) {
    int ix0, iy0;
    stbtt_vertex *vertices;
    int num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);
    stbtt__bitmap gbm;

    stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0, &iy0, 0,
                                    0);
    gbm.pixels = output;
    gbm.w = out_w;
    gbm.h = out_h;
    gbm.stride = out_stride;

    if (gbm.w && gbm.h)
        stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0,
                        iy0, 1, info->userdata);

    STBTT_free(vertices, info->userdata);
}

STBTT_DEF void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w,
                                     int out_h, int out_stride, float scale_x, float scale_y,
                                     int glyph) {
    stbtt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f,
                                  0.0f, glyph);
}

//////////////////////////////////////////////////////////////////////////////
//
// bitmap baking
//
// This is SUPER-CRAPPY packing to keep source code small

static int stbtt_BakeFontBitmap_internal(unsigned char *data,
                                         int offset,  // font location (use offset=0 for plain .ttf)
                                         float pixel_height,  // height of font in pixels
                                         unsigned char *pixels, int pw,
                                         int ph,                         // bitmap to be filled in
                                         int first_char, int num_chars,  // characters to bake
                                         stbtt_bakedchar *chardata) {
    float scale;
    int x, y, bottom_y, i;
    stbtt_fontinfo f;
    f.userdata = NULL;
    if (!stbtt_InitFont(&f, data, offset)) return -1;
    STBTT_memset(pixels, 0, pw * ph);  // background of 0 around pixels
    x = y = 1;
    bottom_y = 1;

    scale = stbtt_ScaleForPixelHeight(&f, pixel_height);

    for (i = 0; i < num_chars; ++i) {
        int advance, lsb, x0, y0, x1, y1, gw, gh;
        int g = stbtt_FindGlyphIndex(&f, first_char + i);
        stbtt_GetGlyphHMetrics(&f, g, &advance, &lsb);
        stbtt_GetGlyphBitmapBox(&f, g, scale, scale, &x0, &y0, &x1, &y1);
        gw = x1 - x0;
        gh = y1 - y0;
        if (x + gw + 1 >= pw) y = bottom_y, x = 1;  // advance to next row
        if (y + gh + 1 >= ph)  // check if it fits vertically AFTER potentially moving to next row
            return -i;
        STBTT_assert(x + gw < pw);
        STBTT_assert(y + gh < ph);
        stbtt_MakeGlyphBitmap(&f, pixels + x + y * pw, gw, gh, pw, scale, scale, g);
        chardata[i].x0 = (stbtt_int16)x;
        chardata[i].y0 = (stbtt_int16)y;
        chardata[i].x1 = (stbtt_int16)(x + gw);
        chardata[i].y1 = (stbtt_int16)(y + gh);
        chardata[i].xadvance = scale * advance;
        chardata[i].xoff = (float)x0;
        chardata[i].yoff = (float)y0;
        x = x + gw + 1;
        if (y + gh + 1 > bottom_y) bottom_y = y + gh + 1;
    }
    return bottom_y;
}

//////////////////////////////////////////////////////////////////////////////
//
// rectangle packing replacement routines if you don't have stb_rect_pack.h
//

#ifndef STB_RECT_PACK_VERSION

typedef int stbrp_coord;

////////////////////////////////////////////////////////////////////////////////////
//                                                                                //
//                                                                                //
// COMPILER WARNING ?!?!?                                                         //
//                                                                                //
//                                                                                //
// if you get a compile warning due to these symbols being defined more than      //
// once, move #include "stb_rect_pack.h" before #include "stb_truetype.h"         //
//                                                                                //
////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    int width, height;
    int x, y, bottom_y;
} stbrp_context;

typedef struct {
    unsigned char x;
} stbrp_node;

struct stbrp_rect {
    stbrp_coord x, y;
    int id, w, h, was_packed;
};
#endif

//////////////////////////////////////////////////////////////////////////////
//
// bitmap baking
//
// This is SUPER-AWESOME (tm Ryan Gordon) packing using stb_rect_pack.h. If
// stb_rect_pack.h isn't available, it uses the BakeFontBitmap strategy.

#define STBTT__OVER_MASK (STBTT_MAX_OVERSAMPLE - 1)

//////////////////////////////////////////////////////////////////////////////
//
// sdf computation
//

#define STBTT_min(a, b) ((a) < (b) ? (a) : (b))
#define STBTT_max(a, b) ((a) < (b) ? (b) : (a))

//////////////////////////////////////////////////////////////////////////////
//
// font name matching -- recommended not to use this
//

STBTT_DEF int stbtt_BakeFontBitmap(const unsigned char *data, int offset, float pixel_height,
                                   unsigned char *pixels, int pw, int ph, int first_char,
                                   int num_chars, stbtt_bakedchar *chardata) {
    return stbtt_BakeFontBitmap_internal((unsigned char *)data, offset, pixel_height, pixels, pw,
                                         ph, first_char, num_chars, chardata);
}

STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset) {
    return stbtt_InitFont_internal(info, (unsigned char *)data, offset);
}

#endif  // STB_TRUETYPE_IMPLEMENTATION

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
/*=== IMPLEMENTATION =========================================================*/

#include "xdg-shell.c"
#include "xdg-shell.h"
#include <fcntl.h>
#include <poll.h>
#include <pty.h>     // forkpty
#include <stdlib.h>  // getenv/setenv/malloc
#include <string.h>  // memset
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>  // exec/fork/env
#include <wayland-client.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>

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
} sfte_cell;

typedef struct {
    sfte_cell *cells;
    sfte_cell *alt_cells;
    int cols;
    int rows;
    int cursor_x;
    int cursor_y;
    int saved_x;  // alt screen x
    int saved_y;  // alt screen y
    uint8_t hide_cursor;
    int scroll_top;
    int scroll_bottom;
    // pen state
    uint32_t cur_fg;
    uint32_t cur_bg;
    uint16_t cur_attr;
    // parser
    int vt_state;
    int vt_params[16];  // stores nums from esc sequences
    int vt_param_idx;
    uint8_t vt_dec_priv;  // tracks if seq starts with ?
} sfte_term;

typedef struct {
    uint8_t *ttf_buf;
    float cur_size;  // starts at SFTE_DEFAULT_FONT_SIZE
    uint8_t *atlas_pxs;
    int atlas_width;
    int atlas_height;
    stbtt_bakedchar char_data[96];  // ASCII 32-126
    int cell_width;                 // width of a single mono char
    int cell_height;                // height of a single mono char
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

    sfte_logger logger;
    sfte_term term;
    sfte_font font;
} _sfte_state;
static _sfte_state _sfte;

// >>memory
#define _SFTE_ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

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
    _SFTE_LOGITEM_XMACRO(FONT_LOADED, "font loaded and baked to atlas")
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

// >>font
static void _sfte_font_bake(void) {
    memset(_sfte.font.atlas_pxs, 0, _sfte.font.atlas_width * _sfte.font.atlas_height);
    int ret = stbtt_BakeFontBitmap(_sfte.font.ttf_buf, 0, _sfte.font.cur_size, _sfte.font.atlas_pxs,
                                   _sfte.font.atlas_width, _sfte.font.atlas_height, 32, 96,
                                   _sfte.font.char_data);
    SFTE_ASSERT(ret > 0, "font atlas not large enough for this font size");
    _sfte.font.cell_width = (int)(_sfte.font.char_data['A' - 32].xadvance + 0.5f);
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
    _sfte_font_bake();
    _SFTE_INFO(FONT_LOADED);
}

static void _sfte_wayland_render(void);
static void _sfte_term_resize(int new_cols, int new_rows);

static void _sfte_font_resize(float delta) {
    float new_size = _sfte.font.cur_size + delta;
    if (new_size < 4.0f || new_size > 96.0f) return;
    _sfte.font.cur_size = new_size;
    _sfte_font_bake();
    int new_cols = _sfte.width / _sfte.font.cell_width;
    int new_rows = _sfte.height / _sfte.font.cell_height;
    if (new_cols != _sfte.term.cols || new_rows != _sfte.term.rows)
        _sfte_term_resize(new_cols, new_rows);
    _sfte_wayland_render();
}

// >>render
static void _sfte_render_cell(int col, int row, uint32_t rune, uint32_t fg, uint32_t bg) {
    int cx = col * _sfte.font.cell_width;
    int cy = row * _sfte.font.cell_height;
    for (int y = 0; y < _sfte.font.cell_height; ++y) {
        for (int x = 0; x < _sfte.font.cell_width; ++x) {
            int px_idx = (cy + y) * _sfte.width + (cx + x);
            if (px_idx < _sfte.width * _sfte.height) _sfte.shm_data[px_idx] = bg;
        }
    }
    if (rune <= 32 || rune > 126) return;
    stbtt_bakedchar *b = &_sfte.font.char_data[rune - 32];
    int gw = b->x1 - b->x0;
    int gh = b->y1 - b->y0;
    int baseline = (int)(_sfte.font.cell_height * 0.8f);
    int draw_x = cx + (int)b->xoff;
    int draw_y = cy + baseline + (int)b->yoff;
    uint8_t fg_r = (fg >> 16) & 0xFF, fg_g = (fg >> 8) & 0xFF, fg_b = fg & 0xFF;
    uint8_t bg_r = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bg_b = bg & 0xFF;
    for (int y = 0; y < gh; ++y) {
        for (int x = 0; x < gw; ++x) {
            int screen_x = draw_x + x;
            int screen_y = draw_y + y;
            if (screen_x < 0 || screen_x >= _sfte.width || screen_y < 0 || screen_y >= _sfte.height)
                continue;
            uint8_t alpha = _sfte.font
                                .atlas_pxs[(b->y0 + y) * _sfte.font.atlas_width + (b->x0 + x)];
            if (alpha == 0) continue;
            int px_idx = screen_y * _sfte.width + screen_x;
            if (alpha == 255)
                _sfte.shm_data[px_idx] = fg;
            else {
                uint8_t col_r = (fg_r * alpha + bg_r * (255 - alpha)) >> 8;
                uint8_t col_g = (fg_g * alpha + bg_g * (255 - alpha)) >> 8;
                uint8_t col_b = (fg_b * alpha + bg_b * (255 - alpha)) >> 8;
                _sfte.shm_data[px_idx] = (col_r << 16) | (col_g << 8) | col_b;
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
                                             WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
}

static void _sfte_term_resize(int new_cols, int new_rows);

static void _sfte_wayland_render(void) {
    int new_cols = _sfte.width / _sfte.font.cell_width;
    int new_rows = _sfte.height / _sfte.font.cell_height;
    if (new_cols != _sfte.term.cols || new_rows != _sfte.term.rows)
        _sfte_term_resize(new_cols, new_rows);
    for (int r = 0; r < _sfte.term.rows; ++r) {
        for (int c = 0; c < _sfte.term.cols; ++c) {
            int idx = r * _sfte.term.cols + c;
            uint32_t rune = _sfte.term.cells[idx].rune;
            if (rune == 0) rune = ' ';
            uint32_t fg = _sfte.term.cells[idx].fg ? _sfte.term.cells[idx].fg : 0xFFFFFF;
            uint32_t bg = _sfte.term.cells[idx].bg ? _sfte.term.cells[idx].bg : SFTE_BG_COLOR;
            if (c == _sfte.term.cursor_x && r == _sfte.term.cursor_y && !_sfte.term.hide_cursor)
                _sfte_render_cell(c, r, rune, bg, fg);  // inverse for cursor
            else
                _sfte_render_cell(c, r, rune, fg, bg);
        }
    }
    wl_surface_attach(_sfte.surface, _sfte.buffer, 0, 0);
    wl_surface_damage_buffer(_sfte.surface, 0, 0, _sfte.width, _sfte.height);
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
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !_sfte.xkb_state) return;
    xkb_keycode_t keycode = key + 8;  // WARN: evdev codes are offset by 8 from xkb keycodes
    xkb_keysym_t sym = xkb_state_key_get_one_sym(_sfte.xkb_state, keycode);
    bool ctrl = xkb_state_mod_name_is_active(_sfte.xkb_state, XKB_MOD_NAME_CTRL,
                                             XKB_STATE_MODS_EFFECTIVE);
    bool alt = xkb_state_mod_name_is_active(_sfte.xkb_state, XKB_MOD_NAME_ALT,
                                            XKB_STATE_MODS_EFFECTIVE);
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
            if (sym == XKB_KEY_equal || sym == XKB_KEY_plus) {
                _sfte_font_resize(SFTE_FONT_RESIZE_SPEED);
                return;
            } else if (sym == XKB_KEY_minus) {
                _sfte_font_resize(-SFTE_FONT_RESIZE_SPEED);
                return;
            } else if (sym == XKB_KEY_0) {
                _sfte_font_resize(SFTE_DEFAULT_FONT_SIZE - _sfte.font.cur_size);
                return;
            }
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

        if (size == 0) size = xkb_state_key_get_utf8(_sfte.xkb_state, keycode, buf, sizeof(buf));
    }
    // if alt is held, prepend esc byte
    if (alt && size > 0 && size < (int)(sizeof(buf) - 1)) {
        memmove(buf + 1, buf, size++);
        buf[0] = '\033';
    }
    if (size > 0) write(_sfte.pty_fd, buf, size);
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
    (void)data, (void)keyboard, (void)rate, (void)delay;
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
    if (width > 0 && height > 0) {
        _sfte.width = width;
        _sfte.height = height;
    }
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
}

static void _sfte_wayland_unload(void) {
    if (_sfte.font.ttf_buf) free(_sfte.font.ttf_buf);
    if (_sfte.font.atlas_pxs) free(_sfte.font.atlas_pxs);
    if (_sfte.term.cells) free(_sfte.term.cells);
    if (_sfte.term.alt_cells) free(_sfte.term.alt_cells);
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
    if (_sfte.xkb_state) xkb_state_unref(_sfte.xkb_state);
    if (_sfte.xkb_keymap) xkb_keymap_unref(_sfte.xkb_keymap);
    if (_sfte.xkb_context) xkb_context_unref(_sfte.xkb_context);
}

// >>state
static void _sfte_state_load(void) {
    memset(&_sfte, 0, sizeof(_sfte));
    _sfte.width = SFTE_STARTUP_WIDTH;
    _sfte.height = SFTE_STARTUP_HEIGHT;
    _sfte.running = 1;
    _sfte.logger.func = SFTE_LOGGER_FUNC;
    _sfte.term.cols = 80;
    _sfte.term.rows = 24;
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
        .ws_row = 24,
        .ws_col = 80,
        .ws_xpixel = (unsigned short)_sfte.width,
        .ws_ypixel = (unsigned short)_sfte.height,
    };
    _sfte.pty_pid = forkpty(&_sfte.pty_fd, NULL, NULL, &ws);
    SFTE_ASSERT(_sfte.pty_pid != -1, "failed to forkpty");
    if (_sfte.pty_pid == 0) {
        setenv("TERM", SFTE_TERM_ENV, 1);
        char *shell = getenv("SHELL");
        if (!shell) shell = "/bin/sh";
        execlp(shell, shell, NULL);  // replace current pimg with shell
        abort();                     // if execlp returns, it failed to exec the shell
    }
    _SFTE_INFO(OK);
}

// >>vt
static const uint32_t _sfte_ansi_palette[16] = {
    0x181818, 0xCC241D, 0x98971A, 0xD79921, 0x458588, 0xB16286, 0x689D6A, 0xA89984,
    0x928374, 0xFB4934, 0xB8BB26, 0xFABD2F, 0x83A598, 0xD3869B, 0x8EC07C, 0xEBDBB2};

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
    struct winsize ws = {.ws_row = (uint8_t)new_rows,
                         .ws_col = (uint8_t)new_cols,
                         .ws_xpixel = (uint8_t)_sfte.width,
                         .ws_ypixel = (uint8_t)_sfte.height};
    ioctl(_sfte.pty_fd, TIOCSWINSZ, &ws);
    for (int i = 0; i < _sfte.width * _sfte.height; ++i) _sfte.shm_data[i] = SFTE_BG_COLOR;
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
        for (int i = 0; i < lines * cols; ++i) {  // clear lines at bot
            int idx = (bot - lines + 1) * cols + i;
            _sfte.term.cells[idx].rune = ' ';
            _sfte.term.cells[idx].fg = _sfte.term.cur_fg;
            _sfte.term.cells[idx].bg = _sfte.term.cur_bg;
            _sfte.term.cells[idx].attr = 0;
        }
    } else if (lines < 0) {  // scroll down
        lines = -lines;
        if (lines > height) lines = height;
        int move_cnt = height - lines;
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[(top + lines) * cols], &_sfte.term.cells[top * cols],
                    move_cnt * cols * sizeof(sfte_cell));
        for (int i = 0; i < lines * cols; ++i) {
            int idx = top * cols + i;
            _sfte.term.cells[idx].rune = ' ';
            _sfte.term.cells[idx].fg = _sfte.term.cur_fg;
            _sfte.term.cells[idx].bg = _sfte.term.cur_bg;
            _sfte.term.cells[idx].attr = 0;
        }
    }
}

static void _sfte_dispatch_csi(uint8_t cmd) {
    int *p = _sfte.term.vt_params;
    int cnt = _sfte.term.vt_param_idx + 1;
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
                _sfte.term.cur_fg = (p[i + 2] << 16) | (p[i + 3] << 8) | p[i + 4];
                i += 4;
            } else if (p[i] == 48 && i + 4 < cnt && p[i + 1] == 2) {  // true bg
                _sfte.term.cur_bg = (p[i + 2] << 16) | (p[i + 3] << 8) | p[i + 4];
                i += 4;
            }
        }
        break;
    case 'H':  // cursor position
    case 'f': {
        // NOTE: vt coords are 1-idxd
        int r = (p[0] > 0 ? p[0] : 1) - 1;
        int c = (cnt > 1 && p[1] > 0 ? p[1] : 1) - 1;
        if (r < 0) r = 0;
        if (r >= _sfte.term.rows) r = _sfte.term.rows - 1;
        if (c < 0) c = 0;
        if (c >= _sfte.term.cols) c = _sfte.term.cols - 1;
        _sfte.term.cursor_y = r;
        _sfte.term.cursor_x = c;
        break;
    }
    case 'J':  // clear screen
        if (p[0] != 2) break;
        for (int i = 0; i < _sfte.term.rows * _sfte.term.cols; ++i) {
            _sfte.term.cells[i].rune = ' ';
            _sfte.term.cells[i].fg = _sfte.term.cur_fg;
            _sfte.term.cells[i].bg = _sfte.term.cur_bg;
            _sfte.term.cells[i].attr = 0;
        }
        _sfte.term.cursor_x = 0;
        _sfte.term.cursor_y = 0;
        break;
    case 'K':  // erase in line
        if (p[0] != 0) break;
        for (int x = _sfte.term.cursor_x; x < _sfte.term.cols; ++x) {
            int idx = (_sfte.term.cursor_y * _sfte.term.cols) + x;
            _sfte.term.cells[idx].rune = ' ';
            _sfte.term.cells[idx].fg = _sfte.term.cur_fg;
            _sfte.term.cells[idx].bg = _sfte.term.cur_bg;
            _sfte.term.cells[idx].attr = 0;
        }
        break;
    case 'n': {  // device status report
        if (p[0] != 6) break;
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "\033[%d;%dR", _sfte.term.cursor_y + 1,
                           _sfte.term.cursor_x + 1);
        write(_sfte.pty_fd, buf, len);
        break;
    }
    case 'A':  // cursor up
        _sfte.term.cursor_y -= (p[0] > 0 ? p[0] : 1);
        if (_sfte.term.cursor_y < 0) _sfte.term.cursor_y = 0;
        break;
    case 'B':  // cursor down
        _sfte.term.cursor_y += (p[0] > 0 ? p[0] : 1);
        if (_sfte.term.cursor_y >= _sfte.term.rows) _sfte.term.cursor_y = _sfte.term.rows - 1;
        break;
    case 'C':  // cursor forward
        _sfte.term.cursor_x += (p[0] > 0 ? p[0] : 1);
        if (_sfte.term.cursor_x >= _sfte.term.cols) _sfte.term.cursor_x = _sfte.term.cols - 1;
        break;
    case 'D':  // cursor backward
        _sfte.term.cursor_x -= (p[0] > 0 ? p[0] : 1);
        if (_sfte.term.cursor_x < 0) _sfte.term.cursor_x = 0;
        break;
    case 'G':  // cursor horizontal abs
        _sfte.term.cursor_x = (p[0] > 0 ? p[0] : 1) - 1;
        if (_sfte.term.cursor_x < 0) _sfte.term.cursor_x = 0;
        if (_sfte.term.cursor_x >= _sfte.term.cols) _sfte.term.cursor_x = _sfte.term.cols - 1;
        break;
    case 'h':  // set mode
        if (!_sfte.term.vt_dec_priv) break;
        if (p[0] == 25)
            _sfte.term.hide_cursor = 0;  // ?25h / show cursor
        else if (p[0] == 1049) {         // ?1049h / save cursor and switch to alt
            if (!_sfte.term.alt_cells)
                _sfte.term.alt_cells = (sfte_cell *)calloc(_sfte.term.cols * _sfte.term.rows,
                                                           sizeof(sfte_cell));
            _sfte.term.saved_x = _sfte.term.cursor_x;
            _sfte.term.saved_y = _sfte.term.cursor_y;
            sfte_cell *tmp = _sfte.term.cells;  // swap buffer ptrs
            _sfte.term.cells = _sfte.term.alt_cells;
            _sfte.term.alt_cells = tmp;
            for (int i = 0; i < _sfte.term.cols * _sfte.term.rows; ++i) {
                _sfte.term.cells[i].rune = ' ';
                _sfte.term.cells[i].bg = SFTE_BG_COLOR;
                _sfte.term.cells[i].fg = 0xFFFFFF;
                _sfte.term.cells[i].attr = 0;
            }
            _sfte.term.cursor_x = 0;
            _sfte.term.cursor_y = 0;
        }
        break;
    case 'l':  // reset mode
        if (!_sfte.term.vt_dec_priv) break;
        if (p[0] == 25)
            _sfte.term.hide_cursor = 1;  // ?25l / hide cursor
        else if (p[0] == 1049) {         // ?1049l / switch to main and restore cursor
            if (_sfte.term.alt_cells) {
                sfte_cell *tmp = _sfte.term.cells;
                _sfte.term.cells = _sfte.term.alt_cells;
                _sfte.term.alt_cells = tmp;
            }
            _sfte.term.cursor_x = _sfte.term.saved_x;
            _sfte.term.cursor_y = _sfte.term.saved_y;
        }
        break;
    case 'r':  // set scroll region
    {
        int top = (p[0] > 0 ? p[0] : 1) - 1;
        int bot = (cnt > 1 && p[1] > 0 ? p[1] : _sfte.term.rows) - 1;
        if (top < 0) top = 0;
        if (bot >= _sfte.term.rows) bot = _sfte.term.rows - 1;
        if (top < bot) {
            _sfte.term.scroll_top = top;
            _sfte.term.scroll_bottom = bot;
        }
        _sfte.term.cursor_x = 0;  // DECSTBM specifies cursor reset
        _sfte.term.cursor_y = 0;
        break;
    }
    case 'S':  // scroll up
        _sfte_scroll(p[0] > 0 ? p[0] : 1);
        break;
    case 'T':  // scroll down
        _sfte_scroll(-(p[0] > 0 ? p[0] : 1));
        break;
    case 'd':  // line pos abs / VPA
    {
        // move to specific row, keep column same
        int r = (p[0] > 0 ? p[0] : 1) - 1;
        if (r < 0) r = 0;
        if (r >= _sfte.term.rows) r = _sfte.term.rows - 1;
        _sfte.term.cursor_y = r;
        break;
    }
    case 'X':  // erase char / ECH
    {
        // replace n chars with spaces from cursor
        int n = p[0] > 0 ? p[0] : 1;
        int rem = _sfte.term.cols - _sfte.term.cursor_x;
        if (n > rem) n = rem;
        for (int i = 0; i < n; ++i) {
            int idx = _sfte.term.cursor_y * _sfte.term.cols + _sfte.term.cursor_x + i;
            _sfte.term.cells[idx].rune = ' ';
            _sfte.term.cells[idx].fg = _sfte.term.cur_fg;
            _sfte.term.cells[idx].bg = _sfte.term.cur_bg;
            _sfte.term.cells[idx].attr = 0;
        }
        break;
    }
    case 'P':  // delete char / DCH
    {
        // deletes n chars, text to the right shifts left, eol blanked
        int n = p[0] > 0 ? p[0] : 1;
        int rem = _sfte.term.cols - _sfte.term.cursor_x;
        if (n > rem) n = rem;
        int move_cnt = rem - n;
        int base_idx = _sfte.term.cursor_y * _sfte.term.cols;
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[base_idx + _sfte.term.cursor_x],
                    &_sfte.term.cells[base_idx + _sfte.term.cursor_x + n],
                    move_cnt * sizeof(sfte_cell));
        for (int i = 0; i < n; ++i) {
            int idx = base_idx + _sfte.term.cols - n + i;
            _sfte.term.cells[idx].rune = ' ';
            _sfte.term.cells[idx].fg = _sfte.term.cur_fg;
            _sfte.term.cells[idx].bg = _sfte.term.cur_bg;
            _sfte.term.cells[idx].attr = 0;
        }
        break;
    }
    case '@':  // insert char / ICH
    {
        // inserts n spaces, text shifts right, text pushed off edge is lost
        int n = p[0] > 0 ? p[0] : 1;
        int rem = _sfte.term.cols - _sfte.term.cursor_x;
        if (n > rem) n = rem;
        int move_cnt = rem - n;
        int base_idx = _sfte.term.cursor_y * _sfte.term.cols;
        if (move_cnt > 0)
            memmove(&_sfte.term.cells[base_idx + _sfte.term.cursor_x + n],
                    &_sfte.term.cells[base_idx + _sfte.term.cursor_x],
                    move_cnt * sizeof(sfte_cell));
        for (int i = 0; i < n; ++i) {
            int idx = base_idx + _sfte.term.cursor_x + i;
            _sfte.term.cells[idx].rune = ' ';
            _sfte.term.cells[idx].fg = _sfte.term.cur_fg;
            _sfte.term.cells[idx].bg = _sfte.term.cur_bg;
            _sfte.term.cells[idx].attr = 0;
        }
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
        for (int i = 0; i < n * cols; ++i) {
            int idx = top * cols + i;
            _sfte.term.cells[idx].rune = ' ';
            _sfte.term.cells[idx].fg = _sfte.term.cur_fg;
            _sfte.term.cells[idx].bg = _sfte.term.cur_bg;
            _sfte.term.cells[idx].attr = 0;
        }
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
        for (int i = 0; i < n; ++i) {
            int idx = (bot - n + 1) * cols + i;
            _sfte.term.cells[idx].rune = ' ';
            _sfte.term.cells[idx].fg = _sfte.term.cur_fg;
            _sfte.term.cells[idx].bg = _sfte.term.cur_bg;
            _sfte.term.cells[idx].attr = 0;
        }
        break;
    }
    }
}

typedef enum {
    VT_GROUND,     // normal
    VT_ESCAPE,     // \033
    VT_CSI_ENTRY,  // \033[
    VT_CSI_PARAM,  // nums
    VT_OSC         // \033]
} sfte_vt_state;

static void _sfte_parse_byte(uint8_t b) {
    switch (_sfte.term.vt_state) {
    case VT_GROUND:
        if (b == '\033' || b == '\x1b')
            _sfte.term.vt_state = VT_ESCAPE;
        else if (b == '\n') {
            if (_sfte.term.cursor_y == _sfte.term.scroll_bottom)
                _sfte_scroll(1);  // at bot margin, scroll text up
            else if (_sfte.term.cursor_y < _sfte.term.rows - 1)
                _sfte.term.cursor_y++;  // not at bot, move cursor down
            _sfte.term.cursor_x = 0;
        } else if (b == '\r')
            _sfte.term.cursor_x = 0;
        else if ((b == '\b' || b == '\x7f') && _sfte.term.cursor_x > 0)
            _sfte.term.cursor_x--;
        else if (b >= 0x20) {
            if (_sfte.term.cursor_x >= _sfte.term.cols) {
                _sfte.term.cursor_x = 0;
                if (_sfte.term.cursor_y == _sfte.term.scroll_bottom)
                    _sfte_scroll(1);
                else if (_sfte.term.cursor_y < _sfte.term.rows - 1)
                    _sfte.term.cursor_y++;
            }
            int idx = (_sfte.term.cursor_y * _sfte.term.cols) + _sfte.term.cursor_x;
            _sfte.term.cells[idx].rune = b;
            _sfte.term.cells[idx].fg = _sfte.term.cur_fg;
            _sfte.term.cells[idx].bg = _sfte.term.cur_bg;
            _sfte.term.cells[idx].attr = _sfte.term.cur_attr;
            _sfte.term.cursor_x++;
        }
        break;
    case VT_ESCAPE:
        if (b == '[') {
            _sfte.term.vt_state = VT_CSI_ENTRY;
            _sfte.term.vt_param_idx = 0;
            _sfte.term.vt_dec_priv = 0;
            memset(_sfte.term.vt_params, 0, sizeof(_sfte.term.vt_params));
        } else if (b == ']')
            _sfte.term.vt_state = VT_OSC;
        else
            _sfte.term.vt_state = VT_GROUND;
        break;
    case VT_OSC:
        if (b == '\x07' || b == '\x1b') _sfte.term.vt_state = VT_GROUND;
        break;
    case VT_CSI_ENTRY:
    case VT_CSI_PARAM:
        if (b == '?') {  // private marker
            _sfte.term.vt_dec_priv = 1;
            _sfte.term.vt_state = VT_CSI_PARAM;
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
    int wl_fd = wl_display_get_fd(_sfte.display);
    while (_sfte.running) {
        wl_display_dispatch_pending(_sfte.display);
        wl_display_flush(_sfte.display);
        struct pollfd fds[] = {{.fd = wl_fd, .events = POLLIN},
                               {.fd = _sfte.pty_fd, .events = POLLIN}};
        if (poll(fds, _SFTE_ARRAY_LEN(fds), -1 /* infinite timeout */) == -1) break;
        if (fds[0].revents & (POLLIN | POLLERR | POLLHUP))
            if (wl_display_dispatch(_sfte.display) == -1) _sfte.running = 0;
        if (fds[1].revents & POLLIN) {
            uint8_t buf[SFTE_PTY_BUF_SIZE];
            ssize_t n = read(_sfte.pty_fd, buf, SFTE_PTY_BUF_SIZE);
            if (n > 0) {
                for (ssize_t i = 0; i < n; ++i) _sfte_parse_byte(buf[i]);
                _sfte_wayland_render();
            } else
                _sfte.running = 0;
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
