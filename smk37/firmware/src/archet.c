/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* MODULAR LANDSCAPE (the ARCHET build, MC_ARCHET=1): the SMK-37 plays Alexis's folk guitar played with a bow, from his
 * album takes (tools/archet/: bow strokes D2 .. E4, 1.7 .. 2.2 s each, a second stroke on some keys played in turn:
 * eng_sample.c), and keeps sketches on its 16 pads (ar_land.c: what sounds; ar_tape.c: the
 * tape; ar_book.c: the sketches in flash).
 *   C2 .. G4      the bow, on the whole keyboard (TRN -12; G#4 .. C5 silent: E4 is its highest stroke)
 *   pitch wheel   bend (a wide dead band at its centre: main.c)
 *   pads 1 .. 8   8 sketches (the bottom row): an empty pad touched records from the first
 *                 note after it (everything heard), touched again keeps it; a full pad touched plays it again and
 *                 again, again stops it; held 1 s: ERASE PAD n? SEQ REC erases it (anything else, or 4 s: kept)
 *   pads 9 .. 16  (37, 44) the options, each on / off, its amount on the knob above (K1 the volume, above TENUE):
 *                 SUSTAIN (a note held as long as its key), BREATH / K2, CONTRA / K3 (the lowest key doubled an
 *                 octave under), DRONE / K4 (the lowest key held, or the last played, held), WIDTH / K5, DISTANCE /
 *                 K6, MUTE / K7, ECHO / K8 (48, 50: in English; 37 .. 49: TENUE, RESPIRATION, FIGE, PEDALE, LARGEUR,
 *                 DISTANCE, SOURDINE, ECHO). On screen
 *                 above the sketch pads; their pads lit while on
 *   mod wheel     (40) PURE: down, the stroke as recorded; up, the bow's noise taken away, the note left (eng_sample.c's
 *                 comb)
 *   F1 .. F4      ATTACK (20 .. 100), RELEASE (40 .. 112), SPACE (the reverb's send and room), LENGTH (635 ms .. 3.9 s,
 *                 DEC 89 .. 114; held by TENUE)
 *   SEQ REC       held: the last phrase kept, on the first free pad (held again at once: one phrase further back)
 *   SEQ PLAY      the sketches one after the other, or stop; OCT+ / OCT- while it plays: the next / its start
 *                 (twice: the one before); SEQ held: the list (K2 chooses, SEQ REC erases)
 *   LEFT          the bow on / off: off, the keyboard only drives the computer (the screen says MIDI)
 *   K1            the volume (master.c); K2 .. K8 (44) the amounts of the pads below (and CC 20 .. 26); the faders
 *                 CC 28 .. 31
 * Every note heard goes out to MIDI as well (the keys', the tape's).
 * The screen: Modular One's deck, always there (its reels turn while a sketch plays); at the top, big, 1.5 s, what
 * was just moved; under the deck the 16 pads as they lie: empty, full, playing, recording. Every LED stays dark (lit,
 * the SMK's pad and button LEDs put a whine in the audio, the red ones most). Everything else does nothing. */

#include "m1_pixfont.h"                                                     /* (tools/gen_pixfont.py: the FM-1's letters) */
/* s at sc x into the canvas (bw x bh), its top-left at x, y (may be outside); c as the LCD takes it (swapped) */
static int32_t ar_pf_w(const char *s, int32_t sc)
{
    int32_t w = 0;
    for (; *s; s++)
        w += sc * PF_ADV[((uint8_t)*s - 32u) < 95u ? (uint8_t)*s - 32u : 0u];
    return w;
}
/* three small letters the FM-1's font draws as capitals: b (a flat), m (minor), o (diminished) */
static const uint8_t AR_PF_LOW[3][5] = {{0x40, 0x40, 0x70, 0x48, 0x70}, {0x00, 0xD0, 0xA8, 0xA8, 0xA8},
                                       {0x00, 0x30, 0x48, 0x30, 0x00}};
static void ar_pf(uint32_t bw, uint32_t bh, int32_t x, int32_t y, const char *s, uint16_t c, int32_t sc)
{
    for (; *s; s++) {
        uint32_t g = (uint8_t)*s - 32u;
        int32_t r, k;
        const uint8_t *low = *s == 'b' ? AR_PF_LOW[0] : *s == 'm' ? AR_PF_LOW[1] : *s == 'o' ? AR_PF_LOW[2] : 0;
        if (g >= 95u)
            g = 0;
        for (r = 0; r < 5 * sc; r++) {
            int32_t yy = y + r;
            uint32_t bits = low ? low[r / sc] : PF_ROWS[g][r / sc];
            if (yy < 0 || yy >= (int32_t)bh || !bits)
                continue;
            for (k = 0; k < 8 * sc; k++)
                if (bits & (0x80u >> (k / sc))) {
                    int32_t xx = x + k;
                    if (xx >= 0 && xx < (int32_t)bw)
                        cv_px[(uint32_t)yy * bw + (uint32_t)xx] = c;
                }
        }
        x += sc * PF_ADV[g];
    }
}

/* ---- the notes heard (the audio side writes, the main loop reads: the opening goes at the first) */
#define AR_EVQ 32u
static volatile uint16_t ar_ev[AR_EVQ];                                      /* note | vel << 7, vel 0: let go */
static volatile uint8_t ar_ev_w;
static uint8_t ar_ev_r;
static void ar_ev_put(uint32_t note, uint32_t vel)
{
    uint8_t w = ar_ev_w;
    ar_ev[w % AR_EVQ] = (uint16_t)((note & 127u) | (vel & 127u) << 7);
    ar_ev_w = (uint8_t)(w + 1u);
}

/* ---- the controls */
#ifndef SMK_BTN_LEFT
#define SMK_BTN_LEFT 2u                                                      /* (host tests: smk_input.h's) */
#define SMK_BTN_FADER_BANK 1u
#endif
#define AR_B_SEQ 15u
#define AR_B_PLAY 16u
#define AR_B_REC 17u
static uint8_t ar_local = 1;                                                 /* LEFT: the bow sounds (else MIDI only) */
static uint8_t ar_knob[8] = {64, 64, 64, 64, 64, 64, 64, 64};               /* K2 .. K8 as sent (CC 20 .. 26) */
static uint8_t ar_redraw;                                                    /* the screen: draw a frame now */
/* the screen: the name and value of what was just moved, 1.5 s */
static struct { char name[20]; char val[20]; uint32_t until; } ar_lab;
static char *ar_utoa(char *d, uint32_t v)                                    /* v in decimal at d -> its end */
{
    char t[10];
    uint32_t n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v && n < 10u);
    while (n)
        *d++ = t[--n];
    *d = 0;
    return d;
}
static void ar_show(const char *name, uint32_t v, uint32_t kind)            /* kind: 0 a number, 1 % (v 0..127), 2 a
                                                                              * time, 3 % (v 0..100) */
{
    char *d = ar_lab.val;
    if (kind == 2) {                                                         /* (v: an ATK / DEC / REL value) */
        uint32_t ms = TIME_MS_X10[v & 127u] / 10u;
        if (ms < 1000u) {
            d = ar_utoa(d, ms);
            d[0] = ' ', d[1] = 'M', d[2] = 'S', d[3] = 0;
        } else {
            d = ar_utoa(d, ms / 1000u);
            *d++ = ',';
            d = ar_utoa(d, ms % 1000u / 100u);
            d[0] = ' ', d[1] = 'S', d[2] = 0;
        }
    } else {
        d = ar_utoa(d, kind == 1 ? (v * 100u + 63u) / 127u : v);
        if (kind == 1 || kind == 3)
            d[0] = ' ', d[1] = '%', d[2] = 0;
    }
    str_cpy(ar_lab.name, name, sizeof ar_lab.name);
    ar_lab.until = fm1_ms + 1500u;
    ar_redraw = 1;
}
static void ar_say(const char *name, const char *val)                        /* the tape's words (m1_say) */
{
    str_cpy(ar_lab.name, name, sizeof ar_lab.name);
    str_cpy(ar_lab.val, val, sizeof ar_lab.val);
    ar_lab.until = fm1_ms + 2000u;
    ar_redraw = 1;
}
#define m1_say ar_say
#if FELUCCA_FLASH
#include "ar_book.c"                                                        /* the sketches in flash, on the pads */
#else
/* (host: no flash) the pads' sketches in RAM */
#define AR_NPAD 16u
static uint8_t pad_item[AR_NPAD];
static volatile uint8_t ar_pad_keep_req, ar_pad_del_req;
static volatile uint32_t ar_pad_i0;
static uint8_t ar_host_buf[AR_NPAD][4096];
static int m1_quiet(void) { return !demo_on && !demo_req; }
static void m1_boot(void)
{
    uint32_t p;
    pl_count = 0;
    for (p = 0; p < AR_NPAD; p++)
        pad_item[p] = 0xFF;
}
static void ar_host_list(void)
{
    static pl_item_t keep[AR_NPAD];
    static uint8_t has[AR_NPAD];
    uint32_t p, n = 0;
    for (p = 0; p < AR_NPAD; p++)
        if (pad_item[p] != 0xFFu)
            keep[p] = pl_item[pad_item[p]], has[p] = 1;
        else
            has[p] = 0;
    for (p = 0; p < AR_NPAD; p++) {
        pad_item[p] = 0xFF;
        if (has[p]) {
            pl_item[n] = keep[p];
            pad_item[p] = (uint8_t)n++;
        }
    }
    pl_count = n;
}
static void m1_service(void)
{
    if (m1_keep_req) {
        m1_keep_req = 0;
        ar_say("SKETCHBOOK", "NO FLASH");
    }
    if (ar_pad_keep_req) {
        uint32_t p = ar_pad_keep_req - 1u, notes, dur10, len;
        ar_pad_keep_req = 0;
        len = sk_extract_from(ar_pad_i0, ar_host_buf[p], sizeof ar_host_buf[p], &notes, &dur10);
        if (len) {
            pl_item[PL_MAX - 1u].data = ar_host_buf[p];
            pl_item[PL_MAX - 1u].n = notes;
            pl_item[PL_MAX - 1u].dur10 = dur10;
            str_cpy(pl_item[PL_MAX - 1u].name, "SKETCH ", PL_NAME);
            ar_utoa(pl_item[PL_MAX - 1u].name + 7, p + 1u);
            pad_item[p] = PL_MAX - 1u;
            ar_host_list();
            ar_say("PAD", "KEPT");
        }
    }
    if (ar_pad_del_req) {
        uint32_t p = ar_pad_del_req - 1u;
        ar_pad_del_req = 0;
        demo_req = 2;
        pad_item[p] = 0xFF;
        ar_host_list();
        ar_say("PAD", "ERASED");
    }
    m1_del_req = 0;
    (void)m1_quiet;
}
#endif
static void ar_midi(uint32_t st, uint32_t d1, uint32_t d2)                   /* a channel message out (USB) */
{
    if (!usb.config)
        return;
    fm1_irq_off();                                                           /* (midi_out_event is the audio ISR's) */
    midi_out_event((st >> 4) | st << 8 | (d1 & 127u) << 16 | (d2 & 127u) << 24);
    fm1_irq_on();
}

