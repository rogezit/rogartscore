/**
 * ╔══════════════════════════════════════════════════════════════╗
 * ║   TEST SENSOR IR · ESP32 DevKit v1 (38 pines)               ║
 * ║   v5 — TCRT5000 + LED RGB (ánodo común)                     ║
 * ╚══════════════════════════════════════════════════════════════╝
 *
 * ┌──────────────────────────────────────────────────────────────┐
 * │  SENSOR: TCRT5000  (módulo de 4 pines)                      │
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
 *   GPIO 25, 26, 27, 14, 12, 13, 23, 19, 5, 17, 16, 4, 15
 *
 *  CONEXIÓN:
 *
 *   ESP32           Sensor TCRT5000
 *   ─────────────────────────────────────────────
 *   3.3V       →    VCC
 *   GND        →    GND
 *   GPIO22     ←    DO  (digital)
 *   GPIO33     ←    AO  (analógico)
 *   ─────────────────────────────────────────────
 *
 *   ESP32           LED RGB (ánodo común)
 *   ─────────────────────────────────────────────
 *   3.3V       →    Ánodo (+) largo
 *   GPIO18     →    R  (con resistencia 220Ω)
 *   GPIO21     →    G  (con resistencia 220Ω)
 *   GPIO2      →    B  (con resistencia 220Ω)
 *   ─────────────────────────────────────────────
 *   NOTA: Ánodo común → LOW=encendido, HIGH=apagado
 *
 *   Colores según distancia:
 *     Verde    = nada detectado (lejos del umbral)
 *     Amarillo = algo se acerca (zona intermedia)
 *     Rojo     = objeto detectado (cruzó el umbral)
 *
 *  CALIBRACIÓN AL ENCENDER (~3 seg):
 *   → El sensor debe estar YA montado en su posición final.
 *   → Durante 3 seg no debe haber nada delante del sensor
 *     (ni jugador, ni raqueta, ni pelota).
 *
 *  Abre Serial Plotter en Arduino IDE a 115200 baud
 */

// ══════════════════════════════════════════════════════════
//  PINES  (compatibles con panel HUB75)
// ══════════════════════════════════════════════════════════
#define PIN_TCRT_DO   22    // TCRT5000 digital out  → GPIO22
#define PIN_TCRT_AO   33    // TCRT5000 analógico    → GPIO33 (ADC1_CH5)

// LED RGB (ánodo común)
#define PIN_LED_R     18    // Rojo   → GPIO18
#define PIN_LED_G     21    // Verde  → GPIO21
#define PIN_LED_B      2    // Azul   → GPIO2

// PWM config
#define PWM_FREQ    5000    // 5 kHz
#define PWM_RES        8    // 8 bits (0–255)

// ══════════════════════════════════════════════════════════
//  CONFIG GENERAL
// ══════════════════════════════════════════════════════════
#define SERIAL_BAUD     115200
#define PRINT_EVERY_MS  80

// ══════════════════════════════════════════════════════════
//  Filtro media móvil (señal AO)
// ══════════════════════════════════════════════════════════
#define FILTER_SIZE     8           // muestras (potencia de 2) — reducido para pala de pádel
int  aoBuffer[FILTER_SIZE];
int  bufferIndex = 0;
bool bufferFull  = false;

// ══════════════════════════════════════════════════════════
//  Calibración y umbral adaptativo (señal AO)
// ══════════════════════════════════════════════════════════
#define CALIB_SAMPLES     50        // muestras durante calibración (~3 seg)
#define CALIB_DELAY_MS    60        // delay entre muestras de calibración
#define THRESHOLD_PCT     5         // % de caída vs baseline = detección (reducido para pala con hoyos)
#define HYSTERESIS_PCT    2         // % extra para soltar (anti-rebote)
// #define THRESHOLD_PCT     8         // % de caída vs baseline = detección
// #define HYSTERESIS_PCT    3         // % extra para soltar (anti-rebote)
#define BASELINE_ALPHA    0.002f    // adapt. lenta (~30 seg para ajustarse)
#define BASELINE_DEADZONE 5         // % máx desviación para adaptar baseline

int   aoBaseline      = 0;         // baseline "sin objeto"
int   aoThreshold     = 0;         // umbral dinámico de detección
int   aoThresholdHigh = 0;         // umbral para soltar (histéresis)
float baselineFloat   = 0.0f;

bool  tcrtDetected    = false;     // detección (AO + histéresis)

// ══════════════════════════════════════════════════════════
//  ESTADO
// ══════════════════════════════════════════════════════════
uint32_t lastPrintMs = 0;

// ──────────────────────────────────────────────────────────
//  FUNCIONES
// ──────────────────────────────────────────────────────────

