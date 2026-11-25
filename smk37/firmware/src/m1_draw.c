/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Compagnon 2 on the SMK-37: the screen, in Modular Town V3.2's own design (the tram timetable's showcase at
 * the terminus: the awning's blue frame, cream enamel plates with a red filet, a serif in the town's ink).
 *   the top plate     where we are: the keeper of the town's sound (THE TERMINUS, MADAME ROUX ... NEON TOWN), or
 *                     the keyboard's place (THE STUDIO, the composer's; THE KIOSK for the Mellotron's flutes). For a
 *                     moment, the control touched: its name, its value, a bar
 *   the postcard      240 x 150, the place as the plugin paints it (tools/gen_cards.py: captured from V3.2's own
 *                     interface): by day full size, by night half size, mixed by F1; F2's snow falls on it
 *   the bottom plates TOWN and KEYS, their sounds. The voice the knobs set is lit; a voice the keys do not play
 *                     (PLAY) says SILENT
 * Only what changed goes to the LCD: the plates when their words change, the postcard when the place, the light or
 * the snow does. GLOBE (the settings) is a page of its own, on the same plates. */
#include "mc_cards.h"
#include "mc_serif.h"

#define M1_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
/* a colour between a and b (pct of the way to b), always in colour (not gfx.c's ux_mix: Felucca's GREY style) */
static uint16_t m1_mix(uint16_t a, uint16_t b, int32_t pct)
{
    static const uint8_t SH[3] = {11, 5, 0}, MK[3] = {31, 63, 31};
    uint32_t out = 0, k;
    for (k = 0; k < 3u; k++) {
        int32_t x = (a >> SH[k]) & MK[k], d = (((b >> SH[k]) & MK[k]) - x) * pct;
        out |= (uint32_t)(x + (d + (d >= 0 ? 50 : -50)) / 100) << SH[k];
    }
    return (uint16_t)out;
}
/* the same, by 0..256 (the postcard's pixels: faster) */
static inline uint16_t m1_mix8(uint16_t a, uint16_t b, uint32_t k)
{
    uint32_t r = ((uint32_t)(a >> 11) * (256u - k) + (uint32_t)(b >> 11) * k) >> 8;
    uint32_t g = ((uint32_t)((a >> 5) & 63u) * (256u - k) + (uint32_t)((b >> 5) & 63u) * k) >> 8;
    uint32_t bl = ((uint32_t)(a & 31u) * (256u - k) + (uint32_t)(b & 31u) * k) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}

/* ------------------------------------------------------------ the colours of the showcase */
#define MC_FRAME M1_RGB(0x2c, 0x46, 0x62)        /* the awning's blue */
#define MC_FRAME_N M1_RGB(0x16, 0x20, 0x30)
#define MC_CREAM M1_RGB(0xef, 0xe4, 0xc6)        /* the enamel */
#define MC_CREAM_N M1_RGB(0x9a, 0x8c, 0x70)
#define MC_INK M1_RGB(0x2a, 0x1e, 0x16)
#define MC_RED M1_RGB(0xb2, 0x3a, 0x2c)          /* the filet */
#define MC_AMBER M1_RGB(0xc0, 0x76, 0x22)        /* the headers */
#define MC_GREY M1_RGB(0x8e, 0x86, 0x78)

#define MC_TOP_H 44u                             /* the top band: rows 0 .. 43 */
#define MC_CARD_Y 44u                            /* the postcard: 44 .. 193 */
#define MC_BOT_Y (MC_CARD_Y + MC_CH)             /* the bottom band: 194 .. 239 */
#define MC_BOT_H (240u - MC_BOT_Y)

static struct {
    int16_t night, winter;                       /* the light drawn (-1: never) */
    uint8_t card;                                /* the postcard drawn (0xFF: none) */
    uint8_t full;                                /* everything again */
    uint32_t top_sig, bot_sig;
    uint32_t snow_t, rnd;
} scr = {.night = -1, .winter = -1, .card = 0xFF, .full = 1, .top_sig = 1, .bot_sig = 1, .rnd = 0x2545F491u};
static struct { uint8_t x, y, dx; } snow[40];

