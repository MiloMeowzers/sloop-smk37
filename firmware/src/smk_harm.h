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
 * last three frames agree, held through silence. */
#define SMK_HARM 1
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
} hm;
static uint32_t scale_mask(const track_t *t);        /* (seq.c) */
/* NOTE REPEAT = REPEAT (HARMONY, T1 selected; ui_input.c): its press (ms), held; as the LED and the header show it:
 * steady while held, flashing while latched (the owner, 2026-11-12) */
static uint32_t hm_rep_t0;
static uint8_t hm_rep_held;
static int hm_rep_shown(void) { return hm.rep && (hm_rep_held || ((fm1_ms / 250u) & 1u)); }

/* the notes (voice.c trk_note_on / off / all_off, on T1 while HARMONY is on) */
static void hm_note_on(uint32_t note)
{
    uint32_t v, k = 3u;
    for (v = 0; v < 3u; v++)
        if (hm.note[v] == note)
            return;                                     /* (held already) */
    for (v = 0; v < 3u && k == 3u; v++)                 /* a free voice */
        if (!hm.note[v])
            k = v;
    if (k == 3u)                                        /* else the oldest's */
        for (k = 0, v = 1; v < 3u; v++)
            if (hm.age[v] < hm.age[k])
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
/* the voices' ratios this block (T1's notes, SCALE, PITCH) */
static void hm_ratios(const track_t *t)
{
    uint32_t v, sc = hm_int != 0, tn = hm.tnote;
    int32_t f = hm_pitch, pit = f < 0 ? f * 6 : f * 192 / 63;   /* PITCH: -64 .. 63 -> -24 .. +12 semitones */
    for (v = 0; v < 3u; v++) {
        int32_t n = hm.note[v], s;
        if (!n)
            continue;                                   /* (fading out: the ratio it had) */
        s = n - 60;
        if (sc && tn) {                                 /* SCALE: the scale note s above the sung note, rounded down (a
                                                         * CHR scale: s itself) */
            uint32_t mask = scale_mask(t), root = (uint32_t)t->p[P_ROOT] % 12u;
            int32_t m = (int32_t)tn + s, guard = 12;
            while (guard-- && !((mask >> (uint32_t)((m - (int32_t)root + 120) % 12)) & 1u))
                m--;
            s = m - (int32_t)tn;
        }
        hm.dinc[v] = hm_dinc(s * 16 + pit);
    }
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
    int32_t pk = hm.pk_in, po = hm.pk_out, gt[3], fbk, wg, nn = 0;
    int32_t vg = VOC_AUDIO_ON ? HM_GAIN(hm_send[0]) : 0;   /* VOICE (AUDIO IN off: the sends only) */
    const int16_t *in = uin_dst[UD_HARM];
    const int32_t *s0 = hm_sb[0][sp], *s1 = hm_sb[1][sp];
    if (!hm.run)
        hm_start();
    hm_ratios(t);
    for (v = 0; v < 3u; v++) {
        gt[v] = hm.note[v] ? 32767 : 0;
        nn += hm.note[v] != 0;
    }
    fbk = clamp(hm_fb, 0, 127) * 246 / (nn ? nn : 1);
    wg = hm_wd >= 127 ? 32768 : clamp(hm_wd, 0, 127) * 256;   /* WET / DRY, Q15: 64 = 16384 */            /* FB MAIN: 0 .. 127 -> 0 .. 0.95, over the sounding voices */
    if (n > VOC_MAXN)
        n = VOC_MAXN;
    for (i = 0; i < n; i++) {
        int32_t x = ((in[i] * vg) >> 15) + s0[i] + s1[i], vs = 0, wq, y, ax;
        x = clamp(x, -32767, 32767);
        ax = x < 0 ? -x : x;
        pk = ax > pk ? ax : pk;
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
                int32_t s = clamp(((hm.half + x) >> 1) + hm.fb, -32767, 32767);
                s -= (((s * s) >> 15) * s >> 15) * 10923 >> 15;   /* x - x^3 / 3 (slope <= 1; full scale -> 2/3) */
                hm_ring[hm.w] = (int16_t)s;
            }
            hm.w = hm.w + 1u >= HM_LEN ? 0u : hm.w + 1u;
        }
        wq = (int32_t)(hm.w << 16) - (int32_t)((~hm.t & 1u) << 15);   /* the newest slot, the half-pair's phase */
        for (v = 0; v < 3u; v++) {                      /* the voices: two taps, a triangle crossfade */
            int32_t p, p2, g1, a1, a2, g = hm.g[v];
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
            vs += (((a1 * g1 + a2 * (32767 - g1)) >> 15) * g) >> 15;
        }
        if (hm.t & 1u) {                                /* (22.05 kHz: the next pair's), rounded toward 0: a floor held
                                                         * the loop on a small offset for ever (-47 at FB 127, the host
                                                         * test); toward 0 it always dies away */
            int32_t f = vs * fbk;
            hm.fb = (f + (f < 0 ? 32767 : 0)) >> 15;
        }
        vs = clamp(vs, -65535, 65535);                  /* (x 32768 in 32 bits) */
        y = ((x * (32768 - wg)) >> 15) + ((vs * wg) >> 15);   /* WET / DRY (64: x / 2 + vs / 2) */
        ax = y < 0 ? -y : y;
        po = ax > po ? ax : po;
        out[i] = VOC_VS(y);
        hm.t++;
    }
    hm.pk_in = pk, hm.pk_out = po;
    if (sc) {
        TIM_VOC_T(tt);
        hm_lags();
        TIM_ADD(TS_TRK, TIM_NOW() - tt);
    } else {
        hm.tnote = 0, hm.p1 = hm.p2 = 0, hm.lag = 0;   /* (SCALE off: the tracker at rest, a fresh start later) */
    }
    return 1u;
}
