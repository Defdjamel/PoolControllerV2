//
// Onglet "Reglages" : tabview interne regroupant le WiFi (scan, connexion,
// deconnexion) et la calibration de la pompe. Inclut l'overlay clavier pour
// la saisie du mot de passe et le service periodique (scan + chrono calib).
//
#include "ui/ui_internal.h"

// ======================= Etat =======================
static lv_obj_t *settings_tv;
// Calibration
static lv_obj_t *cal_info, *cal_time, *cal_factor, *cal_btn_lbl, *cal_btn;
// WiFi
static lv_obj_t *wifi_conn_card, *wifi_conn_ssid, *wifi_conn_ip;
static lv_obj_t *wifi_list, *wifi_scan_st;
// Overlay mot de passe
static lv_obj_t *wifi_overlay, *lbl_ov_ssid, *ta_pwd, *lv_kbd;

static bool      scan_triggered = false;
static uint32_t  scan_start_ms  = 0;
static char      pending_ssid[64];

// ======================= Rafraichissements =======================
void calibRefresh() {
  if (doser_getFlow() > 0.0f) lv_label_set_text_fmt(cal_factor, "Facteur : %.2f ml/s", doser_getFlow());
  else                        lv_label_set_text(cal_factor, "Facteur : non calibre");

  switch (doser_calibState()) {
    case CAL_READY:
      lv_label_set_text(cal_info, "Placez un recipient de 100 ml sous la sortie, puis lancez la pompe.");
      lv_label_set_text(cal_time, "0.0 s");
      lv_label_set_text(cal_btn_lbl, "LANCER");
      lv_obj_set_style_bg_color(cal_btn, C_GREEN, 0);
      lv_obj_set_style_text_color(cal_btn_lbl, lv_color_hex(0x063226), 0);
      break;
    case CAL_RUNNING:
      lv_label_set_text(cal_info, "Arretez des que 100 ml sont atteints.");
      lv_label_set_text_fmt(cal_time, "%.1f s", doser_calibElapsed());
      lv_label_set_text(cal_btn_lbl, "ARRETER");
      lv_obj_set_style_bg_color(cal_btn, C_RED, 0);
      lv_obj_set_style_text_color(cal_btn_lbl, C_TEXT, 0);
      break;
    case CAL_DONE:
      lv_label_set_text_fmt(cal_info, "Debit mesure : %.2f ml/s", doser_getFlow());
      lv_label_set_text(cal_btn_lbl, "REFAIRE");
      lv_obj_set_style_bg_color(cal_btn, C_SURFACE2, 0);
      lv_obj_set_style_text_color(cal_btn_lbl, C_ACCENT, 0);
      break;
  }
}

void refreshWifi() {
  if (net_wifiConnected()) {
    lv_obj_clear_flag(wifi_conn_card, LV_OBJ_FLAG_HIDDEN);
    char ssid[48], ip[24];
    net_wifiSSID(ssid, sizeof(ssid));
    net_wifiIP(ip, sizeof(ip));
    lv_label_set_text(wifi_conn_ssid, ssid);
    lv_label_set_text_fmt(wifi_conn_ip, "Connecte  %s", ip);
  } else {
    lv_obj_add_flag(wifi_conn_card, LV_OBJ_FLAG_HIDDEN);
  }
}

void ui_setFlow(float v) { if (cal_factor) calibRefresh(); }

// ======================= Liste des reseaux =======================
static void cb_wifi_pick(lv_event_t *e);

static void populateWifiList(int count) {
  lv_obj_clean(wifi_list);
  for (int i = 0; i < count && i < 12; i++) {
    char ssid[48];
    net_scanGetSSID(i, ssid, sizeof(ssid));

    lv_obj_t *row = lv_obj_create(wifi_list);
    lv_obj_set_size(row, lv_pct(100), 30);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0x0f263c), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, cb_wifi_pick, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t *ic = mkLabel(row, LV_SYMBOL_WIFI, &lv_font_montserrat_14, C_MUTED);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *nm = mkLabel(row, ssid, &lv_font_montserrat_14, C_TEXT);
    lv_obj_align(nm, LV_ALIGN_LEFT_MID, 26, 0);

    lv_obj_t *ch = mkLabel(row, LV_SYMBOL_RIGHT, &lv_font_montserrat_14, C_MUTED);
    lv_obj_align(ch, LV_ALIGN_RIGHT_MID, 0, 0);
  }
}

// ======================= Overlay mot de passe =======================
static void hideWifiOverlay() {
  lv_keyboard_set_textarea(lv_kbd, NULL);
  lv_obj_add_flag(wifi_overlay, LV_OBJ_FLAG_HIDDEN);
}
static void cb_ov_ok(lv_event_t *) {
  net_connectTo(pending_ssid, lv_textarea_get_text(ta_pwd));
  hideWifiOverlay();
  showToast("Connexion...");
}
static void cb_ov_cancel(lv_event_t *) { hideWifiOverlay(); }

