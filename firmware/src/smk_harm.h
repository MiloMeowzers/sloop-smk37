/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37, the vocoder build: the harmoniser's MEASUREMENT PROBE (smk37-devtools harmoniser-spec.md §7g; HARMONISER.md).
 * Not the harmoniser: its DSP at full size, silent, to time it on the unit (console "timing": the "harm" line, inside
 * pre_in) and to check the pitch tracker on real input (console "hprobe"). Off at power-on. "hprobe on" runs it on the
 * USB voice (AUDIO IN on) in the SLICER STUT memory (smk_fxpool's start: STUT records nothing while it runs; refused
 * while STEM FX lends that memory out). Each output sample (44.1 kHz):
 * - the memory (HP_LEN samples at 22.05 kHz, a pair's mean, as smk_tpunch.h PUNCH) written with the input + feedback;
 * - three voices, each two taps half a segment apart read at the voice's ratio with a triangle crossfade (punch.c
 *   PX_OCT generalised: H949 ALGORITHM 1, segment NORM 25 ms), interpolated (ring_q16 style);
 * - DELAY ONLY, one tap;
 * - the feedback: the voices' sum and DELAY ONLY, through the LOW / HIGH shelves (two one-poles), soft-limited;
 * - the tracker: a YIN difference function on the memory at 11.025 kHz (window HP_TW, lags 1 .. HP_TLN), HP_TLB lags
 *   a block, the threshold search online (no divide but one a frame), a frame every ~10 ms.
 * The output goes nowhere: its peak is kept for the console (so the work cannot be optimised away) */
#define SMK_HARM_PROBE 1
#define HP_LEN 9216u                 /* the memory: 22.05 kHz samples, ~418 ms (the H949's 400 + a margin) */
#define HP_SEG 551u                  /* the segment (NORM): 25 ms at 22.05 kHz */
#define HP_D0 4u                     /* the taps' least delay (storage samples) */
#define HP_DLY 4823u                 /* DELAY ONLY in the probe: 218.75 ms (the switches 6.25 + 12.5 + 200) */
#define HP_TW 128u                   /* the tracker's window, 11.025 kHz samples (11.6 ms) */
#define HP_TLN 143u                  /* .. its lags 1 .. 143 (the search from HP_TLMIN: 689 .. 77 Hz) */
#define HP_TLMIN 16u
#define HP_TLB 10u                   /* .. lags a block (a frame: 15 blocks, ~10 ms) */
#define HP_TB (HP_TW + HP_TLN + 1u)  /* .. the snapshot */
_Static_assert(HP_LEN <= NTRK * SL_LEN, "the harmoniser's memory fits the SLICER STUT recordings' part of smk_fxpool");
_Static_assert(2u * HP_TB + HP_SEG + HP_D0 < HP_LEN && HP_DLY + 2u < HP_LEN, "the reads stay in the memory");
static int16_t *const hp_ring = smk_fxpool;
static const int32_t HP_DINC[3] = {-8520, -16318, 16384};   /* (1 - ratio) x 32768: x1.260, x1.498, x0.500 (Q16
                                                             * storage samples an output sample) */
static struct {
    volatile uint8_t on;             /* the console's hprobe */
    uint32_t w, t;                   /* the write slot; output samples (the pair's phase) */
    int32_t half;                    /* the pair's first sample */
    int32_t ph[3];                   /* each voice's segment phase, Q16 storage samples, 0 .. HP_SEG << 16 */
    int32_t fb, lpl, lph;            /* the feedback for the next write; the shelves' one-poles */
    int16_t tb[HP_TB];               /* the tracker's snapshot (11.025 kHz) */
    uint32_t lag;                    /* the next lag (0: take a snapshot) */
    uint32_t sum;                    /* the running sum of d (lags 1 ..) */
    uint32_t d1, d2;                 /* d of the two lags before */
    uint32_t bn, bd;                 /* the best normalised d so far, as num / den (num = d x lag, den = the sum) */
    uint32_t blag, found;            /* its lag; the threshold met (then the first dip's bottom) */
    volatile uint32_t per;           /* the last frame's period, 11.025 kHz samples Q8 (0: no pitch) */
    volatile uint32_t frames, voiced;
    volatile int32_t peak;           /* the output's peak since the console read it */
} hp;

