//
// Helpers de style reutilisables et toast ephemere.
//
#include "ui/ui_internal.h"

// ======================= Helpers de style =======================
lv_obj_t *mkLabel(lv_obj_t *par, const char *txt, const lv_font_t *font, lv_color_t col) {
  lv_obj_t *l = lv_label_create(par);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, col, 0);
  lv_label_set_text(l, txt);
  return l;
}

lv_obj_t *mkCard(lv_obj_t *par, int x, int y, int w, int h) {
  lv_obj_t *c = lv_obj_create(par);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_color(c, C_SURFACE, 0);
  lv_obj_set_style_border_color(c, C_LINE, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_radius(c, 10, 0);
  lv_obj_set_style_pad_all(c, 7, 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

// Bouton primaire plein
lv_obj_t *mkBtn(lv_obj_t *par, const char *txt, lv_color_t bg, lv_color_t fg, lv_obj_t **lblOut) {
  lv_obj_t *b = lv_btn_create(par);
  lv_obj_set_style_bg_color(b, bg, 0);
  lv_obj_set_style_radius(b, 9, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t *l = lv_label_create(b);
  lv_obj_set_style_text_color(l, fg, 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_label_set_text(l, txt);
  lv_obj_center(l);
  if (lblOut) *lblOut = l;
  return b;
}

// Gros bouton rond +/-
lv_obj_t *mkRound(lv_obj_t *par, const char *sym, lv_event_cb_t cb, int delta) {
  lv_obj_t *b = lv_btn_create(par);
  lv_obj_set_size(b, 52, 52);
  lv_obj_set_style_radius(b, 26, 0);
  lv_obj_set_style_bg_color(b, C_SURFACE2, 0);
  lv_obj_set_style_border_color(b, C_LINE, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)delta);
  lv_obj_add_event_cb(b, cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)delta);
  lv_obj_t *l = mkLabel(b, sym, &lv_font_montserrat_28, C_ACCENT);
  lv_obj_center(l);
  return b;
}

// ======================= Toast =======================
static lv_obj_t *toast_lbl;

static void toast_hide_cb(lv_timer_t *) { lv_obj_add_flag(toast_lbl, LV_OBJ_FLAG_HIDDEN); }

void showToast(const char *msg) {
  if (!toast_lbl) {
    toast_lbl = lv_label_create(lv_layer_top());
    lv_obj_set_style_bg_color(toast_lbl, C_GREEN, 0);
    lv_obj_set_style_bg_opa(toast_lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(toast_lbl, lv_color_hex(0x063226), 0);
    lv_obj_set_style_text_font(toast_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_radius(toast_lbl, 14, 0);
    lv_obj_set_style_pad_hor(toast_lbl, 14, 0);
    lv_obj_set_style_pad_ver(toast_lbl, 7, 0);
    lv_obj_align(toast_lbl, LV_ALIGN_BOTTOM_MID, 0, -52);
  }
  lv_label_set_text(toast_lbl, msg);
  lv_obj_clear_flag(toast_lbl, LV_OBJ_FLAG_HIDDEN);
  lv_timer_t *t = lv_timer_create(toast_hide_cb, 1400, NULL);
  lv_timer_set_repeat_count(t, 1);
}
