/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular Landscape on the FM-1: the controls. Alexis's folk guitar played with a bow, from his own played pieces
 * (0.3: the SMK-37's Modular Landscape 54, its sound: tools/archet/v41, C2 .. G4, a loop over two strokes for SUSTAIN,
 * eng_sample.c MC_ARCHET), on the FM-1's 27 keys, with Modular One's tape and sketchbook.
 *   the keys     F2 .. G4, all of them sound; OCT- : F1 .. G3
 *   K1 LENGTH    the stroke: 635 ms .. FULL (DEC 89 .. 114; past ~1.85 s the whole stroke)
 *   K2 ATTACK    the hair biting (ATK 20) .. the bow settling slowly (ATK 100, ~1.4 s)
 *   K3 SPACE     the reverb, a dry room .. a big one (its send and size; the room in stereo: fx.c MC_ARCHET)
 *   K4 RELEASE   once the key is up: short .. long (REL 40 .. 112)
 *   PRESETS      PURE: the bow's noise taken away, the note left (the SMK's mod wheel)
 *   the effects  a button each, touched: on / off; SELECT then sets its amount (0.4: the last effect touched,
 *                held or not; it stays on or off):
 *                FX SUSTAIN (the note held as long as its key), SCL BREATH (the held notes swell and ebb, ~3 s;
 *                its depth), ENV CONTRA (the lowest key doubled an octave under; its volume), LFO DRONE (the lowest key
 *                held, else the last played, held; its volume), EDIT DISTANCE (further: less dry, more room, duller),
 *                ARP MUTE (con sordino: the tone closed), HOME ECHO (a dotted 1/8; how much). Lit while on.
 *                (The SMK's WIDTH is not here: the FM-1's speaker is mono)
 *   PLAY, OCT- + OCT+ held 1 s, OCT+ / OCT- while it plays, REC held, SEQ held (the list, REC: erase): the tape and the
 *                sketchbook, exactly as in Modular One (m1_tape.c, m1_book.c)
 *   SAVE         keeps your bow (0.4): the four knobs, the effects' amounts and PURE, at power-on again; lit when
 *                what plays is what is kept; held 2 s: the factory ones
 *   GLO          the settings: MIDI CHANNEL, TUNING, SPEAKER, SLEEP (K1 or SELECT the line, K2 or PRESETS its value;
 *                GLO or HOME: back)
 *   MASTER       the volume (master.c). */
#define M1_TAPE 1

enum { KC_LEN, KC_ATK, KC_SPACE, KC_REL, KC_N };
static const char *const KC_NAME[KC_N] = {"LENGTH", "ATTACK", "SPACE", "RELEASE"};
/* 0.3: the effects, on the buttons the tape does not use */
enum { FXE_SUS, FXE_BRE, FXE_CTR, FXE_DRN, FXE_DIS, FXE_MUT, FXE_ECH, FXE_N };
static const char *const FXE_NAME[FXE_N] = {"SUSTAIN", "BREATH", "CONTRA", "DRONE", "DISTANCE", "MUTE", "ECHO"};
static const uint8_t FXE_BTN[FXE_N] = {B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_ARP, B_HOME};
static struct {
    uint8_t on[FXE_N], amt[FXE_N];
    uint8_t held, turned;              /* the button held (0xFF none); SELECT turned while it was */
    uint8_t cur;                       /* (0.4) the effect SELECT sets: the last one touched (0xFF none yet) */
    uint8_t ctr_key, drn_key, last;    /* the notes CONTRA and DRONE play (sounding, 0 none); the last key played */
    uint8_t pure;
} fe = {.amt = {64, 64, 64, 64, 64, 64, 64}, .held = 0xFF, .cur = 0xFF, .last = 50};
static const uint8_t KC_DEF[KC_N] = {107, 0, 51, 71};                       /* DEC 110, ATK 20, the room, REL 80 */

