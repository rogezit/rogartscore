/**
 * ╔══════════════════════════════════════════════════════════════╗
 * ║   TEST SENSOR IR · ESP32 DevKit v1 (38 pines)               ║
 * ║   v6 — 2x TCRT5000 + LED RGB (ánodo común)                  ║
 * ╚══════════════════════════════════════════════════════════════╝
 *
 * ┌──────────────────────────────────────────────────────────────┐
 * │  SENSORES: 2x TCRT5000  (módulo de 4 pines cada uno)        │
 * │  Pines: VCC · GND · DO (digital) · AO (analógico)           │
 * │                                                              │
 * │  · LED IR emisor + fototransistor receptor.                  │
 * │  · AO = señal analógica continua (0–4095). Cuanto más cerca │
 * │    o más reflectante es el objeto, menor es el valor.        │
 * │  · DO = salida digital (HIGH/LOW) según potenciómetro azul. │
 * │  · CALIBRACIÓN POR SOFTWARE: media móvil, baseline          │
 * │    adaptativo y umbral dinámico sobre la señal AO.           │
 * └──────────────────────────────────────────────────────────────┘
 *
 *  PINES RESERVADOS POR PANEL HUB75 (NO USAR):
 *   GPIO 25, 26, 2, 14, 12, 27, 23, 19, 5, 17, 16, 4, 15
 *
 *  CONEXIÓN:
 *
 *   ESP32           Sensor 1 TCRT5000
 *   ─────────────────────────────────────────────
 *   3.3V       →    VCC
 *   GND        →    GND
 *   GPIO22     ←    DO  (digital)
 *   GPIO33     ←    AO  (analógico)
 *   ─────────────────────────────────────────────
 *
 *   ESP32           Sensor 2 TCRT5000
 *   ─────────────────────────────────────────────
 *   3.3V       →    VCC
 *   GND        →    GND
 *   GPIO34     ←    DO  (digital, solo entrada)
 *   GPIO32     ←    AO  (analógico, ADC1_CH4)
 *   ─────────────────────────────────────────────
 *
 *   ESP32           LED RGB (ánodo común)
 *   ─────────────────────────────────────────────
 *   3.3V       →    Ánodo (+) largo
 *   GPIO18     →    R  (con resistencia 220Ω)
 *   GPIO21     →    G  (con resistencia 220Ω)
 *   GPIO13     →    B  (con resistencia 220Ω)
 *   ─────────────────────────────────────────────
 *   NOTA: Ánodo común → LOW=encendido, HIGH=apagado
 *
 *   Colores según distancia:
 *     Verde    = nada detectado (lejos del umbral)
 *     Amarillo = algo se acerca (zona intermedia)
 *     Rojo     = objeto detectado (cruzó el umbral)
 *
 *  CALIBRACIÓN AL ENCENDER (~3 seg):
 *   → Los sensores deben estar YA montados en su posición final.
 *   → Durante 3 seg no debe haber nada delante de los sensores
 *     (ni jugador, ni raqueta, ni pelota).
 *
 *  Abre Serial Monitor en Arduino IDE a 115200 baud
 *
 *  COMPILAR Y SUBIR CON arduino-cli:
 *   arduino-cli compile --fqbn esp32:esp32:esp32 test_sensores_ir/
 *   arduino-cli upload --fqbn esp32:esp32:esp32:UploadSpeed=460800 --port /dev/cu.usbserial-10 test_sensores_ir/
 *
 *  MONITOR SERIAL:
 *   arduino-cli monitor --port /dev/cu.usbserial-10 --config baudrate=115200
 */

// ══════════════════════════════════════════════════════════
//  PINES  (compatibles con panel HUB75)
// ══════════════════════════════════════════════════════════
// Sensor 1
#define PIN_S1_DO   22    // TCRT5000 #1 digital out  → GPIO22
#define PIN_S1_AO   33    // TCRT5000 #1 analógico    → GPIO33 (ADC1_CH5)

// Sensor 2
#define PIN_S2_DO   34    // TCRT5000 #2 digital out  → GPIO34 (solo entrada)
#define PIN_S2_AO   32    // TCRT5000 #2 analógico    → GPIO32 (ADC1_CH4)