/* the memory at a delay of d (Q16 storage samples) behind the newest slot, interpolated */
static inline int32_t hp_read(int32_t wq, int32_t d)
{
    int32_t p = wq - d, a, b;
    uint32_t i;
    if (p < 0)
        p += (int32_t)(HP_LEN << 16);
    i = (uint32_t)p >> 16;
    a = hp_ring[i];
    b = hp_ring[i + 1u >= HP_LEN ? 0u : i + 1u];
    return a + (((b - a) * (int32_t)(((uint32_t)p >> 1) & 0x7FFFu)) >> 15);
}

/* the tracker's frame: the snapshot of the newest HP_TB samples at 11.025 kHz (every other stored sample), scaled so
 * the squared differences fit 32 bits (the peak to under 1024) */
static void hp_snap(void)
{
    uint32_t j, s = 0;
    int32_t i = (int32_t)hp.w - 1 - 2 * (int32_t)(HP_TB - 1u), pk = 0;
    if (i < 0)
        i += (int32_t)HP_LEN;
    for (j = 0; j < HP_TB; j++) {
        int32_t x = hp_ring[i], ax = x < 0 ? -x : x;
        hp.tb[j] = (int16_t)x;
        pk = ax > pk ? ax : pk;
        i += 2;
        if (i >= (int32_t)HP_LEN)
            i -= (int32_t)HP_LEN;
    }
    while ((pk >> s) >= 1024)
        s++;
    for (j = 0; j < HP_TB; j++)
        hp.tb[j] = (int16_t)(hp.tb[j] >> s);
    hp.sum = 0, hp.d1 = hp.d2 = 0, hp.bn = 1, hp.bd = 0, hp.blag = 0, hp.found = 0;
}

/* HP_TLB lags of the frame; at its last lag, its period (a parabola through the dip, one divide) */
static void hp_lags(void)
{
    uint32_t k, j;
    for (k = 0; k < HP_TLB; k++) {
        uint32_t tau = hp.lag, d = 0;
        if (tau == 0u) {
            hp_snap();
            hp.lag = 1;
            continue;
        }
        for (j = 0; j < HP_TW; j++) {
            int32_t e = hp.tb[j] - hp.tb[j + tau];
            d += (uint32_t)(e * e);
        }
        d >>= 8;                                        /* (<= 2.1M: d x lag and the sum fit 32 bits) */
        hp.sum += d;
        if (tau >= HP_TLMIN && !hp.found && hp.sum) {   /* the normalised d = d x lag / sum, compared as fractions */
            uint32_t num = d * tau;
            if ((uint64_t)num * 20u < (uint64_t)hp.sum * 3u) {   /* under 0.15: the threshold met */
                if (hp.blag && hp.blag == tau - 1u && (uint64_t)num * hp.bd >= (uint64_t)hp.bn * hp.sum)
                    hp.found = 1;                       /* rising again: the dip's bottom is the lag before */
                else
                    hp.bn = num, hp.bd = hp.sum, hp.blag = tau;
            } else if (hp.blag && hp.blag == tau - 1u && (uint64_t)hp.bn * 20u < (uint64_t)hp.bd * 3u) {
                hp.found = 1;                           /* back over the threshold after a dip */
            } else if (!hp.bd || (uint64_t)num * hp.bd < (uint64_t)hp.bn * hp.sum) {
                hp.bn = num, hp.bd = hp.sum, hp.blag = tau;   /* (the global best, if no dip meets the threshold) */
            }
            if (hp.found && hp.blag + 1u == tau) {      /* the parabola: d at lag - 1 (d2), lag (d1), lag + 1 (d) */
                int32_t a = (int32_t)hp.d2, b = (int32_t)hp.d1, c = (int32_t)d, den = a - 2 * b + c, fr = 0;
                if (den > 0)
                    fr = ((a - c) * 128) / den;         /* Q8 / 2 x 2: the offset in 1/256 lag */
                hp.per = (hp.blag << 8) + (uint32_t)clamp(fr, -128, 128);
            }
        }
        hp.d2 = hp.d1, hp.d1 = d;
        if (++hp.lag > HP_TLN) {                         /* the frame's end */
            if (!hp.found)                              /* no dip under the threshold: voiced if the best is under 0.35 */
                hp.per = hp.blag && (uint64_t)hp.bn * 20u < (uint64_t)hp.bd * 7u ? hp.blag << 8 : 0u;
            hp.frames++;
            hp.voiced += hp.per != 0u;
            hp.lag = 0;
        }
    }
}

