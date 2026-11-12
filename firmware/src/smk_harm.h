/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37, the vocoder build: the HARMONISER, Stage 1a (smk37-devtools harmoniser-spec.md §8; HARMONISER.md).
 * HOME > HARMONY ON: track 1 is the harmoniser, an Eventide H949 model, instead of a synth. Its "engine" (voice.c
 * track_render calls hm_render) runs in T1's unit, so it renders after core 1 has its job, beside it (§7i: the probe's
 * serial place before the hand-off held core 1 back). The track's chain runs on it (DIST, SLICER, LEVEL, PAN, its
 * reverb and chorus sends, STEM FX, its stem), less its FILTER and its DELAY SEND (fx.c: the H949 is the delay).
 * Each output sample (44.1 kHz):
 * - the input: IN L / IN R routed to HARMONY (usb.c uin_block, uin_dst[UD_HARM]) x VOICE (FADER BANK E, T1's column)
 *   plus the HARM sends of T2, T3 (not while it vocodes) and the drums (FADER BANK E), taken post-fader in the units
 *   (fx.c) a block before (hm_sb: the units of both cores write this block's, the harmoniser reads the last's);
 * - the memory (HM_LEN samples at 22.05 kHz, a pair's mean, as smk_tpunch.h PUNCH; SLICER STUT's part of smk_fxpool,
 *   STUT off meanwhile) written with the input + the feedback, soft-limited; REPEAT (NOTE REPEAT on T1) stops the
 *   writing, the slot still moving: the memory (~418 ms) plays round;
 * - three voices, each two taps half a segment apart read at the voice's ratio with a triangle crossfade (punch.c
 *   PX_OCT generalised: ALGORITHM 1, segment NORM 25 ms), interpolated, each faded in and out (~1.5 ms) with its note;
 * - the feedback FB MAIN (T1's FADER BANK D): the sounding voices' mean x FB (at most 0.95), into the memory through
 *   the soft limit, whose slope is never over 1: the loop's gain stays under 1 (FB LIMIT SAFE; the EQ flat in 1a);
 * - the output: the input (dry) and the voices' sum, half each (WET / DRY 50 %), on the track's scale.
 * The notes: T1's (keys, its sequencer pattern, MIDI in on its channel: voice.c trk_note_on) each set one voice to
 * its distance from C4 (C4 unison, C5 x2, C2 x0.25), up to 3, a fourth taking the oldest's voice; none: the voices
 * silent, the dry plays on. T1's FADER BANK B = PITCH (hm_pitch): the centre x1, down 24 semitones, up 12, all voices.
 * PITCH and FEEDBACK are the harmoniser's own (0 at power-on), not T1's FILTER and DELAY SEND: 67ba07e used those, so
 * a synth preset's delay send became feedback (a fifth fed back climbed octaves) and its filter a transposition.
 * With T1's SCALE on, each voice is the scale note at that distance above the SUNG note (rounded down into the scale)
 * and the tracker runs (only then): YIN on the dry input at 5.5 kHz (a frame ~20 ms), its note taken once two of the
 * last three frames agree, held through silence.
 * Stage 1b (§9, the HARMONY page, ui_hpage.c): the page's settings (hs): each voice's LEVEL, MICRO and function (NORM;
 * SHARP / FLAT: the voice follows its key as in NORM, MICRO its fine shift up to 1.07 / down to 0.93; 2026-11-13: they
 * sounded with no key at 1:1 and ignored the keys, and with VOICE ALL that locked all three: "the keys don't seem to
 * function anymore"), the control method (KEYS &
 * MIDI; MANUAL: the PITCH fader is the VOICE-selected voice's ratio, with no key; both: the keys, PITCH transposing,
 * the MANUAL voice sounding while no key holds it), INPUT LEVEL, IN / OUT (the input straight out, the memory still
 * written), the mutes (DRY, V1-V3), DRY LEVEL, the feedback's LOW / HIGH EQ (two shelves at 22.05 kHz,
 * smk_harm_eq.h) and FB LIMIT: SAFE (the loop's gain under 1 at the EQ's loudest frequency) or H949 (up to 1.25, the
 * soft limit holding the level: it can sustain itself). G1's interval pads play as T1's notes (hm_pad). Every
 * default is 1a's sound, bit for bit. */
