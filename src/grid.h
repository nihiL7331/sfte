#pragma once

#include "src/types.h"
#include "src/util.h"
#include <stdint.h>
#include <string.h>

// =================================================================================================
// >>reflow
// =================================================================================================
#if SFTE_TERM_REFLOW
/*
    Pushes a single cell into the temporary linear buffer.
*/
static inline void _sfte_reflow_push(sfte_reflow_state *st, sfte_cell cell, uint8_t is_cursor) {
#if SFTE_FONT_WIDE_CHARS
    // If we're pushing a double-width character and we're at last column, wrap early.
    if ((cell.attr & _SFTE_ATTR_WIDE) && st->reflow_col == st->new_cols - 1) {
        sfte_cell space = cell;
        space.rune = ' ';
        space.fg = _SFTE_COLOR_FG_DEFAULT;
        space.bg = _SFTE_COLOR_BG_DEFAULT;
        space.attr = 0;
        space.wrapped = 1;
        st->temp_rows[st->reflow_row * st->new_cols + st->reflow_col] = space;
        st->reflow_col = 0;
        st->reflow_row++;
    }
#endif  // SFTE_FONT_WIDE_CHARS

    if (st->reflow_col == st->new_cols) {
        st->temp_rows[st->reflow_row * st->new_cols + st->new_cols - 1].wrapped = 1;
        st->reflow_col = 0;
        st->reflow_row++;
    }

    if (is_cursor) {
        st->new_col = st->reflow_col;
        st->new_row = st->reflow_row;
    }

    cell.wrapped = 0;
    st->temp_rows[st->reflow_row * st->new_cols + st->reflow_col] = cell;
    st->reflow_col++;
}

/*
    Calculates the true length of a line by trimming empty trailing spaces.
    If the cursor is on this line (cursor_cx >= 0), it ensures the length includes the cursor.
*/
static inline int16_t _sfte_reflow_get_len(sfte_cell *row, int16_t cols, int16_t cursor_col) {
    if (row[cols - 1].wrapped) return cols;
    int16_t len = cols;

    // Terminals pad lines with empty cells. When resizing, we must trim these
    // to prevent invisible padding from wrapping and creating artificial blank lines.
    // However, a cell is only truly empty if:
    // 1. it contains a space or null rune,
    // 2. its bg is the default color (colored spaces are used by TUIs to draw UI).
    // Finally, we must NEVER trim the cell where the cursor is currently sitting,
    // even if its a blank space.
    while (len > 0 && (row[len - 1].rune == ' ' || row[len - 1].rune == 0) &&
           row[len - 1].bg == SFTE_COLOR_BG) {
        if (cursor_col >= 0 && len - 1 == cursor_col) break;
        len--;
    }

    // Ensure the cursor isn't trimmed out.
    // Handles cases where cursor_cx is beyond the actual string length, e.g., wrap pending
    // states.
    if (cursor_col >= 0 && len <= cursor_col) len = cursor_col + 1;

    return len;
}

/*
    Processes a single row, extracting length, pushing cells, and handling cursor edge cases.
*/
static inline void _sfte_reflow_process_row(sfte_cell *row, int16_t cols, int16_t cursor_col,
                                            sfte_reflow_state *st) {
    int16_t len = _sfte_reflow_get_len(row, cols, cursor_col);

    for (int16_t c = 0; c < len; ++c) _sfte_reflow_push(st, row[c], c == cursor_col);

    if (cursor_col >= 0 && cursor_col >= len) {
        if (st->reflow_col == st->new_cols) {
            st->temp_rows[st->reflow_row * st->new_cols + st->new_cols - 1].wrapped = 1;
            st->reflow_col = 0;
            st->reflow_row++;
        }
        st->new_col = st->reflow_col;
        st->new_row = st->reflow_row;
    }

    if (!row[cols - 1].wrapped) {
        st->reflow_col = 0;
        st->reflow_row++;
    }
}

/*
    Flattens the terminals history (both the scrollback ring buffer and active 2D grid)
    into a single continuous 1D stream.

    To wrap text accurately across the boundaries of the screen and the scrollback, it
    must evaluate the text from the oldest recorded line down to the newest.
    Because the scrollback is a circular array, we must do modular rithmetic backwards
    from `sb_head` to read it in chronological order.
    The active cursor can only exist on the live screen, so we pass `-1`
    during scrollback iteration to explicitly trim trailing whitespace on all historical lines.
*/
static inline void _sfte_reflow_grid_into_linear(sfte_term *term, sfte_cell *main_old,
                                                 int16_t grid_off_old, sfte_reflow_state *st) {
    const sfte_viewport_state *vp = &term->viewport;

#if SFTE_TERM_SCROLLBACK_CAP
    for (int32_t i = 0; i < vp->sb_len; ++i) {
        uint32_t ring_idx = (vp->sb_head - vp->sb_len + i + vp->sb_cap) % vp->sb_cap;
        sfte_cell *row = &term->scrollback[ring_idx * vp->cols];
        _sfte_reflow_process_row(row, vp->cols, -1, st);
    }
#endif  // SFTE_TERM_SCROLLBACK_CAP

    // reflow live grid
    st->is_live = 1;
    for (int16_t r = 0; r < vp->rows; ++r) {
        int32_t phys_r = (r + grid_off_old) % vp->rows;
        if (phys_r < 0) phys_r += vp->rows;

        sfte_cell *row = &main_old[phys_r * vp->cols];
        int16_t cursor_col = (r == st->target_old_row) ? st->target_old_col : -1;

        _sfte_reflow_process_row(row, vp->cols, cursor_col, st);
    }
}