static uint32_t scr_rand(uint32_t n)
{
    uint32_t x = scr.rnd;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    scr.rnd = x;
    return n ? x % n : x;
}
static int32_t scr_n(void) { return scr.night < 0 ? 0 : scr.night; }

/* ------------------------------------------------------------ the letters */
static const sf_glyph_t *sf_glyph(const sf_font_t *f, uint32_t c)
{
    if (c >= 'a' && c <= 'z')
        c -= 32u;
    return &f->g[c >= 32u && c < 96u ? c - 32u : 0u];
}
static int32_t sf_width(const sf_font_t *f, const char *s)
{
    int32_t w = 0;
    for (; *s; s++)
        w += sf_glyph(f, (uint8_t)*s)->adv;
    return w;
}
/* s into the canvas (swapped colours) of width bw, rows 0 .. bh-1, its top-left at x, y; ink c (not swapped) */
static void sf_draw(const sf_font_t *f, uint32_t bw, uint32_t bh, int32_t x, int32_t y, const char *s, uint16_t c)
{
    for (; *s; s++) {
        const sf_glyph_t *g = sf_glyph(f, (uint8_t)*s);
        uint32_t gx, gy;
        for (gy = 0; gy < f->h; gy++) {
            int32_t yy = y + (int32_t)gy;
            if (yy < 0 || yy >= (int32_t)bh)
                continue;
            for (gx = 0; gx < g->w; gx++) {
                int32_t xx = x + (int32_t)gx;
                uint32_t i = gy * g->w + gx, a = (f->d[g->off + (i >> 1)] >> ((i & 1u) ? 0 : 4)) & 15u;
                uint16_t *px;
                if (!a || xx < 0 || xx >= (int32_t)bw)
                    continue;
                px = &cv_px[(uint32_t)yy * bw + (uint32_t)xx];
                *px = swap16(a == 15u ? c : m1_mix8(swap16(*px), c, a * 17u));
            }
        }
        x += g->adv;
    }
}
/* the bigger of the two sizes that fits w */
static const sf_font_t *sf_fit(const char *s, int32_t w)
{
    return sf_width(&SF_BIG, s) <= w ? &SF_BIG : &SF_SMALL;
}

/* ------------------------------------------------------------ the plates */
static void mc_rect(uint32_t bw, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t i, j;
    uint16_t s = swap16(c);
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            cv_px[(uint32_t)j * bw + (uint32_t)i] = s;
}
static uint16_t mc_frame(void);
/* an enamel plate at x, y in the canvas: its shadow, the cream (corners cut), the red filet (lit), two rivets */
static void mc_plate(uint32_t bw, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t cream, int lit)
{
    uint16_t sh = m1_mix(mc_frame(), M1_RGB(0, 0, 0), 45);
    mc_rect(bw, x + 2, y + 2, w, h, sh);
    mc_rect(bw, x + 1, y, w - 2, h, cream);
    mc_rect(bw, x, y + 1, w, h - 2, cream);
    if (lit) {
        mc_rect(bw, x + 3, y + 3, w - 6, 1, MC_RED);
        mc_rect(bw, x + 3, y + h - 4, w - 6, 1, MC_RED);
        mc_rect(bw, x + 3, y + 3, 1, h - 6, MC_RED);
        mc_rect(bw, x + w - 4, y + 3, 1, h - 6, MC_RED);
    }
    mc_rect(bw, x + 6, y + h / 2 - 1, 2, 2, m1_mix(cream, MC_INK, 45));
    mc_rect(bw, x + w - 8, y + h / 2 - 1, 2, 2, m1_mix(cream, MC_INK, 45));
}

/* the night on the showcase: the frame darker, the enamel lit by the street */
static uint16_t mc_frame(void) { return m1_mix(MC_FRAME, MC_FRAME_N, scr_n() * 100 / 127); }
static uint16_t mc_cream(void) { return m1_mix(MC_CREAM, MC_CREAM_N, scr_n() * 55 / 127); }