#define SMK_HARM 1
#include "smk_harm_eq.h"             /* the feedback EQ's coefficients and INPUT LEVEL's gains (tools/gen_harm_eq.py) */
#define HM_LEN 9216u                 /* the memory: 22.05 kHz samples, ~418 ms (the H949's 400 + a margin) */
#define HM_SEG 551u                  /* the segment (NORM): 25 ms at 22.05 kHz */
#define HM_D0 4u                     /* the taps' least delay (storage samples) */
#define HM_TW 64u                    /* the tracker's window, 5.5 kHz samples (11.6 ms) */
#define HM_TLN 71u                   /* .. its lags 1 .. 71 (the search from HM_TLMIN: 689 .. 78 Hz) */
#define HM_TLMIN 8u
#define HM_TLB 5u                    /* .. lags a block (a frame: 15 blocks, ~20 ms) */
#define HM_TB (HM_TW + HM_TLN + 1u)  /* .. the snapshot */
#define HM_TRN 256u                  /* the tracker's dry ring (5.5 kHz, a power of two), in the pool after the memory */
#define HM_MEM (HM_LEN + HM_TRN + HM_TB)   /* (int16s of smk_fxpool; even) */
#define HM_FADE 512                  /* a voice's gain step a sample (Q15): ~64 samples in or out */
#define HM_GAIN(v) ((int32_t)(v) * 32768 / 100)   /* FADER BANK E's 0 .. 127 as a gain, Q15: 100 = 0 dB exactly, 127
                                                   * +2.1 dB */
_Static_assert((HM_MEM & 1u) == 0u, "the harmoniser's part of the pool stays 32-bit aligned after it");
_Static_assert(HM_TB < HM_TRN, "the snapshot fits the dry ring");
_Static_assert(2u * HM_TB + HM_SEG + HM_D0 < HM_LEN, "the reads stay in the memory");
/* the tracker's lags (Q8, 5.5 kHz) between MIDI note 38 + k and 39 + k (a quarter tone either side of each note) */
#define HM_NLO 38u
static const uint16_t HM_NOTE_LAG[41] = {18675, 17627, 16637, 15703, 14822, 13990, 13205, 12464, 11764, 11104, 10481,
                                         9893,  9337,  8813,  8319,  7852,  7411,  6995,  6603,  6232,  5882,  5552,
                                         5240,  4946,  4669,  4407,  4159,  3926,  3706,  3498,  3301,  3116,  2941,
                                         2776,  2620,  2473,  2334,  2203,  2080,  1963,  1853};

/* (hm_on, HOME > HARMONY: smk_vocoder.h, beside the routes that test it) */
static uint8_t hm_send[4] = {100, 0, 0, 0};   /* FADER BANK E: T1's column = VOICE (the input's level), then the HARM
                                       * sends of T2, T3, the drums; 0 .. 127 (100: 0 dB; kept: vocset) */
static uint8_t hm_int;               /* HOME > HARMONY > INTERVALS: 0 FIXED (the keys from C4, chromatic; the tracker at rest:
                                      * the default), 1 SCALE (from the sung note, in T1's ROOT / SCALE: the song's key);
                                      * kept, project.c vocset (the owner, 2026-11-12) */
static int16_t hm_pitch;             /* FADER BANK B, T1's fader: -64 .. 63 (0: x1; main.c smk_faders, its dead zone) */
static int16_t hm_fb;                /* FB MAIN 0 .. 127 */
static int16_t hm_wd = 64;           /* WET / DRY 0 .. 127 (64: half each, exactly) */
/* the HARMONY page's settings (Stage 1b, harmoniser-spec.md §9; ui_hpage.c; kept: project.c vocset). The audio reads
 * them once a block */
enum { HF_NORM, HF_EXT, HF_SHARP, HF_FLAT, HF_N };   /* a voice's function (FUNCTION SELECT IN; EXTEND: Stage 2) */
#define HC_KEYS 1u                   /* the control method: KEYS & MIDI, MANUAL, both (3) */
#define HC_MAN 2u
typedef struct {
    int8_t mic[3];                   /* MICRO -100 .. 100: cents (NORM); SHARP / FLAT: its size, 0 .. 100 */
    uint8_t lv[3];                   /* LEVEL 0 .. 100 (linear %, 100: 0 dB, 1a's) */
    uint8_t fn[3];                   /* HF_* */
    uint8_t vsel;                    /* VOICE: 0 ALL, 1 .. 3 V1 .. V3 (the MANUAL voice: V1 with ALL) */
    uint8_t ctl;                     /* HC_KEYS, HC_MAN or both */
    uint8_t inl;                     /* INPUT LEVEL, HM_INL's step (HM_INL_0: 0 dB) */
    uint8_t low, high;               /* the feedback EQ's steps (HM_LOW_0, HM_HIGH_0: flat) */
    uint8_t fbl;                     /* FB LIMIT: 0 SAFE, 1 H949 */
    uint8_t mute;                    /* bit 0 DRY, bits 1 .. 3 V1 .. V3: off */
    uint8_t dry;                     /* DRY LEVEL 0 .. 100 (linear %) */
    uint8_t byp;                     /* IN / OUT: 1 OUT (the input straight through) */
    uint8_t fsel;                    /* FUNCTION SELECT: 0 IN (red, pitch), 1 OUT (green, delay: Stage 2) */
    uint8_t rsv[3];
} hset_t;
_Static_assert(sizeof(hset_t) == 22u, "hset_t: as kept in vocset");
static const hset_t HS_DEF = {{0, 0, 0}, {100, 100, 100}, {HF_NORM, HF_NORM, HF_NORM}, 0, HC_KEYS, HM_INL_0, HM_LOW_0,
                              HM_HIGH_0, 0, 0, 100, 0, 0, {0, 0, 0}};