/* ---- the bow, on its parts (ar_land.c), and the faders */
enum { FP_ATK, FP_REL, FP_SPACE, FP_LEN };
static const char *const AR_FADER_N[4] = {"ATTACK", "RELEASE", "SPACE", "LENGTH"};
static uint8_t ar_fv[4] = {0, 71, 51, 84};                                  /* (ATK 20, REL 80, the room, LENGTH 1.9 s) */
static uint8_t ar_dec = 106;                                                 /* F4: the length (DEC) */
/* 37: the options, on the top row of pads (9 .. 12), off at power-on: TENUE (the note held as long as its key, a
 * crossfaded loop), ENSEMBLE (fx.c), FIGE (FREEZE: fx.c), PEDALE (a held drone on part 2: the lowest key held, or the
 * last one played) */
static uint8_t ar_ten, ar_ped_note;                                          /* ar_ped_note: the drone's key, 0 none */
/* 44, 48: the whole top row, each pad with the knob above it (K1 the volume, above TENUE): RESPIRATION / K2 how deep
 * (the held notes swell and ebb, ~3 s, together), CONTRA / K3 its volume (50: was FIGE), PEDALE / K4 its volume, LARGEUR / K5 the bow
 * from mono to wide, DISTANCE / K6 how far (less of it dry, more in the room, duller), SOURDINE / K7 how muted (the
 * tone closed, as a mute on the bridge), ECHO / K8 how much (a dotted 1/8 of the tempo, 120 or a DAW's clock, its
 * repeats growing with it). (44 .. 47: ENSEMBLE, VIBRATO, ARPEGE and K6 the BPM; 48: gone, Alexis's choice) */