/*
    Allocates the temporary linear buffer and performs the topological text reflow.
*/
static sfte_cell *_sfte_reflow_linearize(sfte_term *term, sfte_cell *main_old, int16_t grid_off_old,
                                         int16_t new_cols, int16_t new_rows,
                                         sfte_reflow_state *st) {
    const sfte_viewport_state *vp = &term->viewport;

    // term->viewport.cols / new_cols calculates the raw expansion factor.
    // We add +2 to to this multiplier:
    // +1 to account for integer division truncation, and
    // +1 as a safety margin for double-width characters that force early wraps.
    int16_t max_temp_rows = (
#if SFTE_TERM_SCROLLBACK_CAP
                                vp->sb_len +
#endif  // SFTE_TERM_SCROLLBACK_CAP
                                vp->rows) *
                            (vp->cols / new_cols + 2);

    if (max_temp_rows < new_rows) max_temp_rows = new_rows;

    sfte_cell *temp_rows = (sfte_cell *)SFTE_CALLOC(max_temp_rows * new_cols, sizeof(sfte_cell));
    SFTE_ASSERT(temp_rows, "failed to allocate temporary row data");

    st->new_cols = new_cols;
    st->temp_rows = temp_rows;
    st->reflow_col = 0, st->reflow_row = 0;
    st->new_col = 0, st->new_row = 0;
    st->is_live = 0;

    // Walks the old grids and populates st->temp_rows
    _sfte_reflow_grid_into_linear(term, main_old, grid_off_old, st);

    return temp_rows;
}

/*
    Maps the linearized reflow buffer back into distinct 2D main and scrollback grids.
    Modifies `st->new_cy` to reflect its clamped viewport position.
*/
static inline void _sfte_reflow_extract_view(const sfte_viewport_state *vp, int16_t new_cols,
                                             int16_t new_rows, int16_t target_row,
                                             sfte_reflow_state *st, sfte_resize_buffers *out) {
    (void)vp;
    int32_t total_lines = st->reflow_row + (st->reflow_col > 0 ? 1 : 0);

    out->main_grid = (sfte_cell *)SFTE_CALLOC(new_cols * new_rows, sizeof(sfte_cell));
    SFTE_ASSERT(out->main_grid, "failed to allocate resized terminal grid");

    // Clamp the cursors target row to the new screen height
    int16_t r = target_row < new_rows ? target_row : new_rows - 1;

    // Map the viewport to match the cursors visual row
    int32_t screen_top = st->new_row - r;

    // Clamp the viewport to the absolute limits of the text buffer
    int32_t max_top = total_lines - new_rows;
    if (max_top < 0) max_top = 0;
    screen_top = _SFTE_CLAMP(screen_top, 0, max_top);

#if SFTE_TERM_SCROLLBACK_CAP
    out->sb_grid = (sfte_cell *)SFTE_CALLOC(vp->sb_cap * new_cols, sizeof(sfte_cell));
    SFTE_ASSERT(out->sb_grid, "failed to allocate resized scrollback");

    // The scrollback is above `screen_top`
    out->sb_lines = screen_top;
    if (out->sb_lines > vp->sb_cap) out->sb_lines = vp->sb_cap;
    int32_t sb_start = screen_top - out->sb_lines;

    if (out->sb_lines > 0)
        memcpy(out->sb_grid, &st->temp_rows[sb_start * new_cols],
               (size_t)out->sb_lines * new_cols * sizeof(sfte_cell));
#endif  // SFTE_TERM_SCROLLBACK_CAP

    int32_t copy_lines = _SFTE_CLAMP(total_lines - screen_top, 0, new_rows);
    if (copy_lines > 0)
        memcpy(out->main_grid, &st->temp_rows[screen_top * new_cols],
               (size_t)copy_lines * new_cols * sizeof(sfte_cell));

    for (int32_t i = copy_lines * new_cols; i < new_rows * new_cols; ++i) {
        out->main_grid[i].rune = ' ';
        out->main_grid[i].bg = _SFTE_COLOR_BG_DEFAULT;
        out->main_grid[i].fg = _SFTE_COLOR_FG_DEFAULT;
        out->main_grid[i].attr = 0;
        out->main_grid[i].wrapped = 0;
    }

    st->new_row = _SFTE_CLAMP(st->new_row - screen_top, 0, new_rows - 1);
}

/*
    Orchestrates the reflow pipeline and returns the newly allocated grid buffers.
*/
static sfte_resize_buffers _sfte_reflow_generate_buffers(sfte_term *term, sfte_cell *main_old,
                                                         int16_t grid_off_old, int16_t new_cols,
                                                         int16_t new_rows, int16_t target_col,
                                                         int16_t target_row) {
    const sfte_viewport_state *vp = &term->viewport;

    sfte_reflow_state st = {
        .target_old_col = target_col,
        .target_old_row = target_row,
    };

    sfte_cell *temp = _sfte_reflow_linearize(term, main_old, grid_off_old, new_cols, new_rows, &st);

    sfte_resize_buffers out = {0};
    _sfte_reflow_extract_view(vp, new_cols, new_rows, target_row, &st, &out);

    SFTE_FREE(temp);

    out.new_col = st.new_col;
    out.new_row = st.new_row;

    return out;
}
#endif  // SFTE_TERM_REFLOW
// =================================================================================================
// >>grid
// =================================================================================================

// -------------------------------------------------------------------------------------------------
// >grid helpers
// -------------------------------------------------------------------------------------------------