static hset_t hs = {{0, 0, 0}, {100, 100, 100}, {HF_NORM, HF_NORM, HF_NORM}, 0, HC_KEYS, HM_INL_0, HM_LOW_0, HM_HIGH_0,
                    0, 0, 100, 0, 0, {0, 0, 0}};
static void hs_fix(hset_t *s)        /* every value in range (a load, the editor) */
{
    uint32_t v;
    for (v = 0; v < 3u; v++) {
        s->mic[v] = (int8_t)clamp(s->mic[v], -100, 100);
        s->lv[v] = (uint8_t)(s->lv[v] > 100u ? 100u : s->lv[v]);
        s->fn[v] = (uint8_t)(s->fn[v] == HF_SHARP || s->fn[v] == HF_FLAT ? s->fn[v] : HF_NORM);
    }
    s->vsel &= 3u;
    s->ctl &= 3u;
    if (!s->ctl)
        s->ctl = HC_KEYS;
    s->inl = (uint8_t)(s->inl % HM_INL_N), s->low = (uint8_t)(s->low % HM_LOW_N), s->high = (uint8_t)(s->high % HM_HIGH_N);
    s->fbl &= 1u, s->mute &= 15u, s->fsel &= 1u;
    s->byp = 0;                      /* IN / OUT: always IN after a load or power-on (the H949's performance switch, not a
                                      * setting: left on OUT by accident it passed the input dry, WET / DRY and all
                                      * else ignored, and stayed so; the owner 2026-11-13: "I can't silence the
                                      * unprocessed drums") */
    s->dry = (uint8_t)(s->dry > 100u ? 100u : s->dry);
}
/* G1's 16 interval pads (the HK940's: top unison, 2nd, 3rd, 4th, 5th, 6th, 7th, octave up; bottom 2nd .. octave, two
 * octaves down), held or latched: the page sets them, hm_render plays them as T1's notes (60 + the interval) */
static volatile uint16_t hm_pad, hm_padre;   /* (hm_padre: latched pads whose voice was taken, played again: the page
                                              * sets it, hm_render takes it) */
static const int8_t HM_PADIV[16] = {0, 2, 4, 5, 7, 9, 10, 12, -2, -4, -5, -7, -9, -10, -12, -24};
/* (PITCH, FEEDBACK, VOICE IN, WET / DRY: FADER BANK E, HARM; the sends: F, SENDS; ui_fbank.c, main.c) */
static int16_t *hm_ring;             /* the memory: smk_fxpool's start (fx.c sets it; slicer.c owns it) */
#define hm_tring (hm_ring + HM_LEN)                  /* the dry input, 5.5 kHz */
#define hm_tb (hm_ring + HM_LEN + HM_TRN)            /* the frame's snapshot, scaled */
static int32_t hm_sb[2][2][VOC_MAXN];   /* the HARM sends: [core][block parity], Q15 (fx.c writes, hm_render reads) */
static uint8_t hm_sbp;               /* this block's parity (fx.c, core 0, before the units) */
#define HM_T(t) (hm_on && (t) == &trk[0])
static struct {
    uint8_t note[3];                 /* each voice's note (0: none) */
    uint32_t age[3], seq;            /* .. its order (the oldest the lowest); the next */
    volatile uint8_t rep;            /* REPEAT: the memory not written */
    uint8_t run;                     /* running (its memory cleared at the start) */
    uint32_t w, t;                   /* the write slot; output samples (the pair's phase) */
    int32_t half;                    /* the pair's first sample */
    int32_t ph[3], g[3], dinc[3];    /* each voice: its segment phase (Q16 storage samples), gain (Q15), the delay's
                                      * change an output sample ((1 - ratio) x 32768, Q16) */
    int32_t fb;                      /* the feedback for the next write */
    uint32_t tw;                     /* the dry ring's write count */
    int32_t dacc;                    /* .. the 8-sample sum */
    uint32_t lag, sum;               /* the next lag (0: take a snapshot); the running sum of d */
    uint16_t cm[HM_TLN + 1u];        /* the normalised d, Q12 (4096 = 1), each lag */
    uint8_t p1, p2;                  /* the last two frames' notes (0: unvoiced) */
    volatile uint8_t tnote;          /* the sung note (0: none yet) */
    volatile uint32_t per;           /* the last frame's period, 5.5 kHz samples Q8 (0: no pitch) */
    volatile uint32_t frames, voiced;
    volatile int32_t pk_in, pk_out;  /* the peaks since the console read them */
    uint8_t on[3];                   /* each voice sounding this block (a note, MANUAL, SHARP / FLAT) */
    uint16_t padl;                   /* the pads as last played */
    int32_t eq[2][4];                /* the feedback EQ's state (LOW, HIGH): x1, x2, y1, y2 */
    int32_t cq[2][5];                /* .. its coefficients this block (a copy in RAM) */
    volatile int32_t up[6];          /* the page's peaks (it reads and clears them): the input, V1 .. V3, the output, the
                                      * feedback into the memory before its soft limit */
} hm;
#define HM_UP_IN 0u
#define HM_UP_V 1u
#define HM_UP_OUT 4u
#define HM_UP_FB 5u
static uint32_t hm_mvoice(void) { return hs.vsel ? (hs.vsel - 1u) % 3u : 0u; }   /* the MANUAL voice */
static uint32_t scale_mask(const track_t *t);        /* (seq.c) */
/* NOTE REPEAT = REPEAT (HARMONY, T1 selected; ui_input.c): its press (ms), held; as the LED and the header show it:
 * steady while held, flashing while latched (the owner, 2026-11-12) */
