/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Landscape on the FM-1: the screen, in the four greens of the Game Boy (Modular One's, Modular Koi's).
 *   the image    (0.3, the deck gone) the sound's stereo image over the whole screen (ls_gonio), the correlation a
 *                thin line at the foot; at the top, small, the effects on and the sketch playing
 *   the words    big in the middle, Modular One's pixel letters (m1_pixfont.h), for a moment: the control touched or
 *                what the tape says
 *   the list     SEQ held: the sketches in place of the image (K1 chooses, REC asks to erase, REC again erases)
 *   the opening  BUNGLE SOCIETY comes down from the top to the middle (Modular One's), then MODULAR LANDSCAPE; a key or
 *                a touch skips it
 * GLO is a page of its own, dark green. Drawn when something changes; ~12 pictures a second while there is sound. */
#include "m1_pixfont.h"

#define M1_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
static const uint16_t GB[4] = {M1_RGB(0x0f, 0x38, 0x0f), M1_RGB(0x30, 0x62, 0x30), M1_RGB(0x8b, 0xac, 0x0f),
                               M1_RGB(0x9b, 0xbc, 0x0f)};
#define LS_BG GB[3]                                                          /* the lightest: the screen */
#define LS_INK GB[0]                                                         /* the darkest: the deck, the words */
#define LS_MID GB[1]                                                         /* the tape wound, the quieter words */
#define LS_PALE GB[2]                                                        /* the hubs, the rollers, the bar */

/* ------------------------------------------------------------ the pixel letters */
static int32_t pf_width_s(const char *s, int32_t sc)
{
    int32_t w = 0;
    for (; *s; s++)
        w += sc * PF_ADV[((uint8_t)*s - 32u) < 95u ? (uint8_t)*s - 32u : 0u];
    return w;
}
static int32_t pf_width(const char *s) { return pf_width_s(s, 2); }
static void pf_draw_s(uint16_t *buf, uint32_t bw, uint32_t bh, int32_t x, int32_t y, const char *s, uint16_t c,
                      int32_t sc)
{
    for (; *s; s++) {
        uint32_t g = (uint8_t)*s - 32u;
        int32_t r, k;
        if (g >= 95u)
            g = 0;
        for (r = 0; r < 5 * sc; r++) {
            int32_t yy = y + r;
            uint32_t bits = PF_ROWS[g][r / sc];
            if (yy < 0 || yy >= (int32_t)bh || !bits)
                continue;
            for (k = 0; k < 8 * sc; k++)
                if (bits & (0x80u >> (k / sc))) {
                    int32_t xx = x + k;
                    if (xx >= 0 && xx < (int32_t)bw)
                        buf[(uint32_t)yy * bw + (uint32_t)xx] = c;
                }
        }
        x += sc * PF_ADV[g];
    }
}
static void pf_draw(uint16_t *buf, uint32_t bw, uint32_t bh, int32_t x, int32_t y, const char *s, uint16_t c)
{
    pf_draw_s(buf, bw, bh, x, y, s, c, 2);
}

/* ------------------------------------------------------------ the deck (in screen rows; the band's offset: cv_oy) */
#define DK_RY 106                                                            /* the reels' centres */
#define DK_RL 64
#define DK_RR 176
#define DK_R 44                                                              /* their flanges */
#define DK_GY 166                                                            /* the rollers */
#define DK_GL 20
#define DK_GR 220
static int32_t ls_y0;                                                        /* the band being drawn */
static uint32_t ls_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return r;
}
/* ------------------------------------------------------------ 0.3: the stereo image (as the SMK-37's Modular Landscape
 * 54): the output's last 256 pairs (audio.c), mono up the middle, wide across, its scale following the level; under
 * it the correlation, a thin line, -1 .. +1 (fx.c ar_c_*) */
