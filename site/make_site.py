#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular Landscape: the install pages, from the two trees' installers (smk37/web/compagnon/install.html,
fm1/web/compagnon-fm1/install.html) with Modular Landscape's words (site/text-*.js) and its screen.

  python3 site/make_site.py SMK37.fwsc SMK_VERSION FM1.fwsc FM1_VERSION [OUTDIR]

Writes docs/smk37/index.html and docs/fm1/index.html (the GitHub Pages site, the package inside), docs/firmware/
(the two packages), and, in OUTDIR, the same two pages as single files to open from the disk.
"""
import base64
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SITE = "https://bunglesociety.github.io/modular-landscape/"
REPO = "https://github.com/bunglesociety/modular-landscape"


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    return "".join(chr((m - i - 1) & 0xFF) for i in range(20) if (m := raw[i * 0x30 + 0x2F]) != 0x7D)


def sub1(html, old, new):
    if html.count(old) != 1:
        raise SystemExit(f"template: {old[:60]!r} found {html.count(old)} times")
    return html.replace(old, new)


def page(tree, tmpl, text, hero, pkg, version, local):
    web = ROOT / tree / "web"
    html = (web / tmpl).read_text(encoding="utf-8")
    html = re.sub(r"const TEXT = \{.*?\n\};\n", lambda _: (ROOT / "site" / text).read_text(encoding="utf-8"), html,
                  count=1, flags=re.S)
    html = sub1(html, "<title>Modular Compagnon</title>", "<title>Modular Landscape</title>")
    html = sub1(html, "← MODULAR COMPAGNON", "← MODULAR LANDSCAPE")
    html = sub1(html, "Modular Compagnon by Alexis Arakis", "Modular Landscape by Alexis Arakis")
    html = re.sub(r"https://github\.com/bunglesociety/modular-compagnon\b[^\"<]*", REPO, html)
    html = re.sub(r"github\.com/bunglesociety/modular-compagnon[^<\"]*</a>", "github.com/bunglesociety/modular-landscape</a>", html)
    html = re.sub(r'\$\("home"\)\.href = "[^"]*"', f'$("home").href = "{SITE}"', html)
    html = re.sub(r"Copyright \(C\) 2026 Alexis Arakis \(Bungle Society\) \(Modular Compagnon\)",
                  "Copyright (C) 2026 Alexis Arakis (Bungle Society) (Modular Landscape)", html)
    if 'id="map"' in html:                                   # the SMK-37's: no controls picture for Landscape
        html = sub1(html, '    <img class="map" id="map" alt="" width="1600" height="900">\n', "")
        html = sub1(html, '  const map = document.getElementById("map"); map.src = MAP[lang] || MAP.en; map.alt = t("mapAlt");\n', "")
        html = sub1(html, "/*MAP*/", "{}")
    lib = "\n".join(strip_module((web / f).read_text(encoding="utf-8")) for f in ("fm1pkg.js", "fm1ota.js"))
    raw = pkg.read_bytes()
    meta = {"version": version, "product": product_of(raw), "local": 1 if local else 0, "b64": base64.b64encode(raw).decode()}
    hero_src = "data:image/png;base64," + base64.b64encode(hero.read_bytes()).decode()
    for mark, val in (("/*LIB*/", lib), ("/*META*/", json.dumps(meta)), ("/*HERO*/", hero_src)):
        html = sub1(html, mark, val)
    html = re.sub(r"<!--.*?-->", "<!-- SPDX-License-Identifier: GPL-3.0-only\n     Copyright (C) 2026 Leo Kuroshita (@kurogedelic), H\u00fcgelton Instruments (the installer: Felucca's web/fm1ota.js, fm1pkg.js)\n     Copyright (C) 2026 Alexis Arakis (Bungle Society) (Modular Landscape)\n     The installer of Modular Landscape, made by site/make_site.py. Chrome or Edge (Web MIDI with SysEx). -->", html, count=1, flags=re.S)
    if re.search(r"Compagnon|compagnon", re.sub(r"<script>.*?</script>", "", html, flags=re.S)) or \
            re.search(r"Modular Compagnon", html.split("const TEXT", 1)[1].split("const MAP", 1)[0] if "const MAP" in html else html.split("const TEXT", 1)[1][:30000]):
        raise SystemExit(f"{tree}: the page still says Compagnon")
    return html, meta["product"]


def main(smk, smk_v, fm1, fm1_v, out=None):
    smk, fm1 = Path(smk), Path(fm1)
    docs = ROOT / "docs"
    (docs / "firmware").mkdir(parents=True, exist_ok=True)
    for f in (smk, fm1):
        shutil.copy(f, docs / "firmware" / f.name)
    jobs = (("smk37", "compagnon/install.html", "text-smk37.js", docs / "img" / "screen-smk37.png", smk, smk_v,
             f"SMK-37 - Modular Landscape {smk_v}.html"),
            ("fm1", "compagnon-fm1/install.html", "text-fm1.js", docs / "img" / "screen-fm1.png", fm1, fm1_v,
             f"FM-1 - Modular Landscape FM-1 {fm1_v}.html"))
    block = (ROOT / "site" / "block-style.html").read_text(encoding="utf-8") + \
        (ROOT / "site" / "block-body.html").read_text(encoding="utf-8")
    (ROOT / "site" / "squarespace-block.html").write_text(block, encoding="utf-8")      # bunglesociety.com: a Code block
    home = ("<!doctype html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"utf-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>Modular Landscape</title>\n"
            "<style>html, body { margin: 0; }</style>\n</head>\n<body>\n" + block.replace(SITE, "./") + "</body>\n</html>\n")
    (docs / "index.html").write_text(home, encoding="utf-8")
    print("docs/index.html, site/squarespace-block.html")
    for tree, tmpl, text, hero, pkg, ver, local_name in jobs:
        html, product = page(tree, tmpl, text, hero, pkg, ver, False)
        (docs / tree).mkdir(exist_ok=True)
        (docs / tree / "index.html").write_text(html, encoding="utf-8")
        print(f"docs/{tree}/index.html  {product}  {ver}")
        if out:
            html, _ = page(tree, tmpl, text, hero, pkg, ver, True)
            (Path(out) / local_name).write_text(html, encoding="utf-8")
            print(f"{Path(out) / local_name}")


if __name__ == "__main__":
    if len(sys.argv) not in (5, 6):
        raise SystemExit(__doc__)
    main(*sys.argv[1:])