static uint32_t hm_rep_t0;
static uint8_t hm_rep_held, hm_rep_pad;              /* (hm_rep_pad: the HARMONY page's REPEAT pad held, ui_hpage.c) */
static int hm_rep_shown(void) { return hm.rep && (hm_rep_held || hm_rep_pad || ((fm1_ms / 250u) & 1u)); }

/* the notes (voice.c trk_note_on / off / all_off, on T1 while HARMONY is on) */
static void hm_note_on(uint32_t note)
{
    uint32_t v, k = 3u, res = (hs.ctl & 3u) == HC_MAN ? hm_mvoice() : 3u;   /* (MANUAL alone: its voice is the fader's) */
    for (v = 0; v < 3u; v++)
        if (hm.note[v] == note)
            return;                                     /* (held already) */
    for (v = 0; v < 3u && k == 3u; v++)                 /* a free voice */
        if (!hm.note[v] && v != res)
            k = v;
    if (k == 3u)                                        /* else the oldest's */
        for (v = 0; v < 3u; v++)
            if (v != res && (k == 3u || hm.age[v] < hm.age[k]))
                k = v;
    hm.age[k] = ++hm.seq;
    hm.note[k] = (uint8_t)(note ? note : 1u);
}
static void hm_note_off(uint32_t note)
{
    uint32_t v;
    for (v = 0; v < 3u; v++)
        if (hm.note[v] == note)
            hm.note[v] = 0;
}
static void hm_all_off(void) { hm.note[0] = hm.note[1] = hm.note[2] = 0; }

/* the memory at a delay of d (Q16 storage samples) behind the newest slot, interpolated */
static inline int32_t hm_read(int32_t wq, int32_t d)
{
    int32_t p = wq - d, a, b;
    uint32_t i;
    if (p < 0)
        p += (int32_t)(HM_LEN << 16);
    i = (uint32_t)p >> 16;
    a = hm_ring[i];
    b = hm_ring[i + 1u >= HM_LEN ? 0u : i + 1u];
    return a + (((b - a) * (int32_t)(((uint32_t)p >> 1) & 0x7FFFu)) >> 15);
}

/* the tracker (the probe's, at div 8, tlb 5: harmoniser-spec.md §7i). The frame: the newest HM_TB samples of the dry
 * ring, scaled so the squared differences fit 32 bits (the peak under 1024) */
