/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* MODULAR LANDSCAPE (MC_ARCHET): what sounds, in the audio context (included by seq.c, before ar_tape.c).
 *   C2 .. G4   the bow (the set ARCHET: D2 .. E4 as played)   part 1 the keys, part 4 the tape (the sketches)
 *   (26: the bass under it is gone, the keys a whole octave higher; G#4 .. C5 are silent, a sketch's note under C2,
 *   kept by an older version's bass, plays an octave higher)
 * The tape's notes play on their own parts, so that a note of the keys never cuts the same note of a sketch.
 * Every note that sounds goes into the sketchbook's ring (sk_put: a sketch keeps what you hear, the bass too) and,
 * the tape's, out to MIDI when the keys' notes go there (mc_dest; the keys' own: key_on sends them). */
#define AR_LO 36u                                /* the bow: C2 .. C5 (42: the keyboard to its top; was G4) */
#define AR_HI 72u
enum { TK_KEYS, TK_BASS, TK_TBASS, TK_TAPE };
static void sk_put(uint32_t note, uint32_t vel);         /* ar_tape.c */

static uint8_t ar_on[4][128];
static int ar_tk_ok(uint32_t tk, uint32_t note)
{
    return (tk == TK_KEYS || tk == TK_TAPE) && note >= AR_LO && note <= AR_HI;   /* (TK_BASS, TK_TBASS: unused) */
}
/* 50: CONTRA (pad 11, archet.c): the lowest key held doubled an octave under, on part 3 (the bow's own strokes), as
 * double basses under the bass line; it follows the bass as it moves. One voice in all */
static volatile uint8_t ar_contra, ar_contra_note;          /* ar_contra_note: the key doubled, 0 none */
static void ar_contra_update(void)
{
    uint32_t n, low = 0;
    if (ar_contra)
        for (n = AR_LO; n <= AR_HI && !low; n++)
            if (ar_on[TK_KEYS][n])
                low = n;
    if (low == ar_contra_note)
        return;
    if (ar_contra_note)
        trk_note_off(&trk[TK_TBASS], ar_contra_note - 12u);
    ar_contra_note = (uint8_t)low;
    if (low)
        trk_note_on(&trk[TK_TBASS], low - 12u, 90);
}
static void ar_snd_on(uint32_t tk, uint32_t note, uint32_t vel)
{
    if (!ar_tk_ok(tk, note))
        return;
    if (!vel)
        vel = 1;
    if (mc_dest(note) & 1u) {
        if (ar_on[tk][note])
            trk_note_off(&trk[tk], note);               /* (voice.c: the old one fades, the new one starts) */
        trk_note_on(&trk[tk], note, vel);
    }
    ar_on[tk][note] = 1;
    if ((tk == TK_TAPE || tk == TK_TBASS) && (mc_dest(note) & 2u))
        midi_out_event(0x09u | (0x90u | trk_midi_ch(0)) << 8 | note << 16 | (vel & 127u) << 24);
    sk_put(note, vel);
    ar_ev_put(note, vel);
    if (tk == TK_KEYS)
        ar_contra_update();
}
static void ar_snd_off(uint32_t tk, uint32_t note)
{
    if (!ar_tk_ok(tk, note) || !ar_on[tk][note])
        return;
    ar_on[tk][note] = 0;
    trk_note_off(&trk[tk], note);
    if ((tk == TK_TAPE || tk == TK_TBASS) && (mc_dest(note) & 2u))
        midi_out_event(0x08u | (0x80u | trk_midi_ch(0)) << 8 | note << 16);
    sk_put(note, 0);
    ar_ev_put(note, 0);
    if (tk == TK_KEYS)
        ar_contra_update();
}

/* the keys (seq.c input_on / input_off, part 1 and no arp; MIDI in too) */
static volatile uint8_t ar_last_note = 50u;              /* (37: the last key played, for the PEDALE: archet.c) */
static void ar_layer_on(uint32_t note, uint32_t vel)
{
    note &= 127u;
    if (note >= AR_LO && note <= AR_HI)
        ar_last_note = (uint8_t)note;
    ar_snd_on(TK_KEYS, note, vel);                          /* (outside C2 .. G4: nothing) */
}
static void ar_layer_off(uint32_t note)                   /* (input_off has let part 1 go) */
{
    note &= 127u;
    ar_snd_off(TK_KEYS, note);
}
