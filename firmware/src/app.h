// Shared state between the hardware, storage and web layers.
#pragma once
#include <Arduino.h>
#include "board.h"
#include "config.h"
#include "convert.h"
#include "dosing.h"

struct Calibration {
  tower::PhCal ph;
  bool  phDone = false;
  float ecK = 1.0f;
  bool  ecDone = false;
  float pumpMlPerMin[tower::kPumpCount] = {0, 0, 0, 0};   // 0 = not calibrated
};

struct UserSettings {
  float tankL = 37.0f;
  float phHigh = 6.4f;
  float ecLow = 0.9f;
  float phStepMl = 0.5f;
  float topupMlPerL[3] = {0.13f, 0.20f, 0.07f};   // Terra Aquatica TriPart Micro, Grow, Bloom: about +0.2 EC
  bool  phDosing = true;
  bool  nutrientDosing = true;
  bool  circulation = false;                      // relay K5 / output 5 drives a 12 V circulation pump
};

struct Measurement {
  float phVolts = 0, ph = 0;       bool phValid = false;
  float ecVolts = 0, ec = 0;       bool ecValid = false;
  float tempC = 0;                 bool tempValid = false;
  bool  levelLow = false, tankEmpty = false;
  float v12 = 0;                   bool v12Valid = false;
  uint32_t atS = 0;                bool any = false;
};

extern Calibration       g_cal;
extern UserSettings      g_set;
extern Measurement       g_meas;
extern tower::Controller g_ctrl;
extern volatile bool     g_stopRequested;
extern bool              g_busy;          // a pump run or a measurement is in progress

uint32_t nowS();                          // monotonic seconds, does not wrap
void     logLine(const char* fmt, ...);
String   logJson();
void     applySettings();                 // push g_set into the controller
bool     readyToArm(String& why);

// hw.cpp
void hwBegin();
void measure();
bool runPump(int idx, uint32_t ms, const char* why, bool stopIfEmpty);
void allPumpsOff();
void setCirculation(bool on);
bool pauseButtonPressed();
void waitMs(uint32_t ms);                 // delay that keeps the web server and watchdog alive
float readPhVolts();

// store.cpp
void storeBegin();
void saveCalibration();
void saveSettings();
void saveRuntime();                       // mode, fault, 24 h history
bool loadWifi(String& ssid, String& pass);
void saveWifi(const String& ssid, const String& pass);

// web.cpp
void webBegin();
void webLoop();
bool takePendingPump(int& idx, uint32_t& ms);   // a manual run requested from the web page
