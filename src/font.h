#pragma once

#include "src/util.h"
#include <stdint.h>
#include <string.h>

#ifndef SFTE_FONT_CUSTOM_BACKEND
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "vendor/stb_truetype.h"

#define SFTE_FONT_INIT _sfte_stb_init
#define SFTE_FONT_GET_SCALE _sfte_stb_get_scale
#define SFTE_FONT_VMETRICS _sfte_stb_vmetrics
#define SFTE_FONT_BOUNDS _sfte_stb_bounds
#define SFTE_FONT_BAKE _sfte_stb_bake
#define SFTE_FONT_GET_ID _sfte_stb_get_id
#endif  // !SFTE_FONT_CUSTOM_BACKEND

static const float _sfte_font_scales[SFTE_FONT_MAX_COUNT] = SFTE_FONT_SCALES;

// =================================================================================================
// >>shaper
// =================================================================================================
#if SFTE_FONT_LIGATURES
#define _SFTE_MATCH_TAG(t1, t2)                                                                    \
    (t1)[0] == (t2)[0] && (t1)[1] == (t2)[1] && (t1)[2] == (t2)[2] && (t1)[3] == (t2)[3]

/*
    Grabs a 16-bit big-endian value from `offset` and places the `offset` after grabbed value.
*/
static inline uint16_t _sfte_shaper_consume16(const uint8_t *ttf_data, uint32_t *off) {
    uint16_t val = _SFTE_R16BE(ttf_data, *off);
    *off += sizeof(uint16_t);
    return val;
}

/*
    Grabs a 32-bit big-endian value from `offset` and places the `offset` after grabbed value.
*/
static inline uint32_t _sfte_shaper_consume32(const uint8_t *ttf_data, uint32_t *off) {
    uint32_t val = _SFTE_R32BE(ttf_data, *off);
    *off += sizeof(uint32_t);
    return val;
}

/*
    Returns the byte offset of a specific table in TTF font data.
    Returns 0 if not found.
*/
static inline uint32_t _sfte_shaper_get_font_table_off(const uint8_t *ttf_data, const char tag[4]) {
    // 0-3 - sfnt version
    // 4-7 - number of tables
    // 12+ - tables
    uint32_t off = sizeof(uint32_t);
    uint32_t num_tables = _sfte_shaper_consume16(ttf_data, &off);
    off += 3 * sizeof(uint16_t);

    for (uint32_t i = 0; i < num_tables; ++i, off += 4 * sizeof(uint8_t) + 3 * sizeof(uint32_t))
        if (_SFTE_MATCH_TAG(&ttf_data[off], tag))
            return _SFTE_R32BE(ttf_data, off + 2 * sizeof(uint32_t));

    return 0;
}

/*
    Locates a specific GSUB feature (like 'calt' or 'liga') and extracts its lookup indices.
*/
static inline void _sfte_shaper_load_feature(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                             const char tag[4], sfte_shaper_feature *out_feat) {
    out_feat->lookup_cnt = 0;
    if (ctx->feat_list_off == 0) return;

    // FeatureList header data:
    // 0-1 - FeatureCount
    // 2+  - FeatureRecords
    uint32_t fl_off = ctx->feat_list_off;
    uint16_t feat_cnt = _sfte_shaper_consume16(ttf_data, &fl_off);

    for (uint32_t i = 0, rec_off = fl_off; i < feat_cnt;
         ++i, rec_off += 4 * sizeof(uint8_t) + sizeof(uint16_t))

        if (_SFTE_MATCH_TAG(&ttf_data[rec_off], tag)) {
            rec_off += 4 * sizeof(uint8_t);
            uint32_t feat_tab_off = ctx->feat_list_off + _sfte_shaper_consume16(ttf_data, &rec_off);

            // FeatureTable header data:
            // 0-1 - FeatureParams offset
            // 2-3 - LookupIndexCount
            // 4+  - LookupListIndices
            feat_tab_off += sizeof(uint16_t);  // Skip FeatureParams
            uint16_t lookup_cnt = _sfte_shaper_consume16(ttf_data, &feat_tab_off);

            for (uint16_t j = 0; j < lookup_cnt && j < SFTE_FONT_MAX_LIGATURE_LOOKUPS; ++j)
                out_feat->lookup_indices[j] = _SFTE_R16BE(ttf_data,
                                                          feat_tab_off + (j * sizeof(uint16_t)));

            out_feat->lookup_cnt = lookup_cnt > SFTE_FONT_MAX_LIGATURE_LOOKUPS
                                       ? SFTE_FONT_MAX_LIGATURE_LOOKUPS
                                       : lookup_cnt;
            return;
        }
}

