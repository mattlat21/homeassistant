#include "ui/ui_shell.h"
#include "sdkconfig.h"
#include "ui/nav.h"
#include "ui/ui_watchdog.h"
#include "ui/ui_idle_timeout.h"
#include "ui/screens/screen_loading.h"
#include "ui/screens/screen_home.h"
#include "ui/screens/screen_ollie.h"
#include "ui/screens/screen_dashboard.h"
#include "ui/screens/screen_front_gate.h"
#include "ui/screens/screen_pipboy.h"
#include "ui/screens/screen_settings.h"
#include "ui/screens/screen_about.h"
#include "ui/screens/screen_study.h"
#include "ui/screens/screen_front_door.h"
#include "ui/components/ui_gate_action.h"
#include "ui/screens/screen_placeholder.h"
#if CONFIG_SCREEN_TEST_OTA_ENABLE
#include "ui/screens/screen_ota_progress.h"
#endif

void ui_shell_init(lv_display_t *disp)
{
    nav_init(disp);
    ui_watchdog_init();
    ui_gate_action_init();

    lv_obj_t *scr_loading = screen_loading_create(disp);
    lv_obj_t *scr_home = screen_home_create(disp);
    lv_obj_t *scr_ollie = screen_ollie_create(disp);
    lv_obj_t *scr_dashboard = screen_dashboard_create(disp);
    lv_obj_t *scr_front_gate = screen_front_gate_create(disp);
    lv_obj_t *scr_pipboy = screen_pipboy_create(disp);
    lv_obj_t *scr_settings = screen_settings_create(disp);
    lv_obj_t *scr_about = screen_about_create(disp);
    lv_obj_t *scr_study = screen_study_create(disp);
    lv_obj_t *scr_front_door = screen_front_door_create(disp);
    lv_obj_t *scr_kitchen = screen_placeholder_create(disp, "Kitchen", true);
    lv_obj_t *scr_studio = screen_placeholder_create(disp, "Studio", true);

    nav_register_screen(APP_HOME, scr_home);
    nav_register_screen(APP_OLLIE_ROOM, scr_ollie);
    nav_register_screen(APP_DASHBOARD, scr_dashboard);
    nav_register_screen(APP_FRONT_GATE, scr_front_gate);
    nav_register_screen(APP_PIPBOY, scr_pipboy);
    nav_register_screen(APP_SETTINGS, scr_settings);
    nav_register_screen(APP_STUDY, scr_study);
    nav_register_screen(APP_ABOUT, scr_about);
    nav_register_screen(APP_FRONT_DOOR, scr_front_door);
    nav_register_screen(APP_KITCHEN, scr_kitchen);
    nav_register_screen(APP_STUDIO, scr_studio);

    nav_install_gesture_on_screen(scr_home);
    nav_install_gesture_on_screen(scr_ollie);
    nav_install_gesture_on_screen(scr_dashboard);
    nav_install_gesture_on_screen(scr_front_gate);
    nav_install_gesture_on_screen(scr_pipboy);
    nav_install_gesture_on_screen(scr_settings);
    nav_install_gesture_on_screen(scr_about);
    nav_install_gesture_on_screen(scr_study);
    nav_install_gesture_on_screen(scr_front_door);
    nav_install_gesture_on_screen(scr_kitchen);
    nav_install_gesture_on_screen(scr_studio);

    lv_screen_load(scr_loading);
    screen_loading_begin(scr_loading);

#if CONFIG_SCREEN_TEST_OTA_ENABLE
    (void)screen_ota_progress_create(disp);
#endif

    ui_idle_timeout_init(disp);
}
