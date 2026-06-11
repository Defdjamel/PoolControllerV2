//
// Coeur de l'UI : pilotes ecran/tactile LVGL, barre de statut, barre
// d'onglets, navigation, overlay OTA et cycle de vie (ui_begin / ui_tick).
// Le contenu de chaque ecran vit dans son propre module (ui_home, ui_flow,
// ui_settings, ui_hist) ; les helpers de style dans ui_widgets.
//
#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "ui/ui_internal.h"

// ======================= Pilotes ecran / tactile =======================
static TFT_eSPI tft;
static SPIClass touchSPI(HSPI);
static XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 10];

// ======================= Objets du cadre =======================
static lv_obj_t *content;
static lv_obj_t *pages[N_TABS];
static int       curTab = T_HOME;
// Barre de statut
static lv_obj_t *sb_dot, *sb_state, *sb_wifi, *sb_clock;
// Barre d'onglets
static lv_obj_t *tabIcon[N_TABS], *tabText[N_TABS], *tabInd[N_TABS];
// Overlay OTA
static lv_obj_t *ota_overlay, *lbl_ota_ver, *bar_ota, *lbl_ota_pct, *lbl_ota_err;

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

#define TOUCH_DEBUG 0   // passer a 1 pour afficher les coordonnees brutes (calibration)

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int16_t x = map(p.x, TS_MIN_X, TS_MAX_X, 0, SCREEN_WIDTH);
    int16_t y = map(p.y, TS_MIN_Y, TS_MAX_Y, 0, SCREEN_HEIGHT);
#if TOUCH_DEBUG
    Serial.printf("[Touch] brut x=%d y=%d -> %d,%d\n", p.x, p.y, x, y);
#endif
    data->state = LV_INDEV_STATE_PR;
    data->point.x = constrain(x, 0, SCREEN_WIDTH - 1);
    data->point.y = constrain(y, 0, SCREEN_HEIGHT - 1);
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ======================= Barre de statut =======================
static void refreshStatus() {
  bool pumping = doser_isPumping();
  lv_obj_set_style_bg_color(sb_dot, pumping ? C_GREEN : C_MUTED, 0);
  lv_label_set_text(sb_state, pumping ? "DOSAGE" : "VEILLE");

  int s = net_status();
  lv_obj_set_style_text_color(sb_wifi, s >= 1 ? C_ACCENT : C_MUTED, 0);

  time_t now = time(nullptr);
  if (now > 1700000000) {
    struct tm t; localtime_r(&now, &t);
    lv_label_set_text_fmt(sb_clock, "%02d:%02d", t.tm_hour, t.tm_min);
  } else {
    lv_label_set_text(sb_clock, "--:--");
  }
}

