/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* MODULAR LANDSCAPE (MC_ARCHET): the sketchbook in flash, Modular One's (the FM-1's m1_book.c, the same format: a
 * sketch = a 32-byte header at the start of a sector, its name (12 bytes), its notes (ar_tape.c's piece format); it
 * takes the sectors it needs; they follow each other, the first sector without a valid header is the free space).
 * Where: the SMK-37's second user sample slot, USR2 (0xD6000 .. 0xE9FFF, 20 sectors; this build loads no user
 * sample), through storage_hw.c (st_erase / st_prog: FL_SMP only, the stock mapping's marker released before the
 * first erase there). No songs from the computer here.
 * The pads: a sketch's number (its header's num) is its pad, 1 .. 16 (the bottom row 1 .. 8, the top 9 .. 16); the
 * list is in the pads' order.
 * A pad touched records from the first note after it to the touch that stops it (sk_extract_from); SEQ REC held
 * keeps the last phrase on the first free pad. Everything runs in the main loop (an erase stops the audio for
 * its time, as a save does). Included by archet.c. */
#define SK_BASE (SMP_USER_BASE + SMP_USER_SIZE)
#define SK_NSECT (SMP_USER_SIZE / 0x1000u)
#define SK_MAGIC 0x314B5346u                       /* "FSK1" */
#define SK_OUT (SK_EV * 6u)
_Static_assert(SK_BASE >= FL_SMP_LO && SK_BASE + SK_NSECT * 0x1000u <= FL_SMP_HI, "the sketches: in USRFLASH");

typedef struct {
    uint32_t magic;
    uint16_t nsect, num;                           /* sectors taken; its number when kept */
    uint32_t notes, len, dur10, crc;               /* len: bytes after the header; CRC-32 of them */
    uint32_t flags, rsv;                           /* flags SK_NAMED: the body starts with the name */
} sk_hdr_t;                                        /* 32 B */
#define SK_NAMED 0xFFFFFFFEu
#define SK_NAME 12u                                /* the name: 12 bytes, 0-padded (all 0: "SKETCH n") */
typedef char ar_sk_size_check[sizeof(sk_hdr_t) == 32u ? 1 : -1];

static struct {
    uint8_t scan;                                  /* the list is to be rebuilt (when nothing plays) */
    uint8_t nsk, free_sect;
    uint8_t last_sect, last_ok;                    /* the last kept sketch: overwritten by a deeper keep */
    uint16_t next_num;
    uint32_t last_w, last_ms, depth;
} m1b;
/* the sketch being written: in the screen's canvas (gfx.c cv_px), free while the main loop is here (lcd_sync first:
 * the last blit may still read it); the screen is drawn whole again after */
#define sk_out ((uint8_t *)cv_px)
typedef char ar_skout_fits[SK_OUT + 256u <= sizeof cv_px ? 1 : -1];

#define AR_NPAD 16u
static uint8_t pad_item[AR_NPAD];                  /* pad -> its list item, 0xFF: empty */
static uint8_t pl_sk[PL_MAX];                      /* list item -> its place among the sketches in flash */
static volatile uint8_t ar_pad_keep_req, ar_pad_del_req;   /* pad + 1 (archet.c: a pad's recording stopped, erased) */
static volatile uint32_t ar_pad_i0;                /* .. where the ring was when the pad was touched */
static const uint8_t *m1_xip(uint32_t off) { return fm1_xip_ptr(off); }
static void m1_inval(uint32_t off, uint32_t n)
{
    fm1_irq_off();
    fl_inval(off, n);
    fm1_irq_on();
}

static int m1_sk_valid(uint32_t s, const sk_hdr_t **hp)
{
    const sk_hdr_t *h = (const sk_hdr_t *)m1_xip(SK_BASE + s * 0x1000u);
    *hp = h;
    if (h->magic != SK_MAGIC || !h->nsect || s + h->nsect > SK_NSECT || !h->notes ||
        h->len > h->nsect * 0x1000u - sizeof(sk_hdr_t) || (h->flags == SK_NAMED && h->len < SK_NAME))
        return 0;
    return st_crc32((const uint8_t *)h + sizeof(sk_hdr_t), h->len) == h->crc;
}

