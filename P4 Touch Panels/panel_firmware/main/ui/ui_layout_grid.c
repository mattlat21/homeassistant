#include "ui/ui_layout_grid.h"

#include <stdint.h>

#include "lvgl.h"

int32_t ui_layout_grid_gap_for_length(int32_t cell_px, int32_t cell_count, int32_t screen_len)
{
    if (cell_count <= 0) {
        return 0;
    }
    const int32_t span = cell_px * cell_count;
    int32_t gap = (screen_len - span) / (cell_count + 1);
    if (gap < 0) {
        gap = 0;
    }
    return gap;
}

int32_t ui_layout_grid_gap_for_screen(int32_t cell_hor_px, int32_t cell_ver_px, int32_t cols, int32_t rows,
                                      int32_t hor_res, int32_t ver_res)
{
    const int32_t gh = ui_layout_grid_gap_for_length(cell_hor_px, cols, hor_res);
    const int32_t gv = ui_layout_grid_gap_for_length(cell_ver_px, rows, ver_res);
    return LV_MIN(gh, gv);
}

int32_t ui_layout_grid_stride_px(int32_t cell_px, int32_t gap_px)
{
    return cell_px + gap_px;
}

void ui_layout_grid_symmetric_outer_pads(int32_t cell_px, int32_t cell_count, int32_t screen_len,
                                         int32_t *out_base_gap, int32_t *out_pad_extra_start,
                                         int32_t *out_pad_extra_end)
{
    if (out_base_gap == NULL || out_pad_extra_start == NULL || out_pad_extra_end == NULL) {
        return;
    }
    if (cell_count <= 0) {
        *out_base_gap = 0;
        *out_pad_extra_start = 0;
        *out_pad_extra_end = 0;
        return;
    }
    const int32_t span = cell_px * cell_count;
    const int32_t bands = cell_count + 1;
    const int32_t slack = screen_len - span;
    if (slack <= 0) {
        *out_base_gap = 0;
        *out_pad_extra_start = 0;
        *out_pad_extra_end = 0;
        return;
    }
    *out_base_gap = slack / bands;
    const int32_t rem = slack % bands;
    *out_pad_extra_start = rem / 2;
    *out_pad_extra_end = rem - (rem / 2);
}

bool ui_layout_grid_uniform_gap_square(int32_t hor_len, int32_t ver_len, uint32_t grid_n, int32_t min_gap_px,
                                       int32_t *out_cell_px, int32_t *out_gap, int32_t *out_pad_left,
                                       int32_t *out_pad_right, int32_t *out_pad_top, int32_t *out_pad_bottom)
{
    if (out_cell_px == NULL || out_gap == NULL || out_pad_left == NULL || out_pad_right == NULL ||
        out_pad_top == NULL || out_pad_bottom == NULL || grid_n == 0 || hor_len <= 0 || ver_len <= 0) {
        return false;
    }
    if (min_gap_px < 0) {
        min_gap_px = 0;
    }
    const uint32_t bands = grid_n + 1;
    int32_t best_c = 0;
    int32_t best_g = 0;
    /* Search largest square tile that fits both axes with a shared gap G = min(Gh,Gv). */
    for (int32_t c = LV_MIN(hor_len, ver_len) / (int32_t)grid_n; c >= 40; c--) {
        const int32_t gh = (hor_len - (int32_t)grid_n * c) / (int32_t)bands;
        const int32_t gv = (ver_len - (int32_t)grid_n * c) / (int32_t)bands;
        const int32_t g = LV_MIN(gh, gv);
        if (g < min_gap_px) {
            continue;
        }
        if (c > best_c || (c == best_c && g > best_g)) {
            best_c = c;
            best_g = g;
        }
    }
    if (best_c <= 0 || best_g < min_gap_px) {
        return false;
    }
    const int32_t used = (int32_t)bands * best_g + (int32_t)grid_n * best_c;
    const int32_t sx = hor_len - used;
    const int32_t sy = ver_len - used;

    *out_cell_px = best_c;
    *out_gap = best_g;
    *out_pad_left = best_g + sx / 2;
    *out_pad_right = best_g + (sx - sx / 2);
    *out_pad_top = best_g + sy / 2;
    *out_pad_bottom = best_g + (sy - sy / 2);
    return true;
}

bool ui_layout_grid_rc_to_pos_xy(int32_t gap_x, int32_t gap_y, int32_t cell_px, uint8_t row, uint8_t col,
                                 int32_t *out_x, int32_t *out_y)
{
    if (out_x == NULL || out_y == NULL) {
        return false;
    }
    const int32_t sx = ui_layout_grid_stride_px(cell_px, gap_x);
    const int32_t sy = ui_layout_grid_stride_px(cell_px, gap_y);
    *out_x = (int32_t)col * sx;
    *out_y = (int32_t)row * sy;
    return true;
}

bool ui_layout_grid_rc_to_pos(int32_t gap_px, int32_t cell_px, uint8_t grid_n, uint8_t row, uint8_t col,
                                int32_t *out_x, int32_t *out_y)
{
    if (out_x == NULL || out_y == NULL) {
        return false;
    }
    if (grid_n == 0 || row >= grid_n || col >= grid_n) {
        return false;
    }
    const int32_t s = ui_layout_grid_stride_px(cell_px, gap_px);
    *out_x = (int32_t)col * s;
    *out_y = (int32_t)row * s;
    return true;
}

bool ui_layout_grid_cell_to_pos(int32_t gap_px, int32_t cell_px, uint8_t grid_n, ui_layout_grid_cell_t cell,
                                int32_t *out_x, int32_t *out_y)
{
    if (grid_n == 0) {
        return false;
    }
    const unsigned max_i = (unsigned)grid_n * (unsigned)grid_n;
    if ((unsigned)cell >= max_i) {
        return false;
    }
    const uint8_t row = (uint8_t)(cell / (ui_layout_grid_cell_t)grid_n);
    const uint8_t col = (uint8_t)(cell % (ui_layout_grid_cell_t)grid_n);
    return ui_layout_grid_rc_to_pos(gap_px, cell_px, grid_n, row, col, out_x, out_y);
}
