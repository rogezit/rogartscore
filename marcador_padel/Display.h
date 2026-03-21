#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include "GameLogic.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

class SensorIR;  // forward declaration

class Display {
public:
  void begin();
  void showSelectMode(GameMode mode);
  void showProximity(const SensorIR& sA, const SensorIR& sB);
  void showScore(const GameLogic& game);
  void showGameOver(Team winner, const GameLogic& game);
  void setRGB(uint8_t r, uint8_t g, uint8_t b);

private:
  // LED RGB
  GameMode _lastMode = MODE_OFFICIAL;
  bool     _blinkState = false;
  uint32_t _lastBlinkMs = 0;

  // Panel HUB75
  MatrixPanel_I2S_DMA* _panel = nullptr;
  uint16_t _cRed, _cBlue, _cWhite, _cYellow, _cGreen, _cGray, _cDim;

  void _initPanel();
  void _drawTeamRow(int team, int y, const GameLogic& game);
  void _drawScoreContent(const GameLogic& game);
  void _drawToPanel(const GameLogic& game);
};

#endif
