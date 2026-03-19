/**
 * ╔══════════════════════════════════════════════════════════════╗
 * ║   TEST SENSORES IR · ESP32 DevKit v1 (38 pines)             ║
 * ║   v3 — Dos sensores distintos, cada uno con su tratamiento  ║
 * ╚══════════════════════════════════════════════════════════════╝
 *
 * ┌──────────────────────────────────────────────────────────────┐
 * │  SENSOR 1: TCRT5000  (módulo de 4 pines)                    │
 * │  Pines: VCC · GND · DO (digital) · AO (analógico)           │
 * │                                                              │
 * │  · Tiene un LED IR emisor y un fototransistor receptor.      │
 * │  · AO = señal analógica continua (0–4095). Cuanto más cerca  │
 * │    o más reflectante es el objeto, menor es el valor.        │
 * │  · DO = salida digital (HIGH/LOW) según el potenciómetro     │
 * │    azul del módulo. Gíralo para ajustar la distancia de      │
 * │    disparo.                                                  │
 * │  · CALIBRACIÓN POR SOFTWARE: se le aplica media móvil,       │
 * │    baseline adaptativo y umbral dinámico sobre la señal AO.  │
 * │  · Es el sensor que más datos da ← el más útil para score.  │
 * │                                                              │
 * │  SENSOR 2: Flying-Fish  (módulo de 3 pines)                 │
 * │  Pines: VCC · GND · OUT (digital)                            │
 * │                                                              │
 * │  · También tiene LED IR + fototransistor, pero solo da       │
 * │    salida digital: HIGH (nada) o LOW (objeto detectado).     │
 * │  · NO tiene salida analógica → no se puede hacer media       │
 * │    móvil ni baseline adaptativo por software.                │
 * │  · Su sensibilidad se ajusta SOLO con el potenciómetro       │
 * │    del módulo (si tu modelo lo tiene).                       │
 * │  · Se le aplica DEBOUNCE por software (ignora cambios       │
 * │    más rápidos que 30 ms) para evitar rebotes.               │
 * └──────────────────────────────────────────────────────────────┘
 *
 *  CONEXIÓN (todo del lado derecho del DevKit):
 *
 *   ESP32           Sensor
 *   ─────────────────────────────────────────────
 *   3.3V       →    VCC  (ambos sensores)
 *   GND        →    GND  (ambos sensores)
 *   GPIO19     ←    DO   TCRT5000 (digital, 4 pines)
 *   GPIO15     ←    AO   TCRT5000 (analógico, 4 pines)
 *   GPIO22     ←    OUT  Flying-Fish (digital, 3 pines)
 *   ─────────────────────────────────────────────
 *
 *  CALIBRACIÓN AL ENCENDER (~3 seg):
 *   → Solo aplica al TCRT5000 (señal analógica AO).
 *   → El sensor debe estar YA montado detrás del vidrio
 *     de la cancha, en su posición final.
 *   → Durante 3 seg no debe haber nada del otro lado
 *     del cristal (ni jugador, ni raqueta, ni pelota).
 *   → El Flying-Fish no necesita calibración por software,
 *     solo ajustar su potenciómetro manualmente.
 *
 *  Abre Serial Plotter en Arduino IDE a 115200 baud
 */

// ══════════════════════════════════════════════════════════
//  PINES
// ══════════════════════════════════════════════════════════
#define PIN_TCRT_DO   19    // TCRT5000 digital out  → GPIO19
#define PIN_TCRT_AO   15    // TCRT5000 analógico    → GPIO15 (ADC2_3)
#define PIN_FF_OUT    22    // Flying-Fish digital   → GPIO22

// ══════════════════════════════════════════════════════════
//  CONFIG GENERAL
// ══════════════════════════════════════════════════════════
#define SERIAL_BAUD     115200
#define PRINT_EVERY_MS  80