static uint8_t ar_kv[8] = {64, 64, 64, 64, 64, 64, 64, 64};                  /* K1 .. K8 (index 0 .. 7) */
static uint8_t ar_resp, ar_larg, ar_dist, ar_srd, ar_echo;
static void ar_bow_apply(void)
{
    uint32_t k, sp = ar_fv[FP_SPACE], dist = ar_dist ? ar_kv[5] : 0u, cut;
    song.g[G_DTIME] = 11;                                                    /* ECHO: a dotted 1/8 */
    song.g[G_DFDBK] = (int16_t)(30u + ar_kv[7] * 50u / 127u);
    song.g[G_DMIX] = 110;
    for (k = 0; k < 4u; k++) {
        track_t *t = &trk[k];
        int bass = k == 2u, ped = k == 1u;
        t->p[P_ATK] = (int16_t)(20u + ar_fv[FP_ATK] * 80u / 127u);           /* the bass: the bow's attack */
        t->p[P_REL] = (int16_t)(40u + ar_fv[FP_REL] * 72u / 127u);
        t->p[P_DEC] = ar_dec;                                                /* the bass fades as the bow does */
        t->p[P_SUS] = ar_ten ? 118 : 0;                                      /* TENUE: held (the loop: E3) */
        t->p[P_E3] = ar_ten ? 1 : 0;
        t->p[P_REV] = (int16_t)((sp * (bass ? 30u : 110u) + dist * 90u) / 127u);   /* (DISTANCE: more in the room) */
        t->p[P_CHOR] = 0;
        t->p[P_E6] = 0;
        cut = ar_srd ? 127u - ar_kv[6] * 100u / 127u : 127u;                 /* SOURDINE: the tone closed (CUT) */
        cut -= cut * dist * 30u / (127u * 127u);                             /* DISTANCE: duller, up to a quarter */
        t->p[P_E4] = (int16_t)cut;
        t->p[P_DLY] = (int16_t)(ar_echo && !bass ? 20u + ar_kv[7] * 107u / 127u : 0u);   /* ECHO: the send */
        t->p[P_SPRD] = (int16_t)(bass ? 0u : ar_larg ? ar_kv[4] : 40u);      /* LARGEUR: mono .. wide (off: 40) */
        if (!ped) {                                                          /* RESPIRATION: a slow LFO on the level, */
            t->p[P_LRATE] = 36;                                              /* free (all the notes together), ~0.33 Hz */
            t->p[P_LTRIG] = 1;                                               /* (49, Alexis: 48's did nothing heard:
                                                                              * ~6 s, 2.5 dB at K2's middle; now ~3 s,
                                                                              * up to the full LFO depth) */
            t->p[P_LD_AMP] = (int16_t)(ar_resp ? ar_kv[1] : 0u);
        }
        if (bass) {                                                          /* CONTRA (50): K3 its volume */
            t->p[P_LEVEL] = (int16_t)((30u + ar_kv[2] * 97u / 127u) * (127u - dist * 50u / 127u) / 127u);
        } else if (ped) {                                                    /* PEDALE: held, in and out slowly, breathing */
            t->p[P_ATK] = 96;
            t->p[P_REL] = 110;
            t->p[P_DEC] = 100;
            t->p[P_SUS] = 127;
            t->p[P_E3] = 1;
            t->p[P_LEVEL] = (int16_t)((30u + ar_kv[3] * 97u / 127u) * (127u - dist * 50u / 127u) / 127u);   /* K4 */
        } else
            t->p[P_LEVEL] = (int16_t)(108u - dist * 16u / 127u);             /* DISTANCE: less of it dry (~-7 dB) */
    }
    song.g[G_RSIZE] = (int16_t)(60u + sp * 67u / 127u);                      /* the room */
}
/* the faders, as they are moved (and as they stand once read after power-on) */
static void ar_faders(void)
{
    static uint8_t last[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        uint32_t v = fm1_in.fader[i], first = last[i] == 0xFF;
        if (v > 127u || v == last[i])
            continue;
        last[i] = (uint8_t)v;
        if (!first)
            mc_cc(28u + i, v);                                               /* CC 28 .. 31 (not the first reading) */
        ar_fv[i] = (uint8_t)v;
        if (i == FP_LEN)
            ar_dec = (uint8_t)(89u + v * 25u / 127u);                        /* LENGTH: 635 ms .. 3.9 s (DEC 89 .. 114) */
        ar_bow_apply();
        if (!first) {
            if (i == FP_LEN && TIME_MS_X10[ar_dec & 127u] >= 18500u)
                ar_say("LENGTH", "FULL");                                    /* (53: past a stroke's own length, ~1.85 s,
                                                                              * the whole stroke: said so, not a time) */
            else if (i == FP_ATK || i == FP_REL || i == FP_LEN)
                ar_show(AR_FADER_N[i], (uint32_t)trk[0].p[i == FP_ATK ? P_ATK : i == FP_REL ? P_REL : P_DEC], 2);
            else
                ar_show(AR_FADER_N[i], v, 1);
        }
    }
}
static void ar_wheel(int force)                /* the mod wheel: 0 the stroke as recorded .. 127 the bow's noise gone (40) */
{
    static uint8_t last = 0xFF;
    uint32_t w = smk_modw, first = last == 0xFF;
    if (w <= 3u)                                                             /* (its ends: the ends) */
        w = 0;
    else if (w >= 124u)
        w = 127;
    if (!force && !first && (w == last || (w + 3u > last && w < last + 3u && w && w != 127u)))
        return;                                                              /* (it drifts a little: 3 steps or more) */
    last = (uint8_t)w;
    ar_breath = (int32_t)w;
    if (!first && !force && fm1_ms > 3000u)
        ar_show("PURE", w, 1);
}