/* ------------------------------------------------------------ what the plates say */
static const char *const M1_F_NAME[4] = {"DAY / NIGHT", "SNOW", "DELAY", "REVERB"};
static const char *const MC_K_NAME[M1_NK] = {"CUTOFF", "RESONANCE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DETUNE"};

/* the postcard of the place shown: the knobs' voice; Neon Town once the town travels there */
static uint32_t mc_card_now(void)
{
    if (mc.voice == MC_V_KEYS)
        return mc.keys == 2u ? MC_CARD_KIOSK : MC_CARD_STUDIO;
    if (mc.place)
        return MC_CARD_NEON;
    return mc.town % MC_NT;                                                  /* (the cards are in the pads' order) */
}
static const char *mc_place_name(uint32_t card)
{
    static const char *const N[MC_NCARD] = {"THE TERMINUS", "MADAME ROUX", "ADA", "MISTER ECHO", "VERA", "DUSTY",
                                            "WATTSON", "NEON TOWN", "THE STUDIO", "THE KIOSK"};
    return N[card % MC_NCARD];
}

/* the top plate: a label (and a value and its bar, 0..127, or -1) */
static struct { char lab[20], val[20]; int16_t bar; } top;
static uint32_t m1_hash(uint32_t h, const char *t)
{
    while (*t)
        h = (h ^ (uint8_t)*t++) * 16777619u;
    return h * 16777619u;
}
static void mc_top_words(void)
{
    int show = m1.touched != M1T_NONE && m1.touched != M1T_PAD && fm1_ms - m1.t_ms < 2500u;
    top.lab[0] = top.val[0] = 0;
    top.bar = -1;
    if (show) {
        switch (m1.touched) {
        case M1T_KNOB:
            str_cpy(top.lab, MC_K_NAME[m1.what % M1_NK], sizeof top.lab);
            top.bar = m1.value;
            break;
        case M1T_FADER:
            str_cpy(top.lab, m1.what == 1u && mc.voice == MC_V_KEYS && VM_TOWN((uint32_t)M1T->p[P_E0]) ? "RISE"
                                                                                              : M1_F_NAME[m1.what & 3u],
                    sizeof top.lab);
            top.bar = m1.value;
            break;
        case M1T_VOL: str_cpy(top.lab, "VOLUME", sizeof top.lab); top.bar = m1.value; break;
        case M1T_HOLD:
            str_cpy(top.lab, m1.hlab ? m1.hlab : "", sizeof top.lab);
            str_cpy(top.val, m1.hval, sizeof top.val);
            break;
        case M1T_WAVE:
            str_cpy(top.lab, m1.what == 1u ? "WAVE 1" : "WAVE 2", sizeof top.lab);
            str_cpy(top.val, m1.value < 22 ? "SAW" : m1.value < 54 ? "MIX" : m1.value < 96 ? "TRIANGLE" : "SQUARE",
                    sizeof top.val);
            break;
        case M1T_BTN:
            if (m1.what == FM1_BTN_OCT_DOWN || m1.what == FM1_BTN_OCT_UP) {
                str_cpy(top.lab, "OCTAVE", sizeof top.lab);
                top.val[0] = '+';
                fmt_int(top.val + (m1.value > 0), m1.value);
            } else if (m1.what == 14u) {
                str_cpy(top.lab, "VINTAGE", sizeof top.lab);
                str_cpy(top.val, m1.value ? "ON" : "OFF", sizeof top.val);
            }
            break;
        default:
            break;
        }
    }
    if (!top.lab[0])                                                        /* where we are */
        str_cpy(top.lab, mc_place_name(mc_card_now()), sizeof top.lab);
    else if (top.bar >= 0 && !top.val[0])
        fmt_int(top.val, top.bar);
}
static void mc_draw_top(void)
{
    uint16_t cream = mc_cream();
    cv_begin(240u, MC_TOP_H, mc_frame());
    mc_plate(240u, 6, 5, 228, 34, cream, 1);
    if (top.val[0]) {                                                       /* a control: its name, its value */
        sf_draw(&SF_SMALL, 240u, MC_TOP_H, 16, top.bar >= 0 ? 9 : 13, top.lab, MC_INK);
        sf_draw(&SF_BIG, 240u, MC_TOP_H, 224 - sf_width(&SF_BIG, top.val), 10, top.val, MC_RED);
        if (top.bar >= 0) {
            mc_rect(240u, 16, 29, 112, 3, m1_mix(cream, MC_INK, 22));
            mc_rect(240u, 16, 29, 112 * top.bar / 127, 3, MC_AMBER);
        }
    } else {                                                                 /* the place, centred */
        const sf_font_t *f = sf_fit(top.lab, 200);
        sf_draw(f, 240u, MC_TOP_H, 120 - sf_width(f, top.lab) / 2, 22 - (int32_t)f->h / 2, top.lab, MC_INK);
    }
    cv_blit(0, 0);
}