#define GN_CX 120
#define GN_CY 112
#define GN_R 96
static void ls_gonio(void)
{
    uint32_t w = ar_gw, i;
    int64_t lr = ar_c_lr, ll = ar_c_ll, rr = ar_c_rr;
    static int32_t pk = 4000;
    int32_t k, m = 0;
    cv_rect(GN_CX - 1, GN_CY - 1, 3, 3, LS_PALE);
    for (i = 0; i < AR_GN; i++) {
        int32_t l = ar_gl[i], r = ar_gr[i], a = l > r ? l - r : r - l, b = l + r;
        b = b < 0 ? -b : b;
        a = a > b ? a : b;
        m = a > m ? a : m;
    }
    pk = m > pk ? m : pk - (pk >> 4);
    pk = pk < 1500 ? 1500 : pk;
    k = pk * 10 / 9;
    {                                                                        /* (0.4: on the green, a line 3 px thick
                                                                              * from point to point, the newest darkest) */
        int32_t px = 0, py = 0, b;
        for (i = 0; i < AR_GN; i++) {
            int32_t l = ar_gl[(w - 1u - i) & (AR_GN - 1u)], r = ar_gr[(w - 1u - i) & (AR_GN - 1u)];
            int32_t x = GN_CX + (l - r) * GN_R / k, y = GN_CY - (l + r) * GN_R / k;
            x = clamp(x, GN_CX - GN_R, GN_CX + GN_R);
            y = clamp(y, GN_CY - GN_R, GN_CY + GN_R);
            if (i)
                for (b = 0; b < (i < 128u ? 3 : 2); b++)                      /* (a square brush: thick every way) */
                    cv_line_t(px + b, py, x + b, y, i < 128u ? LS_INK : LS_MID, i < 128u ? 3 : 2);
            px = x;
            py = y;
        }
    }
    cv_rect(40, 229, 160, 3, LS_PALE);                                       /* the correlation: 3 px, its mark bold */
    cv_rect(119, 225, 2, 11, LS_MID);
    if (ll > 4096 && rr > 4096) {
        int32_t c, d, cx;
        while (ll >= 32768 || rr >= 32768) {
            ll >>= 1;
            rr >>= 1;
            lr >>= 1;
        }
        d = (int32_t)ls_isqrt((uint32_t)(ll * rr));
        c = d ? (int32_t)lr * 80 / d : 0;
        cx = 120 + c;
        cx = cx < 40 ? 40 : cx > 199 ? 199 : cx;
        cv_rect(cx - 2, 224, 5, 13, LS_INK);
    }
}