// LED RGB (ánodo común)
#define PIN_LED_R     18    // Rojo   → GPIO18
#define PIN_LED_G     21    // Verde  → GPIO21
#define PIN_LED_B     13    // Azul   → GPIO13

// PWM config
#define PWM_FREQ    5000    // 5 kHz
#define PWM_RES        8    // 8 bits (0–255)

// ══════════════════════════════════════════════════════════
//  CONFIG GENERAL
// ══════════════════════════════════════════════════════════
#define SERIAL_BAUD     115200
#define NUM_SENSORS     2

// ══════════════════════════════════════════════════════════
//  Filtro media móvil (señal AO)
// ══════════════════════════════════════════════════════════
#define FILTER_SIZE     8           // muestras (potencia de 2)

// ══════════════════════════════════════════════════════════
//  Calibración y umbral adaptativo (señal AO)
// ══════════════════════════════════════════════════════════
#define CALIB_SAMPLES     50        // muestras durante calibración (~3 seg)
#define CALIB_DELAY_MS    60        // delay entre muestras de calibración
#define THRESHOLD_PCT     5         // % de caída vs baseline = detección
#define HYSTERESIS_PCT    2         // % extra para soltar (anti-rebote)
#define BASELINE_ALPHA    0.002f    // adapt. lenta (~30 seg para ajustarse)
#define BASELINE_DEADZONE 5         // % máx desviación para adaptar baseline

// ══════════════════════════════════════════════════════════
//  Estructura por sensor
// ══════════════════════════════════════════════════════════
struct SensorIR {
  uint8_t pinDO;
  uint8_t pinAO;
  int     aoBuffer[FILTER_SIZE];
  int     bufferIndex;
  bool    bufferFull;
  int     aoBaseline;
  int     aoThreshold;
  int     aoThresholdHigh;
  float   baselineFloat;
  bool    detected;
  bool    prevDetected;     // para detectar flanco de subida
};

SensorIR sensors[NUM_SENSORS] = {
  { PIN_S1_DO, PIN_S1_AO, {0}, 0, false, 0, 0, 0, 0.0f, false, false },
  { PIN_S2_DO, PIN_S2_AO, {0}, 0, false, 0, 0, 0, 0.0f, false, false }
};

// ──────────────────────────────────────────────────────────
//  FUNCIONES
// ──────────────────────────────────────────────────────────

// Media móvil sobre la señal analógica AO
int addSampleAndFilter(SensorIR &s, int newVal) {
  s.aoBuffer[s.bufferIndex] = newVal;
  s.bufferIndex = (s.bufferIndex + 1) % FILTER_SIZE;
  if (s.bufferIndex == 0) s.bufferFull = true;

  int count = s.bufferFull ? FILTER_SIZE : s.bufferIndex;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += s.aoBuffer[i];
  return (int)(sum / count);
}

// Recalcula umbrales a partir del baseline (detecta CAÍDA)
void updateThresholds(SensorIR &s) {
  s.aoThreshold     = s.aoBaseline - (s.aoBaseline * THRESHOLD_PCT / 100);
  s.aoThresholdHigh = s.aoBaseline - (s.aoBaseline * (THRESHOLD_PCT - HYSTERESIS_PCT) / 100);
}

// ──────────────────────────────────────────────────────────
//  LED RGB — Ánodo común (0=full brillo, 255=apagado)
// ──────────────────────────────────────────────────────────
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  ledcWrite(PIN_LED_R, 255 - r);
  ledcWrite(PIN_LED_G, 255 - g);
  ledcWrite(PIN_LED_B, 255 - b);
}

