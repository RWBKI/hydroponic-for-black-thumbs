// Sensor conversions. Pure functions, tested on a computer.
#pragma once
#include <stdint.h>

namespace tower {

// Two-point pH calibration on the voltage seen by the ADC.
struct PhCal { float v1 = 1.667f; float ph1 = 6.86f; float v2 = 2.014f; float ph2 = 4.00f; };
bool  phCalValid(const PhCal& c);
float phFromVolts(float v, const PhCal& c);

// Analog TDS module: probe voltage to conductivity in mS/cm at 25 C.
// k is the single-point calibration factor (1.0 until calibrated).
float ecFromTdsVolts(float v, float tempC, float k);

// Temperature compensation to 25 C, about 2 % per degree.
float ecCompensate(float ecRaw, float tempC);

// Peristaltic pump: run time for a volume, from the calibrated flow.
// Returns 0 when the flow is not calibrated or the result is not plausible.
uint32_t pumpRunMs(float ml, float mlPerMin, uint32_t maxMs);

}  // namespace tower
