#include <Arduino.h>
#include <time.h>
#include <Preferences.h>
#include "core/doser.h"
#include "hardware/relay.h"
#include "ui/ui.h"
#include "net/net_blynk.h"
#include "config.h"

static Preferences prefs;

// Etat
static float dosage   = 0.0f;   // ml/h
static float flowRate = 0.0f;   // ml/s (0 = non calibre)

static float volumeToday    = 0.0f;
static float volumeTotal    = 0.0f;
static float lastDoseVolume = 0.0f;
static int   currentYday    = -1;
static float histDays[6]    = {0};  // 6 journees terminees (h0=plus ancienne)

static bool     pumpRunning = false;
static uint32_t pumpStartMs = 0;
static uint32_t pumpRunMs   = 0;
static uint32_t lastDoseMs  = 0;

static CalibState calibState      = CAL_READY;
static bool       calibrationMode = false;
static uint32_t   calibStartMs    = 0;

// --- Helpers internes ---
static void setPump(bool on) {
  relay_set(on);
}

static void registerInjection(float ml) {
  if (ml <= 0.0f) return;
  lastDoseVolume = ml;
  volumeToday   += ml;
  volumeTotal   += ml;
  prefs.putFloat("volDay", volumeToday);
  prefs.putFloat("volTot", volumeTotal);
  ui_setToday(volumeToday);
  net_publishVolumes(lastDoseVolume, volumeToday, volumeTotal);
  Serial.printf("[Inj] %.2f ml (jour %.1f, total %.1f)\n", ml, volumeToday, volumeTotal);
}

static void startDose() {
  if (flowRate <= 0.0f || dosage <= 0.0f) return;
  float mlCycle = dosage * (DOSE_INTERVAL_MS / 3600000.0f);  // ml a injecter ce cycle
  float secs    = mlCycle / flowRate;
  pumpRunMs = (uint32_t)(secs * 1000.0f + 0.5f);
  if (pumpRunMs == 0) return;
  setPump(true);
  pumpRunning = true;
  pumpStartMs = millis();
  Serial.printf("[Dose] %.0f ml/h -> %.2f ml -> pompe %u ms\n", dosage, mlCycle, pumpRunMs);
}

static void checkDailyReset() {
  time_t now = time(nullptr);
  if (now < 1700000000) return;  // heure pas encore synchronisee (NTP)
  struct tm t;
  localtime_r(&now, &t);
  if (currentYday == -1) {
    currentYday = t.tm_yday;
    prefs.putInt("yday", currentYday);
    return;
  }
  if (t.tm_yday != currentYday) {
    // Archiver la journee ecoulee dans l'historique (decalage a gauche)
    for (int i = 0; i < 5; i++) histDays[i] = histDays[i + 1];
    histDays[5] = volumeToday;
    for (int i = 0; i < 6; i++) {
      char k[4]; snprintf(k, sizeof(k), "h%d", i);
      prefs.putFloat(k, histDays[i]);
    }
    currentYday = t.tm_yday;
    volumeToday = 0.0f;
    prefs.putFloat("volDay", 0.0f);
    prefs.putInt("yday", currentYday);
    ui_setToday(0.0f);
    net_publishVolumeDay(0.0f);
    Serial.println("[Jour] Nouvelle journee, volume du jour remis a zero.");
  }
}

// --- API ---
void doser_begin() {
  prefs.begin("pool", false);
  flowRate    = prefs.getFloat("flow", 0.0f);
  dosage      = prefs.getFloat("dosage", 0.0f);
  volumeToday = prefs.getFloat("volDay", 0.0f);
  volumeTotal = prefs.getFloat("volTot", 0.0f);
  currentYday = prefs.getInt("yday", -1);
  for (int i = 0; i < 6; i++) {
    char k[4]; snprintf(k, sizeof(k), "h%d", i);
    histDays[i] = prefs.getFloat(k, 0.0f);
  }
  lastDoseMs  = millis();  // premier cycle dans DOSE_INTERVAL_MS

  ui_setDosage(dosage);
  ui_setFlow(flowRate);
  ui_setToday(volumeToday);
}

void doser_loop() {
  if (!calibrationMode) {
    if (!pumpRunning && (millis() - lastDoseMs >= DOSE_INTERVAL_MS)) {
      lastDoseMs = millis();
      startDose();
    }
    if (pumpRunning && (millis() - pumpStartMs >= pumpRunMs)) {
      setPump(false);
      pumpRunning = false;
      registerInjection(flowRate * (pumpRunMs / 1000.0f));
    }
  }
  checkDailyReset();
}

void doser_setDosage(float mlPerHour) {
  dosage = mlPerHour;
  prefs.putFloat("dosage", dosage);
  ui_setDosage(dosage);
}

float doser_getDosage() { return dosage; }
float doser_getFlow()   { return flowRate; }
bool  doser_isPumping() { return pumpRunning; }

int doser_secondsToNext() {
  long rem = (long)DOSE_INTERVAL_MS - (long)(millis() - lastDoseMs);
  if (rem < 0) rem = 0;
  return (int)(rem / 1000);
}

void doser_triggerNow() {
  if (!calibrationMode && !pumpRunning) {
    lastDoseMs = millis();
    startDose();
  }
}

float doser_getVolumeToday() { return volumeToday; }
float doser_getVolumeTotal() { return volumeTotal; }
float doser_getLastVolume()  { return lastDoseVolume; }

void doser_getWeek(float out[7]) {
  for (int i = 0; i < 6; i++) out[i] = histDays[i];
  out[6] = volumeToday;
}

void doser_calibEnter() {
  pumpRunning = false;
  setPump(false);
  calibrationMode = true;
  calibState = CAL_READY;
}

void doser_calibAction() {
  switch (calibState) {
    case CAL_READY:
      setPump(true);
      calibStartMs = millis();
      calibState = CAL_RUNNING;
      break;

    case CAL_RUNNING: {
      setPump(false);
      float elapsed = (millis() - calibStartMs) / 1000.0f;
      if (elapsed < 0.2f) elapsed = 0.2f;  // garde-fou anti division par ~0
      flowRate = CALIB_VOLUME_ML / elapsed;
      prefs.putFloat("flow", flowRate);
      calibState = CAL_DONE;
      ui_setFlow(flowRate);
      Serial.printf("[Calib] %.1f s -> %.2f ml/s\n", elapsed, flowRate);
      break;
    }

    case CAL_DONE:
      calibState = CAL_READY;
      break;
  }
}

void doser_calibBack() {
  setPump(false);
  pumpRunning = false;
  calibState = CAL_READY;
  calibrationMode = false;
  lastDoseMs = millis();  // repart sur un cycle propre
}

CalibState doser_calibState()  { return calibState; }
float      doser_calibElapsed() { return (millis() - calibStartMs) / 1000.0f; }
bool       doser_inCalibration() { return calibrationMode; }
