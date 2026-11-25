/* MODULAR LANDSCAPE (archet.c): the screen on the host. tests/archet_screen OUT-prefix: pictures at set times (PPM).
 * The tape plays here too (demo_block every 10 ms); two sketches are made from notes put in the ring (sk_extract, as
 * ar_book.c keeps them, in RAM) */
#define M1_SCREEN_NO_MAIN
#include "m1_screen.c"
#include "../firmware/src/archet.c"
static void frame(const char *pfx, const char *tag)
{
    char p[256];
    FILE *f;
    uint32_t i;
    snprintf(p, sizeof p, "%s-%s.ppm", pfx, tag);
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
        demo_block(441u);
        sk_clock += 441u;
        m1_input();
        m1_draw();
        m1_leds();
        fm1_ms += 10u;
    }
}
static uint8_t keep_buf[2][4096];
static void keep(uint32_t i, const char *name)                 /* the last phrase as sketch i (as ar_book.c, in RAM) */
{
    uint32_t notes, dur10, got, len = sk_extract(1, keep_buf[i], sizeof keep_buf[i], &notes, &dur10, &got);
    pl_item[i].data = keep_buf[i];
    pl_item[i].n = notes;
    pl_item[i].dur10 = dur10;
    str_cpy(pl_item[i].name, name, PL_NAME);
    pl_count = i + 1u;
    printf("kept %s: %u bytes, %u notes, %u.%02u s\n", name, len, notes, dur10 / 100u, dur10 % 100u);
}
static void phrase(const uint8_t *notes, uint32_t n, uint32_t step_ms)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        sk_put(notes[i], 80u);
        go(step_ms);
        sk_put(notes[i], 0);
    }
}
int main(int argc, char **argv)
{
    const char *pfx = argc > 1 ? argv[1] : "build/archet";
    static const uint8_t A[] = {50, 53, 57, 60, 57, 53, 33, 50};
    static const uint8_t B[] = {45, 48, 52, 55, 57, 55, 52, 48, 45, 28};
    fm1_ms = 1000;
    host_tracks_init();
    m1_init();
    go(800);
    frame(pfx, "0a-descente");
    go(2000);
    frame(pfx, "0b-titre");
    go(2000);
    frame(pfx, "1-vide");
    ar_show("RELEASE", 92, 2);
    go(300);
    frame(pfx, "2-reglage");
    go(2000);
    phrase(A, sizeof A, 400);
    go(5000);
    keep(0, "SKETCH 1");
    phrase(B, sizeof B, 300);
    go(4500);
    keep(1, "SKETCH 2");
    ar_redraw = 1;
    go(300);
    frame(pfx, "3-deux-croquis");
    demo_req = 1;
    go(1600);
    frame(pfx, "4-lecture");
    go(1000);
    frame(pfx, "5-lecture-plus-loin");
    printf("playing %d, sketch %u, %u.%02u s in\n", demo_on, pl_idx + 1u, demo_pos / 44100u, demo_pos % 44100u / 441u);
    ar_tape_input(1u << AR_B_SEQ, 0);
    go(100);
    pl_cursor = 1; ar_tp.hold_used = 1; ar_redraw = 1;
    go(100);
    frame(pfx, "6-liste");
    ar_tape_input(1u << AR_B_REC, 0);
    go(100);
    frame(pfx, "7-effacer");
    ar_tape_input(0, 1u << AR_B_REC);
    ar_tp.hold_used = 1;
    ar_tape_input(0, 1u << AR_B_SEQ);
    go(500);
    printf("after SEQ let go: playing %d, sketch %u\n", demo_on, pl_idx + 1u);
    ar_tape_input(1u << AR_B_PLAY, 0);
    go(300);
    printf("after PLAY: playing %d\n", demo_on);
    ar_local = 0; ar_redraw = 1;
    go(300);
    frame(pfx, "8-midi");
    printf("ok\n");
    return 0;
}
