#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include "GameLogic.h"

class SensorIR;  // forward declaration

// ── Stub de display ──────────────────────────────────────
// Por ahora usa LED RGB + Serial.
// Cuando conectes el panel HUB75/P10, reemplaza esta clase.

class Display {
public:
  void begin();
  void showSelectMode(GameMode mode);
  void showProximity(const SensorIR& sA, const SensorIR& sB);
  void showScore(const GameLogic& game);
  void showGameOver(Team winner, const GameLogic& game);
  void setRGB(uint8_t r, uint8_t g, uint8_t b);

private:
  GameMode _lastMode = MODE_OFFICIAL;
  bool     _blinkState = false;
  uint32_t _lastBlinkMs = 0;
};

#endif