// LED según estado de ambos sensores
void updateLedFromSensors() {
  bool anyDetected = false;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensors[i].detected) { anyDetected = true; break; }
  }

  if (anyDetected) {
    setRGB(255, 0, 0);     // ROJO — objeto detectado
  } else {
    // Busca el sensor más cercano al umbral (menor ratio)
    float minRatio = 1.0f;
    for (int i = 0; i < NUM_SENSORS; i++) {
      float range = (float)(sensors[i].aoBaseline - sensors[i].aoThreshold);
      if (range <= 0) range = 1;
      int aoFilt = 0;
      // Recalcular filtrado actual desde buffer
      int count = sensors[i].bufferFull ? FILTER_SIZE : sensors[i].bufferIndex;
      long sum = 0;
      for (int j = 0; j < count; j++) sum += sensors[i].aoBuffer[j];
      aoFilt = (count > 0) ? (int)(sum / count) : sensors[i].aoBaseline;
      float ratio = (float)(aoFilt - sensors[i].aoThreshold) / range;
      ratio = constrain(ratio, 0.0f, 1.0f);
      if (ratio < minRatio) minRatio = ratio;
    }

    if (minRatio > 0.6f) {
      setRGB(0, 255, 0);   // VERDE
    } else if (minRatio > 0.0f) {
      uint8_t g = (uint8_t)(255.0f * (minRatio / 0.6f));
      setRGB(255, g, 0);   // AMARILLO→ROJO gradual
    } else {
      setRGB(255, 0, 0);   // ROJO
    }
  }
}

// Calibrar un sensor individual
void calibrateSensor(SensorIR &s, int sensorNum) {
  Serial.print(F("  >> CALIBRANDO SENSOR ")); Serial.print(sensorNum);
  Serial.print(F(" (AO=GPIO")); Serial.print(s.pinAO);
  Serial.println(F(")..."));

  long calibSum = 0;
  int  calibMin = 4095, calibMax = 0;
  for (int i = 0; i < CALIB_SAMPLES; i++) {
    int val = analogRead(s.pinAO);
    calibSum += val;
    if (val < calibMin) calibMin = val;
    if (val > calibMax) calibMax = val;
    delay(CALIB_DELAY_MS);
  }
  s.aoBaseline = (int)(calibSum / CALIB_SAMPLES);
  s.baselineFloat = (float)s.aoBaseline;
  updateThresholds(s);

  for (int i = 0; i < FILTER_SIZE; i++) s.aoBuffer[i] = s.aoBaseline;
  s.bufferFull = true;

  Serial.print(F("  Sensor ")); Serial.print(sensorNum);
  Serial.print(F(": baseline=")); Serial.print(s.aoBaseline);
  Serial.print(F("  ruido=")); Serial.print(calibMax - calibMin);
  Serial.print(F("  umbral=<")); Serial.println(s.aoThreshold);
}

// Estabilizar un sensor individual
void stabilizeSensor(SensorIR &s) {
  for (int i = 0; i < 200; i++) {
    int val = analogRead(s.pinAO);
    int filt = addSampleAndFilter(s, val);
    s.baselineFloat = s.baselineFloat * 0.95f + filt * 0.05f;
    s.aoBaseline = (int)s.baselineFloat;
    updateThresholds(s);
    delay(10);
  }
}

// ══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(400);

  // Configurar pines de ambos sensores
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensors[i].pinDO, INPUT);
    pinMode(sensors[i].pinAO, INPUT);
  }

  // Configurar PWM para LED RGB (ESP32 core v3 API)
  ledcAttach(PIN_LED_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_B, PWM_FREQ, PWM_RES);
  setRGB(0, 0, 255);  // Azul durante calibración

  analogReadResolution(12);       // 0–4095
  analogSetAttenuation(ADC_11db); // 0–3.3V

  Serial.println(F("\n═══════════════════════════════════════════════"));
  Serial.println(F("  TEST SENSOR IR v6 — 2x TCRT5000 + LED RGB"));
  Serial.println(F("  Sensor 1: DO=GPIO22  AO=GPIO33"));
  Serial.println(F("  Sensor 2: DO=GPIO34  AO=GPIO32"));
  Serial.println(F("  LED:      R=GPIO18   G=GPIO21   B=GPIO2"));
  Serial.println(F("═══════════════════════════════════════════════"));

  // ── CALIBRACIÓN (~6 seg total) ────────────────────────
  Serial.println(F("     No pongas nada delante de los sensores."));
  for (int i = 0; i < NUM_SENSORS; i++) {
    calibrateSensor(sensors[i], i + 1);
  }

  Serial.println(F("───────────────────────────────────────────────"));

  // ── ESTABILIZACIÓN POST-CALIBRACIÓN (~4 seg total) ────
  Serial.println(F("  >> Estabilizando baselines..."));
  setRGB(0, 0, 255);
  for (int i = 0; i < NUM_SENSORS; i++) {
    stabilizeSensor(sensors[i]);
  }

  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print(F("  Sensor ")); Serial.print(i + 1);
    Serial.print(F(": baseline=")); Serial.print(sensors[i].aoBaseline);
    Serial.print(F("  umbral=<")); Serial.println(sensors[i].aoThreshold);
  }
  Serial.println(F("  >> Listo!\n"));
  Serial.println(F("═══════════════════════════════════════════════\n"));
  delay(300);
}

