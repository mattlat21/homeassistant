#pragma once

#include "lvgl.h"

/** Global corner radius for ui_box_1 and derived components. */
#define UI_BOX_CORNER_RADIUS 25

#define UI_BOX_1_BG_COLOR lv_color_hex(0xFFFFFF)
/** White at ~40% opacity */
#define UI_BOX_1_BG_OPA ((lv_opa_t)(255 * 40 / 100))

#define UI_BOX_1_LABEL_COLOR lv_color_hex(0x333333)

/** Nudge icon-only labels (font metrics vs. optical center); applied with flex + scale in ui_button_1. */
#define UI_BUTTON_1_ICON_ONLY_TRANSLATE_Y 4

/** Single selector: inset of segment strip from outer rounded box. */
#define UI_SINGLE_SELECTOR_INNER_PAD_ALL 12
/** Gap between selector segments (flex pad_row / pad_column). */
#define UI_SINGLE_SELECTOR_INNER_GAP 10
/** Segment corners concentric with outer `UI_BOX_CORNER_RADIUS` (same arc center). */
#define UI_SINGLE_SELECTOR_SEGMENT_RADIUS (UI_BOX_CORNER_RADIUS - UI_SINGLE_SELECTOR_INNER_PAD_ALL)
/** Icon and name label color when segment is not selected. */
#define UI_SINGLE_SELECTOR_TEXT_COLOR_IDLE lv_color_white()
/** Icon and name label color when segment is selected (yellow–orange). */
#define UI_SINGLE_SELECTOR_TEXT_COLOR_SELECTED lv_color_hex(0xFFAA22)
/**
 * Icon scale in segment (LVGL: 256 = 100%).
 * Use 256 so paired icon+text segments skip transform on the label (more stable with flex + wrap on this target).
 */
#define UI_SINGLE_SELECTOR_ICON_SCALE 256
