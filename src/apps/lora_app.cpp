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

#define G  lv_color_hex(0x00E5FF)
#define D  lv_color_hex(0x007280)
#define BG lv_color_hex(0x000000)

static void toggle_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (lora_svc_is_running()) lora_svc_stop();
    else lora_svc_start();
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
    lv_label_set_text(title, "Send MeshCore Msg:");
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
    lv_label_set_text(title, "[ MESHCORE ]");
    lv_obj_set_style_text_color(title, G, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y);

    y += 25;
    lbl_status = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_status, D, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(lbl_status, x, y);

    y += 22;
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 340, 45); lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, BG, 0);
    lv_obj_set_style_border_color(btn, G, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_add_event_cb(btn, toggle_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, "START / STOP MESHCORE");
    lv_obj_set_style_text_color(bl, G, 0); lv_obj_center(bl);

    y += 53;
    btn_send = lv_button_create(scr);
    lv_obj_set_size(btn_send, 340, 40); lv_obj_set_pos(btn_send, x, y);
    lv_obj_set_style_bg_color(btn_send, BG, 0);
    lv_obj_set_style_border_color(btn_send, G, 0);
    lv_obj_set_style_border_width(btn_send, 1, 0);
    lv_obj_set_style_radius(btn_send, 0, 0);
    lv_obj_add_event_cb(btn_send, send_msg_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl_send = lv_label_create(btn_send);
    lv_label_set_text(bl_send, "SEND MESSAGE");
    lv_obj_set_style_text_color(bl_send, G, 0); lv_obj_center(bl_send);

    y += 48;
    lv_obj_t *ml = lv_label_create(scr);
    lv_label_set_text(ml, "MESSAGES:");
    lv_obj_set_style_text_color(ml, D, 0);
    lv_obj_set_style_text_font(ml, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(ml, x, y);

    y += 16;
    lbl_msgs = lv_label_create(scr);
    lv_label_set_text(lbl_msgs, msg_buf[0] ? msg_buf : "No messages\n\nSend via web interface");
    lv_obj_set_style_text_color(lbl_msgs, G, 0);
    lv_obj_set_style_text_font(lbl_msgs, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_msgs, x, y);
    lv_obj_set_width(lbl_msgs, 350);
    lv_label_set_long_mode(lbl_msgs, LV_LABEL_LONG_WRAP);

    if (!lora_svc_is_running()) lora_svc_start();
}

void lora_app_update(void) {
    if (!scr) return;

    if (lbl_status) {
        if (lora_svc_is_running()) {
            char b[64]; snprintf(b, sizeof(b), "ON | Nodes:%d Msgs:%d",
                lora_svc_node_count(), lora_svc_message_count());
            lv_label_set_text(lbl_status, b);
        } else {
            lv_label_set_text(lbl_status, "OFF");
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
            snprintf(msg_buf + len, sizeof(msg_buf) - len, "[%s] %s\n", m->channel, m->text);
            lv_label_set_text(lbl_msgs, msg_buf);
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
}
