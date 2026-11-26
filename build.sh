#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
# Modular Landscape: the two release packages, exactly as published.
#   ./build.sh          -> smk37/build/modular-landscape-smk37-elite-1.0.fwsc, fm1/build/modular-landscape-fm1-0.4.fwsc
# Needs what Felucca needs (see smk37/BUILDING.md): the JieLi toolchain ($JIELI_TOOLCHAIN) and the AC79 SDK ($AC79_SDK).
# The identity, SMK-37 Elite_894 / FM-1_894, is the same for every release (given by The Firmware Hub).
set -e
cd "$(dirname "$0")"
SMK_VERSION=1.0
FM1_VERSION=0.4.2
(cd smk37 && ARCHET=1 BOARD=smk37 MODEL=elite python3 tools/build.py --release "$SMK_VERSION" --ident 894)
(cd fm1 && BOARD=land python3 tools/build.py --release "$FM1_VERSION" --ident 894)