// Media móvil sobre la señal analógica AO
int addSampleAndFilter(int newVal) {
  aoBuffer[bufferIndex] = newVal;
  bufferIndex = (bufferIndex + 1) % FILTER_SIZE;
  if (bufferIndex == 0) bufferFull = true;

  int count = bufferFull ? FILTER_SIZE : bufferIndex;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += aoBuffer[i];
  return (int)(sum / count);
}

// Recalcula umbrales a partir del baseline (detecta CAÍDA)
void updateThresholds() {
  aoThreshold     = aoBaseline - (aoBaseline * THRESHOLD_PCT / 100);
  aoThresholdHigh = aoBaseline - (aoBaseline * (THRESHOLD_PCT - HYSTERESIS_PCT) / 100);
}

// ──────────────────────────────────────────────────────────
//  LED RGB — Ánodo común (0=full brillo, 255=apagado)
// ──────────────────────────────────────────────────────────
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  // Ánodo común: invertir valores (255 - x)
  ledcWrite(PIN_LED_R, 255 - r);
  ledcWrite(PIN_LED_G, 255 - g);
  ledcWrite(PIN_LED_B, 255 - b);
}

// Mapea aoFiltered a color RGB según proximidad al umbral
void updateLedColor(int filtered, int baseline, int threshold) {
  // 1.0 = en baseline (nada), 0.0 = en umbral (objeto)
  float range = (float)(baseline - threshold);
  if (range <= 0) range = 1;
  float ratio = (float)(filtered - threshold) / range;
  ratio = constrain(ratio, 0.0f, 1.0f);

  if (ratio > 0.6f) {
    // VERDE — lejos del umbral, nada detectado
    setRGB(0, 255, 0);
  } else if (ratio > 0.0f) {
    // AMARILLO→ROJO — transición gradual al acercarse
    // ratio 0.6→0.0 mapea G de 255→0
    uint8_t g = (uint8_t)(255.0f * (ratio / 0.6f));
    setRGB(255, g, 0);
  } else {
    // ROJO — objeto detectado (cruzó el umbral)
    setRGB(255, 0, 0);
  }
}

// ══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(400);

  pinMode(PIN_TCRT_DO, INPUT);
  pinMode(PIN_TCRT_AO, INPUT);

  // Configurar PWM para LED RGB (ESP32 core v3 API)
  ledcAttach(PIN_LED_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_B, PWM_FREQ, PWM_RES);
  setRGB(0, 0, 255);  // Azul durante calibración

  analogReadResolution(12);       // 0–4095
  analogSetAttenuation(ADC_11db); // 0–3.3V

  Serial.println(F("\n═══════════════════════════════════════════════"));
  Serial.println(F("  TEST SENSOR IR v5 — TCRT5000 + LED RGB"));
  Serial.println(F("  Sensor: DO=GPIO22  AO=GPIO33"));
  Serial.println(F("  LED:    R=GPIO18   G=GPIO21   B=GPIO2"));
  Serial.println(F("═══════════════════════════════════════════════"));

  // ── CALIBRACIÓN (~3 seg) ──────────────────────────────
  Serial.println(F("  >> CALIBRANDO TCRT5000 (AO)..."));
  Serial.println(F("     No pongas nada delante del sensor."));
  Serial.println(F("     Espera ~3 segundos..."));

  long calibSum = 0;
  int  calibMin = 4095, calibMax = 0;
  for (int i = 0; i < CALIB_SAMPLES; i++) {
    int val = analogRead(PIN_TCRT_AO);
    calibSum += val;
    if (val < calibMin) calibMin = val;
    if (val > calibMax) calibMax = val;
    delay(CALIB_DELAY_MS);
  }
  aoBaseline = (int)(calibSum / CALIB_SAMPLES);
  baselineFloat = (float)aoBaseline;
  updateThresholds();

  // Llena el buffer del filtro con el baseline
  for (int i = 0; i < FILTER_SIZE; i++) aoBuffer[i] = aoBaseline;
  bufferFull = true;

  Serial.println(F("───────────────────────────────────────────────"));
  Serial.println(F("  TCRT5000 calibrado:"));
  Serial.print(F("    Baseline:      ")); Serial.println(aoBaseline);
  Serial.print(F("    Rango calib:   ")); Serial.print(calibMin);
  Serial.print(F(" – ")); Serial.println(calibMax);
  Serial.print(F("    Ruido:         ")); Serial.println(calibMax - calibMin);
  Serial.print(F("    Umbral det:    < ")); Serial.println(aoThreshold);
  Serial.print(F("    Umbral soltar: > ")); Serial.println(aoThresholdHigh);
  Serial.println(F("───────────────────────────────────────────────"));
  Serial.println(F("  Plotter: AO_filt | Baseline | Umbral | DET"));
  Serial.println(F("═══════════════════════════════════════════════\n"));

  // ── ESTABILIZACIÓN POST-CALIBRACIÓN (~2 seg) ─────────
  // Lee valores reales con el vidrio y re-ajusta el baseline
  Serial.println(F("  >> Estabilizando baseline..."));
  setRGB(0, 0, 255);  // Azul durante estabilización
  for (int i = 0; i < 200; i++) {  // 200 x 10ms = 2 seg
    int val = analogRead(PIN_TCRT_AO);
    int filt = addSampleAndFilter(val);
    baselineFloat = baselineFloat * 0.95f + filt * 0.05f;  // convergencia rápida
    aoBaseline = (int)baselineFloat;
    updateThresholds();
    delay(10);
  }
  Serial.print(F("  Baseline estable: ")); Serial.println(aoBaseline);
  Serial.print(F("  Umbral final:     < ")); Serial.println(aoThreshold);
  Serial.println(F("  >> Listo!\n"));
  delay(300);
}

