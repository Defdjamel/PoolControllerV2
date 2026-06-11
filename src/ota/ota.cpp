#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "ota/ota.h"
#include "ui/ui.h"
#include "config.h"

// Extrait la valeur d'une cle dans un JSON plat, avec ou sans espaces autour de ':'
static bool jsonGet(const String &body, const char *key, char *out, size_t n) {
    String pat = String('"') + key + '"';
    int pos = body.indexOf(pat);
    if (pos < 0) return false;
    pos += pat.length();
    // sauter espaces et ':'
    while (pos < (int)body.length() && (body[pos] == ' ' || body[pos] == ':')) pos++;
    // doit tomber sur le '"' ouvrant de la valeur
    if (pos >= (int)body.length() || body[pos] != '"') return false;
    pos++;
    int end = body.indexOf('"', pos);
    if (end < 0) return false;
    strlcpy(out, body.substring(pos, end).c_str(), n);
    return true;
}

// Convertit "major.minor.patch" en entier comparable
static int semver(const char *v) {
    int a = 0, b = 0, c = 0;
    sscanf(v, "%d.%d.%d", &a, &b, &c);
    return a * 10000 + b * 100 + c;
}

void ota_check() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.printf("[OTA] verification (local=%s)...\n", FIRMWARE_VERSION);
    WiFiClientSecure client;
    client.setInsecure();   // HTTPS sans validation du cert (RAM limitee)

    HTTPClient http;
    if (!http.begin(client, OTA_VERSION_URL)) {
        Serial.println("[OTA] URL invalide");
        return;
    }
    // API GitHub "contents" : "Accept: raw" renvoie le fichier brut, toujours
    // frais. Un User-Agent est requis par l'API. (Les en-tetes doivent etre
    // ajoutes APRES begin().)
    http.addHeader("Accept", "application/vnd.github.raw");
    http.addHeader("User-Agent", "PoolControllerV2-OTA");
    int code = http.GET();
    String body = http.getString();
    http.end();

    if (code != 200) {
        Serial.printf("[OTA] HTTP %d\n", code);
        return;
    }

    char remoteVer[16] = "", firmwareUrl[256] = "";
    if (!jsonGet(body, "version", remoteVer, sizeof(remoteVer)) ||
        !jsonGet(body, "url",     firmwareUrl, sizeof(firmwareUrl))) {
        Serial.printf("[OTA] version.json invalide : %s\n", body.c_str());
        return;
    }

    Serial.printf("[OTA] distant=%s\n", remoteVer);
    if (semver(remoteVer) <= semver(FIRMWARE_VERSION)) {
        Serial.println("[OTA] firmware a jour");
        return;
    }

    Serial.printf("[OTA] mise a jour vers %s ...\n", remoteVer);

    // Afficher l'ecran de mise a jour
    String ver(remoteVer);
    ui_otaBegin(ver.c_str());

    // Callbacks pour la barre de progression
    httpUpdate.onProgress([](int cur, int total) {
        if (total > 0) ui_otaProgress(cur * 100 / total);
    });

    WiFiClientSecure fwClient;
    fwClient.setInsecure();

    httpUpdate.rebootOnUpdate(true);
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    t_httpUpdate_return ret = httpUpdate.update(fwClient, firmwareUrl);
    // En cas de succes, httpUpdate redemarre automatiquement — on n'arrive pas ici.
    if (ret == HTTP_UPDATE_FAILED) {
        String err = httpUpdate.getLastErrorString();
        Serial.printf("[OTA] echec : %s\n", err.c_str());
        ui_otaError(err.c_str());
    }
}

void ota_loop() {
    static uint32_t last  = 0;
    static bool     first = true;
    if (WiFi.status() != WL_CONNECTED) { first = true; return; }
    // Premier appel apres connexion, puis toutes les 24 h
    if (first || millis() - last > 24UL * 3600UL * 1000UL) {
        first = false;
        last  = millis();
        ota_check();
    }
}