/*
    Parses inner subtables and extension lookups (type 8 -> type 6).
*/
static inline void _sfte_shaper_parse_subtables(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                                uint16_t lookup_idx, uint32_t lookup_tab_off,
                                                uint16_t lookup_type, uint16_t subtable_cnt) {
    ctx->calt.subtable_cnts[lookup_idx] = subtable_cnt > SFTE_FONT_MAX_LIGATURE_LOOKUPS
                                              ? SFTE_FONT_MAX_LIGATURE_LOOKUPS
                                              : subtable_cnt;

    for (uint16_t s = 0; s < ctx->calt.subtable_cnts[lookup_idx]; ++s) {
        uint16_t st_off_rel = _SFTE_R16BE(ttf_data, lookup_tab_off + (s * sizeof(uint16_t)));
        uint32_t st_off_base = lookup_tab_off - 3 * sizeof(uint16_t) + st_off_rel;
        uint32_t st_off = st_off_base;

        uint16_t type = lookup_type;
        if (lookup_type == 8) {  // Extension lookup
            uint16_t ext_format = _sfte_shaper_consume16(ttf_data, &st_off);
            if (ext_format == 1) {
                type = _sfte_shaper_consume16(ttf_data, &st_off);
                uint32_t ext_off_rel = _sfte_shaper_consume32(ttf_data, &st_off);
                st_off = st_off_base + ext_off_rel;
            }
        }

        ctx->calt.lookup_types[lookup_idx] = type;
        ctx->calt.subtable_offs[lookup_idx][s] = st_off;
    }
}

/*
    Parses the GSUB LookupList and stores offsets for contextual rules.
*/
static inline void _sfte_shaper_parse_lookups(sfte_shaper_ctx *ctx, const uint8_t *ttf_data) {
    if (ctx->lookup_list_off == 0) return;

    // LookupList header data:
    // 0-1 - LookupCount
    // 2+  - offsets to lookup tables
    uint32_t ll_off = ctx->lookup_list_off;
    uint16_t l_cnt = _sfte_shaper_consume16(ttf_data, &ll_off);

    for (uint16_t i = 0; i < ctx->calt.lookup_cnt; ++i) {
        uint16_t idx = ctx->calt.lookup_indices[i];
        if (idx >= l_cnt) continue;

        uint32_t lt_off = ctx->lookup_list_off +
                          _SFTE_R16BE(ttf_data, ll_off + (idx * sizeof(uint16_t)));

        // LookupTable header data:
        // 0-1 - LookupType
        // 2-3 - LookupFlag
        // 4-5 - SubTableCount
        uint16_t l_type = _sfte_shaper_consume16(ttf_data, &lt_off);
        lt_off += sizeof(uint16_t);  // Skip LookupFlag
        uint16_t st_cnt = _sfte_shaper_consume16(ttf_data, &lt_off);

        if (st_cnt > 0)
            _sfte_shaper_parse_subtables(ctx, ttf_data, i, lt_off, l_type, st_cnt);
        else {
            ctx->calt.lookup_types[i] = 0;
            for (uint16_t s = 0; s < SFTE_FONT_MAX_LIGATURE_SUBTABLES; ++s)
                ctx->calt.subtable_offs[i][s] = 0;
        }
    }
}

/*
    Initializes the OpenType GSUB shaper state machine from raw TTF data.
*/
static inline void _sfte_shaper_init(sfte_shaper_ctx *ctx, const uint8_t *ttf_data) {
    ctx->table_off = _sfte_shaper_get_font_table_off(ttf_data, "GSUB");
    if (ctx->table_off == 0) return;

    uint32_t t_off = ctx->table_off;

    // GSUB 1.0 header data:
    // 0-1 - major version
    // 2-3 - minor version
    // 4-5 - ScriptList offset
    // 6-7 - FeatureList offset
    // 8-9 - LookupList offset

    uint16_t major_ver = _sfte_shaper_consume16(ttf_data, &t_off);
    t_off += sizeof(uint16_t);  // Skip minor
    if (major_ver != 1) return;

    ctx->script_list_off = ctx->table_off + _sfte_shaper_consume16(ttf_data, &t_off);
    ctx->feat_list_off = ctx->table_off + _sfte_shaper_consume16(ttf_data, &t_off);
    ctx->lookup_list_off = ctx->table_off + _sfte_shaper_consume16(ttf_data, &t_off);

    _sfte_shaper_load_feature(ctx, ttf_data, "calt", &ctx->calt);
    _sfte_shaper_load_feature(ctx, ttf_data, "liga", &ctx->liga);

    _sfte_shaper_parse_lookups(ctx, ttf_data);
}

