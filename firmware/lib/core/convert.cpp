#include "convert.h"

namespace tower {

bool phCalValid(const PhCal& c) {
  float dv = c.v2 - c.v1, dp = c.ph2 - c.ph1;
  if (dv < 0) dv = -dv;
  if (dp < 0) dp = -dp;
  return dv > 0.05f && dp > 1.0f;        // points too close together give a useless slope
}

float phFromVolts(float v, const PhCal& c) {
  float slope = (c.ph2 - c.ph1) / (c.v2 - c.v1);
  return c.ph1 + slope * (v - c.v1);
}

float ecCompensate(float ecRaw, float tempC) {
  float f = 1.0f + 0.02f * (tempC - 25.0f);
  if (f < 0.5f) f = 0.5f;
  return ecRaw / f;
}

float ecFromTdsVolts(float v, float tempC, float k) {
  // Response curve published for the common analog TDS meter board (TDS in ppm
  // on the 0.5 scale); twice that is conductivity in uS/cm.
  float f = 1.0f + 0.02f * (tempC - 25.0f);
  if (f < 0.5f) f = 0.5f;
  float vc = v / f;
  float tds = (133.42f * vc * vc * vc - 255.86f * vc * vc + 857.39f * vc) * 0.5f;
  float ec = tds * 2.0f / 1000.0f;
  if (ec < 0) ec = 0;
  return ec * k;
}

uint32_t pumpRunMs(float ml, float mlPerMin, uint32_t maxMs) {
  if (ml <= 0 || mlPerMin < 1.0f) return 0;
  float ms = ml / mlPerMin * 60000.0f;
  if (ms > (float)maxMs) return 0;       // refuse rather than truncate: the dose would be wrong
  if (ms < 50.0f) return 0;              // too short to be repeatable
  return (uint32_t)(ms + 0.5f);
}

}  // namespace tower