/* what the screen shows: the last control touched (ls_draw.c) */
enum { M1T_NONE, M1T_KNOB, M1T_VOL, M1T_HOLD, M1T_OCT };
static struct {
    uint8_t touched, what;
    int16_t value;
    uint32_t t_ms;
    uint32_t active_ms;      /* the last hand on anything (the screen sleeps after SLEEP's time) */
    uint8_t globe, grow, dirty;
    const char *hlab;
    char hval[16], hbuf[20];
    uint8_t k[KC_N];
    uint8_t hold, hold_used; /* SEQ held (B_SEQ, 0xFF none) */
    uint8_t list;
    uint8_t rec_down;
    uint32_t rec_ms, octdn_ms;
    uint8_t oct_held, oct_both;
    int8_t oct_before;
    uint32_t both_ms;
    uint8_t save_down;
    uint32_t save_ms;
    uint32_t notes_ms;
} m1 = {.k = {107, 0, 51, 71}, .hold = 0xFF};

/* kept with the settings, in bytes no engine uses (favorites.factory[13] 16..31, as Modular Compagnon's and Koi's):
 * the knobs SAVEd ([0] 'L'), the settings ([8] 'G') */
#define KS (favorites.factory[13] + 16)
enum { KS_MAGIC, KS_K1, KS_G = 8, KS_TUNE, KS_CH };
/* (0.4) SAVE keeps the effects' amounts and PURE too, in the free bytes: 5 .. 7 and 11 .. 15 */
static const uint8_t KS_FX[FXE_N + 1] = {5, 6, 7, 11, 12, 13, 14, 15};       /* the 7 amounts, then PURE */

/* ------------------------------------------------------------ GLO, the settings */
enum { G1_CH, G1_TUNE, G1_EQ, G1_SCR, G1_N };
static const char *const G1_NAME[G1_N] = {"MIDI CHANNEL", "TUNING", "SPEAKER", "SLEEP"};
static const char *const G1_SCR_N[5] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
#define G1_FOOT "K1 LINE  K2 VALUE"

static uint32_t g1_n(uint32_t r)
{
    return r == G1_CH ? menu_n(MI_TCH1) : r == G1_TUNE ? menu_n(MI_TUNE) : r == G1_EQ ? menu_n(MI_LOWCUT) : 5u;
}
static uint32_t g1_get(uint32_t r)
{
    return r == G1_CH ? menu_get(MI_TCH1) : r == G1_TUNE ? menu_get(MI_TUNE) : r == G1_EQ ? menu_get(MI_LOWCUT) :
           scr_get();
}
static const char *g1_vname(uint32_t r, char *b)                             /* (b: 12 bytes) */
{
    uint32_t v = g1_get(r);
    switch (r) {
    case G1_CH: return menu_vname(MI_TCH1, v);
    case G1_TUNE: str_cpy(b, menu_vname(MI_TUNE, v), 8); str_cpy(b + str_len(b), " CT", 4); return b;
    case G1_EQ: return menu_vname(MI_LOWCUT, v);
    default: return G1_SCR_N[v % 5u];
    }
}
static int g1_hot(uint32_t r) { return m1.grow == r; }
static void g1_put(uint32_t r, uint32_t v)
{
    switch (r) {
    case G1_CH: menu_put(MI_TCH1, v); break;
    case G1_TUNE: menu_put(MI_TUNE, v); break;
    case G1_EQ: menu_put(MI_LOWCUT, v); break;
    default: scr_put(v); break;
    }
    KS[KS_G] = 'G';
    KS[KS_TUNE] = (uint8_t)menu_get(MI_TUNE);
    KS[KS_CH] = (uint8_t)menu_get(MI_TCH1);
    settings_save();
}
static void g1_line(int32_t s)
{
    m1.grow = (uint8_t)clamp((int32_t)m1.grow + (s > 0 ? 1 : -1), 0, G1_N - 1);
    m1.active_ms = fm1_ms;
    m1.dirty = 1;
}
static void g1_turn(int32_t s)
{
    uint32_t r = m1.grow % G1_N;
    int32_t n = (int32_t)g1_n(r), v = (int32_t)g1_get(r) + (s > 0 ? 1 : -1);
    if (r == G1_TUNE && (s > 1 || s < -1))
        v += s > 0 ? 4 : -4;
    g1_put(r, (uint32_t)clamp(v, 0, n - 1));
    m1.active_ms = fm1_ms;
    m1.dirty = 1;
}