/* the two bottom plates: TOWN and KEYS. The knobs' voice lit (its red filet), a voice the keys do not play greyed */
static void mc_draw_bottom(void)
{
    uint32_t v;
    uint16_t cream = mc_cream();
    cv_begin(240u, MC_BOT_H, mc_frame());
    for (v = 0; v < 2u; v++) {
        int32_t x = v ? 122 : 6, y = 3, w = 112, h = 40;
        int lit = mc.voice == (v ? MC_V_KEYS : MC_V_TOWN);
        int plays = mc_play == MC_SPLIT || (mc_play & (v ? 1u : 2u)) != 0;
        const char *name = v ? MC_KEYS_NAME[mc.keys % 8u] : MC_TOWN_SND[mc.town % MC_NT][mc.var];
        uint16_t c = plays ? (lit ? cream : m1_mix(cream, mc_frame(), 18)) : m1_mix(cream, mc_frame(), 45);
        uint16_t sh = m1_mix(mc_frame(), M1_RGB(0, 0, 0), 45);
        char b[18];
        mc_rect(240u, x + 2, y + 2, w, h, sh);
        mc_rect(240u, x + 1, y, w - 2, h, c);
        mc_rect(240u, x, y + 1, w, h - 2, c);
        if (lit) {
            mc_rect(240u, x + 2, y + 2, w - 4, 1, MC_RED);
            mc_rect(240u, x + 2, y + h - 3, w - 4, 1, MC_RED);
            mc_rect(240u, x + 2, y + 2, 1, h - 4, MC_RED);
            mc_rect(240u, x + w - 3, y + 2, 1, h - 4, MC_RED);
        }
        sf_draw(&SF_SMALL, 240u, MC_BOT_H, x + 8, y + 3, v ? "KEYS" : "TOWN", plays ? MC_AMBER : MC_GREY);
        str_cpy(b, name, sizeof b);
        while (sf_width(&SF_SMALL, b) > w - 14 && str_len(b) > 3u)          /* (a long name, shortened) */
            b[str_len(b) - 1u] = 0;
        sf_draw(&SF_SMALL, 240u, MC_BOT_H, x + 8, y + 19, b, plays ? MC_INK : MC_GREY);
    }
    cv_blit(0, MC_BOT_Y);
}

