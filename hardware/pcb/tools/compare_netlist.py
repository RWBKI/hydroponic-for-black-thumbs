"""Compares the schematic netlist with the board, pad by pad.

Usage (KiCad 7):
    kicad-cli sch export netlist -o board.net ../kicad/hydroponic-tower-controller.kicad_sch
    python3 compare_netlist.py board.net ../kicad/hydroponic-tower-controller.kicad_pcb [older.net]
With a third argument it also lists what changed since an older netlist.
"""
import sys, re, pcbnew
def netlist(path):
    s = open(path, encoding='utf-8').read(); pins = {}; comps = {}
    for m in re.finditer(r'\(comp \(ref "([^"]+)"\)\s*\(value "([^"]*)"\)\s*\(footprint "([^"]*)"\)(.*?)\(tstamps', s, re.S):
        l = re.search(r'\(property \(name "LCSC"\) \(value "([^"]*)"\)', m.group(4))
        comps[m.group(1)] = (m.group(2), m.group(3).split(':')[-1], l.group(1) if l else '')
    for m in re.finditer(r'\(net \(code "\d+"\) \(name "([^"]*)"\)(.*?)(?=\(net \(code|\)\s*\)\s*$)', s, re.S):
        name = '' if m.group(1).startswith('unconnected-') else m.group(1).lstrip('/')
        for n in re.finditer(r'\(node \(ref "([^"]+)"\) \(pin "([^"]+)"\)', m.group(2)): pins[(n.group(1), n.group(2))] = name
    return comps, pins
def board(path):
    b = pcbnew.LoadBoard(path); pins = {}; comps = {}
    for fp in b.GetFootprints():
        comps[fp.GetReference()] = (fp.GetValue(), str(fp.GetFPID().GetLibItemName()), fp.GetProperty('LCSC') if fp.HasProperty('LCSC') else '')
        for p in fp.Pads():
            if p.GetNumber():
                k = (fp.GetReference(), p.GetNumber()); n = p.GetNetname()
                assert pins.get(k, n) == n, ('pads of one pin on different nets', k); pins[k] = n
    return comps, pins
sc, sp = netlist(sys.argv[1]); bc, bp = board(sys.argv[2])
print('schematic: %d components, %d pins, %d nets | board: %d footprints, %d numbered pads, %d nets' % (len(sc), len(sp), len(set(sp.values()) - {''}), len(bc), len(bp), len(set(bp.values()) - {''})))
d = [(k, sp.get(k), bp.get(k)) for k in sorted(set(sp) | set(bp)) if sp.get(k) != bp.get(k)]
print('pin/net differences:', len(d)); [print('  ', x) for x in d[:30]]
d = [(k, sc.get(k), bc.get(k)) for k in sorted(set(sc) | set(bc)) if sc.get(k) != bc.get(k)]
print('value/footprint/LCSC differences:', len(d)); [print('  ', x) for x in d[:30]]
if len(sys.argv) > 3:
    oc, op = netlist(sys.argv[3])
    ch = [(k, op.get(k), sp.get(k)) for k in sorted(set(sp) | set(op)) if k in op and op.get(k) != sp.get(k)]
    print('pins of existing parts whose net changed since the older netlist:', len(ch)); [print('  ', k[0] + '.' + k[1], repr(a), '->', repr(n)) for k, a, n in ch]
    print('new parts:', sorted(set(sc) - set(oc)), '| removed parts:', sorted(set(oc) - set(sc)))
    print('changed values:', [(k, oc[k], sc[k]) for k in sorted(oc) if k in sc and oc[k] != sc[k]])