// ══════════════════════════════════════════════════════════
void loop() {
  uint32_t now = millis();

  // ── Lectura analógica + filtro ──
  int  aoRaw      = analogRead(PIN_TCRT_AO);
  int  aoFiltered = addSampleAndFilter(aoRaw);
  bool dOut       = (digitalRead(PIN_TCRT_DO) == LOW);  // (informativo)

  // Detección: señal CAE por debajo del umbral
  if (!tcrtDetected && aoFiltered < aoThreshold) {
    tcrtDetected = true;
  } else if (tcrtDetected && aoFiltered > aoThresholdHigh) {
    tcrtDetected = false;
  }

  // Baseline adaptativo (solo cuando no hay objeto Y dentro de zona muerta)
  if (!tcrtDetected) {
    int deadzone = aoBaseline * BASELINE_DEADZONE / 100;
    if (abs(aoFiltered - aoBaseline) <= deadzone) {
      baselineFloat = baselineFloat * (1.0f - BASELINE_ALPHA) + aoFiltered * BASELINE_ALPHA;
      aoBaseline = (int)baselineFloat;
      updateThresholds();
    }
  }

  // ── LED RGB ──
  updateLedColor(aoFiltered, aoBaseline, aoThreshold);

  // ── IMPRIMIR ──
  if (now - lastPrintMs >= PRINT_EVERY_MS) {
    lastPrintMs = now;

    // SERIAL PLOTTER: 4 series
    Serial.print(aoFiltered);                     // 1 azul:    AO filtrado
    Serial.print(",");
    Serial.print(aoBaseline);                     // 2 naranja: baseline
    Serial.print(",");
    Serial.print(aoThreshold);                    // 3 rojo:    umbral
    Serial.print(",");
    Serial.println(tcrtDetected ? 4000 : 0);      // 4 verde:   detección

    // SERIAL MONITOR (descomenta para texto legible):
    /*
    Serial.print("[TCRT] AO_raw="); Serial.print(aoRaw);
    Serial.print(" filt="); Serial.print(aoFiltered);
    Serial.print(" base="); Serial.print(aoBaseline);
    Serial.print(" umbral="); Serial.print(aoThreshold);
    Serial.print(" DET="); Serial.print(tcrtDetected ? "SI" : "no");
    Serial.print(" DO="); Serial.println(dOut ? "SI" : "no");
    */
  }

  delay(5);
}

/**
 * ═══════════════════════════════════════════════════════════
 *  PINES RESERVADOS POR PANEL HUB75
 * ═══════════════════════════════════════════════════════════
 *
 *   GPIO 25 → R1     GPIO 26 → G1
 *   GPIO 27 → B1     GPIO 14 → G2
 *   GPIO 12 → G2     GPIO 13 → B2
 *   GPIO 23 → A      GPIO 19 → B
 *   GPIO  5 → C      GPIO 17 → D
 *   GPIO 16 → CLK    GPIO  4 → LAT
 *   GPIO 15 → OE
 *
 * ═══════════════════════════════════════════════════════════
 *  PINES LIBRES DISPONIBLES
 * ═══════════════════════════════════════════════════════════
 *
 *   GPIO  2  ← B LED RGB (este sketch)
 *   GPIO 18  ← R LED RGB (este sketch)
 *   GPIO 21  ← G LED RGB (este sketch)
 *   GPIO 22  ← DO TCRT5000 (este sketch)
 *   GPIO 32  (ADC1_CH4, entrada)
 *   GPIO 33  ← AO TCRT5000 (este sketch)
 *   GPIO 34  (solo entrada, ADC1_CH6)
 *   GPIO 35  (solo entrada, ADC1_CH7)
 *   GPIO 36  (solo entrada, VP, ADC1_CH0)
 *   GPIO 39  (solo entrada, VN, ADC1_CH3)
 *
 * ═══════════════════════════════════════════════════════════
 */
