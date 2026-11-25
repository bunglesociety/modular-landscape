/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Landscape on the FM-1 on the host (from Modular Koi's koi_fm1.c): its controls (ls_ui.c), the tape and the
 * sketchbook (m1_tape.c, m1_book.c), the bow (eng_sample.c, the ARCHET set) and the screen (ls_draw.c): a script of
 * hands, the sound to OUTDIR/ls.wav, pictures to OUTDIR/NAME.ppm. Exit 1 on a failure.
 *   cc -O1 -w -DMODULAR_ONE=1 -DFELUCCA_SLICE=0 -DMODULAR_LANDSCAPE=1 -DMC_ARCHET=1 -DLS_HOST -Ibuild/gen -Ifirmware/src -Itests -o build/host/ls_fm1 tests/ls_fm1.c -lm
 *   build/host/ls_fm1 OUTDIR */
#define main hostsim_main
#include "hostsim.c"
#undef main

/* ------------------------------------------------------------ LCD */
static uint16_t host_screen[240 * 240];
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = p[j * w + i];
}
#include "../firmware/src/gfx.c"

/* ------------------------------------------------------------ the FM-1's panel (panel.c), by hand */
#define FM1_NBTN 14u
enum { FM1_BTN_OCT_DOWN = 0, FM1_BTN_OCT_UP = 1 };   /* (fm1_input.h) */
enum { B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static struct { uint8_t btn[NB]; } panel = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}};   /* (label = matrix id) */
static uint32_t panel_btn_of(uint32_t id) { return id < NB ? id : NB; }
static uint32_t host_pressed, host_released, host_notes, host_leds;
static int32_t host_enc[NE];
static uint32_t fm1_input_edges(uint32_t *rel) { uint32_t p = host_pressed; *rel = host_released; host_pressed = host_released = 0; return p; }
static uint32_t fm1_input_note_edges(void) { uint32_t n = host_notes; host_notes = 0; return n; }
static int32_t panel_enc(uint32_t e) { int32_t s = host_enc[e]; host_enc[e] = 0; return s; }
static int32_t master_knob = 8000;
static void master_poll(void) {}
static void midi_take(void) {}
static void fm1_led_key(uint32_t k, int on) { host_leds = on ? host_leds | 1u << k : host_leds & ~(1u << k); }
static void apply_preset_to(track_t *t, uint32_t pi) { host_preset(t, ENGI_SAMPLE, pi); }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
/* MENU's rows GLO uses (menu_items.c, ui.c), as plain values */
enum { MI_TUNE, MI_TCH1, MI_LOWCUT };
static uint32_t host_menu[3] = {50, 0, 0}, host_scr, host_saves, host_lcd_on = 1;
static uint32_t menu_n(uint32_t r) { return r == MI_TUNE ? 101u : r == MI_LOWCUT ? 3u : 18u; }
static uint32_t menu_get(uint32_t r) { return host_menu[r % 3u]; }
static void menu_put(uint32_t r, uint32_t v) { host_menu[r % 3u] = v; }
static const char *menu_vname(uint32_t r, uint32_t v)
{
    static char b[8];
    static const char *const EQ[3] = {"OFF", "LOW CUT", "BASS+"};
    if (r == MI_LOWCUT) return EQ[v % 3u];
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
static int param_kept(uint32_t i)
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_AMODE && i <= P_SGATE) ||
           (i >= P_SLCR && i <= P_SLDEPTH) || i == P_CHRD || i == P_VOIC || i == P_SQNT;
}
static void track_defaults_steps(track_t *t) { memset(t->step, 0, sizeof t->step); }   /* (ui.c: step_clear each) */
static void fm1_led_dim_level(uint32_t lo) { (void)lo; }
static int step_on(const step_t *st) { return st->time == ST_NOTE && (st->n || st->hit); }   /* (ui.c) */
static int transport_busy(void) { return song.playing != 0; }
/* the user presets (upreset.c), in RAM */
typedef struct { uint8_t used, engine; char name[13]; int16_t p[P_COUNT]; } host_up_t;
static host_up_t host_up[32];
static uint32_t host_flash_writes;
static int up_used(uint32_t k) { return k < 32u && host_up[k].used; }
static const host_up_t *up_rec(uint32_t k) { return &host_up[k % 32u]; }
static void up_name(uint32_t k, char *b) { str_cpy(b, host_up[k % 32u].name, 13); }
static int up_store(uint32_t k, const char *name)
{
    host_up_t *u = &host_up[k % 32u];
    u->used = 1; u->engine = (uint8_t)trk[0].eng_req;
    str_cpy(u->name, name, 13);
    memcpy(u->p, trk[0].p, sizeof u->p);
    host_flash_writes++;
    return 0;
}
static int up_load(uint32_t k)
{
    uint32_t i;
    if (!up_used(k)) return 1;
    for (i = 0; i < P_COUNT; i++)
        if (!param_kept(i)) trk[0].p[i] = host_up[k].p[i];
    return 0;
}
/* the LEDs, by hand (ui_input.c's helpers): one byte per id */
#define LED_BUF 64u
#define LED_KEY0 14u
static uint8_t host_led_l[64], host_led_d[64], host_led_b[64];
static void led_pos_init(void) {}
static void led_put(uint8_t *nl, uint32_t id, int on) { if (id < LED_BUF && on) nl[id] = 1; }
static int fm1_led_anim_on(void) { return 0; }
#include "../firmware/src/ls_ui.c"
/* the flash of the songs and sketches (0xA0000 .. 0xDBFFF), in RAM; the editor's helpers the Carnet commands use */
#define HOST_FL0 0xA0000u
static uint8_t host_fl[0xDC000u - HOST_FL0];
static uint8_t flash_ok = 1;
static const uint8_t *host_xip(uint32_t off) { return host_fl + (off - HOST_FL0); }
#undef fm1_xip_ptr
#define fm1_xip_ptr(off) host_xip(off)
static int fl_erase4k(uint32_t off, uint32_t *took) { memset(host_fl + (off - HOST_FL0), 0xFF, 0x1000u); *took = 1; return 0; }
static int fl_write(uint32_t off, const void *p, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) host_fl[off - HOST_FL0 + i] &= ((const uint8_t *)p)[i];   /* (NOR: bits only go to 0) */
    return 0;
}
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static uint32_t st_crc32(const void *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, i, k;
    for (i = 0; i < n; i++) { c ^= ((const uint8_t *)p)[i]; for (k = 0; k < 8u; k++) c = c >> 1 ^ (0xEDB88320u & -(c & 1u)); }
    return ~c;
}
static void audio_silence(void) {}
static void fm1_wdt_feed(void) { events_block(CTL); }                       /* (the audio runs while the main loop waits) */
static uint8_t host_ed[8192];
static uint32_t host_edn;
static void ed_b(uint32_t v) { if (host_edn < sizeof host_ed) host_ed[host_edn++] = (uint8_t)(v & 127u); }
static void ed_str(const char *s, uint32_t max) { uint32_t i; for (i = 0; i < max && s[i]; i++) ed_b((uint8_t)s[i]); ed_b(0); }
static uint8_t ed_smp_buf[512];
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j, k;
        na--;
        k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k)) return 0;
        for (j = 0; j < k; j++, na--) out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}