static void m1_name(char *d, const char *s, uint32_t n)   /* printable ASCII, upper case */
{
    uint32_t i, k = 0;
    for (i = 0; i < n && s[i] && k < PL_NAME - 1u; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32);
        d[k++] = c >= 32 && c < 127 ? c : ' ';
    }
    d[k] = 0;
}

/* a sketch's notes; its name (if one was given on the FM-1's Carnet app), else "CROQUIS n" */
static const uint8_t *m1_sk_notes(const sk_hdr_t *h)
{
    return (const uint8_t *)h + sizeof(sk_hdr_t) + (h->flags == SK_NAMED ? SK_NAME : 0u);
}
static void m1_sk_name(const sk_hdr_t *h, char *b)
{
    uint32_t k = 7, v = h->num;
    if (h->flags == SK_NAMED) {
        m1_name(b, (const char *)h + sizeof(sk_hdr_t), SK_NAME);
        if (b[0] && !(b[0] == ' ' && !b[1]))
            return;
    }
    str_cpy(b, "SKETCH ", PL_NAME);
    if (v >= 10u)
        b[k++] = (char)('0' + v / 10u % 10u);
    b[k++] = (char)('0' + v % 10u);
    b[k] = 0;
}

/* rebuild the player's list: the pads' sketches, pad 1 first */
static void m1_rescan(void)
{
    uint32_t n = 0, s, j = 0, p;
    static uint16_t sect[SK_NSECT], num[SK_NSECT];
    pl_songs = 0;
    m1b.nsk = 0;
    for (s = 0; flash_ok && s < SK_NSECT;) {
        const sk_hdr_t *h;
        if (!m1_sk_valid(s, &h))
            break;
        sect[j] = (uint16_t)s;
        num[j] = h->num;
        j++;
        m1b.nsk++;
        s += h->nsect;
    }
    m1b.free_sect = (uint8_t)s;
    for (p = 0; p < AR_NPAD; p++) {
        uint32_t k;
        pad_item[p] = 0xFF;
        for (k = 0; k < j; k++)
            if (num[k] == p + 1u) {
                const sk_hdr_t *h = (const sk_hdr_t *)m1_xip(SK_BASE + sect[k] * 0x1000u);
                pl_item[n].data = m1_sk_notes(h);
                pl_item[n].n = h->notes;
                pl_item[n].dur10 = h->dur10;
                m1_sk_name(h, pl_item[n].name);
                pl_sk[n] = (uint8_t)k;
                pad_item[p] = (uint8_t)n;
                n++;
                break;
            }
    }
    pl_count = n;
}
static int m1_pad_free(void)                       /* the first empty pad, -1 none */
{
    uint32_t p;
    for (p = 0; p < AR_NPAD; p++)
        if (pad_item[p] == 0xFFu)
            return (int)p;
    return -1;
}

/* the player's list may only change while nothing plays */
static int m1_quiet(void) { return !demo_on && !demo_req; }

/* write a sketch at sector s (its sectors are erased first; the header goes last) */
static int m1_sk_write(uint32_t s, uint32_t num, uint32_t flags, const uint8_t *data, uint32_t len, uint32_t notes,
                       uint32_t dur10)
{
    static sk_hdr_t h;
    static uint8_t page[256] __attribute__((aligned(4)));
    uint32_t ns = (sizeof h + len + 0xFFFu) / 0x1000u, i, off;
    if (s + ns > SK_NSECT)
        return 1;
    for (i = 0; i < ns; i++) {
        if (st_erase(SK_BASE + (s + i) * 0x1000u))
            return 2;
        fm1_wdt_feed();
    }
    for (off = 0; off < sizeof h + len; off += 256u) {   /* the header's bytes stay 0xFF for now */
        uint32_t k;
        for (k = 0; k < 256u; k++) {
            uint32_t p = off + k;
            page[k] = p < sizeof h || p >= sizeof h + len ? 0xFFu : data[p - sizeof h];
        }
        if (st_prog(SK_BASE + s * 0x1000u + off, page, 256u))
            return 3;
    }
    h.magic = SK_MAGIC;
    h.nsect = (uint16_t)ns;
    h.num = (uint16_t)num;
    h.notes = notes;
    h.len = len;
    h.dur10 = dur10;
    h.crc = st_crc32(data, len);
    h.flags = flags;
    h.rsv = 0xFFFFFFFFu;
    if (st_prog(SK_BASE + s * 0x1000u, (const uint8_t *)&h, sizeof h))
        return 3;
    m1_inval(SK_BASE + s * 0x1000u, ns * 0x1000u);
    return 0;
}

