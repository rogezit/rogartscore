/**
 * ╔══════════════════════════════════════════════════════════╗
 * ║   TEST SENSORES IR · ESP32 DevKit v1 (38 pines)         ║
 * ║   v2 — Umbral adaptativo + filtro media móvil           ║
 * ║   Resistente a cambios de luz ambiental                  ║
 * ╚══════════════════════════════════════════════════════════╝
 *
 *  CONEXIÓN (todo del lado derecho del DevKit):
 *
 *   ESP32           Sensor
 *   ─────────────────────────────────────────────
 *   3.3V       →    VCC  (ambos sensores)
 *   GND        →    GND  (ambos sensores)
 *   GPIO19     ←    DO   TCRT5000 (salida digital)
 *   GPIO15     ←    AO   TCRT5000 (salida analógica)
 *   GPIO22     ←    OUT  Flying-Fish
 *   ─────────────────────────────────────────────
 *
 *  MEJORAS v2:
 *   - Auto-calibración al encender (3 seg sin objeto)
 *   - Media móvil de 8 muestras para suavizar ruido
 *   - Baseline adaptativo que se ajusta lentamente a
 *     cambios de luz ambiental
 *   - Umbral dinámico = baseline - margen (% configurable)
 *   - Detección con histéresis para evitar rebotes
 *
 *  Abre Serial Plotter en Arduino IDE a 115200 baud
 */

// ── PINES ─────────────────────────────────────────────────
#define PIN_TCRT_DO   19    // TCRT5000 digital out  → GPIO19
#define PIN_TCRT_AO   15    // TCRT5000 analógico    → GPIO15 (ADC2_3)
#define PIN_FF_OUT    22    // Flying-Fish digital   → GPIO22

// ── CONFIG ────────────────────────────────────────────────
#define SERIAL_BAUD     115200
#define PRINT_EVERY_MS  80

// ── FILTRO MEDIA MÓVIL ────────────────────────────────────
#define FILTER_SIZE     8           // muestras para suavizar (potencia de 2)
int aoBuffer[FILTER_SIZE];
int bufferIndex = 0;
bool bufferFull = false;

// ── CALIBRACIÓN Y UMBRAL ADAPTATIVO ──────────────────────
#define CALIB_SAMPLES     50        // muestras durante calibración inicial
#define CALIB_DELAY_MS    60        // delay entre muestras de calibración
#define THRESHOLD_PCT     30        // % de caída respecto al baseline = detección
#define HYSTERESIS_PCT    5         // % extra para soltar (evita rebote)
#define BASELINE_ALPHA    0.002f    // velocidad de adaptación del baseline
                                    // (0.002 = muy lento, se adapta en ~30 seg)

int   aoBaseline      = 0;         // valor "sin objeto" actual
int   aoThreshold     = 0;         // umbral dinámico de detección
int   aoThresholdHigh = 0;         // umbral para "soltar" (histéresis)
float baselineFloat   = 0.0f;      // baseline con precisión float

bool  objectDetected  = false;     // estado de detección con histéresis

uint32_t lastPrintMs  = 0;

// ── FUNCIONES AUXILIARES ──────────────────────────────────

// Media móvil: agrega muestra y retorna promedio filtrado
int addSampleAndFilter(int newVal) {
  aoBuffer[bufferIndex] = newVal;
  bufferIndex = (bufferIndex + 1) % FILTER_SIZE;
  if (bufferIndex == 0) bufferFull = true;

  int count = bufferFull ? FILTER_SIZE : bufferIndex;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += aoBuffer[i];
  return (int)(sum / count);
}

// Recalcula umbrales a partir del baseline actual
void updateThresholds() {
  aoThreshold     = aoBaseline - (aoBaseline * THRESHOLD_PCT / 100);
  aoThresholdHigh = aoBaseline - (aoBaseline * (THRESHOLD_PCT - HYSTERESIS_PCT) / 100);
}

// ──────────────────────────────────────────────────────────
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(400);

  pinMode(PIN_TCRT_DO, INPUT);
  pinMode(PIN_TCRT_AO, INPUT);
  pinMode(PIN_FF_OUT,  INPUT);

  analogReadResolution(12);       // 0–4095
  analogSetAttenuation(ADC_11db); // rango 0–3.3V

  Serial.println(F("\n============================================"));
  Serial.println(F("  TEST SENSORES IR v2 - Umbral adaptativo"));
  Serial.println(F("============================================"));
  Serial.println(F("  TCRT5000  DO=GPIO19   AO=GPIO15"));
  Serial.println(F("  FlyFish   OUT=GPIO22"));
  Serial.println(F("--------------------------------------------"));

  // ── CALIBRACIÓN INICIAL (3 seg, sin objeto enfrente) ──
  Serial.println(F("  >> CALIBRANDO... no pongas nada frente"));
  Serial.println(F("     al sensor durante 3 segundos <<"));

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

  Serial.println(F("--------------------------------------------"));
  Serial.print(F("  Baseline calibrado: ")); Serial.println(aoBaseline);
  Serial.print(F("  Rango durante cal:  ")); Serial.print(calibMin);
  Serial.print(F(" - ")); Serial.println(calibMax);
  Serial.print(F("  Ruido (max-min):    ")); Serial.println(calibMax - calibMin);
  Serial.print(F("  Umbral detección:   < ")); Serial.println(aoThreshold);
  Serial.print(F("  Umbral soltar:      > ")); Serial.println(aoThresholdHigh);
  Serial.println(F("--------------------------------------------"));
  Serial.println(F("  Plotter: AO_filtrado | Baseline | Umbral | DO | FF"));
  Serial.println(F("============================================\n"));
  delay(500);
}