/* ------------------------------------------------------------ the postcard */
/* an LZSS stream (tools/gen_cards.py), decoded byte by byte into a 4096-byte ring */
typedef struct { const uint8_t *p; uint32_t flags, nflag, mlen, mdist, pos; uint8_t *ring; } mc_lz_t;
static uint8_t lz_ring_d[4096] __attribute__((section(".pool")));
static uint8_t lz_ring_n[4096] __attribute__((section(".pool")));
static void mc_lz_start(mc_lz_t *z, const uint8_t *p, uint8_t *ring)
{
    z->p = p; z->flags = 0; z->nflag = 0; z->mlen = 0; z->mdist = 0; z->pos = 0; z->ring = ring;
}
static inline uint32_t mc_lz_byte(mc_lz_t *z)
{
    uint32_t b;
    if (!z->mlen) {
        if (!z->nflag) {
            z->flags = *z->p++;
            z->nflag = 8;
        }
        z->nflag--;
        if (z->flags & 1u) {
            uint32_t b0 = z->p[0], b1 = z->p[1];
            z->p += 2;
            z->flags >>= 1;
            z->mdist = b0 << 4 | b1 >> 4;
            z->mlen = (b1 & 15u) + 3u;
        } else {
            z->flags >>= 1;
            b = *z->p++;
            z->ring[z->pos++ & 4095u] = (uint8_t)b;
            return b;
        }
    }
    z->mlen--;
    b = z->ring[(z->pos - z->mdist) & 4095u];
    z->ring[z->pos++ & 4095u] = (uint8_t)b;
    return b;
}

/* the snow: flakes on a half-size grid; 1 = a flake at (x, y) */
static uint8_t flake_row[MC_CH / 2u][MC_CW / 16u];                           /* a bit per 2 x 2 cell */
static void mc_snow_grid(uint32_t nfl)
{
    uint32_t k;
    for (k = 0; k < sizeof flake_row; k++)
        ((uint8_t *)flake_row)[k] = 0;
    for (k = 0; k < nfl; k++)
        if (snow[k].y < MC_CH / 2u && snow[k].x < MC_CW / 2u)
            flake_row[snow[k].y][snow[k].x >> 3] |= (uint8_t)(1u << (snow[k].x & 7u));
}

static void mc_draw_card(uint32_t card)
{
    mc_lz_t dz, nz;
    uint16_t dp[64], np[64], night_row[MC_NW];
    uint32_t y, x, k = (uint32_t)scr_n() * 256u / 127u, nfl = scr.winter > 16 ? (uint32_t)scr.winter * 40u / 127u : 0u;
    uint16_t flake = swap16(M1_RGB(0xf6, 0xf2, 0xea));
    card %= MC_NCARD;
    for (x = 0; x < 64u; x++) {
        dp[x] = MC_DPAL[card][x];
        np[x] = MC_NPAL[card][x];
    }
    mc_snow_grid(nfl);
    mc_lz_start(&dz, MC_DDATA + MC_DOFF[card], lz_ring_d);
    mc_lz_start(&nz, MC_NDATA + MC_NOFF[card], lz_ring_n);
    for (y = 0; y < MC_CH;) {
        uint32_t nr = MC_CH - y, r;
        if (nr > 75u)
            nr = 75u;                                                        /* two strips (the canvas: 124 rows) */
        lcd_sync();
        for (r = 0; r < nr; r++, y++) {
            uint16_t *row = cv_px + r * 240u;
            const uint8_t *fr = flake_row[y >> 1];
            if (!(y & 1u))                                                   /* the night: half size, each row twice */
                for (x = 0; x < MC_NW; x++)
                    night_row[x] = np[mc_lz_byte(&nz) & 63u];
            for (x = 0; x < MC_CW; x++) {
                uint16_t d = dp[mc_lz_byte(&dz) & 63u];
                uint16_t c = k == 0u ? d : k >= 256u ? night_row[x >> 1] : m1_mix8(d, night_row[x >> 1], k);
                row[x] = (fr[x >> 4] >> ((x >> 1) & 7u)) & 1u ? flake : swap16(c);
            }
        }
        lcd_blit(0, MC_CARD_Y + y - nr, 240u, nr, cv_px);
    }
}

