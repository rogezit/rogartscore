#include "GameLogic.h"

// ── Secuencia de puntos pádel oficial ────────────────────
// Internamente: 0=0, 1=15, 2=30, 3=40, 4=AD
static const char* POINT_LABELS[] = {"0", "15", "30", "40", "AD"};

void GameLogic::init(GameMode mode) {
  _mode = mode;
  reset();
}

void GameLogic::reset() {
  _sets[0] = _sets[1] = 0;
  _games[0] = _games[1] = 0;
  _points[0] = _points[1] = 0;
  _tiebreak = false;
  _superTiebreak = false;
  _gameOver = false;
}

void GameLogic::resetSet() {
  _games[0] = _games[1] = 0;
  _points[0] = _points[1] = 0;
  _tiebreak = false;
}

void GameLogic::addPoint(Team t) {
  if (_gameOver) return;

  switch (_mode) {
    case MODE_OFFICIAL:  _addPointOfficial(t); break;
    case MODE_GOLDEN:    _addPointGolden(t);   break;
    case MODE_AMERICANO: _addPointAmericano(t); break;
    case MODE_TRAINING:  _addPointTraining(t); break;
  }
}

void GameLogic::undoPoint(Team t) {
  if (_gameOver) return;

  switch (_mode) {
    case MODE_TRAINING:
      if (_points[t] > 0) _points[t]--;
      break;

    case MODE_AMERICANO:
      if (_points[t] > 0) {
        _points[t]--;
      } else if (_games[t] > 0) {
        // Volver al game anterior: restar game, puntos quedan en 0
        _games[t]--;
      }
      break;

    default:  // OFFICIAL / GOLDEN
      if (_tiebreak || _superTiebreak) {
        if (_points[t] > 0) _points[t]--;
      } else {
        if (_points[t] > 0) _points[t]--;
      }
      break;
  }
}

// ══════════════════════════════════════════════════════════
//  Modo 1: Partido Oficial
// ══════════════════════════════════════════════════════════

void GameLogic::_addPointOfficial(Team t) {
  if (_tiebreak || _superTiebreak) {
    uint8_t target = _superTiebreak ? 10 : 7;
    _addPointTiebreak(t, target);
    return;
  }

  Team other = (t == TEAM_A) ? TEAM_B : TEAM_A;

  if (_points[t] < 3) {
    // 0→1(15), 1→2(30), 2→3(40)
    _points[t]++;
  } else if (_points[t] == 3 && _points[other] < 3) {
    // Tiene 40, rival < 40 → gana game
    _winGame(t);
  } else if (_points[t] == 3 && _points[other] == 3) {
    // 40-40 (deuce) → ventaja
    _points[t] = 4;  // AD
  } else if (_points[t] == 4) {
    // Tiene AD → gana game
    _winGame(t);
  } else if (_points[other] == 4) {
    // Rival tiene AD → vuelve a deuce
    _points[other] = 3;
  }
}

// ══════════════════════════════════════════════════════════
//  Modo 2: Punto de Oro (sin ventaja)
// ══════════════════════════════════════════════════════════

void GameLogic::_addPointGolden(Team t) {
  if (_tiebreak || _superTiebreak) {
    uint8_t target = _superTiebreak ? 10 : 7;
    _addPointTiebreak(t, target);
    return;
  }

  Team other = (t == TEAM_A) ? TEAM_B : TEAM_A;

  if (_points[t] < 3) {
    _points[t]++;
  } else if (_points[t] == 3 && _points[other] < 3) {
    _winGame(t);
  } else if (_points[t] == 3 && _points[other] == 3) {
    // 40-40 → punto decisivo, gana directamente
    _winGame(t);
  }
}

// ══════════════════════════════════════════════════════════
//  Modo 3: Americano / Mixto
// ══════════════════════════════════════════════════════════

void GameLogic::_addPointAmericano(Team t) {
  // Puntos normales dentro del game (0→15→30→40→game)
  Team other = (t == TEAM_A) ? TEAM_B : TEAM_A;

  if (_points[t] < 3) {
    _points[t]++;
  } else {
    // Ganó el game (sin deuce en americano)
    _games[t]++;
    _points[0] = _points[1] = 0;
    // No hay sets ni fin automático en americano
  }
}

// ══════════════════════════════════════════════════════════
//  Modo 4: Entrenamiento (conteo libre)
// ══════════════════════════════════════════════════════════

void GameLogic::_addPointTraining(Team t) {
  _points[t]++;
}

