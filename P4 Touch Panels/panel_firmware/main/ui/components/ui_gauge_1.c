/**
 * @file ui_gauge_1.c
 *
 * Reusable “car dial” style gauge built from LVGL primitives:
 *
 * - A circular root object (semi-transparent white) holds everything.
 * - An @c lv_arc draws the track (background angles) and the filled indicator.
 * - A centered @c lv_label shows caller-controlled text (e.g. “72%”, “— kWh”).
 *
 * Child widgets are not stored on the root’s default user_data slot without conflict:
 * we allocate @ref ui_gauge_1_ud_t with @c lv_malloc, point the root’s user_data at
 * it, and free it on @c LV_EVENT_DELETE. A magic field confirms the pointer is ours
 * before dereferencing in the public setters.
 */

#include "ui/components/ui_gauge_1.h"

/** ASCII-ish tag “gA1\0” so @ref gauge_1_get_ud rejects foreign user_data. */
#define UI_GAUGE_1_MAGIC 0x67314131u

/**
 * Private state hung off the root gauge object’s user_data.
 *
 * LVGL destroys arc/label when the root is deleted; we only own this struct.
 */
typedef struct {
    uint32_t magic;
    lv_obj_t *arc;
    lv_obj_t *label;
} ui_gauge_1_ud_t;

/**
 * Maximum LVGL transform scale for the center label (256 = 1×). 2048 ≈ 8× the largest
 * built-in Montserrat — enough for very large dial text without absurd raster cost.
 */
#ifndef GAUGE_1_LABEL_SCALE_MAX
#define GAUGE_1_LABEL_SCALE_MAX 2048
#endif

typedef struct {
    const lv_font_t *font;
    int32_t nominal_px;
} gauge_builtin_font_t;

/** Largest Montserrat shipped in this build (LVGL has no Montserrat > 48 in Kconfig). */
static gauge_builtin_font_t gauge_largest_montserrat(void)
{
#if LV_FONT_MONTSERRAT_48
    return (gauge_builtin_font_t){ &lv_font_montserrat_48, 48 };
#elif LV_FONT_MONTSERRAT_46
    return (gauge_builtin_font_t){ &lv_font_montserrat_46, 46 };
#elif LV_FONT_MONTSERRAT_44
    return (gauge_builtin_font_t){ &lv_font_montserrat_44, 44 };
#elif LV_FONT_MONTSERRAT_42
    return (gauge_builtin_font_t){ &lv_font_montserrat_42, 42 };
#elif LV_FONT_MONTSERRAT_40
    return (gauge_builtin_font_t){ &lv_font_montserrat_40, 40 };
#elif LV_FONT_MONTSERRAT_38
    return (gauge_builtin_font_t){ &lv_font_montserrat_38, 38 };
#elif LV_FONT_MONTSERRAT_36
    return (gauge_builtin_font_t){ &lv_font_montserrat_36, 36 };
#elif LV_FONT_MONTSERRAT_34
    return (gauge_builtin_font_t){ &lv_font_montserrat_34, 34 };
#elif LV_FONT_MONTSERRAT_32
    return (gauge_builtin_font_t){ &lv_font_montserrat_32, 32 };
#elif LV_FONT_MONTSERRAT_30
    return (gauge_builtin_font_t){ &lv_font_montserrat_30, 30 };
#elif LV_FONT_MONTSERRAT_28
    return (gauge_builtin_font_t){ &lv_font_montserrat_28, 28 };
#elif LV_FONT_MONTSERRAT_26
    return (gauge_builtin_font_t){ &lv_font_montserrat_26, 26 };
#elif LV_FONT_MONTSERRAT_24
    return (gauge_builtin_font_t){ &lv_font_montserrat_24, 24 };
#elif LV_FONT_MONTSERRAT_22
    return (gauge_builtin_font_t){ &lv_font_montserrat_22, 22 };
#elif LV_FONT_MONTSERRAT_20
    return (gauge_builtin_font_t){ &lv_font_montserrat_20, 20 };
#elif LV_FONT_MONTSERRAT_18
    return (gauge_builtin_font_t){ &lv_font_montserrat_18, 18 };
#elif LV_FONT_MONTSERRAT_16
    return (gauge_builtin_font_t){ &lv_font_montserrat_16, 16 };
#elif LV_FONT_MONTSERRAT_14
    return (gauge_builtin_font_t){ &lv_font_montserrat_14, 14 };
#elif LV_FONT_MONTSERRAT_12
    return (gauge_builtin_font_t){ &lv_font_montserrat_12, 12 };
#elif LV_FONT_MONTSERRAT_10
    return (gauge_builtin_font_t){ &lv_font_montserrat_10, 10 };
#elif LV_FONT_MONTSERRAT_8
    return (gauge_builtin_font_t){ &lv_font_montserrat_8, 8 };
#else
    return (gauge_builtin_font_t){ NULL, 0 };
#endif
}

