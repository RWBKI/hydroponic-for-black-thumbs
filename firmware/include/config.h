// Tunable parameters. The dosing ceilings are NOT here: they are constants in
// lib/core/dosing.h and cannot be changed from the web page.
#pragma once

#define MEASURE_PERIOD_MS   30000UL
#define TDS_SETTLE_MS        1500UL   // TDS module on before sampling
#define PH_QUIET_MS          1000UL   // wait after the TDS module is off before reading pH
#define MAX_PUMP_RUN_MS     60000UL   // no single pump run is ever longer than this
#define AP_PASSWORD         "towersetup"
#define MDNS_NAME           "tower"   // http://tower.local
#define LOG_LINES           40