// ──────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // Lectura cruda + filtro media móvil
  int  aoRaw      = analogRead(PIN_TCRT_AO);
  int  aoFiltered = addSampleAndFilter(aoRaw);
  bool tcrtDO     = (digitalRead(PIN_TCRT_DO) == LOW);
  bool ffOut      = (digitalRead(PIN_FF_OUT)  == LOW);

  // ── DETECCIÓN CON HISTÉRESIS ──
  if (!objectDetected && aoFiltered < aoThreshold) {
    objectDetected = true;
  } else if (objectDetected && aoFiltered > aoThresholdHigh) {
    objectDetected = false;
  }

  // ── BASELINE ADAPTATIVO ──
  // Solo actualiza cuando NO hay objeto detectado
  // para que el baseline represente siempre "vacío"
  if (!objectDetected) {
    baselineFloat = baselineFloat * (1.0f - BASELINE_ALPHA) + aoFiltered * BASELINE_ALPHA;
    aoBaseline = (int)baselineFloat;
    updateThresholds();
  }

  // ── IMPRIMIR ──
  if (now - lastPrintMs >= PRINT_EVERY_MS) {
    lastPrintMs = now;

    // SERIAL PLOTTER: 5 líneas
    Serial.print(aoFiltered);                   // azul:  AO filtrado
    Serial.print(",");
    Serial.print(aoBaseline);                   // naranja: baseline adaptativo
    Serial.print(",");
    Serial.print(aoThreshold);                  // rojo:  umbral de detección
    Serial.print(",");
    Serial.print(objectDetected ? 4000 : 0);    // verde: detección combinada
    Serial.print(",");
    Serial.println(ffOut ? 3500 : 0);           // morado: Flying-Fish

    // SERIAL MONITOR (descomenta para texto legible):
    /*
    Serial.print("AO_raw="); Serial.print(aoRaw);
    Serial.print(" AO_filt="); Serial.print(aoFiltered);
    Serial.print(" base="); Serial.print(aoBaseline);
    Serial.print(" umbral="); Serial.print(aoThreshold);
    Serial.print(" DET="); Serial.print(objectDetected ? "SI" : "no");
    Serial.print(" DO="); Serial.print(tcrtDO ? "SI" : "no");
    Serial.print(" FF="); Serial.println(ffOut ? "SI" : "no");
    */
  }

  delay(5);
}

/**
 * =====================================================
 *  MAPA LADO DERECHO DevKit 38 pines (de arriba abajo)
 * =====================================================
 *
 *   GPIO23   libre
 *   GPIO22  ← OUT Flying-Fish
 *   GPIO1    no usar (TX)
 *   GPIO3    no usar (RX)
 *   GPIO21   libre
 *   GPIO19  ← DO  TCRT5000 digital
 *   GPIO18   libre
 *   GPIO5    libre
 *   GPIO17   libre
 *   GPIO16   libre
 *   GPIO4    libre
 *   GPIO2    libre
 *   GPIO15  ← AO  TCRT5000 analógico (ADC2_3)
 *   GND     → GND sensores
 *   3.3V    → VCC sensores
 *
 * =====================================================
 *  TIPS CONTRA VARIACIÓN DE LUZ AMBIENTAL
 * =====================================================
 *  1. El baseline se auto-ajusta — no necesitas tocar
 *     el código al cambiar de lugar/iluminación.
 *  2. Si sigue muy sensible a la luz, sube THRESHOLD_PCT
 *     (ej: 40 o 50) para exigir más caída.
 *  3. Si hay falsos positivos, sube HYSTERESIS_PCT (ej: 10).
 *  4. Si oscila mucho, sube FILTER_SIZE a 16.
 *  5. Pon un tubo negro (shrink tube) alrededor del
 *     emisor y receptor IR del TCRT5000 para bloquear
 *     luz lateral — es la mejora hardware más efectiva.
 *  6. Ajusta el potenciómetro azul del TCRT5000 para
 *     que DO solo cambie al acercar un objeto.
 * =====================================================
 */