/*
    Resolves a glyphs index within a Coverage table (format 1 or 2).
    Returns -1 if the glyph is not covered by the rule.
*/
static inline int32_t _sfte_shaper_get_coverage_index(const uint8_t *ttf_data, uint32_t cov_off,
                                                      uint16_t glyph_id) {
    if (cov_off == 0) return -1;

    uint16_t format = _sfte_shaper_consume16(ttf_data, &cov_off);

    if (format == 1) {  // Array of individual glyph IDs
        uint16_t g_cnt = _sfte_shaper_consume16(ttf_data, &cov_off);
        for (uint16_t i = 0; i < g_cnt; ++i)
            if (_SFTE_R16BE(ttf_data, cov_off + (i * sizeof(uint16_t))) == glyph_id) return i;

    } else if (format == 2) {  // Array of glyph ID ranges
        uint16_t gr_cnt = _sfte_shaper_consume16(ttf_data, &cov_off);
        for (uint32_t i = 0, r_off = cov_off; i < gr_cnt; ++i, r_off += 3 * sizeof(uint16_t))
            if (glyph_id >= _SFTE_R16BE(ttf_data, r_off) &&
                glyph_id <= _SFTE_R16BE(ttf_data, r_off + sizeof(uint16_t)))
                return _SFTE_R16BE(ttf_data, r_off + 2 * sizeof(uint16_t)) +
                       (glyph_id - _SFTE_R16BE(ttf_data, r_off));
    }

    return -1;
}

/*
    Resolves a glyphs class within a ClassDef table.
*/
static inline uint16_t _sfte_shaper_get_class(const uint8_t *ttf_data, uint32_t class_off,
                                              uint16_t glyph_id) {
    if (class_off == 0) return 0;

    uint16_t format = _sfte_shaper_consume16(ttf_data, &class_off);

    if (format == 1) {  // Array of classses for a contiguous range of glyph IDs
        uint16_t c_start = _sfte_shaper_consume16(ttf_data, &class_off);
        uint16_t c_cnt = _sfte_shaper_consume16(ttf_data, &class_off);

        if (glyph_id >= c_start && glyph_id < c_start + c_cnt)
            return _SFTE_R16BE(ttf_data, class_off + ((glyph_id - c_start) * sizeof(uint16_t)));

    } else if (format == 2) {  // Array of glyph ID ranges mapping to specific classes
        uint16_t g_cnt = _sfte_shaper_consume16(ttf_data, &class_off);

        for (uint16_t i = 0; i < g_cnt; ++i, class_off += 3 * sizeof(uint16_t))
            if (glyph_id >= _SFTE_R16BE(ttf_data, class_off) &&
                glyph_id <= _SFTE_R16BE(ttf_data, class_off + sizeof(uint16_t)))
                return _SFTE_R16BE(ttf_data, class_off + 2 * sizeof(uint16_t));
    }

    return 0;
}

/*
    Evaluates a single substitution rule (LookupType=1).
*/
static inline uint16_t _sfte_shaper_get_type1_subst(const uint8_t *ttf_data, uint32_t t1_off,
                                                    uint16_t glyph_id) {
    uint32_t t_off = t1_off;
    uint16_t format = _sfte_shaper_consume16(ttf_data, &t_off);
    if (format != 1 && format != 2) return glyph_id;

    uint32_t c_off = t1_off + _sfte_shaper_consume16(ttf_data, &t_off);

    int32_t c_idx = _sfte_shaper_get_coverage_index(ttf_data, c_off, glyph_id);
    if (c_idx < 0) return glyph_id;

    if (format == 1)
        return (uint16_t)(glyph_id + _SFTE_R16BE(ttf_data, t_off));
    else if (c_idx < _sfte_shaper_consume16(ttf_data, &t_off))
        return _SFTE_R16BE(ttf_data, t_off + (c_idx * sizeof(uint16_t)));

    return glyph_id;
}

