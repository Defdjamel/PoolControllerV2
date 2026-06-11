#pragma once

// Pilotage du relais de la pompe (gere le niveau actif via config.h).
void relay_begin();
void relay_set(bool on);