// ══════════════════════════════════════════════════════════
//  TCRT5000 — Filtro media móvil (solo para señal AO)
// ══════════════════════════════════════════════════════════
#define FILTER_SIZE     8           // muestras (potencia de 2)
int  aoBuffer[FILTER_SIZE];
int  bufferIndex = 0;
bool bufferFull  = false;

// ══════════════════════════════════════════════════════════
//  TCRT5000 — Calibración y umbral adaptativo (solo AO)
// ══════════════════════════════════════════════════════════
#define CALIB_SAMPLES     50        // muestras durante calibración (~3 seg)
#define CALIB_DELAY_MS    60        // delay entre muestras de calibración
#define THRESHOLD_PCT     30        // % de caída vs baseline = detección
#define HYSTERESIS_PCT    5         // % extra para soltar (anti-rebote)
#define BASELINE_ALPHA    0.002f    // adapt. lenta (~30 seg para ajustarse)

int   aoBaseline      = 0;         // baseline TCRT5000 "sin objeto"
int   aoThreshold     = 0;         // umbral dinámico de detección
int   aoThresholdHigh = 0;         // umbral para soltar (histéresis)
float baselineFloat   = 0.0f;

bool  tcrtDetected    = false;     // detección TCRT5000 (AO + histéresis)

// ══════════════════════════════════════════════════════════
//  FLYING-FISH — Debounce (solo digital, no hay analógico)
// ══════════════════════════════════════════════════════════
#define FF_DEBOUNCE_MS    30        // ignora cambios más rápidos que esto
bool     ffDetected       = false;  // estado estable del Flying-Fish
bool     ffLastRaw        = false;  // última lectura cruda
uint32_t ffLastChangeMs   = 0;      // timestamp del último cambio crudo

// ══════════════════════════════════════════════════════════
//  ESTADO COMBINADO
// ══════════════════════════════════════════════════════════
uint32_t lastPrintMs = 0;

// ──────────────────────────────────────────────────────────
//  FUNCIONES — TCRT5000
// ──────────────────────────────────────────────────────────

// Media móvil sobre la señal analógica AO del TCRT5000
int tcrt_addSampleAndFilter(int newVal) {
  aoBuffer[bufferIndex] = newVal;
  bufferIndex = (bufferIndex + 1) % FILTER_SIZE;
  if (bufferIndex == 0) bufferFull = true;

  int count = bufferFull ? FILTER_SIZE : bufferIndex;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += aoBuffer[i];
  return (int)(sum / count);
}

// Recalcula umbrales del TCRT5000 a partir del baseline
void tcrt_updateThresholds() {
  aoThreshold     = aoBaseline - (aoBaseline * THRESHOLD_PCT / 100);
  aoThresholdHigh = aoBaseline - (aoBaseline * (THRESHOLD_PCT - HYSTERESIS_PCT) / 100);
}

// ──────────────────────────────────────────────────────────
//  FUNCIONES — FLYING-FISH
// ──────────────────────────────────────────────────────────

// Debounce: solo acepta el cambio si se mantiene >30 ms
bool ff_debounce(bool rawReading, uint32_t now) {
  if (rawReading != ffLastRaw) {
    ffLastRaw = rawReading;
    ffLastChangeMs = now;
  }
  if ((now - ffLastChangeMs) >= FF_DEBOUNCE_MS) {
    ffDetected = ffLastRaw;
  }
  return ffDetected;
}