/*
    Evaluates a specific rule sequence.
    Returns 1 if sequence matches, fills out `out_records` and `out_cnt`.
    Returns 0 if sequence doesn't match.
*/
static inline uint8_t _sfte_shaper_match_rule_format1(const uint8_t *ttf_data, uint32_t rule_off,
                                                      uint16_t *grid, size_t grid_len, size_t pos,
                                                      sfte_shaper_subst_record *out_records,
                                                      uint16_t *out_cnt) {
    // Backtrack, stored in reverse visual order
    uint16_t b_cnt = _sfte_shaper_consume16(ttf_data, &rule_off);
    if (pos < b_cnt) return 0;  // Not enough cells behind to match
    for (uint16_t i = 0; i < b_cnt; ++i)
        if (grid[pos - (i + 1)] != _sfte_shaper_consume16(ttf_data, &rule_off)) return 0;

    // Input, 0 is implicit via Coverage, check 1 to input_cnt-1
    uint16_t i_cnt = _sfte_shaper_consume16(ttf_data, &rule_off);
    uint16_t ia_len = (i_cnt > 0) ? i_cnt - 1 : 0;
    if (pos + i_cnt > grid_len) return 0;  // Not enough cells ahead to match
    for (uint16_t i = 0; i < ia_len; ++i)
        if (grid[pos + (i + 1)] != _sfte_shaper_consume16(ttf_data, &rule_off)) return 0;

    // Lookahead, starts immediately after the input sequence
    uint16_t l_cnt = _sfte_shaper_consume16(ttf_data, &rule_off);
    if (pos + i_cnt + l_cnt > grid_len) return 0;  // Not enough cells for lookahead
    for (uint16_t i = 0; i < l_cnt; ++i)
        if (grid[pos + (i + i_cnt)] != _sfte_shaper_consume16(ttf_data, &rule_off)) return 0;

    // Matched, extract substitution records
    *out_cnt = _sfte_shaper_consume16(ttf_data, &rule_off);
    for (uint16_t i = 0; i < *out_cnt && i < SFTE_FONT_MAX_LIGATURE_RECORDS; ++i) {
        out_records[i].sequence_idx = _sfte_shaper_consume16(ttf_data, &rule_off);
        out_records[i].lookup_idx = _sfte_shaper_consume16(ttf_data, &rule_off);
    }

    return 1;
}

/*
    Applies substitution records to the temporary glyph ID buffer.
*/
static inline void _sfte_shaper_apply_subst(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                            uint16_t *grid, size_t pos,
                                            const sfte_shaper_subst_record *records,
                                            uint16_t records_cnt) {
    for (uint16_t i = 0; i < records_cnt; ++i) {
        uint16_t t_pos = pos + records[i].sequence_idx;
        uint32_t tl_off = ctx->lookup_list_off +
                          _SFTE_R16BE(ttf_data, ctx->lookup_list_off + sizeof(uint16_t) +
                                                    (records[i].lookup_idx * sizeof(uint16_t)));

        uint16_t ts_rel_off = _SFTE_R16BE(ttf_data, tl_off + 3 * sizeof(uint16_t));
        uint32_t ts_off = tl_off + ts_rel_off;

        grid[t_pos] = _sfte_shaper_get_type1_subst(ttf_data, ts_off, grid[t_pos]);
    }
}

/*
    Evaluates a format 1 contextual lookup (Rule-based).
    Returns 1 on success.
    Returns 0 if failed to match a rule.
*/
static inline uint8_t _sfte_shaper_eval_format1(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                                uint32_t sub_off, uint16_t cur_glyph,
                                                uint16_t *grid, size_t grid_len, size_t pos) {
    uint32_t s_off = sub_off + sizeof(uint16_t);  // Skip format

    uint32_t c_off = sub_off + _sfte_shaper_consume16(ttf_data, &s_off);
    int32_t c_idx = _sfte_shaper_get_coverage_index(ttf_data, c_off, cur_glyph);
    if (c_idx < 0) return 0;

    uint16_t rs_cnt = _sfte_shaper_consume16(ttf_data, &s_off);
    if (c_idx >= rs_cnt) return 0;

    uint32_t rs_off = sub_off + 3 * sizeof(uint16_t) + (c_idx * sizeof(uint16_t));
    uint16_t rs_rel_off = _SFTE_R16BE(ttf_data, rs_off);
    if (!rs_rel_off) return 0;

    uint32_t rs_base_off = sub_off + rs_rel_off;
    rs_off = rs_base_off;
    uint16_t r_cnt = _sfte_shaper_consume16(ttf_data, &rs_off);

    for (uint16_t r = 0; r < r_cnt; ++r) {
        uint32_t r_off = rs_base_off + _sfte_shaper_consume16(ttf_data, &rs_off);

        sfte_shaper_subst_record records[SFTE_FONT_MAX_LIGATURE_RECORDS];
        uint16_t rec_cnt = 0;

        if (_sfte_shaper_match_rule_format1(ttf_data, r_off, grid, grid_len, pos, records,
                                            &rec_cnt)) {
            _sfte_shaper_apply_subst(ctx, ttf_data, grid, pos, records, rec_cnt);
            return 1;
        }
    }

    return 0;
}

