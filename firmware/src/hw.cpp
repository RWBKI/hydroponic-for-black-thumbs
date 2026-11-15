// Sensors and outputs.
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Wire.h>
#include <esp_timer.h>
#include "app.h"

using namespace tower;

static const int kPumpPins[kPumpCount] = {PIN_PUMP_MICRO, PIN_PUMP_GROW, PIN_PUMP_BLOOM, PIN_PUMP_PH_DOWN};
static OneWire           s_oneWire(PIN_DS18B20);
static DallasTemperature s_temp(&s_oneWire);
static esp_timer_handle_t s_killTimer = nullptr;
static bool s_adsOk = false;
#define ADS_STR2(x) #x
#define ADS_STR(x) ADS_STR2(x)
#define ADS_ADDR_TEXT ADS_STR(ADS1115_ADDR)

static inline void outWrite(int pin, bool on) { digitalWrite(pin, on ? PUMP_ON_LEVEL : !PUMP_ON_LEVEL); }

void allPumpsOff() {
  for (int i = 0; i < kPumpCount; i++) outWrite(kPumpPins[i], false);
}

// Backstop: fires even if the main loop is stuck while a pump is on.
static void killCb(void*) { allPumpsOff(); }

void waitMs(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) { feedLoopWDT(); webLoop(); delay(5); }
}

// ---- ADS1115 (single-shot, +/-4.096 V, 128 SPS) --------------------------------
static bool adsRead(uint8_t mux, float& volts) {
  uint16_t cfg = 0x8000 | ((uint16_t)mux << 12) | (0x1 << 9) | 0x0100 | (0x4 << 5) | 0x0003;
  Wire.beginTransmission(ADS1115_ADDR);
  Wire.write(0x01); Wire.write(cfg >> 8); Wire.write(cfg & 0xFF);
  if (Wire.endTransmission() != 0) return false;
  delay(10);
  Wire.beginTransmission(ADS1115_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((int)ADS1115_ADDR, 2) != 2) return false;
  int16_t raw = (int16_t)((Wire.read() << 8) | Wire.read());
  volts = raw * 4.096f / 32768.0f;
  return true;
}

static bool adsAverage(uint8_t mux, int n, float& volts) {
  float sum = 0;
  for (int i = 0; i < n; i++) { float v; if (!adsRead(mux, v)) return false; sum += v; }
  volts = sum / n;
  return true;
}

float readPhVolts() {
  float v = NAN;
  if (s_adsOk) adsAverage(ADS_MUX_PH, 16, v);
  return v;
}

static bool floatOpen(int pin) {           // open contact = float down (or a broken wire): the safe reading
  int open = 0;
  for (int i = 0; i < 5; i++) { if (digitalRead(pin) == HIGH) open++; delay(2); }
  return open >= 3;
}

void hwBegin() {
  // Outputs are driven to "off" before they become outputs, so nothing pulses at boot.
  for (int i = 0; i < kPumpCount; i++) { outWrite(kPumpPins[i], false); pinMode(kPumpPins[i], OUTPUT); outWrite(kPumpPins[i], false); }
  outWrite(PIN_CIRCULATION, false); pinMode(PIN_CIRCULATION, OUTPUT); outWrite(PIN_CIRCULATION, false);
#if defined(BOARD_PCB)
  digitalWrite(PIN_AUX, LOW); pinMode(PIN_AUX, OUTPUT);
  pinMode(PIN_EC_A, INPUT); pinMode(PIN_EC_B, INPUT);
#else
  digitalWrite(PIN_TDS_POWER, LOW); pinMode(PIN_TDS_POWER, OUTPUT);
#endif
  pinMode(PIN_LEVEL_LOW, INPUT_PULLUP);
  pinMode(PIN_LEVEL_EMPTY, INPUT_PULLUP);
  pinMode(PIN_LED_STATUS, OUTPUT);
  if (PIN_BTN_PAUSE >= 0) pinMode(PIN_BTN_PAUSE, INPUT);

  esp_timer_create_args_t args = {};
  args.callback = &killCb;
  args.name = "pumpkill";
  esp_timer_create(&args, &s_killTimer);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  // On the PCB the converter sits behind an isolator that answers only once its
  // isolated supply is up, so the first probe can fail. Retry for up to half a second.
  for (int i = 0; i < 25 && !s_adsOk; i++) {
    Wire.beginTransmission(ADS1115_ADDR);
    s_adsOk = (Wire.endTransmission() == 0);
    if (!s_adsOk) delay(20);
  }
  logLine(s_adsOk ? "ADS1115 found at " ADS_ADDR_TEXT : "ADS1115 NOT found at " ADS_ADDR_TEXT ": no pH or EC readings");
  s_temp.begin();
  s_temp.setWaitForConversion(false);
}

