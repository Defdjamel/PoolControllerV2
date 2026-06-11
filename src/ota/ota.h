#pragma once

void ota_loop();   // a appeler dans loop() — verifie au 1er WiFi puis toutes les 24h
void ota_check();  // check immediat et bloquant (~2 s)
