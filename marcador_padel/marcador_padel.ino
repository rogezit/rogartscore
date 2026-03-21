/**
 * ╔══════════════════════════════════════════════════════════════╗
 * ║   MARCADOR PÁDEL · ESP32 DevKit v1 (38 pines)               ║
 * ║   v1.0 — 2x TCRT5000 + Botón PULSE + LED RGB               ║
 * ╚══════════════════════════════════════════════════════════════╝
 *
 *  CONEXIÓN:
 *
 *   ESP32           Sensor 1 TCRT5000 (Equipo A)
 *   ─────────────────────────────────────────────
 *   3.3V       →    VCC
 *   GND        →    GND
 *   GPIO22     ←    DO  (digital)
 *   GPIO33     ←    AO  (analógico)
 *
 *   ESP32           Sensor 2 TCRT5000 (Equipo B)
 *   ─────────────────────────────────────────────
 *   3.3V       →    VCC
 *   GND        →    GND
 *   GPIO34     ←    DO  (digital)
 *   GPIO32     ←    AO  (analógico)
 *
 *   ESP32           Botón PULSE
 *   ─────────────────────────────────────────────
 *   GPIO35     ←    un terminal del botón
 *   GND        →    otro terminal del botón
 *   3.3V ──[10kΩ]── GPIO35  (pull-up externo)
 *
 *   ESP32           LED RGB (ánodo común)
 *   ─────────────────────────────────────────────
 *   3.3V       →    Ánodo (+) largo
 *   GPIO18     →    R  (con resistencia 220Ω)
 *   GPIO21     →    G  (con resistencia 220Ω)
 *   GPIO13     →    B  (con resistencia 220Ω)
 *
 *  PINES RESERVADOS POR PANEL HUB75 (NO USAR):
 *   GPIO 25, 26, 2, 14, 12, 27, 23, 19, 5, 17, 16, 4, 15
 *
 *  FSM:
 *   SEL_MODO → (PULSE cicla modos, PULSE largo confirma)
 *   EN_JUEGO → (sensores detectan puntos)
 *   FIN_PARTIDO → (parpadeo, PULSE largo → vuelve a SEL_MODO)
 *
 *  COMPILAR Y SUBIR:
 *   arduino-cli compile --fqbn esp32:esp32:esp32 marcador_padel/
 *   arduino-cli upload --fqbn esp32:esp32:esp32:UploadSpeed=460800 --port /dev/cu.usbserial-10 marcador_padel/
 *
 *  MONITOR SERIAL:
 *   arduino-cli monitor --port /dev/cu.usbserial-10 --config baudrate=115200
 */

#include "config.h"
#include "SensorIR.h"
#include "GameLogic.h"
#include "ButtonInput.h"
#include "Display.h"

// ── Instancias ───────────────────────────────────────────
SensorIR    sensorA;
SensorIR    sensorB;
GameLogic   game;
ButtonInput btnPulse;
Display     display;

// ── Estado FSM ───────────────────────────────────────────
SystemState state = STATE_SELECT_MODE;
GameMode    selectedMode = MODE_OFFICIAL;

// ══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(400);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Inicializar módulos
  sensorA.begin(PIN_S1_DO, PIN_S1_AO);
  sensorB.begin(PIN_S2_DO, PIN_S2_AO);
  btnPulse.begin(PIN_BTN_PULSE);
  display.begin();

  Serial.println(F("\n═══════════════════════════════════════════════"));
  Serial.println(F("  MARCADOR PÁDEL v1.0"));
  Serial.println(F("  Sensor A: DO=GPIO22  AO=GPIO33"));
  Serial.println(F("  Sensor B: DO=GPIO34  AO=GPIO32"));
  Serial.println(F("  Botón PULSE: GPIO35"));
  Serial.println(F("  LED RGB: R=GPIO18 G=GPIO21 B=GPIO2"));
  Serial.println(F("═══════════════════════════════════════════════"));

  // Calibración de sensores
  Serial.println(F("  >> Calibrando sensores..."));
  Serial.println(F("     No pongas nada delante."));
  display.setRGB(0, 0, 255);  // Azul durante calibración

  sensorA.calibrate();
  Serial.print(F("  Sensor A: baseline=")); Serial.print(sensorA.getBaseline());
  Serial.print(F("  umbral=<")); Serial.println(sensorA.getThreshold());

  sensorB.calibrate();
  Serial.print(F("  Sensor B: baseline=")); Serial.print(sensorB.getBaseline());
  Serial.print(F("  umbral=<")); Serial.println(sensorB.getThreshold());

  // Estabilización
  Serial.println(F("  >> Estabilizando..."));
  sensorA.stabilize();
  sensorB.stabilize();

  Serial.print(F("  Sensor A estable: ")); Serial.println(sensorA.getBaseline());
  Serial.print(F("  Sensor B estable: ")); Serial.println(sensorB.getBaseline());
  Serial.println(F("═══════════════════════════════════════════════"));

  // Entrar a selección de modo
  state = STATE_SELECT_MODE;
  selectedMode = MODE_OFFICIAL;
  Serial.println(F("\n  >> SELECCIONA MODO (PULSE corto = ciclar, PULSE largo = confirmar)"));
  Serial.println(F("[MODO] Seleccionado: 1 - Partido Oficial"));
  delay(300);
}

