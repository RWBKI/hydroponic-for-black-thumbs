---
name: hydroponic-tower-coach
description: >
  This skill should be used when the user wants to set up, run or troubleshoot
  a home hydroponic tower or similar recirculating system. Trigger phrases
  include "set up my hydroponic tower", "create my tower dashboard", "help me
  with my hydroponic tower", "what should I dose", "my pH keeps rising",
  "my EC is too low", "when do I drain the reservoir", or a pasted block that
  starts with "Hydroponic tower status". It builds a personal dashboard from
  the bundled template and then coaches the user from their own readings.
metadata:
  version: "0.1.0"
---

# Hydroponic tower coach

Help a beginner run a hydroponic tower well: set up a dashboard that tells them what to do today, then advise them from their own pH, EC, temperature and level readings until they can manage without asking.

Talk to the user in their language. The dashboard interface itself is in English.

## What is bundled

| File | Use |
|---|---|
| `assets/dashboard.html` | The dashboard template. One self-contained page. Never rewrite it; only replace its embedded state. |
| `references/state-schema.md` | The exact JSON the dashboard reads. Read it before building a state. |
| `references/growing-guide.md` | The method: doses, mixing order, routine, transplanting, troubleshooting. Read the relevant section before giving advice beyond the basics below. |

## Decide which mode applies

- The user has no dashboard yet, or asks to set one up: run **Setup**.
- The user pastes a block starting with "Hydroponic tower status", shares readings, or describes a problem: run **Coaching**.
- The user wants to change their tower, tank or nutrients: run **Updating**.

## Setup

### 1. Ask only what the dashboard needs

Ask these in one message. Accept "I don't know" for any of them and use the default.

1. The tower: how many columns of planting sites, how many levels, and the tank volume. Default 4 columns, 6 levels.
2. Where it stands: indoors or outdoors.
3. Tap water: pH and EC of plain tap water, if they can measure it.
4. Nutrients: brand and product line, and how many bottles.
5. Tools: a pH meter, an EC meter, pH Down.
6. Units: litres or US gallons, °C or °F, and what their EC meter displays (mS/cm, ppm on the 500 scale, ppm on the 700 scale, or hundreds of µS/cm).

Optionally ask what is already planted and when.

### 2. Choose the doses honestly

The template's default doses are for **Terra Aquatica TriPart** (Micro, Grow, Bloom) on soft tap water, for leafy greens and herbs. They were measured on one real tower: about 0.5 EC per mL/L of the mix.

- If the user has TriPart: keep the defaults, and tell them to confirm with their EC meter on the first fill.
- If the user has another three-part nutrient: do not invent doses. Start the cruise regime at half of the manufacturer's dose for leafy greens, have the user measure EC after mixing, then adjust the rates so the tank lands at EC 1.0 to 1.2 mS/cm. Set the recovery and low light regimes to about two thirds of cruise. Set the top-up dose to about one fifth of cruise. Say plainly that these are starting points to be corrected by measurement.
- If the user has a one- or two-part nutrient: put the bottle names in `parts`, pad to three entries with a rate of 0 only if needed, and apply the same half-dose-then-measure approach.
- If tap water EC is above about 0.6 mS/cm, warn that the targets leave less room for nutrients and suggest checking the water hardness.

Targets that do not depend on the brand: pH 5.8 to 6.2 (5.5 to 6.5 acceptable), EC 1.0 to 1.2 mS/cm in cruise for leafy greens, water 18 to 22 °C.

### 3. Build the state

Read `references/state-schema.md` and build one JSON object. Set `demo` to `false`. Convert everything to litres, °C and mS/cm in the stored values; the unit settings only change how the dashboard displays them. Leave `readings` empty unless the user gave readings.

### 4. Create the dashboard

1. Copy `assets/dashboard.html` to a working file. Do not retype or regenerate it.
2. In the copy, replace the content of `<script id="state" type="application/json">…</script>` with the state JSON on one line. Escape every `</` inside the JSON as `<\/`. Change nothing else.
3. Publish the copy as an artifact titled with the tower's name, when an artifact publishing tool is available.
4. If the environment lets a published page use declared capabilities, declare two for this page: `artifact` (the page saves every change by republishing itself with its data embedded; it then shows "synced", and the data can be read back later by reading the artifact) and `downloads` (the "Export backup" button saves a file). Without them the dashboard still works: it keeps data in the browser and copies backups to the clipboard.
5. If no artifact tool is available, give the user the HTML file to open in a browser. It works offline and keeps data in that browser only.

Never paste the full HTML into the conversation.

### 5. Hand over in a few lines

Tell the user:

- Take a reading every 2 to 3 days and follow the "Today" card.
- To get advice, press "Copy summary for Claude" and paste it here.
- Change the tank size, units or doses in the Setup card.

Do not explain the whole method up front. Answer what they ask and let the dashboard carry the routine.

## Coaching

### Get the data first

Use the user's latest data, in this order of preference:

1. A pasted "Hydroponic tower status" block.
2. The dashboard artifact, if a tool can read it: the state is the JSON inside `<script id="state">`.
3. Otherwise ask for the latest pH, EC, water temperature, tank level and the date of the last fresh solution.

Do not advise on doses without a reading from the last few days.

### How to advise

- Give one action at a time, in the order that matters: water level, then pH, then EC.
- Correct pH in small steps: about 0.05 mL of pH Down per litre of tank, stir, wait 15 minutes, measure again. Never a dose computed in advance.
- Top up with plain water adjusted to pH 6, never with nutrient solution. Plants drink more water than they eat salts.
- EC below the ideal range: the dashboard's "EC correction" block gives the exact millilitres of each bottle for the water actually in the tank. Use those figures. If the tank is low, top up with water and measure again first. For a correction above 0.4 mS/cm, add half, wait 15 minutes, measure, then decide on the rest. EC above the high alert: plain water only, and 20 % lower doses at the next fresh solution.
- Full drain every 15 to 21 days, or when cumulative top-ups reach the tank volume. EC says how much is dissolved, not what.
- Mixing order is always the same: each bottle into the full volume, one after the other, stirring in between, then wait 15 minutes, then pH. Two concentrates must never be mixed together.
- When growth slows with good pH and EC, suspect light or temperature. Never raise doses to compensate; lower them.
- Explain the reason behind each instruction in a sentence. The aim is that the user stops needing to ask.

For symptoms on leaves or roots, transplanting, noise, crop choice or seasons, read the matching section of `references/growing-guide.md` first.

### Be clear about limits

- The plants cannot be seen. Ask for a photo when the user describes a symptom, and say when a diagnosis is a guess.
- Estimates of yield, light and harvest dates are orders of magnitude.
- pH Down is acid: small doses, into the full volume, away from other concentrates.
- Keep mains electricity away from the tank. If the user asks about automating dosing with pumps and sensors, say that it must start with safeguards (shadow mode, hard daily ceilings, monthly calibration) and that the automation design published with this project is untested.

## Updating

To change settings, prefer telling the user which field to edit in the Setup card.

When the dashboard itself must be regenerated (new template version, or a change the Setup card does not offer):

1. Read the current dashboard's state first, from the artifact or from an "Export backup" file the user provides.
2. Carry over `readings`, `slots`, `reservoir`, `calib` and `id` unchanged.
3. Apply the change, then publish as in Setup step 4, over the same artifact.

Never publish over a dashboard without its latest readings.
