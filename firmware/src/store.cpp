// Persistent storage (ESP32 NVS).
#include <Preferences.h>
#include "app.h"

using namespace tower;
static Preferences s_p;

void storeBegin() {
  s_p.begin("tower", true);
  g_cal.phDone = s_p.getBool("phDone", false);
  g_cal.ph.v1 = s_p.getFloat("phV1", g_cal.ph.v1);   g_cal.ph.ph1 = s_p.getFloat("phP1", g_cal.ph.ph1);
  g_cal.ph.v2 = s_p.getFloat("phV2", g_cal.ph.v2);   g_cal.ph.ph2 = s_p.getFloat("phP2", g_cal.ph.ph2);
  g_cal.ecDone = s_p.getBool("ecDone", false);
  g_cal.ecK = s_p.getFloat("ecK", 1.0f);
  for (int i = 0; i < kPumpCount; i++) { char k[8]; snprintf(k, sizeof k, "flow%d", i); g_cal.pumpMlPerMin[i] = s_p.getFloat(k, 0); }

  g_set.tankL = s_p.getFloat("tankL", g_set.tankL);
  g_set.phHigh = s_p.getFloat("phHigh", g_set.phHigh);
  g_set.ecLow = s_p.getFloat("ecLow", g_set.ecLow);
  g_set.phStepMl = s_p.getFloat("phStep", g_set.phStepMl);
  for (int i = 0; i < 3; i++) { char k[8]; snprintf(k, sizeof k, "top%d", i); g_set.topupMlPerL[i] = s_p.getFloat(k, g_set.topupMlPerL[i]); }
  g_set.phDosing = s_p.getBool("phDos", true);
  g_set.nutrientDosing = s_p.getBool("nutDos", true);
  g_set.circulation = s_p.getBool("circ", false);

  bool armed = s_p.getBool("armed", false);
  Fault f = (Fault)s_p.getUChar("fault", 0);
  History h;
  h.phDoses = s_p.getUChar("hPh", 0);
  h.phMl = s_p.getFloat("hPhMl", 0);
  h.topups = s_p.getUChar("hTop", 0);
  s_p.end();

  g_ctrl.begin(nowS());
  applySettings();
  g_ctrl.setMode(armed ? Mode::Armed : Mode::Shadow);
  g_ctrl.restoreHistory(nowS(), h);       // doses before the reboot still count against the ceilings
  g_ctrl.restoreFault(f);                 // a fault is cleared by a person, not by a power cycle
}

void saveCalibration() {
  s_p.begin("tower", false);
  s_p.putBool("phDone", g_cal.phDone);
  s_p.putFloat("phV1", g_cal.ph.v1); s_p.putFloat("phP1", g_cal.ph.ph1);
  s_p.putFloat("phV2", g_cal.ph.v2); s_p.putFloat("phP2", g_cal.ph.ph2);
  s_p.putBool("ecDone", g_cal.ecDone); s_p.putFloat("ecK", g_cal.ecK);
  for (int i = 0; i < kPumpCount; i++) { char k[8]; snprintf(k, sizeof k, "flow%d", i); s_p.putFloat(k, g_cal.pumpMlPerMin[i]); }
  s_p.end();
}

void saveSettings() {
  s_p.begin("tower", false);
  s_p.putFloat("tankL", g_set.tankL); s_p.putFloat("phHigh", g_set.phHigh); s_p.putFloat("ecLow", g_set.ecLow);
  s_p.putFloat("phStep", g_set.phStepMl);
  for (int i = 0; i < 3; i++) { char k[8]; snprintf(k, sizeof k, "top%d", i); s_p.putFloat(k, g_set.topupMlPerL[i]); }
  s_p.putBool("phDos", g_set.phDosing); s_p.putBool("nutDos", g_set.nutrientDosing); s_p.putBool("circ", g_set.circulation);
  s_p.end();
}

void saveRuntime() {
  History h = g_ctrl.history(nowS());
  s_p.begin("tower", false);
  s_p.putBool("armed", g_ctrl.mode() == Mode::Armed);
  s_p.putUChar("fault", (uint8_t)g_ctrl.fault());
  s_p.putUChar("hPh", h.phDoses); s_p.putFloat("hPhMl", h.phMl); s_p.putUChar("hTop", h.topups);
  s_p.end();
}

bool loadWifi(String& ssid, String& pass) {
  s_p.begin("wifi", true);
  ssid = s_p.getString("ssid", ""); pass = s_p.getString("pass", "");
  s_p.end();
  return ssid.length() > 0;
}

void saveWifi(const String& ssid, const String& pass) {
  s_p.begin("wifi", false);
  s_p.putString("ssid", ssid); s_p.putString("pass", pass);
  s_p.end();
}