/*
    Converts a visual screen row (0 to ctx->term.viewport.rows-1) into a logical row.
    Projects the coordinate into the scrollback history if `SFTE_TERM_SCROLLBACK_CAP` isn't 0.
*/
static inline int32_t _sfte_grid_vis2log(const sfte_viewport_state *vp, int16_t visual_row) {
#if SFTE_TERM_SCROLLBACK_CAP
    return (int32_t)visual_row - vp->sb_off;
#else   // !SFTE_TERM_SCROLLBACK_CAP
    (void)vp;
    return (int32_t)visual_row;
#endif  // !SFTE_TERM_SCROLLBACK_CAP
}

/*
    Converts a logical row (chronological) into a visual screen row.
    Returns negative values if the row is currently hidden in the scrollback.
 */
static inline int32_t _sfte_grid_log2vis(const sfte_viewport_state *vp, int32_t logical_row) {
#if SFTE_TERM_SCROLLBACK_CAP
    return logical_row + vp->sb_off;
#else   // !SFTE_TERM_SCROLLBACK_CAP
    (void)vp;
    return logical_row;
#endif  // !SFTE_TERM_SCROLLBACK_CAP
}

/*
    Converts a visual 2D coordinate into a 1D physical memory index.
*/
static inline uint32_t _sfte_grid_get_idx(const sfte_viewport_state *vp, int16_t col,
                                          int32_t logical_row) {
    int32_t r = (logical_row + vp->grid_off) % vp->rows;
    if (r < 0) r += vp->rows;
    return r * vp->cols + col;
}

/*
    Retrieves a pointer to a specific cell in memory using logical grid coordinates.

    A negative `logical_row` seamlessly reaches back into the scrollback ring buffer.
    Assumes the caller has already validated that `logical_row` doesn't exceed scrollback
   length.
*/
static inline sfte_cell *_sfte_grid_get_cell(const sfte_term *term, int16_t col,
                                             int32_t logical_row) {
    const sfte_viewport_state *vp = &term->viewport;

#if SFTE_TERM_SCROLLBACK_CAP
    if (logical_row >= 0)
        return &term->cells[_sfte_grid_get_idx(vp, col, logical_row)];
    else {
        int32_t ring_row = ((vp->sb_head + logical_row) % vp->sb_cap + vp->sb_cap) % vp->sb_cap;
        return &term->scrollback[ring_row * vp->cols + col];
    }
#else   // !SFTE_TERM_SCROLLBACK_CAP
    return &term->cells[_sfte_grid_get_idx(vp, col, logical_row)];
#endif  // !SFTE_TERM_SCROLLBACK_CAP
}

/*
    Returns the background color of a cell.
    The behavior of this function depends on whether `SFTE_COLOR_TRUECOLOR` is 1.
    If it is, it just returns the value.
    If it isn't, it treats the color value as an index to the 256-color palette.
*/
static inline uint32_t _sfte_grid_get_bg(sfte_cell *cell) {
#if SFTE_COLOR_TRUECOLOR
    return cell->bg ? cell->bg : SFTE_COLOR_BG;
#endif  // SFTE_COLOR_TRUECOLOR
    if (cell->bg == _SFTE_COLOR_BG_DEFAULT || (!cell->bg && !cell->rune)) return SFTE_COLOR_BG;
    return _sfte_palette_256[cell->bg & 0xFF];
}

/*
    Returns the foreground color of a cell.
    The behavior of this function depends on whether `SFTE_COLOR_TRUECOLOR` is 1.
    If it is, it just returns the value.
    If it isn't, it treats the color value as an index to the 256-color palette.
*/
static inline uint32_t _sfte_grid_get_fg(sfte_cell *cell) {
#if SFTE_COLOR_TRUECOLOR
    return cell->fg ? cell->fg : SFTE_COLOR_FG;
#endif  // SFTE_COLOR_TRUECOLOR
    if (cell->fg == _SFTE_COLOR_FG_DEFAULT || (!cell->fg && !cell->rune)) return SFTE_COLOR_FG;
    return _sfte_palette_256[cell->fg & 0xFF];
}

/*
    Returns the underline color of a cell.
    The behavior of this function depends on whether `SFTE_COLOR_TRUECOLOR` is 1.
    If it is, it just returns the value.
    If it isn't, it treats the color value as an index to the 256-color palette.

    If underline color is not set, defaults to foreground default color.
    If underline colors are not supported, returns foreground default color.
*/
static inline uint32_t _sfte_grid_get_ul(sfte_cell *cell) {
#if SFTE_UNDERLINE_COLORED
#if SFTE_COLOR_TRUECOLOR
    return cell->ul_color ? cell->ul_color : SFTE_COLOR_FG;
#endif  // SFTE_COLOR_TRUECOLOR

    if (cell->ul_color == _SFTE_COLOR_FG_DEFAULT || !cell->ul_color) return SFTE_COLOR_FG;
    return _sfte_palette_256[cell->ul_color & 0xFF];
#endif  // SFTE_UNDERLINE_COLORED
    (void)cell;
    return SFTE_COLOR_FG;
}

