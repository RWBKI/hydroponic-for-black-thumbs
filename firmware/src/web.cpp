// Wi-Fi setup and the web interface.
//
// There is no login: keep the controller on a home network you trust and never
// expose it to the internet. Every command must carry an X-Tower header, which
// a web page from another site cannot send, so a stray link cannot run a pump.
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include "app.h"
#include "page.h"

using namespace tower;

static WebServer s_server(80);
static DNSServer s_dns;
static bool s_ap = false;
static int s_pendIdx = -1;
static uint32_t s_pendMs = 0;

bool takePendingPump(int& idx, uint32_t& ms) {
  if (s_pendIdx < 0) return false;
  idx = s_pendIdx; ms = s_pendMs; s_pendIdx = -1;
  return true;
}

static String num(float v, int digits = 2) { return (isnan(v) || isinf(v)) ? String("null") : String(v, digits); }
static const char* tf(bool b) { return b ? "true" : "false"; }
static float argF(const char* name, float fallback) { return s_server.hasArg(name) ? s_server.arg(name).toFloat() : fallback; }
static void reply(int code, const String& msg) { s_server.send(code, "application/json", String("{\"ok\":") + tf(code == 200) + ",\"msg\":\"" + msg + "\"}"); }
static bool guard() {
  if (!s_server.hasHeader("X-Tower")) { reply(403, "missing X-Tower header"); return false; }
  return true;
}