void setCirculation(bool on) { outWrite(PIN_CIRCULATION, on); }

bool pauseButtonPressed() {
  return PIN_BTN_PAUSE >= 0 && digitalRead(PIN_BTN_PAUSE) == LOW;
}

void measure() {
  g_busy = true;
  Measurement m;
  m.atS = nowS(); m.any = true;

  // Temperature first: EC needs it.
  s_temp.requestTemperatures();
  waitMs(800);
  float t = s_temp.getTempCByIndex(0);
  m.tempValid = (t > -20.0f && t < 60.0f && t != 85.0f);
  m.tempC = m.tempValid ? t : 25.0f;

#if HAS_EC
  // The TDS probe injects a small AC current that disturbs the pH electrode,
  // so it is powered only while it is read, and pH is read afterwards.
  digitalWrite(PIN_TDS_POWER, HIGH);
  waitMs(TDS_SETTLE_MS);
  float ve;
  bool ok = s_adsOk && adsAverage(ADS_MUX_EC, 16, ve);
  digitalWrite(PIN_TDS_POWER, LOW);
  if (ok) {
    m.ecVolts = ve;
    m.ec = ecFromTdsVolts(ve, m.tempC, g_cal.ecK);
    m.ecValid = m.tempValid && g_cal.ecDone && ve > 0.01f && ve < 3.0f;
  }
  waitMs(PH_QUIET_MS);
#endif

  float vp = readPhVolts();
  if (!isnan(vp)) {
    m.phVolts = vp;
    if (g_cal.phDone && phCalValid(g_cal.ph)) {
      m.ph = phFromVolts(vp, g_cal.ph);
      m.phValid = (vp > PH_VOLTS_MIN && vp < PH_VOLTS_MAX);   // outside: disconnected or saturated
    }
  }

  m.levelLow  = floatOpen(PIN_LEVEL_LOW);
  m.tankEmpty = floatOpen(PIN_LEVEL_EMPTY);

#if defined(BOARD_PCB)
  m.v12 = analogReadMilliVolts(PIN_SENSE_12V) / 1000.0f * 115.0f / 15.0f;
  m.v12Valid = true;
#endif

  g_meas = m;
  g_busy = false;
}

bool runPump(int idx, uint32_t ms, const char* why, bool stopIfEmpty) {
  if (idx < 0 || idx >= kPumpCount || ms == 0 || ms > MAX_PUMP_RUN_MS) return false;
  g_busy = true;
  g_stopRequested = false;
  esp_timer_stop(s_killTimer);
  esp_timer_start_once(s_killTimer, (uint64_t)(ms + 500) * 1000ULL);
  uint32_t t0 = millis();
  outWrite(kPumpPins[idx], true);
  bool completed = true;
  int emptyCount = 0;
  while (millis() - t0 < ms) {
    if (g_stopRequested) { completed = false; break; }
    // An automatic dose stops if the tank-empty float opens for about 100 ms.
    // Manual runs (bench calibration into a cup) ignore the floats.
    if (stopIfEmpty) { emptyCount = (digitalRead(PIN_LEVEL_EMPTY) == HIGH) ? emptyCount + 1 : 0; if (emptyCount >= 50) { completed = false; break; } }
    feedLoopWDT();
    webLoop();
    delay(2);
  }
  outWrite(kPumpPins[idx], false);
  esp_timer_stop(s_killTimer);
  uint32_t ran = millis() - t0;
  logLine("%s pump ran %lu ms (%s)%s", pumpName((Pump)idx), (unsigned long)ran, why, completed ? "" : " STOPPED EARLY");
  g_busy = false;
  return completed;
}