/*
    Flags a range of logical rows as dirty to force a redraw.
    Automatically handles min/max sorting, scrollback offset mapping,
    and clamping to the visible physical screen.
*/
static inline void _sfte_grid_dirty_rows(sfte_term *term, int32_t logical_row1,
                                         int32_t logical_row2) {
    const sfte_viewport_state *vp = &term->viewport;

    int32_t min_logical_r = logical_row1 < logical_row2 ? logical_row1 : logical_row2;
    int32_t max_logical_r = logical_row1 > logical_row2 ? logical_row1 : logical_row2;

    for (int32_t logical_r = min_logical_r; logical_r <= max_logical_r; ++logical_r)
        if (logical_r >= 0) {
            int32_t row_start_idx = _sfte_grid_get_idx(vp, 0, logical_r);
            for (int16_t c = 0; c < vp->cols; ++c) term->cells[row_start_idx + c].dirty = 1;
        }
#if SFTE_TERM_SCROLLBACK_CAP
        else {
            int32_t ring_row = ((vp->sb_head + logical_r) % vp->sb_cap + vp->sb_cap) % vp->sb_cap;
            int32_t row_start_idx = ring_row * vp->cols;
            for (int16_t c = 0; c < vp->cols; ++c) term->scrollback[row_start_idx + c].dirty = 1;
        }
#endif  // SFTE_TERM_SCROLLBACK_CAP
}

/*
    Flags a rectangular region of the grid as dirty, forcing a redraw on the next frame.
    Safely clips coordinates that fall outside the terminal boundaries.
    Routes into the scrollback buffer if start_logical_r < 0.
*/
static inline void _sfte_grid_dirty_rect(sfte_term *term, int16_t start_col,
                                         int32_t start_logical_row, int16_t cols, int16_t rows) {
    const sfte_viewport_state *vp = &term->viewport;

    int16_t start_c = _SFTE_CLAMP(start_col, 0, vp->cols - 1);
    int16_t end_c = _SFTE_CLAMP(start_col + cols - 1, 0, vp->cols - 1);
    int32_t end_logical_r = start_logical_row + rows - 1;

    for (int32_t logical_r = start_logical_row; logical_r <= end_logical_r; ++logical_r)
        for (int16_t c = start_c; c <= end_c; ++c)
            _sfte_grid_get_cell(term, c, logical_r)->dirty = 1;
}

/*
    Flags a contiguous 1D range of grid cells [start_idx; start_idx + cnt) as dirty.
    Does NOT perform boundary checking, uses asserts for boundary safety
    to avoid branch-checking overhead in RELEASE builds.
*/
static inline void _sfte_grid_dirty_range(sfte_term *term, uint32_t start_idx, uint32_t cnt) {
    const sfte_viewport_state *vp = &term->viewport;
    SFTE_ASSERT(start_idx + cnt <= (uint32_t)(vp->cols * vp->rows), "dirty_range index overflow");

    for (uint32_t i = 0; i < cnt; ++i) term->cells[start_idx + i].dirty = 1;
}

/*
    Calculates how many terminal cells are spanned by a given pixel dimension,
    accounting for an arbitrary pixel offset within the starting cell.
*/
static inline int16_t _sfte_grid_span(int32_t px_len, int32_t px_off, int32_t cell_px) {
    return (px_len + px_off + cell_px - 1) / cell_px;
}

#if SFTE_IMG_SIXEL
/*
    Deletes Sixel image placements that intersect with a cleared grid region.
    Automatically frees Sixel image data if the reference count drops to 0.
    Kitty images are ignored as they require explicit terminal delete commands.
*/
static inline void _sfte_grid_clear_sixel(sfte_term *term, int32_t start_idx, int32_t cnt) {
    const sfte_modes_state *modes = &term->modes;
    const sfte_viewport_state *vp = &term->viewport;
    sfte_image_state *images = &term->images;

    for (uint32_t i = 0; i < images->placements_len; ++i) {
        sfte_img_placement *p = &images->placements[i];
        if (!p->is_sixel) continue;
#if SFTE_TERM_ALT_SCREEN
        // Prevent active-screen clears from wiping out hidden-screen images
        if (p->alt_screen != modes->alt_active) continue;
#endif  // SFTE_TERM_ALT_SCREEN

        sfte_img *img = _sfte_img_find(images, p->img_id);
        if (!img) continue;

        int16_t cols = _sfte_grid_span(img->width, p->x_off, vp->cell_width);
        int16_t rows = _sfte_grid_span(img->height, p->y_off, vp->cell_height);

        uint8_t overlap = 0;
        for (int16_t r = p->start_row; r < p->start_row + rows && !overlap; ++r)
            for (int16_t c = p->start_col; c < p->start_col + cols; ++c) {
                int32_t cell_idx = r * vp->cols + c;
                if (cell_idx >= start_idx && cell_idx < start_idx + cnt) {
                    overlap = 1;
                    break;
                }
            }

        if (overlap) {
            img->ref_cnt--;
            images->placements[i--] = images->placements[--images->placements_len];
        }
    }

    for (uint32_t i = 0; i < images->pool_len; ++i) {
        if (images->pool[i].ref_cnt || !images->pool[i].is_sixel) continue;
        SFTE_FREE(images->pool[i].pixels);
        images->pool[i--] = images->pool[--images->pool_len];
    }
}
#endif  // SFTE_IMG_SIXEL

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
/*
    Translates image placements up/down during screen scroll.
    Deletes images that scroll entirely out of the scrollback buffer.
*/
static inline void _sfte_grid_scroll_images(sfte_term *term, int16_t lines, int16_t top,
                                            int16_t bot) {
    const sfte_modes_state *modes = &term->modes;
    const sfte_viewport_state *vp = &term->viewport;
    sfte_image_state *images = &term->images;

    for (uint32_t i = 0; i < images->placements_len; ++i) {
        sfte_img_placement *p = &images->placements[i];
#if SFTE_TERM_ALT_SCREEN
        if (p->alt_screen != modes->alt_active) continue;
#endif  // SFTE_TERM_ALT_SCREEN

        // An image should only scroll if it falls into one of two categories:
        // 1. it is inside the active scrolling margins (or sitting just on the edge at bot+1)
        // 2. It is in the scrollback buffer (<0), AND we are doing a full-screen scroll
        // (top==0) which pushes new lines into the scrollback.
        if ((p->start_row >= top && p->start_row <= bot + 1) || (top == 0 && p->start_row < 0))
            p->start_row -= lines;

        sfte_img *img = _sfte_img_find(images, p->img_id);
        if (!img) continue;

        int32_t rows = _sfte_grid_span(img->height, p->y_off, vp->cell_height);
        if (p->start_row + rows <= -SFTE_TERM_SCROLLBACK_CAP) {
            img->ref_cnt--;
            images->placements[i--] = images->placements[--images->placements_len];
        }
    }
}
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