/*
    Evaluates a format 3 contextual lookup (Coverage-based).
    Returns 1 on success.
    Returns 0 if backtrack/input/lookahead failed.
*/
static inline uint8_t _sfte_shaper_eval_format3(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                                uint32_t sub_off, uint16_t *grid, size_t grid_len,
                                                size_t pos) {
    uint32_t s_off = sub_off + sizeof(uint16_t);  // Skip format

    // Backtrack, stored in reverse visual order
    uint16_t b_cnt = _sfte_shaper_consume16(ttf_data, &s_off);
    if (pos < b_cnt) return 0;
    for (uint16_t i = 0; i < b_cnt; ++i)
        if (_sfte_shaper_get_coverage_index(ttf_data,
                                            sub_off + _sfte_shaper_consume16(ttf_data, &s_off),
                                            grid[pos - (i + 1)]) < 0)
            return 0;

    // Input, includes the current glyph at index 0
    uint16_t i_cnt = _sfte_shaper_consume16(ttf_data, &s_off);
    if (pos + i_cnt > grid_len) return 0;
    for (uint16_t i = 0; i < i_cnt; ++i)
        if (_sfte_shaper_get_coverage_index(
                ttf_data, sub_off + _sfte_shaper_consume16(ttf_data, &s_off), grid[pos + i]) < 0)
            return 0;

    // Lookahead, starts immediately after the input sequence
    uint16_t l_cnt = _sfte_shaper_consume16(ttf_data, &s_off);
    if (pos + i_cnt + l_cnt > grid_len) return 0;
    for (uint16_t i = 0; i < l_cnt; ++i)
        if (_sfte_shaper_get_coverage_index(ttf_data,
                                            sub_off + _sfte_shaper_consume16(ttf_data, &s_off),
                                            grid[pos + i_cnt + i]) < 0)
            return 0;

    // Matched, extract and apply substitutions
    uint16_t rec_cnt = _sfte_shaper_consume16(ttf_data, &s_off);
    sfte_shaper_subst_record records[SFTE_FONT_MAX_LIGATURE_RECORDS];

    for (uint16_t i = 0; i < rec_cnt && i < SFTE_FONT_MAX_LIGATURE_RECORDS; ++i) {
        records[i].sequence_idx = _sfte_shaper_consume16(ttf_data, &s_off);
        records[i].lookup_idx = _sfte_shaper_consume16(ttf_data, &s_off);
    }

    _sfte_shaper_apply_subst(ctx, ttf_data, grid, pos, records, rec_cnt);

    return 1;
}

/*
    Shapes a continuous block of uniform text against all lookups.
    OpenType dictates a cascading loop.
    A lookup must evaluate the entire string before yielding to the next lookup.
    A simple loop would fail on something like '==='.
*/
static inline void _sfte_shaper_shape_row(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                          uint16_t *grid, size_t grid_len) {
    for (uint16_t i = 0; i < ctx->calt.lookup_cnt; ++i) {
        if (ctx->calt.lookup_types[i] != 6) continue;

        for (size_t pos = 0; pos < grid_len; ++pos) {
            uint16_t g = grid[pos];
            if (!g) continue;

            for (uint16_t s = 0; s < ctx->calt.subtable_cnts[i]; ++s) {
                uint32_t s_off = ctx->calt.subtable_offs[i][s];
                if (!s_off) continue;

                uint16_t format = _SFTE_R16BE(ttf_data, s_off);
                uint8_t matched = 0;

                if (format == 1)
                    matched = _sfte_shaper_eval_format1(ctx, ttf_data, s_off, g, grid, grid_len,
                                                        pos);
                else if (format == 3)
                    matched = _sfte_shaper_eval_format3(ctx, ttf_data, s_off, grid, grid_len, pos);

                if (matched) break;
            }
        }
    }
}

#undef _SFTE_MATCH_TAG
#endif  // SFTE_FONT_LIGATURES

// =================================================================================================
// >>font
// =================================================================================================
#ifndef SFTE_FONT_CUSTOM_BACKEND
/*
    Default stb_truetype wrappers.
    Can be overriden by defining SFTE_FONT_CUSTOM_BACKEND.
*/
static inline void _sfte_stb_init(sfte_font_backend_info *info, const uint8_t *data) {
    stbtt_InitFont(info, data, 0);
}

static inline float _sfte_stb_get_scale(sfte_font_backend_info *info, float px_hei) {
    return stbtt_ScaleForPixelHeight(info, px_hei);
}