/* the pitch wheel, on screen while it moves: its offset from the centre / that side's travel (main.c smk_wheels:
 * the first 45 % does nothing), so where it comes back to can be read */
/* (the pitch wheel's value is not shown, 54: Alexis, it drifts and kept showing) */
static void ar_init(void)
{
    uint32_t k, i;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            if (param_kept(i))
                trk[k].p[i] = TP[i].def;
        track_defaults_steps(&trk[k]);
    }
    song.octave = 0;
    song.sel = 0;
    song.g[G_RTYPE] = 0;                                                     /* ROOM (made stereo in fx.c: MC_ARCHET) */
    smk_pad_free = 1;                                                        /* pads are ours: not notes */
    g1_boot();                                                               /* (the saved MIDI channel, tuning) */
    for (k = 0; k < 4u; k++) {                    /* parts 1, 4 the bow; 2, 3 the bass (ar_land.c) */
        track_t *t = &trk[k];
        int bass = k == 2u;                                                  /* (37: part 2 the DRONE; 50: part 3 CONTRA) */
        fm1_irq_off();
        t->eng_req = ENGI_SAMPLE;                                            /* (all four the bow) */
        fm1_irq_on();
        apply_preset_to(t, 0u);                                              /* ARCHET */
        t->p[P_AMODE] = 0;                                                   /* no arp */
        t->p[P_VOICE] = 0;                                                   /* poly */
        t->p[P_TRANS] = k == 0u || k == 1u ? -12 : 0;                        /* the keys (and the PEDALE): C2 .. C5 */
        t->p[P_LD_FLT] = 0;
        t->p[P_CHOR] = 0;
        t->p[P_DLY] = 0;
        t->p[P_E3] = 0;                                                      /* (LOOP: the zones have none) */
        if (k == 1u) {                                                       /* PEDALE: breathing (a slow LFO on its level) */
            t->p[P_LRATE] = 18;
            t->p[P_LD_AMP] = 30;
        }
        t->p[P_SPRD] = bass ? 0 : 40;        /* stereo: the notes placed by pitch, C2 left .. G4 right, a guitar wide
                                              * (+-20 of 64: voice.c MC_ARCHET), not swapped left / right; the bass
                                              * in the middle */
    }
    m1_boot();                                                               /* the sketches kept (ar_book.c) */
    mc_play = 3u;                                                            /* the keyboard plays and its notes go out */
    mc.auto_play = 0;
    ar_bow_apply();
    ar_wheel(1);
}

/* ---- the tape's controls (Modular One's gestures, on the SMK-37's SEQ, SEQ PLAY, SEQ REC and OCT) */
static struct { uint8_t list, hold_used, rec_down, oct_play; uint32_t rec_ms, octdn_ms; } ar_tp;
static uint8_t ar_pad_ask;                                                   /* a pad to erase + 1, SEQ REC confirms */
static uint32_t ar_pad_ask_ms;
static void ar_tape_input(uint32_t pressed, uint32_t released)
{
    if ((pressed >> AR_B_SEQ) & 1u) {                                        /* SEQ held: the list */
        ar_tp.list = 1;
        ar_tp.hold_used = 0;
        pl_cursor = (uint8_t)(demo_on ? pl_idx : pl_start);
        pl_del = 0;
        ar_redraw = 1;
    }
    if ((released >> AR_B_SEQ) & 1u && ar_tp.list) {                         /* let go: the one chosen plays */
        if (ar_tp.hold_used == 1u && pl_count) {
            pl_loop = 0;
            if (demo_on) {
                pl_target = pl_cursor;
                demo_req = 6;
            } else {
                pl_start = pl_cursor;
                demo_req = 1;
            }
        }
        ar_tp.list = 0;
        pl_del = 0;
        ar_redraw = 1;
    }
    if ((pressed >> AR_B_PLAY) & 1u) {
        if (!pl_count && !demo_on)
            ar_say("TAPE", "EMPTY");
        else {
            pl_loop = 0;
            demo_req = demo_on ? 2u : 1u;
        }
        ar_redraw = 1;
    }
    if ((pressed >> AR_B_REC) & 1u && ar_pad_ask) {                         /* a pad held: SEQ REC erases it */
        ar_pad_del_req = ar_pad_ask;
        ar_pad_ask = 0;
        pressed &= ~(1u << AR_B_REC);
        ar_tp.rec_down = 3;                                                  /* (its release: nothing more) */
    }
    if ((pressed >> AR_B_REC) & 1u) {
        if (ar_tp.list) {                                                    /* in the list: erase this one? */
            if (pl_cursor < pl_count && pl_del == pl_cursor + 1u) {
                m1_del_req = (uint8_t)(pl_cursor + 1u);                      /* again: erased (ar_book.c) */
                pl_del = 0;
            } else if (pl_cursor < pl_count) {
                pl_del = (uint8_t)(pl_cursor + 1u);
            }
            ar_tp.hold_used = 2;                                             /* (letting SEQ go plays nothing) */
            ar_redraw = 1;
        } else {
            ar_tp.rec_down = 1;
            ar_tp.rec_ms = fm1_ms;
        }
    }
    if ((released >> AR_B_REC) & 1u)
        ar_tp.rec_down = 0;
    if (ar_tp.rec_down == 1u && fm1_ms - ar_tp.rec_ms >= 600u) {             /* SEQ REC held: keep the last phrase */
        ar_tp.rec_down = 2;
        m1_keep_req = 1;
    }
    /* OCT+ / OCT- while it plays: the next one / its start (twice within 1 s: the one before); when let go */
    if ((pressed >> FM1_BTN_OCT_UP) & 1u && demo_on)
        ar_tp.oct_play |= 2u;
    if ((pressed >> FM1_BTN_OCT_DOWN) & 1u && demo_on)
        ar_tp.oct_play |= 1u;
    if ((released >> FM1_BTN_OCT_UP) & 1u && (ar_tp.oct_play & 2u)) {
        ar_tp.oct_play &= (uint8_t)~2u;
        if (demo_on)
            demo_req = 3;
    }
    if ((released >> FM1_BTN_OCT_DOWN) & 1u && (ar_tp.oct_play & 1u)) {
        ar_tp.oct_play &= (uint8_t)~1u;
        if (demo_on) {
            uint32_t r = ar_tp.octdn_ms && fm1_ms - ar_tp.octdn_ms < 1000u ? 5u : 4u;
            demo_req = (uint8_t)r;
            ar_tp.octdn_ms = r == 4u ? (fm1_ms | 1u) : 0u;
        }
    }
}

