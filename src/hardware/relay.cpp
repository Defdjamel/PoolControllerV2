#include <Arduino.h>
#include "hardware/relay.h"
#include "config.h"

void relay_begin() {
  pinMode(PIN_RELAY, OUTPUT);
  relay_set(false);  // pompe coupee au demarrage
}

void relay_set(bool on) {
  bool level = RELAY_ACTIVE_HIGH ? on : !on;
  digitalWrite(PIN_RELAY, level ? HIGH : LOW);
}
