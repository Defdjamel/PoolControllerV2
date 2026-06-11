#pragma once
#include <stddef.h>

// Couche reseau : WiFi + Blynk IoT + NTP. Seul ce module inclut Blynk.
void net_begin();
void net_loop();
int  net_status();   // 0 = pas de WiFi, 1 = WiFi seul, 2 = Blynk connecte

// Infos / actions WiFi (pour l'onglet reseau).
void net_wifiInfo(char *buf, size_t n);
void net_reconnect();
bool net_wifiConnected();
void net_wifiSSID(char *buf, size_t n);
void net_wifiIP(char *buf, size_t n);

// Publications (sans effet si Blynk n'est pas connecte).
void net_publishDosage(float mlPerHour);
void net_publishVolumes(float last, float today, float total);
void net_publishVolumeDay(float today);

// Configuration WiFi persistee en NVS.
void net_connectTo(const char *ssid, const char *pass); // sauvegarde + reconnecte
void net_disconnect();

// Scan WiFi asynchrone.
void net_scanStart();
int  net_scanCount();                                   // -1=en cours, -2=echec, >=0=termine
void net_scanGetSSID(int i, char *buf, size_t n);
int  net_scanGetRSSI(int i);