/* ------------------------------------------------------------ the opening */
#define SPL_MS 2800u
static struct { uint8_t on; uint32_t t0; } spl = {1, 0};
static void mc_draw_splash(void)
{
    cv_begin(240u, MC_TOP_H, MC_FRAME);
    mc_plate(240u, 6, 5, 228, 34, MC_CREAM, 1);
    sf_draw(sf_fit("MODULAR COMPAGNON", 210), 240u, MC_TOP_H,
            120 - sf_width(sf_fit("MODULAR COMPAGNON", 210), "MODULAR COMPAGNON") / 2,
            22 - (int32_t)sf_fit("MODULAR COMPAGNON", 210)->h / 2, "MODULAR COMPAGNON", MC_INK);
    cv_blit(0, 0);
    mc_draw_card(MC_CARD_DEPART);
    cv_begin(240u, MC_BOT_H, MC_FRAME);
    sf_draw(&SF_SMALL, 240u, MC_BOT_H, 120 - sf_width(&SF_SMALL, "BUNGLE SOCIETY") / 2, 15, "BUNGLE SOCIETY", MC_CREAM);
    cv_blit(0, MC_BOT_Y);
}

/* ------------------------------------------------------------ GLOBE, the settings: a plate a row */
static void m1_globe_row(uint32_t r)
{
    char b[12], k[3] = {'K', (char)('2' + r), 0};
    int hot = m1.grow == r && fm1_ms - m1.grow_ms < 3000u;
    const char *v = g1_vname(r, b);
    uint16_t cream = hot ? MC_CREAM : m1_mix(MC_CREAM, MC_FRAME, 25);
    cv_begin(240u, 32u, MC_FRAME);
    mc_plate(240u, 6, 1, 228, 28, cream, hot);
    sf_draw(&SF_SMALL, 240u, 32u, 16, 6, k, MC_AMBER);
    sf_draw(&SF_SMALL, 240u, 32u, 44, 6, G1_NAME[r], MC_INK);
    sf_draw(&SF_SMALL, 240u, 32u, 222 - sf_width(&SF_SMALL, v), 6, v, hot ? MC_RED : MC_INK);
    cv_blit(0, 48u + r * 32u);
}
/* the whole page once, then a row when it changes (and the highlight going out after 3 s) */
static void m1_globe_draw(int all)
{
    static uint32_t sig[G1_N];
    uint32_t r;
    if (all) {
        lcd_fill(0, 0, 240, 240, MC_FRAME);
        cv_begin(240u, MC_TOP_H, MC_FRAME);
        mc_plate(240u, 6, 5, 228, 34, MC_CREAM, 1);
        sf_draw(&SF_BIG, 240u, MC_TOP_H, 120 - sf_width(&SF_BIG, "SETTINGS") / 2, 22 - (int32_t)SF_BIG.h / 2, "SETTINGS",
                MC_INK);
        cv_blit(0, 0);
        cv_begin(240u, 30u, MC_FRAME);
        sf_draw(&SF_SMALL, 240u, 30u, 120 - sf_width(&SF_SMALL, "GLOBE : BACK") / 2, 7, "GLOBE : BACK", MC_CREAM);
        cv_blit(0, 210);
    }
    for (r = 0; r < G1_N; r++) {
        uint32_t v = g1_get(r) | (m1.grow == r && fm1_ms - m1.grow_ms < 3000u ? 0x100u : 0u);
        if (all || v != sig[r]) {
            sig[r] = v;
            m1_globe_row(r);
        }
    }
}

/* ------------------------------------------------------------ the screen asleep (GLOBE SLEEP) */
static struct { uint8_t st; uint32_t t; } m1_scr;                            /* 0 on, 1 off, 2 waking (SLPOUT sent) */

/* 1 = draw; 0 = the panel is dark or waking; 2 = awake again (draw everything, then DISPON) */
static int m1_screen_awake(void)
{
    uint32_t lim = SCR_MIN[scr_get() % 5u] * 60000u, now = fm1_ms;
    int active = (int32_t)(now - m1.active_ms) < 1000;
    switch (m1_scr.st) {
    case 0:
        if (!lim || (int32_t)(now - m1.active_ms) < (int32_t)lim)
            return 1;
        lcd_power(0);                                                        /* DISPOFF, SLPIN */
        m1_scr.st = 1;
        m1_scr.t = now;
        return 0;
    case 1:
        if (active && now - m1_scr.t >= 120u) {
            lcd_power(1);                                                    /* SLPOUT */
            m1_scr.st = 2;
            m1_scr.t = now;
        }
        return 0;
    default:
        if (now - m1_scr.t < 120u)
            return 0;
        m1_scr.st = 0;
        scr.full = 1;                                                        /* everything again, then the panel on */
        return 2;
    }
}

