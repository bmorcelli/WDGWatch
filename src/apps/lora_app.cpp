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

#define G  lv_color_hex(0x00E5FF)
#define D  lv_color_hex(0x007280)
#define BG lv_color_hex(0x000000)

static void toggle_cb(lv_event_t *e) {
    (void)e; haptic_click();
    if (lora_svc_is_running()) lora_svc_stop();
    else lora_svc_start();
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

    y += 55;
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
}
