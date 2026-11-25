/* MODULAR LANDSCAPE (archet.c): F4, the grain, on the host. tests/archet_grain OUT.wav: the same chords at F4 0, 64,
 * 100, 127 (6 s each), the levels of the bow and the clouds printed */
#define M1_SCREEN_NO_MAIN
#include "m1_screen.c"
#include "../firmware/src/archet.c"
static FILE *wav;
static uint32_t frames, clips;
static double e2[2], ell, err, elr;
static void go(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        uint32_t b;
        for (b = 0; b < 10u * 44100u / 1000u / CTL + (fm1_ms % 3u == 0u); b++) {
            int32_t o[2 * CTL];
            uint32_t i;
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t l = o[2 * i], r = o[2 * i + 1];
                if (l > 32700 || l < -32700) clips++;
                ell += (double)l * l; err += (double)r * r; elr += (double)l * r;
                wav_put(wav, l, r);
                frames++;
            }
        }
        fm1_ms += 10u;
    }
}
int main(int argc, char **argv)
{
    static const uint8_t CH[3][3] = {{41, 48, 57}, {46, 53, 58}, {43, 50, 55}};
    static const uint8_t G[4] = {0, 64, 100, 127};
    uint32_t gi, c, k;
    wav = fopen(argc > 1 ? argv[1] : "build/grain.wav", "wb");
    wav_hdr(wav, 0);
    fm1_ms = 1000;
    host_tracks_init();
    song.master_q12 = 4096;
    m1_init();
    for (gi = 0; gi < 4u; gi++) {
        ar_grain = G[gi];
        ar_grain_apply();
        ell = err = elr = 0;
        for (c = 0; c < 3u; c++) {
            for (k = 0; k < 3u; k++) ar_layer_on(CH[c][k], 90);
            go(1600);
            for (k = 0; k < 3u; k++) { trk_note_off(&trk[0], CH[c][k]); ar_layer_off(CH[c][k]); }
            go(400);
        }
        go(1000);
        printf("F4 %3u: bow LEVEL %3d, clouds LEVEL %3d, rms %.1f dB, corr L/R %.2f, voices in clouds %u+%u\n", G[gi],
               trk[0].p[P_LEVEL], trk[2].p[P_LEVEL], 10 * log10((ell + err) / 2 / (7.0 * 44100) / (32768.0 * 32768)),
               elr / sqrt(ell * err + 1), ar_gr_live(&trk[2]), ar_gr_live(&trk[3]));
    }
    fclose(wav);
    { FILE *f = fopen(argc > 1 ? argv[1] : "build/grain.wav", "r+b"); wav_hdr(f, frames); fclose(f); }
    printf("clipped %u\n", clips);
    return 0;
}