static void hm_snap(void)
{
    uint32_t j, s = 0, i = hm.tw - HM_TB;
    int32_t pk = 0;
    for (j = 0; j < HM_TB; j++) {
        int32_t x = hm_tring[(i + j) & (HM_TRN - 1u)], ax = x < 0 ? -x : x;
        hm_tb[j] = (int16_t)x;
        pk = ax > pk ? ax : pk;
    }
    while ((pk >> s) >= 1024)
        s++;
    for (j = 0; j < HM_TB; j++)
        hm_tb[j] = (int16_t)(hm_tb[j] >> s);
    hm.sum = 0;
}
/* the frame's end: its period from the normalised curve (Q8 lags, 0: none) */
static uint32_t hm_pick(void)
{
    uint32_t t, best = 0, h, k;
    const uint16_t *cm = hm.cm;
    for (t = HM_TLMIN; t < HM_TLN; t++)                 /* the first dip under 0.15, to its bottom */
        if (cm[t] < 614u) {
            while (t + 1u < HM_TLN && cm[t + 1u] < cm[t])
                t++;
            best = t;
            break;
        }
    if (!best) {                                        /* none: the deepest, if under 0.35 */
        uint32_t m = 1434u;
        for (t = HM_TLMIN; t < HM_TLN; t++)
            if (cm[t] < m)
                m = cm[t], best = t;
        if (!best)
            return 0;
    }
    if (best + 1u >= HM_TLN)                            /* at the lags' end: below the range, not a pitch */
        return 0;
    for (k = 3u; k >= 2u; k--) {                        /* the octave check: a dip near lag / 3, lag / 2 nearly as deep */
        h = (best + k / 2u) / k;
        if (h > HM_TLMIN && h + 1u < HM_TLN) {
            uint32_t c = h;
            c = cm[h - 1u] < cm[c] ? h - 1u : c;
            c = cm[h + 1u] < cm[c] ? h + 1u : c;
            if (cm[c] < 614u && cm[c] <= cm[best] + 410u && cm[c] <= cm[c - 1u] && cm[c] <= cm[c + 1u]) {
                best = c;
                break;
            }
        }
    }
    {   /* a parabola through the dip */
        int32_t a = cm[best - 1u], b = cm[best], c = cm[best + 1u], den = a - 2 * b + c, fr = 0;
        if (den > 0)
            fr = ((a - c) * 128) / den;
        return (best << 8) + (uint32_t)clamp(fr, -128, 128);
    }
}
/* a period's note (MIDI), or 0 */
static uint32_t hm_per_note(uint32_t per)
{
    uint32_t k = 0;
    if (!per)
        return 0;
    while (k < 41u && HM_NOTE_LAG[k] > per)
        k++;
    return HM_NLO + k;
}
/* HM_TLB lags of the frame; at its end, the period and the note (two of the last three frames agreeing) */
static void hm_lags(void)
{
    uint32_t k, j;
    for (k = 0; k < HM_TLB; k++) {
        uint32_t tau = hm.lag, d = 0;
        if (tau == 0u) {
            hm_snap();
            hm.lag = 1;
            continue;
        }
        for (j = 0; j < HM_TW; j++) {
            int32_t e = hm_tb[j] - hm_tb[j + tau];
            d += (uint32_t)(e * e);
        }
        d >>= 8;                                        /* (<= 1.1M: d x lag and the sum fit 32 bits) */
        hm.sum += d;
        {   /* the normalised d = d x lag / sum, Q12 (one hardware divide) */
            uint32_t sh = hm.sum >> 12, c = sh ? d * tau / sh : 65535u;
            hm.cm[tau] = (uint16_t)(c > 65535u ? 65535u : c);
        }
        if (++hm.lag > HM_TLN) {                        /* the frame's end */
            uint32_t nt;
            hm.per = hm_pick();
            nt = hm_per_note(hm.per);
            hm.frames++;
            hm.voiced += nt != 0u;
            if (nt && (nt == hm.p1 || nt == hm.p2))
                hm.tnote = (uint8_t)nt;                 /* (unvoiced, or no two agreeing: the last note held) */
            hm.p2 = hm.p1, hm.p1 = (uint8_t)nt;
            hm.lag = 0;
        }
    }
}

/* a voice's (1 - ratio) x 32768 (Q16 storage samples an output sample): s16 = its distance in 1/16 semitones */
static int32_t hm_dinc(int32_t s16)
{
    uint32_t b = pitch_inc(960u), r;                    /* (C4: the table holds two octaves down, one up from it) */
    s16 = clamp(s16, -384, 192);
    r = (uint32_t)(((uint64_t)pitch_inc((uint32_t)(960 + s16)) << 16) / b);   /* the ratio, Q16 */
    return 32768 - (int32_t)(r >> 1);
}
/* .. with ct cents more (MICRO; 0: hm_dinc's, exactly), the table read between its 1/16 semitone steps */
static int32_t hm_dincf(int32_t s16, int32_t ct)
{
    int32_t q, i, fr;
    uint32_t b, a0, a1, r;
    if (!ct)
        return hm_dinc(s16);
    q = clamp(s16 * 100 + ct * 16, -38400, 19200);     /* (1/1600 semitones) */
    i = q >= 0 ? q / 100 : -((-q + 99) / 100);
    fr = q - i * 100;
    b = pitch_inc(960u);
    a0 = pitch_inc((uint32_t)(960 + i));
    a1 = i < 192 ? pitch_inc((uint32_t)(961 + i)) : a0;
    r = (uint32_t)((((uint64_t)a0 * (uint32_t)(100 - fr) + (uint64_t)a1 * (uint32_t)fr) << 16) / (100u * (uint64_t)b));
    return 32768 - (int32_t)(r >> 1);
}
/* the voices this block: which sound, and their ratios (T1's notes, SCALE, PITCH, the page's control method, MICRO,
 * SHARP / FLAT) */
