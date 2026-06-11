// ESP32-2432S028R (« Cheap Yellow Display » / CYD)
// Doseur de chlore asservi : dosage (ml/h) regle via Blynk, pompe pilotee par
// relais toutes les 5 min selon le debit calibre, historique des volumes.
//
// Le code est decoupe en modules :
//   config.h      constantes (broches, durees, virtual pins Blynk)
//   relay.*       pilotage du relais
//   doser.*       ordonnanceur, calibration, volumes, persistance NVS
//   ui.*          LVGL : ecran, tactile, ecrans accueil + calibration
//   net_blynk.*   WiFi + Blynk + NTP

#include <Arduino.h>
#include "hardware/relay.h"
#include "ui/ui.h"
#include "core/doser.h"
#include "net/net_blynk.h"
#include "ota/ota.h"

void setup() {
  Serial.begin(115200);

  relay_begin();   // pompe coupee en premier (securite)
  ui_begin();      // ecran + tactile + ecrans LVGL
  doser_begin();   // charge la config NVS et alimente l'affichage
  net_begin();     // WiFi + Blynk + NTP (non bloquant)

  Serial.println("Demarrage termine.");
}

void loop() {
  net_loop();      // connexion + echanges Blynk
  doser_loop();    // ordonnanceur de dosage + reset quotidien
  ui_tick();       // moteur LVGL + rafraichissements
  ota_loop();      // detection de mise a jour (1x/24h apres connexion WiFi)
  delay(5);
}
