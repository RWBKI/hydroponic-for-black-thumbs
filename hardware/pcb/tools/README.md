# Board checks

Two scripts that re-check claims made about rev 1.1 without relying on KiCad's design-rule check. They need KiCad 7 (for its `pcbnew` Python module and `kicad-cli`) and `shapely`.

| Script | What it checks |
|---|---|
| `verify_geometry.py` | Smallest distance between copper of the isolated island and the rest of the board, on each layer. Whether the guard copper closes around the pH input, and its narrowest section |
| `compare_netlist.py` | Schematic against board, pin by pin. With an older netlist as third argument, which pins changed net |

```bash
python3 verify_geometry.py ../kicad/hydroponic-tower-controller.kicad_pcb

kicad-cli sch export netlist -o board.net ../kicad/hydroponic-tower-controller.kicad_sch
python3 compare_netlist.py board.net ../kicad/hydroponic-tower-controller.kicad_pcb
```

Expected for rev 1.1: `barrier` 1.0 and `guard_closed` True on both layers; 0 pin differences.

The one value difference reported (D2, "SMF15A" against "SMF15A (TVS)") is a label, present since rev 1.0.