#if SFTE_TERM_SCROLLBACK_CAP
/*
    Pushes lines scrolling off the top of the screen into the ring buffer.
*/
static inline void _sfte_grid_push_scrollback(sfte_term *term, int16_t lines) {
    const sfte_modes_state *modes = &term->modes;
    sfte_viewport_state *vp = &term->viewport;

#if SFTE_TERM_ALT_SCREEN
    if (modes->alt_active) return;
#endif  // SFTE_TERM_ALT_SCREEN

    for (int16_t i = 0; i < lines; ++i) {
        int32_t ring_idx = vp->sb_head * vp->cols;
        uint32_t screen_idx = _sfte_grid_get_idx(vp, 0, i);
        memcpy(&term->scrollback[ring_idx], &term->cells[screen_idx], vp->cols * sizeof(sfte_cell));

        vp->sb_head = (vp->sb_head + 1) % vp->sb_cap;
        if (vp->sb_len < vp->sb_cap) vp->sb_len++;
    }

    if (vp->sb_off > 0) {
        vp->sb_off = _SFTE_CLAMP(vp->sb_off + lines, 0, vp->sb_len);
        vp->last_sb_off = vp->sb_off;
    }
}
#endif  // SFTE_TERM_SCROLLBACK_CAP

// -------------------------------------------------------------------------------------------------
// >grid core
// -------------------------------------------------------------------------------------------------

/*
    Wipes a 1D range of cells, resetting them to ' ' and applying the
    currently active terminal foreground, background and text attributes.
    Also triggers the deletion of any Sixel images that intersect that region.
    Does NOT move the cursor.
*/
static inline void _sfte_grid_clear_cells(sfte_term *term, uint32_t start_idx, uint32_t cnt) {
    const sfte_sgr_state *sgr = &term->sgr;

    for (uint32_t i = 0; i < cnt; ++i) {
        sfte_cell *c = &term->cells[start_idx + i];
        c->rune = ' ';
        c->fg = sgr->fg;
        c->bg = sgr->bg;
        c->attr = 0;
        c->dirty = 1;
#if SFTE_UNDERLINE_EXTENDED
        c->ul_style = 0;
#endif  // SFTE_UNDERLINE_EXTENDED
#if SFTE_UNDERLINE_COLORED
        c->ul_color = _SFTE_COLOR_FG_DEFAULT;
#endif  // SFTE_UNDERLINE_COLORED
#if SFTE_TERM_REFLOW
        c->wrapped = 0;
#endif  // SFTE_TERM_REFLOW
#if SFTE_INPUT_HYPERLINKS
        c->link_idx = 0;
#endif  // SFTE_INPUT_HYPERLINKS
    }

#if SFTE_IMG_SIXEL
    _sfte_grid_clear_sixel(term, start_idx, cnt);
#endif  // SFTE_IMG_SIXEL
}

/*
    Wipes `cnt` rows starting from `start_row`, resetting them to ' ' and applying the
    currently active terminal foreground, background and text attributes.
    Also triggers the deletion of any Sixel images that intersect these rows.
    Does NOT move the cursor.
*/
static inline void _sfte_grid_clear_rows(sfte_term *term, int16_t start_row, int16_t cnt) {
    const sfte_viewport_state *vp = &term->viewport;

    for (int16_t r = start_row; r < start_row + cnt; ++r)
        _sfte_grid_clear_cells(term, _sfte_grid_get_idx(vp, 0, r), vp->cols);
}

/*
    Shifts the terminal grid up or down by the specified number of lines.
    Positive values scroll the text UP (moving the viewport down).
    Negative values scroll the text DOWN (moving the viewport up).

    Strictly respects the active scroll margins (`ctx->term.viewport.scroll_top/bottom`).
    Pushes lines that fall off the top margin into the scrollback buffer
    (only if scrolling the primary screen and starting from row 0).

    Synchronizes image placements to scroll with the text.
*/
static inline void _sfte_grid_scroll(sfte_term *term, int16_t lines) {
    sfte_viewport_state *vp = &term->viewport;

    int16_t height = vp->scroll_bot - vp->scroll_top + 1;

    // Clamp the scroll amount to the region height while preserving direction.
    lines = _SFTE_CLAMP(lines, -height, height);

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    _sfte_grid_scroll_images(term, lines, vp->scroll_top, vp->scroll_bot);
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

    if (lines > 0) {  // Scroll up
#if SFTE_TERM_SCROLLBACK_CAP
        if (vp->scroll_top == 0) _sfte_grid_push_scrollback(term, lines);
#endif  // SFTE_TERM_SCROLLBACK_CAP

        if (vp->scroll_top == 0 && vp->scroll_bot == vp->rows - 1)
            vp->grid_off = (vp->grid_off + lines) % vp->rows;
        else {
            int16_t move_cnt = height - lines;
            for (int16_t i = 0; i < move_cnt; ++i) {
                uint32_t dst_idx = _sfte_grid_get_idx(vp, 0, vp->scroll_top + i);
                uint32_t src_idx = _sfte_grid_get_idx(vp, 0, vp->scroll_top + lines + i);
                memcpy(&term->cells[dst_idx], &term->cells[src_idx], vp->cols * sizeof(sfte_cell));
            }
        }

        _sfte_grid_clear_rows(term, vp->scroll_bot - lines + 1, lines);
    } else if (lines < 0) {  // Scroll down
        lines = -lines;

        if (vp->scroll_top == 0 && vp->scroll_bot == vp->rows - 1) {
            vp->grid_off = (vp->grid_off - lines) % vp->rows;
            if (vp->grid_off < 0) vp->grid_off += vp->rows;
        } else {
            int16_t move_cnt = height - lines;
            for (int16_t i = move_cnt - 1; i >= 0; --i) {
                uint32_t dst_idx = _sfte_grid_get_idx(vp, 0, vp->scroll_top + lines + i);
                uint32_t src_idx = _sfte_grid_get_idx(vp, 0, vp->scroll_top + i);
                memcpy(&term->cells[dst_idx], &term->cells[src_idx], vp->cols * sizeof(sfte_cell));
            }
        }

        _sfte_grid_clear_rows(term, vp->scroll_top, lines);
    }

    _sfte_grid_dirty_rows(term, vp->scroll_top, vp->scroll_bot);
}

