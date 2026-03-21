#ifndef SENSOR_IR_H
#define SENSOR_IR_H

#include "config.h"

// Acción resultante de la detección del sensor
enum SensorAction : uint8_t {
  ACTION_NONE,
  ACTION_POINT,     // pala < 1 s  → +1 punto
  ACTION_UNDO,      // pala 3 s    → −1 punto
  ACTION_RESET      // pala 10 s   → reset marcador
};

class SensorIR {
public:
  void begin(uint8_t pinDO, uint8_t pinAO);
  void calibrate();
  void stabilize();

  // Llamar cada ciclo de loop(). Retorna la acción cuando
  // el objeto se RETIRA del sensor (flanco de bajada).
  SensorAction update();

  bool isDetected() const { return _detected; }
  int  getFiltered() const;
  int  getBaseline() const { return _baseline; }
  int  getThreshold() const { return _threshold; }

private:
  uint8_t _pinDO = 0;
  uint8_t _pinAO = 0;

  // Filtro media móvil
  int  _buffer[FILTER_SIZE] = {0};
  int  _bufIdx = 0;
  bool _bufFull = false;

  // Calibración
  int   _baseline = 0;
  int   _threshold = 0;
  int   _thresholdHigh = 0;
  float _baselineF = 0.0f;

  // Detección
  bool     _detected = false;
  bool     _prevDetected = false;
  uint32_t _detectStartMs = 0;   // cuándo empezó la detección
  uint32_t _lastActionMs = 0;    // debounce

  int  _addSample(int val);
  void _updateThresholds();
  void _updateBaseline(int filtered);
};

#endif
