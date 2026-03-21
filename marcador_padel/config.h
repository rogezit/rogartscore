#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ══════════════════════════════════════════════════════════
//  PINES RESERVADOS POR PANEL HUB75 (NO USAR):
//  GPIO 25, 26, 2, 14, 12, 27, 23, 19, 5, 17, 16, 4, 15
//
//  IDC Pin 1  R1  → GPIO 25
//  IDC Pin 2  G1  → GPIO 26
//  IDC Pin 3  B1  → GPIO 2
//  IDC Pin 5  R2  → GPIO 14
//  IDC Pin 6  G2  → GPIO 12
//  IDC Pin 7  B2  → GPIO 27
//  IDC Pin 9  A   → GPIO 23
//  IDC Pin 10 B   → GPIO 19
//  IDC Pin 11 C   → GPIO 5
//  IDC Pin 12 D   → GPIO 17
//  IDC Pin 13 CLK → GPIO 16
//  IDC Pin 14 LAT → GPIO 4
//  IDC Pin 15 OE  → GPIO 15
//
//  GPIO 13 queda LIBRE
// ══════════════════════════════════════════════════════════

// ── Sensor 1 (Equipo A) ─────────────────────────────────
#define PIN_S1_DO     22    // TCRT5000 #1 digital  → GPIO22
#define PIN_S1_AO     33    // TCRT5000 #1 análogo  → GPIO33 (ADC1_CH5)

// ── Sensor 2 (Equipo B) ─────────────────────────────────
#define PIN_S2_DO     34    // TCRT5000 #2 digital  → GPIO34 (solo entrada)
#define PIN_S2_AO     32    // TCRT5000 #2 análogo  → GPIO32 (ADC1_CH4)

// ── LED RGB (ánodo común) ────────────────────────────────
#define PIN_LED_R     18
#define PIN_LED_G     21
#define PIN_LED_B     13

// ── Botón PULSE ──────────────────────────────────────────
//  GPIO 35: solo entrada, no tiene pull-up interno.
//  Conectar con resistencia pull-up externa de 10kΩ a 3.3V.
#define PIN_BTN_PULSE 35

// ── PWM config ───────────────────────────────────────────
#define PWM_FREQ      5000
#define PWM_RES       8

// ── Serial ───────────────────────────────────────────────
#define SERIAL_BAUD   115200

// ══════════════════════════════════════════════════════════
//  Parámetros de sensores TCRT5000
// ══════════════════════════════════════════════════════════
#define FILTER_SIZE       8
#define CALIB_SAMPLES     50
#define CALIB_DELAY_MS    60
#define THRESHOLD_PCT     5
#define HYSTERESIS_PCT    2
#define BASELINE_ALPHA    0.002f
#define BASELINE_DEADZONE 5

// ══════════════════════════════════════════════════════════
//  Tiempos de detección del sensor (ms)
// ══════════════════════════════════════════════════════════
//  Pala frente al sensor:
//    < 300 ms → ignorar (ruido / accidental)
//    300 ms – 1 s  →  +1 punto
//    ≥ 2 s  →  −1 punto (corrección)
//    ≥ 10 s →  reset marcador
#define SENSOR_TIME_MIN_MS       300
#define SENSOR_TIME_POINT_MS    1000
#define SENSOR_TIME_UNDO_MS     2000
#define SENSOR_TIME_RESET_MS   10000
#define SENSOR_DEBOUNCE_MS      200

// ══════════════════════════════════════════════════════════
//  Tiempos de botón PULSE (ms)
// ══════════════════════════════════════════════════════════
#define PULSE_LONG_MS   1500    // Largo → confirma modo

// ══════════════════════════════════════════════════════════
//  Modos de juego
// ══════════════════════════════════════════════════════════
enum GameMode : uint8_t {
  MODE_OFFICIAL    = 1,   // Partido oficial (deuce, best of 3)
  MODE_GOLDEN      = 2,   // Punto de oro (sin ventaja)
  MODE_AMERICANO   = 3,   // Americano/Mixto (games libres)
  MODE_TRAINING    = 4    // Entrenamiento (conteo libre)
};

#define NUM_MODES  4

// ══════════════════════════════════════════════════════════
//  Estados FSM del sistema
// ══════════════════════════════════════════════════════════
enum SystemState : uint8_t {
  STATE_SELECT_MODE,    // Selección de modo (PULSE cicla, PULSE largo confirma)
  STATE_PLAYING,        // En juego (sensores activos)
  STATE_GAME_OVER       // Partido terminado
};

// ══════════════════════════════════════════════════════════
//  Panel HUB75 P10 32×16
// ══════════════════════════════════════════════════════════
#define PANEL_WIDTH   32
#define PANEL_HEIGHT  16
#define PANELS_NUM    1

#define PIN_HUB_R1    25
#define PIN_HUB_G1    26
#define PIN_HUB_B1    2
#define PIN_HUB_R2    14
#define PIN_HUB_G2    12
#define PIN_HUB_B2    27
#define PIN_HUB_A     23
#define PIN_HUB_B     19
#define PIN_HUB_C     5
#define PIN_HUB_D     17
#define PIN_HUB_CLK   16
#define PIN_HUB_LAT   4
#define PIN_HUB_OE    15

// Layout panel (posiciones en px)
#define X_S0    0     // Set 1
#define X_S1    6     // Set 2
#define X_S2    12    // Set 3
#define X_SEP   18    // Separador vertical
#define X_PTS   20    // Inicio zona puntos
#define PTS_W   12    // Ancho zona puntos (20..31)

#endif
