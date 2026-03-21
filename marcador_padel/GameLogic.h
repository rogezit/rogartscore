#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "config.h"

// Equipo
enum Team : uint8_t { TEAM_A = 0, TEAM_B = 1 };

class GameLogic {
public:
  void init(GameMode mode);
  void reset();             // reinicia partido completo
  void resetSet();          // reinicia set actual (games y puntos a 0)

  void addPoint(Team team);
  void undoPoint(Team team);

  // Getters
  GameMode getMode() const { return _mode; }
  bool     isGameOver() const { return _gameOver; }
  Team     getWinner() const { return _winner; }

  uint8_t  getSets(Team t) const  { return _sets[t]; }
  uint8_t  getGames(Team t) const { return _games[t]; }
  uint8_t  getPointsRaw(Team t) const { return _points[t]; }
  bool     isTiebreak() const { return _tiebreak; }
  bool     isSuperTiebreak() const { return _superTiebreak; }
  uint8_t  getCurrentSet() const { return _currentSet; }
  uint8_t  getSetHistoryGames(uint8_t set, Team t) const { return _setHistory[set][t]; }
  bool     isDeuce() const;

  // Retorna el texto del punto para display: "0","15","30","40","AD" o número
  const char* getPointDisplay(Team t) const;

  // Imprime estado completo al Serial
  void printState() const;

private:
  GameMode _mode = MODE_OFFICIAL;
  bool     _gameOver = false;
  Team     _winner = TEAM_A;

  uint8_t _sets[2]   = {0, 0};
  uint8_t _games[2]  = {0, 0};
  uint8_t _points[2] = {0, 0};
  bool    _tiebreak = false;
  bool    _superTiebreak = false;
  uint8_t _setHistory[3][2] = {{0,0},{0,0},{0,0}};
  uint8_t _currentSet = 0;

  // Buffers para getPointDisplay
  mutable char _ptBuf[2][8];

  void _addPointOfficial(Team t);
  void _addPointGolden(Team t);
  void _addPointAmericano(Team t);
  void _addPointTraining(Team t);

  void _winGame(Team t);
  void _winSet(Team t);
  void _winMatch(Team t);

  void _addPointTiebreak(Team t, uint8_t target);
  void _checkTiebreak();
};

#endif
