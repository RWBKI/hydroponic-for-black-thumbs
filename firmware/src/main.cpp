// Hydroponic tower controller.
//
// Reads pH, EC, water temperature and two float switches, serves a web page,
// and doses pH Down and nutrients with peristaltic pumps.
//
// Out of the box the controller is in SHADOW mode: it takes every dosing
// decision and writes it to the log, but runs no pump. Real dosing starts
// only after someone arms it from the web page.
//
// This firmware has not been run on hardware yet. See firmware/README.md.
#include <Arduino.h>
#include <esp_timer.h>
#include <stdarg.h>
#include "app.h"

using namespace tower;

Calibration   g_cal;
UserSettings  g_set;
Measurement   g_meas;
Controller    g_ctrl;
volatile bool g_stopRequested = false;
bool          g_busy = false;

uint32_t nowS() { return (uint32_t)(esp_timer_get_time() / 1000000LL); }

// ---- log: a small ring buffer shown on the web page ------------------------------
struct LogEntry { uint32_t t; char text[96]; };
static LogEntry s_log[LOG_LINES];
static int s_logHead = 0, s_logCount = 0;

void logLine(const char* fmt, ...) {
  LogEntry& e = s_log[s_logHead];
  e.t = nowS();
  va_list ap; va_start(ap, fmt); vsnprintf(e.text, sizeof e.text, fmt, ap); va_end(ap);
  for (char* p = e.text; *p; p++) if (*p == '"' || *p == '\\' || (unsigned char)*p < 0x20) *p = ' ';
  Serial.printf("[%lu] %s\n", (unsigned long)e.t, e.text);
  s_logHead = (s_logHead + 1) % LOG_LINES;
  if (s_logCount < LOG_LINES) s_logCount++;
}

String logJson() {                         // newest first
  String s = "[";
  for (int i = 0; i < s_logCount; i++) {
    const LogEntry& e = s_log[(s_logHead - 1 - i + 2 * LOG_LINES) % LOG_LINES];
    if (i) s += ',';
    s += "{\"t\":"; s += e.t; s += ",\"m\":\""; s += e.text; s += "\"}";
  }
  return s + "]";
}

void applySettings() {
  Settings s;
  s.phHigh = g_set.phHigh;
  s.ecLow = g_set.ecLow;
  s.phStepMl = g_set.phStepMl;
  for (int i = 0; i < 3; i++) s.topupMl[i] = g_set.topupMlPerL[i] * g_set.tankL;
  s.phDosing = g_set.phDosing;
  s.nutrientDosing = g_set.nutrientDosing && HAS_EC;
  g_ctrl.configure(s);
}

bool readyToArm(String& why) {
  if (!g_cal.phDone || !phCalValid(g_cal.ph)) { why = "calibrate the pH probe first"; return false; }
  for (int i = 0; i < kPumpCount; i++)
    if (g_cal.pumpMlPerMin[i] < 1.0f) { why = String("calibrate the ") + pumpName((Pump)i) + " pump first"; return false; }
  if (g_ctrl.lockedOut()) { why = "clear the fault first"; return false; }
  return true;
}

static void execute(const Action& a) {
  int idx = (int)a.pump;
  if (a.shadow) {
    logLine("SHADOW: would dose %.1f mL of %s (%s)", a.ml, pumpName(a.pump), a.reason);
  } else {
    uint32_t ms = pumpRunMs(a.ml, g_cal.pumpMlPerMin[idx], MAX_PUMP_RUN_MS);
    if (ms == 0) logLine("SKIPPED %.1f mL of %s: pump not calibrated or dose too large", a.ml, pumpName(a.pump));
    else { logLine("DOSING %.1f mL of %s (%s)", a.ml, pumpName(a.pump), a.reason); runPump(idx, ms, "automatic dose", true); }
  }
  saveRuntime();                           // the dose counts against the ceilings even after a reboot
}

void setup() {
  hwBegin();                               // outputs off before anything else
  Serial.begin(115200);
  logLine("Tower controller starting, board: %s", BOARD_NAME);
  storeBegin();
  logLine("Mode: %s%s", g_ctrl.mode() == Mode::Armed ? "ARMED" : "shadow", g_ctrl.lockedOut() ? ", FAULT latched" : "");
  webBegin();
  enableLoopWDT();                         // reboot if the loop stalls
}

void loop() {
  static uint32_t lastMeasure = 0;
  static bool first = true, lastEmpty = false, lastLow = false, lastBtn = false;
  static Fault lastFault = Fault::None;

  feedLoopWDT();
  webLoop();

  int pIdx; uint32_t pMs;
  if (takePendingPump(pIdx, pMs)) runPump(pIdx, pMs, "manual run", false);

  bool btn = pauseButtonPressed();
  if (btn && !lastBtn) { g_ctrl.setPaused(!g_ctrl.paused()); logLine(g_ctrl.paused() ? "Paused by button" : "Resumed by button"); }
  lastBtn = btn;

  if (first || millis() - lastMeasure >= MEASURE_PERIOD_MS) {
    first = false; lastMeasure = millis();
    measure();

    if (g_meas.tankEmpty != lastEmpty) { logLine(g_meas.tankEmpty ? "TANK EMPTY: all pumps stopped" : "Tank level restored"); lastEmpty = g_meas.tankEmpty; }
    if (g_meas.levelLow != lastLow) { logLine(g_meas.levelLow ? "Water level low: top up with plain water" : "Water level ok"); lastLow = g_meas.levelLow; }
    setCirculation(g_set.circulation && !g_meas.tankEmpty);

    Reading r;
    r.ph = g_meas.ph; r.phValid = g_meas.phValid;
    r.ec = g_meas.ec; r.ecValid = g_meas.ecValid;
    r.tankEmpty = g_meas.tankEmpty;
    Action a;
    if (g_ctrl.update(nowS(), r, a)) execute(a);

    if (g_ctrl.fault() != lastFault) {
      lastFault = g_ctrl.fault();
      if (lastFault != Fault::None) logLine("FAULT: %s. Dosing disabled until cleared", faultName(lastFault));
      saveRuntime();
    }
  }

  // Status LED: solid = armed, slow blink = shadow, fast blink = fault or tank empty.
  uint32_t ms = millis();
  bool alarm = g_ctrl.lockedOut() || g_meas.tankEmpty;
  digitalWrite(PIN_LED_STATUS, alarm ? (ms / 150) % 2 : (g_ctrl.mode() == Mode::Armed ? HIGH : (ms / 1000) % 2));
}