/*
    Evaluates the cursor's X position against the terminal width and handles wrapping.
    Must be called BEFORE printing a character that might fall off the edge.

    If auto-wrap is ON, it wraps cursor to column 0 of the next line.
    If already at the bottom scroll margin, forces a 1-line scroll upwards.
    Marks the wrapped cell with a `wrapped = 1` flag (used by the reflow engine).

    If auto-wrap is OFF, it clamps the cursor to the final column, causing subsequent
    characters to overwrite each other.
*/
static inline void _sfte_grid_check_wrap(sfte_term *term) {
    const sfte_viewport_state *vp = &term->viewport;
    const sfte_modes_state *modes = &term->modes;
    sfte_cursor_state *cursor = &term->cursor;
    sfte_sgr_state *sgr = &term->sgr;

    if (cursor->col >= vp->cols) {
        if (modes->auto_wrap) {
#if SFTE_TERM_REFLOW
            _sfte_grid_get_cell(term, vp->cols - 1, cursor->row)->wrapped = 1;
#endif  // SFTE_TERM_REFLOW
            cursor->col = 0;
            if (cursor->row == vp->scroll_bot) {
                uint32_t saved_bg = term->sgr.bg;
                sgr->bg = _SFTE_COLOR_BG_DEFAULT;
                _sfte_grid_scroll(term, 1);
                sgr->bg = saved_bg;
            } else if (cursor->row < vp->rows - 1)
                cursor->row++;
        } else
            cursor->col = vp->cols - 1;
    }
}

// -------------------------------------------------------------------------------------------------
// >resize
// -------------------------------------------------------------------------------------------------

#if SFTE_TERM_ALT_SCREEN || !SFTE_TERM_REFLOW
/*
    Performs a simple 2D truncation/padding copy of a grid.
    Used for alt-screens (which don't reflow) and as the primary resizer in non-reflow builds.
    Allocates and returns a new `sfte_cell` array. Caller assumes ownership.
*/
static sfte_cell *_sfte_grid_resize_dumb_copy(sfte_cell *old_grid, int16_t old_cols,
                                              int16_t old_rows, int16_t new_cols,
                                              int16_t new_rows) {
    sfte_cell *new_grid = (sfte_cell *)SFTE_CALLOC(new_cols * new_rows, sizeof(sfte_cell));
    if (!old_grid) return new_grid;
    SFTE_ASSERT(new_grid, "failed to allocate resized grid");

    int16_t min_cols = new_cols < old_cols ? new_cols : old_cols;
    int16_t min_rows = new_rows < old_rows ? new_rows : old_rows;

    for (int16_t r = 0; r < min_rows; ++r)
        for (int16_t c = 0; c < min_cols; ++c)
            new_grid[r * new_cols + c] = old_grid[r * old_cols + c];

    return new_grid;
}
#endif  // SFTE_TERM_ALT_SCREEN || !SFTE_TERM_REFLOW

/*
    Reallocates the tab stops array and populates new columns with default intervals.
*/
static inline void _sfte_grid_resize_tabs(sfte_term *term, int16_t old_cols, int16_t new_cols) {
    uint8_t *new_tabs = (uint8_t *)SFTE_MALLOC(new_cols);
    SFTE_ASSERT(new_tabs, "failed to allocate new tab stops");

    uint8_t tw = term->tab_width ? term->tab_width : SFTE_TERM_TAB_WIDTH;
    for (int16_t i = 0; i < new_cols; ++i) {
        if (i < old_cols)
            new_tabs[i] = term->tab_stops[i];
        else
            new_tabs[i] = (i % tw == 0);
    }

    SFTE_FREE(term->tab_stops);
    term->tab_stops = new_tabs;
}

