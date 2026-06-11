//
// Ecran "Historique" : total + moyenne de la semaine et graphique a barres
// des 7 derniers jours (le dernier = aujourd'hui).
//
#include "ui/ui_internal.h"

static lv_obj_t *hist_total, *hist_avg, *hist_bar[7], *hist_lbl[7];

void refreshHist() {
  float w[7];
  doser_getWeek(w);
  float maxv = 1.0f, total = 0.0f;
  for (int i = 0; i < 7; i++) { if (w[i] > maxv) maxv = w[i]; total += w[i]; }

  if (total >= 1000.0f) lv_label_set_text_fmt(hist_total, "%.2f L", total / 1000.0f);
  else                  lv_label_set_text_fmt(hist_total, "%.0f ml", total);
  lv_label_set_text_fmt(hist_avg, "%.0f ml", total / 7.0f);

  // Lettres des jours (le dernier = aujourd'hui)
  static const char *J[7] = {"D", "L", "M", "M", "J", "V", "S"};
  time_t now = time(nullptr);
  int wday = -1;
  if (now > 1700000000) { struct tm t; localtime_r(&now, &t); wday = t.tm_wday; }

  for (int i = 0; i < 7; i++) {
    int h = (int)(w[i] / maxv * 58.0f);
    lv_obj_set_height(hist_bar[i], h < 3 ? 3 : h);
    lv_obj_set_style_bg_color(hist_bar[i], i == 6 ? C_ACCENT : C_ACCENT_D, 0);
    if (wday >= 0) {
      int d = (wday - (6 - i)) % 7; if (d < 0) d += 7;
      lv_label_set_text(hist_lbl[i], J[d]);
    } else {
      lv_label_set_text_fmt(hist_lbl[i], "%d", i - 6);
    }
  }
}

void buildHist(lv_obj_t *p) {
  lv_obj_t *ttl = mkLabel(p, "HISTORIQUE - 7 JOURS", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 8, 2);

  lv_obj_t *c1 = mkCard(p, 8, 18, (PW - 22) / 2, 38);
  mkLabel(c1, "TOTAL SEMAINE", &lv_font_montserrat_12, C_MUTED);
  hist_total = mkLabel(c1, "0 ml", &lv_font_montserrat_16, C_TEXT);
  lv_obj_align(hist_total, LV_ALIGN_BOTTOM_LEFT, 0, 2);

  lv_obj_t *c2 = mkCard(p, 8 + (PW - 22) / 2 + 6, 18, (PW - 22) / 2, 38);
  mkLabel(c2, "MOY. / JOUR", &lv_font_montserrat_12, C_MUTED);
  hist_avg = mkLabel(c2, "0 ml", &lv_font_montserrat_16, C_TEXT);
  lv_obj_align(hist_avg, LV_ALIGN_BOTTOM_LEFT, 0, 2);

  // Graphique a barres
  lv_obj_t *chart = mkCard(p, 8, 62, PW - 16, PH - 66);
  int bw = (PW - 16 - 14) / 7;
  for (int i = 0; i < 7; i++) {
    int bx = 4 + i * bw;
    hist_bar[i] = lv_obj_create(chart);
    lv_obj_set_size(hist_bar[i], bw - 5, 3);
    lv_obj_set_style_radius(hist_bar[i], 3, 0);
    lv_obj_set_style_border_width(hist_bar[i], 0, 0);
    lv_obj_set_style_bg_color(hist_bar[i], C_ACCENT_D, 0);
    lv_obj_align(hist_bar[i], LV_ALIGN_BOTTOM_LEFT, bx, -14);
    hist_lbl[i] = mkLabel(chart, "-", &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(hist_lbl[i], LV_ALIGN_BOTTOM_LEFT, bx + (bw - 5) / 2 - 4, 2);
  }
}