// ══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(400);

  pinMode(PIN_TCRT_DO, INPUT);
  pinMode(PIN_TCRT_AO, INPUT);
  pinMode(PIN_FF_OUT,  INPUT);

  analogReadResolution(12);       // 0–4095
  analogSetAttenuation(ADC_11db); // 0–3.3V

  Serial.println(F("\n═══════════════════════════════════════════════"));
  Serial.println(F("  TEST SENSORES IR v3"));
  Serial.println(F("  TCRT5000 (4pin) + Flying-Fish (3pin)"));
  Serial.println(F("═══════════════════════════════════════════════"));
  Serial.println(F("  TCRT5000: DO=GPIO19  AO=GPIO15"));
  Serial.println(F("  FlyFish:  OUT=GPIO22"));
  Serial.println(F("───────────────────────────────────────────────"));

  // ── CALIBRACIÓN TCRT5000 (solo AO, ~3 seg) ────────────
  // ¡El sensor debe estar detrás del vidrio ya montado!
  // No pongas nada del otro lado del cristal.
  Serial.println(F("  >> CALIBRANDO TCRT5000 (AO)..."));
  Serial.println(F("     Sensor detrás del vidrio, cancha vacía"));
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
  tcrt_updateThresholds();

  // Llena el buffer del filtro con el baseline
  for (int i = 0; i < FILTER_SIZE; i++) aoBuffer[i] = aoBaseline;
  bufferFull = true;

  Serial.println(F("───────────────────────────────────────────────"));
  Serial.println(F("  TCRT5000 calibrado:"));
  Serial.print(F("    Baseline:     ")); Serial.println(aoBaseline);
  Serial.print(F("    Rango calib:  ")); Serial.print(calibMin);
  Serial.print(F(" – ")); Serial.println(calibMax);
  Serial.print(F("    Ruido:        ")); Serial.println(calibMax - calibMin);
  Serial.print(F("    Umbral det:   < ")); Serial.println(aoThreshold);
  Serial.print(F("    Umbral soltar: > ")); Serial.println(aoThresholdHigh);
  Serial.println(F("───────────────────────────────────────────────"));
  Serial.println(F("  Flying-Fish: sin calibración SW"));
  Serial.println(F("    Ajusta su potenciómetro manualmente."));
  Serial.println(F("    Debounce = 30 ms activo."));
  Serial.println(F("───────────────────────────────────────────────"));
  Serial.println(F("  Plotter: AO_filt | Baseline | Umbral |"));
  Serial.println(F("           TCRT_det | FF_det"));
  Serial.println(F("═══════════════════════════════════════════════\n"));
  delay(500);
}

// ══════════════════════════════════════════════════════════
void loop() {
  uint32_t now = millis();

  // ── TCRT5000 (4 pines): lectura analógica + filtro ──
  int  aoRaw      = analogRead(PIN_TCRT_AO);
  int  aoFiltered = tcrt_addSampleAndFilter(aoRaw);
  bool tcrtDO     = (digitalRead(PIN_TCRT_DO) == LOW);  // (informativo)

  // Detección TCRT5000 con histéresis sobre AO filtrado
  if (!tcrtDetected && aoFiltered < aoThreshold) {
    tcrtDetected = true;
  } else if (tcrtDetected && aoFiltered > aoThresholdHigh) {
    tcrtDetected = false;
  }

  // Baseline adaptativo TCRT5000 (solo cuando no hay objeto)
  if (!tcrtDetected) {
    baselineFloat = baselineFloat * (1.0f - BASELINE_ALPHA) + aoFiltered * BASELINE_ALPHA;
    aoBaseline = (int)baselineFloat;
    tcrt_updateThresholds();
  }

  // ── FLYING-FISH (3 pines): lectura digital + debounce ──
  bool ffRaw = (digitalRead(PIN_FF_OUT) == LOW);
  ff_debounce(ffRaw, now);

  // ── IMPRIMIR ──
  if (now - lastPrintMs >= PRINT_EVERY_MS) {
    lastPrintMs = now;

    // SERIAL PLOTTER: 5 series
    Serial.print(aoFiltered);                     // 1 azul:    TCRT AO filtrado
    Serial.print(",");
    Serial.print(aoBaseline);                     // 2 naranja: TCRT baseline
    Serial.print(",");
    Serial.print(aoThreshold);                    // 3 rojo:    TCRT umbral
    Serial.print(",");
    Serial.print(tcrtDetected ? 4000 : 0);        // 4 verde:   TCRT detección
    Serial.print(",");
    Serial.println(ffDetected ? 3500 : 0);        // 5 morado:  FF detección

    // SERIAL MONITOR (descomenta para texto legible):
    /*
    Serial.print("[TCRT5000] AO_raw="); Serial.print(aoRaw);
    Serial.print(" filt="); Serial.print(aoFiltered);
    Serial.print(" base="); Serial.print(aoBaseline);
    Serial.print(" umbral="); Serial.print(aoThreshold);
    Serial.print(" DET="); Serial.print(tcrtDetected ? "SI" : "no");
    Serial.print(" DO="); Serial.print(tcrtDO ? "SI" : "no");
    Serial.print("  [FF] DET="); Serial.println(ffDetected ? "SI" : "no");
    */
  }

  delay(5);
}