/*
    Reallocates all terminal buffers (main, alt, scrollback, tab stops) to match new dimensions.
    Assumes ownership of freeing the old ctx->term.cells arrays.

    if SFTE_TERM_REFLOW is enabled, performs a topological wrap/unwrap of the main screen text.
    If SFTE_TERM_REFLOW is disabled, performs a simple 2D truncation/padding copy.
    Alt-screen grids are always dumb-copied and never reflowed.
*/
static inline void _sfte_grid_resize(sfte_term *term, int16_t new_cols, int16_t new_rows) {
    if (new_cols < 1 || new_rows < 1) return;

    sfte_render_buffers *render = &term->render;
    const sfte_modes_state *modes = &term->modes;
    sfte_viewport_state *vp = &term->viewport;
    sfte_saved_state *saved = &term->saved;
    sfte_cursor_state *cursor = &term->cursor;

#if SFTE_FONT_LIGATURES
    render->shaper_ids = (uint16_t *)SFTE_REALLOC(render->shaper_ids, new_cols * sizeof(uint16_t));
    render->row_hashes = (uint64_t *)SFTE_REALLOC(render->row_hashes, new_rows * sizeof(uint64_t));
    memset(render->row_hashes, 0, new_rows * sizeof(uint64_t));  // Force cache miss on resize
    render->row_shaper_ids = (uint16_t *)SFTE_REALLOC(render->row_shaper_ids,
                                                      new_rows * new_cols * sizeof(uint16_t));
#endif  // SFTE_FONT_LIGATURES
    render->ids = (uint16_t *)SFTE_REALLOC(render->ids, new_cols * sizeof(uint16_t));
    render->font_indices = (uint8_t *)SFTE_REALLOC(render->font_indices,
                                                   new_cols * sizeof(uint8_t));
    render->target_caches = (sfte_font_cache **)SFTE_REALLOC(render->target_caches,
                                                             new_cols * sizeof(sfte_font_cache *));

#if SFTE_TERM_REFLOW
    int16_t grid_off_old = vp->grid_off;
#endif  // SFTE_TERM_REFLOW
    int16_t old_cols = vp->cols;
#if SFTE_TERM_ALT_SCREEN || !SFTE_TERM_REFLOW
    int16_t old_rows = vp->rows;
#endif  // SFTE_TERM_ALT_SCREEN || !SFTE_TERM_REFLOW

#if SFTE_TERM_ALT_SCREEN
    sfte_cell *main_old = modes->alt_active ? term->alt_cells : term->cells;
    sfte_cell *alt_old = modes->alt_active ? term->cells : NULL;
    int16_t target_col = modes->alt_active ? saved->col[0] : cursor->col;
    int16_t target_row = modes->alt_active ? saved->row[0] : cursor->row;
#else
    sfte_cell *main_old = term->cells;
    int16_t target_col = cursor->col;
    int16_t target_row = cursor->row;
#endif  // !SFTE_TERM_ALT_SCREEN

    sfte_resize_buffers out = {0};

#if SFTE_TERM_REFLOW
    out = _sfte_reflow_generate_buffers(term, main_old, grid_off_old, new_cols, new_rows,
                                        target_col, target_row);
#else  // !SFTE_TERM_REFLOW
    out.main_grid = _sfte_grid_resize_dumb_copy(main_old, old_cols, old_rows, new_cols, new_rows);
    out.new_col = _SFTE_CLAMP(target_col, 0, new_cols - 1);
    out.new_row = _SFTE_CLAMP(target_row, 0, new_rows - 1);
#if SFTE_TERM_SCROLLBACK_CAP
    out.sb_grid = (sfte_cell *)SFTE_CALLOC(vp->sb_cap * new_cols, sizeof(sfte_cell));
    out.sb_lines = 0;
#endif  // SFTE_TERM_SCROLLBACK_CAP
#endif  // !SFTE_TERM_REFLOW

#if SFTE_TERM_ALT_SCREEN
    // Alt screen uses dumb copy, since it's never reflowed
    sfte_cell *alt_new = NULL;
    if (alt_old)
        alt_new = _sfte_grid_resize_dumb_copy(alt_old, old_cols, old_rows, new_cols, new_rows);

    // Only free old grids if we got a different pointer
    if (main_old && main_old != out.main_grid && main_old != alt_new) SFTE_FREE(main_old);
    if (alt_old && alt_old != out.main_grid && alt_old != alt_new) SFTE_FREE(alt_old);

    term->cells = modes->alt_active ? alt_new : out.main_grid;

    // Don't destroy the alt screen if resized while on main screen
    if (modes->alt_active) {
        term->alt_cells = out.main_grid;
        saved->col[0] = out.new_col;
        saved->row[0] = out.new_row;
        cursor->col = _SFTE_CLAMP(cursor->col, 0, new_cols - 1);
        cursor->row = _SFTE_CLAMP(cursor->row, 0, new_rows - 1);
    } else {
        term->alt_cells = alt_new ? alt_new : alt_old;
        term->cursor.col = out.new_col;
        term->cursor.row = out.new_row;
    }
#else   // !SFTE_TERM_ALT_SCREEN
    if (main_old && main_old != out.main_grid) SFTE_FREE(main_old);
    term->cells = out.main_grid;
    cursor->col = out.new_col;
    cursor->row = out.new_row;
#endif  // !SFTE_TERM_ALT_SCREEN

#if SFTE_TERM_SCROLLBACK_CAP
    if (term->scrollback) SFTE_FREE(term->scrollback);
    term->scrollback = out.sb_grid;
    vp->sb_head = out.sb_lines % vp->sb_cap;
    vp->sb_off = 0;
    vp->sb_len = out.sb_lines;
#endif  // SFTE_TERM_SCROLLBACK_CAP

    term->viewport.grid_off = 0;
    term->viewport.cols = new_cols;
    term->viewport.rows = new_rows;
    term->viewport.scroll_top = 0;
    term->viewport.scroll_bot = new_rows - 1;

#if SFTE_INPUT_MOUSE && SFTE_INPUT_SELECTION
    int32_t min_sel_r = 0;
#if SFTE_TERM_SCROLLBACK_CAP
    min_sel_r = -vp->sb_len;
#endif  // SFTE_TERM_SCROLLBACK_CAP
    term->mouse.sel_start_col = _SFTE_CLAMP(term->mouse.sel_start_col, 0, new_cols - 1);
    term->mouse.sel_end_col = _SFTE_CLAMP(term->mouse.sel_end_col, 0, new_cols - 1);
    term->mouse.sel_start_row = _SFTE_CLAMP(term->mouse.sel_start_row, min_sel_r, new_rows - 1);
    term->mouse.sel_end_row = _SFTE_CLAMP(term->mouse.sel_end_row, min_sel_r, new_rows - 1);
    if (!term->mouse.sel_dragging) term->mouse.sel_active = 0;
#endif  // SFTE_INPUT_MOUSE && SFTE_INPUT_SELECTION

    _sfte_grid_resize_tabs(term, old_cols, new_cols);
    _sfte_grid_dirty_range(term, 0, new_cols * new_rows);
}

