#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Landscape: the controls pictures (docs/img/commandes-smk37-fr.png, controls-smk37-en.png,
commandes-fm1-fr.png, controls-fm1-en.png), the SMK-37 Elite
and the FM-1 seen from above, drawn like Modular Compagnon's (tools/make_controls_img.py there) in the page's colours:
the grey-green paper, the slate ink, the honey of the guitar's top, the Landscape blue on the effect pads.

  site/make_controls_img.py [OUTDIR]"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT = ROOT / "smk37" / "assets" / "fonts" / "ui" / "silkscreen-latin-400-normal.woff"
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs" / "img"

PAPER, GRAIN, INK, DIM = (216, 220, 214), (204, 209, 202), (39, 50, 58), (95, 107, 112)
BODY, SHADOW, HONEY, HONEY2 = (54, 62, 68), (186, 192, 185), (217, 164, 102), (176, 128, 74)
BLUE, CREAM, LBL, OFF, KEYB = (143, 180, 208), (240, 237, 230), (226, 230, 224), (84, 94, 100), (30, 34, 38)
S = 2


def font(px):
    return ImageFont.truetype(str(FONT), px * S)


def k(v):
    return int(v * S)


EN = {
    "MODULAR LANDSCAPE  ·  LE SMK-37 ELITE": "MODULAR LANDSCAPE  ·  THE SMK-37 ELITE",
    "MODULAR LANDSCAPE  ·  LE FM-1": "MODULAR LANDSCAPE  ·  THE FM-1",
    "ARCHET": "BOW", "ESQUISSE": "SKETCH", "QUANTITE": "AMOUNT", "OCTAVE": "OCTAVE",
    "GARDER": "KEEP", "REGLAGES": "SETTINGS", "MAGNETO": "TAPE",
    "37 TOUCHES : L'ARCHET, DO2 A SOL4": "37 KEYS: THE BOW, C2 TO G4",
    "27 TOUCHES : L'ARCHET, FA2 A SOL4": "27 KEYS: THE BOW, F2 TO G4",
    "PADS DU HAUT : LES EFFETS, UN APPUI = ON / OFF  ·  LE POTARD AU-DESSUS : SA QUANTITE (K1 : LE VOLUME)":
        "TOP PADS: THE EFFECTS, A TOUCH = ON / OFF  ·  THE KNOB ABOVE: HOW MUCH (K1: THE VOLUME)",
    "PADS DU BAS : 8 ESQUISSES  ·  UN APPUI ENREGISTRE, UN SECOND GARDE, UN PAD PLEIN REJOUE EN BOUCLE":
        "BOTTOM PADS: 8 SKETCHES  ·  A TOUCH RECORDS, A SECOND KEEPS, A FULL PAD LOOPS ITS SKETCH",
    "SEQ REC MAINTENU : LA DERNIERE PHRASE GARDEE  ·  SEQ PLAY : LES ESQUISSES A LA SUITE  ·  SEQ MAINTENU : LA LISTE":
        "SEQ REC HELD: THE LAST PHRASE KEPT  ·  SEQ PLAY: THE SKETCHES ONE AFTER THE OTHER  ·  SEQ HELD: THE LIST",
    "FADERS : ATTACK, RELEASE, SPACE (LA REVERB), LENGTH (LE COUP D'ARCHET)  ·  MOLETTE : PURE":
        "FADERS: ATTACK, RELEASE, SPACE (THE REVERB), LENGTH (THE BOW STROKE)  ·  MOD WHEEL: PURE",
    "ARCHET : L'ARCHET ON / OFF (OFF : LE CLAVIER NE PILOTE QUE L'ORDINATEUR)  ·  OCT- / OCT+ : PENDANT UNE ESQUISSE":
        "BOW: THE BOW ON / OFF (OFF: THE KEYBOARD ONLY DRIVES THE COMPUTER)  ·  OCT- / OCT+: WHILE A SKETCH PLAYS",
    "LES BOUTONS D'EFFET : UN APPUI = ON / OFF  ·  SELECT REGLE LA QUANTITE DU DERNIER EFFET TOUCHE":
        "THE EFFECT BUTTONS: A TOUCH = ON / OFF  ·  SELECT SETS HOW MUCH OF THE LAST EFFECT TOUCHED",
    "SAVE GARDE TON ARCHET POUR LE PROCHAIN ALLUMAGE  ·  MAINTENU 2 S : LES REGLAGES D'USINE":
        "SAVE KEEPS YOUR BOW FOR THE NEXT POWER-ON  ·  HELD 2 S: THE FACTORY SETTINGS",
    "PLAY · REC · SEQ : LE MAGNETO ET LE CARNET DE CROQUIS DE MODULAR ONE  ·  PAS DE WIDTH : LE HAUT-PARLEUR EST MONO":
        "PLAY · REC · SEQ: MODULAR ONE'S TAPE AND SKETCHBOOK  ·  NO WIDTH: THE SPEAKER IS MONO",
}
LANG = "fr"


