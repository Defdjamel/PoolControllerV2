//
// Ecran "Debit" : reglage du dosage cible (ml/h) avec application immediate.
// Pour menager la NVS, l'ecriture est differee (anti-rebond) apres la
// derniere action utilisateur.
//
#include "ui/ui_internal.h"

static lv_obj_t *flow_val, *flow_bar;
static int       flowVal = 0;

// ======================= Rafraichissement =======================
void refreshFlow() {
  lv_label_set_text_fmt(flow_val, "%d", flowVal);
  lv_bar_set_value(flow_bar, flowVal, LV_ANIM_OFF);
}

void flowSetValue(float v) {
  flowVal = (int)(v + 0.5f);
  if (flow_val) refreshFlow();
}

void flowEnter() {
  flowVal = (int)(doser_getDosage() + 0.5f);
  refreshFlow();
}

// ======================= Commit differe (anti-rebond) =======================
static lv_timer_t *flow_commit_timer = NULL;
static void flow_commit_cb(lv_timer_t *) {
  if ((int)(doser_getDosage() + 0.5f) != flowVal) {
    doser_setDosage((float)flowVal);
    net_publishDosage((float)flowVal);
  }
  flow_commit_timer = NULL;
}
static void scheduleFlowCommit() {
  if (flow_commit_timer) lv_timer_del(flow_commit_timer);
  flow_commit_timer = lv_timer_create(flow_commit_cb, 600, NULL);
  lv_timer_set_repeat_count(flow_commit_timer, 1);
}

// ======================= Evenements =======================
static void cb_flow_bump(lv_event_t *e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  flowVal = constrain(flowVal + d, 0, DOSAGE_MAX_ML_H);
  refreshFlow();
  scheduleFlowCommit();
}
static void cb_flow_preset(lv_event_t *e) {
  flowVal = (int)(intptr_t)lv_event_get_user_data(e);
  refreshFlow();
  scheduleFlowCommit();
}

// ======================= Construction =======================
void buildFlow(lv_obj_t *p) {
  lv_obj_t *ttl = mkLabel(p, "REGLAGE DU DEBIT", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 8, 4);

  lv_obj_t *minus = mkRound(p, "-", cb_flow_bump, -FLOW_STEP);
  lv_obj_align(minus, LV_ALIGN_LEFT_MID, 8, -10);
  lv_obj_t *plus = mkRound(p, "+", cb_flow_bump, FLOW_STEP);
  lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -8, -10);

  flow_val = mkLabel(p, "0", &lv_font_montserrat_28, C_ACCENT);
  lv_obj_align(flow_val, LV_ALIGN_CENTER, 0, -16);
  lv_obj_t *u = mkLabel(p, "ml / heure", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(u, LV_ALIGN_CENTER, 0, 6);

  flow_bar = lv_bar_create(p);
  lv_obj_set_size(flow_bar, PW - 30, 6);
  lv_obj_align(flow_bar, LV_ALIGN_CENTER, 0, 28);
  lv_obj_set_style_bg_color(flow_bar, C_SURFACE2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(flow_bar, C_ACCENT, LV_PART_INDICATOR);
  lv_bar_set_range(flow_bar, 0, DOSAGE_MAX_ML_H);

  // Preselections (<= max)
  int presets[] = {60, 120, 180};
  int px = 8;
  for (int i = 0; i < 3; i++) {
    if (presets[i] > DOSAGE_MAX_ML_H) continue;
    char t[8]; snprintf(t, sizeof(t), "%d", presets[i]);
    lv_obj_t *pb = mkBtn(p, t, C_SURFACE2, C_TEXT, NULL);
    lv_obj_set_size(pb, 52, 24);
    lv_obj_align(pb, LV_ALIGN_BOTTOM_LEFT, px, -2);
    lv_obj_add_event_cb(pb, cb_flow_preset, LV_EVENT_CLICKED, (void *)(intptr_t)presets[i]);
    px += 58;
  }
}