// =================================================================================================
// >>rune
// =================================================================================================

/*
    Stamps the active terminal cursor styling onto a specific grid cell.
    Isolates all feature-toggle macros to keep call sites clean.
    Does NOT advance the cursor.
*/
static inline void _sfte_rune_stamp_cell(sfte_term *term, uint32_t idx, sfte_rune rune,
                                         uint8_t extra_attr) {
    const sfte_sgr_state *sgr = &term->sgr;
#if SFTE_INPUT_HYPERLINKS
    const sfte_link_state *links = &term->links;
#endif  // SFTE_INPUT_HYPERLINKS

    sfte_cell *c = &term->cells[idx];
    c->rune = rune;
    c->fg = sgr->fg;
    c->bg = sgr->bg;
    c->attr = sgr->attr | extra_attr;
    c->dirty = 1;

#if SFTE_INPUT_HYPERLINKS
    c->link_idx = links->cur_idx;
#endif  // SFTE_INPUT_HYPERLINKS
#if SFTE_UNDERLINE_EXTENDED
    c->ul_style = sgr->ul_style;
#endif  // SFTE_UNDERLINE_EXTENDED
#if SFTE_UNDERLINE_COLORED
    c->ul_color = sgr->ul_color;
#endif  // SFTE_UNDERLINE_COLORED
}

/*
    Inserts a decoded unicode rune into the terminal grid at the current cursor position.
    Combining characters (these of width 0) are appended to the previous logical cell.
    Double-width characters (these of width 2) if placed on the last column, the column
    is left blank, the line wraps early, and the character is placed on the next line.
*/
static inline void _sfte_rune_insert(sfte_term *term, sfte_rune rune) {
    sfte_cursor_state *cursor = &term->cursor;
    const sfte_viewport_state *vp = &term->viewport;

    int8_t w = _SFTE_CHAR_WIDTH(rune);
    if (w < 0) w = 1;

    if (w == 0) {
#if SFTE_FONT_WIDE_CHARS
        if (cursor->col > 0) {
            int32_t prev_idx = _sfte_grid_get_idx(vp, cursor->col - 1, cursor->row);
            sfte_cell *prev = &term->cells[prev_idx];

            if (prev->attr & _SFTE_ATTR_DUMMY && cursor->col > 1) {
                prev_idx = _sfte_grid_get_idx(vp, cursor->col - 2, cursor->row);
                prev = &term->cells[prev_idx];
            }

#if SFTE_FONT_MAX_COMBINING
            if (prev->combining_cnt < SFTE_FONT_MAX_COMBINING) {
                prev->combining_runes[prev->combining_cnt++] = rune;
                prev->dirty = 1;
            }
#endif  // SFTE_FONT_MAX_COMBINING
        }
#endif  // SFTE_FONT_WIDE_CHARS
        return;
    }

    // Evaluate line wrapping before drawing,
    // ensures characters placed in the final column enter a pending wrap state.
    _sfte_grid_check_wrap(term);

#if SFTE_FONT_WIDE_CHARS
    if (w == 2) {
        // A wide character cannot be split across lines,
        // if in last column, leave it blank and wrap early.
        if (cursor->col == vp->cols - 1) {
            int32_t idx = _sfte_grid_get_idx(vp, cursor->col, cursor->row);
            _sfte_rune_stamp_cell(term, idx, ' ', 0);
            term->cells[idx].fg = _SFTE_COLOR_FG_DEFAULT;
            term->cells[idx].bg = _SFTE_COLOR_BG_DEFAULT;
            term->cells[idx].attr = 0;

            cursor->col++;
            _sfte_grid_check_wrap(term);
        }

        // If terminal grid has only one column, we can't draw the double-width character.
        if (cursor->col == vp->cols - 1) return;

        // Draw the actual character.
        int32_t idx = _sfte_grid_get_idx(vp, cursor->col, cursor->row);
        _sfte_rune_stamp_cell(term, idx, rune, _SFTE_ATTR_WIDE);

        // Place a dummy right after it so that it has enough space to render.
        int32_t dummy_idx = _sfte_grid_get_idx(vp, cursor->col + 1, cursor->row);
        _sfte_rune_stamp_cell(term, dummy_idx, rune, _SFTE_ATTR_DUMMY);

        cursor->col += 2;
        return;
    }
#endif  // SFTE_FONT_WIDE_CHARS

    // Write a normal, one-width character.
    int32_t idx = _sfte_grid_get_idx(vp, cursor->col, cursor->row);
    _sfte_rune_stamp_cell(term, idx, rune, 0);
    cursor->col++;
}