// ══════════════════════════════════════════════════════════
void loop() {
  PulseAction pulse = btnPulse.update();

  switch (state) {

    // ── SELECCIÓN DE MODO ──────────────────────────────
    case STATE_SELECT_MODE:
      display.showSelectMode(selectedMode);

      if (pulse == PULSE_SHORT) {
        // Ciclar modo 1→2→3→4→1...
        uint8_t m = (uint8_t)selectedMode;
        m = (m % NUM_MODES) + 1;
        selectedMode = (GameMode)m;
        Serial.print(F("[MODO] Seleccionado: "));
        switch (selectedMode) {
          case MODE_OFFICIAL:  Serial.println(F("1 - Partido Oficial")); break;
          case MODE_GOLDEN:    Serial.println(F("2 - Punto de Oro"));    break;
          case MODE_AMERICANO: Serial.println(F("3 - Americano/Mixto")); break;
          case MODE_TRAINING:  Serial.println(F("4 - Entrenamiento"));   break;
        }
      }

      if (pulse == PULSE_LONG) {
        // Confirma modo → inicia partido
        game.init(selectedMode);
        state = STATE_PLAYING;
        Serial.println(F("\n══════════════════════════════════════"));
        Serial.print(F("  PARTIDO INICIADO — Modo "));
        Serial.println((uint8_t)selectedMode);
        Serial.println(F("  Sensores activos. ¡A jugar!"));
        Serial.println(F("══════════════════════════════════════\n"));
        display.setRGB(0, 255, 0);
        display.showScore(game);
      }
      break;

    // ── EN JUEGO ───────────────────────────────────────
    case STATE_PLAYING: {
      display.showProximity(sensorA, sensorB);

      // Leer sensores
      SensorAction actA = sensorA.update();
      SensorAction actB = sensorB.update();

      // Sensor A (Equipo A)
      if (actA == ACTION_POINT) {
        Serial.println(F(">>> +1 PUNTO EQUIPO A"));
        game.addPoint(TEAM_A);
        game.printState();
        display.showScore(game);
        display.setRGB(128, 0, 255);  // morado flash
      } else if (actA == ACTION_UNDO) {
        Serial.println(F("<<< -1 PUNTO EQUIPO A (corrección)"));
        game.undoPoint(TEAM_A);
        game.printState();
        display.showScore(game);
        display.setRGB(255, 180, 0);  // amarillo flash
      } else if (actA == ACTION_RESET) {
        Serial.println(F("!!! RESET MARCADOR (Sensor A largo)"));
        game.resetSet();
        game.printState();
        display.showScore(game);
        display.setRGB(255, 0, 0);    // rojo flash
      }

      // Sensor B (Equipo B)
      if (actB == ACTION_POINT) {
        Serial.println(F(">>> +1 PUNTO EQUIPO B"));
        game.addPoint(TEAM_B);
        game.printState();
        display.showScore(game);
        display.setRGB(255, 0, 80);   // coral flash
      } else if (actB == ACTION_UNDO) {
        Serial.println(F("<<< -1 PUNTO EQUIPO B (corrección)"));
        game.undoPoint(TEAM_B);
        game.printState();
        display.showScore(game);
        display.setRGB(255, 180, 0);
      } else if (actB == ACTION_RESET) {
        Serial.println(F("!!! RESET MARCADOR (Sensor B largo)"));
        game.resetSet();
        game.printState();
        display.showScore(game);
        display.setRGB(255, 0, 0);
      }

      // ¿Terminó el partido?
      if (game.isGameOver()) {
        state = STATE_GAME_OVER;
        Team w = game.getWinner();
        Serial.println(F("\n══════════════════════════════════════"));
        Serial.print(F("  ¡¡¡ PARTIDO TERMINADO !!! Ganador: Equipo "));
        Serial.println(w == TEAM_A ? "A" : "B");
        game.printState();
        Serial.println(F("  PULSE largo → nuevo partido"));
        Serial.println(F("══════════════════════════════════════\n"));
      }

      // Botón PULSE durante juego: no hace nada (reservado futuro)
      break;
    }

    // ── FIN DE PARTIDO ─────────────────────────────────
    case STATE_GAME_OVER:
      display.showGameOver(game.getWinner(), game);

      if (pulse == PULSE_LONG) {
        // Volver a selección de modo
        state = STATE_SELECT_MODE;
        selectedMode = game.getMode();
        Serial.println(F("\n  >> NUEVO PARTIDO — Selecciona modo"));
      }
      break;
  }

  delay(5);
}