/* ------------------------------------------------------------ the words on the screen */
static void m1_touch(uint32_t kind, uint32_t what, int32_t v)
{
    m1.touched = (uint8_t)kind;
    m1.what = (uint8_t)what;
    m1.value = (int16_t)v;
    m1.t_ms = m1.active_ms = fm1_ms;
    m1.dirty = 1;
}
static void mp_show(const char *lab, const char *val)
{
    m1.hlab = lab;
    str_cpy(m1.hval, val, sizeof m1.hval);
    m1_touch(M1T_HOLD, 0, 0);
}
static void m1_say(const char *lab, const char *val)                         /* (the tape's words: copied) */
{
    str_cpy(m1.hbuf, lab, sizeof m1.hbuf);
    mp_show(m1.hbuf, val);
}

/* ------------------------------------------------------------ the bow */
#define M1T (&trk[0])
static void ls_world(void)
{
    uint32_t k, sp = m1.k[KC_SPACE], dist = fe.on[FXE_DIS] ? fe.amt[FXE_DIS] : 0u, cut;
    song.g[G_RSIZE] = (int16_t)(60u + sp * 67u / 127u);                      /* the room */
    song.g[G_DTIME] = 11;                                                    /* ECHO: a dotted 1/8 */
    song.g[G_DFDBK] = (int16_t)(30u + fe.amt[FXE_ECH] * 50u / 127u);
    song.g[G_DMIX] = 110;
    ar_breath = fe.pure;                                                     /* PURE (eng_sample.c) */
    for (k = 0; k < 3u; k++) {                                               /* the keys, the DRONE, the CONTRA */
        track_t *t = &trk[k];
        t->p[P_DEC] = (int16_t)(89u + m1.k[KC_LEN] * 25u / 127u);
        t->p[P_ATK] = (int16_t)(20u + m1.k[KC_ATK] * 80u / 127u);
        t->p[P_REL] = (int16_t)(40u + m1.k[KC_REL] * 72u / 127u);
        t->p[P_REV] = (int16_t)((sp * (k == 2u ? 30u : 110u) + dist * 90u) / 127u);
        cut = fe.on[FXE_MUT] ? 127u - fe.amt[FXE_MUT] * 100u / 127u : 127u;  /* MUTE: the tone closed (CUT) */
        cut -= cut * dist * 30u / (127u * 127u);                             /* DISTANCE: duller */
        t->p[P_E4] = (int16_t)cut;
        t->p[P_SUS] = fe.on[FXE_SUS] ? 118 : 0;                              /* SUSTAIN: held (the loop) */
        t->p[P_E3] = fe.on[FXE_SUS] ? 1 : 0;
        t->p[P_DLY] = (int16_t)(fe.on[FXE_ECH] && k != 2u ? 20u + fe.amt[FXE_ECH] * 107u / 127u : 0u);
        t->p[P_CHOR] = 0;
        t->p[P_DIST] = 0;
        t->p[P_VOICE] = V_POLY;
        t->p[P_AMODE] = 0;
        t->p[P_CHRD] = CH_OFF;
        t->p[P_QUANT] = QN_OFF;
        t->p[P_LD_FLT] = 0;
        if (k == 1u) {                                                       /* DRONE: in and out slowly, breathing */
            t->p[P_ATK] = 96;
            t->p[P_REL] = 110;
            t->p[P_DEC] = 100;
            t->p[P_SUS] = 127;
            t->p[P_E3] = 1;
            t->p[P_LRATE] = 18;
            t->p[P_LTRIG] = 0;
            t->p[P_LD_AMP] = 30;
            t->p[P_LEVEL] = (int16_t)((30u + fe.amt[FXE_DRN] * 97u / 127u) * (127u - dist * 50u / 127u) / 127u);
        } else {                                                             /* BREATH: a slow LFO on the level, free */
            t->p[P_LRATE] = 36;
            t->p[P_LTRIG] = 1;
            t->p[P_LD_AMP] = (int16_t)(fe.on[FXE_BRE] ? fe.amt[FXE_BRE] : 0u);
            t->p[P_LEVEL] = (int16_t)(k == 2u ? (30u + fe.amt[FXE_CTR] * 97u / 127u) * (127u - dist * 50u / 127u) / 127u
                                              : 108u - dist * 16u / 127u);
        }
    }
    m1_vintage = 0;
}
/* CONTRA, DRONE: the notes they play (the main loop; the audio ISR held off while a part changes) */
static void fe_play(uint32_t part, uint8_t *cur, uint32_t note)
{
    if (note == *cur)
        return;
    fm1_irq_off();
    if (*cur)
        trk_note_off(&trk[part], *cur);
    *cur = (uint8_t)note;
    if (note)
        trk_note_on(&trk[part], note, 90);
    fm1_irq_on();
}
static uint32_t fe_lowest(void)                                              /* the lowest key held, sounding; 0 none */
{
    uint32_t k, held = fm1_in.notes;
    for (k = 0; k < 27u; k++)
        if ((held >> k) & 1u) {
            int32_t n = 53 + (int32_t)k + 12 * song.octave + trk[0].p[P_TRANS];
            if (n >= 36 && n <= 67)
                return (uint32_t)n;
        }
    return 0;
}
static void fe_notes(void)                                                   /* each frame: CONTRA follows the bass */
{
    uint32_t low = fe_lowest();
    if (low)
        fe.last = (uint8_t)low;
    fe_play(2, &fe.ctr_key, fe.on[FXE_CTR] && low ? low - 12u : 0u);
}
static void fe_toggle(uint32_t e)
{
    fe.on[e] ^= 1u;
    if (e == FXE_DRN)
        fe_play(1, &fe.drn_key, fe.on[e] ? (fe_lowest() ? fe_lowest() : fe.last) : 0u);
    ls_world();
    fe_notes();
    m1_say(FXE_NAME[e], fe.on[e] ? "ON" : "OFF");
}
/* the value shown for knob c: a time (ms) or a % */
static void ls_val(uint32_t c, char *b)
{
    track_t *t = M1T;
    if (c == KC_SPACE) {
        fmt_int(b, (int32_t)((m1.k[c] * 100u + 63u) / 127u));
        str_cpy(b + str_len(b), " %", 3);
        return;
    }
    {
        uint32_t v = (uint32_t)(c == KC_LEN ? t->p[P_DEC] : c == KC_ATK ? t->p[P_ATK] : t->p[P_REL]);
        uint32_t ms = TIME_MS_X10[v & 127u] / 10u;
        if (c == KC_LEN && ms >= 1850u) {                                    /* (past a stroke's own length: the whole) */
            str_cpy(b, "FULL", 5);
            return;
        }
        if (ms >= 1000u) {
            fmt_int(b, (int32_t)(ms / 1000u));
            str_cpy(b + str_len(b), ".", 2);
            fmt_int(b + str_len(b), (int32_t)(ms % 1000u / 100u));
            str_cpy(b + str_len(b), " S", 3);
        } else {
            fmt_int(b, (int32_t)ms);
            str_cpy(b + str_len(b), " MS", 4);
        }
    }
}
static int ls_saved(void)
{
    uint32_t i;
    if (KS[KS_MAGIC] != 'L')
        return 0;
    for (i = 0; i < KC_N; i++)
        if (KS[KS_K1 + i] != m1.k[i])
            return 0;
    for (i = 0; i < FXE_N; i++)
        if (KS[KS_FX[i]] != fe.amt[i])
            return 0;
    return KS[KS_FX[FXE_N]] == fe.pure;
}
static void ls_save(void)
{
    uint32_t i;
    KS[KS_MAGIC] = 'L';
    for (i = 0; i < KC_N; i++)
        KS[KS_K1 + i] = m1.k[i];
    for (i = 0; i < FXE_N; i++)
        KS[KS_FX[i]] = fe.amt[i];
    KS[KS_FX[FXE_N]] = fe.pure;
    settings_save();
    mp_show("YOUR BOW", "SAVED");
}
static void ls_factory(int say)
{
    uint32_t i;
    for (i = 0; i < FXE_N; i++)
        fe.amt[i] = 64;
    fe.pure = 0;
    for (i = 0; i < KC_N; i++)
        m1.k[i] = KC_DEF[i];
    ls_world();
    if (say)
        mp_show("THE BOW", "FACTORY");
}