static inline void _sfte_stb_vmetrics(sfte_font_backend_info *info, int *ascent, int *descent,
                                      int *linegap) {
    stbtt_GetFontVMetrics(info, ascent, descent, linegap);
}

static inline void _sfte_stb_bounds(sfte_font_backend_info *info, int glyph_id, float scale,
                                    int *adv, int *x0, int *y0, int *x1, int *y1) {
    int lsb;
    stbtt_GetGlyphHMetrics(info, glyph_id, adv, &lsb);
    stbtt_GetGlyphBitmapBox(info, glyph_id, scale, scale, x0, y0, x1, y1);
}

static inline void _sfte_stb_bake(sfte_stack *stack, sfte_font_backend_info *info, int glyph_idx,
                                  float scale, uint8_t *atlas_ptr, int gw, int gh,
                                  int atlas_stride) {
#if SFTE_FONT_OVERSAMPLE <= 1
    (void)stack;
    stbtt_MakeGlyphBitmap(info, atlas_ptr, gw, gh, atlas_stride, scale, scale, glyph_idx);
#else   // SFTE_FONT_OVERSAMPLE > 1
    int bw = gw * SFTE_FONT_OVERSAMPLE;
    int bh = gh;

    size_t pre_off = _sfte_mem_stack_save(stack);
    uint8_t *temp_buf = (uint8_t *)_sfte_mem_stack_alloc(stack, bw * bh * sizeof(uint8_t),
                                                         _Alignof(uint8_t));

    stbtt_MakeGlyphBitmap(info, temp_buf, bw, bh, bw, scale * SFTE_FONT_OVERSAMPLE, scale,
                          glyph_idx);

    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x) {
            int32_t sum = 0;
            for (int ox = 0; ox < SFTE_FONT_OVERSAMPLE; ++ox)
                sum += temp_buf[y * bw + (x * SFTE_FONT_OVERSAMPLE + ox)];
            atlas_ptr[y * atlas_stride + x] = (uint8_t)(sum / SFTE_FONT_OVERSAMPLE);
        }

    _sfte_mem_stack_rewind(stack, pre_off);
#endif  // SFTE_FONT_OVERSAMPLE > 1
}

static inline int _sfte_stb_get_id(sfte_font_backend_info *info, uint32_t rune) {
    return stbtt_FindGlyphIndex(info, rune);
}
#endif  // !SFTE_FONT_CUSTOM_BACKEND

/*
    Distance in pixels between each glyph in the atlas 2D texture.
*/
#define _SFTE_FONT_PADDING 1

/*
    Retrieves the active cache pool for a specific font style (regular/bold/italic/bold italic).
*/
static inline sfte_font_cache *_sfte_font_get_cache(sfte_ctx *ctx, sfte_font_style style) {
    sfte_font_state *font = &ctx->font;
    if (style == SFTE_FONT_STYLE_REGULAR) return &font->regular;
#ifdef SFTE_FONT_BOLD
    if (style == SFTE_FONT_STYLE_BOLD) return &font->bold;
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    if (style == SFTE_FONT_STYLE_ITALIC) return &font->italic;
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    if (style == SFTE_FONT_STYLE_BOLD_ITALIC) return &font->bold_italic;
#endif  // SFTE_FONT_BOLD_ITALIC
    return NULL;
}

/*
    Clears a font cache's atlas texture and glyph hash map.
*/
static inline void _sfte_font_clear_cache(sfte_font_cache *cache) {
    if (cache->atlas_pxs) memset(cache->atlas_pxs, 0, SFTE_FONT_ATLAS_SIZE * SFTE_FONT_ATLAS_SIZE);
    if (cache->glyphs) memset(cache->glyphs, 0, SFTE_FONT_GLYPH_CAP * sizeof(sfte_glyph));
    cache->atlas_x = 0;
    cache->atlas_y = 0;
    cache->atlas_row_h = 0;
}

/*
    Recalculates font scales for the primary font and all fallbacks based on current size.
*/
static inline void _sfte_font_update_scales(sfte_font_cache *cache, float cur_size) {
    for (uint8_t i = 0; i < cache->num_fonts; ++i) {
        float tweak = _sfte_font_scales[i];
        if (tweak <= 0.0f) tweak = 1.0f;
        cache->scales[i] = SFTE_FONT_GET_SCALE(&cache->info[i], cur_size * tweak);
    }
}

