#include "Display.h"
#include "SensorIR.h"

// ── INIT PANEL HUB75 ─────────────────────────────────────
void Display::_initPanel() {
  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUM);
  mxconfig.gpio.r1  = PIN_HUB_R1;
  mxconfig.gpio.g1  = PIN_HUB_G1;
  mxconfig.gpio.b1  = PIN_HUB_B1;
  mxconfig.gpio.r2  = PIN_HUB_R2;
  mxconfig.gpio.g2  = PIN_HUB_G2;
  mxconfig.gpio.b2  = PIN_HUB_B2;
  mxconfig.gpio.a   = PIN_HUB_A;
  mxconfig.gpio.b   = PIN_HUB_B;
  mxconfig.gpio.c   = PIN_HUB_C;
  mxconfig.gpio.d   = PIN_HUB_D;
  mxconfig.gpio.clk = PIN_HUB_CLK;
  mxconfig.gpio.lat = PIN_HUB_LAT;
  mxconfig.gpio.oe  = PIN_HUB_OE;
  mxconfig.driver   = HUB75_I2S_CFG::FM6126A;
  mxconfig.clkphase = false;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.latch_blanking = 4;
  mxconfig.min_refresh_rate = 60;
  mxconfig.double_buff = true;

  _panel = new MatrixPanel_I2S_DMA(mxconfig);
  if (!_panel->begin()) {
    Serial.println(F("ERROR panel HUB75"));
    return;
  }
  _panel->setBrightness8(200);
  _panel->clearScreen();
  _panel->flipDMABuffer();
  delay(100);

  _cRed    = _panel->color565(255, 0, 0);
  _cBlue   = _panel->color565(0, 80, 255);
  _cWhite  = _panel->color565(255, 255, 255);
  _cYellow = _panel->color565(255, 220, 0);
  _cGreen  = _panel->color565(0, 220, 0);
  _cGray   = _panel->color565(100, 100, 100);
  _cDim    = _panel->color565(35, 35, 35);
}

// ── BEGIN ─────────────────────────────────────────────────
void Display::begin() {
  // LED RGB (ánodo común)
  ledcAttach(PIN_LED_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_B, PWM_FREQ, PWM_RES);
  setRGB(0, 0, 0);

  // Panel HUB75
  _initPanel();
}

// ── LED RGB ───────────────────────────────────────────────
void Display::setRGB(uint8_t r, uint8_t g, uint8_t b) {
  ledcWrite(PIN_LED_R, 255 - r);
  ledcWrite(PIN_LED_G, 255 - g);
  ledcWrite(PIN_LED_B, 255 - b);
}

// ── DIBUJAR FILA DE EQUIPO (misma posición que padel-panel) ──
void Display::_drawTeamRow(int team, int y, const GameLogic& game) {
  if (!_panel) return;

  uint16_t mainColor = (team == 0) ? _cRed : _cBlue;
  uint16_t dimColor  = (team == 0)
                       ? _panel->color565(120, 0, 0)
                       : _panel->color565(0, 0, 120);

  _panel->setTextWrap(false);
  _panel->setTextSize(1);

  const int setX[3] = { X_S0, X_S1, X_S2 };
  Team t   = (Team)team;
  Team opp = (team == 0) ? TEAM_B : TEAM_A;
  uint8_t curSet = game.getCurrentSet();

  // ── SETS ─────────────────────────────────────────────
  for (int s = 0; s < 3; s++) {
    if (s < curSet) {
      uint8_t myVal  = game.getSetHistoryGames(s, t);
      uint8_t oppVal = game.getSetHistoryGames(s, opp);
      bool iWon = myVal > oppVal;
      _panel->setTextColor(iWon ? mainColor : _cGray);
      _panel->setCursor(setX[s], y);
      _panel->print(myVal);
    } else if (s == curSet) {
      _panel->setTextColor(dimColor);
      _panel->setCursor(setX[s], y);
      _panel->print(game.getGames(t));
    } else {
      _panel->setTextColor(_cDim);
      _panel->setCursor(setX[s], y);
      _panel->print("-");
    }
  }

  // ── SEPARADOR VERTICAL ───────────────────────────────
  for (int py = y; py < y + 7; py++) {
    _panel->drawPixel(X_SEP, py, _cGray);
  }

  // ── PUNTOS centrado en zona X_PTS..31 ────────────────
  const char* pstr = game.getPointDisplay(t);
  int lenPx = strlen(pstr) * 6;
  int xPts  = X_PTS + (PTS_W - lenPx) / 2;
  if (xPts < X_PTS) xPts = X_PTS;

  bool deuce = game.isDeuce();
  uint16_t pColor = (deuce && game.getPointsRaw(t) == 4) ? _cYellow
                    : deuce ? _cGray
                    : _cWhite;

  _panel->setTextColor(pColor);
  _panel->setCursor(xPts, y);
  _panel->print(pstr);
}

// ── CONTENIDO MARCADOR COMPLETO ───────────────────────────
void Display::_drawScoreContent(const GameLogic& game) {
  if (!_panel) return;

  // Equipo A — fila superior (y=1)
  _drawTeamRow(0, 1, game);

  // Línea divisora horizontal (y=8)
  for (int x = 0; x < PANEL_WIDTH; x++) {
    _panel->drawPixel(x, 8, _cDim);
  }

  // Equipo B — fila inferior (y=9)
  _drawTeamRow(1, 9, game);
}

// ── DIBUJAR EN PANEL (doble buffer) ──────────────────────
void Display::_drawToPanel(const GameLogic& game) {
  if (!_panel) return;

  for (int i = 0; i < 2; i++) {
    _panel->clearScreen();
    _drawScoreContent(game);
    _panel->flipDMABuffer();
    delay(20);
  }
}

// ── MOSTRAR SELECCIÓN DE MODO ─────────────────────────────
void Display::showSelectMode(GameMode mode) {
  if (mode != _lastMode) {
    _lastMode = mode;
  }

  // LED azul parpadeante durante selección
  uint32_t now = millis();
  if (now - _lastBlinkMs >= 500) {
    _lastBlinkMs = now;
    _blinkState = !_blinkState;
    setRGB(0, 0, _blinkState ? 255 : 60);
  }
}

// ── PROXIMIDAD (solo LED) ─────────────────────────────────
void Display::showProximity(const SensorIR& sA, const SensorIR& sB) {
  bool anyDetected = sA.isDetected() || sB.isDetected();

  if (anyDetected) {
    setRGB(255, 0, 0);
    return;
  }

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
    setRGB(0, 255, 0);
  } else if (minRatio > 0.0f) {
    uint8_t g = (uint8_t)(255.0f * (minRatio / 0.6f));
    setRGB(255, g, 0);
  } else {
    setRGB(255, 0, 0);
  }
}

// ── MOSTRAR SCORE EN PANEL ────────────────────────────────
void Display::showScore(const GameLogic& game) {
  _drawToPanel(game);
  setRGB(0, 255, 0);
}

// ── FIN DE PARTIDO ────────────────────────────────────────
void Display::showGameOver(Team winner, const GameLogic& game) {
  // Mostrar marcador final en panel
  _drawToPanel(game);

  // LED parpadea entre verde y rojo
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
