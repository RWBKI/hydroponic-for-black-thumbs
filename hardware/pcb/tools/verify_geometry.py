"""Geometric checks of the isolation barrier and of the pH guard ring.

Independent of KiCad's DRC: copper is rebuilt from pads, tracks, vias and zone fills and measured with shapely.
Usage (KiCad 7 Python environment, `pip install shapely`):
    python3 verify_geometry.py ../kicad/hydroponic-tower-controller.kicad_pcb
Expected for rev 1.1: barrier >= 1.0 mm and guard_closed True on both layers.
"""
import sys, pcbnew
from shapely.geometry import Polygon, LineString, Point
from shapely.ops import unary_union
mm = pcbnew.ToMM
b = pcbnew.LoadBoard(sys.argv[1])
ISO = {'GND_ISO','+3V3_ISO','SDA_ISO','SCL_ISO','VSEL','BNC_SH','PH_REF','PH_SIG','VREF_DIV','PH_IN','PH_IN_F'}
NODE = {'PH_IN','PH_IN_F'}
def chain(c): return [(mm(c.CPoint(i).x), mm(c.CPoint(i).y)) for i in range(c.PointCount())]
def polyset(ps):
    out = []
    for i in range(ps.OutlineCount()):
        ext = chain(ps.Outline(i)); holes = [chain(ps.Hole(i, h)) for h in range(ps.HoleCount(i))]
        if len(ext) >= 3: out.append(Polygon(ext, holes).buffer(0))
    return out
res = {}
for lname, lid in (('F.Cu', pcbnew.F_Cu), ('B.Cu', pcbnew.B_Cu)):
    items = []   # (net, geometry, kind)
    for fp in b.GetFootprints():
        for p in fp.Pads():
            if p.IsOnLayer(lid):
                for g in polyset(p.GetEffectivePolygon()): items.append((p.GetNetname(), g, 'pad'))
    for t in b.GetTracks():
        s, e = t.GetStart(), t.GetEnd()
        if t.GetClass() == 'PCB_VIA': items.append((t.GetNetname(), Point(mm(s.x), mm(s.y)).buffer(mm(t.GetWidth())/2, 64), 'via'))
        elif t.GetLayer() == lid: items.append((t.GetNetname(), LineString([(mm(s.x), mm(s.y)), (mm(e.x), mm(e.y))]).buffer(mm(t.GetWidth())/2, 32), 'track'))
    for z in b.Zones():
        if z.GetIsRuleArea() or not z.IsOnLayer(lid): continue
        for g in polyset(z.GetFilledPolysList(lid)): items.append((z.GetNetname(), g, 'zone'))
    iso = unary_union([g for n, g, k in items if n in ISO]); main = unary_union([g for n, g, k in items if n and n not in ISO])
    node = unary_union([g for n, g, k in items if n in NODE]); guard = unary_union([g for n, g, k in items if n == 'PH_SIG'])
    others = unary_union([g for n, g, k in items if n and n not in NODE and n != 'PH_SIG'])
    gnd_iso = unary_union([g for n, g, k in items if n == 'GND_ISO'])
    # the guard encloses the node if the node lies inside the guard's outer boundary but does not touch guard copper
    guard = guard.buffer(0.002).buffer(-0.002)      # heals the zero-width slits KiCad uses to store holes
    parts = list(guard.geoms) if guard.geom_type == 'MultiPolygon' else [guard]
    nodes = list(node.geoms) if node.geom_type == 'MultiPolygon' else [node]
    enclosing, ring_w = True, None
    for nd in nodes:
        holes = [(p, Polygon(h)) for p in parts for h in p.interiors if Polygon(h).contains(nd)]
        if not holes: enclosing = False; continue
        w = holes[0][0].exterior.distance(holes[0][1])
        ring_w = w if ring_w is None else min(ring_w, w)
    res[lname] = dict(barrier=round(iso.distance(main), 3), node_to_guard=round(node.distance(guard), 3), node_to_other=round(node.distance(others), 3),
                      node_to_gnd_iso=round(node.distance(gnd_iso), 3), guard_closed=enclosing, node_parts=len(nodes),
                      ring_min_width=None if ring_w is None else round(ring_w, 3))
    print(lname, res[lname])