/* ---- the pads: 16 sketches (smk_padev: pad 8..15 the bottom row, sketches 1 .. 8; 0..7 the top, 9 .. 16) */
static uint8_t ar_rec_pad = 0xFF;                                            /* the pad recording (or waiting) */
static uint32_t ar_rec_i0;                                                   /* the ring when it was touched */
static struct { uint8_t down, done; uint32_t ms; } ar_pd[AR_NPAD];
static void ar_pad_lab(uint32_t q, char *lab)                                /* "PAD 1" .. "PAD 16" */
{
    str_cpy(lab, "PAD ", 5);
    ar_utoa(lab + 4, q + 1u);
}
static void ar_pad_stop_rec(void)
{
    if (ar_rec_pad == 0xFFu)
        return;
    ar_pad_i0 = ar_rec_i0;
    ar_pad_keep_req = (uint8_t)(ar_rec_pad + 1u);                            /* (ar_book.c, in m1_service) */
    ar_rec_pad = 0xFF;
}
static void ar_pad_tap(uint32_t q)                                           /* a pad touched */
{
    char lab[8];
    if (ar_pad_ask) {                                                        /* (the erase asked for: not this) */
        ar_pad_ask = 0;
        ar_lab.until = fm1_ms;
        ar_redraw = 1;
        return;
    }
    ar_pad_lab(q, lab);
    if (ar_rec_pad == q) {                                                   /* recording: keep it */
        ar_pad_stop_rec();
    } else if (pad_item[q] == 0xFFu) {                                       /* empty: record from the next note */
        ar_pad_stop_rec();
        ar_rec_pad = (uint8_t)q;
        ar_rec_i0 = sk_w;
        ar_say(lab, "PLAY TO RECORD");
    } else if (demo_on && pl_loop && pl_idx == pad_item[q]) {                /* playing: stop */
        demo_req = 2;
    } else {                                                                 /* full: it plays, again and again */
        pl_loop = 1;
        if (demo_on) {
            pl_target = pad_item[q];
            demo_req = 6;
        } else {
            pl_start = pad_item[q];
            demo_req = 1;
        }
    }
    ar_redraw = 1;
}
/* 37: the top row, the options (q 8 .. 11: pads 9 .. 12; 13 .. 16 nothing yet) */
static void ar_ped_set(uint32_t key)                                         /* the PEDALE's drone: key, or 0 off */
{
    fm1_irq_off();
    if (ar_ped_note)
        trk_note_off(&trk[1], ar_ped_note);
    ar_ped_note = (uint8_t)key;
    if (key)
        trk_note_on(&trk[1], key, 90);
    fm1_irq_on();
}
static int ar_opt_on(uint32_t q)                                             /* an option pad lit? */
{
    return q == 8u ? ar_ten : q == 9u ? ar_resp : q == 10u ? ar_contra : q == 11u ? ar_ped_note != 0u :
           q == 12u ? ar_larg : q == 13u ? ar_dist : q == 14u ? ar_srd : q == 15u ? ar_echo : 0;
}
static void ar_option(uint32_t q)
{
    static const char *const NAME[8] = {"SUSTAIN", "BREATH", "CONTRA", "DRONE", "WIDTH", "DISTANCE", "MUTE", "ECHO"};
    uint32_t on = 0;
    if (q == 8u)
        on = ar_ten ^= 1u;
    else if (q == 9u)
        on = ar_resp ^= 1u;
    else if (q == 10u) {
        fm1_irq_off();
        on = ar_contra ^= 1u;
        ar_contra_update();
        fm1_irq_on();
    }
    else if (q == 11u) {
        uint32_t n, key = 0;
        if (!ar_ped_note) {                                                  /* the lowest key held, else the last */
            for (n = AR_LO; n <= AR_HI && !key; n++)
                if (ar_on[TK_KEYS][n])
                    key = n;
            if (!key)
                key = ar_last_note;
        }
        ar_ped_set(key);
        on = key != 0u;
    } else if (q == 12u)
        on = ar_larg ^= 1u;
    else if (q == 13u)
        on = ar_dist ^= 1u;
    else if (q == 14u)
        on = ar_srd ^= 1u;
    else if (q == 15u)
        on = ar_echo ^= 1u;
    else
        return;
    ar_bow_apply();
    ar_say(NAME[q - 8u], on ? "ON" : "OFF");
}
static void ar_pads(void)
{
    int32_t e;
    uint32_t q;
    while ((e = smk_padev_take()) >= 0) {
        uint32_t pad = (uint32_t)e >> 8 & 15u, vel = (uint32_t)e & 127u;
        q = pad >= 8u ? pad - 8u : pad + 8u;
        if (q >= 8u) {                                                       /* (37: the top row, the options) */
            if (vel)
                ar_option(q);
            continue;
        }
        if (vel) {
            ar_pd[q].down = 1;
            ar_pd[q].done = 0;
            ar_pd[q].ms = fm1_ms;
        } else if (ar_pd[q].down) {
            ar_pd[q].down = 0;
            if (!ar_pd[q].done)
                ar_pad_tap(q);
        }
    }
    for (q = 0; q < 8u; q++)                                                 /* held 1 s: erase it? (SEQ REC) */
        if (ar_pd[q].down && !ar_pd[q].done && fm1_ms - ar_pd[q].ms >= 1000u && pad_item[q] != 0xFFu &&
            ar_rec_pad != q) {
            char lab[8];
            ar_pd[q].done = 1;
            ar_pad_ask = (uint8_t)(q + 1u);
            ar_pad_ask_ms = fm1_ms;
            ar_pad_lab(q, lab);
            str_cpy(ar_lab.name, "ERASE ", 7);
            str_cpy(ar_lab.name + 6, lab, 8);
            str_cpy(ar_lab.val, "SEQ REC: YES", sizeof ar_lab.val);
            ar_lab.until = fm1_ms + 4000u;
            ar_redraw = 1;
        }
    if (ar_pad_ask && fm1_ms - ar_pad_ask_ms >= 4000u) {                     /* (no answer in 4 s: kept) */
        ar_pad_ask = 0;
        ar_redraw = 1;
    }
}

