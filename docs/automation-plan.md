# Automation plan

The plan for a tower that measures itself, corrects itself and warns you: probes, a brain, dosing pumps, a camera. It is built in four tiers, each useful on its own, with the safeguards defined before any autonomy.

**Status: designed, not built.** Parts are ordered. Nothing here has been validated on the real tower yet.

## Two software routes

The plan was first written around **ESPHome + Home Assistant**: the ESP32 is configured rather than programmed, and Home Assistant provides history, graphs, phone notifications and an automation engine for free.

This repository implements the second route: a **standalone ESP32 firmware** that serves its own web page and needs no server. It is in [`firmware/`](../firmware/), matches the hardware in the [wiring guide](wiring.md), and has not been run on hardware yet.

The rules and safeguards below apply to both routes. The firmware implements the dosing rules, the suspect-probe lockout and the safeguards; the frost, photo and notification rules need a home automation server and are not in it.

## Do not buy a 7-in-1 soil probe

The cheap RS485 "7-in-1 NPK" probes are the classic trap for this project.

- They are soil probes. The spikes measure electrical properties between points in a soil matrix. In a liquid, half the numbers mean nothing.
- The NPK reading is fiction. There is no ion-selective electrode inside: the probe measures conductivity and multiplies by fixed coefficients. "N" moves exactly like EC.
- Metal spikes cannot measure pH. Real pH needs a glass electrode with a reference junction.
- Permanently immersed in nutrient solution, it fouls, corrodes and drifts. IP68 protects the electronics, not the measurements.

What works is one laboratory-style probe per quantity, and a brain that reads them.

## How it fits together

One idea to remember: **probes never drive a pump directly.** Everything goes through a brain that applies rules, and the rules have ceilings.

```
measure            decide                  act
probes  ──────▶  controller + rules  ──────▶  dosing pumps
                        ▲
camera + vision model ──┘   (looks, never touches)
```

## Four tiers

Prices are indicative, in euros, at the time of writing.

### 1. See (about €40)

- ESP32 dev board
- DS18B20 waterproof temperature probe. Required: it corrects pH and EC for temperature
- Float switch: the "low level" alert
- BME280 air sensor (temperature, humidity, pressure)
- BH1750 light sensor, to check the light estimates
- IP65 enclosure, cable, connectors, 5 V supply

Result: water temperature, level, light and air on your phone, with curves and alerts.

### 2. Measure (about €100)

- ADS1115 16-bit ADC. The ESP32's built-in ADC is non-linear and noisy; reading pH to 0.05 needs this
- pH: an analog pH board with a BNC glass electrode. The DFRobot Gravity pH V2 is the better option; a PH-4502C board works but drifts faster
- EC: a K=1 conductivity probe with its board (DFRobot Gravity EC V2), or an analog TDS module as the budget option
- EC calibration solution, 1413 µS/cm, plus pH 4 and 7 buffers

The professional route is Atlas Scientific EZO circuits (about €250 for pH and EC). They speak I2C directly. Not needed to start.

### 3. Act (about €70)

- 4 peristaltic dosing pumps, 12 V: one per bottle (pH Down, plus the three Terra Aquatica TriPart bottles: Micro, Grow, Bloom)
- 5 V opto-isolated relay board, or MOSFET outputs
- 12 V 3 A power supply
- Silicone tubing 3 × 5 mm
- A smart plug for the main circulation pump, to cut it on frost nights

Each tube goes into its original bottle. No pre-mixing, ever.

### 4. Look (about €15)

- An ESP32-CAM, or an old phone with an IP camera app
- A vision model called twice a day with the photo and the current readings: "this is the tower this morning, pH 6.3, EC 1.1, water 19 °C. What is wrong?"

Qualitative, but it catches wilting, aphids and discolouration early.

**Total: €220 to €260**, plus a Home Assistant host if you go that route.

## The rules

The dashboard's rule engine is the specification. The regime (recovery, cruise, winter) is a selector that changes targets and volumes.