// ══════════════════════════════════════════════════════════
void loop() {

  for (int i = 0; i < NUM_SENSORS; i++) {
    SensorIR &s = sensors[i];

    // ── Lectura analógica + filtro ──
    int aoRaw      = analogRead(s.pinAO);
    int aoFiltered = addSampleAndFilter(s, aoRaw);

    // Guardar estado previo para detectar flanco
    s.prevDetected = s.detected;

    // Detección: señal CAE por debajo del umbral
    if (!s.detected && aoFiltered < s.aoThreshold) {
      s.detected = true;
    } else if (s.detected && aoFiltered > s.aoThresholdHigh) {
      s.detected = false;
    }

    // Imprimir al detectar (flanco de subida)
    if (s.detected && !s.prevDetected) {
      Serial.print(F(">>> PUNTO DETECTADO SENSOR "));
      Serial.println(i + 1);
    }

    // Baseline adaptativo (solo cuando no hay objeto Y dentro de zona muerta)
    if (!s.detected) {
      int deadzone = s.aoBaseline * BASELINE_DEADZONE / 100;
      if (abs(aoFiltered - s.aoBaseline) <= deadzone) {
        s.baselineFloat = s.baselineFloat * (1.0f - BASELINE_ALPHA) + aoFiltered * BASELINE_ALPHA;
        s.aoBaseline = (int)s.baselineFloat;
        updateThresholds(s);
      }
    }
  }

  // ── LED RGB (refleja el sensor más cercano al umbral) ──
  updateLedFromSensors();

  delay(5);
}

/**
 * ═══════════════════════════════════════════════════════════
 *  PINES RESERVADOS POR PANEL HUB75
 * ═══════════════════════════════════════════════════════════
 *
 *   IDC 1  R1  → GPIO 25    IDC 2  G1  → GPIO 26
 *   IDC 3  B1  → GPIO 2
 *   IDC 5  R2  → GPIO 14    IDC 6  G2  → GPIO 12
 *   IDC 7  B2  → GPIO 27
 *   IDC 9  A   → GPIO 23    IDC 10 B   → GPIO 19
 *   IDC 11 C   → GPIO 5     IDC 12 D   → GPIO 17
 *   IDC 13 CLK → GPIO 16    IDC 14 LAT → GPIO 4
 *   IDC 15 OE  → GPIO 15
 *
 * ═══════════════════════════════════════════════════════════
 *  PINES LIBRES DISPONIBLES
 * ═══════════════════════════════════════════════════════════
 *
 *   GPIO 13  ← B LED RGB (este sketch)
 *   GPIO 18  ← R LED RGB (este sketch)
 *   GPIO 21  ← G LED RGB (este sketch)
 *   GPIO 22  ← DO TCRT5000 #1 (este sketch)
 *   GPIO 32  ← AO TCRT5000 #2 (este sketch)
 *   GPIO 33  ← AO TCRT5000 #1 (este sketch)
 *   GPIO 34  ← DO TCRT5000 #2 (este sketch)
 *   GPIO 35  (solo entrada, ADC1_CH7)
 *   GPIO 36  (solo entrada, VP, ADC1_CH0)
 *   GPIO 39  (solo entrada, VN, ADC1_CH3)
 *
 * ═══════════════════════════════════════════════════════════
 */
