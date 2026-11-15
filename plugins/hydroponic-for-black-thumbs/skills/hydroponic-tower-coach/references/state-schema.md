# Dashboard state

The dashboard reads one JSON object from `<script id="state" type="application/json">`. Stored values are always metric: litres, °C, mS/cm. Unit settings only change the display.

When the object has no `settings`, the page shows demo data.

## Example

```json
{
  "v": 2,
  "demo": false,
  "savedAt": 0,
  "settings": {
    "name": "Balcony tower",
    "volume": 37,
    "volUnit": "L",
    "tempUnit": "C",
    "ecUnit": "mS",
    "ecMeter": true,
    "phDown": true,
    "outdoors": true,
    "regime": "recovery",
    "columns": 4,
    "levels": 6,
    "nutrient": {
      "brand": "Terra Aquatica TriPart",
      "parts": ["Micro", "Grow", "Bloom"],
      "rates": {
        "recovery": [0.4, 0.4, 0.4],
        "cruise": [0.6, 0.9, 0.3],
        "lowlight": [0.4, 0.4, 0.4],
        "topup": [0.13, 0.2, 0.07]
      },
      "topupGain": 0.2,
      "ec": {
        "recovery": [0.7, 0.9, 0.6, 1.1],
        "cruise": [1.0, 1.2, 0.9, 1.4],
        "lowlight": [0.7, 0.9, 0.6, 1.1]
      }
    }
  },
  "reservoir": { "lastChange": "2026-11-15", "topUp": 0, "lastThird": null },
  "calib": { "last": null },
  "readings": [
    { "d": "2026-11-15", "ph": 6.0, "ec": 0.85, "t": 20, "level": 37 }
  ],
  "slots": [
    { "c": "basil", "p": "2026-11-15" },
    null
  ]
}
```

## Fields

| Field | Meaning |
|---|---|
| `v` | Schema version. Always 2 |
| `demo` | `false` for a real tower |
| `savedAt` | Milliseconds timestamp of the last save. Use 0 for a new dashboard |
| `id` | Optional. The dashboard assigns one on first save. Keep it when regenerating |
| `settings.name` | Shown as the page title. 40 characters at most |
| `settings.volume` | Tank volume in litres |
| `settings.volUnit` | `"L"` or `"gal"` (US gallons) |
| `settings.tempUnit` | `"C"` or `"F"` |
| `settings.ecUnit` | `"mS"` (mS/cm), `"x100"` (meter shows 11 for 1.1 mS/cm), `"ppm500"`, `"ppm700"` |
| `settings.ecMeter` | Whether the user owns an EC meter. Without one the dashboard uses the one-third method |
| `settings.phDown` | Whether the user owns pH Down |
| `settings.outdoors` | Adds frost reminders in the low light regime |
| `settings.regime` | `"recovery"` (first 10 days after transplanting), `"cruise"`, `"lowlight"` (short days, slow growth) |
| `settings.columns`, `settings.levels` | Tower geometry. 1 to 8 columns, 1 to 12 levels |
| `settings.nutrient.brand` | Shown on the dashboard |
| `settings.nutrient.parts` | Bottle names, in mixing order |
| `settings.nutrient.rates` | mL per litre for each bottle, per regime, plus `topup` (raises EC by about 0.2) |
| `settings.nutrient.topupGain` | How much one `topup` dose raises EC, in mS/cm. The dashboard scales the top-up row by this to compute the exact correction when EC is low. Measure it once for another nutrient line: add one top-up dose, wait 15 minutes, note the rise |
| `settings.nutrient.ec` | Per regime, in mS/cm: ideal low, ideal high, alert low, alert high |
| `reservoir.lastChange` | ISO date of the last fresh solution, or `null` |
| `reservoir.topUp` | Litres of plain water added since then |
| `reservoir.lastThird` | ISO date of the last one-third replacement, or `null` |
| `calib.last` | ISO date of the last pH meter calibration, or `null` |
| `readings[]` | `d` ISO date, `ph`, `ec` in mS/cm, `t` in °C, `level` in litres. Any value may be `null`. Oldest first |
| `slots[]` | One entry per site, row by row from the top level: index = level × columns + column. `null` for an empty site, or `c` crop key and `p` planting date. The length must be columns × levels |

## Crop keys

`lettuce`, `cutlettuce`, `lambs`, `cress`, `spinach`, `mizuna`, `rocket`, `chives`, `basil`, `parsley`, `chervil`.

Other crops are not known to the dashboard. Leave the site empty and track them in conversation.

## Conversions

- US gallons to litres: × 3.785
- °F to °C: (°F − 32) × 5 / 9
- ppm (500 scale) to mS/cm: ÷ 500
- ppm (700 scale) to mS/cm: ÷ 700
- hundreds of µS/cm to mS/cm: ÷ 10