static void hm_ratios(const track_t *t)
{
    uint32_t v, sc = hm_int != 0, tn = hm.tnote, m = hm_mvoice(), ctl = hs.ctl & 3u;
    int32_t f = hm_pitch, pit = f < 0 ? f * 6 : f * 192 / 63;   /* PITCH: -64 .. 63 -> -24 .. +12 semitones */
    if (!ctl)
        ctl = HC_KEYS;
    for (v = 0; v < 3u; v++) {
        int32_t n = hm.note[v], s, ct = hs.mic[v], fn = hs.fn[v];
        int man = (ctl & HC_MAN) && v == m && (ctl == HC_MAN || !n);   /* (both: the MANUAL voice while no key holds
                                                         * it) */
        hm.on[v] = (uint8_t)(n || man);
        if (!hm.on[v])
            continue;                                   /* (fading out: the ratio it had) */
        if (fn == HF_SHARP || fn == HF_FLAT) {          /* MICRO as the H949's fine shift: 1:1 up to 1.07 / down to
                                                         * 0.93, finer near 1:1 (to ~1.001), on the voice's interval */
            int32_t a = ct < 0 ? -ct : ct;
            ct = fn == HF_SHARP ? a * a * 117 / 10000 : -(a * a * 126 / 10000);
        }
        if (man) {                                      /* MANUAL: the fader is the ratio */
            hm.dinc[v] = hm_dincf(pit, ct);
            continue;
        }
        s = n - 60;
        if (sc && tn) {                                 /* SCALE: the scale note s above the sung note, rounded down (a
                                                         * CHR scale: s itself) */
            uint32_t mask = scale_mask(t), root = (uint32_t)t->p[P_ROOT] % 12u;
            int32_t m = (int32_t)tn + s, guard = 12;
            while (guard-- && !((mask >> (uint32_t)((m - (int32_t)root + 120) % 12)) & 1u))
                m--;
            s = m - (int32_t)tn;
        }
        hm.dinc[v] = hm_dincf(s * 16 + ((ctl & HC_KEYS) ? pit : 0), ct);   /* (MANUAL alone: PITCH is its voice's) */
    }
}
/* the readout: voice v's ratio x 1000 (1000: 1:1) */
static uint32_t hm_ratio1000(uint32_t v) { return (uint32_t)((((int64_t)(32768 - hm.dinc[v % 3u]) * 2000) + 32768) >> 16); }
/* a feedback EQ section (direct form 1, Q HM_EQ_Q), its state 8 bits finer than the samples (a 400 Hz shelf's poles
 * sit near 1 at 22.05 kHz: with the state in whole samples its rounding came back ~30 times over, -13 dB for -12 at
 * low levels), the output rounded toward 0 as the loop is */
static inline int32_t hm_bq(int32_t *z, const int32_t *c, int32_t x)
{
    int32_t x8 = x * 256, y8;
    int64_t a = (int64_t)c[0] * x8 + (int64_t)c[1] * z[0] + (int64_t)c[2] * z[1] - (int64_t)c[3] * z[2] -
                (int64_t)c[4] * z[3];
    a >>= HM_EQ_Q;
    y8 = a > 33554431 ? 33554431 : a < -33554431 ? -33554431 : (int32_t)a;   /* (+-131072 samples) */
    z[1] = z[0], z[0] = x8, z[3] = z[2], z[2] = y8;
    return (y8 + (y8 < 0 ? 255 : 0)) >> 8;
}

/* the harmoniser's start: no stale memory, the voices at rest (the held notes kept) */
static void hm_start(void)
{
    uint8_t nt[3] = {hm.note[0], hm.note[1], hm.note[2]};
    uint32_t ag[3] = {hm.age[0], hm.age[1], hm.age[2]}, sq = hm.seq;
    memset(hm_ring, 0, HM_MEM * sizeof hm_ring[0]);
    memset(&hm, 0, sizeof hm);
    memcpy(hm.note, nt, sizeof nt), memcpy(hm.age, ag, sizeof ag), hm.seq = sq;
    hm.ph[1] = (int32_t)(HM_SEG << 14), hm.ph[2] = (int32_t)(HM_SEG << 15);
    hm.run = 1;
}

