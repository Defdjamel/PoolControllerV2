#pragma once
#include <stdint.h>

// ======================= MATERIEL (a verifier) =======================
#define PIN_RELAY          27   // ⚠️ GPIO libre du CYD relie au relais (alt: 22)
#define RELAY_ACTIVE_HIGH  1    // 1 = actif a l'etat HAUT ; 0 = actif a l'etat BAS

// ======================= ECRAN / TACTILE (CYD) =======================
static const uint16_t SCREEN_WIDTH  = 240;   // portrait
static const uint16_t SCREEN_HEIGHT = 320;

#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// Plage brute approximative du tactile (a affiner par calibration)
static const uint16_t TS_MIN_X = 200, TS_MAX_X = 3700;
static const uint16_t TS_MIN_Y = 240, TS_MAX_Y = 3800;

// ======================= DOSAGE =======================
static const uint32_t DOSE_INTERVAL_MS = 5UL * 60UL * 1000UL;  // 1 cycle = 5 min
static const float    CALIB_VOLUME_ML  = 100.0f;               // recipient de reference
static const int      DOSAGE_MAX_ML_H  = 200;                  // plage du slider de dosage
static const float    MANUAL_DOSE_ML   = 25.0f;                // volume du bouton "Doser maintenant"

// ======================= CUVE (reservoir de chlore) =======================
static const float TANK_CAPACITY_DEFAULT_L = 20.0f;   // capacite par defaut
static const float TANK_CAPACITY_MIN_L     = 5.0f;
static const float TANK_CAPACITY_MAX_L     = 200.0f;

// ======================= HEURE (NTP) =======================
static const char TZ_INFO[]    = "CET-1CEST,M3.5.0,M10.5.0/3";  // Europe/Paris
static const char NTP_SERVER[] = "pool.ntp.org";

// ======================= OTA =======================
// On interroge l'API "contents" de GitHub (et non raw.githubusercontent.com) :
// le CDN raw sert un cache de ~5 min qui IGNORE le parametre "?t=", alors que
// l'API renvoie toujours le contenu frais (en-tete Accept: application/vnd.github.raw).
#define FIRMWARE_VERSION  "1.0.11"
#define OTA_VERSION_URL   "https://api.github.com/repos/Defdjamel/PoolControllerV2/contents/firmware/version.json"

// ======================= BLYNK : virtual pins =======================
#define VPIN_DOSAGE    V0   // dosage cible ml/h          (app <-> appareil)
#define VPIN_VOL_LAST  V2   // volume derniere injection  (appareil -> app)
#define VPIN_VOL_DAY   V3   // volume injecte aujourd'hui (appareil -> app)
#define VPIN_VOL_TOTAL V4   // volume injecte cumule      (appareil -> app)
#define VPIN_TANK      V5   // pourcentage restant cuve   (appareil -> app)
