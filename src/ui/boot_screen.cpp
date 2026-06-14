#include "boot_screen.h"
#include "../config.h"
#include <stdio.h>

#define CLR_MAIN  lv_color_hex(0x00E5FF)
#define CLR_DIM   lv_color_hex(0x007280)
#define CLR_DARK  lv_color_hex(0x003840)
#define CLR_BLACK lv_color_hex(0x000000)

static lv_obj_t *boot_scr     = nullptr;
static lv_obj_t *bar_progress = nullptr;
static lv_obj_t *lbl_progress = nullptr;
static lv_obj_t *lbl_msg      = nullptr;
static lv_obj_t *lbl_scanline = nullptr;

static lv_anim_t scanline_anim;

static void scanline_anim_cb(void *obj, int32_t v) {
    lv_obj_set_y((lv_obj_t *)obj, v);
}

static void start_scanline_anim(void) {
    lv_anim_init(&scanline_anim);
    lv_anim_set_var(&scanline_anim, lbl_scanline);
    lv_anim_set_exec_cb(&scanline_anim, scanline_anim_cb);
    lv_anim_set_values(&scanline_anim, 0, SCREEN_HEIGHT);
    lv_anim_set_duration(&scanline_anim, 1800);
    lv_anim_set_repeat_count(&scanline_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&scanline_anim, lv_anim_path_linear);
    lv_anim_start(&scanline_anim);
}

void boot_screen_show(lv_obj_t *parent) {

    boot_scr = lv_obj_create(parent);
    lv_obj_remove_style_all(boot_scr);
    lv_obj_set_size(boot_scr, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(boot_scr, CLR_BLACK, 0);
    lv_obj_set_style_bg_opa(boot_scr, LV_OPA_COVER, 0);
    lv_obj_center(boot_scr);
    lv_obj_clear_flag(boot_scr, LV_OBJ_FLAG_SCROLLABLE);

    
    lbl_scanline = lv_obj_create(boot_scr);
    lv_obj_remove_style_all(lbl_scanline);
    lv_obj_set_size(lbl_scanline, SCREEN_WIDTH, 2);
    lv_obj_set_style_bg_color(lbl_scanline, CLR_MAIN, 0);
    lv_obj_set_style_bg_opa(lbl_scanline, LV_OPA_30, 0);
    lv_obj_set_pos(lbl_scanline, 0, 0);
    start_scanline_anim();

    
    lv_obj_t *top_line = lv_obj_create(boot_scr);
    lv_obj_remove_style_all(top_line);
    lv_obj_set_size(top_line, SCREEN_WIDTH - 40, 1);
    lv_obj_set_style_bg_color(top_line, CLR_DIM, 0);
    lv_obj_set_style_bg_opa(top_line, LV_OPA_COVER, 0);
    lv_obj_align(top_line, LV_ALIGN_TOP_MID, 0, 52);

    
    lv_obj_t *lbl_title = lv_label_create(boot_scr);
    lv_label_set_text(lbl_title, "SCR TERMINAL");
    lv_obj_set_style_text_color(lbl_title, CLR_MAIN, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_letter_space(lbl_title, 6, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 18);

    
    lv_obj_t *lbl_ver = lv_label_create(boot_scr);
    lv_label_set_text(lbl_ver, "WDGWatch v2.1  |  T-Watch Ultra");
    lv_obj_set_style_text_color(lbl_ver, CLR_DIM, 0);
    lv_obj_set_style_text_font(lbl_ver, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_ver, LV_ALIGN_TOP_MID, 0, 57);

    
    lv_obj_t *lbl_logo = lv_label_create(boot_scr);
    lv_label_set_text(lbl_logo,
        "  ____  ____ ____  \n"
        " / ___|/ ___|  _ \\ \n"
        " \\___ \\\\ |   | |_) |\n"
        "  ___) || |___| _ < \n"
        " |____/ \\____|_| \\_\\");
    lv_obj_set_style_text_color(lbl_logo, CLR_DARK, 0);
    lv_obj_set_style_text_font(lbl_logo, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_logo, LV_ALIGN_CENTER, 0, -30);

    
    lv_obj_t *bot_line = lv_obj_create(boot_scr);
    lv_obj_remove_style_all(bot_line);
    lv_obj_set_size(bot_line, SCREEN_WIDTH - 40, 1);
    lv_obj_set_style_bg_color(bot_line, CLR_DIM, 0);
    lv_obj_set_style_bg_opa(bot_line, LV_OPA_COVER, 0);
    lv_obj_align(bot_line, LV_ALIGN_BOTTOM_MID, 0, -92);

    
    bar_progress = lv_bar_create(boot_scr);
    lv_obj_set_size(bar_progress, SCREEN_WIDTH - 80, 6);
    lv_bar_set_range(bar_progress, 0, 100);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_progress, CLR_DARK, 0);
    lv_obj_set_style_bg_opa(bar_progress, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar_progress, CLR_MAIN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_progress, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_progress, 0, 0);
    lv_obj_set_style_radius(bar_progress, 0, LV_PART_INDICATOR);
    lv_obj_align(bar_progress, LV_ALIGN_BOTTOM_MID, 0, -57);

    
    lbl_progress = lv_label_create(boot_scr);
    lv_label_set_text(lbl_progress, "  0%");
    lv_obj_set_style_text_color(lbl_progress, CLR_DIM, 0);
    lv_obj_set_style_text_font(lbl_progress, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_progress, LV_ALIGN_BOTTOM_MID, 0, -37);

    
    lbl_msg = lv_label_create(boot_scr);
    lv_label_set_text(lbl_msg, "> Initializing...");
    lv_obj_set_style_text_color(lbl_msg, CLR_DIM, 0);
    lv_obj_set_style_text_font(lbl_msg, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_msg, LV_ALIGN_BOTTOM_MID, 0, -72);
}

void boot_screen_update_progress(const char *msg, uint8_t pct) {
    if (!boot_scr) return;

    if (bar_progress)
        lv_bar_set_value(bar_progress, pct, LV_ANIM_ON);

    if (lbl_progress) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%3d%%", pct);
        lv_label_set_text(lbl_progress, buf);
    }

    if (lbl_msg && msg) {
        char buf[64];
        snprintf(buf, sizeof(buf), "> %s", msg);
        lv_label_set_text(lbl_msg, buf);
    }

    lv_timer_handler();
}

void boot_screen_hide(void) {
    if (!boot_scr) return;
    lv_anim_delete(lbl_scanline, scanline_anim_cb);
    lv_obj_delete(boot_scr);
    boot_scr     = nullptr;
    bar_progress = nullptr;
    lbl_progress = nullptr;
    lbl_msg      = nullptr;
    lbl_scanline = nullptr;
}