static void m1_draw(void)
{
    static uint8_t globe;
    uint32_t now = fm1_ms, card, sig;
    int32_t night = m1.fader[0] == 0xFF ? 0 : 127 - m1.fader[0], winter = m1.fader[1] == 0xFF ? 0 : m1.fader[1];
    int wake = m1_screen_awake(), redraw_card = 0;
    if (!wake)
        return;
    if (m1.globe) {
        m1_globe_draw(!globe || wake == 2);
        globe = 1;
        if (wake == 2)
            lcd_power(2);
        return;
    }
    if (globe) {                                                             /* back from GLOBE: everything again */
        globe = 0;
        scr.full = 1;
    }
    if (spl.on) {                                                            /* the opening */
        if (!spl.t0) {
            spl.t0 = now ? now : 1u;
            mc_draw_splash();
        }
        if (now - spl.t0 < SPL_MS && !(m1.notes_ms && (int32_t)(m1.notes_ms - spl.t0) > 0) &&
            !(m1.touched != M1T_NONE && (int32_t)(m1.t_ms - spl.t0) > 300)) {
            if (wake == 2)
                lcd_power(2);
            return;
        }
        spl.on = 0;
        scr.full = 1;
    }
    if (night - scr.night > 3 || scr.night - night > 3 || (night != scr.night && (night == 0 || night == 127))) {
        scr.night = (int16_t)night;
        redraw_card = 1;
    }
    if (winter - scr.winter > 3 || scr.winter - winter > 3 || (winter != scr.winter && (winter == 0 || winter == 127))) {
        scr.winter = (int16_t)winter;
        redraw_card = 1;
    }
    if (scr.winter > 16 && now - scr.snow_t >= 140u) {                       /* the snow falls */
        uint32_t k;
        scr.snow_t = now;
        for (k = 0; k < 40u; k++) {
            if (!snow[k].y || snow[k].y >= MC_CH / 2u - 1u) {
                snow[k].x = (uint8_t)scr_rand(MC_CW / 2u);
                snow[k].y = (uint8_t)(snow[k].y ? 1u : 1u + scr_rand(70u));
                snow[k].dx = (uint8_t)scr_rand(3);
            }
            snow[k].y++;
            if (!scr_rand(3))
                snow[k].x = (uint8_t)((snow[k].x + MC_CW / 2u + snow[k].dx - 1u) % (MC_CW / 2u));
        }
        redraw_card = 1;
    }
    card = mc_card_now();
    if (card != scr.card || scr.full || redraw_card) {
        scr.card = (uint8_t)card;
        mc_draw_card(card);
    }
    mc_top_words();
    sig = m1_hash(m1_hash(2166136261u, top.lab), top.val) ^ (uint32_t)(top.bar + 1) * 2654435761u ^
          (uint32_t)scr.night * 40503u;
    if (sig != scr.top_sig || scr.full) {
        scr.top_sig = sig;
        mc_draw_top();
    }
    sig = (uint32_t)mc.voice | (uint32_t)mc.town << 2 | (uint32_t)mc.var << 6 | (uint32_t)mc.keys << 10 |
          (uint32_t)mc_play << 14 | (uint32_t)scr.night << 18;
    if (sig != scr.bot_sig || scr.full) {
        scr.bot_sig = sig;
        mc_draw_bottom();
    }
    scr.full = 0;
    if (wake == 2)
        lcd_power(2);                                                        /* DISPON: the picture is there */
    m1.dirty = 0;
}
