#include "Display.h"
#include "SensorIR.h"

void Display::begin() {
  ledcAttach(PIN_LED_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_B, PWM_FREQ, PWM_RES);
  setRGB(0, 0, 0);
}

void Display::setRGB(uint8_t r, uint8_t g, uint8_t b) {
  // Ánodo común: invertir (255 = apagado, 0 = brillo máximo)
  ledcWrite(PIN_LED_R, 255 - r);
  ledcWrite(PIN_LED_G, 255 - g);
  ledcWrite(PIN_LED_B, 255 - b);
}

void Display::showSelectMode(GameMode mode) {
  if (mode != _lastMode) {
    _lastMode = mode;
    Serial.print(F("[MODO] Seleccionado: "));
    switch (mode) {
      case MODE_OFFICIAL:  Serial.println(F("1 - Partido Oficial")); break;
      case MODE_GOLDEN:    Serial.println(F("2 - Punto de Oro"));    break;
      case MODE_AMERICANO: Serial.println(F("3 - Americano/Mixto")); break;
      case MODE_TRAINING:  Serial.println(F("4 - Entrenamiento"));   break;
    }
  }

  // LED azul parpadeante durante selección
  uint32_t now = millis();
  if (now - _lastBlinkMs >= 500) {
    _lastBlinkMs = now;
    _blinkState = !_blinkState;
    setRGB(0, 0, _blinkState ? 255 : 60);
  }
}

void Display::showProximity(const SensorIR& sA, const SensorIR& sB) {
  bool anyDetected = sA.isDetected() || sB.isDetected();

  if (anyDetected) {
    setRGB(255, 0, 0);     // ROJO — objeto detectado
    return;
  }

  // Busca el sensor más cercano al umbral
  float minRatio = 1.0f;
  const SensorIR* sensors[] = { &sA, &sB };
  for (int i = 0; i < 2; i++) {
    float range = (float)(sensors[i]->getBaseline() - sensors[i]->getThreshold());
    if (range <= 0) range = 1;
    float ratio = (float)(sensors[i]->getFiltered() - sensors[i]->getThreshold()) / range;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    if (ratio < minRatio) minRatio = ratio;
  }

  if (minRatio > 0.6f) {
    setRGB(0, 255, 0);     // VERDE
  } else if (minRatio > 0.0f) {
    uint8_t g = (uint8_t)(255.0f * (minRatio / 0.6f));
    setRGB(255, g, 0);     // AMARILLO→ROJO gradual
  } else {
    setRGB(255, 0, 0);     // ROJO
  }
}

void Display::showScore(const GameLogic& game) {
  setRGB(0, 255, 0);  // LED verde durante juego
}

void Display::showGameOver(Team winner, const GameLogic& game) {
  uint32_t now = millis();
  if (now - _lastBlinkMs >= 400) {
    _lastBlinkMs = now;
    _blinkState = !_blinkState;
    if (_blinkState) {
      setRGB(0, 255, 0);
    } else {
      setRGB(255, 0, 0);
    }
  }
}