/* the harmoniser: T1's engine (voice.c track_render), n <= VOC_MAXN samples on the track's scale into out; 1 (the
 * chain runs: the dry plays even with no note) */
static uint32_t __attribute__((noinline)) hm_render(track_t *t, int32_t *out, uint32_t n)
{
    uint32_t i, v, sp = hm_sbp ^ 1u, sc = hm_int != 0;   /* (INTERVALS SCALE: the tracker runs) */
    int32_t pk = hm.pk_in, po = hm.pk_out, gt[3], lg[3], fbk, wg, dg, nn = 0, ui_in = 0, ui_out = 0, ui_fb = 0;
    int32_t ui_v[3] = {0, 0, 0};
    int32_t vg = VOC_AUDIO_ON ? HM_GAIN(hm_send[0]) : 0;   /* VOICE (AUDIO IN off: the sends only) */
    int32_t ig = HM_INL[hs.inl % HM_INL_N], byp = hs.byp & 1u;   /* INPUT LEVEL (Q12), IN / OUT */
    uint32_t li = hs.low % HM_LOW_N, hi = hs.high % HM_HIGH_N, eql = li != HM_LOW_0, eqh = hi != HM_HIGH_0;
    const int16_t *in = uin_dst[UD_HARM];
    const int32_t *s0 = hm_sb[0][sp], *s1 = hm_sb[1][sp];
    if (!hm.run)
        hm_start();
    {   /* G1's pads: as notes (here, on this core, as the sequencer's) */
        uint32_t pd = hm_pad, re = hm_padre & pd, ch = (pd ^ hm.padl) | re, k;
        hm_padre = 0;
        for (k = 0; ch && k < 16u; k++)
            if ((ch >> k) & 1u) {
                uint32_t nt = (uint32_t)(60 + HM_PADIV[k]);
                if ((pd >> k) & 1u)
                    hm_note_on(nt);
                else
                    hm_note_off(nt);
            }
        hm.padl = (uint16_t)pd;
    }
    hm_ratios(t);
    for (v = 0; v < 3u; v++) {
        gt[v] = hm.on[v] ? 32767 : 0;
        nn += hm.on[v] != 0;
        lg[v] = (hs.mute >> (v + 1u)) & 1u ? 0 : (int32_t)(hs.lv[v] > 100u ? 100u : hs.lv[v]) * 32768 / 100;   /* LEVEL
                                                         * (100: 32768, exactly 1a's) */
    }
    if (hs.fbl & 1u) {                                  /* FB MAIN: H949, to 1.25 over the sounding voices (the soft
                                                         * limit holds it) */
        fbk = clamp(hm_fb, 0, 127) * 323 / (nn ? nn : 1);
    } else {                                            /* SAFE: to 0.95, under the EQ's loudest gain */
        fbk = clamp(hm_fb, 0, 127) * 246 / (nn ? nn : 1);
        if (eql)
            fbk = fbk * 4096 / HM_LOW_PK[li];
        if (eqh)
            fbk = fbk * 4096 / HM_HIGH_PK[hi];
    }
    memcpy(hm.cq[0], HM_LOW[li], sizeof hm.cq[0]), memcpy(hm.cq[1], HM_HIGH[hi], sizeof hm.cq[1]);
    wg = hm_wd >= 127 ? 32768 : clamp(hm_wd, 0, 127) * 256;   /* WET / DRY, Q15: 64 = 16384 */
    dg = hs.mute & 1u ? 0 : (int32_t)(hs.dry > 100u ? 100u : hs.dry) * 32768 / 100;   /* DRY LEVEL (100: 32768) */
    if (n > VOC_MAXN)
        n = VOC_MAXN;
    for (i = 0; i < n; i++) {
        int32_t x = ((in[i] * vg) >> 15) + s0[i] + s1[i], vs = 0, wq, y, ax;
        x = clamp((x * ig) >> 12, -32767, 32767);      /* (INPUT LEVEL: 0 dB = 4096, exactly) */
        ax = x < 0 ? -x : x;
        pk = ax > pk ? ax : pk;
        ui_in = ax > ui_in ? ax : ui_in;
        if (sc) {
            hm.dacc += x;                               /* the tracker's dry ring: an 8-sample mean, 5.5 kHz */
            if ((hm.t & 7u) == 7u) {
                hm_tring[hm.tw & (HM_TRN - 1u)] = (int16_t)(hm.dacc >> 3);
                hm.tw++;
                hm.dacc = 0;
            }
        }
        if (!(hm.t & 1u)) {
            hm.half = x;
        } else {                                        /* a pair: one stored sample, the feedback in, soft-limited */
            if (!hm.rep) {
                int32_t s = clamp(((hm.half + x) >> 1) + hm.fb, -32767, 32767), as = s < 0 ? -s : s;
                ui_fb = as > ui_fb ? as : ui_fb;
                s -= (((s * s) >> 15) * s >> 15) * 10923 >> 15;   /* x - x^3 / 3 (slope <= 1; full scale -> 2/3) */
                hm_ring[hm.w] = (int16_t)s;
            }
            hm.w = hm.w + 1u >= HM_LEN ? 0u : hm.w + 1u;
        }
        wq = (int32_t)(hm.w << 16) - (int32_t)((~hm.t & 1u) << 15);   /* the newest slot, the half-pair's phase */
        for (v = 0; v < 3u; v++) {                      /* the voices: two taps, a triangle crossfade */
            int32_t p, p2, g1, a1, a2, c, g = hm.g[v];
            g = g < gt[v] ? (g + HM_FADE > gt[v] ? gt[v] : g + HM_FADE) : (g - HM_FADE < gt[v] ? gt[v] : g - HM_FADE);
            hm.g[v] = g;
            if (!g)
                continue;
            p = hm.ph[v] + hm.dinc[v];
            if (p < 0)
                p += (int32_t)(HM_SEG << 16);
            else if (p >= (int32_t)(HM_SEG << 16))
                p -= (int32_t)(HM_SEG << 16);
            hm.ph[v] = p;
            p2 = p + (int32_t)(HM_SEG << 15);
            if (p2 >= (int32_t)(HM_SEG << 16))
                p2 -= (int32_t)(HM_SEG << 16);
            g1 = p < (int32_t)(HM_SEG << 15) ? p : (int32_t)(HM_SEG << 16) - p;   /* 0 .. HM_SEG << 15 */
            g1 = (int32_t)(((uint64_t)(uint32_t)g1 * (0xFFFFFFFFu / HM_SEG)) >> 32);   /* -> Q15 */
            a1 = hm_read(wq, p + (int32_t)(HM_D0 << 16));
            a2 = hm_read(wq, p2 + (int32_t)(HM_D0 << 16));
            c = (((((a1 * g1 + a2 * (32767 - g1)) >> 15) * g) >> 15) * lg[v]) >> 15;   /* (LEVEL, the mute) */
            vs += c;
            c = c < 0 ? -c : c;
            ui_v[v] = c > ui_v[v] ? c : ui_v[v];
        }
        if (hm.t & 1u) {                                /* (22.05 kHz: the next pair's), rounded toward 0: a floor held
                                                         * the loop on a small offset for ever (-47 at FB 127, the host
                                                         * test); toward 0 it always dies away */
            int32_t f = vs * fbk;
            f = (f + (f < 0 ? 32767 : 0)) >> 15;
            if (eql)                                    /* (the EQ: flat, not run: 1a's loop exactly) */
                f = hm_bq(hm.eq[0], hm.cq[0], f);
            if (eqh)
                f = hm_bq(hm.eq[1], hm.cq[1], f);
            hm.fb = f;
        }
        vs = clamp(vs, -65535, 65535);                  /* (x 32768 in 32 bits) */
        y = ((((x * dg) >> 15) * (32768 - wg)) >> 15) + ((vs * wg) >> 15);   /* DRY LEVEL, WET / DRY (64: x / 2 + vs /
                                                         * 2) */
        if (byp)
            y = x;                                      /* IN / OUT: OUT, the input straight through */
        ax = y < 0 ? -y : y;
        po = ax > po ? ax : po;
        ui_out = ax > ui_out ? ax : ui_out;
        out[i] = VOC_VS(y);
        hm.t++;
    }
    hm.pk_in = pk, hm.pk_out = po;
    if (ui_in > hm.up[HM_UP_IN])                        /* (the page's: it takes them at its rate) */
        hm.up[HM_UP_IN] = ui_in;
    for (v = 0; v < 3u; v++)
        if (ui_v[v] > hm.up[HM_UP_V + v])
            hm.up[HM_UP_V + v] = ui_v[v];
    if (ui_out > hm.up[HM_UP_OUT])
        hm.up[HM_UP_OUT] = ui_out;
    if (ui_fb > hm.up[HM_UP_FB])
        hm.up[HM_UP_FB] = ui_fb;
    if (sc) {
        TIM_VOC_T(tt);
        hm_lags();
        TIM_ADD(TS_TRK, TIM_NOW() - tt);
    } else {
        hm.tnote = 0, hm.p1 = hm.p2 = 0, hm.lag = 0;   /* (SCALE off: the tracker at rest, a fresh start later) */
    }
    return 1u;
}
