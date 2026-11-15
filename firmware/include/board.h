// Pin maps. Pick the board with -DBOARD_MODULES (default) or -DBOARD_PCB.
#pragma once

#if !defined(BOARD_MODULES) && !defined(BOARD_PCB)
#define BOARD_MODULES
#endif

// Common to both boards
#define PIN_PUMP_MICRO    16
#define PIN_PUMP_GROW     17
#define PIN_PUMP_BLOOM    26
#define PIN_PUMP_PH_DOWN  27
#define PIN_CIRCULATION   13
#define PIN_DS18B20        4
#define PIN_LEVEL_LOW     32   // float F1: open = add water
#define PIN_LEVEL_EMPTY   33   // float F2: open = tank empty, everything stops
#define PIN_I2C_SDA       21
#define PIN_I2C_SCL       22
#define PIN_LED_STATUS     2

#if defined(BOARD_MODULES)
// ESP32 dev kit + relay board + analog pH board + analog TDS module (docs/wiring.md)
#define BOARD_NAME        "modules"
#define PUMP_ON_LEVEL     LOW    // relay inputs are active low
#define PIN_TDS_POWER     25     // powers the TDS module only while measuring
#define PIN_BTN_PAUSE     -1
#define PIN_SENSE_12V     -1
#define ADS1115_ADDR      0x48
#define ADS_MUX_PH        4      // single-ended A0 (through the 3-resistor divider)
#define PH_VOLTS_MIN      -3.0f  // a reading outside this window means a disconnected or saturated input
#define PH_VOLTS_MAX      3.2f
#define ADS_MUX_EC        5      // single-ended A1
#define HAS_EC            1
#else
// Custom PCB v1.1 (docs/pcb.md). Never fabricated: untested.
#define BOARD_NAME        "pcb"
#define PUMP_ON_LEVEL     HIGH   // MOSFET gates
#define PIN_AUX           12     // strapping pin: must be low at boot
#define PIN_EC_A          25
#define PIN_EC_B          14
#define PIN_BTN_PAUSE     34     // pressed = LOW, pull-up on the board
#define PIN_SENSE_12V     35     // V12 = pin millivolts x 115 / 15
#define ADS1115_ADDR      0x49   // U7, the pH converter on the isolated island. U5 (0x48) is kept for EC and is not read yet
#define ADS_MUX_PH        0      // differential A0 - A1
#define PH_VOLTS_MIN      -0.6f  // the electrode gives +/-0.41 V over pH 0 to 14; the amplifier saturates near +/-1.65 V
#define PH_VOLTS_MAX      0.6f
#define HAS_EC            0      // the on-board EC front end needs characterising on real hardware first
#endif
