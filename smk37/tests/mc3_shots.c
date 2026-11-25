/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Compagnon 2: the screen's pictures (the town's places, the keyboard's, night, snow, a knob, PLAY), drawn by
 * the firmware's own code on the host.
 *   cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -Itests -o build/host/mc3_shots tests/mc3_shots.c -lm
 *   build/host/mc3_shots OUTDIR */
#define M1_SCREEN_NO_MAIN
#include "m1_screen.c"
#include <sys/stat.h>

static char scene_dir[512];
static void scene(const char *root, const char *name)
{
    snprintf(scene_dir, sizeof scene_dir, "%s/%s", root, name);
    mkdir(scene_dir, 0755);
    dir = scene_dir;
    nframe = 0;
}
static void fader(uint32_t i, uint32_t to) { uint32_t k, from = fm1_in.fader[i]; for (k = 1; k <= 10u; k++) { fm1_in.fader[i] = (uint8_t)(from + ((int32_t)to - (int32_t)from) * (int32_t)k / 10); run(40); } }

int main(int argc, char **argv)
{
    const char *root = argc > 1 ? argv[1] : "build/mc3";
    mkdir(root, 0755);
    host_tracks_init();
    fm1_ms = 1000;
    fm1_in.fader[0] = 127; fm1_in.fader[1] = 0; fm1_in.fader[2] = 30; fm1_in.fader[3] = 40;
    no_shots = 1; m1_init(); no_shots = 0;
    scene(root, "a-ouverture"); run(600);
    no_shots = 1; run(3000); no_shots = 0;
    scene(root, "b-cles"); run(400);
    scene(root, "c-roux"); host_pad = 1; run(3000);
    scene(root, "d-roux2"); host_pad = 1; run(3000);
    scene(root, "e-knob"); host_enc[1] = -5; run(200); host_enc[1] = -5; run(400);
    no_shots = 1; run(3000); no_shots = 0;
    scene(root, "f-nuit"); fader(0, 0); run(400);
    scene(root, "g-neige"); fader(0, 127); fader(1, 120); run(1200);
    fader(1, 0);
    scene(root, "h-neon"); host_pad = 7; run(3000);
    scene(root, "i-flutes"); host_pad = 10; run(3000);
    scene(root, "j-play"); host_pressed = 1u << 7; run(100); host_released = 1u << 7; run(400);
    scene(root, "k-globe"); host_pressed = 1u << 10; run(600);
    printf("mc3_shots: %s\n", root);
    return 0;
}
