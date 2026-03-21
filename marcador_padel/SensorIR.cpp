#include "SensorIR.h"

void SensorIR::begin(uint8_t pinDO, uint8_t pinAO) {
  _pinDO = pinDO;
  _pinAO = pinAO;
  pinMode(_pinDO, INPUT);
  pinMode(_pinAO, INPUT);
}

void SensorIR::calibrate() {
  long sum = 0;
  for (int i = 0; i < CALIB_SAMPLES; i++) {
    sum += analogRead(_pinAO);
    delay(CALIB_DELAY_MS);
  }
  _baseline  = (int)(sum / CALIB_SAMPLES);
  _baselineF = (float)_baseline;
  _updateThresholds();

  for (int i = 0; i < FILTER_SIZE; i++) _buffer[i] = _baseline;
  _bufFull = true;
}

void SensorIR::stabilize() {
  for (int i = 0; i < 200; i++) {
    int val  = analogRead(_pinAO);
    int filt = _addSample(val);
    _baselineF = _baselineF * 0.95f + filt * 0.05f;
    _baseline  = (int)_baselineF;
    _updateThresholds();
    delay(10);
  }
}

SensorAction SensorIR::update() {
  int aoRaw    = analogRead(_pinAO);
  int filtered = _addSample(aoRaw);

  _prevDetected = _detected;

  // Detección: señal CAE por debajo del umbral
  if (!_detected && filtered < _threshold) {
    _detected = true;
    _detectStartMs = millis();
  } else if (_detected && filtered > _thresholdHigh) {
    _detected = false;
  }

  // Baseline adaptativo (solo sin objeto, dentro de zona muerta)
  if (!_detected) {
    _updateBaseline(filtered);
  }

  // Acción al SOLTAR (flanco de bajada: estaba detectado, ya no)
  SensorAction action = ACTION_NONE;
  if (_prevDetected && !_detected) {
    uint32_t now = millis();
    // Debounce: ignorar si la última acción fue muy reciente
    if (now - _lastActionMs >= SENSOR_DEBOUNCE_MS) {
      uint32_t held = now - _detectStartMs;
      if (held >= SENSOR_TIME_RESET_MS) {
        action = ACTION_RESET;
      } else if (held >= SENSOR_TIME_UNDO_MS) {
        action = ACTION_UNDO;
      } else if (held >= SENSOR_TIME_MIN_MS) {
        action = ACTION_POINT;
      }
      _lastActionMs = now;
    }
  }

  return action;
}

int SensorIR::getFiltered() const {
  int count = _bufFull ? FILTER_SIZE : _bufIdx;
  if (count == 0) return _baseline;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += _buffer[i];
  return (int)(sum / count);
}

// ── Privados ─────────────────────────────────────────────

int SensorIR::_addSample(int val) {
  _buffer[_bufIdx] = val;
  _bufIdx = (_bufIdx + 1) % FILTER_SIZE;
  if (_bufIdx == 0) _bufFull = true;

  int count = _bufFull ? FILTER_SIZE : _bufIdx;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += _buffer[i];
  return (int)(sum / count);
}

void SensorIR::_updateThresholds() {
  _threshold     = _baseline - (_baseline * THRESHOLD_PCT / 100);
  _thresholdHigh = _baseline - (_baseline * (THRESHOLD_PCT - HYSTERESIS_PCT) / 100);
}

void SensorIR::_updateBaseline(int filtered) {
  int deadzone = _baseline * BASELINE_DEADZONE / 100;
  if (abs(filtered - _baseline) <= deadzone) {
    _baselineF = _baselineF * (1.0f - BASELINE_ALPHA) + filtered * BASELINE_ALPHA;
    _baseline  = (int)_baselineF;
    _updateThresholds();
  }
}
