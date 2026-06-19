#include "lora_app.h"
#include <LilyGoLib.h>
#include <cstdio>
#include "../config.h"
#include "../hal/haptic.h"
#include "../hal/lora_service.h"

static lv_obj_t *scr = nullptr;
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_msgs = nullptr;
static char msg_buf[1200] = "";

static lv_obj_t *lora_kbd_container = nullptr;
static lv_obj_t *ta_message = nullptr;
static lv_obj_t *kb_message = nullptr;
static lv_obj_t *btn_send = nullptr;
static lv_obj_t *btn_m = nullptr;
static lv_obj_t *btn_t = nullptr;
static lv_obj_t *btn_o = nullptr;

#define G  lv_color_hex(0x00E5FF)
#define D  lv_color_hex(0x007280)
#define BG lv_color_hex(0x000000)

static void toggle_meshcore_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (lora_svc_is_running() && lora_svc_get_mode() == MODE_MESHCORE) lora_svc_stop();
    else lora_svc_start(MODE_MESHCORE);
}

static void toggle_meshtastic_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (lora_svc_is_running() && lora_svc_get_mode() == MODE_MESHTASTIC) lora_svc_stop();
    else lora_svc_start(MODE_MESHTASTIC);
}

static LoraMode last_session_mode = (LoraMode)-1;

static void check_session_divider(void) {
    if (!lbl_msgs) return;
    LoraMode current_mode = lora_svc_get_mode();
    if (current_mode != last_session_mode) {
        last_session_mode = current_mode;
        
        int len = strlen(msg_buf);
        if (len > 900) {
            char *nl = strchr(msg_buf + 200, '\n');
            if (nl) { memmove(msg_buf, nl+1, strlen(nl)); len = strlen(msg_buf); }
        }
        
        const char *session_name = "UNKNOWN";
        if (current_mode == MODE_MESHCORE) session_name = "MESHCORE";
        else if (current_mode == MODE_MESHTASTIC) session_name = "MESHTASTIC";
        else if (current_mode == MODE_POCSAG) session_name = "POCSAG PAGER";
        else if (current_mode == MODE_BRUCE) session_name = "BRUCE CHAT";
        
        snprintf(msg_buf + len, sizeof(msg_buf) - len, "\n#ffb300 [%s SESSION]#\n", session_name);
        lv_label_set_text(lbl_msgs, msg_buf);
    }
}

static lv_obj_t *popup_win = nullptr;
static lv_obj_t *mode_select_btn[2] = {nullptr};
static lv_obj_t *opt_container = nullptr;
static LoraMode popup_selected_mode = MODE_POCSAG;
static float popup_selected_freq = 439.9875f;
static uint32_t popup_selected_ric = 1234567;

static float bruce_freqs[] = {868.000f, 433.920f, 915.000f};
static int bruce_freq_idx = 0;

static lv_obj_t *ric_ta = nullptr;
static lv_obj_t *ric_kb = nullptr;

static void refresh_popup_opts(void);

static void mode_btn_cb(lv_event_t *e) {
    int mode_idx = (int)(intptr_t)lv_event_get_user_data(e);
    haptic_click();
    if (mode_idx == 0) {
        popup_selected_mode = MODE_POCSAG;
        popup_selected_freq = 439.9875f;
    } else if (mode_idx == 1) {
        popup_selected_mode = MODE_BRUCE;
        popup_selected_freq = bruce_freqs[bruce_freq_idx];
    }
    
    for (int i = 0; i < 2; i++) {
        if (i == mode_idx) {
            lv_obj_set_style_bg_color(mode_select_btn[i], D, 0);
            lv_obj_set_style_bg_opa(mode_select_btn[i], LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_color(mode_select_btn[i], BG, 0);
            lv_obj_set_style_bg_opa(mode_select_btn[i], LV_OPA_TRANSP, 0);
        }
    }
    
    refresh_popup_opts();
}

static void freq_prev_cb(lv_event_t *e) {
    haptic_click();
    if (popup_selected_mode == MODE_BRUCE) {
        bruce_freq_idx = (bruce_freq_idx - 1 + 3) % 3;
        popup_selected_freq = bruce_freqs[bruce_freq_idx];
    }
    refresh_popup_opts();
}

static void freq_next_cb(lv_event_t *e) {
    haptic_click();
    if (popup_selected_mode == MODE_BRUCE) {
        bruce_freq_idx = (bruce_freq_idx + 1) % 3;
        popup_selected_freq = bruce_freqs[bruce_freq_idx];
    }
    refresh_popup_opts();
}

static void style_keyboard_pipboy(lv_obj_t *kb, lv_obj_t *ta);

static void ric_kb_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        haptic_click();
        if (code == LV_EVENT_READY) {
            popup_selected_ric = strtoul(lv_textarea_get_text(ric_ta), nullptr, 10);
        }
        lv_obj_delete(ric_kb); ric_kb = nullptr;
        lv_obj_delete(ric_ta); ric_ta = nullptr;
        refresh_popup_opts();
    }
}

