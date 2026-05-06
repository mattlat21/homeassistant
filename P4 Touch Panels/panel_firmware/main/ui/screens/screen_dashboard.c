/**
 * @file screen_dashboard.c
 *
 * Dashboard screen: full template layout without status bar, red-orange gradient background.
 */

#include "ui/screens/screen_dashboard.h"
#include "ui/components/ui_gauge_1.h"
#include "ui/ui_dashboard_gradient.h"
#include "ui/ui_layout.h"

lv_obj_t *screen_dashboard_create(lv_display_t *disp)
{
    ui_screen_template_params_t params;
    ui_screen_template_params_init_defaults(&params);
    params.status_bar = false;

    ui_screen_template_result_t layout;
    if (!ui_screen_template_create(disp, &params, &layout)) {
        return NULL;
    }

    ui_dashboard_gradient_apply(layout.screen);

    // lv_obj_t *title = lv_label_create(layout.grid);
    // lv_label_set_text(title, "Dashboard");
    // lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    // lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    // lv_obj_set_grid_cell(title, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    // Speedometer
    lv_obj_t *spedometer = ui_gauge_1_create(layout.grid, 330, 20, 120, "68");
    if (spedometer != NULL) {
        ui_gauge_1_set_percent(spedometer, 60);
        /* Span full 6×6 template grid; CENTER aligns the fixed-size dial in the cell. */
        lv_obj_set_grid_cell(spedometer, LV_GRID_ALIGN_CENTER, 0, 6, LV_GRID_ALIGN_CENTER, 0, 6);
    }

    /* Fuel: overlay on content so it does not share the speedometer’s full-screen grid cell. */
    const int32_t gauge_edge_padding = 20;
    lv_obj_t *fuel_gauge = ui_gauge_1_create(layout.content, 230, 20, 48, "70%");
    if (fuel_gauge != NULL) {
        ui_gauge_1_set_percent(fuel_gauge, 70);
        lv_obj_align_to(fuel_gauge, layout.content, LV_ALIGN_BOTTOM_LEFT, gauge_edge_padding, -gauge_edge_padding);
        lv_obj_move_foreground(fuel_gauge);
    }

    // RPM
    lv_obj_t *rpm_gauge = ui_gauge_1_create(layout.content, 230, 20, 48, "4000");
    if (rpm_gauge != NULL) {
        ui_gauge_1_set_percent(rpm_gauge, 80);
        lv_obj_align_to(rpm_gauge, layout.content, LV_ALIGN_TOP_RIGHT, -gauge_edge_padding, gauge_edge_padding);
        lv_obj_move_foreground(rpm_gauge);
    }

    // Temperature
    lv_obj_t *temperature_gauge = ui_gauge_1_create(layout.content, 230, 20, 48, "50°C");
    if (temperature_gauge != NULL) {
        ui_gauge_1_set_percent(temperature_gauge, 30);
        lv_obj_align_to(temperature_gauge, layout.content, LV_ALIGN_BOTTOM_RIGHT, -gauge_edge_padding, -gauge_edge_padding);
        lv_obj_move_foreground(temperature_gauge);
    }

    return layout.screen;
}