static void cb_wifi_pick(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  net_scanGetSSID(idx, pending_ssid, sizeof(pending_ssid));
  char buf[80]; snprintf(buf, sizeof(buf), "Reseau : %s", pending_ssid);
  lv_label_set_text(lbl_ov_ssid, buf);
  lv_textarea_set_text(ta_pwd, "");
  lv_obj_clear_flag(wifi_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(lv_kbd, ta_pwd);
}

static void buildWifiOverlay() {
  wifi_overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(wifi_overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(wifi_overlay, 0, 0);
  lv_obj_set_style_bg_color(wifi_overlay, C_BG, 0);
  lv_obj_set_style_bg_opa(wifi_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(wifi_overlay, 0, 0);
  lv_obj_set_style_radius(wifi_overlay, 0, 0);
  lv_obj_set_style_pad_all(wifi_overlay, 0, 0);
  lv_obj_clear_flag(wifi_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(wifi_overlay, LV_OBJ_FLAG_HIDDEN);

  lv_kbd = lv_keyboard_create(wifi_overlay);
  lv_obj_set_size(lv_kbd, SCREEN_WIDTH, 140);
  lv_obj_align(lv_kbd, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(lv_kbd, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_READY)  cb_ov_ok(NULL);
    if (lv_event_get_code(e) == LV_EVENT_CANCEL) cb_ov_cancel(NULL);
  }, LV_EVENT_ALL, NULL);

  lbl_ov_ssid = mkLabel(wifi_overlay, "Reseau : ...", &lv_font_montserrat_14, C_ACCENT);
  lv_obj_set_pos(lbl_ov_ssid, 8, 6);

  ta_pwd = lv_textarea_create(wifi_overlay);
  lv_textarea_set_one_line(ta_pwd, true);
  lv_textarea_set_password_mode(ta_pwd, true);
  lv_textarea_set_placeholder_text(ta_pwd, "Mot de passe");
  lv_obj_set_size(ta_pwd, SCREEN_WIDTH - 16, 32);
  lv_obj_set_pos(ta_pwd, 8, 28);

  lv_obj_t *ok = mkBtn(wifi_overlay, "OK", C_ACCENT, lv_color_hex(0x04222b), NULL);
  lv_obj_set_size(ok, 150, 28);
  lv_obj_set_pos(ok, 8, 66);
  lv_obj_add_event_cb(ok, cb_ov_ok, LV_EVENT_CLICKED, NULL);

  lv_obj_t *an = mkBtn(wifi_overlay, "Annuler", C_SURFACE2, C_TEXT, NULL);
  lv_obj_set_size(an, 150, 28);
  lv_obj_set_pos(an, 162, 66);
  lv_obj_add_event_cb(an, cb_ov_cancel, LV_EVENT_CLICKED, NULL);
}

// ======================= Evenements =======================
static void cb_calib_action(lv_event_t *) { doser_calibAction(); calibRefresh(); }
static void cb_settings_sub(lv_event_t *) { syncSettings(true); }

static void cb_wifi_rescan(lv_event_t *) {
  net_scanStart();
  lv_label_set_text(wifi_scan_st, "Recherche...");
  scan_triggered = true;
  scan_start_ms  = millis();
}
static void cb_wifi_disconnect(lv_event_t *) { net_disconnect(); refreshWifi(); }

// ======================= Sous-pages =======================
static void buildWifi(lv_obj_t *p) {
  mkLabel(p, "RESEAUX WIFI", &lv_font_montserrat_12, C_MUTED);

  lv_obj_t *rb = mkBtn(p, LV_SYMBOL_REFRESH, C_SURFACE2, C_ACCENT, NULL);
  lv_obj_set_size(rb, 34, 22);
  lv_obj_align(rb, LV_ALIGN_TOP_RIGHT, -4, -2);
  lv_obj_add_event_cb(rb, cb_wifi_rescan, LV_EVENT_CLICKED, NULL);

  // Carte du reseau connecte
  wifi_conn_card = mkCard(p, 4, 18, PW - 8, 38);
  lv_obj_set_style_border_color(wifi_conn_card, C_ACCENT_D, 0);
  lv_obj_t *ck = mkLabel(wifi_conn_card, LV_SYMBOL_WIFI, &lv_font_montserrat_16, C_GREEN);
  lv_obj_align(ck, LV_ALIGN_LEFT_MID, 0, 0);
  wifi_conn_ssid = mkLabel(wifi_conn_card, "", &lv_font_montserrat_14, C_TEXT);
  lv_obj_align(wifi_conn_ssid, LV_ALIGN_TOP_LEFT, 26, -2);
  wifi_conn_ip = mkLabel(wifi_conn_card, "", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(wifi_conn_ip, LV_ALIGN_BOTTOM_LEFT, 26, 2);
  lv_obj_t *xb = mkBtn(wifi_conn_card, LV_SYMBOL_CLOSE, C_SURFACE2, C_RED, NULL);
  lv_obj_set_size(xb, 30, 26);
  lv_obj_align(xb, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(xb, cb_wifi_disconnect, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(wifi_conn_card, LV_OBJ_FLAG_HIDDEN);

  wifi_scan_st = mkLabel(p, "DISPONIBLES", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(wifi_scan_st, LV_ALIGN_TOP_LEFT, 4, 60);

  // Liste defilante des reseaux (dans le sous-onglet, hauteur reduite)
  wifi_list = lv_obj_create(p);
  lv_obj_set_size(wifi_list, PW - 8, PH - 122);
  lv_obj_align(wifi_list, LV_ALIGN_TOP_LEFT, 4, 76);
  lv_obj_set_style_bg_opa(wifi_list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(wifi_list, 0, 0);
  lv_obj_set_style_pad_all(wifi_list, 0, 0);
  lv_obj_set_flex_flow(wifi_list, LV_FLEX_FLOW_COLUMN);
}

static void buildCalib(lv_obj_t *p) {
  mkLabel(p, "CALIBRATION POMPE", &lv_font_montserrat_12, C_MUTED);

  cal_info = lv_label_create(p);
  lv_label_set_long_mode(cal_info, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(cal_info, PW - 24);
  lv_obj_set_style_text_font(cal_info, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(cal_info, C_TEXT, 0);
  lv_obj_set_style_text_align(cal_info, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(cal_info, LV_ALIGN_TOP_MID, 0, 22);

  cal_time = mkLabel(p, "0.0 s", &lv_font_montserrat_28, C_ACCENT);
  lv_obj_align(cal_time, LV_ALIGN_CENTER, 0, -2);

  cal_factor = mkLabel(p, "Facteur : non calibre", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(cal_factor, LV_ALIGN_BOTTOM_MID, 0, -40);

  cal_btn = mkBtn(p, "LANCER", C_GREEN, lv_color_hex(0x063226), &cal_btn_lbl);
  lv_obj_set_size(cal_btn, 150, 32);
  lv_obj_align(cal_btn, LV_ALIGN_BOTTOM_MID, 0, -2);
  lv_obj_add_event_cb(cal_btn, cb_calib_action, LV_EVENT_CLICKED, NULL);
}

// ======================= Onglet Reglages =======================
void buildSettings(lv_obj_t *p) {
  lv_obj_set_style_pad_all(p, 0, 0);

  settings_tv = lv_tabview_create(p, LV_DIR_TOP, 30);
  lv_obj_set_size(settings_tv, PW, PH);
  lv_obj_set_style_bg_color(settings_tv, C_BG, 0);

  lv_obj_t *btns = lv_tabview_get_tab_btns(settings_tv);
  lv_obj_set_style_bg_color(btns, C_STATUS, 0);
  lv_obj_set_style_text_color(btns, C_MUTED, 0);
  lv_obj_set_style_text_font(btns, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(btns, C_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_color(btns, C_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);

  lv_obj_t *tw = lv_tabview_add_tab(settings_tv, "WiFi");
  lv_obj_t *tp = lv_tabview_add_tab(settings_tv, "Pompe");
  lv_obj_clear_flag(tw, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(tp, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(tw, 6, 0);
  lv_obj_set_style_pad_all(tp, 6, 0);
  lv_obj_set_style_bg_color(tw, C_BG, 0);
  lv_obj_set_style_bg_color(tp, C_BG, 0);

  buildWifi(tw);
  buildCalib(tp);
  lv_obj_add_event_cb(settings_tv, cb_settings_sub, LV_EVENT_VALUE_CHANGED, NULL);

  buildWifiOverlay();
}

// ======================= Navigation / service periodique =======================
// Synchronise l'etat materiel selon le sous-onglet actif :
//  - Pompe : prise de main sur le relais (calibration)
//  - WiFi  : lancement d'un scan a l'arrivee
void syncSettings(bool inSettings) {
  uint16_t sub = inSettings ? lv_tabview_get_tab_act(settings_tv) : 0xFF;

  if (inSettings && sub == SUB_PUMP) {
    if (!doser_inCalibration()) doser_calibEnter();
    calibRefresh();
  } else if (doser_inCalibration()) {
    doser_calibBack();
  }

  if (inSettings && sub == SUB_WIFI && !scan_triggered) {
    net_scanStart();
    lv_label_set_text(wifi_scan_st, "Recherche...");
    scan_triggered = true;
    scan_start_ms  = millis();
  }
}

void ui_settingsTick() {
  uint16_t sub = lv_tabview_get_tab_act(settings_tv);
  if (sub == SUB_PUMP && doser_calibState() == CAL_RUNNING) calibRefresh();
  if (sub == SUB_WIFI) {
    refreshWifi();
    if (scan_triggered) {
      int cnt = net_scanCount();
      static int last_cnt = -99;
      if (cnt != last_cnt) {
        Serial.printf("[SCAN] scanComplete=%d elapsed=%lums\n", cnt, millis() - scan_start_ms);
        last_cnt = cnt;
      }
      if (cnt >= 0) {
        last_cnt = -99;
        scan_triggered = false;
        populateWifiList(cnt);
        lv_label_set_text_fmt(wifi_scan_st, cnt > 0 ? "%d reseaux" : "Aucun reseau", cnt);
      } else if (cnt == -2 && millis() - scan_start_ms > 5000) {
        last_cnt = -99;
        scan_triggered = false;
        lv_label_set_text(wifi_scan_st, "Echec - reessayez");
      }
    }
  }
}