static void buildStatusBar(lv_obj_t *scr) {
  lv_obj_t *sb = lv_obj_create(scr);
  lv_obj_set_size(sb, PW, SB_H);
  lv_obj_set_pos(sb, 0, 0);
  lv_obj_set_style_bg_color(sb, C_STATUS, 0);
  lv_obj_set_style_border_width(sb, 0, 0);
  lv_obj_set_style_radius(sb, 0, 0);
  lv_obj_set_style_pad_all(sb, 0, 0);
  lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);

  sb_dot = lv_obj_create(sb);
  lv_obj_set_size(sb_dot, 6, 6);
  lv_obj_set_style_radius(sb_dot, 3, 0);
  lv_obj_set_style_border_width(sb_dot, 0, 0);
  lv_obj_set_style_bg_color(sb_dot, C_MUTED, 0);
  lv_obj_align(sb_dot, LV_ALIGN_LEFT_MID, 8, 0);
  sb_state = mkLabel(sb, "VEILLE", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(sb_state, LV_ALIGN_LEFT_MID, 20, 0);

  sb_clock = mkLabel(sb, "--:--", &lv_font_montserrat_12, C_TEXT);
  lv_obj_align(sb_clock, LV_ALIGN_RIGHT_MID, -8, 0);
  sb_wifi = mkLabel(sb, LV_SYMBOL_WIFI, &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(sb_wifi, LV_ALIGN_RIGHT_MID, -46, 0);

  // Version du firmware (fixe : ne change pas a l'execution)
  lv_obj_t *sb_ver = mkLabel(sb, "v" FIRMWARE_VERSION, &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(sb_ver, LV_ALIGN_CENTER, 0, 0);
}

// ======================= Navigation =======================
void setTab(int i) {
  for (int k = 0; k < N_TABS; k++) {
    bool act = (k == i);
    lv_obj_set_style_text_color(tabIcon[k], act ? C_ACCENT : C_MUTED, 0);
    lv_obj_set_style_text_color(tabText[k], act ? C_ACCENT : C_MUTED, 0);
    lv_obj_set_width(tabInd[k], act ? 22 : 0);
    if (act) lv_obj_clear_flag(pages[k], LV_OBJ_FLAG_HIDDEN);
    else     lv_obj_add_flag(pages[k], LV_OBJ_FLAG_HIDDEN);
  }
  curTab = i;

  if (i == T_FLOW) flowEnter();
  if (i == T_HIST) refreshHist();
  if (i == T_HOME) refreshHome();
  syncSettings(curTab == T_SETTINGS);   // calibration / scan WiFi selon Reglages
}

static void cb_tab(lv_event_t *e) { setTab((int)(intptr_t)lv_event_get_user_data(e)); }

static void buildTabBar(lv_obj_t *scr) {
  static const char *icons[N_TABS] = {
    LV_SYMBOL_HOME, LV_SYMBOL_TINT, LV_SYMBOL_SETTINGS, LV_SYMBOL_LIST
  };
  static const char *texts[N_TABS] = { "Accueil", "Debit", "Reglages", "Histo" };

  lv_obj_t *tb = lv_obj_create(scr);
  lv_obj_set_size(tb, PW, TB_H);
  lv_obj_set_pos(tb, 0, SCREEN_HEIGHT - TB_H);
  lv_obj_set_style_bg_color(tb, C_STATUS, 0);
  lv_obj_set_style_border_color(tb, lv_color_hex(0x0e2236), 0);
  lv_obj_set_style_border_width(tb, 1, 0);
  lv_obj_set_style_border_side(tb, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_radius(tb, 0, 0);
  lv_obj_set_style_pad_all(tb, 0, 0);
  lv_obj_clear_flag(tb, LV_OBJ_FLAG_SCROLLABLE);

  int tw = PW / N_TABS;
  for (int i = 0; i < N_TABS; i++) {
    lv_obj_t *t = lv_obj_create(tb);
    lv_obj_set_size(t, tw, TB_H);
    lv_obj_set_pos(t, i * tw, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(t, 0, 0);
    lv_obj_set_style_pad_all(t, 0, 0);
    lv_obj_set_style_radius(t, 0, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(t, cb_tab, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    tabInd[i] = lv_obj_create(t);
    lv_obj_set_size(tabInd[i], 0, 3);
    lv_obj_set_style_radius(tabInd[i], 2, 0);
    lv_obj_set_style_border_width(tabInd[i], 0, 0);
    lv_obj_set_style_bg_color(tabInd[i], C_ACCENT, 0);
    lv_obj_align(tabInd[i], LV_ALIGN_TOP_MID, 0, 0);

    tabIcon[i] = mkLabel(t, icons[i], &lv_font_montserrat_16, C_MUTED);
    lv_obj_align(tabIcon[i], LV_ALIGN_TOP_MID, 0, 7);
    tabText[i] = mkLabel(t, texts[i], &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(tabText[i], LV_ALIGN_BOTTOM_MID, 0, -4);
  }
}

// ======================= Overlay OTA =======================
static void buildOtaOverlay() {
  ota_overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ota_overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(ota_overlay, 0, 0);
  lv_obj_set_style_bg_color(ota_overlay, C_BG, 0);
  lv_obj_set_style_bg_opa(ota_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ota_overlay, 0, 0);
  lv_obj_set_style_radius(ota_overlay, 0, 0);
  lv_obj_clear_flag(ota_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ota_overlay, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *title = mkLabel(ota_overlay, LV_SYMBOL_DOWNLOAD " Mise a jour", &lv_font_montserrat_22, C_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 26);

  lbl_ota_ver = mkLabel(ota_overlay, "", &lv_font_montserrat_16, C_ACCENT);
  lv_obj_align(lbl_ota_ver, LV_ALIGN_TOP_MID, 0, 62);

  bar_ota = lv_bar_create(ota_overlay);
  lv_obj_set_size(bar_ota, 280, 20);
  lv_obj_align(bar_ota, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(bar_ota, C_SURFACE2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_ota, C_ACCENT, LV_PART_INDICATOR);
  lv_bar_set_range(bar_ota, 0, 100);

  lbl_ota_pct = mkLabel(ota_overlay, "0 %", &lv_font_montserrat_16, C_TEXT);
  lv_obj_align(lbl_ota_pct, LV_ALIGN_CENTER, 0, 28);

  lbl_ota_err = mkLabel(ota_overlay, "", &lv_font_montserrat_12, C_RED);
  lv_obj_align(lbl_ota_err, LV_ALIGN_BOTTOM_MID, 0, -34);

  lv_obj_t *warn = mkLabel(ota_overlay, LV_SYMBOL_WARNING " Ne pas couper l'alimentation", &lv_font_montserrat_12, C_AMBER);
  lv_obj_align(warn, LV_ALIGN_BOTTOM_MID, 0, -12);
}

void ui_otaBegin(const char *version) {
  lv_label_set_text_fmt(lbl_ota_ver, "Installation de v%s", version);
  lv_bar_set_value(bar_ota, 0, LV_ANIM_OFF);
  lv_label_set_text(lbl_ota_pct, "0 %");
  lv_label_set_text(lbl_ota_err, "");
  lv_obj_clear_flag(ota_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_timer_handler();
}
void ui_otaProgress(int pct) {
  lv_bar_set_value(bar_ota, pct, LV_ANIM_OFF);
  lv_label_set_text_fmt(lbl_ota_pct, "%d %%", pct);
  lv_timer_handler();
}
void ui_otaError(const char *msg) {
  lv_label_set_text(lbl_ota_err, msg);
  lv_timer_handler();
}

// ======================= Assemblage =======================
static void buildUi() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, C_BG, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  buildStatusBar(scr);

  content = lv_obj_create(scr);
  lv_obj_set_size(content, PW, PH);
  lv_obj_set_pos(content, 0, SB_H);
  lv_obj_set_style_bg_color(content, C_BG, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_radius(content, 0, 0);
  lv_obj_set_style_pad_all(content, 0, 0);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < N_TABS; i++) {
    pages[i] = lv_obj_create(content);
    lv_obj_set_size(pages[i], PW, PH);
    lv_obj_set_pos(pages[i], 0, 0);
    lv_obj_set_style_bg_color(pages[i], C_BG, 0);
    lv_obj_set_style_border_width(pages[i], 0, 0);
    lv_obj_set_style_radius(pages[i], 0, 0);
    lv_obj_set_style_pad_all(pages[i], 0, 0);   // marges gerees par chaque ecran (x=8)
    lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
  }
  buildHome(pages[T_HOME]);
  buildFlow(pages[T_FLOW]);
  buildSettings(pages[T_SETTINGS]);   // construit aussi son overlay mot de passe
  buildHist(pages[T_HIST]);

  buildTabBar(scr);
  buildOtaOverlay();

  setTab(T_HOME);
}

// ======================= API =======================
void ui_begin() {
  tft.init();
  tft.setRotation(0);   // portrait 240x320
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(0);

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
    refreshStatus();
    switch (curTab) {
      case T_HOME:     refreshHome();     break;
      case T_SETTINGS: ui_settingsTick(); break;
    }
  }
  lv_timer_handler();
}