/* ------------------------------------------------------------ the words */
static void ls_time(char *b, uint32_t d10)                                   /* 10 ms -> M:SS */
{
    uint32_t sec = d10 / 100u, k = 0;
    if (sec >= 600u)
        b[k++] = (char)('0' + sec / 600u % 10u);
    b[k++] = (char)('0' + sec / 60u % 10u);
    b[k++] = ':';
    b[k++] = (char)('0' + sec % 60u / 10u);
    b[k++] = (char)('0' + sec % 10u);
    b[k] = 0;
}
/* letters at screen row y in the band; centred when x < 0 */
static void ls_txt(int32_t x, int32_t y, const char *t, uint16_t c, int32_t sc)
{
    if (x < 0)
        x = 120 - pf_width_s(t, sc) / 2;
    pf_draw_s(cv_px, 240u, 120u, x, y - ls_y0, t, swap16(c), sc);
}
static int32_t ls_fit(const char *t, int32_t w, int32_t hi)                 /* the biggest scale <= hi that fits */
{
    while (hi > 1 && pf_width_s(t, hi) > w)
        hi--;
    return hi;
}
static struct { char lab[20], val[20]; uint8_t show; } cap;
static void m1_caption(void)
{
    cap.lab[0] = cap.val[0] = 0;
    cap.show = m1.touched != M1T_NONE && fm1_ms - m1.t_ms < 1800u;
    if (!cap.show)
        return;
    switch (m1.touched) {
    case M1T_KNOB: str_cpy(cap.lab, KC_NAME[m1.what % KC_N], sizeof cap.lab); ls_val(m1.what % KC_N, cap.val); break;
    case M1T_VOL:
        str_cpy(cap.lab, "VOLUME", sizeof cap.lab);
        fmt_int(cap.val, m1.value);
        str_cpy(cap.val + str_len(cap.val), " %", 3);
        break;
    case M1T_OCT:
        str_cpy(cap.lab, "KEYS", sizeof cap.lab);
        str_cpy(cap.val, m1.value < 0 ? "F1 - G3" : "F2 - G4", sizeof cap.val);
        break;
    case M1T_HOLD:
        str_cpy(cap.lab, m1.hlab ? m1.hlab : "", sizeof cap.lab);
        str_cpy(cap.val, m1.hval, sizeof cap.val);
        break;
    default: cap.show = 0; break;
    }
}
/* the sketch list (SEQ held): 7 rows from y 64 */
static void ls_list(void)
{
    int32_t first = clamp((int32_t)pl_cursor - 3, 0, pl_count > 7u ? (int32_t)pl_count - 7 : 0), row;
    char b[28], c[12];
    ls_txt(-1, 20, "SKETCHBOOK", LS_MID, 2);
    if (!pl_count) {
        ls_txt(-1, 120, "NO SKETCH YET", LS_INK, 2);
        ls_txt(-1, 144, "REC HELD: KEEP", LS_MID, 1);
        return;
    }
    for (row = 0; row < 7 && first + row < (int32_t)pl_count; row++) {
        uint32_t k = (uint32_t)(first + row);
        int32_t yy = 64 + row * 22;
        int sel = k == pl_cursor, playing = demo_on && k == pl_idx;
        if (sel || pl_del == k + 1u)
            cv_rect(6, yy - 5, 228, 20, pl_del == k + 1u ? LS_INK : LS_PALE);
        if (pl_del == k + 1u) {                                              /* "erase?" */
            str_cpy(b, "ERASE ", 8);
            str_cpy(b + 6, pl_item[k].name, sizeof b - 6u);
            ls_txt(12, yy, b, LS_BG, ls_fit(b, 170, 2));
            ls_txt(228 - pf_width_s("REC", 1), yy + 3, "REC", LS_BG, 1);
            continue;
        }
        ls_txt(12, yy, playing ? ">" : " ", LS_INK, 2);
        ls_txt(30, yy, pl_item[k].name, sel || playing ? LS_INK : LS_MID, ls_fit(pl_item[k].name, 140, 2));
        ls_time(c, pl_item[k].dur10);
        ls_txt(228 - pf_width_s(c, 2), yy, c, LS_MID, 2);
    }
    ls_txt(-1, 224, "K1 CHOOSE  REC ERASE  LET GO: PLAY", LS_MID, 1);
}
/* the band y0 .. y0 + 120 of the screen */
static void ls_band(uint32_t y0, uint32_t p1000, uint32_t ang)
{
    char b[28], c[12];
    ls_y0 = (int32_t)y0;
    cv_begin(240u, 120u, LS_BG);
    cv_oy = -(int32_t)y0;
    if (m1.list)
        ls_list();
    else {
        uint32_t e, x = 4;
        ls_gonio();                                                          /* 0.3: the image, the whole screen */
        for (e = 0; e < FXE_N; e++)                                          /* the effects on: their words, quiet */
            if (fe.on[e]) {
                ls_txt((int32_t)x, 4, FXE_NAME[e], LS_MID, 1);
                x += (uint32_t)pf_width_s(FXE_NAME[e], 1) + 8u;
            }
        if (demo_on && pl_count) {                                           /* the tape: the sketch, its time */
            const pl_item_t *it = &pl_item[pl_idx % PL_MAX];
            str_cpy(b, it->name, sizeof b);
            str_cpy(b + str_len(b), "  ", 3);
            ls_time(c, demo_pos / 441u);
            str_cpy(b + str_len(b), c, 8);
            ls_txt(4, 14, b, LS_MID, 1);
        }
        (void)p1000;
        (void)ang;
    }
    if (cap.show) {                                                          /* what was just touched: a moment, big in
                                                                              * the middle over the image (0.3) */
        int32_t sn = ls_fit(cap.lab, 220, 3), sv = ls_fit(cap.val, 220, 6);
        cv_rect(0, 84, 240, 64, LS_BG);
        ls_txt(-1, 90, cap.lab, LS_MID, sn);
        ls_txt(-1, 92 + 6 * sn, cap.val, LS_INK, sv);
    }
    cv_oy = 0;
    cv_blit(0, y0);
}