static void ar_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), k;
    master_poll();
    midi_take();
    ar_pads();
    for (k = 1; k < 8u; k++) {                                               /* K2 .. K8: CC 20 .. 26 */
        int32_t s = fm1_enc_take(k);
        if (!s)
            continue;
        if (ar_tp.list) {                                                    /* SEQ held: K2 chooses a sketch */
            if (k == 1u && pl_count) {
                pl_cursor = (uint8_t)clamp((int32_t)pl_cursor + (s > 0 ? 1 : -1), 0, (int32_t)pl_count - 1);
                pl_del = 0;
                if (ar_tp.hold_used != 2u)
                    ar_tp.hold_used = 1;
                ar_redraw = 1;
            }
            continue;
        }
        if (s > 1 || s < -1)
            s *= 2;
        s = clamp((int32_t)ar_knob[k] + s, 0, 127);
        if (s != ar_knob[k]) {                                               /* 44: each the amount of the pad below */
            static const char *const KN[8] = {"", "BREATH", "CONTRA", "DRONE", "WIDTH", "DISTANCE", "MUTE", "ECHO"};
            ar_knob[k] = (uint8_t)s;
            ar_kv[k] = (uint8_t)s;
            mc_cc(19u + k, (uint32_t)s);                                     /* (and CC 20 .. 26, as before) */
            ar_bow_apply();
            ar_show(KN[k], (uint32_t)s, 1);
        }
    }
    if ((pressed >> SMK_BTN_LEFT) & 1u) {                                    /* the bow on / off */
        ar_local ^= 1u;
        mc_play = ar_local ? 3u : 2u;
        ar_redraw = 1;
    }
    {   /* K1, the volume (master.c): shown when it moves by 3 % or more (the pot is never quite still) */
        static int32_t shown = -1;
        uint32_t k10 = (uint32_t)(master_knob / 16);
        int32_t pc = k10 >= 1000u ? 100 : (int32_t)(k10 / 10u);                 /* (the pot stops short of 1023) */
        if (shown < 0)
            shown = pc;
        else if (pc > shown + 2 || pc < shown - 2) {
            shown = pc;
            ar_show("VOLUME", (uint32_t)pc, 3);
        }
    }
    ar_tape_input(pressed, released);
    m1_service();                                                            /* (ar_book.c: keep, erase, the pads) */
    midi_bend_target[1] = midi_bend_target[0];                               /* (the bass bends with the bow) */
    ar_faders();
    ar_wheel(0);
}

/* ---- the LEDs: dark (lit, the pads' and the buttons' LEDs put a whine in the audio, the red ones most). 52: lit,
 * dim and in one family, blue (no red, Alexis: the blue makes no noise; and no Christmas tree): an option on, a
 * quiet blue; a sketch kept, a faint blue; the one playing, a brighter cyan; the one recording, that cyan
 * breathing in and out */
static void ar_leds(void)
{
    uint32_t p, br = fm1_ms % 1200u;
    br = br < 600u ? br : 1200u - br;                                        /* 0 .. 600 .. 0 */
    for (p = 0; p < 16u; p++) {
        uint32_t q = p >= 8u ? p - 8u : p + 8u;
        if (q >= 8u)
            padled_set(p, 0, ar_opt_on(q) ? 10 : 0, ar_opt_on(q) ? 40 : 0);
        else if (ar_rec_pad == q)
            padled_set(p, 0, (uint8_t)(6u + br * 30u / 600u), (uint8_t)(10u + br * 40u / 600u));
        else if (demo_on && pad_item[q] != 0xFFu && pad_item[q] == pl_idx)
            padled_set(p, 0, 30, 45);
        else if (pad_item[q] != 0xFFu)
            padled_set(p, 0, 3, 10);
        else
            padled_set(p, 0, 0, 0);
    }
    for (p = 0; p < 18u; p++)
        fm1_led_key(p, 0);
}

/* ---- the screen: Modular One's deck (Modular Koi's kd_deck, a little lower), always there, in warm ink on the dark;
 * its reels turn while a sketch plays. At the top: the key and the chord (big), the scene; or, big, 1.5 .. 2 s, what
 * was just moved. Under the deck: the 8 sketch pads, their states, what the tape does. SEQ held: the list. Drawn when
 * something changes; ~8 pictures a second while a sketch plays, ~4 while a pad records */