static void m1_say_kept(uint32_t num, uint32_t dur10)
{
    static char lab[16] = "PAD ";   /* (num 1 .. 16) */
    char b[16];
    uint32_t k = 4, sec = dur10 / 100u;
    if (num >= 10u)
        lab[k++] = (char)('0' + num / 10u % 10u);
    lab[k++] = (char)('0' + num % 10u);
    lab[k] = 0;
    k = 0;
    if (sec >= 600u)
        b[k++] = (char)('0' + sec / 600u % 10u);
    b[k++] = (char)('0' + sec / 60u % 10u);
    b[k++] = ':';
    b[k++] = (char)('0' + sec % 60u / 10u);
    b[k++] = (char)('0' + sec % 10u);
    str_cpy(b + k, " KEPT", 6);
    m1_say(lab, b);
}

/* REC held: keep the last phrase (again right away: one phrase further back) */
static void m1_keep(void)
{
    lcd_sync();
    uint32_t w = sk_w, deeper, depth, len, notes, dur10, got, s, num, rc;
    if (!flash_ok) {
        m1_say("SKETCHBOOK", "NO FLASH");
        return;
    }
    deeper = m1b.last_ok && w == m1b.last_w && fm1_ms - m1b.last_ms < 20000u;
    depth = deeper ? m1b.depth + 1u : 1u;
    len = sk_extract(depth, sk_out + SK_NAME, SK_OUT, &notes, &dur10, &got);   /* after an empty name */
    if (!len) {
        m1_say("SKETCHBOOK", "NOTHING YET");
        return;
    }
    if (deeper && got < depth) {                     /* no older phrase: that one stays as it is */
        m1_say("SKETCHBOOK", "NO EARLIER");
        m1b.last_ms = fm1_ms;
        return;
    }
    if (demo_on || demo_req) {                       /* the list changes under the player */
        demo_req = 2;
        while (demo_on)
            fm1_wdt_feed();
    }
    if (deeper) {
        s = m1b.last_sect;
        num = m1b.next_num;                          /* (the pad it went to) */
    } else {
        int p = m1_pad_free();
        if (p < 0) {
            m1_say("SKETCHBOOK", "PADS FULL");
            m1b.last_ok = 0;
            return;
        }
        s = m1b.free_sect;
        num = (uint32_t)p + 1u;
        m1b.next_num = (uint16_t)num;
    }
    memset(sk_out, 0, SK_NAME);
    rc = m1_sk_write(s, num, SK_NAMED, sk_out, SK_NAME + len, notes, dur10);
    m1_rescan();
    if (rc == 1u) {
        m1_say("SKETCHBOOK", "FULL");
        m1b.last_ok = 0;
        return;
    }
    if (rc) {
        m1_say("SKETCHBOOK", "FLASH ERROR");
        m1b.last_ok = 0;
        return;
    }
    m1b.last_ok = 1;
    m1b.last_sect = (uint8_t)s;
    m1b.last_w = w;
    m1b.last_ms = fm1_ms;
    m1b.depth = got;
    m1_say_kept(num, dur10);
}

/* erase the sketch j (0 = the first one): the ones after it move down, the sectors freed at the end are
 * erased (after the last sketch the flash stays blank: the scan stops there) */
static int m1_sk_erase(uint32_t j)
{
    lcd_sync();
    const sk_hdr_t *h;
    uint32_t s = 0, i, dst;
    for (i = 0; i < j; i++) {
        if (!m1_sk_valid(s, &h))
            return 1;
        s += h->nsect;
    }
    if (!m1_sk_valid(s, &h))
        return 1;
    dst = s;
    s += h->nsect;
    while (s < SK_NSECT && m1_sk_valid(s, &h)) {
        uint32_t ns = h->nsect, num = h->num, notes = h->notes, len = h->len, dur10 = h->dur10, fl = h->flags;
        if (len > SK_OUT + 256u)
            return 2;
        memcpy(sk_out, (const uint8_t *)h + sizeof(sk_hdr_t), len);   /* (the copy may land on itself) */
        if (m1_sk_write(dst, num, fl, sk_out, len, notes, dur10))
            return 3;
        dst += (sizeof(sk_hdr_t) + len + 0xFFFu) / 0x1000u;
        s += ns;
    }
    for (i = dst; i < s; i++) {
        if (st_erase(SK_BASE + i * 0x1000u))
            return 3;
        fm1_wdt_feed();
    }
    m1_inval(SK_BASE + dst * 0x1000u, (s - dst) * 0x1000u);
    m1b.last_ok = 0;
    return 0;
}

