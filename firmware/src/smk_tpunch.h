/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37: the rhythmic punch-ins on the stems (HOME menu > USB > STEM FX = PUNCH or PUNCH HQ, in a stem layout).
 * On the master (punch.c) a punch-in replays a mono ring of the whole mix, so the stems, taken before the master,
 * never had it. With STEM FX on, the rhythmic punch-ins (tp_fx: the loops, STUTTER, REVERSE, STOP, HALF, REWIND,
 * GATE) act on each stem's own signal instead, where fx.c takes the stem (a synth track after its LEVEL and mute,
 * before its pan and sends; the drum groups after their chain): the stems, the stereo mix and the headphones hear the
 * same, and the reverb hears the punched signal. The tonal ones (the filters, kills, CRUSH, ECHO, FREEZE ...) stay
 * on the master (punch.c), which never sees the rhythmic ones then.
 * Memory: the master ring's 64 KB and the SLICER's STUT recordings' 32 KB (slicer.c smk_fxpool, one block): the
 * master keeps TP_MRING samples at its end (FLANGER, OCT UP, WOBBLE; ECHO short), the rest is shared out into one ring
 * a stem (SYNTHS+RET 4: the synths, the kit; DRUMS+RET 7: the synths, the 4 groups; ALL DRY 6: the synths, KICK,
 * SNARE, HATS + PERC), so SLICER STUT is off while STEM FX is on. PUNCH stores at 22.05 kHz (a pair's mean, as the
 * SLICER's STUT): twice the time; PUNCH HQ at 44.1 kHz. Every ring writes in step, so one timeline serves them all:
 * tp_block (core 0, before the units) works out each sample's write slot, read position, effect gain and crossfade
 * for the block, the ring index wrapped there once; tp_x applies them to one ring's sample (either core: a ring is
 * its stem's alone). A span longer than a ring (a slow tempo) is cut to what it holds; STOP and REWIND fade where the
 * past runs out, as REWIND does on the master */
#define TP_MRING 4096u                  /* the master's ring with STEM FX on (punch.c punch_cap): 93 ms */
#define TP_POOL (NTRK * SL_LEN + PUNCH_N)
#define TP_MAXR 7u
enum { SFX_SLICER, SFX_PUNCH, SFX_HQ, SFX_N };
static uint8_t stem_fx;                 /* the menu's STEM FX (project.c smkset); live */
/* HOME > PUNCH > XFADE, CURVE (project.c smkset; live): a stitch on the stems (a punch in or out between unrelated
 * audio) takes at least XFADE (FADE may make it longer); EQUAL keeps the level through it (unrelated audio summed
 * linearly dips ~3 dB half way), LINEAR is the old ramp. A stitch between the same audio (a fresh loop's punch-in:
 * it plays what comes in) stays linear either way (equal power there is a 3 dB bump) */
enum { TPX_128, TPX_64, TPX_256, TPX_N };   /* (0: the default, 2.9 ms) */
static uint8_t tp_xfl;                  /* XFADE: TPX_* */
static uint8_t tp_lin;                  /* CURVE: 0 EQUAL, 1 LINEAR */
static uint8_t tp_on;                   /* this start's layout and STEM FX: the stems' punch-ins (tp_mode) */
static uint8_t tp_live;                 /* this block: one of them plays or fades */
static struct {
    uint8_t mode, n, s;                 /* the mode set up (0xFF: none yet); rings; 1 (HQ) or 2 (PUNCH) */
    uint32_t cap;                       /* a ring's samples (storage rate) */
    uint32_t ws;                        /* the write counter, every ring (storage samples) */
    uint32_t t;                         /* output samples (the storage pairs' phase) */
    int8_t cur;                         /* the effect playing (-1 none) */
    int32_t g;                          /* its wet share, Q15 */
    uint32_t start, len, k, pre;        /* loops, REVERSE, HALF: the span's start (storage counter) and length
                                         * (output samples), the place in it (output samples), dry samples to wait */
    int64_t rp;                         /* STOP, REWIND: the read position, storage samples Q16 */
    uint32_t spd, dspd;                 /* STOP: the speed (Q16, output samples a sample), its step */
    int32_t v;                          /* REWIND: the speed Q16 (signed) */
    uint32_t gp;                        /* GATE: samples into the 1/16 step */
    uint32_t own_lo, own_n;             /* the captured span the writer leaves when it comes round (storage), 0: none */
    uint8_t eq;                         /* this one's stitches equal power (CURVE, and unrelated audio) */
} tp = {.mode = 0xFFu, .cur = -1};
static int16_t *tp_ring[TP_MAXR];
static int32_t tp_half[TP_MAXR];        /* PUNCH: the first sample of a pair */
/* the block's timeline, per output sample (core 0 writes it before the units, the units read it) */
static uint8_t tp_wr[CTL];              /* 1: a storage sample is written (PUNCH: the pair's second), 3: its slot kept */
static uint16_t tp_wi[CTL];             /* .. its slot */
static uint8_t tp_md[CTL];              /* 0: dry, 1: the ring at tp_ia / tp_fr x tp_wg, 2: the signal x tp_wg (GATE),
                                         * 3: a loop's seam, the ring at tp_ia x tp_wg + at tp_ic x tp_gb */
static uint16_t tp_ia[CTL], tp_ib[CTL]; /* the ring slots read, either side of the position */
static int32_t tp_fr[CTL];              /* the position between them, Q15 */
static int32_t tp_wg[CTL], tp_xg[CTL];  /* the effect's gain, Q15; the crossfade (wet share), Q15 */
static int32_t tp_xd[CTL];              /* .. and the dry share, Q15 (LINEAR: 32767 - tp_xg; EQUAL: the cosine) */
static uint16_t tp_ic[CTL], tp_id[CTL]; /* the seam: the second position's slots .. */
static int32_t tp_fr2[CTL], tp_gb[CTL]; /* .. between them, Q15, and its gain, Q15 */
/* a loop's seam: over its last TP_SEAM samples (a quarter of a short loop) each pass blends, equal power, into the
 * audio a loop's length before (forwards: just before the span's start; REVERSE: just after its turn), so the wrap
 * lands on audio that runs on: no gap and no level dip (a fade to silence each pass clicked: the owner, rapid
 * punches catching a sound's edge); the ring keeps those samples while the effect plays (own_lo, own_n) */
#define TP_SEAM 128u
static uint32_t tp_seam(uint32_t len);
_Static_assert(TP_POOL - TP_MRING <= 4u * 65536u, "a ring's slots fit tp_wi (16 bits) for every ring count");

/* the stem punch-in page (FX held, STEM FX = PUNCH / HQ in a stem layout): the effects that play on the stems, three
 * rows of four tiles and pads, coloured by kind: the loops orange, time and tape cyan, rhythm green, space violet. A
 * pad picks its effect (seq.c pfx_pad: as the effect's key, its MIDI note on FX CH too); the keys stay as they are */
#define TP_PAGE_N 12u
static const uint8_t TP_PAGE[TP_PAGE_N] = {PX_LOOP4, PX_LOOP8, PX_LOOP16, PX_LOOP32, PX_STUT, PX_REV, PX_STOP, PX_HALF,
                                           PX_LOOP8T, PX_GATE, PX_RWD, PX_FREEZE};
enum { TPK_LOOP, TPK_TAPE, TPK_RHYTHM, TPK_SPACE, TPK_SEQ, TPK_N };   /* (TPK_SEQ: row 4, smk_seqfx.h) */
static const uint8_t TP_PAGE_K[TP_PAGE_N] = {TPK_LOOP, TPK_LOOP, TPK_LOOP, TPK_LOOP, TPK_TAPE, TPK_TAPE, TPK_TAPE,
                                             TPK_TAPE, TPK_LOOP, TPK_RHYTHM, TPK_TAPE, TPK_SPACE};
static const uint8_t TP_KIND_RGB[TPK_N][3] = {{255, 110, 0}, {0, 190, 255}, {40, 220, 60}, {190, 60, 255},
                                              {255, 220, 40}};
static int tp_page_on(void) { return tp_on != 0u; }    /* (the UI: this page instead of the 27 keys) */

static int tp_fx(int32_t fx)            /* the rhythmic ones: on the stems with STEM FX */
{
    return (fx >= PX_LOOP4 && fx <= PX_HALF) || fx == PX_GATE || fx == PX_RWD || fx == PX_LOOP8T;
}

/* the memory shared out for this mode (core 0, at a block's start: nothing reads it then) */
static void tp_setup(uint32_t mode, uint32_t layout)
{
    uint32_t i, n = layout == 2u ? 4u : layout == 1u ? 7u : 6u;   /* (usb.c UL_*: 2 SYNTHS+RET, 1 DRUMS+RET, 3 ALL DRY) */
    memset(smk_fxpool, 0, sizeof smk_fxpool);
    for (i = 0; i < NTRK; i++)                          /* the SLICERs: no STUT recording or repeat left */
        sl[i].loop = 0, sl[i].rec_on = 0, sl[i].rec = 0, sl[i].w = 0;
    punch.cur = -1, punch.g = 0, punch.w = 0;
    tp.cur = -1, tp.g = 0, tp.own_n = 0, tp.t = 0;
    tp.ws = TP_POOL;                                    /* (a whole ring ahead of 0: a span behind it never wraps
                                                         * the counter, so idx % cap stays continuous) */
    if (mode == SFX_SLICER || !layout) {                /* (0: STEREO) */    /* as SLOOP: the master's whole ring, the SLICER's STUT */
        punch_ring = smk_fxpool + NTRK * SL_LEN, punch_cap = PUNCH_N;
        tp.n = 0, tp_on = 0, sl_mem_lent = 0;
        return;
    }
    punch_ring = smk_fxpool + TP_POOL - TP_MRING, punch_cap = TP_MRING;
    tp.n = (uint8_t)n;
    tp.s = mode == SFX_HQ ? 1u : 2u;
    tp.cap = (TP_POOL - TP_MRING) / n;
    for (i = 0; i < n; i++)
        tp_ring[i] = smk_fxpool + i * tp.cap, tp_half[i] = 0;
    tp_on = 1, sl_mem_lent = 1;
}

static void tp_start(int32_t fx)
{
    uint32_t beat = beat_samples(), ph = clk_samples(), len, maxl = (tp.cap - 256u) * tp.s;
    static const uint8_t DIV[8] = {1, 2, 4, 8, 6, 1, 1, 2};   /* as punch.c: loops beat / DIV, REVERSE a beat, HALF 1/8 */
    tp.cur = (int8_t)fx;
    len = fx <= PX_HALF ? beat / DIV[fx] : fx == PX_LOOP8T ? beat / 3u : beat;
    if (len > maxl)
        len = maxl;                                     /* (a slow tempo: what the ring holds) */
    if (len < 64u)
        len = 64u;
    tp.len = len;
    tp.pre = 0;
    tp.k = 0;
    tp.start = tp.ws;
    if (px_loop(fx) && song.playing) {                  /* on the grid, as punch.c */
        uint32_t back = ph % len;
        if (len - back <= 1536u) {
            tp.pre = len - back;
        } else {
            tp.start = tp.ws - back / tp.s;
            tp.k = back;
        }
    }
    tp.own_n = 0;
    if (tp.s == 2u)
        tp.start--;                                     /* PUNCH: a slot behind the writer (it fills a slot on a pair's
                                                         * second sample: the first pass read the one not yet written,
                                                         * a ring old, half of every sample) */
    if (fx == PX_REV) {
        uint32_t back = len / tp.s + 1u, after = tp_seam(len) / tp.s + 2u;   /* (the storage samples it reads, the
                                                         * interpolation's too; the seam's, after the turn) */
        tp.start = tp.ws - 1u;                          /* (backwards from the last whole sample) */
        tp.own_lo = tp.start - back, tp.own_n = back + 1u + after;   /* already captured: the writer leaves it from now
                                                         * (it came round over the span's oldest part: the pops); the
                                                         * seam's samples after the turn as they come */
    }
    tp.eq = (uint8_t)(!tp_lin && (fx == PX_REV || fx == PX_HALF || (px_loop(fx) && tp.k != 0u)));   /* (a fresh loop,
                                                         * STOP, REWIND start on what comes in: the same audio) */
    tp.rp = (int64_t)(tp.ws - 1u) << 16;
    tp.spd = 65536u;
    tp.dspd = 65536u / len + 1u;
    tp.v = 65536;
    tp.gp = song.playing ? clk_samples() % (beat / 4u ? beat / 4u : 1u) : 0u;
}

static void tp_at(uint32_t i, uint32_t idx, int32_t fr)   /* sample i reads between storage counters idx, idx + 1 */
{
    uint32_t a = idx % tp.cap;
    tp_ia[i] = (uint16_t)a;
    tp_ib[i] = (uint16_t)(a + 1u >= tp.cap ? 0u : a + 1u);
    tp_fr[i] = fr;
}
static void tp_at2(uint32_t i, uint32_t idx, int32_t fr) /* (the seam's second position) */
{
    uint32_t a = idx % tp.cap;
    tp_ic[i] = (uint16_t)a;
    tp_id[i] = (uint16_t)(a + 1u >= tp.cap ? 0u : a + 1u);
    tp_fr2[i] = fr;
}
/* a loop's read position x output samples into its span (x < 0: before it), forwards or (REVERSE) backwards from
 * tp.start: the storage counter and the fraction (PUNCH: a slot two output samples; between, half way) */
static uint32_t tp_pos(int32_t x, int rev, int32_t *fr)
{
    int32_t y = rev ? -x : x;
    if (tp.s == 1u) {
        *fr = 0;
        return tp.start + (uint32_t)y;
    }
    *fr = (y & 1) ? 16384 : 0;
    return tp.start + (uint32_t)((y - (y & 1)) / 2);
}
static uint32_t tp_seam(uint32_t len)   /* a loop's seam: TP_SEAM, a quarter of a short loop */
{
    return len / 4u < TP_SEAM ? len / 4u : TP_SEAM;
}

/* the block's timeline (core 0, before the units); want: the punch-in asked for (punch.req) */
static void tp_block(uint32_t n, int32_t want)
{
    uint32_t i, beat = beat_samples(), step = beat / 4u ? beat / 4u : 1u;
    int32_t pstep;
    {   /* the fade in and out (GLO > MASTER 2 > FADE), as punch.c */
        int32_t f = (int32_t)song.g[G_FXFADE];
        pstep = 512 / (1 + (f < 0 ? 0 : f > 15 ? 15 : f) * 17);
        if (pstep < 1)
            pstep = 1;
        f = tp_xfl == TPX_64 ? 512 : tp_xfl == TPX_256 ? 128 : 256;   /* (XFADE: the shortest stitch) */
        if (pstep > f)
            pstep = f;
    }
    if (!tp_fx(want))
        want = -1;
    if (tp.cur < 0 && want >= 0)
        tp_start(want);
    tp_live = 0;
    for (i = 0; i < n; i++) {
        int32_t fx = tp.cur, wg = 32767;
        uint32_t target = want == fx && fx >= 0 ? 32767u : 0u, last = tp.s == 1u || (tp.t & 1u);
        uint8_t md = 0;
        if (last) {
            uint32_t d = tp.ws - tp.own_lo;
            tp_wr[i] = (uint8_t)(tp.own_n && d >= tp.own_n && d % tp.cap < tp.own_n ? 3u : 1u);   /* (come round to
                                                         * the captured span: leave it) */
            tp_wi[i] = (uint16_t)(tp.ws % tp.cap);
        } else {
            tp_wr[i] = 0;
        }
        if (fx >= 0) {
            if (tp.pre) {                               /* waiting for the division: dry */
                if (!--tp.pre)
                    tp.start = tp.ws + 3u - 2u * tp.s, tp.k = 0;   /* (PUNCH: two slots behind: either
                                                         * parity, the slots read are written; see tp_start) */
                tp.own_n = 0;                           /* (captured from the division on) */
            } else if (fx == PX_GATE) {
                uint32_t p = tp.gp;
                wg = p < step / 2u ? 32767 : 0;
                if (p < 64u)
                    wg = (int32_t)p * 512;
                else if (p >= step / 2u && p < step / 2u + 64u)
                    wg = 32767 - (int32_t)(p - step / 2u) * 512;
                tp.gp = p + 1u >= step ? 0u : p + 1u;
                md = 2;
            } else if (fx == PX_STOP || fx == PX_RWD) {
                tp_at(i, (uint32_t)(tp.rp >> 16), (int32_t)((tp.rp >> 1) & 0x7FFF));
                if (fx == PX_STOP) {
                    wg = (int32_t)(tp.spd >> 1);
                    tp.rp += tp.spd / tp.s;
                    tp.spd = tp.spd > tp.dspd ? tp.spd - tp.dspd : 0u;
                } else {
                    int32_t room = (int32_t)tp.cap - 256 - (int32_t)(tp.ws - (uint32_t)(tp.rp >> 16));
                    wg = room <= 0 ? 0 : room < 1024 ? room * 32 : 32767;
                    tp.rp += tp.v / (int32_t)tp.s;
                    if (tp.v > -PX_RWD_MAX)
                        tp.v -= PX_RWD_ACC;
                }
                md = 1;
            } else {                                    /* loops, STUTTER, REVERSE, HALF: kk samples into the span */
                uint32_t k = tp.k, len = tp.len, kk = fx == PX_HALF ? k / 2u : k, sn = tp_seam(len);
                int rev = fx == PX_REV;
                int32_t fr;
                uint32_t idx = tp_pos((int32_t)kk, rev, &fr);
                tp_at(i, idx, fr);
                md = 1;
                if (kk + sn >= len) {                   /* the seam: into the audio a loop's length on */
                    uint32_t ph = (uint32_t)(((uint64_t)(kk + sn - len) << 30) / sn);   /* 0 .. a quarter turn */
                    idx = tp_pos((int32_t)kk - (int32_t)len, rev, &fr);
                    tp_at2(i, idx, fr);
                    wg = sine_i(ph + 0x40000000u);      /* (equal power: cos, sin) */
                    tp_gb[i] = sine_i(ph);
                    md = 3;
                }
                tp.k = k + 1u >= (fx == PX_HALF ? 2u * len : len) ? 0u : k + 1u;
                if (!tp.own_n && !rev && tp.ws - tp.start >= len / tp.s + 1u)
                    tp.own_lo = tp.start - sn / tp.s - 2u, tp.own_n = (len + sn) / tp.s + 4u;   /* captured: the
                                                         * writer leaves it, and the seam's samples before it */
            }
        }
        if ((uint32_t)tp.g < target)
            tp.g = tp.g + pstep > 32767 ? 32767 : tp.g + pstep;
        else if ((uint32_t)tp.g > target)
            tp.g = tp.g < pstep ? 0 : tp.g - pstep;
        tp_md[i] = md, tp_wg[i] = wg;
        if (tp.eq && md != 2u) {                        /* (equal power: sin, cos of a quarter turn) */
            uint32_t ph = (uint32_t)tp.g << 15;
            tp_xg[i] = tp.g ? sine_i(ph) : 0;
            tp_xd[i] = sine_i(ph + 0x40000000u);
        } else {
            tp_xg[i] = tp.g;
            tp_xd[i] = 32767 - tp.g;
        }
        tp_live |= (uint8_t)(md && tp.g);
        if (last)
            tp.ws++;
        tp.t++;
        if (tp.g == 0 && want != fx) {                  /* faded out: none, the next one starts fresh */
            tp.cur = -1;
            tp.own_n = 0;
        }
    }
}

/* one ring's sample i of the block (either core): x in, the punched x out (mix units) */
static inline int32_t tp_x(uint32_t r, uint32_t i, int32_t x)
{
    int16_t *b = tp_ring[r];
    int32_t w;
    if (tp_wr[i]) {                                     /* the ring records (every ring, in step) */
        if (tp_wr[i] == 1u)
            b[tp_wi[i]] = (int16_t)clamp((tp.s == 1u ? x : (tp_half[r] + x) >> 1) >> 3, -32768, 32767);
    } else {
        tp_half[r] = x;
    }
    if (!tp_md[i] || !tp_xg[i])
        return x;
    if (tp_md[i] == 2u) {
        w = x;
    } else {
        int32_t a = b[tp_ia[i]], c = b[tp_ib[i]];
        w = (a + (((c - a) * tp_fr[i]) >> 15)) << 3;
    }
    w = (int32_t)(((int64_t)w * tp_wg[i]) >> 15);
    if (tp_md[i] == 3u) {                               /* (the seam: the other position, its share) */
        int32_t a = b[tp_ic[i]], c = b[tp_id[i]], v = (a + (((c - a) * tp_fr2[i]) >> 15)) << 3;
        w += (int32_t)(((int64_t)v * tp_gb[i]) >> 15);
    }
    if (tp_xg[i] >= 32767)
        return w;
    return (int32_t)(((int64_t)x * tp_xd[i] + (int64_t)w * tp_xg[i]) >> 15);
}
static void tp_zero(uint32_t r, uint32_t n)             /* a silent stem's block: its ring records the silence */
{
    uint32_t i;
    for (i = 0; i < n; i++)
        (void)tp_x(r, i, 0);
}