#define AR_BG_R 16u
#define AR_BG_G 12u
#define AR_BG_B 8u
#define AR_BG M1_RGB(AR_BG_R, AR_BG_G, AR_BG_B)
#define AR_INK M1_RGB(0xF0, 0xDC, 0xBE)                                     /* cream */
#define AR_SOFT M1_RGB(0xA8, 0x94, 0x7C)                                    /* the quieter words */
#define AR_DIM M1_RGB(0x5C, 0x42, 0x2E)                                     /* the tape wound on a reel */
#define AR_PANEL M1_RGB(0x2C, 0x22, 0x1A)                                   /* the hubs, the rollers */
#define AR_ACC M1_RGB(0xE8, 0x8C, 0x3C)                                     /* orange: playing, the time gone */
#define AR_RED M1_RGB(0xC8, 0x3C, 0x2C)                                     /* recording */
#define DK_RY 104                                                           /* the reels' centres */
#define DK_RL 64
#define DK_RR 176
#define DK_R 44                                                             /* their flanges */
#define DK_GY 160                                                           /* the rollers */
#define DK_GL 20
#define DK_GR 220
static int32_t ar_band_y0;                                                  /* the band being drawn (screen rows) */
static uint32_t ar_isqrt(uint32_t v)
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
static void ar_time(char *b, uint32_t d10)                                  /* 10 ms -> M:SS */
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
/* pixel letters at screen row y (the band's offset applied), sc x; centred when x < 0 */
static void ar_txt(int32_t x, int32_t y, const char *t, uint16_t c, int32_t sc)
{
    if (x < 0)
        x = 120 - ar_pf_w(t, sc) / 2;
    ar_pf(240u, 120u, x, y - ar_band_y0, t, swap16(c), sc);
}
/* 51: the screen without the tape deck (Alexis): the stereo image (a goniometer: mono up the middle, wide across) and
 * its correlation (-1 .. +1), the four faders and PURE, the knobs over their pads */
#define GN_CX 120
#define GN_CY 112
#define GN_R 104
static void ar_gonio(void)
{
    uint32_t w = ar_gw, i;
    int64_t lr = ar_c_lr, ll = ar_c_ll, rr = ar_c_rr;
    int32_t k, cx;
    cv_pset(GN_CX, GN_CY, AR_DIM);                                           /* (53: no frame, no letters: the centre) */
    {                                                                        /* its scale follows the level: the loudest
                                                                              * of the last pairs at 0.9 of the frame,
                                                                              * falling back slowly (a quiet bow seen) */
        static int32_t pk = 4000;
        int32_t m = 0;
        for (i = 0; i < AR_GN; i++) {
            int32_t l = ar_gl[i], r = ar_gr[i], a = (l > r ? l - r : r - l), b = l + r;
            a = a > (b < 0 ? -b : b) ? a : (b < 0 ? -b : b);
            m = a > m ? a : m;
        }
        pk = m > pk ? m : pk - (pk >> 4);
        pk = pk < 1500 ? 1500 : pk;
        k = pk * 10 / 9;
    }
    for (i = 0; i < AR_GN; i++) {                                            /* the last 256 pairs (~12 ms) */
        int32_t l = ar_gl[(w - 1u - i) & (AR_GN - 1u)], r = ar_gr[(w - 1u - i) & (AR_GN - 1u)];
        int32_t x = GN_CX + (l - r) * GN_R / k, y = GN_CY - (l + r) * GN_R / k;
        if (x > GN_CX - GN_R && x < GN_CX + GN_R && y > GN_CY - GN_R && y < GN_CY + GN_R)
            cv_pset(x, y, i < 64u ? AR_INK : AR_ACC);
    }
    cv_rect(60, 230, 120, 1, AR_PANEL);                                      /* the correlation, -1 .. +1: a thin line, */
    cv_rect(120, 228, 1, 5, AR_PANEL);                                       /* its middle, and a mark (53: quieter) */
    if (ll > 4096 && rr > 4096) {                                            /* (in integers: no 64-bit floats here) */
        int32_t c, d;
        while (ll >= 32768 || rr >= 32768) {
            ll >>= 1;
            rr >>= 1;
            lr >>= 1;
        }
        d = (int32_t)ar_isqrt((uint32_t)(ll * rr));
        c = d ? (int32_t)lr * 60 / d : 0;                                    /* -60 .. 60 */
        cx = 120 + c;
        cx = cx < 60 ? 60 : cx > 179 ? 179 : cx;
        cv_rect(cx - 1, 228, 3, 5, c < 0 ? AR_SOFT : AR_DIM);
    }
}
static int32_t ar_fit(const char *t, int32_t w, int32_t hi)                 /* the biggest scale <= hi that fits */
{
    while (hi > 1 && ar_pf_w(t, hi) > w)
        hi--;
    return hi;
}
/* the sketch list (SEQ held): 7 rows from y 64 */
static void ar_list(void)
{
    int32_t first = clamp((int32_t)pl_cursor - 3, 0, pl_count > 7u ? (int32_t)pl_count - 7 : 0), row;
    char b[28], c[12];
    ar_txt(-1, 20, "SKETCHBOOK", AR_SOFT, 2);
    if (!pl_count) {
        ar_txt(-1, 120, "NO SKETCH YET", AR_SOFT, 2);
        ar_txt(-1, 144, "SEQ REC HELD: KEEP", AR_SOFT, 1);
        return;
    }
    for (row = 0; row < 7 && first + row < (int32_t)pl_count; row++) {
        uint32_t k = (uint32_t)(first + row);
        int32_t yy = 64 + row * 22;
        int sel = k == pl_cursor, playing = demo_on && k == pl_idx;
        if (sel || pl_del == k + 1u)
            cv_rect(6, yy - 5, 228, 20, pl_del == k + 1u ? AR_ACC : AR_PANEL);
        if (pl_del == k + 1u) {                                             /* "erase?" */
            str_cpy(b, "ERASE ", 8);
            str_cpy(b + 6, pl_item[k].name, sizeof b - 6u);
            ar_txt(12, yy, b, AR_BG, ar_fit(b, 160, 2));
            ar_txt(228 - ar_pf_w("SEQ REC", 1), yy + 3, "SEQ REC", AR_BG, 1);
            continue;
        }
        ar_txt(12, yy, playing ? ">" : " ", AR_ACC, 2);
        ar_txt(30, yy, pl_item[k].name, sel || playing ? AR_INK : AR_SOFT, ar_fit(pl_item[k].name, 140, 2));
        ar_time(c, pl_item[k].dur10);
        ar_txt(228 - ar_pf_w(c, 2), yy, c, AR_SOFT, 2);
    }
    ar_txt(-1, 224, "K2 CHOOSE  SEQ REC ERASE  LET GO: PLAY", AR_SOFT, 1);
}
/* the ring index of a pad's first note since it was touched, or sk_w (none yet) */
static uint32_t ar_rec_first(void) { return sk_first_on(ar_rec_i0, sk_w); }
/* the band y0 .. y0 + 120 of the screen */
static void ar_band(uint32_t y0, uint32_t p1000, uint32_t ang)
{
    char b[28], c[12];
    ar_band_y0 = (int32_t)y0;
    cv_begin(240u, 120u, AR_BG);
    cv_oy = -(int32_t)y0;
    if (ar_tp.list)
        ar_list();
    else {
        ar_gonio();                                                          /* 52: the stereo image, the whole screen */
        if (ar_rec_pad != 0xFFu) {                                           /* the pads tell (their LEDs); a quiet word */
            str_cpy(b, "REC ", 5);
            ar_utoa(b + 4, ar_rec_pad + 1u);
            if (ar_rec_first() < sk_w) {
                str_cpy(b + str_len(b), "  ", 3);
                ar_time(c, (sk_clock - sk_ev[ar_rec_first() % SK_EV].t) / 441u);
                str_cpy(b + str_len(b), c, 8);
            }
            ar_txt(4, 4, b, AR_SOFT, 1);
        } else if (demo_on && pl_count) {
            const pl_item_t *it = &pl_item[pl_idx % PL_MAX];
            str_cpy(b, it->name, sizeof b);
            str_cpy(b + str_len(b), "  ", 3);
            ar_time(c, demo_pos / 441u);
            str_cpy(b + str_len(b), c, 8);
            ar_txt(4, 4, b, AR_SOFT, 1);
        }
        if (!ar_local)
            ar_txt(236 - ar_pf_w("MIDI", 1), 4, "MIDI", AR_SOFT, 1);        /* the bow off: a quiet word */
    }
    if (ar_lab.name[0] && (int32_t)(fm1_ms - ar_lab.until) < 0) {           /* what was just moved, a moment, big in the
                                                                              * middle over the image (52) */
        int32_t sn = ar_fit(ar_lab.name, 220, 3), sv = ar_fit(ar_lab.val, 220, 6);
        cv_rect(0, 84, 240, 64, AR_BG);
        ar_txt(-1, 90, ar_lab.name, AR_SOFT, sn);
        ar_txt(-1, 92 + 6 * sn, ar_lab.val, AR_INK, sv);
    }
    cv_oy = 0;
    cv_blit(0, y0);
}