#include "../firmware/src/m1_book.c"
#include "../firmware/src/ls_draw.c"

static const char *dir;
static uint32_t fails, clips, frames_w;
static FILE *wav;
#define FAIL(...) (printf("ls_fm1: FAIL " __VA_ARGS__), printf("\n"), fails++)
static void shot(const char *name)
{
    char p[512];
    FILE *f;
    uint32_t i;
    snprintf(p, sizeof p, "%s/%s.ppm", dir, name);
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
static uint32_t peak;
static void run(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        uint32_t b;
        m1_input();
        m1_service();
        m1_leds();
        m1_draw();
        for (b = 0; b < 15u * 44100u / 1000u / CTL + (fm1_ms % 3u == 0u); b++) {
            int32_t o[2 * CTL];
            uint32_t i;
            mix_block(o, CTL);
            { int64_t lr = 0, ll = 0, rr = 0; uint32_t w = ar_gw, j;       /* (as audio.c: the screen's stereo image) */
              for (j = 0; j < CTL; j++) { int32_t a = o[2 * j] >> 4, c = o[2 * j + 1] >> 4; lr += a * c; ll += a * a; rr += c * c;
                if (j & 1u) { ar_gl[w & (AR_GN - 1u)] = (int16_t)o[2 * j]; ar_gr[w & (AR_GN - 1u)] = (int16_t)o[2 * j + 1]; w++; } }
              ar_gw = w; ar_c_lr += lr - (ar_c_lr >> 5); ar_c_ll += ll - (ar_c_ll >> 5); ar_c_rr += rr - (ar_c_rr >> 5); }
            for (i = 0; i < CTL; i++) {
                int32_t l = o[2 * i] < 0 ? -o[2 * i] : o[2 * i];
                if (l > 32700)
                    clips++;
                if ((uint32_t)l > peak)
                    peak = (uint32_t)l;
                if (wav)
                    wav_put(wav, o[2 * i], o[2 * i + 1]), frames_w++;
            }
        }
        fm1_ms += 15u;
    }
}
static void press(uint32_t b) { host_pressed = 1u << b; run(60); host_released = 1u << b; run(60); }
static void hold(uint32_t b, uint32_t ms) { host_pressed = 1u << b; run(ms); host_released = 1u << b; run(30); }
static void turn(uint32_t e, int32_t s) { host_enc[e] = s; run(120); }
static void kk(uint32_t k, uint32_t hold_ms, uint32_t after)
{
    fm1_in.notes |= 1u << k; host_notes |= 1u << k; run(hold_ms); fm1_in.notes &= ~(1u << k); run(after);
}
static uint32_t voices_live(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < trk_nvoice(&trk[0]); i++)
        n += trk[0].v[i].active;
    return n;
}

