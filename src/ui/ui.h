#pragma once

// Interface LVGL : ecran, tactile, ecrans accueil + calibration.
void ui_begin();   // init LVGL + ecran + tactile + creation des ecrans
void ui_tick();    // lv_timer_handler() + rafraichissements (a appeler dans loop())

// Mise a jour de l'affichage, appelee par le doser.
void ui_setDosage(float mlPerHour);
void ui_setFlow(float mlPerSec);
void ui_setToday(float ml);

// Ecran de mise a jour OTA.
void ui_otaBegin(const char *version);   // affiche l'overlay
void ui_otaProgress(int pct);            // met a jour la barre (0-100)
void ui_otaError(const char *msg);       // affiche un message d'erreur