All volumes are for Terra Aquatica TriPart (Micro, Grow, Bloom), as in the [growing guide](growing-guide.md#doses). With another nutrient line, keep the rules and re-measure the doses.

| Trigger | Condition | Action |
|---|---|---|
| pH too high | pH > 6.4 for 10 min, last dose more than 20 min ago, fewer than 6 doses today | Run the pH Down pump for the time that delivers 0.5 mL. Wait 20 min. Re-evaluate |
| EC too low | EC below the regime's low target for 30 min, last top-up more than 2 h ago, fewer than 2 top-ups today | Sequence: Micro, 5 min, Grow, 5 min, Bloom, each at its [top-up dose](growing-guide.md#top-up-dose-raises-ec-by-about-02). Then 2 h rest. Never all three at once |
| EC too high | EC above the high target | No dose. Notify: "top up with plain water" |
| Low level | Float switch triggered | Notify. Nothing automatic: an automatic fill that sticks open floods the floor |
| Warm water | Water above 24 °C | Notify |
| Frost forecast | Forecast below 2 °C tonight | Notify at 18:00 and cut the circulation pump from 23:00 to 08:00 |
| Suspect probe | pH < 4 or > 9, EC < 0.1 or > 3, a jump of more than 1.0 in 5 min, probe out of water | Automatic dosing disabled until re-armed by hand. Immediate notification |
| Photo | 08:00 and 19:00 | Capture, vision model with readings, summary as a notification. Nothing else |

## Safeguards before autonomy

An automated tower never dies from a lack of automation. It dies from a probe that drifts by 0.5 and a controller that confidently "corrects" until the tank is at pH 4.

1. **Shadow mode, two weeks minimum.** The system computes and logs what it would have dosed without pouring anything. You compare with what you do by hand. Real dosing is enabled only when its proposals and your actions match.
2. **Hard ceilings in the code, not in the settings.** 6 mL of pH Down per day, 2 top-ups per day. No loop can exceed them, whatever the reading.
3. **Dead band and rest time.** Never two doses less than 20 minutes apart. A probe needs minutes to settle; dosing on a moving reading means oscillating.
4. **An implausible reading stops everything.** Probe out of water, broken wire, absurd value: dosing is disabled and you are told. It never re-arms itself.
5. **Default is off.** Normally open relays, outputs low at boot. If the controller loses contact for ten minutes, pumps are stopped, not in an unknown state.
6. **Monthly calibration or nothing.** An uncalibrated cheap pH probe plus an automatic doser is a machine that poisons the tank with confidence.
7. **Probes far from the injection point.** Measure on the pump intake side, inject on the opposite side.
8. **A physical switch.** A real auto/manual button on the box, on top of the software setting.

## Making a pump say "0.5 mL"

A peristaltic pump knows seconds, not millilitres. Run it for 60 seconds into a measuring cup, weigh the result (1 g = 1 mL) and you have its flow rate, typically 20 to 40 mL/min. Dose time is volume divided by flow: at 30 mL/min, 0.5 mL takes 1 second and 9 mL takes 18 seconds. Store one flow rate per pump.

Silicone tubing wears and flow drops a few percent a month: redo the weighing when you calibrate the probes. Prime each tube before the first dose, or the first command pumps air.

## Order of work

| Weeks | Step |
|---|---|
| 1 to 2 | **See.** Temperature, float, light and air reporting. You learn the tools on sensors that cannot break anything |
| 3 to 4 | **Measure.** pH and EC connected. Compare with your handheld meters every day until they agree within ±0.1 pH and ±0.05 EC. Nothing else until then |
| 5 to 6 | **Act, in shadow mode.** Pumps mounted, calibrated, primed. Automations log "I would have dosed 0.5 mL at 14:12" |
| 7 | **pH only.** The most frequent, smallest and easiest action to watch. Nutrients stay in shadow |
| 9 | **Nutrients.** Once pH has run alone without surprises for two weeks |
| Later | **Look.** Camera and vision model |

At every step the tower still works by hand. Automation is added on top, never instead.

## What not to buy

- All-in-one spike probes, NPK or not.
- A solenoid valve for water. Topping up by hand is the safest choice by far.
- CO2, dissolved oxygen or "plant health" sensors. The first tells you nothing actionable outdoors, the second costs €200 to confirm what water temperature already tells you, the third does not exist.
- A turnkey hydroponic controller at €300 to €600. It does what this plan does, as a black box, without the safeguards you wrote yourself.
