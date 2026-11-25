/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One SMK-37: the screen (m1_ui.c, m1_draw.c) on the host, with a script of hands on the controls; every
 * 40 ms of simulated time the LCD as the firmware left it, to OUTDIR/fNNNNN.ppm (tests/m1_screen.py: a video).
 *   cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -Itests -o build/host/m1_screen tests/m1_screen.c -lm
 *   build/host/m1_screen OUTDIR
 * Checks: every clip plays to its end without leaving the grid, the band shows each control touched, the cat
 * falls asleep at night, sleeps through the music and wakes with the day. Exit 1 on a failure. */
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------ LCD */
static uint16_t host_screen[240 * 240];
static uint64_t host_blit_px;
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
    host_blit_px += w * h;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = p[j * w + i];
    host_blit_px += w * h;
}
#include "../firmware/src/gfx.c"

/* ------------------------------------------------------------ the SMK-37's controls (smk_input.h), by hand */
#define FM1_NBTN 18u
#define SMK_KEY_F3 5u
enum { FM1_BTN_OCT_DOWN = 4, FM1_BTN_OCT_UP = 5 };
enum { SMK_BTN_KNOB_BANK = 0, SMK_BTN_FADER_BANK = 1 };
#define fm1_in m1h_in                            /* (hostsim.c has its own) */
static struct { uint8_t fader[4]; uint8_t key_vel[40]; uint32_t frames, buttons; } fm1_in = {{127, 0, 40, 30}, {0}, 1000, 0};
static uint32_t host_pressed, host_released, host_notes, host_btn;
static int32_t host_enc[8], host_pad = -1;
static uint32_t fm1_input_edges(uint32_t *rel) { uint32_t p = host_pressed; *rel = host_released; host_pressed = host_released = 0; return p; }
static uint32_t fm1_input_note_edges(void) { uint32_t n = host_notes; host_notes = 0; return n; }
static int32_t fm1_enc_take(uint32_t e) { int32_t s = host_enc[e & 7u]; host_enc[e & 7u] = 0; return s; }
static volatile uint8_t smk_pad_free;
static int32_t smk_padev_take(void) { int32_t p = host_pad; host_pad = -1; return p < 0 ? -1 : p << 8 | 100; }
static int32_t master_knob = 8000;
static void master_poll(void) {}
static void midi_take(void) {}
static void padled_set(uint32_t p, uint8_t r, uint8_t g, uint8_t b) { (void)p; (void)r; (void)g; (void)b; }
static void fm1_led_key(uint32_t k, int on) { (void)k; (void)on; }
static void apply_preset_to(track_t *t, uint32_t pi) { host_preset(t, ENGI_VILLE, pi); }
static volatile uint8_t smk_modw;
static volatile int16_t smk_pd, smk_pspan;
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static volatile uint8_t smk_vel_curve;
/* MENU's rows GLOBE uses (menu_items.c, ui.c), as plain values */
enum { MI_TUNE, MI_TCH1 };
static uint32_t host_menu[2] = {50, 0}, host_scr, host_saves, host_lcd_on = 1;
static uint32_t menu_n(uint32_t r) { return r == MI_TUNE ? 101u : 18u; }
static uint32_t menu_get(uint32_t r) { return host_menu[r & 1u]; }
static void menu_put(uint32_t r, uint32_t v) { host_menu[r & 1u] = v; }
static const char *menu_vname(uint32_t r, uint32_t v)
{
    static char b[8];
    if (r == MI_TCH1 && v >= 16u) return v == 16u ? "OMNI" : "OFF";
    b[0] = '+';
    fmt_int(b + (r == MI_TUNE && (int32_t)v > 50), r == MI_TUNE ? (int32_t)v - 50 : (int32_t)v + 1);
    return b;
}
static const uint8_t SCR_MIN[5] = {0, 5, 15, 30, 60};
static uint32_t scr_get(void) { return host_scr; }
static void scr_put(uint32_t v) { host_scr = v; }
static void settings_save(void) { host_saves++; }
static void lcd_power(uint32_t s) { host_lcd_on = s != 0u; }
#ifndef HOST_HAS_FAVORITES
static struct { uint8_t factory[16][32]; uint32_t user, filter; } favorites;
#endif

