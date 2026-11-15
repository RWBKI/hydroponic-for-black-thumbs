# Firmware

ESP32 firmware for the tower controller. It reads pH, EC, water temperature and two float switches, serves a web page on your home network, and doses pH Down and nutrients with four peristaltic pumps.

## Status: never run on hardware

Read this before flashing.

| What | Verified how |
|---|---|
| Builds for the ESP32 | Compiled for both boards with the Arduino toolchain (core 2.0.17), no warnings in this code. The PlatformIO configuration itself has not been run |
| Dosing rules and safeguards | 182 automated checks on a computer, all passing (`test/host`) |
| Web page | Exercised in a browser against a simulated controller |
| Sensors, pumps, Wi-Fi, storage on a real ESP32 | **Not tested.** No hardware was available when this was written |
| pH and EC accuracy | **Not tested.** Depends on your probes and your calibration |

Expect to find problems when you first power it. Test with plain water, never with acid, until every pump and reading behaves.

## What it does

- Measures every 30 seconds. The EC module is powered only while it is read, and pH is read afterwards, because the EC probe disturbs the pH electrode.
- Decides whether a dose is due: pH Down when pH stays above the threshold for 10 minutes, a nutrient top-up (Micro, then Grow, then Bloom, 5 minutes apart) when EC stays below the threshold for 30 minutes.
- Starts in **shadow mode**: every decision is written to the log and no pump runs. You arm real dosing from the web page, and only once the pH probe and all four pumps are calibrated.
- Never fills the tank. Low water only raises a notice.
- Remembers whether it was armed across a power cut. The 10-minute hold after power-up and the 24 h ceilings still apply.

## Safeguards

These are constants in `lib/core/dosing.h`. No setting or web request can raise them.

| Safeguard | Value |
|---|---|
| pH Down per 24 h | 6 doses and 6 mL at most |
| One pH Down dose | 1 mL at most (0.5 mL by default) |
| Nutrient top-ups per 24 h | 2 at most, 2 h apart |
| Rest after any dose | 20 minutes |
| After power-up | No dose for 10 minutes |
| Implausible reading | pH outside 4 to 9, EC outside 0.1 to 3, or a jump above 1.0 in 5 minutes: dosing locks until you clear the fault by hand |
| Tank empty (float F2) | Nothing doses, circulation stops |
| Longest pump run | 60 seconds, enforced by a hardware timer as well as the loop |
| Outputs at boot | Driven off before they are enabled |
| Stalled program | Watchdog reboot |
| After a reboot | Doses already given still count against the 24 h ceilings, and a latched fault stays latched |

Changing the tank water moves pH by more than 1.0 within minutes, so a refill locks dosing. That is intended: pause before maintenance, clear the fault afterwards.

## Build and flash

With [PlatformIO](https://platformio.org/):

```
cd firmware
pio run -t upload            # module version (default)
pio run -e pcb -t upload     # custom PCB
pio device monitor
```

The first flash is over USB. Unplug the 12 V supply while the USB cable is connected on the module version.

## First start

1. The controller creates a Wi-Fi network named `TowerController-XXXX`, password `towersetup`.
2. Join it and open `http://192.168.4.1`.
3. Enter your home Wi-Fi name and password. The controller restarts and joins your network.
4. Open `http://tower.local`, or the address printed on the serial monitor.

No Wi-Fi password is stored in the source code. If the controller cannot join your network it brings the setup network back.

## Calibration, in this order

1. **Pumps.** For each one: "Run 60 s" into a measuring cup of plain water, then enter the millilitres collected.
2. **pH.** Probe in the first buffer for a minute, save point 1. Rinse, second buffer, save point 2.
3. **EC.** Probe in a 1.413 mS/cm calibration solution, wait for a fresh reading, calibrate.
4. Compare the readings with your handheld meters for two weeks in shadow mode. Arm only when the log matches what you do by hand.

## Web interface and security

There is no login. Keep the controller on a home network you trust and never expose it to the internet. Commands are refused unless they carry an `X-Tower` header, which a page from another website cannot send.

| Endpoint | Purpose |
|---|---|
| `GET /api/status` | Readings, mode, calibration, settings, 24 h counters, log |
| `POST /api/mode` `armed=0\|1` | Shadow or armed |
| `POST /api/pause` `on=0\|1` | Pause dosing |
| `POST /api/fault/clear` | Clear a latched fault |
| `POST /api/cal/ph` `point=1\|2&ph=` | Save a pH calibration point |
| `POST /api/cal/ec` `ec=` | Calibrate EC against a known solution |
| `POST /api/pump/run` `i=0..3&s=` | Manual pump run, 60 s at most |
| `POST /api/pump/stop` | Stop any running pump |
| `POST /api/pump/cal` `i=&ml=&s=` | Save a pump's flow |
| `POST /api/settings` | Tank volume, thresholds, top-up doses |
| `POST /api/wifi` `ssid=&pass=` | Save Wi-Fi and restart |

## Layout

```
lib/core/     dosing rules and sensor maths, plain C++ with no hardware dependency
src/          hardware (hw.cpp), storage (store.cpp), web (web.cpp, page.h), main loop
include/      pin maps (board.h) and tunable parameters (config.h)
test/host/    tests of lib/core that run on a computer
```

## Run the tests

```
cd firmware
g++ -std=c++17 -Wall -Wextra -I lib/core lib/core/*.cpp test/host/run_tests.cpp -o run_tests && ./run_tests
```

## Known limits

- **Custom PCB (rev 1.1):** pH is read from the isolated ADS1115 at address 0x49. EC is not implemented: the on-board EC circuit has to be characterised on a real board first, so the PCB build doses pH only and leaves the EC pins floating. Read the [board's status](../docs/pcb.md) before building it.
- **EC on the module version** uses the response curve published for the common analog TDS board, scaled by one calibration point. Check it against a handheld meter across your working range.
- **pH** is not temperature compensated. Within 15 to 25 °C the error is small next to probe drift; calibrate monthly.
- **Time** is counted from power-up. There is no clock, so "24 h" means a rolling window, and after a reboot earlier doses are counted as if just given.
- Default top-up doses are for **Terra Aquatica TriPart**. Set your own from measurements for another nutrient line.
