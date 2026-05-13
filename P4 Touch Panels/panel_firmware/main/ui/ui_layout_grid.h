#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Square N×N layout grid helpers: uniform gap bands (count+1) around N cells of fixed pixel size.
 * Positions are **content coordinates** inside a parent that uses `lv_obj_set_style_pad_all(parent, gap, ...)`
 * so (0,0) is the top-left of the first cell.
 */

/** Default cells per row/column when using a 6×6 launcher-style grid. */
#define UI_LAYOUT_GRID_N_DEFAULT 6

/** Default cell edge length in pixels (one dimension). */
#define UI_LAYOUT_GRID_CELL_PX_DEFAULT 100

/** Linear cell index 0 .. grid_n * grid_n - 1, row-major (0 = top-left). */
typedef uint8_t ui_layout_grid_cell_t;

/**
 * Gap for one screen dimension: (screen_len - cell_px * cell_count) / (cell_count + 1), clamped ≥ 0.
 */
int32_t ui_layout_grid_gap_for_length(int32_t cell_px, int32_t cell_count, int32_t screen_len);

/**
 * Minimum of horizontal and vertical gaps from @ref ui_layout_grid_gap_for_length (square grid per axis).
 */
int32_t ui_layout_grid_gap_for_screen(int32_t cell_hor_px, int32_t cell_ver_px, int32_t cols, int32_t rows,
                                      int32_t hor_res, int32_t ver_res);

/** Stride from one cell origin to the next: cell_px + gap_px. */
int32_t ui_layout_grid_stride_px(int32_t cell_px, int32_t gap_px);

/**
 * Base band gap plus extra padding on the start and end edges so @a screen_len is fully used.
 * Place cells with stride (cell_px + base_gap); set parent pad_start = base_gap + pad_extra_start,
 * pad_end = base_gap + pad_extra_end (same axis).
 */
void ui_layout_grid_symmetric_outer_pads(int32_t cell_px, int32_t cell_count, int32_t screen_len,
                                         int32_t *out_base_gap, int32_t *out_pad_extra_start,
                                         int32_t *out_pad_extra_end);

/**
 * Square cells, uniform gap G between tiles and between outer edge and first/last tile (6 bands on each axis).
 * When hor_len != ver_len, slack is split symmetrically on left/right or top/bottom.
 * Chooses the largest cell edge such that G >= min_gap_px (when possible).
 */
bool ui_layout_grid_uniform_gap_square(int32_t hor_len, int32_t ver_len, uint32_t grid_n, int32_t min_gap_px,
                                       int32_t *out_cell_px, int32_t *out_gap, int32_t *out_pad_left,
                                       int32_t *out_pad_right, int32_t *out_pad_top, int32_t *out_pad_bottom);

/** Top-left of cell (row, col) with independent horizontal and vertical strides. */
bool ui_layout_grid_rc_to_pos_xy(int32_t gap_x, int32_t gap_y, int32_t cell_px, uint8_t row, uint8_t col,
                                 int32_t *out_x, int32_t *out_y);

/**
 * Top-left of cell at (row, col) in grid content space; grid is @a grid_n × @a grid_n.
 */
bool ui_layout_grid_rc_to_pos(int32_t gap_px, int32_t cell_px, uint8_t grid_n, uint8_t row, uint8_t col,
                              int32_t *out_x, int32_t *out_y);

/**
 * Top-left of linear @a cell (row-major) in the same coordinate system as @ref ui_layout_grid_rc_to_pos.
 */
bool ui_layout_grid_cell_to_pos(int32_t gap_px, int32_t cell_px, uint8_t grid_n, ui_layout_grid_cell_t cell,
                                int32_t *out_x, int32_t *out_y);
