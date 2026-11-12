# Gehaeuse fuer den CRSF-Router 8 (RC-CRSF-Router-RP2350, Platine hardware/Router, 38,4 x 25,5 mm)
# Parametrisch mit CadQuery. Aufruf: python gehaeuse.py [ausgabeordner] [--pruefen]
#   -> Gehaeuse_Unterteil.stl/.step, Gehaeuse_Deckel.stl/.step
#
# Masse aus RC-CRSF-Router-RP2350.kicad_pcb (Stand 11.11.2026, main 3c74544).
# Koordinaten: Gehaeuse X = KiCad x - 100 (USB-Seite links = 0), Y = 125,5 - KiCad y (Reihe U hinten),
# Z = Hoehe ueber dem Druckbett. Die Platine liegt bei X 0..38,4, Y 0..25,5.
# Autor: PiperPilot
import os, sys
import cadquery as cq

# ---------------- Parameter (alles in mm) ----------------
WAND    = 1.6     # Wandstaerke
BODEN   = 1.6     # Bodenstaerke
DECKEL  = 1.6     # Deckelstaerke
LIPPE   = 2.0     # Hoehe der Klemmlippe am Deckel
TOL     = 0.15    # Spiel Lippe <-> Innenwand (bei strammem Sitz auf 0,2 erhoehen)
SPALT   = 0.3     # Luft Platine <-> Innenwand
PCB_L, PCB_B, PCB_T = 38.4, 25.5, 1.6
UNTEN   = 2.5     # Platz unter der Platine (Loetstellen, abgeschnittene Beinchen)
OBEN    = 13.0    # Platz ueber der Platine: F1 (PTC, bis 12,5 hoch angenommen) und C1 (5 x 11)
AUFLAGE = 0.8     # Breite der Auflagekante unter dem Platinenrand

def kx(x): return x - 100.0           # KiCad -> Gehaeuse
def ky(y): return 125.5 - y

Z_PB = BODEN + UNTEN                  # Platinen-Unterseite
Z_PT = Z_PB + PCB_T                   # Platinen-Oberseite
H_BASE = Z_PT + OBEN                  # Hoehe Unterteil = Deckel-Unterseite

# Lage der Teile (aus der Platine, Zero-Bauteile aus dem Foto geschaetzt)
ZERO   = (kx(100.0), kx(123.5), ky(118.8), ky(100.8))   # RP2350-Zero 23,5 x 18, 1 mm dick, flach aufgeloetet
USB_Y  = ky(109.8)                    # Mitte USB-C
USB_Z  = Z_PT + 1.0 + 1.6             # Mitte Buchse (3,2 hoch auf dem Zero)
USB_OEFF = (12.6, 7.2)                # Oeffnung fuer die Stecker-Umspritzung (B x H)
BOOT   = (kx(115.3), ky(114.6))       # Tasten 15,3 mm vom USB-Ende, BOOT links / RUN rechts (USB oben)
RUN    = (kx(115.3), ky(105.0))
LED    = (kx(115.0), ky(109.8))
TASTE_OBEN = Z_PT + 1.0 + 1.5
P = 2.54
REIHEN = [ky(104.6 + P * i) for i in range(8)]          # U, 1..7
SPALTEN = [kx(129.0 + P * i) for i in range(4)]         # T R V G
HDR_X = (kx(127.73), kx(137.89))      # Kunststoff der Stiftleisten
HDR_Y = (ky(123.65), ky(103.33))
HDR_H = 2.5
# Steckerfeld: oben und rechts offen, damit die Dupont-Stecker greifbar bleiben
FELD_X0 = kx(127.0)
FELD_Y  = (HDR_Y[0] - 0.45, HDR_Y[1] + 0.45)

def box(x0, y0, z0, x1, y1, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))

def rbox(x0, y0, z0, x1, y1, z1, r):
    return (cq.Workplane("XY").workplane(offset=z0).center((x0 + x1) / 2, (y0 + y1) / 2)
            .rect(x1 - x0, y1 - y0).extrude(z1 - z0).edges("|Z").fillet(r))

def cyl(x, y, z0, z1, r):
    return cq.Workplane("XY").workplane(offset=z0).center(x, y).circle(r).extrude(z1 - z0)

IX0, IX1, IY0, IY1 = -SPALT, PCB_L + SPALT, -SPALT, PCB_B + SPALT
OX0, OX1, OY0, OY1 = IX0 - WAND, IX1 + WAND, IY0 - WAND, IY1 + WAND
ECKE_R = 2.0
SNAP_X = (3.0, 21.0)                  # Rastnasen an den Laengsseiten (links vom Steckerfeld)