/* fx.c mix_block, core 0, after the USB voice is in (voc_in, Q15 mono, n <= CTL) */
static void __attribute__((noinline)) hp_block(uint32_t n)
{
    uint32_t i, v;
    int32_t pk = hp.peak;
    for (i = 0; i < n; i++) {
        int32_t x = voc_in[i], vs = 0, dl, y, wq;
        if (!(hp.t & 1u)) {
            hp.half = x;
        } else {                                        /* a pair: one stored sample, the feedback in, soft-limited */
            int32_t s = ((hp.half + x) >> 1) + hp.fb;
            s = clamp(s, -32767, 32767);
            s -= (((s * s) >> 15) * s >> 15) * 10923 >> 15;   /* x - x^3 / 3 (gentle; full scale -> 2/3) */
            hp_ring[hp.w] = (int16_t)s;
            hp.w = hp.w + 1u >= HP_LEN ? 0u : hp.w + 1u;
        }
        wq = (int32_t)(hp.w << 16) - (int32_t)((~hp.t & 1u) << 15);   /* the newest slot, the half-pair's phase */
        for (v = 0; v < 3u; v++) {                      /* the voices: two taps, a triangle crossfade */
            int32_t p = hp.ph[v] + HP_DINC[v], p2, g1, a1, a2;
            if (p < 0)
                p += (int32_t)(HP_SEG << 16);
            else if (p >= (int32_t)(HP_SEG << 16))
                p -= (int32_t)(HP_SEG << 16);
            hp.ph[v] = p;
            p2 = p + (int32_t)(HP_SEG << 15);
            if (p2 >= (int32_t)(HP_SEG << 16))
                p2 -= (int32_t)(HP_SEG << 16);
            g1 = p < (int32_t)(HP_SEG << 15) ? p : (int32_t)(HP_SEG << 16) - p;   /* 0 .. HP_SEG << 15 */
            g1 = (int32_t)(((uint64_t)(uint32_t)g1 * (0xFFFFFFFFu / HP_SEG)) >> 32);   /* -> Q15 */
            a1 = hp_read(wq, p + (int32_t)(HP_D0 << 16));
            a2 = hp_read(wq, p2 + (int32_t)(HP_D0 << 16));
            vs += (a1 * g1 + a2 * (32767 - g1)) >> 15;
        }
        dl = hp_read(wq, (int32_t)(HP_DLY << 16));      /* DELAY ONLY */
        if (hp.t & 1u) {                                /* the feedback (22.05 kHz): FB MAIN 40 %, FB DLY ONLY 30 %,
                                                         * then LOW / HIGH (the one-poles: ~500 Hz, ~3 kHz) */
            int32_t f = (vs * 4369 + dl * 9830) >> 15;  /* (the voices' sum / 3 x 0.4 ..) */
            hp.lpl += ((f - hp.lpl) * 4349) >> 15;      /* 500 Hz */
            hp.lph += ((f - hp.lph) * 18842) >> 15;     /* 3 kHz */
            hp.fb = f + ((hp.lpl * 1229) >> 12) - (((f - hp.lph) * 1229) >> 12);   /* LOW +3 dB, HIGH -3 dB: a darker loop */
        }
        y = (vs + dl) >> 2;
        y = y < 0 ? -y : y;
        pk = y > pk ? y : pk;
        hp.t++;
    }
    hp.peak = pk;
    hp_lags();
}