/*
    Allocates space in the 2D texture atlas for a new glyph and bakes the pixels.
    Glyphs are packed sequentially into rows. If a row runs out of horizontal space,
    we step down by the height of the tallest glyph in that row.
*/
static inline void _sfte_font_pack_and_bake(sfte_stack *stack, sfte_font_cache *cache,
                                            sfte_glyph *g, int32_t font_idx, int32_t glyph_id,
                                            int32_t gw, int32_t gh) {
    if (cache->atlas_x + gw >= SFTE_FONT_ATLAS_SIZE) {
        cache->atlas_x = 0;
        cache->atlas_y += cache->atlas_row_h + _SFTE_FONT_PADDING;
        cache->atlas_row_h = 0;
    }

    SFTE_ASSERT(cache->atlas_y + gh < SFTE_FONT_ATLAS_SIZE, "glyph atlas full");

    if (gh > cache->atlas_row_h) cache->atlas_row_h = gh;

    g->x0 = cache->atlas_x;
    g->y0 = cache->atlas_y;
    g->x1 = g->x0 + gw;
    g->y1 = g->y0 + gh;

    if (gw > 0 && gh > 0) {
        int32_t atlas_idx = g->y0 * SFTE_FONT_ATLAS_SIZE + g->x0;
        SFTE_FONT_BAKE(stack, &cache->info[font_idx], glyph_id, cache->scales[font_idx],
                       &cache->atlas_pxs[atlas_idx], gw, gh, SFTE_FONT_ATLAS_SIZE);
    }

    cache->atlas_x += gw + _SFTE_FONT_PADDING;
}

/*
    Retrieves a cached glyph raster, or bakes a new one on cache miss.
    This function is blind to unicode, it expects a pre-resolved TrueType ID and font index from
   `_sfte_font_resolve_rune`.
*/
static inline sfte_glyph *_sfte_font_get_glyph(sfte_stack *stack, sfte_font_cache *cache,
                                               uint16_t glyph_id, uint8_t font_idx) {
    if (glyph_id == 0) return NULL;

    uint32_t h = (glyph_id ^ (font_idx << 16)) % SFTE_FONT_GLYPH_CAP;

    // hash map logic
    for (uint16_t i = 0; i < SFTE_FONT_GLYPH_CAP; ++i) {
        uint16_t idx = (h + i) % SFTE_FONT_GLYPH_CAP;

        if (cache->glyphs[idx].glyph_id == glyph_id && cache->glyphs[idx].font_idx == font_idx)
            return &cache->glyphs[idx];                  // Cache hit
        if (cache->glyphs[idx].glyph_id != 0) continue;  // Collision

        // Cache miss, look up bounds across primary and fallback fonts
        int32_t adv = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        SFTE_FONT_BOUNDS(&cache->info[font_idx], glyph_id, cache->scales[font_idx], &adv, &x0, &y0,
                         &x1, &y1);

        sfte_glyph *g = &cache->glyphs[idx];
        g->glyph_id = glyph_id;
        g->xadvance = (int)(adv * cache->scales[font_idx] + 0.5f);
        g->xoff = x0;
        g->yoff = y0;
        g->font_idx = font_idx;

        _sfte_font_pack_and_bake(stack, cache, g, font_idx, glyph_id, x1 - x0, y1 - y0);

        return g;
    }

    return NULL;
}

/*
    Resolves a unicode rune to a specific font index and TrueType glyph ID.
    Handles cascading fallback fonts.
    Sets both values to 0 if glyph is not found.
*/
static inline void _sfte_font_resolve_rune(sfte_font_cache *cache, sfte_rune rune,
                                           uint8_t *out_font_idx, uint16_t *out_glyph_id) {
    if (rune == 0) rune = ' ';

    for (uint8_t f = 0; f < cache->num_fonts; ++f) {
        uint16_t id = (uint16_t)SFTE_FONT_GET_ID(&cache->info[f], rune);
        if (id != 0) {
            *out_font_idx = f;
            *out_glyph_id = id;
            return;
        }
    }

    *out_font_idx = 0;
    *out_glyph_id = 0;
}