static void m1_init(void)
{
    uint32_t k, i;
    track_t *t = M1T;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            if (param_kept(i))
                trk[k].p[i] = TP[i].def;
        track_defaults_steps(&trk[k]);
    }
    song.octave = 0;
    song.rec = 0;
    song.sel = 0;
    fm1_led_dim_level(0);
    for (k = 1; k < NTRK; k++)                                               /* parts 2..4 silent */
        trk[k].p[P_LEVEL] = 0;
    for (k = 1; k < 3u; k++) {                                               /* 0.3: part 2 the DRONE, part 3 the CONTRA */
        fm1_irq_off();
        trk[k].eng_req = ENGI_SAMPLE;
        fm1_irq_on();
        apply_preset_to(&trk[k], 0);
        trk[k].p[P_TRANS] = 0;
        trk[k].p[P_SPRD] = 0;
    }
    song.g[G_RTYPE] = 0;                                                     /* ROOM (in stereo: fx.c MC_ARCHET) */
    song.g[G_CLOCK] = 0;
    fm1_irq_off();
    t->eng_req = ENGI_SAMPLE;
    fm1_irq_on();
    apply_preset_to(t, 0);                                                   /* ARCHET */
    t->p[P_TRANS] = -12;                                                     /* the keys: F2 .. G4 */
    t->p[P_SPRD] = 40;                       /* stereo: the notes placed by pitch, C2 left .. C4 right, a guitar wide
                                              * (voice.c MC_ARCHET) */
    ls_factory(0);
    if (KS[KS_MAGIC] == 'L') {                                               /* your bow */
        for (i = 0; i < KC_N; i++)
            m1.k[i] = (uint8_t)(KS[KS_K1 + i] > 127u ? KC_DEF[i] : KS[KS_K1 + i]);
        for (i = 0; i <= FXE_N; i++)                                         /* (0.4: the amounts, PURE; an older save
                                                                              * left them 0: the defaults) */
            if (KS[KS_FX[i]] && KS[KS_FX[i]] <= 127u) {
                if (i < FXE_N)
                    fe.amt[i] = KS[KS_FX[i]];
                else
                    fe.pure = KS[KS_FX[i]];
            }
    }
    if (KS[KS_G] == 'G') {
        if (KS[KS_TUNE] < menu_n(MI_TUNE))
            menu_put(MI_TUNE, KS[KS_TUNE]);
        if (KS[KS_CH] < menu_n(MI_TCH1))
            menu_put(MI_TCH1, KS[KS_CH]);
    }
    ls_world();
    m1_touch(M1T_NONE, 0, 0);
}

