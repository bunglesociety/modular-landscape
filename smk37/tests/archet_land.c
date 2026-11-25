/* MODULAR LANDSCAPE (archet.c, ar_land.c): the bow (C2 .. G4) and the 16 pads on the host. tests/archet_land OUTDIR:
 * OUTDIR/land.wav (a melody on the bow over its low notes, kept on pad 1, another on pad 2 (37: the top row is the options), both played back) and
 * pictures OUTDIR/NAME.ppm. Exit 1 on a failure. */
#define M1_SCREEN_NO_MAIN
#include "m1_screen.c"
#include "../firmware/src/archet.c"
static FILE *wav;
static const char *dir;
static uint32_t frames, clips, fails, peak;
#define FAIL(...) (printf("archet_land: FAIL " __VA_ARGS__), printf("\n"), fails++)
static void snap(const char *name)
{
    char p[512];
    FILE *f;
    uint32_t i;
    snprintf(p, sizeof p, "%s/%s.ppm", dir, name);
    f = fopen(p, "wb");
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t v = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t rgb[3] = {(uint8_t)((v >> 11) << 3), (uint8_t)(((v >> 5) & 63u) << 2), (uint8_t)((v & 31u) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void go(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        uint32_t b;
        m1_input();
        m1_draw();
        m1_leds();
        for (b = 0; b < 10u * 44100u / 1000u / CTL + (fm1_ms % 3u == 0u); b++) {
            int32_t o[2 * CTL];
            uint32_t i;
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t l = o[2 * i] < 0 ? -o[2 * i] : o[2 * i];
                if (l > 32700) clips++;
                if ((uint32_t)l > peak) peak = (uint32_t)l;
                wav_put(wav, o[2 * i], o[2 * i + 1]);
                frames++;
            }
        }
        fm1_ms += 10u;
    }
}
static void kp(uint32_t note, uint32_t vel) { input_on(&trk[0], note, vel); }
static void keyup(uint32_t note) { input_off(&trk[0], note); }
static void pad(uint32_t q) { ar_pad_tap(q); go(120); }     /* (m1_screen.c's pads have no release: as ar_pads does) */
static uint32_t live(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < trk_nvoice(&trk[k]); i++)
        n += trk[k].v[i].active;
    return n;
}
int main(int argc, char **argv)
{
    static const uint8_t MEL[8] = {57, 60, 57, 55, 53, 52, 50, 53};
    static const uint8_t BAS[4] = {38, 46, 43, 45};                          /* D2 Bb2 G2 A2: the bow */
    char path[512];
    uint32_t k;
    double eb = 0, ek = 0;
    dir = argc > 1 ? argv[1] : "build";
    snprintf(path, sizeof path, "%s/land.wav", dir);
    wav = fopen(path, "wb");
    wav_hdr(wav, 0);
    fm1_ms = 1000;
    host_tracks_init();
    song.master_q12 = 4096;
    for (k = 0; k < 4u; k++)
        fm1_in.fader[k] = 0xFF;                              /* (not read yet: the values of ar_fv) */
    m1_init();
    for (k = 0; k < 4u; k++) {                               /* (m1_screen.c's apply_preset_to loads VILLE) */
        track_t *t = &trk[k];
        int bass = k == 1u || k == 2u;
        host_preset(t, bass ? 0u : ENGI_SAMPLE, bass ? 7u : 0u);
        t->p[P_AMODE] = 0; t->p[P_TRANS] = k ? 0 : -12; t->p[P_LD_FLT] = 0;
        t->p[P_CHOR] = 0; t->p[P_DLY] = 0; t->p[P_E3] = bass ? t->p[P_E3] : 0; t->p[P_SPRD] = bass ? 0 : 40;
        t->p[P_VOICE] = 0;
    }
    smk_modw = 100;                                          /* (the wheel up: the long fade) */
    ar_wheel(1);
    ar_bow_apply();
    go(4800);
    snap("a_home");
    kp(38, 90); go(300);                                     /* D2: its own stroke */
    if (live(0) < 1u) FAIL("D2: no voice");
    keyup(38); go(1500);
    kp(64, 90); go(300); if (live(0) < 1u) FAIL("E4: no voice"); keyup(64); go(1200);
    kp(67, 90); go(300); if (live(0) < 1u) FAIL("G4: no voice"); keyup(67); go(1200);
    kp(72, 90); go(300); if (live(0) < 1u) FAIL("C5: no voice"); keyup(72); go(1200);   /* (42: the keyboard to its top) */
    kp(73, 90); go(300); if (live(0)) FAIL("C#5 sounds"); keyup(73); go(300);
    kp(30, 90); go(300); if (live(0) || live(1)) FAIL("a key under C2 sounds"); keyup(30); go(300);
    ar_tp_on(30, 90); go(300);                               /* an older sketch's bass note: the bow, an octave up */
    if (live(3) < 1u) FAIL("a sketch's note under C2: silent");
    ar_tp_off(30); go(1500);
    pad(0); go(100);                                         /* pad 1: record */
    snap("b_rec_wait");
    for (k = 0; k < 8u; k++) {
        if (!(k & 1u)) kp(BAS[k / 2u], 85);
        kp(MEL[k], 85); go(420); keyup(MEL[k]); go(80);
        if (k & 1u) keyup(BAS[k / 2u]);
    }
    snap("c_recording");
    go(800);
    pad(0); go(300);                                         /* keep it */
    if (pad_item[0] == 0xFFu) FAIL("pad 1: nothing kept");
    else printf("pad 1: %u notes, %u.%02u s\n", pl_item[pad_item[0]].n, pl_item[pad_item[0]].dur10 / 100u, pl_item[pad_item[0]].dur10 % 100u);
    pad(1); go(100);                                        /* pad 2 */
    for (k = 0; k < 8u; k++) { kp(MEL[7u - k] + 2u, 80); go(300); keyup(MEL[7u - k] + 2u); go(60); }
    pad(1); go(300);
    if (pad_item[1] == 0xFFu) FAIL("pad 2: nothing kept");
    pad(0); go(1500);                                        /* pad 1 plays */
    if (!demo_on || !pl_loop) FAIL("pad 1: not playing");
    snap("d_pad1_playing");
    printf("tape part while it plays: bow %u\n", live(3));
    go(4000);
    pad(0); go(400);
    if (demo_on) FAIL("pad 1 again: still playing");
    pad(1); go(3500);
    snap("e_pad2_playing");
    pad(1); go(500);
    ar_pd[1].down = 1; ar_pd[1].done = 0; ar_pd[1].ms = fm1_ms; go(1200);       /* pad 2 held: erase? */
    snap("f_erase_ask");
    ar_pd[1].down = 0;
    if (pad_item[1] == 0xFFu) FAIL("pad 2 erased without SEQ REC");
    ar_tape_input(1u << AR_B_REC, 0); go(100); ar_tape_input(0, 1u << AR_B_REC); go(400);
    if (pad_item[1] != 0xFFu) FAIL("SEQ REC: pad 2 not erased");
    if (m1_keep_req) FAIL("SEQ REC also kept a phrase");
    go(1500);
    fclose(wav);
    { FILE *f = fopen(path, "r+b"); wav_hdr(f, frames); fclose(f); }
    printf("archet_land: %s, %u s, peak %u, clipped %u, %u failures\n", path, frames / 44100u, peak, clips, fails);
    (void)eb; (void)ek;
    return fails ? 1 : 0;
}