def unterteil():
    b = rbox(OX0, OY0, 0, OX1, OY1, H_BASE, ECKE_R)
    b = b.cut(box(IX0, IY0, BODEN, IX1, IY1, H_BASE + 1))
    # Auflagekante rundum unter dem Platinenrand
    rand = box(IX0, IY0, BODEN - 0.01, IX1, IY1, Z_PB)
    rand = rand.cut(box(AUFLAGE, AUFLAGE, 0, PCB_L - AUFLAGE, PCB_B - AUFLAGE, Z_PB + 1))
    b = b.union(rand)
    # USB-C in der linken Stirnwand
    w, h = USB_OEFF
    b = b.cut(box(OX0 - 1, USB_Y - w / 2, USB_Z - h / 2, IX0 + 0.5, USB_Y + w / 2, USB_Z + h / 2))
    # Steckerfeld: rechte Wand bis auf Hoehe Stiftleisten-Kunststoff ausgespart
    b = b.cut(box(FELD_X0, FELD_Y[0], Z_PT + HDR_H, OX1 + 1, FELD_Y[1], H_BASE + 1))
    # Rastnuten fuer die Deckelnasen (Laengsseiten)
    for y in (IY0, IY1):
        b = b.cut(cq.Workplane("YZ").workplane(offset=SNAP_X[0]).center(y, H_BASE - LIPPE / 2 - 0.1)
                  .circle(0.5).extrude(SNAP_X[1] - SNAP_X[0]))
    # Befestigungslaschen an den Laengsseiten (M2 oder Kabelbinder)
    xm = 14.0
    for y, s in ((OY0, -1), (OY1, 1)):
        t = rbox(xm - 4, y - 6 if s < 0 else y - 1, 0, xm + 4, y + 1 if s < 0 else y + 6, 1.8, 1.5)
        b = b.union(t).cut(cyl(xm, y + s * 3.3, -1, 4, 1.2))
    # Beschriftung unten (0,5 mm vertieft, von unten lesbar)
    def txt(s, x, y, size):
        return (cq.Workplane("XY").workplane(offset=0.5).center(x, y)
                .transformed(rotate=(180, 0, 0)).text(s, size, 1.0, halign="center", valign="center"))
    for s, x, y, sz in [("CRSF-ROUTER 8", PCB_L / 2, 16.0, 3.0), ("5-6V, kein HV-BEC", PCB_L / 2, 9.5, 2.2)]:
        b = b.cut(txt(s, x, y, sz))
    return b

def deckel():
    d = rbox(OX0, OY0, 0, OX1, OY1, DECKEL, ECKE_R)
    # Klemmlippe
    lip = box(IX0 + TOL, IY0 + TOL, -LIPPE, IX1 - TOL, IY1 - TOL, 0.01)
    lip = lip.cut(box(IX0 + TOL + 1.0, IY0 + TOL + 1.0, -LIPPE - 1, IX1 - TOL - 1.0, IY1 - TOL - 1.0, 1))
    for y in (IY0 + TOL, IY1 - TOL):
        lip = lip.union(cq.Workplane("YZ").workplane(offset=SNAP_X[0] + 1).center(y, -LIPPE / 2 - 0.1)
                        .circle(0.35).extrude(SNAP_X[1] - SNAP_X[0] - 2))
    lip = lip.cut(box(kx(103.6) - 3.2, IY0 - 1, -LIPPE - 1, kx(103.6) + 3.2, IY0 + 1.5, 1))   # Platz fuer C1
    d = d.union(lip)
    # Niederhalter (0,1 mm ueber der Platine) an freien Stellen: zwischen Zero und Leisten, Ecken rechts
    zu = -(H_BASE - Z_PT - 0.1)
    for x0, y0, x1, y1 in [(24.3, 6.5, 26.9, 10.0), (24.3, 20.5, 26.9, 24.5),   # zwischen Zero und Leisten
                           (28.5, 22.9, 37.5, 25.1),                            # Ecke hinten rechts
                           (28.5, 0.3, 37.5, 1.3)]:                             # Ecke vorne rechts
        d = d.union(box(x0, y0, zu, x1, y1, 0.01))
    # Steckerfeld oben offen
    d = d.cut(box(FELD_X0, FELD_Y[0], -H_BASE, OX1 + 1, FELD_Y[1], DECKEL + 1))
    # BOOT und RUN: Fuehrungsrohr fuer Bueroklammer / Zahnstocher, endet 1 mm ueber der Taste
    for x, y in (BOOT, RUN):
        d = d.union(cyl(x, y, -(H_BASE - TASTE_OBEN - 1.0), 0.01, 2.0))
        d = d.cut(cyl(x, y, -H_BASE, DECKEL + 1, 1.0))
    # LED: Lichtrohr, offen
    d = d.union(cyl(LED[0], LED[1], -(H_BASE - Z_PT - 2.0 - 1.0), 0.01, 2.0))
    d = d.cut(cyl(LED[0], LED[1], -H_BASE, DECKEL + 1, 1.3))
    # Beschriftung (0,5 mm vertieft)
    def txt(s, x, y, size, rot=0):
        return (cq.Workplane("XY").workplane(offset=DECKEL - 0.5).center(x, y)
                .transformed(rotate=(0, 0, rot)).text(s, size, 1.0, halign="center", valign="center"))
    t = [("BOOT", BOOT[0], BOOT[1] - 3.4, 1.6, 0), ("RUN", RUN[0], RUN[1] + 3.2, 1.6, 0),
         ("USB", 1.4, USB_Y, 1.8, 90), ("CRSF-Router 8", 8.5, 3.0, 2.0, 0)]
    for s, x in zip("TRVG", SPALTEN):
        t.append((s, x, 24.1, 1.6, 0))
    for s, y in zip("U1234567", REIHEN):
        t.append((s, FELD_X0 - 1.2, y, 1.5, 0))
    for s, x, y, sz, r in t:
        d = d.cut(txt(s, x, y, sz, r))
    return d