/**
 * ═══════════════════════════════════════════════════════════
 *  DIFERENCIAS ENTRE TUS DOS SENSORES
 * ═══════════════════════════════════════════════════════════
 *
 *  TCRT5000 (4 pines: VCC, GND, DO, AO)
 *  ──────────────────────────────────────
 *  · Tiene salida ANALÓGICA (AO): valor continuo 0–4095
 *    que varía con la distancia/reflectancia del objeto.
 *  · Tiene salida DIGITAL (DO): HIGH/LOW según potenciómetro.
 *  · Podemos hacer calibración y filtrado por SOFTWARE
 *    sobre AO → más preciso y adaptable.
 *  · Ideal como sensor principal.
 *
 *  Flying-Fish (3 pines: VCC, GND, OUT)
 *  ──────────────────────────────────────
 *  · Solo tiene salida DIGITAL (OUT): HIGH o LOW.
 *  · No hay señal analógica → no se puede filtrar ni
 *    hacer baseline adaptativo por software.
 *  · Su sensibilidad se ajusta SOLO con el potenciómetro
 *    físico del módulo (gíralo con destornillador).
 *  · Le aplicamos DEBOUNCE (30 ms) para evitar que
 *    rebote entre ON/OFF rápidamente.
 *  · Útil como confirmación/segundo sensor.
 *
 * ═══════════════════════════════════════════════════════════
 *  MAPA LADO DERECHO DevKit 38 pines
 * ═══════════════════════════════════════════════════════════
 *
 *   GPIO23   libre
 *   GPIO22  ← OUT Flying-Fish (3 pines)
 *   GPIO1    no usar (TX)
 *   GPIO3    no usar (RX)
 *   GPIO21   libre
 *   GPIO19  ← DO  TCRT5000  (4 pines)
 *   GPIO18   libre
 *   GPIO5    libre
 *   GPIO17   libre
 *   GPIO16   libre
 *   GPIO4    libre
 *   GPIO2    libre
 *   GPIO15  ← AO  TCRT5000  (4 pines)
 *   GND     → GND sensores
 *   3.3V    → VCC sensores
 *
 * ═══════════════════════════════════════════════════════════
 *  TIPS PARA USO DETRÁS DE VIDRIO
 * ═══════════════════════════════════════════════════════════
 *  1. Calibra CON el vidrio ya montado. El cristal absorbe
 *     parte de la señal IR — el baseline lo captura.
 *  2. El baseline adaptativo del TCRT5000 se ajusta solo
 *     si el vidrio se ensucia o cambia la luz ambiental.
 *  3. El Flying-Fish: ajusta su potenciómetro CON el vidrio
 *     puesto hasta que solo dispare al acercar un objeto.
 *  4. Si el vidrio es muy grueso/tintado, acerca más el
 *     sensor al cristal o sube THRESHOLD_PCT a 40–50.
 *  5. Pon tubo negro (shrink tube) en el emisor/receptor
 *     del TCRT5000 para bloquear luz lateral del sol.
 * ═══════════════════════════════════════════════════════════
 */