/* a knob turned s */
static void ls_knob(uint32_t c, int32_t s)
{
    if (s > 1 || s < -1)
        s *= 2;                                                              /* (a fast turn goes further) */
    m1.k[c] = (uint8_t)clamp((int32_t)m1.k[c] + s * 2, 0, 127);
    ls_world();
    m1_touch(M1T_KNOB, c, m1.k[c]);
}

/* the keys that sound (the bow: C2 .. G4 after the transposition and the octave): key k 0 = F3 .. 26 = G5 */
static int ls_key_sounds(uint32_t k)
{
    int32_t n = 53 + (int32_t)k + 12 * song.octave + M1T->p[P_TRANS];
    return n >= 36 && n <= 67;
}

static int fe_of(uint32_t b)                                                 /* the effect on button b, -1 none */
{
    uint32_t e;
    for (e = 0; e < FXE_N; e++)
        if (FXE_BTN[e] == b)
            return (int)e;
    return -1;
}
static void m1_btn(uint32_t b, int down)
{
    if (!m1.globe && !m1.list && fe_of(b) >= 0) {                            /* 0.3: an effect's button */
        uint32_t e = (uint32_t)fe_of(b);
        if (down) {
            fe.held = (uint8_t)e;
            fe.cur = (uint8_t)e;                                             /* (0.4: SELECT now sets this one) */
            fe.turned = 0;
        } else if (fe.held == e) {
            if (!fe.turned)
                fe_toggle(e);                                                /* touched: on / off */
            fe.held = 0xFF;
        }
        return;
    }
    if (!down) {
        if (b == m1.hold) {                                                  /* SEQ let go */
            if (m1.hold_used == 1u && pl_count) {                            /* a piece chosen in the list: it plays */
                if (demo_on) {
                    pl_target = pl_cursor;
                    demo_req = 6;
                } else {
                    pl_start = pl_cursor;
                    demo_req = 1;
                }
            }
            m1.list = 0;
            pl_del = 0;
            m1.dirty = 1;
            m1.hold = 0xFF;
        }
        if (b == B_REC)
            m1.rec_down = 0;
        if (b == B_OCTDN || b == B_OCTUP) {                                  /* the tape: OCT- / OCT+ act when let go */
            if (demo_on && !m1.oct_both) {
                if (b == B_OCTUP)
                    demo_req = 3;
                else
                    demo_req = m1.octdn_ms && fm1_ms - m1.octdn_ms < 1000u ? 5u : 4u;
                m1.octdn_ms = b == B_OCTDN && demo_req == 4u ? (fm1_ms | 1u) : 0u;
                m1.active_ms = fm1_ms;
                m1.dirty = 1;
            }
            m1.oct_held &= (uint8_t)~(b == B_OCTDN ? 1u : 2u);
            if (!m1.oct_held)
                m1.oct_both = 0;
        }
        if (b == B_SAVE && m1.save_down) {
            if (m1.save_down == 1u)
                ls_save();
            m1.save_down = 0;
        }
        return;
    }
    switch (b) {
    case B_OCTDN: case B_OCTUP:
        if (m1.oct_held) {                                                   /* both held: the tape's gesture */
            m1.oct_held |= b == B_OCTDN ? 1u : 2u;
            m1.oct_both = 1;
            m1.both_ms = fm1_ms;
            if (!demo_on)
                song.octave = m1.oct_before;
            break;
        }
        m1.oct_held = b == B_OCTDN ? 1u : 2u;
        m1.oct_before = song.octave;
        if (demo_on)
            break;
        song.octave = (int8_t)clamp(song.octave + (b == B_OCTUP ? 1 : -1), -1, 0);
        m1_touch(M1T_OCT, 0, song.octave);
        break;
    case B_SEQ:                                                              /* the tape's list */
        m1.hold = (uint8_t)b;
        m1.hold_used = 0;
        m1.list = 1;
        pl_cursor = (uint8_t)(demo_on ? pl_idx : pl_start);
        pl_del = 0;
        m1.globe = 0;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case B_PLAY:
        if (!pl_count && !demo_on) {
            m1_say("TAPE", "EMPTY");
            break;
        }
        demo_req = demo_on ? 2u : 1u;
        m1.touched = M1T_NONE;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case B_REC:
        if (m1.list) {                                                       /* in the list: erase this sketch? */
            if (pl_cursor < pl_songs) {
                m1_say(pl_item[pl_cursor].name, "A SONG");
            } else if (pl_cursor < pl_count && pl_del == pl_cursor + 1u) {
                m1_del_req = (uint8_t)(pl_cursor + 1u);
                pl_del = 0;
            } else if (pl_cursor < pl_count) {
                pl_del = (uint8_t)(pl_cursor + 1u);
            }
            m1.hold_used = 2;
            m1.dirty = 1;
            break;
        }
        m1.rec_down = 1;
        m1.rec_ms = fm1_ms;
        break;
    case B_SAVE: m1.save_down = 1; m1.save_ms = fm1_ms; break;
    case B_GLO:
        m1.globe ^= 1u;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case B_HOME:
        if (m1.globe) {
            m1.globe = 0;
            m1.dirty = 1;
        }
        break;
    default: break;
    }
}

