#pragma once

// Interface LVGL : ecran, tactile, ecrans accueil + calibration.
void ui_begin();   // init LVGL + ecran + tactile + creation des ecrans
void ui_tick();    // lv_timer_handler() + rafraichissements (a appeler dans loop())

// Mise a jour de l'affichage, appelee par le doser.
void ui_setDosage(float mlPerHour);
void ui_setFlow(float mlPerSec);
void ui_setToday(float ml);
