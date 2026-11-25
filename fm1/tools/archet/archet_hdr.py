#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""ARCHET (MC_ARCHET builds): put the bowed-guitar set (archet.json zones, archet.bin IMA ADPCM, made by
build_archet.py from Alexis's takes) into felucca_samples.h as SAMPLE set 0, after the SLICE BREAK's data."""
import json, re, sys
from pathlib import Path
here = Path(__file__).resolve().parent
hdr = Path(sys.argv[1])
src = hdr.read_text()
j = json.loads((here / "archet.json").read_text())
blob = (here / "archet.bin").read_bytes()
m = re.search(r'static const uint8_t SMP_DATA\[(\d+)\] = \{(.*?)\n\};', src, re.S)
base = int(m.group(1))
rows = ",\n".join(", ".join(str(b) for b in blob[i:i + 32]) for i in range(0, len(blob), 32))
data = f"static const uint8_t SMP_DATA[{base + len(blob)}] = {{{m.group(2).rstrip().rstrip(",")},\n{rows}\n}};"
src = src[:m.start()] + data + src[m.end():]
drone = j.get("drone", [])                  # set 1, DRONE: GRAIN's material (its own few zones, the same data)
zl = "".join("    {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d},\n" % (z[0] + base, *z[1:11]) for z in j["zones"] + drone)
# (39) played once (LOOP off), where each zone's note ends (its first stroke: eng_sample.c fades it out before), 0: its end
osl = ", ".join(str(z[11] if len(z) > 11 else 0) for z in j["zones"] + drone)
# (40) each zone's period, in its own samples, Q8 (eng_sample.c's comb, the wheel), 0: none
perl = ", ".join(str(z[12] if len(z) > 12 else 0) for z in j["zones"] + drone)
src = re.sub(r'static const smp_zone_t SMP_ZONES\[\] = \{.*?\n\};', lambda _: "static const smp_zone_t SMP_ZONES[] = {\n" + zl + "};", src, flags=re.S)
src = re.sub(r'static const smp_zone_t SMP_ZONES\[\] = \{(.*?)\n\};', lambda mm: mm.group(0) + "\nstatic const uint32_t AR_OS_END[] = {" + osl + "};\nstatic const uint32_t AR_PER[] = {" + perl + "};", src, count=1, flags=re.S)
src = re.sub(r'static const smp_set_t SMP_SETS\[\] = \{.*?\n\};', lambda _: 'static const smp_set_t SMP_SETS[] = {\n    {"ARCHET", 0, %d},\n' % len(j["zones"]) + ('    {"DRONE", %d, %d},\n' % (len(j["zones"]), len(drone)) if drone else '') + '};', src, flags=re.S)
if drone:
    src = re.sub(r'#define SMP_NSETS \d+', '#define SMP_NSETS 2', src)
    src = re.sub(r'static const uint8_t SMP_SET_ORIG\[\] = \{.*?\};', 'static const uint8_t SMP_SET_ORIG[] = {0, 1};', src, flags=re.S)
    src = re.sub(r'#define SMP_SET_NAMES_INIT .*', '#define SMP_SET_NAMES_INIT "ARCHET", "DRONE"', src)
    src = re.sub(r'static const char \*const SMP_SET_NAMES\[\] = \{.*?\};', 'static const char *const SMP_SET_NAMES[] = {"ARCHET", "DRONE"};', src, flags=re.S)
src = re.sub(r'static const preset_t SMP_PRESET_TABLE\[\] = \{.*?\n\};', lambda _: 'static const preset_t SMP_PRESET_TABLE[] = {\n    {"ARCHET", {0, 0, 0, 1, 127, 0, 0, 0}, {20, 80, 127, 70}, 0, 0},\n};', src, flags=re.S)
src = src.replace('"NONE"', '"ARCHET"')
hdr.write_text(src)
print(f"archet: {len(j['zones'])} zones, {len(blob)} B ADPCM -> {hdr}")
