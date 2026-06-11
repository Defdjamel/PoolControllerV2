#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "ui/ui.h"
#include "core/doser.h"
#include "net/net_blynk.h"
#include "config.h"

// Index des onglets (ordre de creation)
enum { TAB_HOME = 0, TAB_PUMP = 1, TAB_WIFI = 2 };

static TFT_eSPI tft;
static SPIClass touchSPI(HSPI);
static XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 10];

static lv_obj_t *tabview;
// Onglet HOME
static lv_obj_t *lbl_dosage, *dosage_slider, *lbl_today, *lbl_next, *lbl_status;
// Onglet POMPE
static lv_obj_t *lbl_flow, *lbl_calib_info, *lbl_calib_time, *btn_action_label;
// Onglet WIFI
static lv_obj_t *lbl_wifi_status;
static lv_obj_t *dd_networks;
static lv_obj_t *lbl_scan_st;
// Overlay saisie du mot de passe (enfant de lv_layer_top)
static lv_obj_t *wifi_overlay;
static lv_obj_t *lbl_ov_ssid;
static lv_obj_t *ta_pwd;
static lv_obj_t *lv_kbd;
static bool      scan_triggered = false;
static uint32_t  scan_start_ms  = 0;
static char      pending_ssid[64];

// ======================= Pilotes LVGL =======================
static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int16_t x = map(p.x, TS_MIN_X, TS_MAX_X, 0, SCREEN_WIDTH);
    int16_t y = map(p.y, TS_MIN_Y, TS_MAX_Y, 0, SCREEN_HEIGHT);
    data->state = LV_INDEV_STATE_PR;
    data->point.x = constrain(x, 0, SCREEN_WIDTH - 1);
    data->point.y = constrain(y, 0, SCREEN_HEIGHT - 1);
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ======================= Setters (appeles par le doser) =======================
void ui_setDosage(float v) {
  lv_label_set_text_fmt(lbl_dosage, "%.0f ml/h", v);
  if (dosage_slider) lv_slider_set_value(dosage_slider, (int)(v + 0.5f), LV_ANIM_OFF);
}

void ui_setFlow(float v) {
  if (v > 0.0f) lv_label_set_text_fmt(lbl_flow, "Debit : %.2f ml/s", v);
  else          lv_label_set_text(lbl_flow, "Debit : non calibre");
}

void ui_setToday(float v) { lv_label_set_text_fmt(lbl_today, "Aujourd'hui : %.0f ml", v); }

// ======================= Rafraichissements internes =======================
static void refreshNext() {
  if (doser_getFlow() <= 0.0f)        lv_label_set_text(lbl_next, "Calibrez la pompe");
  else if (doser_getDosage() <= 0.0f) lv_label_set_text(lbl_next, "Reglez le dosage");
  else if (doser_isPumping())         lv_label_set_text(lbl_next, "Injection en cours...");
  else {
    int s = doser_secondsToNext();
    lv_label_set_text_fmt(lbl_next, "Prochaine dose : %d:%02d", s / 60, s % 60);
  }
}

static void refreshStatus() {
  int s = net_status();
  static int last = -1;
  if (s == last) return;
  last = s;
  const char *txt = (s == 2) ? "Blynk : connecte"
                  : (s == 1) ? "WiFi OK, Blynk..."
                             : "WiFi...";
  lv_palette_t pal = (s == 2) ? LV_PALETTE_GREEN
                   : (s == 1) ? LV_PALETTE_ORANGE
                              : LV_PALETTE_RED;
  lv_label_set_text(lbl_status, txt);
  lv_obj_set_style_text_color(lbl_status, lv_palette_main(pal), 0);
}

static void refreshWifi() {
  char info[128];
  net_wifiInfo(info, sizeof(info));
  lv_label_set_text(lbl_wifi_status, info);
}

static void calibRefresh() {
  switch (doser_calibState()) {
    case CAL_READY:
      lv_label_set_text(lbl_calib_info, "Placez un recipient de 100 ml sous la sortie.");
      lv_label_set_text(lbl_calib_time, "0.0 s");
      lv_label_set_text(btn_action_label, "Demarrer");
      break;
    case CAL_RUNNING:
      lv_label_set_text(lbl_calib_info, "Arretez des que 100 ml sont atteints.");
      lv_label_set_text_fmt(lbl_calib_time, "%.1f s", doser_calibElapsed());
      lv_label_set_text(btn_action_label, "Arreter");
      break;
    case CAL_DONE:
      lv_label_set_text_fmt(lbl_calib_info, "Debit mesure : %.2f ml/s", doser_getFlow());
      lv_label_set_text(btn_action_label, "Refaire");
      break;
  }
}

