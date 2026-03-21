#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include "config.h"

// Resultado de leer el botón PULSE
enum PulseAction : uint8_t {
  PULSE_NONE,
  PULSE_SHORT,    // press corto → cicla modo
  PULSE_LONG      // press largo ≥ 1.5 s → confirma
};

class ButtonInput {
public:
  void begin(uint8_t pin);
  PulseAction update();   // llamar cada ciclo de loop()

private:
  uint8_t  _pin = 0;
  bool     _pressed = false;
  uint32_t _pressStart = 0;
  bool     _longFired = false;
};

#endif