static uint8_t ar_shown_sound;
static void ar_draw(void)
{
    static uint32_t t0, t_start;
    static uint8_t title = 1, idle;
    uint32_t now = fm1_ms;
    while (ar_ev_r != ar_ev_w) {                                             /* the notes heard */
        uint16_t e = ar_ev[ar_ev_r % AR_EVQ];
        uint32_t vel = e >> 7;
        ar_ev_r++;
        if (title && vel)
            title = 2;                                                       /* a note: the title goes */
    }
    if (title) {   /* the opening (4.5 s, a note skips it): BUNGLE SOCIETY comes down from the top to the middle in the
                    * FM-1's letters, as on the other firmwares; then MODULAR LANDSCAPE under it */
        static int32_t last_y = -100;
        static uint8_t named;
        int32_t t, y;
        uint32_t b;
        if (!t_start)
            t_start = now ? now : 1u;
        t = (int32_t)(now - t_start);
        if (title == 1 && t < 4500) {
            y = t >= 2000 ? 104 : -14 + 118 * t / 2000;
            if (y != last_y || (t >= 2300 && !named)) {
                last_y = y;
                named = t >= 2300;
                for (b = 0; b < 2u; b++) {
                    int32_t y0 = (int32_t)b * 120;
                    cv_begin(240u, 120u, M1_RGB(AR_BG_R, AR_BG_G, AR_BG_B));
                    ar_pf(240u, 120u, 120 - ar_pf_w("BUNGLE SOCIETY", 2) / 2, y - y0, "BUNGLE SOCIETY",
                          swap16(M1_RGB(0xF0, 0xDC, 0xBE)), 2);
                    if (named && b)                                          /* (the name: in the lower half) */
                        sf_draw(&SF_SMALL, 240u, 120u, 120 - sf_width(&SF_SMALL, "MODULAR LANDSCAPE") / 2, y + 24 - y0,
                                "MODULAR LANDSCAPE", M1_RGB(0xC8, 0xB4, 0x96));
                    cv_blit(0, (uint32_t)y0);
                }
            }
            return;
        }
        title = 0;
        idle = 0;                                                            /* clear it on the next frame */
    }
    {   /* a picture: when something changed; ~8 a second while a sketch plays */
        static uint8_t lab_on, was_on, rec;
        uint32_t on = ar_lab.name[0] && (int32_t)(now - ar_lab.until) < 0, p1000 = 0;
        static uint32_t ang;
        if (on != lab_on || demo_on != was_on || rec != ar_rec_pad)
            ar_redraw = 1;
        lab_on = (uint8_t)on;
        was_on = demo_on;
        rec = ar_rec_pad;
        if (demo_on && now - t0 >= 120u)
            ar_redraw = 1;
        if (now - t0 >= 80u && (ar_c_ll > 4096 || ar_c_rr > 4096 || ar_shown_sound))
            ar_redraw = 1;                                                   /* (51: the image moves with the sound,
                                                                              * ~12 a second; once more when it stops) */
        ar_shown_sound = ar_c_ll > 4096 || ar_c_rr > 4096;
        if (ar_rec_pad != 0xFFu && now - t0 >= 250u)
            ar_redraw = 1;
        if (!ar_redraw && idle)
            return;
        if (now - t0 < 40u && idle)
            return;
        t0 = now;
        ar_redraw = 0;
        idle = 1;
        if (demo_on && pl_count) {
            const pl_item_t *it = &pl_item[pl_idx % PL_MAX];
            uint32_t el = demo_pos / 441u, dur = it->dur10;
            p1000 = dur ? (el >= dur ? 1000u : el * 1000u / dur) : 0u;
            ang = demo_pos * 1715u;                                          /* (a turn every ~2.5 s) */
        }
        ar_band(0, p1000, ang);
        ar_band(120, p1000, ang);
    }
}

#define m1_init ar_init
#define m1_input ar_input
#define m1_leds ar_leds
#define m1_draw ar_draw
