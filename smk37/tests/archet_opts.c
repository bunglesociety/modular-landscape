/* MODULAR LANDSCAPE 37 (archet.c): the options on the host. tests/archet_opts OUTDIR: OUTDIR/opts.wav (the bow plain,
 * then TENUE, the BREATH, the WEAR, ENSEMBLE, the PEDALE, FIGE) and OUTDIR/opts.ppm. Exit 1 on a failure. */
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
static uint32_t live(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < trk_nvoice(&trk[k]); i++)
        n += trk[k].v[i].active;
    return n;
}
static int clk_on; static int32_t clk_max, clk_prev;
static double rms_ms(uint32_t ms)                    /* the level over the next ms (rendered), dBFS */
{
    double e = 0;
    uint32_t n = 0, end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        uint32_t b;
        m1_input(); m1_draw();
        for (b = 0; b < 10u * 44100u / 1000u / CTL; b++) {
            int32_t o[2 * CTL];
            uint32_t i;
            mix_block(o, CTL);
            { int64_t lr = 0, ll = 0, rr = 0; uint32_t w = ar_gw, j;       /* (as audio.c: the screen's stereo image) */
              for (j = 0; j < CTL; j++) { int32_t l = o[2 * j] >> 4, r = o[2 * j + 1] >> 4; lr += l * r; ll += l * l; rr += r * r;
                if (j & 1u) { ar_gl[w & (AR_GN - 1u)] = (int16_t)o[2 * j]; ar_gr[w & (AR_GN - 1u)] = (int16_t)o[2 * j + 1]; w++; } }
              ar_gw = w; ar_c_lr += lr - (ar_c_lr >> 5); ar_c_ll += ll - (ar_c_ll >> 5); ar_c_rr += rr - (ar_c_rr >> 5); }
            for (i = 0; i < CTL; i++) {
                int32_t l = o[2 * i] < 0 ? -o[2 * i] : o[2 * i];
                if (l > 32700) clips++;
                if ((uint32_t)l > peak) peak = (uint32_t)l;
                if (clk_on) { int32_t dd = o[2 * i] - clk_prev; if (dd < 0) dd = -dd; if (dd > clk_max) clk_max = dd; }
                clk_prev = o[2 * i];
                wav_put(wav, o[2 * i], o[2 * i + 1]);
                e += (double)o[2 * i] * o[2 * i] + (double)o[2 * i + 1] * o[2 * i + 1];
                n += 2; frames++;
            }
        }
        fm1_ms += 10u;
    }
    return 10 * log10(e / (n ? n : 1) / (32768.0 * 32768.0) + 1e-12);
}
static void chord(const uint8_t *c, uint32_t nn, int on) { uint32_t i; for (i = 0; i < nn; i++) on ? kp(c[i], 90) : keyup(c[i]); }
int main(int argc, char **argv)
{
    static const uint8_t DM[3] = {50, 53, 57};          /* D3 F3 A3 */
    char path[512];
    uint32_t k;
    double a, b, c;
    dir = argc > 1 ? argv[1] : "build";
    snprintf(path, sizeof path, "%s/opts.wav", dir);
    wav = fopen(path, "wb");
    wav_hdr(wav, 0);
    fm1_ms = 1000;
    host_tracks_init();
    song.master_q12 = 4096;
    for (k = 0; k < 4u; k++)
        fm1_in.fader[k] = 0xFF;
    m1_init();
    for (k = 0; k < 4u; k++) {                               /* (as ar_init) */
        track_t *t = &trk[k];
        int bass = k == 2u;
        host_preset(t, bass ? 0u : ENGI_SAMPLE, bass ? 7u : 0u);
        t->p[P_AMODE] = 0; t->p[P_TRANS] = k == 0u || k == 1u ? -12 : 0; t->p[P_LD_FLT] = 0;
        t->p[P_CHOR] = 0; t->p[P_DLY] = 0; t->p[P_SPRD] = bass ? 0 : 40; t->p[P_VOICE] = 0;
        if (k == 1u) { t->p[P_LRATE] = 18; t->p[P_LD_AMP] = 30; }
    }
    smk_modw = 0; ar_wheel(1); ar_bow_apply();
    rms_ms(500);
    /* plain: a held chord dies with the stroke */
    chord(DM, 3, 1); a = rms_ms(500); rms_ms(3000); b = rms_ms(500); chord(DM, 3, 0); rms_ms(2500);
    printf("plain: %.1f dB at the start, %.1f dB after 3.5 s held\n", a, b);
    if (b > -60) FAIL("plain: still sounding after 3.5 s (%.1f dB)", b);
    /* TENUE */
    ar_option(8); if (!ar_ten) FAIL("pad 9: TENUE not on");
    chord(DM, 3, 1); a = rms_ms(500); rms_ms(5000); b = rms_ms(500);
    printf("TENUE: %.1f dB at the start, %.1f dB after 5.5 s held\n", a, b);
    if (b < a - 12) FAIL("TENUE: the chord does not hold (%.1f -> %.1f dB)", a, b);
    /* BREATH, held */
    smk_modw = 127; ar_wheel(0); c = rms_ms(1500); smk_modw = 0; ar_wheel(0);
    printf("WHEEL 127 (the bow's noise away): %.1f dB (held, before %.1f)\n", c, b);
    if (c < b - 8 || c > b + 2) FAIL("WHEEL: level %.1f vs %.1f", c, b);
    printf("SEG wheel0 %u wheel127 %u\n", frames, frames);
    chord(DM, 3, 0); rms_ms(3000);
    if (live(0)) FAIL("TENUE: still sounding 3 s after the keys");
    /* 48: RESPIRATION (pad 10, K2): the held level swells and ebbs */
    ar_kv[1] = 127; ar_option(9); if (!ar_resp || !trk[0].p[P_LD_AMP]) FAIL("pad 10: RESPIRATION not on");
    { double lo = 0, hi = -200, x; uint32_t t0;
      kp(62, 90); rms_ms(800);
      for (t0 = 0; t0 < 24u; t0++) { x = rms_ms(250); if (x > hi) hi = x; if (!t0 || x < lo) lo = x; }
      keyup(62); rms_ms(2500); ar_option(9);
      printf("RESPIRATION: the held note between %.1f and %.1f dB over 6 s\n", lo, hi);
      if (hi - lo < 8) FAIL("RESPIRATION: too shallow (%.1f dB)", hi - lo);
      if (hi - lo < 3) FAIL("RESPIRATION: no swell (%.1f dB)", hi - lo); }
    /* 48: LARGEUR (pad 13, K5), DISTANCE (pad 14, K6) */
    ar_kv[4] = 127; ar_option(12); if (trk[0].p[P_SPRD] != 127) FAIL("pad 13: LARGEUR %d", trk[0].p[P_SPRD]);
    ar_option(12); if (trk[0].p[P_SPRD] != 40) FAIL("LARGEUR off: %d", trk[0].p[P_SPRD]);
    kp(62, 90); a = rms_ms(1500); keyup(62); rms_ms(2000);
    ar_kv[5] = 127; ar_option(13);
    if (trk[0].p[P_LEVEL] >= 108 || trk[0].p[P_REV] <= trk[3].p[P_REV] - 200) FAIL("pad 14: DISTANCE");
    kp(62, 90); b = rms_ms(1500); keyup(62); rms_ms(2000); ar_option(13);
    printf("DISTANCE: %.1f dB (near %.1f)\n", b, a);
    if (b > a - 1) FAIL("DISTANCE: not further away");
    /* 44: SOURDINE, ECHO */
    ar_ten = 0; ar_bow_apply();
    ar_kv[6] = 127; ar_option(14); if (trk[0].p[P_E4] > 40) FAIL("SOURDINE: CUT %d", trk[0].p[P_E4]);
    kp(62, 90); a = rms_ms(1500); keyup(62); rms_ms(2000); ar_option(14);
    printf("SOURDINE: %.1f dB\n", a);
    kp(62, 90); rms_ms(300); keyup(62); rms_ms(600); a = rms_ms(1500); rms_ms(2000);
    ar_kv[7] = 100; ar_option(15);
    kp(62, 90); rms_ms(300); keyup(62); rms_ms(600); b = rms_ms(1500); rms_ms(4000); ar_option(15);
    printf("ECHO: the tail %.1f dB (without %.1f)\n", b, a);
    if (b < a + 6) FAIL("ECHO: no echo heard");
    ar_ten = 1; ar_bow_apply();
    /* PEDALE */
    kp(38, 90); rms_ms(300); ar_option(11);
    if (ar_ped_note != 38u) FAIL("pad 12: the pedal is not the key held (%u)", ar_ped_note);
    keyup(38); rms_ms(6000);
    if (!live(1)) FAIL("PEDALE: silent 6 s on");
    kp(57, 90); rms_ms(800); keyup(57); a = rms_ms(2000);
    { ar_kv[4] = 127; ar_option(12); ar_option(15); chord(DM, 3, 1); rms_ms(1500); ar_lab.name[0] = 0; { int32_t mx = 0; uint32_t j; for (j = 0; j < AR_GN; j++) if (abs(ar_gl[j]) > mx) mx = abs(ar_gl[j]); printf("GONIO max %d ll %lld lr %lld gw %u\n", mx, (long long)ar_c_ll, (long long)ar_c_lr, ar_gw); } ar_redraw = 1; m1_draw(); snap("gonio-wide");
      chord(DM, 3, 0); rms_ms(2000); ar_option(12); ar_option(15); }
    snap("opts");
    ar_option(11); rms_ms(6000);
    if (live(1)) FAIL("PEDALE off: still sounding 6 s after");
    /* 50: CONTRA (pad 11, K3): the lowest key held, an octave under, on part 3; it follows the bass */
    ar_option(10); if (!ar_contra) FAIL("pad 11: CONTRA not on");
    chord(DM, 3, 1); rms_ms(500);
    if (ar_contra_note != 50u || live(2) != 1u) FAIL("CONTRA: key %u, voices %u (50, 1)", ar_contra_note, live(2));
    kp(45, 90); rms_ms(500);
    if (ar_contra_note != 45u) FAIL("CONTRA: does not follow the bass (%u)", ar_contra_note);
    keyup(45); rms_ms(300);
    if (ar_contra_note != 50u) FAIL("CONTRA: does not come back (%u)", ar_contra_note);
    chord(DM, 3, 0); rms_ms(3000);
    if (ar_contra_note || live(2)) FAIL("CONTRA: still sounding (%u, %u)", ar_contra_note, live(2));
    printf("CONTRA: follows the lowest key, an octave under\n");
    ar_option(10);
    { uint32_t f0, f1;                                    /* one held note, the wheel down then up (analysed apart) */
      ar_freeze = 0; ar_ten = 1; ar_bow_apply(); rms_ms(3000);
      kp(62, 100); rms_ms(1000); f0 = frames; rms_ms(2000);
      smk_modw = 127; ar_wheel(0); rms_ms(500); f1 = frames; rms_ms(2000);
      smk_modw = 0; ar_wheel(0); keyup(62); rms_ms(2500);
      printf("NOTE D3 wheel0 at %u, wheel127 at %u (2 s each)\n", f0, f1); }
    fclose(wav);
    { FILE *f = fopen(path, "r+b"); wav_hdr(f, frames); fclose(f); }
    printf("archet_opts: %s, %u s, peak %u, clipped %u, %u failures\n", path, frames / 44100u, peak, clips, fails);
    return fails ? 1 : 0;
}