static void m1_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), k;
    int32_t s;
    master_poll();
    midi_take();
    {   /* the volume (the MASTER pot): only to show it */
        static int32_t last = -1;
        int32_t v = master_knob / 16;
        if (last >= 0 && (v - last > 6 || last - v > 6))
            m1_touch(M1T_VOL, 0, v >= 1000 ? 100 : v / 10);
        if (last < 0 || v - last > 6 || last - v > 6)
            last = v;
    }
    s = panel_enc(EN_PRESET);
    if (s && m1.globe)
        g1_turn(s);
    else if (s) {                                                            /* PURE */
        char b[8];
        fe.pure = (uint8_t)clamp((int32_t)fe.pure + s * ((s > 1 || s < -1) ? 4 : 2), 0, 127);
        ls_world();
        fmt_int(b, (int32_t)((fe.pure * 100u + 63u) / 127u));
        str_cpy(b + str_len(b), " %", 3);
        m1_say("PURE", b);
    }
    s = panel_enc(EN_ALGO);
    (void)s;
    s = panel_enc(EN_SELECT);
    if (s && m1.globe)
        g1_line(s);
    else if (s && (fe.cur == 0xFFu || fe.cur == FXE_SUS))                  /* (0.4) nothing to set yet: say how */
        m1_say(fe.cur == FXE_SUS ? "SUSTAIN" : "SELECT", fe.cur == FXE_SUS ? "NO AMOUNT" : "TOUCH AN EFFECT");
    else if (s) {                                                            /* the effect last touched (or held): its
                                                                              * amount (0.4, Alexis: SELECT did nothing;
                                                                              * 0.3 set it only with the button held) */
        char b[8];
        uint32_t e = fe.cur;
        fe.amt[e] = (uint8_t)clamp((int32_t)fe.amt[e] + s * ((s > 1 || s < -1) ? 4 : 2), 0, 127);
        fe.turned = 1;
        ls_world();
        fmt_int(b, (int32_t)((fe.amt[e] * 100u + 63u) / 127u));
        str_cpy(b + str_len(b), " %", 3);
        m1_say(FXE_NAME[e], b);
    }
    for (k = 0; k < 4u; k++) {                                               /* K1 .. K4 */
        s = panel_enc(EN_K1 + k);
        if (!s)
            continue;
        if (m1.globe) {
            m1.active_ms = fm1_ms;
            if (k == 0)
                g1_line(s);
            else if (k == 1)
                g1_turn(s);
        } else if (m1.hold == B_SEQ) {                                       /* the tape's list */
            if (k == 0 && pl_count) {
                pl_cursor = (uint8_t)clamp((int32_t)pl_cursor + (s > 0 ? 1 : -1), 0, (int32_t)pl_count - 1);
                pl_del = 0;
                if (m1.hold_used != 2u)
                    m1.hold_used = 1;
                m1.dirty = 1;
            }
        } else
            ls_knob(k, s);
    }
    for (k = 0; k < FM1_NBTN; k++) {
        if ((pressed >> k) & 1u)
            m1_btn(panel_btn_of(k), 1);
        if ((released >> k) & 1u)
            m1_btn(panel_btn_of(k), 0);
    }
    if (m1.oct_both == 1u && m1.oct_held == 3u && fm1_ms - m1.both_ms >= 1000u) {   /* OCT- OCT+ held 1 s: the tape */
        m1.oct_both = 2;
        if (pl_count || demo_on)
            demo_req = demo_on ? 2u : 1u;
        else
            m1_say("TAPE", "EMPTY");
        m1.touched = M1T_NONE;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
    }
    if (m1.rec_down == 1u && fm1_ms - m1.rec_ms >= 600u) {                   /* REC held: keep the last phrase */
        m1.rec_down = 2;
        m1_keep_req = 1;
        m1.active_ms = fm1_ms;
    }
    if (m1.save_down == 1u && fm1_ms - m1.save_ms >= 2000u) {                /* SAVE held 2 s: the factory bow */
        m1.save_down = 2;
        ls_factory(1);
    }
    if (fm1_input_note_edges())
        m1.notes_ms = m1.active_ms = fm1_ms;
    fe_notes();
    if (demo_on)                                                             /* (the reels turn: the screen stays) */
        m1.active_ms = fm1_ms;
}

