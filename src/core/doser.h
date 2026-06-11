#pragma once

// Coeur metier du doseur : ordonnanceur de dosage, calibration de la pompe,
// historique des volumes injectes, persistance NVS.

enum CalibState { CAL_READY, CAL_RUNNING, CAL_DONE };

void doser_begin();   // charge la config NVS et alimente l'UI
void doser_loop();    // ordonnanceur + reset quotidien (a appeler dans loop())

// --- Reglages ---
void  doser_setDosage(float mlPerHour);   // appele par Blynk
float doser_getDosage();
float doser_getFlow();
bool  doser_isPumping();
int   doser_secondsToNext();
void  doser_triggerNow();                  // injection manuelle d'un volume fixe (MANUAL_DOSE_ML)

// --- Historique des volumes ---
float doser_getVolumeToday();
float doser_getVolumeTotal();
float doser_getLastVolume();
void  doser_getWeek(float out[7]);  // out[0]=plus ancien ... out[6]=aujourd'hui

// --- Cuve (reservoir de chlore) ---
float doser_getTankPercent();          // % restant (0-100), derive de la conso depuis le remplissage
float doser_getTankCapacityL();        // capacite configuree, en litres
void  doser_setTankCapacityL(float litres);  // borne a [MIN, MAX], persiste
void  doser_refillTank();              // marque la cuve comme pleine (conso = 0)

// --- Calibration (pilotee par l'UI) ---
void       doser_calibEnter();   // entre en mode calibration (stoppe le dosage)
void       doser_calibAction();  // READY -> RUNNING -> DONE -> READY
void       doser_calibBack();     // sort du mode calibration (relais coupe)
CalibState doser_calibState();
float      doser_calibElapsed();  // secondes ecoulees (chrono en direct)
bool       doser_inCalibration();