/**
 * Maps a requested nominal text height in px to an enabled @c lv_font_t *.
 *
 * Used only when @a text_size_px does not exceed the largest built-in Montserrat in
 * this firmware; larger requests are handled by @ref gauge_1_apply_label_typography
 * (same bitmap + @c LV_STYLE_TRANSFORM_SCALE).
 */
static const lv_font_t *pick_font_for_size(int32_t text_size_px)
{
    if (text_size_px < 1) {
        text_size_px = 14;
    }

#if LV_FONT_MONTSERRAT_48
    if (text_size_px >= 48) {
        return &lv_font_montserrat_48;
    }
#endif
#if LV_FONT_MONTSERRAT_40
    if (text_size_px >= 40) {
        return &lv_font_montserrat_40;
    }
#endif
#if LV_FONT_MONTSERRAT_36
    if (text_size_px >= 36) {
        return &lv_font_montserrat_36;
    }
#endif
#if LV_FONT_MONTSERRAT_32
    if (text_size_px >= 32) {
        return &lv_font_montserrat_32;
    }
#endif
#if LV_FONT_MONTSERRAT_28
    if (text_size_px >= 28) {
        return &lv_font_montserrat_28;
    }
#endif
#if LV_FONT_MONTSERRAT_24
    if (text_size_px >= 24) {
        return &lv_font_montserrat_24;
    }
#endif
#if LV_FONT_MONTSERRAT_20
    if (text_size_px >= 20) {
        return &lv_font_montserrat_20;
    }
#endif
#if LV_FONT_MONTSERRAT_18
    if (text_size_px >= 18) {
        return &lv_font_montserrat_18;
    }
#endif
#if LV_FONT_MONTSERRAT_16
    if (text_size_px >= 16) {
        return &lv_font_montserrat_16;
    }
#endif
#if LV_FONT_MONTSERRAT_14
    if (text_size_px >= 14) {
        return &lv_font_montserrat_14;
    }
#endif
#if LV_FONT_MONTSERRAT_12
    if (text_size_px >= 12) {
        return &lv_font_montserrat_12;
    }
#endif
#if LV_FONT_MONTSERRAT_10
    return &lv_font_montserrat_10;
#elif LV_FONT_MONTSERRAT_14
    return &lv_font_montserrat_14;
#else
    return LV_FONT_DEFAULT;
#endif
}

