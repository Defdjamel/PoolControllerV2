/**
 * lv_conf.h — Configuration LVGL pour le ESP32-2432S028 (CYD)
 *
 * Seules les options qui different des valeurs par defaut de LVGL sont
 * definies ici ; lv_conf_internal.h fournit le reste. Active via le
 * build flag -DLV_CONF_INCLUDE_SIMPLE dans platformio.ini.
 *
 * Cible : LVGL 8.x
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
 *   COULEURS
 *====================*/
/* Profondeur 16 bits (RGB565), comme l'ecran ILI9341. */
#define LV_COLOR_DEPTH 16
/* Pas de swap cote LVGL : c'est TFT_eSPI.pushColors(..., true) qui swap. */
#define LV_COLOR_16_SWAP 0

/*====================
 *   MEMOIRE
 *====================*/
/* Allocateur interne de LVGL (suffisant ici). */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)

/*========================
 *   HAL / TICK
 *========================*/
#define LV_DPI_DEF 130
/* La base de temps de LVGL utilise millis() d'Arduino. */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/*========================
 *   DEBUG / LOGS
 *========================*/
#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/*========================
 *   POLICES
 *========================*/
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*========================
 *   AUTRES
 *========================*/
/* Support des flottants (%f) dans lv_snprintf / lv_label_set_text_fmt. */
#define LV_SPRINTF_USE_FLOAT 1

#endif /* LV_CONF_H */