/* ------------------------------------------------------------ the LEDs: the keys that sound glow, a key held is lit,
 * the silent ones dark; the tape's buttons lit while they act */
static void m1_leds(void)
{
    uint8_t nl[LED_BUF] = {0}, nd[LED_BUF] = {0}, nb[LED_BUF] = {0};
    static const uint8_t BTN_ALL[7] = {B_GLO, B_SAVE, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP};   /* (0.4: the others, a
                                                                              * glow; the effects' dark unless on) */
    static uint8_t ready;
    uint32_t k, c, held = fm1_in.notes;
    if (fm1_led_anim_on())
        return;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    for (k = 0; k < 27u; k++) {
        uint32_t id = LED_KEY0 + k;
        if (!ls_key_sounds(k))
            continue;
        if ((held >> k) & 1u)
            led_put(nl, id, 1);
        else
            led_put(nd, id, 1);
    }
    for (k = 0; k < 7u; k++)
        led_put(nd, panel.btn[BTN_ALL[k]], 1);
    if (m1.globe)
        led_put(nd, panel.btn[B_HOME], 1);                                   /* (in GLO, HOME: back) */
    led_put(nl, panel.btn[B_GLO], m1.globe);
    led_put(nl, panel.btn[B_OCTDN], song.octave < 0);
    led_put(nl, panel.btn[B_PLAY], demo_on != 0);
    led_put(nl, panel.btn[B_REC], m1.rec_down != 0 || (m1.list && pl_del));
    led_put(nl, panel.btn[B_SEQ], m1.list);
    led_put(nl, panel.btn[B_SAVE], ls_saved());
    for (k = 0; k < FXE_N; k++)                                              /* 0.3: the effects, lit while on */
        led_put(nl, panel.btn[FXE_BTN[k]], fe.on[k] && !(FXE_BTN[k] == B_HOME && m1.globe));
    for (c = 0; c < LED_BUF; c++) {
        nd[c] &= (uint8_t)~(nl[c] | nb[c]);
        nb[c] &= (uint8_t)~nl[c];
    }
#ifndef LS_HOST
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = nb[c];
        fm1_led_mid[c] = 0;
    }
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = nl[c];
#endif
}