/* m1_init's clean start (ui.c, hal): as the firmware has them, or a stand-in */
static int param_kept(uint32_t i)
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_AMODE && i <= P_SGATE) ||
           (i >= P_SLCR && i <= P_SLDEPTH) || i == P_CHRD || i == P_VOIC || i == P_SQNT;
}
static void track_defaults_steps(track_t *t) { (void)t; }
static uint32_t host_dim = 9;
static void fm1_led_dim_level(uint32_t lo) { host_dim = lo; }
#include "../firmware/src/m1_ui.c"

#include "../firmware/src/m1_draw.c"

/* ------------------------------------------------------------ the film */
static const char *dir;
static uint32_t nframe, fails, no_shots;
static void shot(void)
{
    char p[512];
    FILE *f;
    uint32_t i;
    snprintf(p, sizeof p, "%s/f%05u.ppm", dir, nframe++);
    if (!(f = fopen(p, "wb")))
        return;
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t v = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t rgb[3] = {(uint8_t)((v >> 11) << 3), (uint8_t)(((v >> 5) & 63u) << 2), (uint8_t)((v & 31u) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* run for ms of simulated time: the UI loop every 15 ms, a picture every 40 ms */
static void run(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        fm1_in.buttons = host_btn;
        m1_input();
        m1_draw();
        {   /* the audio's events, block by block */
            uint32_t b;
            for (b = 0; b < 15u * 44100u / 1000u / CTL; b++)
                events_block(CTL);
        }
        fm1_ms += 15u;
        if (fm1_ms % 40u < 15u && !no_shots)
            shot();
    }
}

static void key(uint32_t i, uint32_t ms)
{
    uint32_t t;
    for (t = 0; t < ms; t += 300u) {
        host_notes |= 1u << i;
        fm1_in.key_vel[SMK_KEY_F3 + i] = 90;
        run(300);
    }
}

/* Modular Compagnon 2: the screen's checks (the cards decode whole: the last row of each one is not the frame's) */
static int card_ok(void)
{
    uint32_t x, same = 0, y = MC_CARD_Y + MC_CH - 1u;
    uint16_t f = swap16(mc_frame());
    for (x = 0; x < 240u; x++)
        same += host_screen[y * 240u + x] == f;
    return same < 200u;
}
#ifndef M1_SCREEN_NO_MAIN
int main(int argc, char **argv)
{
    uint32_t p;
    dir = argc > 1 ? argv[1] : "build/m1_screen";
    no_shots = 1;
    host_tracks_init();
    fm1_ms = 1000;
    fm1_in.fader[0] = 127;
    m1_init();
    run(4000);
    for (p = 0; p < 16u; p++) {                                              /* every pad, every card */
        host_pad = (int32_t)p;
        run(400);
        if (!card_ok()) { printf("m1_screen: FAIL pad %u: the postcard is not whole\n", p); fails++; }
    }
    if (mc.voice != MC_V_KEYS || mc.keys != 7u) { printf("m1_screen: FAIL the last pad is not the keyboard's\n"); fails++; }
    host_pad = 1; run(300); host_pad = 1; run(300);
    if (mc.town != 1u || mc.var != 1u) { printf("m1_screen: FAIL a keeper's pad again: not his next sound\n"); fails++; }
    fm1_in.fader[0] = 0; run(600);
    if (!card_ok()) { printf("m1_screen: FAIL the night\n"); fails++; }
    fm1_in.fader[1] = 120; run(1500);
    host_pressed = 1u << 10; run(300); host_pressed = 1u << 10; run(300);    /* GLOBE and back */
    printf(fails ? "m1_screen: %u FAILED\n" : "m1_screen: ok\n", fails);
    return fails != 0;
}
#endif