int main(int argc, char **argv)
{
    /* keys: 0 = F3 .. 26 = G5; TRANS -12: F2 .. G4; the bow C2 .. C4: keys 0 .. 19 sound (F2 .. C4), 20 .. 26 silent */
    static const uint8_t PH[8] = {9, 12, 16, 19, 16, 12, 9, 5};      /* D3 F3 A3 C4 A3 F3 D3 A#2 */
    char path[512];
    uint32_t k;
    dir = argc > 1 ? argv[1] : "build/host";
    snprintf(path, sizeof path, "%s/ls.wav", dir);
    wav = fopen(path, "wb");
    wav_hdr(wav, 0);
    host_tracks_init();
    fm1_ms = 1000;
    song.master_q12 = 4096;
    m1_init();
    m1_boot();
    run(900); shot("a_opening");
    run(3000); shot("b_opening_name");
    run(1500); shot("c_deck_empty");
    if (trk[0].eng_req != ENGI_SAMPLE || trk[0].p[P_TRANS] != -12) FAIL("the bow: engine %d, trans %d", trk[0].eng_req, trk[0].p[P_TRANS]);
    turn(EN_K1, -3); shot("d_length");
    if (trk[0].p[P_DEC] >= 110) FAIL("K1: the length did not move (%d)", trk[0].p[P_DEC]);
    turn(EN_K1, 3); run(2000);
    fm1_in.notes |= 1u << 26; run(200);                     /* 0.3: G4, the top key, sounds (C2 .. G4) */
    if (!voices_live()) FAIL("G4 (the top key) silent");
    fm1_in.notes &= ~(1u << 26); run(300);
    run(2000);
    for (k = 0; k < 8u; k++) kk(PH[k], 380, 120);
    if (pl_count) FAIL("a sketch before REC");
    run(4500);
    hold(B_REC, 900); run(600); shot("e_kept");
    if (pl_count != 1u) FAIL("REC held: %u sketches", pl_count);
    else printf("kept: %s, %u notes, %u.%02u s\n", pl_item[0].name, pl_item[0].n, pl_item[0].dur10 / 100u, pl_item[0].dur10 % 100u);
    press(B_PLAY); run(1500); shot("f_playing");
    if (!demo_on) FAIL("PLAY: not playing");
    host_pressed = 1u << B_SEQ; run(300); shot("g_list");
    host_released = 1u << B_SEQ; run(200);
    run(3000);
    press(B_PLAY); run(500);
    if (demo_on) FAIL("PLAY again: still playing");
    /* 0.3: the effects */
    press(B_FX); if (!fe.on[FXE_SUS]) FAIL("FX: SUSTAIN not on");
    fm1_in.notes |= 1u << 9; run(5000);
    if (!voices_live()) FAIL("SUSTAIN: the note did not hold 5 s");
    shot("j_sustain");
    fm1_in.notes &= ~(1u << 9); run(3000);
    if (voices_live()) FAIL("SUSTAIN: still sounding after the key");
    { uint8_t a0 = fe.amt[FXE_BRE];                          /* SCL held, SELECT turned: BREATH's amount, not toggled */
      host_pressed = 1u << B_SCL; run(60); host_enc[EN_SELECT] = 3; run(120); host_released = 1u << B_SCL; run(60);
      if (fe.on[FXE_BRE] || fe.amt[FXE_BRE] <= a0) FAIL("SCL held + SELECT: on %u, amount %u (was %u)", fe.on[FXE_BRE], fe.amt[FXE_BRE], a0); }
    press(B_ENV); if (!fe.on[FXE_CTR]) FAIL("ENV: CONTRA not on");
    { uint8_t a0 = fe.amt[FXE_CTR]; turn(EN_SELECT, 3);       /* 0.4: SELECT alone: the effect last touched */
      if (fe.amt[FXE_CTR] <= a0 || !fe.on[FXE_CTR]) FAIL("SELECT after ENV: amount %u (was %u), on %u", fe.amt[FXE_CTR], a0, fe.on[FXE_CTR]); }
    fm1_in.notes |= (1u << 9) | (1u << 12); run(400);
    if (fe.ctr_key != 50u - 12u) FAIL("CONTRA: key %u (38)", fe.ctr_key);
    fm1_in.notes &= ~((1u << 9) | (1u << 12)); run(400);
    if (fe.ctr_key) FAIL("CONTRA: still on %u", fe.ctr_key);
    press(B_ENV);
    fm1_in.notes |= 1u << 4; run(200); press(B_LFO); fm1_in.notes &= ~(1u << 4); run(1500);
    if (fe.drn_key != 45u) FAIL("DRONE: key %u (45)", fe.drn_key);
    shot("k_drone");
    press(B_LFO); run(300); if (fe.drn_key) FAIL("DRONE: not off");
    press(B_FX); run(3000);
    turn(EN_PRESET, 5); if (!fe.pure) FAIL("PRESETS: PURE did not move");
    press(B_GLO); run(300); shot("h_glo");
    press(B_GLO); run(300);
    host_pressed = 1u << B_OCTDN; run(60); host_released = 1u << B_OCTDN; run(300); shot("i_octave");
    if (song.octave != -1) FAIL("OCT-: octave %d", song.octave);
    fclose(wav);
    {
        FILE *fw = fopen(path, "r+b");
        wav_hdr(fw, frames_w);
        fclose(fw);
    }
    printf("ls_fm1: %s, %u s, peak %u, clipped %u, %u failures\n", path, frames_w / 44100u, peak, clips, fails);
    return fails ? 1 : 0;
}
