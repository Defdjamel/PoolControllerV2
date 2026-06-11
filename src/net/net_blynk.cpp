#include "secrets.h"          // BLYNK_TEMPLATE_ID/NAME/AUTH + WIFI_* (avant Blynk !)
#define BLYNK_PRINT Serial

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <BlynkSimpleEsp32.h>
#include "net/net_blynk.h"
#include "core/doser.h"
#include "config.h"

static char wifiSsid[64];
static char wifiPass[64];

static void loadWifiCreds() {
  Preferences p;
  p.begin("wifi", true);
  strlcpy(wifiSsid, p.getString("ssid", WIFI_SSID).c_str(), sizeof(wifiSsid));
  strlcpy(wifiPass, p.getString("pass", WIFI_PASS).c_str(), sizeof(wifiPass));
  p.end();
}

// --- Reception depuis l'app Blynk ---
BLYNK_WRITE(VPIN_DOSAGE) { doser_setDosage(param.asFloat()); }

BLYNK_WRITE(VPIN_MANUAL) {
  if (param.asInt()) doser_triggerNow();
}

BLYNK_CONNECTED() {
  // (Re)synchronise l'app avec l'etat courant de l'appareil.
  Blynk.virtualWrite(VPIN_DOSAGE, doser_getDosage());
  Blynk.virtualWrite(VPIN_VOL_LAST, doser_getLastVolume());
  Blynk.virtualWrite(VPIN_VOL_DAY, doser_getVolumeToday());
  Blynk.virtualWrite(VPIN_VOL_TOTAL, doser_getVolumeTotal());
}

// --- API ---
void net_begin() {
  loadWifiCreds();
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPass);
  Blynk.config(BLYNK_AUTH_TOKEN);
  configTzTime(TZ_INFO, NTP_SERVER);
}

void net_loop() {
  static uint32_t lastTry = 0;
  if (millis() - lastTry > 5000) {
    lastTry = millis();
    if (WiFi.status() != WL_CONNECTED) {
      // Reconnexion WiFi si deconnecte (ex: apres un scan)
      if (wifiSsid[0] != '\0') WiFi.begin(wifiSsid, wifiPass);
    } else if (!Blynk.connected()) {
      Blynk.connect(1000);
    }
  }
  Blynk.run();
}

int net_status() {
  return Blynk.connected() ? 2 : (WiFi.status() == WL_CONNECTED ? 1 : 0);
}

void net_wifiInfo(char *buf, size_t n) {
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(buf, n, "SSID : %s\nIP : %s\nRSSI : %d dBm\nBlynk : %s",
             WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
             (int)WiFi.RSSI(), Blynk.connected() ? "connecte" : "deconnecte");
  } else {
    snprintf(buf, n, "WiFi : deconnecte\nReconnexion...");
  }
}

void net_reconnect() {
  WiFi.disconnect();
  WiFi.begin(wifiSsid, wifiPass);
}

bool net_wifiConnected() { return WiFi.status() == WL_CONNECTED; }

void net_wifiSSID(char *buf, size_t n) {
  strlcpy(buf, WiFi.SSID().c_str(), n);
}

void net_wifiIP(char *buf, size_t n) {
  strlcpy(buf, WiFi.localIP().toString().c_str(), n);
}

void net_publishDosage(float v) {
  if (Blynk.connected()) Blynk.virtualWrite(VPIN_DOSAGE, v);
}

void net_publishVolumes(float last, float today, float total) {
  if (!Blynk.connected()) return;
  Blynk.virtualWrite(VPIN_VOL_LAST, last);
  Blynk.virtualWrite(VPIN_VOL_DAY, today);
  Blynk.virtualWrite(VPIN_VOL_TOTAL, total);
}

void net_publishVolumeDay(float today) {
  if (Blynk.connected()) Blynk.virtualWrite(VPIN_VOL_DAY, today);
}

void net_connectTo(const char *ssid, const char *pass) {
  Preferences p;
  p.begin("wifi", false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.end();
  strlcpy(wifiSsid, ssid, sizeof(wifiSsid));
  strlcpy(wifiPass, pass, sizeof(wifiPass));
  WiFi.disconnect();
  WiFi.begin(ssid, pass);
}

void net_disconnect() {
  WiFi.disconnect();
}

void net_scanStart() {
  wifi_mode_t mode = WiFi.getMode();
  Serial.printf("[SCAN] mode=%d wifiStatus=%d\n", (int)mode, (int)WiFi.status());
  if (mode == WIFI_OFF) WiFi.mode(WIFI_STA);
  // Interrompre toute tentative de connexion en cours : le driver WiFi
  // refuse de scanner tant qu'il est en train de se connecter.
  WiFi.disconnect(false);   // false = ne pas effacer les credentials NVS
  WiFi.scanDelete();
  int ret = WiFi.scanNetworks(true);
  Serial.printf("[SCAN] scanNetworks(async)=%d\n", ret);
}

int net_scanCount() {
  return WiFi.scanComplete();  // -1=en cours, -2=echec, >=0=nb reseaux
}

void net_scanGetSSID(int i, char *buf, size_t n) {
  strlcpy(buf, WiFi.SSID(i).c_str(), n);
}

int net_scanGetRSSI(int i) {
  return (int)WiFi.RSSI(i);
}
