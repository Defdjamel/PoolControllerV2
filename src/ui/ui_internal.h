#pragma once
//
// En-tete interne partage par les modules de l'UI (ui_*.cpp).
// Regroupe le theme, la geometrie, les helpers de style et les prototypes
// echanges entre ecrans. NE PAS inclure depuis le reste du projet : seul
// ui.h expose l'API publique.
//
#include <Arduino.h>
#include <time.h>
#include <lvgl.h>
#include "ui/ui.h"
#include "core/doser.h"
#include "net/net_blynk.h"
#include "config.h"

// ======================= Palette (design "Pompe Piscine") =======================
#define C_BG       lv_color_hex(0x06101f)
#define C_STATUS   lv_color_hex(0x081320)
#define C_SURFACE  lv_color_hex(0x0e2740)
#define C_SURFACE2 lv_color_hex(0x123459)
#define C_LINE     lv_color_hex(0x1d3f63)
#define C_ACCENT   lv_color_hex(0x22d3ee)
#define C_ACCENT_D lv_color_hex(0x0b6e85)
#define C_TEXT     lv_color_hex(0xe8f2f7)
#define C_MUTED    lv_color_hex(0x6f90aa)
#define C_GREEN    lv_color_hex(0x34d399)
#define C_AMBER    lv_color_hex(0xf5a623)
#define C_RED      lv_color_hex(0xf87171)

// ======================= Geometrie (portrait 240x320) =======================
static const int SB_H = 22;                            // barre de statut
static const int TB_H = 44;                            // barre d'onglets
static const int PW   = SCREEN_WIDTH;                  // 240
static const int PH   = SCREEN_HEIGHT - SB_H - TB_H;   // 254

enum { T_HOME = 0, T_FLOW, T_SETTINGS, T_HIST, N_TABS };
enum { SUB_WIFI = 0, SUB_PUMP };
static const int FLOW_STEP = 5;

// ======================= Helpers de style (ui_widgets.cpp) =======================
lv_obj_t *mkLabel(lv_obj_t *par, const char *txt, const lv_font_t *font, lv_color_t col);
lv_obj_t *mkCard(lv_obj_t *par, int x, int y, int w, int h);
lv_obj_t *mkBtn(lv_obj_t *par, const char *txt, lv_color_t bg, lv_color_t fg, lv_obj_t **lblOut);
lv_obj_t *mkRound(lv_obj_t *par, const char *sym, lv_event_cb_t cb, int delta);
void      showToast(const char *msg);

// ======================= Navigation (ui.cpp) =======================
void setTab(int i);

// ======================= Ecrans =======================
// Accueil (ui_home.cpp)
void buildHome(lv_obj_t *p);
void refreshHome();
// Debit (ui_flow.cpp)
void buildFlow(lv_obj_t *p);
void refreshFlow();
void flowSetValue(float v);   // pousse une valeur externe dans le compteur
void flowEnter();             // resynchronise depuis le doseur a l'arrivee
// Reglages : WiFi + Pompe (ui_settings.cpp)
void buildSettings(lv_obj_t *p);
void refreshWifi();
void calibRefresh();
void syncSettings(bool inSettings);   // prise de main relais / scan selon le sous-onglet
void ui_settingsTick();               // rafraichissement periodique (calib + scan)
// Historique (ui_hist.cpp)
void buildHist(lv_obj_t *p);
void refreshHist();