static void handleStatus() {
  uint32_t t = nowS();
  History h = g_ctrl.history(t);
  const Measurement& m = g_meas;
  String s;
  s.reserve(4096);
  s += "{\"board\":\""; s += BOARD_NAME; s += "\",\"uptime\":"; s += t;
  s += ",\"mode\":\""; s += (g_ctrl.mode() == Mode::Armed ? "armed" : "shadow");
  s += "\",\"paused\":"; s += tf(g_ctrl.paused());
  s += ",\"fault\":\""; s += faultName(g_ctrl.fault());
  s += "\",\"locked\":"; s += tf(g_ctrl.lockedOut());
  s += ",\"busy\":"; s += tf(g_busy);
  s += ",\"hasEc\":"; s += tf(HAS_EC);
  s += ",\"meas\":{\"any\":"; s += tf(m.any); s += ",\"age\":"; s += (m.any ? t - m.atS : 0);
  s += ",\"ph\":"; s += num(m.ph); s += ",\"phV\":"; s += num(m.phVolts, 4); s += ",\"phValid\":"; s += tf(m.phValid);
  s += ",\"ec\":"; s += num(m.ec); s += ",\"ecV\":"; s += num(m.ecVolts, 4); s += ",\"ecValid\":"; s += tf(m.ecValid);
  s += ",\"temp\":"; s += num(m.tempC, 1); s += ",\"tempValid\":"; s += tf(m.tempValid);
  s += ",\"levelLow\":"; s += tf(m.levelLow); s += ",\"tankEmpty\":"; s += tf(m.tankEmpty);
  s += ",\"v12\":"; s += (m.v12Valid ? num(m.v12, 1) : String("null"));
  s += "},\"cal\":{\"phDone\":"; s += tf(g_cal.phDone);
  s += ",\"ph1\":"; s += num(g_cal.ph.ph1); s += ",\"v1\":"; s += num(g_cal.ph.v1, 4);
  s += ",\"ph2\":"; s += num(g_cal.ph.ph2); s += ",\"v2\":"; s += num(g_cal.ph.v2, 4);
  s += ",\"ecDone\":"; s += tf(g_cal.ecDone); s += ",\"ecK\":"; s += num(g_cal.ecK, 3);
  s += ",\"flow\":[";
  for (int i = 0; i < kPumpCount; i++) { if (i) s += ','; s += num(g_cal.pumpMlPerMin[i], 1); }
  s += "]},\"set\":{\"tankL\":"; s += num(g_set.tankL, 1); s += ",\"phHigh\":"; s += num(g_set.phHigh);
  s += ",\"ecLow\":"; s += num(g_set.ecLow); s += ",\"phStepMl\":"; s += num(g_set.phStepMl);
  s += ",\"topup\":[";
  for (int i = 0; i < 3; i++) { if (i) s += ','; s += num(g_set.topupMlPerL[i], 3); }
  s += "],\"phDosing\":"; s += tf(g_set.phDosing); s += ",\"nutrientDosing\":"; s += tf(g_set.nutrientDosing);
  s += ",\"circulation\":"; s += tf(g_set.circulation);
  s += "},\"hist\":{\"phDoses\":"; s += h.phDoses; s += ",\"phMl\":"; s += num(h.phMl, 1); s += ",\"topups\":"; s += h.topups;
  s += "},\"limits\":{\"phDoses\":"; s += Limits::kMaxPhDosesPer24h; s += ",\"phMl\":"; s += num(Limits::kMaxPhDownMlPer24h, 1);
  s += ",\"topups\":"; s += Limits::kMaxTopupsPer24h;
  uint32_t np = g_ctrl.nextPhAllowedS(), nt = g_ctrl.nextTopupAllowedS();
  s += "},\"wait\":{\"ph\":"; s += (np > t ? np - t : 0); s += ",\"topup\":"; s += (nt > t ? nt - t : 0);
  s += "},\"wifi\":{\"ap\":"; s += tf(s_ap); s += ",\"ip\":\""; s += (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
  s += "\"},\"log\":"; s += logJson(); s += "}";
  s_server.send(200, "application/json", s);
}

static void handleSettings() {
  if (!guard()) return;
  UserSettings n = g_set;
  n.tankL = argF("tankL", n.tankL);
  n.phHigh = argF("phHigh", n.phHigh);
  n.ecLow = argF("ecLow", n.ecLow);
  n.phStepMl = argF("phStepMl", n.phStepMl);
  n.topupMlPerL[0] = argF("top0", n.topupMlPerL[0]);
  n.topupMlPerL[1] = argF("top1", n.topupMlPerL[1]);
  n.topupMlPerL[2] = argF("top2", n.topupMlPerL[2]);
  if (s_server.hasArg("phDosing")) n.phDosing = s_server.arg("phDosing") == "1";
  if (s_server.hasArg("nutrientDosing")) n.nutrientDosing = s_server.arg("nutrientDosing") == "1";
  if (s_server.hasArg("circulation")) n.circulation = s_server.arg("circulation") == "1";
  if (!(n.tankL >= 5 && n.tankL <= 500)) return reply(400, "tank volume must be 5 to 500 L");
  if (!(n.phHigh >= 6.0f && n.phHigh <= 7.5f)) return reply(400, "pH threshold must be 6.0 to 7.5");
  if (!(n.ecLow >= 0.3f && n.ecLow <= 2.5f)) return reply(400, "EC threshold must be 0.3 to 2.5 mS/cm");
  if (!(n.phStepMl > 0 && n.phStepMl <= Limits::kMaxPhStepMl)) return reply(400, "pH step must be above 0 and at most 1 mL");
  for (int i = 0; i < 3; i++) if (!(n.topupMlPerL[i] >= 0 && n.topupMlPerL[i] * n.tankL <= Limits::kMaxTopupMlPerPart)) return reply(400, "top-up dose too large for this tank");
  g_set = n; applySettings(); saveSettings();
  logLine("Settings changed");
  reply(200, "saved");
}

static void handleMode() {
  if (!guard()) return;
  bool arm = s_server.arg("armed") == "1";
  if (arm) { String why; if (!readyToArm(why)) return reply(400, "cannot arm: " + why); }
  g_ctrl.setMode(arm ? Mode::Armed : Mode::Shadow);
  saveRuntime();
  logLine(arm ? "ARMED: automatic dosing is now real" : "Shadow mode: decisions are logged, no pump runs");
  reply(200, arm ? "armed" : "shadow");
}

static void handlePause() {
  if (!guard()) return;
  g_ctrl.setPaused(s_server.arg("on") == "1");
  logLine(g_ctrl.paused() ? "Paused from the web page" : "Resumed from the web page");
  reply(200, g_ctrl.paused() ? "paused" : "resumed");
}

static void handleFaultClear() {
  if (!guard()) return;
  g_ctrl.clearFault(nowS());
  saveRuntime();
  logLine("Fault cleared by the user");
  reply(200, "cleared");
}

static void handleCalPh() {
  if (!guard()) return;
  if (g_busy) return reply(409, "busy, try again in a few seconds");
  int point = s_server.arg("point").toInt();
  float ph = argF("ph", NAN);
  if ((point != 1 && point != 2) || !(ph >= 1 && ph <= 13)) return reply(400, "need point=1 or 2 and the buffer pH");
  float v = readPhVolts();
  if (isnan(v)) return reply(500, "no reading from the ADC");
  if (point == 1) { g_cal.ph.v1 = v; g_cal.ph.ph1 = ph; } else { g_cal.ph.v2 = v; g_cal.ph.ph2 = ph; }
  g_cal.phDone = phCalValid(g_cal.ph);
  saveCalibration();
  logLine("pH calibration point %d: pH %.2f at %.4f V%s", point, ph, v, g_cal.phDone ? "" : " (second point still needed)");
  reply(200, g_cal.phDone ? "pH calibration complete" : "point saved, now do the other buffer");
}

static void handleCalEc() {
  if (!guard()) return;
  float known = argF("ec", NAN);
  if (!(known >= 0.2f && known <= 5.0f)) return reply(400, "enter the known EC in mS/cm (0.2 to 5)");
  if (!g_meas.any || !g_meas.tempValid || g_meas.ecVolts < 0.01f) return reply(400, "no EC reading yet: probe in the solution, wait for the next measurement");
  float raw = ecFromTdsVolts(g_meas.ecVolts, g_meas.tempC, 1.0f);
  if (raw < 0.05f) return reply(400, "reading too low to calibrate");
  float k = known / raw;
  if (!(k > 0.3f && k < 3.0f)) return reply(400, "calibration factor out of range: check the probe and the solution");
  g_cal.ecK = k; g_cal.ecDone = true;
  saveCalibration();
  logLine("EC calibrated: factor %.3f", k);
  reply(200, "EC calibrated");
}

static void handlePumpRun() {
  if (!guard()) return;
  int i = s_server.arg("i").toInt();
  float secs = argF("s", 0);
  if (i < 0 || i >= kPumpCount || !(secs >= 0.5f && secs <= MAX_PUMP_RUN_MS / 1000.0f)) return reply(400, "pump 0 to 3, 0.5 to 60 s");
  if (g_busy || s_pendIdx >= 0) return reply(409, "busy, try again in a few seconds");
  s_pendIdx = i; s_pendMs = (uint32_t)(secs * 1000.0f);
  reply(200, "pump started");
}

static void handlePumpStop() {
  if (!guard()) return;
  g_stopRequested = true; s_pendIdx = -1;
  reply(200, "stop requested");
}

static void handlePumpCal() {
  if (!guard()) return;
  int i = s_server.arg("i").toInt();
  float ml = argF("ml", 0), secs = argF("s", 60);
  if (i < 0 || i >= kPumpCount || !(ml > 0.5f && ml < 500) || !(secs >= 10 && secs <= 60)) return reply(400, "need the pump, the mL collected and the run time");
  float flow = ml / secs * 60.0f;
  if (!(flow >= 1 && flow <= 500)) return reply(400, "flow out of range");
  g_cal.pumpMlPerMin[i] = flow;
  saveCalibration();
  logLine("%s pump calibrated: %.1f mL/min", pumpName((Pump)i), flow);
  reply(200, "pump calibrated");
}

static void handleWifi() {
  if (!guard()) return;
  String ssid = s_server.arg("ssid"), pass = s_server.arg("pass");
  if (ssid.length() < 1 || ssid.length() > 32 || pass.length() > 64) return reply(400, "check the network name and password");
  saveWifi(ssid, pass);
  reply(200, "saved, restarting");
  allPumpsOff();
  delay(500);
  ESP.restart();
}

static void startAp() {
  uint8_t mac[6]; WiFi.macAddress(mac);
  char name[32]; snprintf(name, sizeof name, "TowerController-%02X%02X", mac[4], mac[5]);
  WiFi.softAP(name, AP_PASSWORD);
  s_dns.start(53, "*", WiFi.softAPIP());
  s_ap = true;
  logLine("Setup network %s, open http://%s", name, WiFi.softAPIP().toString().c_str());
}

void webBegin() {
  WiFi.persistent(false);
  String ssid, pass;
  bool have = loadWifi(ssid, pass);
  if (have) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(MDNS_NAME);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), pass.c_str());
    for (int i = 0; i < 80 && WiFi.status() != WL_CONNECTED; i++) delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) logLine("Wi-Fi connected: http://%s or http://%s.local", WiFi.localIP().toString().c_str(), MDNS_NAME);
  else { WiFi.mode(have ? WIFI_AP_STA : WIFI_AP); startAp(); }   // dosing does not depend on Wi-Fi
  MDNS.begin(MDNS_NAME);

  static const char* headers[] = {"X-Tower"};
  s_server.collectHeaders(headers, 1);
  s_server.on("/", HTTP_GET, []() { s_server.send_P(200, "text/html", PAGE_HTML); });
  s_server.on("/api/status", HTTP_GET, handleStatus);
  s_server.on("/api/settings", HTTP_POST, handleSettings);
  s_server.on("/api/mode", HTTP_POST, handleMode);
  s_server.on("/api/pause", HTTP_POST, handlePause);
  s_server.on("/api/fault/clear", HTTP_POST, handleFaultClear);
  s_server.on("/api/cal/ph", HTTP_POST, handleCalPh);
  s_server.on("/api/cal/ec", HTTP_POST, handleCalEc);
  s_server.on("/api/pump/run", HTTP_POST, handlePumpRun);
  s_server.on("/api/pump/stop", HTTP_POST, handlePumpStop);
  s_server.on("/api/pump/cal", HTTP_POST, handlePumpCal);
  s_server.on("/api/wifi", HTTP_POST, handleWifi);
  s_server.onNotFound([]() {
    if (s_ap) { s_server.sendHeader("Location", "/"); s_server.send(302, "text/plain", ""); }
    else s_server.send(404, "text/plain", "not found");
  });
  s_server.begin();
}

void webLoop() {
  static bool inside = false;              // handlers can wait; never re-enter the server
  if (inside) return;
  inside = true;
  if (s_ap) s_dns.processNextRequest();
  s_server.handleClient();
  inside = false;
}
