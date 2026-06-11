//
// Ecran d'accueil : debit cible, volumes du jour / cumule, prochaine dose,
// bouton "doser maintenant".
//
#include "ui/ui_internal.h"

static lv_obj_t *home_pill, *home_pill_dot, *home_pill_lbl;
static lv_obj_t *home_flow, *home_unit, *home_today, *home_tank, *home_tank_bar, *home_next, *home_bar;

// ======================= Rafraichissement =======================
void refreshHome() {
  bool pumping = doser_isPumping();
  lv_obj_set_style_bg_color(home_pill, pumping ? lv_color_hex(0x113a2f) : C_SURFACE2, 0);
  lv_obj_set_style_bg_color(home_pill_dot, pumping ? C_GREEN : C_MUTED, 0);
  lv_obj_set_style_text_color(home_pill_lbl, pumping ? C_GREEN : C_MUTED, 0);
  lv_label_set_text(home_pill_lbl, pumping ? "INJECTION" : "VEILLE");

  lv_label_set_text_fmt(home_flow, "%.0f", doser_getDosage());
  lv_obj_align_to(home_unit, home_flow, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
  lv_label_set_text_fmt(home_today, "%.0f ml", doser_getVolumeToday());

  float pct = doser_getTankPercent();
  lv_color_t tankCol = (pct <= 15.0f) ? C_RED : (pct <= 35.0f) ? C_AMBER : C_GREEN;
  lv_label_set_text_fmt(home_tank, "%.0f %%", pct);
  lv_obj_set_style_text_color(home_tank, tankCol, 0);
  lv_bar_set_value(home_tank_bar, (int)(pct + 0.5f), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(home_tank_bar, tankCol, LV_PART_INDICATOR);

  if (doser_getFlow() <= 0.0f) {
    lv_label_set_text(home_next, "Calibrez la pompe");
    lv_bar_set_value(home_bar, 0, LV_ANIM_OFF);
  } else if (doser_getDosage() <= 0.0f) {
    lv_label_set_text(home_next, "Reglez le debit");
    lv_bar_set_value(home_bar, 0, LV_ANIM_OFF);
  } else if (pumping) {
    lv_label_set_text(home_next, "Injection en cours...");
    lv_bar_set_value(home_bar, 100, LV_ANIM_OFF);
  } else {
    int s = doser_secondsToNext();
    lv_label_set_text_fmt(home_next, "Prochaine dose : %d:%02d", s / 60, s % 60);
    int pct = 100 - (int)(100L * s * 1000L / DOSE_INTERVAL_MS);
    lv_bar_set_value(home_bar, constrain(pct, 0, 100), LV_ANIM_OFF);
  }
}

// ======================= Setters publics (appeles par le doseur) =======================
void ui_setDosage(float v) {
  flowSetValue(v);   // l'ecran Debit gere son propre compteur
  if (home_flow) {
    lv_label_set_text_fmt(home_flow, "%.0f", v);
    lv_obj_align_to(home_unit, home_flow, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
  }
}
void ui_setToday(float v) { if (home_today) lv_label_set_text_fmt(home_today, "%.0f ml", v); }

// ======================= Evenements =======================
static void cb_goto_flow(lv_event_t *) { setTab(T_FLOW); }
static void cb_dose_now(lv_event_t *)  { doser_triggerNow(); refreshHome(); }

// ======================= Construction =======================
void buildHome(lv_obj_t *p) {
  lv_obj_t *title = mkLabel(p, "Doseur Chlore", &lv_font_montserrat_14, C_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 4);

  // Pastille d'etat
  home_pill = lv_obj_create(p);
  lv_obj_set_size(home_pill, 96, 20);
  lv_obj_align(home_pill, LV_ALIGN_TOP_RIGHT, -8, 3);
  lv_obj_set_style_radius(home_pill, 10, 0);
  lv_obj_set_style_border_width(home_pill, 0, 0);
  lv_obj_set_style_pad_all(home_pill, 0, 0);
  lv_obj_clear_flag(home_pill, LV_OBJ_FLAG_SCROLLABLE);
  home_pill_dot = lv_obj_create(home_pill);
  lv_obj_set_size(home_pill_dot, 7, 7);
  lv_obj_set_style_radius(home_pill_dot, 4, 0);
  lv_obj_set_style_border_width(home_pill_dot, 0, 0);
  lv_obj_align(home_pill_dot, LV_ALIGN_LEFT_MID, 10, 0);
  home_pill_lbl = mkLabel(home_pill, "VEILLE", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(home_pill_lbl, LV_ALIGN_LEFT_MID, 22, 0);

  // Carte debit cible
  lv_obj_t *fc = mkCard(p, 8, 34, PW - 16, 54);
  lv_obj_t *dic = mkLabel(fc, LV_SYMBOL_TINT, &lv_font_montserrat_22, C_ACCENT);
  lv_obj_align(dic, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t *fl = mkLabel(fc, "DEBIT CIBLE", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(fl, LV_ALIGN_TOP_LEFT, 34, -2);
  home_flow = mkLabel(fc, "0", &lv_font_montserrat_28, C_TEXT);
  lv_obj_align(home_flow, LV_ALIGN_BOTTOM_LEFT, 34, 4);
  home_unit = mkLabel(fc, "ml/h", &lv_font_montserrat_14, C_MUTED);
  lv_obj_align_to(home_unit, home_flow, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
  lv_obj_t *chev = mkBtn(fc, LV_SYMBOL_RIGHT, C_SURFACE2, C_ACCENT, NULL);
  lv_obj_set_size(chev, 34, 34);
  lv_obj_align(chev, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(chev, cb_goto_flow, LV_EVENT_CLICKED, NULL);

  // Deux statistiques
  lv_obj_t *c1 = mkCard(p, 8, 98, (PW - 22) / 2, 46);
  mkLabel(c1, "AUJOURD'HUI", &lv_font_montserrat_12, C_MUTED);
  home_today = mkLabel(c1, "0 ml", &lv_font_montserrat_16, C_TEXT);
  lv_obj_align(home_today, LV_ALIGN_BOTTOM_LEFT, 0, 2);

  lv_obj_t *c2 = mkCard(p, 8 + (PW - 22) / 2 + 6, 98, (PW - 22) / 2, 46);
  mkLabel(c2, "NIVEAU CUVE", &lv_font_montserrat_12, C_MUTED);
  home_tank = mkLabel(c2, "100 %", &lv_font_montserrat_16, C_GREEN);
  lv_obj_align(home_tank, LV_ALIGN_BOTTOM_LEFT, 0, 2);
  // Mini-jauge verticale facon reservoir (se remplit du bas)
  home_tank_bar = lv_bar_create(c2);
  lv_obj_set_size(home_tank_bar, 11, 28);
  lv_obj_align(home_tank_bar, LV_ALIGN_RIGHT_MID, 2, 0);
  lv_obj_set_style_radius(home_tank_bar, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(home_tank_bar, 3, LV_PART_INDICATOR);
  lv_obj_set_style_border_width(home_tank_bar, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(home_tank_bar, C_LINE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(home_tank_bar, C_STATUS, LV_PART_MAIN);
  lv_obj_set_style_bg_color(home_tank_bar, C_GREEN, LV_PART_INDICATOR);
  lv_bar_set_range(home_tank_bar, 0, 100);

  // Prochaine dose
  home_next = mkLabel(p, "", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(home_next, LV_ALIGN_TOP_LEFT, 8, 158);
  home_bar = lv_bar_create(p);
  lv_obj_set_size(home_bar, PW - 16, 6);
  lv_obj_align(home_bar, LV_ALIGN_TOP_LEFT, 8, 178);
  lv_obj_set_style_bg_color(home_bar, C_SURFACE2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(home_bar, C_ACCENT, LV_PART_INDICATOR);
  lv_bar_set_range(home_bar, 0, 100);

  // Bouton doser maintenant
  lv_obj_t *b = mkBtn(p, "DOSER MAINTENANT", C_GREEN, lv_color_hex(0x063226), NULL);
  lv_obj_set_size(b, PW - 16, 40);
  lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_add_event_cb(b, cb_dose_now, LV_EVENT_CLICKED, NULL);
}