static void gauge_1_label_center_pivot(lv_obj_t *label)
{
    lv_coord_t w = lv_obj_get_width(label);
    lv_coord_t h = lv_obj_get_height(label);
    if (w > 0 && h > 0) {
        lv_obj_set_style_transform_pivot_x(label, w / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(label, h / 2, LV_PART_MAIN);
    }
}

/** Keeps transform pivot at the label center when size changes (needed for scale > 1×). */
static void gauge_1_label_on_size_changed(lv_event_t *e)
{
    lv_obj_t *label = lv_event_get_target(e);
    gauge_1_label_center_pivot(label);
}

/**
 * Sets label font and optional software zoom. Montserrat in LVGL tops out at 48 px
 * nominal; for larger @a text_size_px we use the largest enabled bitmap and
 * @c lv_obj_set_style_transform_scale (256 = 100%, 512 ≈ 200%).
 */
static void gauge_1_apply_label_typography(lv_obj_t *label, int32_t text_size_px)
{
    if (label == NULL) {
        return;
    }
    if (text_size_px < 1) {
        text_size_px = 14;
    }

    gauge_builtin_font_t big = gauge_largest_montserrat();
    int32_t scale = LV_SCALE_NONE;
    const lv_font_t *font;

    if (big.font != NULL && text_size_px > big.nominal_px) {
        font = big.font;
        int64_t s = ((int64_t)text_size_px * (int64_t)LV_SCALE_NONE) / (int64_t)big.nominal_px;
        if (s < (int64_t)LV_SCALE_NONE) {
            s = LV_SCALE_NONE;
        }
        if (s > (int64_t)GAUGE_1_LABEL_SCALE_MAX) {
            s = GAUGE_1_LABEL_SCALE_MAX;
        }
        scale = (int32_t)s;
    } else {
        font = pick_font_for_size(text_size_px);
    }

    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_transform_scale(label, scale, LV_PART_MAIN);
    gauge_1_label_center_pivot(label);
}

/** Frees @ref ui_gauge_1_ud_t; registered on the root gauge only. */
static void gauge_1_on_delete(lv_event_t *e)
{
    ui_gauge_1_ud_t *ud = lv_event_get_user_data(e);
    if (ud != NULL) {
        lv_free(ud);
    }
}

/**
 * Returns our userdata if @a gauge is non-NULL and the magic matches.
 * Any other object (or wrong user_data) yields NULL so setters become no-ops.
 */
static ui_gauge_1_ud_t *gauge_1_get_ud(lv_obj_t *gauge)
{
    if (gauge == NULL) {
        return NULL;
    }
    ui_gauge_1_ud_t *ud = (ui_gauge_1_ud_t *)lv_obj_get_user_data(gauge);
    if (ud == NULL || ud->magic != UI_GAUGE_1_MAGIC) {
        return NULL;
    }
    return ud;
}

lv_obj_t *ui_gauge_1_create(lv_obj_t *parent, int32_t size_px, int32_t arc_thickness_px, int32_t text_size_px,
                            const char *value_text)
{
    if (parent == NULL || size_px <= 0) {
        return NULL;
    }

    /* Allocate userdata first so we can bail before creating the root if OOM. */
    ui_gauge_1_ud_t *ud = (ui_gauge_1_ud_t *)lv_malloc(sizeof(ui_gauge_1_ud_t));
    if (ud == NULL) {
        return NULL;
    }
    ud->magic = 0; /* Set to UI_GAUGE_1_MAGIC only after children succeed; eases partial-fail cleanup. */
    ud->arc = NULL;
    ud->label = NULL;

    /* --- Root: circular “glass” dial --------------------------------------- */
    lv_obj_t *gauge = lv_obj_create(parent);
    if (gauge == NULL) {
        lv_free(ud);
        return NULL;
    }

    lv_obj_remove_style_all(gauge);
    lv_obj_set_size(gauge, size_px, size_px);
    lv_obj_set_style_radius(gauge, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(gauge, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(gauge, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_border_width(gauge, 0, LV_PART_MAIN);
    /*
     * Default clipping would mask arc caps that extend slightly outside the bbox.
     * Allowing overflow keeps the stroke visually centered on the circle edge.
     */
    lv_obj_add_flag(gauge, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    /* Clamp arc stroke so it cannot disappear or swallow the whole dial. */
    int32_t th = arc_thickness_px;
    if (th < 2) {
        th = 2;
    }
    const int32_t th_max = size_px / 3;
    if (th > th_max) {
        th = LV_MAX(2, th_max);
    }

    /* --- Arc: 270° gauge, 0–100 logical range ------------------------------ */
    lv_obj_t *arc = lv_arc_create(gauge);
    lv_obj_set_size(arc, lv_pct(90), lv_pct(90));
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);

    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);

    /*
     * Geometry (LVGL arc): rotation shifts where 0° starts; bg_angles define the
     * inactive “track” sweep. Together: 135° rotation + 0..270 background yields a
     * 270° arc opening toward the bottom (classic speedometer-style), not a full ring.
     */
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);

    /* Main arc container part: transparent; only PART_MAIN/PART_INDICATOR arcs draw. */
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, th, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, th, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    /* Track: soft white; indicator: saturated accent for readability on the glass. */
    lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_20, LV_PART_MAIN);
    // lv_obj_set_style_arc_color(arc, lv_color_hex(0x007AFF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);

    /* This is a display-only widget, not a touch control. */
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    /* --- Center label -------------------------------------------------------- */
    lv_obj_t *label = lv_label_create(gauge);
    lv_label_set_text(label, value_text != NULL ? value_text : "");
    gauge_1_apply_label_typography(label, text_size_px);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    /* Ellipsis if the string is too wide for the inner circle. */
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(label, lv_pct(70));
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    /* Ensure text paints above the arc in stacking order. */
    lv_obj_move_foreground(label);
    lv_obj_add_event_cb(label, gauge_1_label_on_size_changed, LV_EVENT_SIZE_CHANGED, NULL);

    ud->magic = UI_GAUGE_1_MAGIC;
    ud->arc = arc;
    ud->label = label;
    lv_obj_set_user_data(gauge, ud);
    lv_obj_add_event_cb(gauge, gauge_1_on_delete, LV_EVENT_DELETE, ud);

    return gauge;
}

void ui_gauge_1_set_value_text(lv_obj_t *gauge, const char *value_text)
{
    ui_gauge_1_ud_t *ud = gauge_1_get_ud(gauge);
    if (ud == NULL || ud->label == NULL) {
        return;
    }
    lv_label_set_text(ud->label, value_text != NULL ? value_text : "");
}

void ui_gauge_1_set_text_size(lv_obj_t *gauge, int32_t text_size_px)
{
    ui_gauge_1_ud_t *ud = gauge_1_get_ud(gauge);
    if (ud == NULL || ud->label == NULL) {
        return;
    }
    gauge_1_apply_label_typography(ud->label, text_size_px);
}

void ui_gauge_1_set_percent(lv_obj_t *gauge, int32_t percent_0_100)
{
    ui_gauge_1_ud_t *ud = gauge_1_get_ud(gauge);
    if (ud == NULL || ud->arc == NULL) {
        return;
    }
    int32_t v = percent_0_100;
    if (v < 0) {
        v = 0;
    }
    if (v > 100) {
        v = 100;
    }
    lv_arc_set_value(ud->arc, v);
}
