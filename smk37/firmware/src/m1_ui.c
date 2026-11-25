/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One on the SMK-37: the controls (what you see is what you get: one control, one job, no bank, no menu).
 *   16 pads      the sounds (VILLE, eng_ville.c). Top row: MODULAR JUNO SUPERSAW PWM TAPE FM SOUFFLE CREPUSC;
 *                bottom row: FLUTE BRASS STRINGS CHOIR RAIN DEEP PULSE MEMORY. A pad loads its sound.
 *   K1           the volume (smk_input.h: the FM-1's MASTER pot, master.c)
 *   K2 .. K8     CUTOFF RESO ATTACK DECAY SUSTAIN RELEASE DETUNE
 *   faders       1 day (up) / night (down), 2 FM (the winter; on SOUFFLE / CREPUSC the Montee), 3 echo TIME (free,
 *                10 ms .. 0.75 s), 4 ECHO (the send and its repeats)
 *   KNOB BANK / FADER BANK   WAVE 1 / WAVE 2: the next wave of the vending machines (lit where a sound has them)
 *   pitch wheel  bend (seq.c, as Felucca); mod wheel: the cutoff waves, faster as it goes up (the track LFO)
 *   OCTAVE- / +  octaves; ARP the arpeggio; SCALE the scale (snap); CHORD a chord on each key; NOTE REPEAT a
 *                (Modular Compagnon: ARP only: a tap turns it on / off; held, K2 its mode, K3 its speed, K4 the tempo;
 *                kept with the settings; a DAW's MIDI clock is followed by itself, seq.c. CHORD: an A 440 to tune
 *                to, on / off. SCALE, NOTE REPEAT and the tape: nothing)
 *                1/16 repeat while held; FX the vintage (fx.c); GLOBE the settings: one knob, one setting (below)
 *   PLAY, REC, < >  nothing (Modular Compagnon has no tape recorder)
 *   37 keys      their notes (seq.c: the 27 middle keys, the 10 outer ones) on part 1
 * The screen: m1_draw.c. Only part 1 plays; the other three are silent. */

/* the pads: hardware pad -> VILLE sound; their colours (RGB, dim and lit) */
static const uint8_t M1_PAD_SOUND[16] = {0, 1, 2, 3, 4, 5, 14, 15, 6, 7, 8, 9, 10, 11, 12, 13};
static const uint8_t M1_PAD_RGB[16][3] = {
    {120, 160, 70}, {200, 110, 40}, {220, 160, 50}, {150, 120, 200}, {170, 120, 80}, {150, 200, 255},
    {240, 200, 140}, {230, 130, 90},
    {180, 220, 160}, {255, 70, 160}, {80, 220, 255}, {200, 120, 255}, {80, 140, 255}, {40, 80, 200},
    {255, 90, 60}, {200, 160, 230},
};
enum { M1_K_CUT, M1_K_RES, M1_K_ATK, M1_K_DEC, M1_K_SUS, M1_K_REL, M1_K_DTN, M1_NK };
static const uint8_t M1_K_PARAM[M1_NK] = {P_E3, P_E4, P_ATK, P_DEC, P_SUS, P_REL, P_E5};
static const char *const M1_K_NAME[M1_NK] = {"CUTOFF", "RESO", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DETUNE"};

/* what the screen shows: the last control touched (m1_draw.c) */
enum { M1T_NONE, M1T_KNOB, M1T_FADER, M1T_WAVE, M1T_BTN, M1T_PAD, M1T_VOL, M1T_HOLD };
enum { M1_B_LEFT = 2, M1_B_RIGHT = 3, M1_B_PLAY = 16, M1_B_REC = 17 };
static struct {
    uint8_t pad;             /* the sound's pad (hardware number) */
    uint8_t touched, what;   /* M1T_*, which control */
    int16_t value;           /* its value, 0..127 (or a wave notch) */
    uint32_t t_ms;           /* when */
    uint8_t vintage, globe;  /* FX on; the GLOBE screen open */
    uint8_t fader[4];        /* the faders as applied (0xFF: not read yet) */
    uint32_t notes_ms;       /* the last note played (the cat listens) */
    uint8_t vel;             /* its velocity */
    uint8_t dirty;
    uint32_t active_ms;      /* the last hand on anything (the screen sleeps after MENU's time) */
    uint8_t grow;            /* GLOBE: the setting turned last (0xFF none) */
    uint32_t grow_ms;
    uint8_t hold, hold_used; /* ARP / SCALE / CHORD held (0xFF none); a knob turned meanwhile */
    const char *hlab;        /* M1T_HOLD: the words */
    char hval[16];
} m1 = {.fader = {0xFF, 0xFF, 0xFF, 0xFF}, .grow = 0xFF, .hold = 0xFF};

/* GLOBE, the settings: K2 .. K7 one each. Kept with the settings, in bytes no engine uses: favorites.factory[13]
 * 24..31 (VILLE's favorites are bits of bytes 0..1; Modular One shows none). [0] 'M' once written. */
enum { G1_CH, G1_VEL, G1_TUNE, G1_PADS, G1_SCR, G1_N };
static const char *const G1_NAME[G1_N] = {"MIDI CHANNEL", "VELOCITY", "TUNING", "PADS", "SLEEP"};
static const char *const G1_VEL_N[4] = {"NORMAL", "SOFT", "HARD", "FIXED"};
static const char *const G1_PADS_N[4] = {"DIM", "SOFT", "BRIGHT", "VIVID"};
static const char *const G1_SCR_N[5] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
#define M1G (favorites.factory[13] + 24)
enum { M1G_MAGIC, M1G_VEL, M1G_PADS, M1G_CAT, M1G_PET = 7 };   /* ([4] [5]: TUNE, MIDI CH; [6] retired) */

static uint32_t g1_n(uint32_t r)
{
    return r == G1_CH ? menu_n(MI_TCH1) : r == G1_VEL ? 4u : r == G1_TUNE ? menu_n(MI_TUNE) : r == G1_PADS ? 4u :
           5u;
}
static uint32_t g1_get(uint32_t r)
{
    return r == G1_CH ? menu_get(MI_TCH1) : r == G1_VEL ? M1G[M1G_VEL] % 4u : r == G1_TUNE ? menu_get(MI_TUNE) :
           r == G1_PADS ? M1G[M1G_PADS] % 4u : scr_get();
}
/* the value's words (b: 12 bytes) */
static const char *g1_vname(uint32_t r, char *b)
{
    uint32_t v = g1_get(r);
    switch (r) {
    case G1_CH: return menu_vname(MI_TCH1, v);
    case G1_VEL: return G1_VEL_N[v];
    case G1_TUNE: str_cpy(b, menu_vname(MI_TUNE, v), 8); str_cpy(b + str_len(b), " CT", 4); return b;
    case G1_PADS: return G1_PADS_N[v];
    default: return G1_SCR_N[v % 5u];
    }
}
static void g1_put(uint32_t r, uint32_t v)
{
    switch (r) {
    case G1_CH: menu_put(MI_TCH1, v); break;
    case G1_VEL: M1G[M1G_VEL] = (uint8_t)v; smk_vel_curve = (uint8_t)v; break;
    case G1_TUNE: menu_put(MI_TUNE, v); break;
    case G1_PADS: M1G[M1G_PADS] = (uint8_t)v; break;
    default: scr_put(v); break;
    }
    M1G[M1G_MAGIC] = 'M';
    M1G[4] = (uint8_t)menu_get(MI_TUNE);                                     /* (the project's two: kept here too, */
    M1G[5] = (uint8_t)menu_get(MI_TCH1);                                     /*  Modular One has no project) */
    settings_save();                                                         /* (queued: settings_poll writes it) */
}
static void g1_turn(uint32_t r, int32_t s)
{
    int32_t n = (int32_t)g1_n(r), v = (int32_t)g1_get(r) + (s > 0 ? 1 : -1);
    if (r == G1_TUNE && (s > 1 || s < -1))
        v += s > 0 ? 4 : -4;                                                 /* (a fast turn: 5 cents) */
    g1_put(r, (uint32_t)clamp(v, 0, n - 1));
    m1.grow = (uint8_t)r;
    m1.grow_ms = fm1_ms;
    m1.dirty = 1;
}
static void g1_boot(void)                                                    /* the saved settings, at power-on */
{
    if (M1G[M1G_MAGIC] != 'M') {
        M1G[M1G_VEL] = 0;
        M1G[M1G_PADS] = 2;
        M1G[M1G_CAT] = 0;
        M1G[M1G_PET] = 0;
        return;
    }
    smk_vel_curve = (uint8_t)(M1G[M1G_VEL] % 4u);
    if (M1G[4] < menu_n(MI_TUNE))
        menu_put(MI_TUNE, M1G[4]);
    if (M1G[5] < menu_n(MI_TCH1))
        menu_put(MI_TCH1, M1G[5]);
}

#define M1T (&trk[0])
#define M1T_P(k) (trk[0].p[M1_K_PARAM[k]])

static void m1_touch(uint32_t kind, uint32_t what, int32_t v)
{
    m1.touched = (uint8_t)kind;
    m1.what = (uint8_t)what;
    m1.value = (int16_t)v;
    m1.t_ms = m1.active_ms = fm1_ms;
    m1.dirty = 1;
}

/* ARP: its choices (held, K2 .. K4), kept with the settings in favorites.factory[13][16..23] (bytes no engine uses;
 * [0] 'P' once written). They stay when the sound changes */
#define M1P (favorites.factory[13] + 16)
enum { MP_MAGIC, MP_ATYPE, MP_ARATE, MP_AOCT, MP_BPM };
enum { M1_B_ARP = 6, M1_B_CHORD = 9 };
#define MP_NATYPE 13u
static const uint8_t MP_ATYPE_V[MP_NATYPE] = {1, 2, 3, 7, 4, 5, 8, 9, 10, 11, 12, 13, 14};   /* params.c N_AMODE */
static const char *const MP_ATYPE_N[MP_NATYPE] = {"UP", "DOWN", "UP-DOWN", "DOWN-UP", "RANDOM",
                                                   "AS PLAYED", "UP +8", "CONVERGE", "DIVERGE", "PINKY",
                                                   "THUMB", "WALK", "CHORDS"};
#define MP_NRATE 7u
static const uint8_t MP_RATE_V[MP_NRATE] = {6, 0, 1, 4, 2, 5, 3};                              /* N_DIV, slow first */
static const char *const MP_RATE_N[MP_NRATE] = {"1/2", "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32"};

static void mp_boot(void)
{
    if (M1P[MP_MAGIC] != 'P' || M1P[MP_ATYPE] >= MP_NATYPE || M1P[MP_ARATE] >= MP_NRATE || M1P[MP_BPM] > 200u) {
        M1P[MP_ATYPE] = 0;                                                   /* up */
        M1P[MP_ARATE] = 4;                                                   /* 1/16 */
        M1P[MP_AOCT] = 0;                                                    /* one octave */
        M1P[MP_BPM] = 120 - 40;
    }
    song.g[G_CLOCK] = 0;                                                     /* (a DAW's clock: seq.c follows it) */
    song.g[G_BPM] = (int16_t)(40 + M1P[MP_BPM]);
}
/* the choices onto the sound (arp: on); no chord keys, no scale */
static void mp_apply(track_t *t, uint32_t arp)
{
    t->p[P_AMODE] = (int16_t)(arp ? MP_ATYPE_V[M1P[MP_ATYPE]] : 0);
    t->p[P_ARATE] = (int16_t)MP_RATE_V[M1P[MP_ARATE]];
    t->p[P_AOCT] = 1;
    t->p[P_CHRD] = CH_OFF;
    t->p[P_QUANT] = QN_OFF;
}

/* MODULAR COMPAGNON 2: two voices, one world. The keyboard plays Modular Town V3.2 (the plugin, over USB MIDI on the
 * MIDI channel of GLOBE, K2) and its own engine (VILLE), side by side.
 *   the world    the faders, always both: F1 day / night, F2 the snow (the winter), F3 the delay, F4 the reverb;
 *                < > the place, Modular Town / Neon Town
 *   the town     the top row of pads: the terminus (the opening electric piano), five keepers (Madame Roux, Ada,
 *                Mister Echo, Vera, Dusty), Wattson, Neon Town's jukebox. Pressed again: the keeper's next sound. The
 *                plugin goes there (index.js _mcPreset)
 *   the keyboard the bottom row: its own sounds (MC_KEYS): KEYS (a clean tape), TAPE, the Mellotron's FLUTES, and
 *                the composer's SOUFFLE MAREE VELOURS NUIT CREPUSCULE
 *   VOICE        (SCALE, lit: the town) which voice K2..K8 and the waves set; a pad picks its voice too
 *   PLAY         (NOTE REPEAT) who the keys play: BOTH, the TOWN, the KEYBOARD, or SPLIT (the keyboard below the
 *                split, the town from it; held, K2 moves the split). At power-on: BOTH once a computer is there,
 *                the KEYBOARD alone without one (until PLAY is pressed). The ARP follows the same rule (seq.c mc_dest)
 *   K1           the keyboard's own volume
 * The CC (dsp.cmajor, processor CompagnonMirror, has the same table): 20..26 K2..K8, 27 the town's sound (pad * 16 +
 * its variant), 28..31 F1..F4, 85 / 86 WAVE 1 / 2, 88 the place. */
#define MC_NT 8u
static const char *const MC_TOWN_WHO[MC_NT] = {"THE TERMINUS", "MADAME ROUX", "ADA", "MISTER ECHO", "VERA", "DUSTY", "WATTSON", "NEON TOWN"};
static const uint8_t MC_TOWN_NV[MC_NT] = {1, 3, 3, 3, 3, 3, 3, 7};
/* each sound's name, and its CUT RES ATK DEC SUS REL DTN WAVE1 WAVE2 as the plugin sets them (0..127; index.js
 * _npcScript `more`, _ntPresets, the V3.2 opening sound) */
static const char *const MC_TOWN_SND[MC_NT][7] = {
    {"E-PIANO", "", "", "", "", "", ""},
    {"SLOW BROTH", "QUICK FRY", "SUNDAY ORGAN", "", "", "", ""},
    {"ACID SQUELCH", "RUBBER BASS", "WHISTLING WIND", "", "", "", ""},
    {"DUB ECHOES", "SLAPBACK", "POSTCARD", "", "", "", ""},
    {"CATHEDRAL", "WOODEN ROOM", "WATERCOLOUR", "", "", "", ""},
    {"DUSTY RECORD", "JUNO AFTERNOON", "OLD FILM STRINGS", "", "", "", ""},
    {"VIBRATO", "SLOW TIDE", "WOBBLY TAPE", "", "", "", ""},
    {"BRASS", "STRINGS", "CHOIR", "RAIN", "DEEP", "PULSE", "MEMORY"},
};
static const uint8_t MC_TOWN_K[MC_NT][7][9] = {
    {{76, 10, 3, 64, 38, 38, 64, 64, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{61, 10, 70, 76, 89, 91, 64, 64, 0}, {79, 10, 0, 28, 0, 25, 64, 64, 0}, {74, 10, 4, 38, 127, 15, 64, 127, 127}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{25, 89, 3, 38, 6, 38, 64, 0, 0}, {38, 32, 3, 44, 13, 38, 64, 127, 0}, {70, 112, 64, 64, 76, 76, 64, 64, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{76, 10, 3, 32, 6, 38, 64, 64, 0}, {76, 10, 3, 64, 38, 38, 64, 64, 0}, {76, 10, 38, 64, 38, 76, 64, 64, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{76, 10, 44, 64, 108, 76, 64, 64, 0}, {76, 10, 3, 64, 38, 38, 64, 64, 0}, {53, 10, 57, 64, 38, 89, 71, 64, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{51, 10, 3, 64, 38, 38, 69, 64, 0}, {70, 10, 25, 64, 89, 64, 64, 64, 0}, {64, 10, 57, 64, 102, 76, 64, 64, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{76, 10, 3, 64, 89, 57, 64, 64, 0}, {57, 51, 3, 64, 102, 64, 64, 64, 0}, {76, 10, 3, 64, 38, 38, 64, 64, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{64, 10, 8, 51, 76, 51, 64, 64, 0}, {70, 10, 44, 64, 95, 70, 64, 64, 0}, {57, 10, 57, 64, 102, 76, 64, 64, 0}, {70, 10, 4, 44, 64, 57, 64, 64, 0}, {38, 10, 51, 76, 102, 76, 64, 64, 0}, {66, 10, 0, 23, 0, 25, 64, 64, 0}, {53, 10, 13, 57, 76, 64, 64, 64, 0}},
};
/* the keyboard's sounds: VILLE presets (eng_ville.c) */
static const uint8_t MC_KEYS[8] = {16, 4, 6, 14, 17, 18, 19, 15};
static const char *const MC_KEYS_NAME[8] = {"KEYS", "TAPE", "FLUTES", "SOUFFLE", "MAREE", "VELOURS", "NUIT", "CREPUSCULE"};
enum { MC_V_TOWN, MC_V_KEYS };
static struct {
    uint8_t voice;           /* MC_V_*: what K2..K8 and the waves set */
    uint8_t town, var, chosen;   /* the town's pad, its variant; a town pad pressed since power-on */
    uint8_t keys;            /* the keyboard's pad */
    uint8_t place;           /* 0 Modular Town, 1 Neon Town */
    uint8_t auto_play;       /* PLAY not pressed yet: it follows the USB */
    uint8_t online;          /* USB configured: the state was sent */
    uint8_t k[9];            /* the town's knobs and waves as last set */
} mc = {.voice = MC_V_KEYS, .auto_play = 1};
#define MC_PLACE (mc.place)

static void mc_cc(uint32_t n, uint32_t v)
{
    uint32_t ch = trk_midi_ch(0);
    if (!usb.config)
        return;
    fm1_irq_off();                       /* (midi_out_event is the audio ISR's: no two writers at once) */
    midi_out_event(0x0Bu | (0xB0u | ch) << 8 | (n & 0x7Fu) << 16 | (v > 127u ? 127u : v) << 24);
    fm1_irq_on();
}
static void mc_town_pick(uint32_t p)
{
    uint32_t i;
    p %= MC_NT;
    mc.var = (uint8_t)(mc.chosen && p == mc.town ? (mc.var + 1u) % MC_TOWN_NV[p] : 0u);
    mc.town = (uint8_t)p;
    mc.chosen = 1;
    mc.voice = MC_V_TOWN;
    for (i = 0; i < 9u; i++)
        mc.k[i] = MC_TOWN_K[p][mc.var][i];
    mc.place = (uint8_t)(p == MC_NT - 1u);
    mc_cc(27, p * 16u + mc.var);
}
/* the world as the faders stand, the place and the town's sound: when the town comes online */
static void mc_send_world(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        if (fm1_in.fader[i] <= 127u)
            mc_cc(28u + i, fm1_in.fader[i]);
    mc_cc(88, mc.place ? 127u : 0u);
    if (mc.chosen)
        mc_cc(27, mc.town * 16u + mc.var);
}
static const char *mc_play_name(void)
{
    return mc_play == 3u ? "BOTH" : mc_play == 2u ? "THE TOWN" : mc_play == 1u ? "THE KEYBOARD" : "SPLIT";
}

/* the waves the sound has (MODULAR TAPE FM: both; JUNO: WAVE 1, the saw / pulse mix); 0 none */
static uint32_t m1_waves(void)
{
    uint32_t m = (uint32_t)M1T->p[P_E0];
    if (mc.voice == MC_V_TOWN)
        return 3u;                                                           /* the town: both machines */
    return m == VM_MODULAR || m == VM_TAPE || m == VM_KEYS || m == VM_FM || VM_TOWN(m) ? 3u : m == VM_JUNO ? 1u : 0u;
}

/* the faders: the world (absolute: where a fader stands is what plays), the keyboard's sound and the town's.
 * F1 day (up) / night, F2 the snow (the winter; the Rise on the composer's voice), F3 the delay, F4 the reverb */
static void m1_faders_apply(int force)
{
    uint32_t i;
    track_t *t = M1T;
    for (i = 0; i < 4u; i++) {
        uint32_t v = fm1_in.fader[i];
        if (!force && v == m1.fader[i])
            continue;
        if (!force && m1.fader[i] != 0xFF) {
            m1_touch(M1T_FADER, i, (int32_t)v);
            mc_cc(28u + i, v);
        }
        m1.fader[i] = (uint8_t)v;
        switch (i) {
        case 0: t->p[P_E6] = (int16_t)(127 - v); break;                     /* NUIT */
        case 1: t->p[P_E7] = (int16_t)v; break;                             /* HIVER (the Montee on TOWN) */
        case 2:
            t->p[P_DLY] = (int16_t)v;                                        /* the delay: the send .. */
            song.g[G_DFDBK] = (int16_t)(25 + v * 60 / 127);                  /* .. and the repeats */
            break;
        default: t->p[P_REV] = (int16_t)v; break;                           /* the reverb */
        }
    }
}

/* the keyboard's sound (bottom pad q) */
static void mc_keys_load(uint32_t q)
{
    track_t *t = M1T;
    q %= 8u;
    mc.keys = (uint8_t)q;
    fm1_irq_off();
    t->eng_req = ENGI_VILLE;
    fm1_irq_on();
    {
        uint32_t arp = t->p[P_AMODE] != 0;
        apply_preset_to(t, MC_KEYS[q]);
        mp_apply(t, arp);                                                    /* the ARP stays as it is */
    }
    t->p[P_LWAVE] = 0;                                                       /* the mod wheel's LFO: a sine */
    m1_faders_apply(1);                                                      /* the faders stay what they say */
}
/* a pad: the top row the town, the bottom row the keyboard */
static void m1_sound(uint32_t pad)
{
    m1.pad = (uint8_t)(pad % 16u);
    if (m1.pad < 8u)
        mc_town_pick(m1.pad);
    else {
        mc_keys_load(m1.pad - 8u);
        mc.voice = MC_V_KEYS;
    }
    m1_touch(M1T_PAD, m1.pad, 0);
}

/* the mod wheel: the cutoff waves, slow and shallow at the bottom, fast and deep at the top */
static void m1_modwheel(void)
{
    static uint8_t last = 0xFF;
    uint32_t w = smk_modw;
    track_t *t = M1T;
    if (w == last)
        return;
    last = (uint8_t)w;
    t->p[P_LD_FLT] = (int16_t)(w * 48 / 127);
    t->p[P_LRATE] = (int16_t)(w ? 50 + w * 50 / 127 : 60);
}

static void m1_init(void)
{
    uint32_t k, i;
    /* the same on every SMK-37: nothing of a Felucca session (no autosave restored, main.c) nor of its settings
     * that are not Modular Compagnon's: the globals and every part's own values (those a preset keeps: level, pan,
     * mute, arp, scale, transpose, slicer, chord keys) at their defaults, no steps (a DAW's Start plays nothing) */
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            if (param_kept(i))
                trk[k].p[i] = TP[i].def;
        track_defaults_steps(&trk[k]);
    }
    song.octave = 0;
    fm1_led_dim_level(0);                                                    /* (the buttons' glow: Felucca's default) */
    for (k = 1; k < NTRK; k++)                                               /* parts 2..4 silent */
        trk[k].p[P_LEVEL] = 0;
    song.sel = 0;
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = 1;
    song.g[G_DMIX] = 100;
    smk_pad_free = 1;                                                        /* pads are ours: not notes */
    g1_boot();
    mp_boot();
    for (k = 0; k < 9u; k++)
        mc.k[k] = MC_TOWN_K[0][0][k];
    m1_dly_target = 441u + 64u * 257u;                                       /* the echo's time: 0.38 s */
    mc_keys_load(0);
    mc.voice = MC_V_KEYS;
}

/* what the screen says for the ARP */
static void mp_show(const char *lab, const char *val)
{
    m1.hlab = lab;
    str_cpy(m1.hval, val, sizeof m1.hval);
    m1_touch(M1T_HOLD, 0, 0);
}
static void mp_tempo(char *b)
{
    if (song.g[G_CLOCK]) {
        str_cpy(b, "DAW", 4);
        return;
    }
    fmt_int(b, song.g[G_BPM]);
    str_cpy(b + str_len(b), " BPM", 5);
}
/* ARP held, knob K2 + k turned s */
static void mp_turn(uint32_t k, int32_t s)
{
    track_t *t = M1T;
    int32_t d = s > 0 ? 1 : -1;
    uint32_t arp = t->p[P_AMODE] != 0;
    char b[16];
    m1.hold_used = 1;
    switch (k) {
    case 0: M1P[MP_ATYPE] = (uint8_t)clamp(M1P[MP_ATYPE] + d, 0, MP_NATYPE - 1); arp = 1; break;
    case 1: M1P[MP_ARATE] = (uint8_t)clamp(M1P[MP_ARATE] + d, 0, MP_NRATE - 1); break;
    case 2:
        if (song.g[G_CLOCK])
            break;                                                           /* (the DAW's tempo) */
        M1P[MP_BPM] = (uint8_t)clamp(M1P[MP_BPM] + (s > 1 || s < -1 ? d * 5 : d), 0, 200);
        song.g[G_BPM] = (int16_t)(40 + M1P[MP_BPM]);
        break;
    default: return;
    }
    mp_apply(t, arp);
    M1P[MP_MAGIC] = 'P';
    settings_save();                                                         /* (queued: settings_poll writes it) */
    if (k == 0) {
        mp_show("ARP", MP_ATYPE_N[M1P[MP_ATYPE]]);
    } else if (k == 1) {
        mp_show("SPEED", MP_RATE_N[M1P[MP_ARATE]]);
    } else {
        mp_tempo(b);
        mp_show("TEMPO", b);
    }
}

static void m1_btn(uint32_t id, int down)
{
    track_t *t = M1T;
    if (!down) {
        if (id == m1.hold) {                                                 /* ARP / PLAY let go */
            if (!m1.hold_used && id == 7u) {                                 /* PLAY tapped: the next */
                static const uint8_t NEXT[5] = {3, 4, 1, 2, 3};               /* BOTH > TOWN > KEYBOARD > SPLIT > BOTH */
                mc_play = NEXT[mc_play % 5u];
                mc.auto_play = 0;
                mp_show("PLAY", mc_play_name());
            } else if (!m1.hold_used) {                                      /* ARP tapped: on / off */
                mp_apply(M1T, !M1T->p[P_AMODE]);
                mp_show("ARP", M1T->p[P_AMODE] ? MP_ATYPE_N[M1P[MP_ATYPE]] : "OFF");
            }
            m1.hold = 0xFF;
        }
        return;
    }
    switch (id) {
    case SMK_BTN_KNOB_BANK: case SMK_BTN_FADER_BANK: {                       /* WAVE 1 / WAVE 2: the next notch */
        uint32_t w = id == SMK_BTN_KNOB_BANK ? 1u : 2u, p = id == SMK_BTN_KNOB_BANK ? P_E1 : P_E2;
        static const int16_t WV[4] = {0, 43, 64, 127};                       /* saw, between, triangle, square */
        uint32_t k;
        if (!(m1_waves() & w))
            break;
        if (mc.voice == MC_V_TOWN) {                                         /* the town's vending machines */
            uint8_t *x = &mc.k[6u + w];
            for (k = 0; k < 4u && WV[k] <= *x; k++)
                ;
            *x = (uint8_t)WV[k % 4u];
            m1_touch(M1T_WAVE, w, *x);
            mc_cc(84u + w, *x);
            break;
        }
        for (k = 0; k < 4u && WV[k] <= t->p[p]; k++)
            ;
        t->p[p] = WV[k % 4u];
        m1_touch(M1T_WAVE, w, t->p[p]);
        break;
    }
    case FM1_BTN_OCT_DOWN: case FM1_BTN_OCT_UP:
        song.octave = (int8_t)clamp(song.octave + (id == FM1_BTN_OCT_UP ? 1 : -1), -3, 3);
        m1_touch(M1T_BTN, id, song.octave);
        break;
    case M1_B_ARP:                                                           /* (on / off when let go) */
        m1.hold = (uint8_t)id;
        m1.hold_used = 0;
        break;
    case M1_B_CHORD:                                                         /* the A 440 */
        m1_la ^= 1u;
        mp_show("A 440", m1_la ? "ON" : "OFF");
        break;
    case 10:                                                                 /* GLOBE: the settings */
        m1.globe ^= 1u;
        m1.grow = 0xFF;
        m1.active_ms = fm1_ms;
        m1.dirty = 1;
        break;
    case 8:                                                                  /* SCALE: VOICE, the town / the keyboard */
        mc.voice ^= 1u;
        mp_show("VOICE", mc.voice == MC_V_TOWN ? "THE TOWN" : "THE KEYBOARD");
        break;
    case 7:                                                                  /* NOTE REPEAT: PLAY (a tap when let go) */
        m1.hold = (uint8_t)id;
        m1.hold_used = 0;
        break;
    case M1_B_LEFT: case M1_B_RIGHT:                                         /* the place: Modular Town / Neon Town */
        mc.place = (uint8_t)(id == M1_B_RIGHT);
        mc_cc(88, mc.place ? 127u : 0u);
        mp_show(mc.place ? "NEON TOWN" : "MODULAR TOWN", "");
        break;
    case 14:                                                                 /* FX: the vintage */
        m1.vintage ^= 1u;
        m1_vintage = m1.vintage;
        m1_touch(M1T_BTN, id, m1.vintage);
        break;
    default:
        break;
    }
}

static void m1_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), k;
    int32_t e;
    master_poll();
    midi_take();
    if (usb.config && !mc.online && fm1_in.frames > 200u) {                  /* a computer: the town may be there */
        mc.online = 1;
        if (mc.auto_play)
            mc_play = 3u;                                                    /* BOTH */
        mc_send_world();
    } else if (!usb.config && mc.online) {
        mc.online = 0;
        if (mc.auto_play)
            mc_play = 1u;                                                    /* alone: the keyboard */
    }
    {   /* the volume (K1): only to show it */
        static int32_t last = -1;
        int32_t v = master_knob / 16;
        if (last >= 0 && (v - last > 6 || last - v > 6))
            m1_touch(M1T_VOL, 0, v * 127 / 1023);
        if (last < 0 || v - last > 6 || last - v > 6)
            last = v;
    }
    while ((e = smk_padev_take()) >= 0)                                      /* pads: a sound */
        if (e & 0x7F)
            m1_sound((uint32_t)e >> 8);
    for (k = 0; k < M1_NK; k++) {                                            /* K2 .. K8 */
        int32_t s = fm1_enc_take(k + 1u);
        if (s && m1.hold == 7u && !m1.globe) {                               /* PLAY held: K2 the split */
            char b[8];
            m1.hold_used = 1;
            if (k == 0) {
                static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                mc_split = (uint8_t)clamp(mc_split + (s > 0 ? 1 : -1), 36, 84);
                mc_play = 4u;
                mc.auto_play = 0;
                str_cpy(b, NN[mc_split % 12u], 3);
                fmt_int(b + str_len(b), (int32_t)mc_split / 12 - 1);
                mp_show("SPLIT", b);
            }
        } else if (s && m1.hold != 0xFF && !m1.globe) {                     /* ARP held: choose */
            mp_turn(k, s);
        } else if (s && m1.globe) {                                                 /* GLOBE: K2 .. K7 the settings */
            m1.active_ms = fm1_ms;
            if (k < G1_N)
                g1_turn(k, s);
        } else if (s) {
            uint32_t id = M1_K_PARAM[k];
            int32_t lo = 0, hi = 127, v;
            if (s > 1 || s < -1)
                s *= 2;                                                      /* a fast turn goes further */
            if (mc.voice == MC_V_TOWN) {                                     /* the town's sound */
                v = clamp(mc.k[k] + s, lo, hi);
                mc.k[k] = (uint8_t)v;
                mc_cc(20u + k, (uint32_t)v);
            } else {
                v = clamp(M1T->p[id] + s, lo, hi);
                M1T->p[id] = (int16_t)v;
            }
            m1_touch(M1T_KNOB, k, v);
        }
    }
    for (k = 0; k < FM1_NBTN; k++) {
        if ((pressed >> k) & 1u)
            m1_btn(k, 1);
        if ((released >> k) & 1u)
            m1_btn(k, 0);
    }
    if (fm1_in.frames > 200u)
        m1_faders_apply(m1.fader[0] == 0xFF);
    m1_modwheel();
    {   /* the cat listens: the last key */
        uint32_t n = fm1_input_note_edges();
        if (n) {
            uint32_t i;
            m1.notes_ms = m1.active_ms = fm1_ms;
            for (i = 0; i < 27u; i++)
                if ((n >> i) & 1u)
                    m1.vel = fm1_in.key_vel[SMK_KEY_F3 + i];
        }
    }
}

static void m1_leds(void)
{
    uint32_t p, w = m1_waves();
    track_t *t = M1T;
    static const uint8_t LIT[4] = {12, 20, 30, 42}, DIM[4] = {4, 7, 10, 14};   /* GLOBE LUMIERE PADS, % */
    uint32_t b = M1G[M1G_PADS] % 4u;
    for (p = 0; p < 16u; p++) {                                              /* the town cream, the keyboard amber */
        uint32_t top = p < 8u, on = top ? (mc.chosen && p == mc.town) : p - 8u == mc.keys;
        uint32_t pc = on ? LIT[b] : DIM[b];
        if (on && (top ? MC_V_TOWN : MC_V_KEYS) != mc.voice)
            pc = (LIT[b] + DIM[b]) / 2u;                                     /* the other voice's: half lit */
        if (top)
            padled_set(p, (uint8_t)(255u * pc / 100u), (uint8_t)(214u * pc / 100u), (uint8_t)(160u * pc / 100u));
        else
            padled_set(p, (uint8_t)(255u * pc / 100u), (uint8_t)(140u * pc / 100u), (uint8_t)(50u * pc / 100u));
    }
    fm1_led_key(SMK_BTN_KNOB_BANK, (w & 1u) != 0);
    fm1_led_key(SMK_BTN_FADER_BANK, (w & 2u) != 0);
    fm1_led_key(FM1_BTN_OCT_DOWN, song.octave < 0);
    fm1_led_key(FM1_BTN_OCT_UP, song.octave > 0);
    fm1_led_key(6, t->p[P_AMODE] != 0);
    fm1_led_key(8, mc.voice == MC_V_TOWN);                                   /* VOICE: lit, the town */
    fm1_led_key(7, mc_play != 1u);                                           /* PLAY: lit when the town plays */
    fm1_led_key(M1_B_LEFT, !mc.place);
    fm1_led_key(M1_B_RIGHT, mc.place != 0);
    fm1_led_key(9, m1_la);
    fm1_led_key(10, m1.globe);
    fm1_led_key(14, m1.vintage);
    fm1_led_key(M1_B_PLAY, 0);
    fm1_led_key(M1_B_REC, 0);
}