static void ric_click_cb(lv_event_t *e) {
    (void)e; haptic_click();
    ric_ta = lv_textarea_create(popup_win);
    lv_textarea_set_one_line(ric_ta, true);
    lv_textarea_set_accepted_chars(ric_ta, "0123456789");
    lv_obj_set_size(ric_ta, 260, 45);
    lv_obj_align(ric_ta, LV_ALIGN_TOP_MID, 0, 70);
    char buf[16]; snprintf(buf, sizeof(buf), "%lu", (unsigned long)popup_selected_ric);
    lv_textarea_set_text(ric_ta, buf);
    
    ric_kb = lv_keyboard_create(popup_win);
    lv_keyboard_set_mode(ric_kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(ric_kb, ric_ta);
    lv_obj_set_size(ric_kb, 340, 180);
    lv_obj_align(ric_kb, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(ric_kb, ric_kb_event_cb, LV_EVENT_ALL, nullptr);
    
    style_keyboard_pipboy(ric_kb, ric_ta);
}

static void refresh_popup_opts(void) {
    if (!opt_container) return;
    lv_obj_clean(opt_container);
    
    if (popup_selected_mode == MODE_POCSAG) {
        lv_obj_t *lbl_ric = lv_label_create(opt_container);
        lv_label_set_text(lbl_ric, "RIC CODE (Tap to edit):");
        lv_obj_set_style_text_color(lbl_ric, G, 0);
        lv_obj_set_style_text_font(lbl_ric, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl_ric, LV_ALIGN_TOP_MID, 0, 10);
        
        lv_obj_t *btn_ric = lv_button_create(opt_container);
        lv_obj_set_size(btn_ric, 200, 40);
        lv_obj_align(btn_ric, LV_ALIGN_TOP_MID, 0, 35);
        lv_obj_set_style_bg_color(btn_ric, BG, 0);
        lv_obj_set_style_border_color(btn_ric, G, 0);
        lv_obj_set_style_border_width(btn_ric, 1, 0);
        lv_obj_add_event_cb(btn_ric, ric_click_cb, LV_EVENT_CLICKED, nullptr);
        
        lv_obj_t *lbl_ric_val = lv_label_create(btn_ric);
        char buf[32]; snprintf(buf, sizeof(buf), "%lu", (unsigned long)popup_selected_ric);
        lv_label_set_text(lbl_ric_val, buf);
        lv_obj_set_style_text_color(lbl_ric_val, G, 0);
        lv_obj_center(lbl_ric_val);
        
        lv_obj_t *lbl_freq = lv_label_create(opt_container);
        lv_label_set_text(lbl_freq, "Freq: 439.9875 MHz");
        lv_obj_set_style_text_color(lbl_freq, D, 0);
        lv_obj_set_style_text_font(lbl_freq, &lv_font_montserrat_16, 0);
        lv_obj_align(lbl_freq, LV_ALIGN_TOP_MID, 0, 90);
    } else {
        lv_obj_t *lbl_title = lv_label_create(opt_container);
        lv_label_set_text(lbl_title, "SELECT FREQUENCY:");
        lv_obj_set_style_text_color(lbl_title, G, 0);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);
        
        lv_obj_t *btn_prev = lv_button_create(opt_container);
        lv_obj_set_size(btn_prev, 40, 40);
        lv_obj_align(btn_prev, LV_ALIGN_TOP_MID, -90, 35);
        lv_obj_set_style_bg_color(btn_prev, BG, 0);
        lv_obj_set_style_border_color(btn_prev, G, 0);
        lv_obj_set_style_border_width(btn_prev, 1, 0);
        lv_obj_add_event_cb(btn_prev, freq_prev_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t *lbl_prev = lv_label_create(btn_prev);
        lv_label_set_text(lbl_prev, "<");
        lv_obj_set_style_text_color(lbl_prev, G, 0);
        lv_obj_center(lbl_prev);
        
        lv_obj_t *lbl_val = lv_label_create(opt_container);
        char buf[32]; snprintf(buf, sizeof(buf), "%.4f MHz", popup_selected_freq);
        lv_label_set_text(lbl_val, buf);
        lv_obj_set_style_text_color(lbl_val, G, 0);
        lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_16, 0);
        lv_obj_align(lbl_val, LV_ALIGN_TOP_MID, 0, 45);
        
        lv_obj_t *btn_next = lv_button_create(opt_container);
        lv_obj_set_size(btn_next, 40, 40);
        lv_obj_align(btn_next, LV_ALIGN_TOP_MID, 90, 35);
        lv_obj_set_style_bg_color(btn_next, BG, 0);
        lv_obj_set_style_border_color(btn_next, G, 0);
        lv_obj_set_style_border_width(btn_next, 1, 0);
        lv_obj_add_event_cb(btn_next, freq_next_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t *lbl_next = lv_label_create(btn_next);
        lv_label_set_text(lbl_next, ">");
        lv_obj_set_style_text_color(lbl_next, G, 0);
        lv_obj_center(lbl_next);
    }
}

static void save_and_close_cb(lv_event_t *e) {
    (void)e; haptic_click();
    lora_svc_set_ric(popup_selected_ric);
    lora_svc_set_freq(popup_selected_freq);
    lora_svc_start(popup_selected_mode);
    
    if (popup_win) {
        lv_obj_delete(popup_win);
        popup_win = nullptr;
        opt_container = nullptr;
        for (int i = 0; i < 2; i++) mode_select_btn[i] = nullptr;
    }
}

static void toggle_other_devices_cb(lv_event_t *e) {
    (void)e; haptic_click();
    lora_svc_stop();
    
    popup_selected_mode = MODE_POCSAG;
    popup_selected_freq = 439.9875f;
    popup_selected_ric = lora_svc_get_ric();
    
    popup_win = lv_obj_create(scr);
    lv_obj_set_size(popup_win, 380, 440);
    lv_obj_center(popup_win);
    lv_obj_set_style_bg_color(popup_win, BG, 0);
    lv_obj_set_style_border_color(popup_win, G, 0);
    lv_obj_set_style_border_width(popup_win, 2, 0);
    lv_obj_set_style_radius(popup_win, 10, 0);
    lv_obj_clear_flag(popup_win, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *title = lv_label_create(popup_win);
    lv_label_set_text(title, "OTHER DEVICES CFG");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    const char *modes_str[] = {"Pocsag", "Bruce"};
    for (int i = 0; i < 2; i++) {
        mode_select_btn[i] = lv_button_create(popup_win);
        lv_obj_set_size(mode_select_btn[i], 110, 40);
        lv_obj_align(mode_select_btn[i], LV_ALIGN_TOP_MID, (i == 0) ? -65 : 65, 45);
        lv_obj_set_style_bg_color(mode_select_btn[i], BG, 0);
        lv_obj_set_style_border_color(mode_select_btn[i], G, 0);
        lv_obj_set_style_border_width(mode_select_btn[i], 1, 0);
        lv_obj_set_style_radius(mode_select_btn[i], 5, 0);
        lv_obj_add_event_cb(mode_select_btn[i], mode_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        lv_obj_t *lbl = lv_label_create(mode_select_btn[i]);
        lv_label_set_text(lbl, modes_str[i]);
        lv_obj_set_style_text_color(lbl, G, 0);
        lv_obj_center(lbl);
    }
    
    lv_obj_set_style_bg_color(mode_select_btn[0], D, 0);
    lv_obj_set_style_bg_opa(mode_select_btn[0], LV_OPA_COVER, 0);
    
    opt_container = lv_obj_create(popup_win);
    lv_obj_set_size(opt_container, 340, 240);
    lv_obj_align(opt_container, LV_ALIGN_TOP_MID, 0, 95);
    lv_obj_set_style_bg_opa(opt_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(opt_container, 0, 0);
    lv_obj_clear_flag(opt_container, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *btn_save = lv_button_create(popup_win);
    lv_obj_set_size(btn_save, 260, 40);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(btn_save, BG, 0);
    lv_obj_set_style_border_color(btn_save, G, 0);
    lv_obj_set_style_border_width(btn_save, 1, 0);
    lv_obj_add_event_cb(btn_save, save_and_close_cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "SAVE & CLOSE");
    lv_obj_set_style_text_color(lbl_save, G, 0);
    lv_obj_center(lbl_save);
    
    refresh_popup_opts();
}

static void style_keyboard_pipboy(lv_obj_t *kb, lv_obj_t *ta) {
    lv_obj_set_style_bg_color(ta, BG, 0);
    lv_obj_set_style_border_color(ta, G, 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_text_color(ta, G, 0);
    lv_obj_set_style_radius(ta, 0, 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_18, 0);

    lv_obj_set_style_bg_color(kb, BG, 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(kb, BG, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, D, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, G, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_16, LV_PART_ITEMS);
}

static void kb_msg_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        haptic_click();
        if (ta_message) {
            const char *msg_txt = lv_textarea_get_text(ta_message);
            if (msg_txt && strlen(msg_txt) > 0) {
                lora_svc_send_message(msg_txt);

                int len = strlen(msg_buf);
                if (len > 900) {
                    char *nl = strchr(msg_buf + 200, '\n');
                    if (nl) { 
                        memmove(msg_buf, nl+1, strlen(nl)); 
                        len = strlen(msg_buf); 
                    }
                }
                snprintf(msg_buf + len, sizeof(msg_buf) - len, "[local] %s\n", msg_txt);
                if (lbl_msgs) {
                    lv_label_set_text(lbl_msgs, msg_buf);
                }
            }
        }

        if (lora_kbd_container) {
            lv_obj_delete(lora_kbd_container);
            lora_kbd_container = nullptr;
            ta_message = nullptr;
            kb_message = nullptr;
        }

    } else if (code == LV_EVENT_CANCEL) {
        haptic_click();
        if (lora_kbd_container) {
            lv_obj_delete(lora_kbd_container);
            lora_kbd_container = nullptr;
            ta_message = nullptr;
            kb_message = nullptr;
        }
    }
}

static void show_message_keyboard(void) {
    lora_kbd_container = lv_obj_create(scr);
    lv_obj_set_size(lora_kbd_container, 410, 502);
    lv_obj_set_pos(lora_kbd_container, 0, 0);
    lv_obj_set_style_bg_color(lora_kbd_container, BG, 0);
    lv_obj_set_style_bg_opa(lora_kbd_container, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(lora_kbd_container, 10, 0);
    lv_obj_clear_flag(lora_kbd_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(lora_kbd_container);
    LoraMode mode = lora_svc_get_mode();
    if (mode == MODE_MESHTASTIC) {
        lv_label_set_text(title, "Send Meshtastic:");
    } else if (mode == MODE_POCSAG) {
        lv_label_set_text(title, "Send Pocsag Pager:");
    } else if (mode == MODE_BRUCE) {
        lv_label_set_text(title, "Send Bruce Chat:");
    } else {
        lv_label_set_text(title, "Send MeshCore Msg:");
    }
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 10);

    ta_message = lv_textarea_create(lora_kbd_container);
    lv_textarea_set_password_mode(ta_message, false);
    lv_textarea_set_one_line(ta_message, true);
    lv_obj_set_size(ta_message, 410 - 40, 50);
    lv_obj_align(ta_message, LV_ALIGN_TOP_MID, 0, SAFE_TOP + 75);

    kb_message = lv_keyboard_create(lora_kbd_container);
    lv_keyboard_set_textarea(kb_message, ta_message);
    lv_obj_set_size(kb_message, 410 - 20, 240);
    lv_obj_align(kb_message, LV_ALIGN_BOTTOM_MID, 0, -SAFE_BOTTOM);
    lv_obj_add_event_cb(kb_message, kb_msg_event_cb, LV_EVENT_ALL, nullptr);

    style_keyboard_pipboy(kb_message, ta_message);
}

static void send_msg_btn_cb(lv_event_t *e) {
    (void)e; haptic_click();
    show_message_keyboard();
}

void lora_app_create(lv_obj_t *parent) {
    scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, 410, 502);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_center(scr);

    int x = SAFE_LEFT + 10, y = SAFE_TOP;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "[ RADIO CHAT ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y);

    y += 25;
    lbl_status = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_status, D, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_status, x, y);

    y += 22;
    btn_m = lv_button_create(scr);
    lv_obj_set_size(btn_m, 340, 35); lv_obj_set_pos(btn_m, x, y);
    lv_obj_set_style_bg_color(btn_m, BG, 0);
    lv_obj_set_style_border_color(btn_m, G, 0);
    lv_obj_set_style_border_width(btn_m, 1, 0);
    lv_obj_set_style_radius(btn_m, 0, 0);
    lv_obj_add_event_cb(btn_m, toggle_meshcore_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl_m = lv_label_create(btn_m);
    lv_label_set_text(bl_m, "MESHCORE");
    lv_obj_set_style_text_color(bl_m, G, 0); lv_obj_center(bl_m);

    y += 40;
    btn_t = lv_button_create(scr);
    lv_obj_set_size(btn_t, 340, 35); lv_obj_set_pos(btn_t, x, y);
    lv_obj_set_style_bg_color(btn_t, BG, 0);
    lv_obj_set_style_border_color(btn_t, G, 0);
    lv_obj_set_style_border_width(btn_t, 1, 0);
    lv_obj_set_style_radius(btn_t, 0, 0);
    lv_obj_add_event_cb(btn_t, toggle_meshtastic_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl_t = lv_label_create(btn_t);
    lv_label_set_text(bl_t, "MESHTASTIC");
    lv_obj_set_style_text_color(bl_t, G, 0); lv_obj_center(bl_t);

    y += 40;
    btn_o = lv_button_create(scr);
    lv_obj_set_size(btn_o, 340, 35); lv_obj_set_pos(btn_o, x, y);
    lv_obj_set_style_bg_color(btn_o, BG, 0);
    lv_obj_set_style_border_color(btn_o, G, 0);
    lv_obj_set_style_border_width(btn_o, 1, 0);
    lv_obj_set_style_radius(btn_o, 0, 0);
    lv_obj_add_event_cb(btn_o, toggle_other_devices_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl_o = lv_label_create(btn_o);
    lv_label_set_text(bl_o, "OTHER DEVICES");
    lv_obj_set_style_text_color(bl_o, G, 0); lv_obj_center(bl_o);

    y += 42;
    lv_obj_t *ml = lv_label_create(scr);
    lv_label_set_text(ml, "MESSAGES:");
    lv_obj_set_style_text_color(ml, D, 0);
    lv_obj_set_style_text_font(ml, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(ml, x, y);

    y += 18;
    lv_obj_t *terminal_box = lv_obj_create(scr);
    lv_obj_set_size(terminal_box, 340, 160);
    lv_obj_set_pos(terminal_box, x, y);
    lv_obj_set_style_bg_color(terminal_box, BG, 0);
    lv_obj_set_style_border_color(terminal_box, G, 0);
    lv_obj_set_style_border_width(terminal_box, 1, 0);
    lv_obj_set_style_radius(terminal_box, 0, 0);
    lv_obj_set_scroll_dir(terminal_box, LV_DIR_ALL);
    lv_obj_set_style_pad_all(terminal_box, 5, 0);

    lbl_msgs = lv_label_create(terminal_box);
    lv_label_set_text(lbl_msgs, msg_buf[0] ? msg_buf : "No messages\n\nSend via web interface");
    lv_obj_set_style_text_color(lbl_msgs, G, 0);
    lv_obj_set_style_text_font(lbl_msgs, &lv_font_montserrat_16, 0);
    lv_obj_set_width(lbl_msgs, 310);
    lv_label_set_long_mode(lbl_msgs, LV_LABEL_LONG_WRAP);
    lv_label_set_recolor(lbl_msgs, true);
    lv_obj_align(lbl_msgs, LV_ALIGN_TOP_LEFT, 0, 0);

    y += 165;
    btn_send = lv_button_create(scr);
    lv_obj_set_size(btn_send, 340, 35); lv_obj_set_pos(btn_send, x, y);
    lv_obj_set_style_bg_color(btn_send, BG, 0);
    lv_obj_set_style_border_color(btn_send, G, 0);
    lv_obj_set_style_border_width(btn_send, 1, 0);
    lv_obj_set_style_radius(btn_send, 0, 0);
    lv_obj_add_event_cb(btn_send, send_msg_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl_send = lv_label_create(btn_send);
    lv_label_set_text(bl_send, "SEND MESSAGE");
    lv_obj_set_style_text_color(bl_send, G, 0); lv_obj_center(bl_send);

    if (!lora_svc_is_running()) lora_svc_start(MODE_MESHCORE);
    check_session_divider();
}

void lora_app_update(void) {
    if (!scr) return;

    check_session_divider();

    if (lbl_status) {
        if (lora_svc_is_running()) {
            LoraMode m = lora_svc_get_mode();
            char b[64];
            if (m == MODE_MESHCORE) {
                snprintf(b, sizeof(b), "MESHCORE | Nodes:%d Msgs:%d", lora_svc_node_count(), lora_svc_message_count());
            } else if (m == MODE_MESHTASTIC) {
                snprintf(b, sizeof(b), "MESHTASTIC | Nodes:%d Msgs:%d", lora_svc_node_count(), lora_svc_message_count());
            } else if (m == MODE_POCSAG) {
                snprintf(b, sizeof(b), "POCSAG PAGER | RIC:%lu", (unsigned long)lora_svc_get_ric());
            } else if (m == MODE_BRUCE) {
                snprintf(b, sizeof(b), "BRUCE CHAT | %.3f MHz", lora_svc_get_freq());
            }
            lv_label_set_text(lbl_status, b);
        } else {
            lv_label_set_text(lbl_status, "OFF");
        }
    }

    if (btn_m && btn_t && btn_o) {
        if (lora_svc_is_running()) {
            LoraMode mode = lora_svc_get_mode();
            if (mode == MODE_MESHCORE) {
                lv_obj_set_style_bg_color(btn_m, D, 0);
                lv_obj_set_style_bg_opa(btn_m, LV_OPA_COVER, 0);
                lv_obj_set_style_bg_color(btn_t, BG, 0);
                lv_obj_set_style_bg_opa(btn_t, LV_OPA_TRANSP, 0);
                lv_obj_set_style_bg_color(btn_o, BG, 0);
                lv_obj_set_style_bg_opa(btn_o, LV_OPA_TRANSP, 0);
            } else if (mode == MODE_MESHTASTIC) {
                lv_obj_set_style_bg_color(btn_t, D, 0);
                lv_obj_set_style_bg_opa(btn_t, LV_OPA_COVER, 0);
                lv_obj_set_style_bg_color(btn_m, BG, 0);
                lv_obj_set_style_bg_opa(btn_m, LV_OPA_TRANSP, 0);
                lv_obj_set_style_bg_color(btn_o, BG, 0);
                lv_obj_set_style_bg_opa(btn_o, LV_OPA_TRANSP, 0);
            } else {
                lv_obj_set_style_bg_color(btn_o, D, 0);
                lv_obj_set_style_bg_opa(btn_o, LV_OPA_COVER, 0);
                lv_obj_set_style_bg_color(btn_m, BG, 0);
                lv_obj_set_style_bg_opa(btn_m, LV_OPA_TRANSP, 0);
                lv_obj_set_style_bg_color(btn_t, BG, 0);
                lv_obj_set_style_bg_opa(btn_t, LV_OPA_TRANSP, 0);
            }
        } else {
            lv_obj_set_style_bg_color(btn_m, BG, 0);
            lv_obj_set_style_bg_opa(btn_m, LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(btn_t, BG, 0);
            lv_obj_set_style_bg_opa(btn_t, LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(btn_o, BG, 0);
            lv_obj_set_style_bg_opa(btn_o, LV_OPA_TRANSP, 0);
        }
    }

    if (btn_send) {
        if (lora_svc_is_running()) {
            lv_obj_remove_state(btn_send, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btn_send, LV_STATE_DISABLED);
        }
    }

    if (lora_svc_has_new_message() && lbl_msgs) {
        const MeshMsg *m = lora_svc_last_message();
        if (m) {
            int len = strlen(msg_buf);
            if (len > 900) {
                char *nl = strchr(msg_buf + 200, '\n');
                if (nl) { memmove(msg_buf, nl+1, strlen(nl)); len = strlen(msg_buf); }
            }
            snprintf(msg_buf + len, sizeof(msg_buf) - len, "%s\n", m->text);
            lv_label_set_text(lbl_msgs, msg_buf);
            
            
            lv_obj_scroll_to_y(lv_obj_get_parent(lbl_msgs), 9999, LV_ANIM_ON);
        }
    }
}

void lora_app_destroy(void) {
    if (scr) { lv_obj_delete(scr); scr = nullptr; }
    lbl_status = lbl_msgs = nullptr;
    lora_kbd_container = nullptr;
    ta_message = nullptr;
    kb_message = nullptr;
    btn_send = nullptr;
    btn_m = btn_t = btn_o = nullptr;
    
    if (popup_win) {
        lv_obj_delete(popup_win);
        popup_win = nullptr;
    }
    opt_container = nullptr;
    for (int i = 0; i < 2; i++) mode_select_btn[i] = nullptr;
    ric_ta = ric_kb = nullptr;
}
