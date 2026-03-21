#include "ButtonInput.h"

void ButtonInput::begin(uint8_t pin) {
  _pin = pin;
  pinMode(_pin, INPUT);  // pull-up externo de 10kΩ a 3.3V
}

PulseAction ButtonInput::update() {
  // Activo en LOW (pull-up externo, botón conecta a GND)
  bool reading = (digitalRead(_pin) == LOW);

  if (reading && !_pressed) {
    // Flanco de presión
    _pressed = true;
    _pressStart = millis();
    _longFired = false;
  }

  if (reading && _pressed && !_longFired) {
    // ¿Ya pasó el tiempo de long press?
    if (millis() - _pressStart >= PULSE_LONG_MS) {
      _longFired = true;
      return PULSE_LONG;
    }
  }

  if (!reading && _pressed) {
    // Soltó el botón
    _pressed = false;
    if (!_longFired) {
      return PULSE_SHORT;
    }
  }

  return PULSE_NONE;
}