// ══════════════════════════════════════════════════════════
//  Tiebreak (estándar 7pts o súper 10pts)
// ══════════════════════════════════════════════════════════

void GameLogic::_addPointTiebreak(Team t, uint8_t target) {
  _points[t]++;
  Team other = (t == TEAM_A) ? TEAM_B : TEAM_A;

  if (_points[t] >= target && (_points[t] - _points[other]) >= 2) {
    // Ganó el tiebreak → ganó el set
    _tiebreak = false;
    bool wasSuperTB = _superTiebreak;
    _superTiebreak = false;

    if (wasSuperTB) {
      // Súper tiebreak reemplaza el 3er set → gana partido
      _winMatch(t);
    } else {
      // Tiebreak normal → gana set (games queda 7-6)
      _games[t]++;
      _winSet(t);
    }
  }
}

void GameLogic::_checkTiebreak() {
  // Solo aplica en modos con sets (OFFICIAL y GOLDEN)
  if (_mode != MODE_OFFICIAL && _mode != MODE_GOLDEN) return;

  if (_games[0] == 6 && _games[1] == 6) {
    _tiebreak = true;
    _points[0] = _points[1] = 0;
  }
}

// ══════════════════════════════════════════════════════════
//  Ganar game / set / partido
// ══════════════════════════════════════════════════════════

void GameLogic::_winGame(Team t) {
  _games[t]++;
  _points[0] = _points[1] = 0;

  Team other = (t == TEAM_A) ? TEAM_B : TEAM_A;

  // ¿Ganó el set? (6 games con +2 diferencia)
  if (_games[t] >= 6 && (_games[t] - _games[other]) >= 2) {
    _winSet(t);
  } else {
    _checkTiebreak();
  }
}

void GameLogic::_winSet(Team t) {
  _sets[t]++;
  _games[0] = _games[1] = 0;
  _points[0] = _points[1] = 0;
  _tiebreak = false;

  // ¿Ganó el partido? (best of 3 → 2 sets)
  if (_sets[t] >= 2) {
    _winMatch(t);
  } else if (_sets[0] == 1 && _sets[1] == 1) {
    // 1-1 en sets → súper tiebreak reemplaza 3er set
    _superTiebreak = true;
    _points[0] = _points[1] = 0;
  }
}

void GameLogic::_winMatch(Team t) {
  _gameOver = true;
  _winner = t;
}

// ══════════════════════════════════════════════════════════
//  Display
// ══════════════════════════════════════════════════════════

const char* GameLogic::getPointDisplay(Team t) const {
  if (_mode == MODE_TRAINING || _tiebreak || _superTiebreak) {
    snprintf(_ptBuf[t], sizeof(_ptBuf[t]), "%d", _points[t]);
    return _ptBuf[t];
  }

  if (_mode == MODE_AMERICANO) {
    if (_points[t] <= 3) return POINT_LABELS[_points[t]];
    snprintf(_ptBuf[t], sizeof(_ptBuf[t]), "%d", _points[t]);
    return _ptBuf[t];
  }

  // OFFICIAL / GOLDEN
  if (_points[t] <= 4) return POINT_LABELS[_points[t]];
  snprintf(_ptBuf[t], sizeof(_ptBuf[t]), "%d", _points[t]);
  return _ptBuf[t];
}

void GameLogic::printState() const {
  Serial.print(F("[SCORE] "));

  if (_mode == MODE_TRAINING) {
    Serial.print(F("A:")); Serial.print(_points[0]);
    Serial.print(F("  B:")); Serial.println(_points[1]);
    return;
  }

  if (_mode == MODE_AMERICANO) {
    Serial.print(F("Games A:")); Serial.print(_games[0]);
    Serial.print(F("  B:")); Serial.print(_games[1]);
    Serial.print(F("  | Pts A:")); Serial.print(getPointDisplay(TEAM_A));
    Serial.print(F("  B:")); Serial.println(getPointDisplay(TEAM_B));
    return;
  }

  // OFFICIAL / GOLDEN
  Serial.print(F("Sets ")); Serial.print(_sets[0]);
  Serial.print(F("-")); Serial.print(_sets[1]);
  Serial.print(F("  Games ")); Serial.print(_games[0]);
  Serial.print(F("-")); Serial.print(_games[1]);

  if (_tiebreak) Serial.print(F(" [TB]"));
  if (_superTiebreak) Serial.print(F(" [STB]"));

  Serial.print(F("  Pts ")); Serial.print(getPointDisplay(TEAM_A));
  Serial.print(F("-")); Serial.println(getPointDisplay(TEAM_B));
}