def text(d, xy, s, f, fill, anchor="mm"):
    if LANG == "en" and s in EN:
        s = EN[s]
    d.text((k(xy[0]), k(xy[1])), s, font=f, fill=fill, anchor=anchor)


def fit(d, s, width, sizes):
    for px in sizes:
        f = font(px)
        if d.textlength(s, font=f) <= k(width):
            return f
    return f


def canvas(W, H, title):
    im = Image.new("RGB", (W * S, H * S), PAPER)
    d = ImageDraw.Draw(im)
    for y in range(0, H * S, 14):
        for x in range((y // 14) % 2 * 7, W * S, 14):
            d.rectangle([x, y, x + 1, y + 1], fill=GRAIN)
    text(d, (W / 2, 46), title, font(28), INK)
    return im, d


def rr(d, box, r, fill, outline=None, w=0):
    d.rounded_rectangle([k(box[0]), k(box[1]), k(box[2]), k(box[3])], k(r), fill=fill,
                        outline=outline, width=k(w) if w else 0)


def knob(d, cx, cy, r, col):
    d.ellipse([k(cx - r), k(cy - r), k(cx + r), k(cy + r)], fill=col)
    d.line([k(cx), k(cy - r + 3), k(cx), k(cy - r / 3)], fill=CREAM, width=k(4))


def lead_up(d, cx, s, i, top=(118, 140), y1=184):
    y = top[i % 2]
    text(d, (cx, y), s, font(16), INK)
    d.line([k(cx), k(y + 12), k(cx), k(y1)], fill=DIM, width=k(2))


def notes(d, y, lines, W):
    for i, s in enumerate(lines):
        s = "·  " + (EN.get(s, s) if LANG == "en" else s)
        text(d, (90, y + i * 30), s, fit(d, s, W - 180, (16, 14, 12)), INK, anchor="lm")


def smk37(screen):
    W, H = 1600, 900
    im, d = canvas(W, H, "MODULAR LANDSCAPE  ·  LE SMK-37 ELITE")
    X0, Y0, X1, Y1 = 90, 150, 1530, 700
    rr(d, (X0 + 8, Y0 + 8, X1 + 8, Y1 + 8), 26, SHADOW)
    rr(d, (X0, Y0, X1, Y1), 26, BODY)
    for i, (x, a, b) in enumerate(((140, "PITCH", "BEND"), (232, "MOD", "PURE"))):
        rr(d, (x - 30, 240, x + 30, 385), 14, HONEY if i else OFF)
        text(d, (x, 205), a, font(16), LBL)
        text(d, (x, 130), b, font(16), INK if i else DIM)
        d.line([k(x), k(142), k(x), k(196)], fill=INK if i else DIM, width=k(2))
    sx, sy, sw = 290, 190, 150
    rr(d, (sx - 10, sy - 10, sx + sw + 10, sy + sw + 10), 12, (22, 20, 18))
    im.paste(screen.convert("RGB").resize((k(sw), k(sw)), Image.NEAREST), (k(sx), k(sy)))
    for i, s in enumerate(("", "", "ARCHET", "", "OCT-", "OCT+")):
        cx, cy = 318 + (i % 2) * 96, 395 + (i // 2) * 52
        rr(d, (cx - 44, cy - 20, cx + 44, cy + 20), 6, HONEY2 if s else OFF)
        if s:
            text(d, (cx, cy), s, font(12 if len(s) > 4 else 16), CREAM)
    for i, s in enumerate(("VOLUME", "BREATH", "CONTRA", "DRONE", "WIDTH", "DISTANCE", "MUTE", "ECHO")):
        cx = 530 + i * 82
        knob(d, cx, 241, 26, HONEY)
        text(d, (cx, 195), f"K{i + 1}", font(12), LBL)
        lead_up(d, cx, s, i)
    fx = ("SUSTAIN", "BREATH", "CONTRA", "DRONE", "WIDTH", "DISTANCE", "MUTE", "ECHO")
    for i in range(16):
        cx, cy = 530 + (i % 8) * 82, 315 + (i // 8) * 82
        top = i < 8
        rr(d, (cx - 37, cy - 33, cx + 37, cy + 33), 6, BLUE if top else CREAM)
        if top:
            text(d, (cx, cy - 10), f"PAD {i + 9}", font(8), INK)
            text(d, (cx, cy + 8), fx[i], fit(d, fx[i], 66, (12, 10, 8)), INK)
        else:
            text(d, (cx, cy - 10), f"PAD {i - 7}", font(8), DIM)
            w = "SKETCH" if LANG == "en" else "ESQUISSE"
            text(d, (cx, cy + 8), w, fit(d, w, 66, (10, 8)), INK)
    for i, s in enumerate(("ATTACK", "RELEASE", "SPACE", "LENGTH")):
        cx = 1232 + i * 80
        d.rectangle([k(cx - 3), k(215), k(cx + 3), k(380)], fill=(28, 32, 36))
        rr(d, (cx - 24, 250, cx + 24, 282), 4, HONEY)
        text(d, (cx, 195), f"F{i + 1}", font(12), LBL)
        lead_up(d, cx, s, i)
    names = (("ARP", "NOTE REPEAT", "SCALE", "CHORD", "GLOBE", "BT"), ("PATCH", "PARA", "FX", "SEQ", "SEQ PLAY", "SEQ REC"))
    used = {"SEQ", "SEQ PLAY", "SEQ REC"}
    for row in range(2):
        for i in range(6):
            cx, cy = 1218 + i * 56, 420 + row * 48
            n = names[row][i]
            rr(d, (cx - 26, cy - 19, cx + 26, cy + 19), 5, HONEY if n in used else OFF)
            text(d, (cx, cy), n, fit(d, n, 48, (10, 9, 8, 7, 6)), INK if n in used else (150, 160, 164))
    kx0, kx1, ky0, ky1, nw = 120, 1500, 520, 690, 22
    ww = (kx1 - kx0) / nw
    for i in range(nw):
        d.rectangle([k(kx0 + i * ww + 1), k(ky0), k(kx0 + (i + 1) * ww - 1), k(ky1)], fill=CREAM)
    for i in range(nw - 1):
        if i % 7 in (0, 1, 3, 4, 5):
            bx = kx0 + (i + 1) * ww
            d.rectangle([k(bx - ww * 0.32), k(ky0), k(bx + ww * 0.32), k(ky0 + 100)], fill=KEYB)
    text(d, (810, 505), "37 TOUCHES : L'ARCHET, DO2 A SOL4", font(12), LBL)
    notes(d, 735, [
        "PADS DU HAUT : LES EFFETS, UN APPUI = ON / OFF  ·  LE POTARD AU-DESSUS : SA QUANTITE (K1 : LE VOLUME)",
        "PADS DU BAS : 8 ESQUISSES  ·  UN APPUI ENREGISTRE, UN SECOND GARDE, UN PAD PLEIN REJOUE EN BOUCLE",
        "SEQ REC MAINTENU : LA DERNIERE PHRASE GARDEE  ·  SEQ PLAY : LES ESQUISSES A LA SUITE  ·  SEQ MAINTENU : LA LISTE",
        "FADERS : ATTACK, RELEASE, SPACE (LA REVERB), LENGTH (LE COUP D'ARCHET)  ·  MOLETTE : PURE",
        "ARCHET : L'ARCHET ON / OFF (OFF : LE CLAVIER NE PILOTE QUE L'ORDINATEUR)  ·  OCT- / OCT+ : PENDANT UNE ESQUISSE"], W)
    return im.resize((W, H), Image.LANCZOS)


def fm1(screen):
    W, H = 1600, 900
    im, d = canvas(W, H, "MODULAR LANDSCAPE  ·  LE FM-1")
    X0, Y0, X1, Y1 = 300, 160, 1170, 730
    rr(d, (X0 + 8, Y0 + 8, X1 + 8, Y1 + 8), 34, SHADOW)
    rr(d, (X0, Y0, X1, Y1), 34, BODY)
    for (cx, cy, name, col) in ((370, 240, "MASTER", HONEY), (470, 240, "SELECT", HONEY),
                                (370, 335, "PRESETS", HONEY), (470, 335, "ALGORITHM", OFF)):
        knob(d, cx, cy, 24, col)
        text(d, (cx, cy - 38), name, fit(d, name, 90, (11, 10, 9)), LBL)
    rr(d, (328, 393, 512, 441), 8, (44, 50, 55))
    for cx, s in ((372, "OCT-"), (468, "OCT+")):
        rr(d, (cx - 38, 400, cx + 38, 434), 5, HONEY)
        text(d, (cx, 417), s, font(12), INK)
    rr(d, (548, 196, 762, 410), 16, (22, 20, 18))
    im.paste(screen.convert("RGB").resize((k(186), k(186)), Image.NEAREST), (k(562), k(210)))
    for i, s in enumerate(("LENGTH", "ATTACK", "SPACE", "RELEASE")):
        cx = 840 + i * 92
        knob(d, cx, 240, 24, HONEY)
        text(d, (cx, 202), f"KNOB{i + 1}", font(10), LBL)
        lead_up(d, cx, s, i, top=(108, 130), y1=188)
    lead_up(d, 470, "QUANTITE", 1, top=(108, 130), y1=188)
    rr(d, (796, 290, 1140, 428), 10, (44, 50, 55))
    rows = (("FX", "SCL", "ENV", "LFO", "EDIT", "GLO"), ("HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"))
    fxb = {"FX", "SCL", "ENV", "LFO", "EDIT", "ARP", "HOME"}
    for r, names in enumerate(rows):
        for i, s in enumerate(names):
            cx, cy = 826 + i * 57, 326 + r * 64
            rr(d, (cx - 24, cy - 22, cx + 24, cy + 22), 6, HONEY if s in fxb else OFF)
            text(d, (cx, cy), s, fit(d, s, 42, (12, 10, 9)), INK if s in fxb else LBL)
    rr(d, (330, 462, 1140, 708), 18, (44, 50, 55))
    col = (1140 - 350) / 16
    for x in (0.55, 1.55, 2.55, 4.3, 5.3, 7.15, 8.15, 9.15, 11.1, 12.1, 14.4):
        cx = 350 + x * col + col / 2
        rr(d, (cx - 19, 476, cx + 19, 572), 19, (34, 38, 42))
        d.line([k(cx), k(500), k(cx), k(548)], fill=DIM, width=k(3))
    for i in range(16):
        cx = 350 + i * col + col / 2
        rr(d, (cx - 20, 590, cx + 20, 694), 20, (70, 79, 86))
        d.line([k(cx), k(614), k(cx), k(670)], fill=CREAM, width=k(3))
    # the labels on the left, the list on the right
    for y, s, x in ((240, "VOLUME", 344), (335, "PURE", 344), (417, "OCTAVE", 332)):
        text(d, (280, y), s, font(16), INK, anchor="rm")
        d.line([k(288), k(y), k(x), k(y)], fill=DIM, width=k(2))
    lst = (("FX", "SUSTAIN"), ("SCL", "BREATH"), ("ENV", "CONTRA"), ("LFO", "DRONE"), ("EDIT", "DISTANCE"),
           ("ARP", "MUTE"), ("HOME", "ECHO"), ("SAVE", "GARDER"), ("GLO", "REGLAGES"), ("PLAY REC SEQ", "MAGNETO"))
    for i, (a, b) in enumerate(lst):
        y = 200 + i * 40
        text(d, (1210, y), a, font(13), DIM, anchor="lm")
        text(d, (1470, y), b, font(16), INK, anchor="rm")
    text(d, (735, 450), "27 TOUCHES : L'ARCHET, FA2 A SOL4", font(11), LBL)
    notes(d, 770, [
        "LES BOUTONS D'EFFET : UN APPUI = ON / OFF  ·  SELECT REGLE LA QUANTITE DU DERNIER EFFET TOUCHE",
        "SAVE GARDE TON ARCHET POUR LE PROCHAIN ALLUMAGE  ·  MAINTENU 2 S : LES REGLAGES D'USINE",
        "PLAY · REC · SEQ : LE MAGNETO ET LE CARNET DE CROQUIS DE MODULAR ONE  ·  PAS DE WIDTH : LE HAUT-PARLEUR EST MONO"], W)
    return im.resize((W, H), Image.LANCZOS)


def main():
    global LANG
    OUT.mkdir(parents=True, exist_ok=True)
    img = ROOT / "docs" / "img"
    for LANG, name, fn, scr in (("fr", "commandes-smk37-fr.png", smk37, "screen-smk37.png"),
                                ("en", "controls-smk37-en.png", smk37, "screen-smk37.png"),
                                ("fr", "commandes-fm1-fr.png", fm1, "screen-fm1.png"),
                                ("en", "controls-fm1-en.png", fm1, "screen-fm1.png")):
        out = OUT / name
        fn(Image.open(img / scr)).quantize(96, dither=Image.Dither.NONE).save(out, optimize=True)
        print(f"controls: {out}")


if __name__ == "__main__":
    main()