/*
    Purges all glyph atlases and recalculates strict terminal grid metrics.
    Must be called on startup, and whenever the DPI or font size changes.
*/
static inline void _sfte_font_reset_cache(sfte_ctx *ctx) {
    sfte_mux *mux = &ctx->mux;
    sfte_font_state *font = &ctx->font;
    sfte_term *term = sfte_term_get_active(ctx);
    sfte_stack *stack = &term->stack;

    _sfte_font_clear_cache(&font->regular);
    _sfte_font_update_scales(&font->regular, font->cur_size);

#ifdef SFTE_FONT_BOLD
    _sfte_font_clear_cache(&font->bold);
    _sfte_font_update_scales(&font->bold, font->cur_size);
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    _sfte_font_clear_cache(&font->italic);
    _sfte_font_update_scales(&font->italic, font->cur_size);
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    _sfte_font_clear_cache(&font->bold_italic);
    _sfte_font_update_scales(&font->bold_italic, font->cur_size);
#endif  // SFTE_FONT_BOLD_ITALIC

    int32_t ascent_u, descent_u, line_gap_u;
    SFTE_FONT_VMETRICS(&font->regular.info[0], &ascent_u, &descent_u, &line_gap_u);

    float primary_scale = font->regular.scales[0];
    font->ascent = (int)(ascent_u * primary_scale + 0.5f);
    font->descent = (int)(descent_u * primary_scale -
                          0.5f /* - instead of + because descent is natively negative */);
    font->line_gap = (int)(line_gap_u * primary_scale + 0.5f);

    int16_t cell_h = font->ascent - font->descent + font->line_gap;

    // NOTE:
    // Terminal column width is locked to advance of 'M'.
    uint8_t m_font_idx = 0;
    uint16_t m_glyph_id = 0;
    _sfte_font_resolve_rune(&font->regular, 'M', &m_font_idx, &m_glyph_id);
    sfte_glyph *m = _sfte_font_get_glyph(stack, &font->regular, m_glyph_id, m_font_idx);
    int16_t cell_w = m->xadvance;

    for (uint8_t i = 0; i < SFTE_MUX_MAX_WINDOWS; ++i)
        if (mux->terms[i].is_allocated) {
            sfte_viewport_state *vp = &mux->terms[i].viewport;
            vp->cell_width = cell_w;
            vp->cell_height = cell_h;
        }
}

// =================================================================================================
// >>public api
// =================================================================================================

void sfte_font_load_mem(sfte_ctx *ctx, sfte_font_style style, const uint8_t *ttf_data) {
    sfte_font_state *font = &ctx->font;

    sfte_font_cache *cache = _sfte_font_get_cache(ctx, style);
    if (!cache || !ttf_data || cache->num_fonts >= SFTE_FONT_MAX_COUNT) return;

    uint8_t idx = cache->num_fonts++;
    cache->ttf_buf[idx] = (uint8_t *)ttf_data;
    cache->owns_ttf_buf[idx] = 0;

    if (idx == 0) {
        if (style == SFTE_FONT_STYLE_REGULAR && idx == 0) font->cur_size = SFTE_FONT_DEFAULT_SIZE;

        if (!cache->atlas_pxs) {
            cache->atlas_pxs = (uint8_t *)SFTE_MALLOC(SFTE_FONT_ATLAS_SIZE * SFTE_FONT_ATLAS_SIZE);
            SFTE_ASSERT(cache->atlas_pxs, "failed to allocate font atlas");
        }

        if (!cache->glyphs) {
            cache->glyphs = (sfte_glyph *)SFTE_CALLOC(SFTE_FONT_GLYPH_CAP, sizeof(sfte_glyph));
            SFTE_ASSERT(cache->glyphs, "failed to allocate glyphs storage");
        }
    }

    SFTE_FONT_INIT(&cache->info[idx], cache->ttf_buf[idx]);

#if SFTE_FONT_LIGATURES
    _sfte_shaper_init(&cache->shaper[idx], cache->ttf_buf[idx]);
#endif  // SFTE_FONT_LIGATURES

    if (style == SFTE_FONT_STYLE_REGULAR && idx == 0)
        _sfte_font_reset_cache(ctx);
    else {
        float tweak = _sfte_font_scales[idx];
        if (tweak <= 0.0f) tweak = 1.0f;
        cache->scales[idx] = SFTE_FONT_GET_SCALE(&cache->info[idx], font->cur_size * tweak);
    }

    _SFTE_INFO(ctx, FONT_LOADED);
}

void sfte_font_load_file(sfte_ctx *ctx, sfte_font_style style, const char *path) {
    sfte_font_cache *cache = _sfte_font_get_cache(ctx, style);
    if (!cache) return;

    FILE *f = fopen(path, "rb");
    if (!f) _SFTE_ERROR(ctx, FONT_LOAD_FAIL, path);

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = (uint8_t *)SFTE_MALLOC(size);
    SFTE_ASSERT(fread(buf, 1, size, f) == size, "failed to read font file");
    fclose(f);

    sfte_font_load_mem(ctx, style, buf);
    cache->owns_ttf_buf[cache->num_fonts - 1] = 1;
}