/* ------------------------------------------------------------ the opening, Modular One's: BUNGLE SOCIETY comes down
 * from the top to the middle on the lightest green, stops; then MODULAR LANDSCAPE under it. A key or a touch skips it */
#define SPL_MS 4200u
#define SPL_STOP 2000
#define SPL_Y 104
static struct { uint8_t on, started, named; uint32_t t0; int16_t y; } spl = {1, 0, 0, 0, -100};
static void splash_draw(uint32_t now)
{
    int32_t t = (int32_t)(now - spl.t0), y = t >= SPL_STOP ? SPL_Y : -14 + (SPL_Y + 14) * t / SPL_STOP;
    const char *w = "BUNGLE SOCIETY";
    if (spl.y == -100)
        lcd_fill(0, 0, 240, 240, LS_BG);
    if (y != spl.y) {                                                        /* one strip: where it was and where it is */
        int32_t lo = spl.y > -14 && spl.y < y ? spl.y : y, hi = (spl.y > y ? spl.y : y) + 12;
        if (lo < 0)
            lo = 0;
        spl.y = (int16_t)y;
        if (hi > lo) {
            cv_begin(240u, (uint32_t)(hi - lo), LS_BG);
            pf_draw(cv_px, 240u, (uint32_t)(hi - lo), 120 - pf_width(w) / 2, y + 1 - lo, w, swap16(LS_INK));
            cv_blit(0, (uint32_t)lo);
        }
    }
    if (t >= SPL_STOP + 300 && !spl.named) {
        const char *n = "MODULAR LANDSCAPE";
        spl.named = 1;
        cv_begin(240u, 12u, LS_BG);
        pf_draw_s(cv_px, 240u, 12u, 120 - pf_width_s(n, 1) / 2, 2, n, swap16(LS_MID), 1);
        cv_blit(0, SPL_Y + 22);
    }
}

/* ------------------------------------------------------------ GLO: a dark green page */
#define G1_BG GB[0]
#define G1_HOT GB[1]
#define G1_INK GB[3]
#define G1_DIM GB[2]
static void m1_globe_row(uint32_t r)
{
    char b[12];
    int hot = g1_hot(r);
    const char *v = g1_vname(r, b);
    cv_begin(240u, 24u, hot ? G1_HOT : G1_BG);
    pf_draw(cv_px, 240u, 24u, 10, 7, hot ? ">" : " ", swap16(G1_INK));
    pf_draw(cv_px, 240u, 24u, 42, 7, G1_NAME[r], swap16(hot ? G1_INK : G1_DIM));
    pf_draw(cv_px, 240u, 24u, 230 - pf_width(v), 7, v, swap16(G1_INK));
    cv_blit(0, 38u + r * 24u);
}
static void m1_globe_draw(int all)
{
    static uint32_t sig[G1_N];
    uint32_t r;
    if (all) {
        lcd_fill(0, 0, 240, 240, G1_BG);
        cv_begin(240u, 36u, G1_BG);
        pf_draw(cv_px, 240u, 36u, 10, 14, "SETTINGS", swap16(G1_INK));
        cv_blit(0, 2);
        cv_begin(240u, 30u, G1_BG);
        pf_draw(cv_px, 240u, 30u, 10, 10, G1_FOOT, swap16(G1_DIM));
        cv_blit(0, 208);
    }
    for (r = 0; r < G1_N; r++) {
        uint32_t v = g1_get(r) | (g1_hot(r) ? 0x100u : 0u);
        if (all || v != sig[r]) {
            sig[r] = v;
            m1_globe_row(r);
        }
    }
}