def teile():
    """Vereinfachte Platine mit Bauteilen fuer Vorschau und Kollisionskontrolle."""
    t = {}
    t["pcb"] = rbox(0, 0, Z_PB, PCB_L, PCB_B, Z_PT, 1.0)
    t["zero"] = box(ZERO[0], ZERO[2], Z_PT, ZERO[1], ZERO[3], Z_PT + 1.0)
    t["usb"] = box(kx(99.25), USB_Y - 4.5, Z_PT + 1.0, kx(106.6), USB_Y + 4.5, Z_PT + 4.2)
    t["chip"] = box(LED[0] - 7.5, USB_Y - 3.5, Z_PT + 1.0, LED[0] - 0.5, USB_Y + 3.5, Z_PT + 1.9)
    t["tasten"] = (box(BOOT[0] - 1.5, BOOT[1] - 1.5, Z_PT + 1.0, BOOT[0] + 1.5, BOOT[1] + 1.5, TASTE_OBEN)
                   .union(box(RUN[0] - 1.5, RUN[1] - 1.5, Z_PT + 1.0, RUN[0] + 1.5, RUN[1] + 1.5, TASTE_OBEN)))
    t["led"] = box(LED[0] - 0.8, LED[1] - 0.8, Z_PT + 1.0, LED[0] + 0.8, LED[1] + 0.8, Z_PT + 2.0)
    t["c1"] = cyl(kx(103.6), ky(122.6), Z_PT + 0.5, Z_PT + 11.5, 2.5)
    t["d1"] = (cq.Workplane("YZ").workplane(offset=kx(109.3)).center(ky(122.6), Z_PT + 1.4)
               .circle(1.35).extrude(4.8))
    t["f1"] = box(kx(117.0), ky(122.0), Z_PT + 0.5, kx(124.4), ky(121.0) + 0.5, Z_PT + 12.5)
    t["hdr"] = box(HDR_X[0], HDR_Y[0], Z_PT, HDR_X[1], HDR_Y[1], Z_PT + HDR_H)
    pins = None
    for x in SPALTEN:
        for y in REIHEN:
            p = box(x - 0.32, y - 0.32, Z_PB - 1.5, x + 0.32, y + 0.32, Z_PT + 8.5)
            pins = p if pins is None else pins.union(p)
    t["pins"] = pins
    return t

def stecker():
    """Acht 4-pol. Dupont-Stecker (2,54 x 10,16 x 14) auf den Leisten, fuer die Vorschau."""
    s = None
    for y in REIHEN:
        p = box(SPALTEN[0] - 1.25, y - 1.22, Z_PT + HDR_H, SPALTEN[3] + 1.25, y + 1.22, Z_PT + HDR_H + 14)
        s = p if s is None else s.union(p)
    return s

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out = args[0] if args else "export"
    os.makedirs(out, exist_ok=True)
    u, d = unterteil(), deckel()
    print("Aussenmasse: %.1f x %.1f x %.1f mm (mit Deckel, ohne Laschen)" % (OX1 - OX0, OY1 - OY0, H_BASE + DECKEL))
    cq.exporters.export(u, f"{out}/Gehaeuse_Unterteil.stl", tolerance=0.02, angularTolerance=0.1)
    cq.exporters.export(u, f"{out}/Gehaeuse_Unterteil.step")
    d_druck = d.rotate((0, 0, 0), (1, 0, 0), 180)   # Deckel mit der Oberseite aufs Druckbett
    cq.exporters.export(d_druck, f"{out}/Gehaeuse_Deckel.stl", tolerance=0.02, angularTolerance=0.1)
    cq.exporters.export(d, f"{out}/Gehaeuse_Deckel.step")
    if "--pruefen" in sys.argv:
        dd = d.translate((0, 0, H_BASE))
        for k, v in list(teile().items()) + [("stecker", stecker())]:
            a, b = v.intersect(u).val().Volume(), v.intersect(dd).val().Volume()
            print("Kollision %-8s Unterteil %.3f  Deckel %.3f mm3" % (k, a, b))