// ======================= Evenements =======================
static void cb_dosage_slider(lv_event_t *e) {
  int v = lv_slider_get_value(lv_event_get_target(e));
  doser_setDosage((float)v);   // met a jour l'etat + le label + le slider
  net_publishDosage((float)v); // reflete le changement local vers l'app
}

static void cb_calib_action(lv_event_t *) {
  doser_calibAction();
  calibRefresh();
}

// L'onglet "Pompe" prend la main sur le relais : on suspend le dosage en y entrant,
// on le reprend en sortant.
static void cb_tab_changed(lv_event_t *) {
  uint16_t act = lv_tabview_get_tab_act(tabview);
  if (act == TAB_PUMP) {
    doser_calibEnter();
    calibRefresh();
  } else if (act == TAB_WIFI && !scan_triggered) {
    net_scanStart();
    lv_label_set_text(lbl_scan_st, "Scan...");
    scan_triggered = true;
    scan_start_ms  = millis();
  } else if (doser_inCalibration()) {
    doser_calibBack();
    refreshNext();
  }
}

// ======================= Callbacks overlay WiFi =======================
static void hideWifiOverlay() {
  lv_keyboard_set_textarea(lv_kbd, NULL);
  lv_obj_add_flag(wifi_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void cb_ov_ok(lv_event_t *) {
  const char *pass = lv_textarea_get_text(ta_pwd);
  net_connectTo(pending_ssid, pass);
  hideWifiOverlay();
}

static void cb_ov_cancel(lv_event_t *) { hideWifiOverlay(); }

static void cb_wifi_connect(lv_event_t *) {
  char ssid[64];
  lv_dropdown_get_selected_str(dd_networks, ssid, sizeof(ssid));
  if (ssid[0] == '\0' || ssid[0] == '-') return;  // rien de selectionne
  strlcpy(pending_ssid, ssid, sizeof(pending_ssid));
  char buf[80];
  snprintf(buf, sizeof(buf), "SSID : %s", ssid);
  lv_label_set_text(lbl_ov_ssid, buf);
  lv_textarea_set_text(ta_pwd, "");
  lv_obj_clear_flag(wifi_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(lv_kbd, ta_pwd);
}

static void cb_wifi_disconnect(lv_event_t *) { net_disconnect(); }

static void cb_wifi_scan(lv_event_t *) {
  net_scanStart();
  lv_label_set_text(lbl_scan_st, "Scan...");
  scan_triggered = true;
  scan_start_ms  = millis();
}

// ======================= Construction de l'UI =======================
static void buildHomeTab(lv_obj_t *tab) {
  lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(tab);
  lv_label_set_text(title, "Dosage");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

  lbl_dosage = lv_label_create(tab);
  lv_obj_set_style_text_font(lbl_dosage, &lv_font_montserrat_22, 0);
  lv_label_set_text(lbl_dosage, "0 ml/h");
  lv_obj_align(lbl_dosage, LV_ALIGN_TOP_MID, 0, 24);

  dosage_slider = lv_slider_create(tab);
  lv_obj_set_width(dosage_slider, 190);
  lv_slider_set_range(dosage_slider, 0, DOSAGE_MAX_ML_H);
  lv_obj_align(dosage_slider, LV_ALIGN_TOP_MID, 0, 66);
  lv_obj_add_event_cb(dosage_slider, cb_dosage_slider, LV_EVENT_VALUE_CHANGED, NULL);

  lbl_today = lv_label_create(tab);
  lv_label_set_text(lbl_today, "Aujourd'hui : 0 ml");
  lv_obj_align(lbl_today, LV_ALIGN_TOP_MID, 0, 96);

  lbl_next = lv_label_create(tab);
  lv_label_set_text(lbl_next, "");
  lv_obj_align(lbl_next, LV_ALIGN_TOP_MID, 0, 122);

  lbl_status = lv_label_create(tab);
  lv_label_set_text(lbl_status, "WiFi...");
  lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, 0);
}

static void buildPumpTab(lv_obj_t *tab) {
  lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

  lbl_flow = lv_label_create(tab);
  lv_obj_set_style_text_font(lbl_flow, &lv_font_montserrat_16, 0);
  lv_label_set_text(lbl_flow, "Debit : non calibre");
  lv_obj_align(lbl_flow, LV_ALIGN_TOP_MID, 0, 0);

  lbl_calib_info = lv_label_create(tab);
  lv_label_set_long_mode(lbl_calib_info, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_calib_info, 220);
  lv_obj_set_style_text_align(lbl_calib_info, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(lbl_calib_info, "Placez un recipient de 100 ml sous la sortie.");
  lv_obj_align(lbl_calib_info, LV_ALIGN_TOP_MID, 0, 28);

  lbl_calib_time = lv_label_create(tab);
  lv_obj_set_style_text_font(lbl_calib_time, &lv_font_montserrat_22, 0);
  lv_label_set_text(lbl_calib_time, "0.0 s");
  lv_obj_align(lbl_calib_time, LV_ALIGN_CENTER, 0, 5);

  lv_obj_t *btn = lv_btn_create(tab);
  lv_obj_set_size(btn, 150, 44);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_add_event_cb(btn, cb_calib_action, LV_EVENT_CLICKED, NULL);
  btn_action_label = lv_label_create(btn);
  lv_label_set_text(btn_action_label, "Demarrer");
  lv_obj_center(btn_action_label);
}

static void buildWifiTab(lv_obj_t *tab) {
  lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(tab, 4, 0);

  // Etat de la connexion actuelle
  lbl_wifi_status = lv_label_create(tab);
  lv_obj_set_style_text_font(lbl_wifi_status, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(lbl_wifi_status, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_wifi_status, 230);
  lv_label_set_text(lbl_wifi_status, "WiFi...");
  lv_obj_align(lbl_wifi_status, LV_ALIGN_TOP_LEFT, 0, 0);

  // Dropdown des reseaux
  dd_networks = lv_dropdown_create(tab);
  lv_obj_set_width(dd_networks, 155);
  lv_dropdown_set_options(dd_networks, "---");
  lv_obj_align(dd_networks, LV_ALIGN_TOP_LEFT, 0, 72);

  // Bouton Scan
  lv_obj_t *btn_scan = lv_btn_create(tab);
  lv_obj_set_size(btn_scan, 68, 32);
  lv_obj_align(btn_scan, LV_ALIGN_TOP_LEFT, 159, 72);
  lv_obj_add_event_cb(btn_scan, cb_wifi_scan, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ls = lv_label_create(btn_scan);
  lv_label_set_text(ls, "Scan");
  lv_obj_center(ls);

  // Statut du scan
  lbl_scan_st = lv_label_create(tab);
  lv_obj_set_style_text_font(lbl_scan_st, &lv_font_montserrat_14, 0);
  lv_label_set_text(lbl_scan_st, "");
  lv_obj_align(lbl_scan_st, LV_ALIGN_TOP_LEFT, 0, 110);

  // Bouton Connecter
  lv_obj_t *btn_conn = lv_btn_create(tab);
  lv_obj_set_size(btn_conn, 108, 34);
  lv_obj_align(btn_conn, LV_ALIGN_BOTTOM_LEFT, 0, -2);
  lv_obj_add_event_cb(btn_conn, cb_wifi_connect, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lc = lv_label_create(btn_conn);
  lv_label_set_text(lc, "Connecter");
  lv_obj_center(lc);

  // Bouton Deconnecter
  lv_obj_t *btn_disc = lv_btn_create(tab);
  lv_obj_set_size(btn_disc, 116, 34);
  lv_obj_set_style_bg_color(btn_disc, lv_palette_main(LV_PALETTE_RED), 0);
  lv_obj_align(btn_disc, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
  lv_obj_add_event_cb(btn_disc, cb_wifi_disconnect, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ld = lv_label_create(btn_disc);
  lv_label_set_text(ld, "Deconnecter");
  lv_obj_center(ld);
}

static void buildWifiOverlay();  // declaration anticipee (definie apres buildUi)

static void buildUi() {
  tabview = lv_tabview_create(lv_scr_act(), LV_DIR_LEFT, 74);

  buildHomeTab(lv_tabview_add_tab(tabview, LV_SYMBOL_HOME "\nHome"));
  buildPumpTab(lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS "\nPompe"));
  buildWifiTab(lv_tabview_add_tab(tabview, LV_SYMBOL_WIFI "\nWiFi"));

  lv_obj_add_event_cb(tabview, cb_tab_changed, LV_EVENT_VALUE_CHANGED, NULL);
  lv_tabview_set_act(tabview, TAB_HOME, LV_ANIM_OFF);
  buildWifiOverlay();
}

static void buildWifiOverlay() {
  wifi_overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(wifi_overlay, 320, 240);
  lv_obj_set_pos(wifi_overlay, 0, 0);
  lv_obj_set_style_bg_color(wifi_overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(wifi_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_width(wifi_overlay, 0, 0);
  lv_obj_set_style_pad_all(wifi_overlay, 0, 0);
  lv_obj_clear_flag(wifi_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(wifi_overlay, LV_OBJ_FLAG_HIDDEN);

  // Clavier en bas (145 px de haut)
  lv_kbd = lv_keyboard_create(wifi_overlay);
  lv_obj_set_size(lv_kbd, 320, 145);
  lv_obj_align(lv_kbd, LV_ALIGN_BOTTOM_MID, 0, 0);
  // Le clavier OK (coche) valide aussi la connexion
  lv_obj_add_event_cb(lv_kbd, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_READY)   cb_ov_ok(NULL);
    if (lv_event_get_code(e) == LV_EVENT_CANCEL)  cb_ov_cancel(NULL);
  }, LV_EVENT_ALL, NULL);

  // SSID selectionne
  lbl_ov_ssid = lv_label_create(wifi_overlay);
  lv_obj_set_style_text_font(lbl_ov_ssid, &lv_font_montserrat_14, 0);
  lv_label_set_text(lbl_ov_ssid, "SSID : ...");
  lv_obj_set_pos(lbl_ov_ssid, 6, 4);

  // Champ mot de passe (mode masque)
  ta_pwd = lv_textarea_create(wifi_overlay);
  lv_textarea_set_one_line(ta_pwd, true);
  lv_textarea_set_password_mode(ta_pwd, true);
  lv_textarea_set_placeholder_text(ta_pwd, "Mot de passe");
  lv_obj_set_size(ta_pwd, 312, 34);
  lv_obj_set_pos(ta_pwd, 4, 24);

  // Bouton OK
  lv_obj_t *btn_ok = lv_btn_create(wifi_overlay);
  lv_obj_set_size(btn_ok, 150, 30);
  lv_obj_set_pos(btn_ok, 4, 62);
  lv_obj_add_event_cb(btn_ok, cb_ov_ok, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lok = lv_label_create(btn_ok);
  lv_label_set_text(lok, "OK");
  lv_obj_center(lok);

  // Bouton Annuler
  lv_obj_t *btn_ann = lv_btn_create(wifi_overlay);
  lv_obj_set_size(btn_ann, 150, 30);
  lv_obj_set_pos(btn_ann, 162, 62);
  lv_obj_set_style_bg_color(btn_ann, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_add_event_cb(btn_ann, cb_ov_cancel, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lann = lv_label_create(btn_ann);
  lv_label_set_text(lann, "Annuler");
  lv_obj_center(lann);
}

// ======================= API =======================
void ui_begin() {
  tft.init();
  tft.setRotation(1);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Tactile (HSPI) -> il FAUT passer touchSPI a begin()
  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 10);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_WIDTH;
  disp_drv.ver_res = SCREEN_HEIGHT;
  disp_drv.flush_cb = disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_read;
  lv_indev_drv_register(&indev_drv);

  buildUi();
}

void ui_tick() {
  static uint32_t last = 0;
  if (millis() - last > 250) {
    last = millis();
    switch (lv_tabview_get_tab_act(tabview)) {
      case TAB_PUMP:
        if (doser_calibState() == CAL_RUNNING) calibRefresh();  // chrono en direct
        break;
      case TAB_WIFI:
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
            char opts[512] = "";
            for (int i = 0; i < cnt && i < 12; i++) {
              char ssid[64];
              net_scanGetSSID(i, ssid, sizeof(ssid));
              Serial.printf("[SCAN]   %d: %s (%d dBm)\n", i, ssid, net_scanGetRSSI(i));
              if (i > 0) strncat(opts, "\n", sizeof(opts) - strlen(opts) - 1);
              strncat(opts, ssid, sizeof(opts) - strlen(opts) - 1);
            }
            lv_dropdown_set_options(dd_networks, cnt > 0 ? opts : "---");
            char st[24];
            snprintf(st, sizeof(st), cnt > 0 ? "%d reseaux" : "Aucun reseau", cnt);
            lv_label_set_text(lbl_scan_st, st);
          } else if (cnt == -2 && millis() - scan_start_ms > 5000) {
            last_cnt = -99;
            scan_triggered = false;
            lv_label_set_text(lbl_scan_st, "Echec - reessayez");
            Serial.println("[SCAN] echec confirme apres 5s");
          }
        }
        break;
      default:
        refreshNext();
        break;
    }
    refreshStatus();
  }
  lv_timer_handler();
}