/* ------------------------------------------------------------ the screen asleep (GLO SLEEP) */
static struct { uint8_t st; uint32_t t; } m1_scr;
static uint8_t ls_full = 1;
static int m1_screen_awake(void)
{
    uint32_t lim = SCR_MIN[scr_get() % 5u] * 60000u, now = fm1_ms;
    int active = (int32_t)(now - m1.active_ms) < 1000;
    switch (m1_scr.st) {
    case 0:
        if (!lim || (int32_t)(now - m1.active_ms) < (int32_t)lim)
            return 1;
        lcd_power(0);
        m1_scr.st = 1;
        m1_scr.t = now;
        return 0;
    case 1:
        if (active && now - m1_scr.t >= 120u) {
            lcd_power(1);
            m1_scr.st = 2;
            m1_scr.t = now;
        }
        return 0;
    default:
        if (now - m1_scr.t < 120u)
            return 0;
        m1_scr.st = 0;
        ls_full = 1;
        return 2;
    }
}

static void m1_draw(void)
{
    static uint8_t page, cap_on, was_on;
    static uint32_t frame_ms, ang;
    uint32_t now = fm1_ms, p1000 = 0;
    int wake = m1_screen_awake();
    if (!wake)
        return;
    if (m1.globe) {
        m1_globe_draw(page != 1u || wake == 2);
        page = 1;
        if (wake == 2)
            lcd_power(2);
        return;
    }
    if (page) {
        page = 0;
        ls_full = 1;
    }
    if (!spl.started) {
        spl.started = 1;
        spl.t0 = now;
    }
    if (spl.on && (now - spl.t0 >= SPL_MS || (m1.notes_ms && (int32_t)(m1.notes_ms - spl.t0) > 0) ||
                   (m1.touched != M1T_NONE && (int32_t)(m1.t_ms - spl.t0) > 300) || demo_on)) {
        spl.on = 0;
        ls_full = 1;
    }
    if (spl.on) {
        splash_draw(now);
        if (wake == 2)
            lcd_power(2);
        return;
    }
    m1_caption();
    if (cap.show != cap_on)
        ls_full = 1;                                                         /* (and one picture to take it away) */
    cap_on = cap.show;
    if (demo_on != was_on)
        ls_full = 1;
    was_on = demo_on;
    if (m1.dirty)
        ls_full = 1;
    {                                                                        /* 0.3: the image moves with the sound, ~12
                                                                              * pictures a second; one more when it stops */
        static uint8_t sound;
        if (now - frame_ms >= 80u && (ar_c_ll > 4096 || ar_c_rr > 4096 || sound))
            ls_full = 1;
        sound = ar_c_ll > 4096 || ar_c_rr > 4096;
    }
    if (!ls_full || now - frame_ms < 40u)
        return;
    frame_ms = now;
    ls_full = 0;
    m1.dirty = 0;
    if (demo_on && pl_count) {
        const pl_item_t *it = &pl_item[pl_idx % PL_MAX];
        uint32_t el = demo_pos / 441u, dur = it->dur10;
        p1000 = dur ? (el >= dur ? 1000u : el * 1000u / dur) : 0u;
        ang = demo_pos * 1715u;                                              /* (a turn every ~2.5 s) */
    }
    ls_band(0, p1000, ang);
    ls_band(120, p1000, ang);
    if (wake == 2)
        lcd_power(2);
}