/* the list's item k erased (a sketch; ui_input.c, REC held twice in the list): the player stops,
 * the list is rebuilt, the player goes on from the same place */
static void m1_del(uint32_t k)
{
    char b[PL_NAME + 8];
    int was = demo_on;
    if (!flash_ok || k < pl_songs || k >= pl_count)
        return;
    str_cpy(b, pl_item[k].name, PL_NAME);
    if (demo_on || demo_req) {
        demo_req = 2;
        while (demo_on)
            fm1_wdt_feed();
    }
    if (m1_sk_erase(pl_sk[k])) {
        m1_rescan();
        m1_say("SKETCHBOOK", "FLASH ERROR");
        return;
    }
    m1_rescan();
    m1_say(b, "ERASED");
    if (was && pl_count) {
        pl_start = (uint8_t)(k < pl_count ? k : 0u);
        pl_cursor = pl_start;
        demo_req = 1;
    }
}


static void m1_stop_tape(void)
{
    demo_req = 2;
    while (demo_on)
        fm1_wdt_feed();
}

static void m1_boot(void)
{
    m1_rescan();
}

/* a pad's recording stopped: what was played since the pad was touched, kept on the pad */
static void m1_keep_pad(uint32_t pad, uint32_t i0)
{
    lcd_sync();
    uint32_t len, notes, dur10, rc;
    char lab[8] = "PAD ";
    if (pad >= 9u) {
        lab[4] = '1';
        lab[5] = (char)('0' + pad - 9u);
        lab[6] = 0;
    } else {
        lab[4] = (char)('1' + pad);
        lab[5] = 0;
    }
    if (!flash_ok) {
        m1_say("SKETCHBOOK", "NO FLASH");
        return;
    }
    len = sk_extract_from(i0, sk_out + SK_NAME, SK_OUT, &notes, &dur10);
    if (!len) {
        m1_say(lab, "NOTHING PLAYED");
        return;
    }
    if (demo_on || demo_req) {
        demo_req = 2;
        while (demo_on)
            fm1_wdt_feed();
    }
    if (pad_item[pad] != 0xFFu && m1_sk_erase(pl_sk[pad_item[pad]])) {   /* (a pad holds one sketch) */
        m1_rescan();
        m1_say("SKETCHBOOK", "FLASH ERROR");
        return;
    }
    m1_rescan();
    memset(sk_out, 0, SK_NAME);
    rc = m1_sk_write(m1b.free_sect, pad + 1u, SK_NAMED, sk_out, SK_NAME + len, notes, dur10);
    m1_rescan();
    m1b.last_ok = 0;
    if (rc) {
        m1_say("SKETCHBOOK", rc == 1u ? "FULL" : "FLASH ERROR");
        return;
    }
    m1_say_kept(pad + 1u, dur10);
}

/* main loop (archet.c ar_input) */
static void m1_service(void)
{
    if (m1_del_req) {
        uint32_t k = m1_del_req - 1u;
        m1_del_req = 0;
        m1_del(k);
    }
    if (m1_keep_req) {
        m1_keep_req = 0;
        m1_keep();
    }
    if (ar_pad_keep_req) {
        uint32_t p = ar_pad_keep_req - 1u;
        ar_pad_keep_req = 0;
        m1_keep_pad(p, ar_pad_i0);
    }
    if (ar_pad_del_req) {
        uint32_t p = ar_pad_del_req - 1u;
        ar_pad_del_req = 0;
        if (p < AR_NPAD && pad_item[p] != 0xFFu)
            m1_del(pad_item[p]);
    }
    if (m1b.scan && m1_quiet()) {
        m1b.scan = 0;
        m1_rescan();
    }
}
