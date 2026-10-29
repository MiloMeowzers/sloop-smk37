/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 vocoder (SMK_UAC_IN_PROBE builds: the vocoder candidate's first step, smk37-devtools README "Candidate" and
 * vocoder-harmoniser-reference.md): a channel vocoder after the Sennheiser VSM201. The voice is the computer's audio
 * (usb.c, EP4 OUT), the carrier a SLOOP synth track (HOME > VOCODER > TRACK): fx.c mix_part_run hands the track's
 * signal here after its own chain (DIST, INSERT, SLICER, FILTER), so what comes out goes on through its LEVEL, PAN,
 * sends, punch-ins and stem as the track did.
 *   rate       the voice and the carrier down 2:1 to 22050 Hz (the VSM201's range is 100 Hz - 8 kHz), the output up
 *              2:1 again, through a half-band FIR (27 taps, 8 not zero; ~0.3 ms)
 *   analysis   the voice (mono, Q15, this block: voc_in, usb.c uin_block) through 20 band-passes at the VSM201's
 *              channel centres (smk_vocoder_tab.h, tools/gen_vocoder.py), 4th order (two sections: 12 dB an octave,
 *              a band's envelope stays its own), each full-wave rectified and smoothed (attack ~3 ms, release ~8 ms:
 *              the VSM201's 20 - 50 Hz channel low-passes)
 *   synthesis  the carrier through the same 20 band-passes (4th order: a bright carrier's strong fundamental stays
 *              out of the bands above it), each scaled by its band's envelope, summed; LEVEL the make-up gain; DRY the
 *              voice added back at the full rate (the VSM201's speech addition)
 *   voicing    the VSM201's detector: channel 19's envelope (5.6 kHz) against a channel 0, a low-pass below 1 kHz;
 *              unvoiced (s, sh, t, f) when the high one passes half the low one (back under a third). With SIBILANCE
 *              the carrier then crossfades (~3 ms) to noise at the carrier's own level ("automatically matched to the
 *              voiced replacement signal"), as the VSM201's internal hiss generator
 * Fixed point: the band-passes Q30 coefficients on 64-bit products, rounded, their signals 4 bits above the input's
 * (a low, narrow band's recursion: truncation read 0.6 dB high at 145 Hz); the envelopes Q19. 4 sections a band at
 * 22050 Hz. */
#include "smk_vocoder_tab.h"

#ifndef VOC_MAXN
#define VOC_MAXN 32u                     /* the largest block (CTL); even */
#endif
static uint8_t voc_track;                /* TRACK: 0 OFF, 1..3 the synth track vocoded */
static uint8_t voc_level = 4;            /* LEVEL: make-up gain in 3 dB steps, 0 dB .. +30 dB (default +12 dB) */
static uint8_t voc_dry;                  /* DRY: 0 OFF, 1 -12 dB, 2 -6 dB */
static uint8_t voc_sib = 1;              /* SIBILANCE: noise for unvoiced sounds */
#define VOC_LEVEL_N 11u
#define VOC_DRY_N 3u
static const int32_t VOC_LV_Q8[VOC_LEVEL_N] = {256, 362, 512, 724, 1024, 1448, 2048, 2896, 4096, 5793, 8192};

#define VOC_HN 32u                       /* the half-band's history at 44.1 kHz (27 taps), a power of two */
#define VOC_ON 16u                       /* the decimated outputs' history (the interpolator's: 2 HB), a power of two */
_Static_assert(VOC_HN >= 4u * VOC_HB && VOC_ON >= 2u * VOC_HB && !(VOC_HN & (VOC_HN - 1u)) && !(VOC_ON & (VOC_ON - 1u)),
               "the half-band's rings: powers of two, each its span");
static int16_t voc_in[VOC_MAXN];         /* this block's voice: mono, Q15, the engines' rate */
static struct {
    int32_t ax1[VOC_NB], ax2[VOC_NB], ay1[VOC_NB], ay2[VOC_NB];   /* analysis (the voice): section 1 */
    int32_t bx1[VOC_NB], bx2[VOC_NB], by1[VOC_NB], by2[VOC_NB];   /* section 2 */
    int32_t sx1[VOC_NB], sx2[VOC_NB], sy1[VOC_NB], sy2[VOC_NB];   /* synthesis (the carrier): section 1 */
    int32_t tx1[VOC_NB], tx2[VOC_NB], ty1[VOC_NB], ty2[VOC_NB];   /* section 2 */
    int32_t env[VOC_NB];                 /* the voice's band envelopes, Q19 */
    int32_t vh[VOC_HN], ch[VOC_HN];      /* the voice's and the carrier's last samples at 44.1 kHz (the decimator) */
    int32_t oh[VOC_ON];                  /* the vocoded samples at 22.05 kHz (the interpolator) */
    uint32_t hw, ow;                     /* samples written into vh / ch, into oh */
    int32_t lf, lf_env;                  /* channel 0: the voice low-passed (Q15), its envelope (Q19) */
    int32_t cenv;                        /* the carrier's level (its own scale), for the noise */
    int32_t uvg;                         /* the noise's share: 0 voiced .. 32767 unvoiced, ramped */
    uint8_t uv;                          /* the detector: unvoiced now */
    uint32_t rnd;
    uint32_t blocks, uv_blocks;          /* (the console) */
    int32_t peak_in, peak_out;
} voc = {.rnd = 0x9E3779B9u};

/* one band-pass section of band k, rounded */
static inline int32_t voc_sec(int32_t x, int32_t *x1, int32_t *x2, int32_t *y1, int32_t *y2, uint32_t k)
{
    int64_t a = (int64_t)VOC_B0[k] * (x - *x2) + (int64_t)VOC_C1[k] * *y1 - (int64_t)VOC_A2[k] * *y2 + (1 << 29);
    int32_t y = (int32_t)(a >> 30);
    *x2 = *x1, *x1 = x, *y2 = *y1, *y1 = y;
    return y;
}
static inline int32_t voc_env(int32_t e, int32_t y)      /* full-wave rectified (y Q19), one-pole: attack / release */
{
    int32_t t = y < 0 ? -y : y, d;
    if (t > (1 << 23))
        t = 1 << 23;
    d = t - e;
    return e + (int32_t)(((int64_t)d * (d > 0 ? VOC_ENV_ATT : VOC_ENV_REL)) >> 16);
}
/* the half-band, 2:1 down: the sample 2 HB behind the next, filtered (h: a history, Q15 taps) */
static inline int32_t voc_dec(const int32_t *h)
{
    uint32_t c = voc.hw - 2u * VOC_HB, j;
    int64_t a = (int64_t)h[c & (VOC_HN - 1u)] * 16384;
    for (j = 0; j < VOC_HB; j++)
        a += (int64_t)VOC_HBT[j] * (h[(c - 2u * j - 1u) & (VOC_HN - 1u)] + h[(c + 2u * j + 1u) & (VOC_HN - 1u)]);
    return (int32_t)((a + (1 << 14)) >> 15);
}

/* the voiced / unvoiced decision for a block, from the envelopes at its start (hysteresis: on at 1/2, off at 1/3) */
static void voc_detect(void)
{
    int32_t hf = voc.env[18], lf = voc.lf_env;
    if (!voc.uv && hf * 2 > lf && hf > (64 << 4))
        voc.uv = 1;
    else if (voc.uv && (hf * 3 < lf || hf <= (64 << 4)))
        voc.uv = 0;
}

/* one vocoded sample at 22.05 kHz from the voice v (Q15) and the carrier c (its own scale) */
static int32_t voc_tick(int32_t v, int32_t c)
{
    int32_t ac = c < 0 ? -c : c, tgt;
    int64_t sum = 0;
    uint32_t k;
    voc.lf += (int32_t)(((int64_t)(v - voc.lf) * VOC_LF_K) >> 16);   /* channel 0 */
    voc.lf_env = voc_env(voc.lf_env, voc.lf << 4);
    voc.cenv += (int32_t)(((int64_t)(ac - voc.cenv) * VOC_CENV_K) >> 16);
    tgt = voc.uv && voc_sib ? 32767 : 0;                 /* SIBILANCE: towards noise at the carrier's level, ~3 ms */
    voc.uvg = voc.uvg < tgt ? (voc.uvg + 496 > tgt ? tgt : voc.uvg + 496) : (voc.uvg - 496 < tgt ? tgt : voc.uvg - 496);
    if (voc.uvg) {
        int32_t nz;
        voc.rnd ^= voc.rnd << 13, voc.rnd ^= voc.rnd >> 17, voc.rnd ^= voc.rnd << 5;
        nz = (int32_t)(((int64_t)(int16_t)(voc.rnd >> 16) * voc.cenv) >> 14);   /* +-2 x the level: the rectified
                                                                           * means match */
        c += (int32_t)(((int64_t)(nz - c) * voc.uvg) >> 15);
    }
    v <<= 4, c <<= 4;
    for (k = 0; k < VOC_NB; k++) {
        int32_t ya = voc_sec(v, &voc.ax1[k], &voc.ax2[k], &voc.ay1[k], &voc.ay2[k], k);
        int32_t ys = voc_sec(c, &voc.sx1[k], &voc.sx2[k], &voc.sy1[k], &voc.sy2[k], k);
        ya = voc_sec(ya, &voc.bx1[k], &voc.bx2[k], &voc.by1[k], &voc.by2[k], k);
        ys = voc_sec(ys, &voc.tx1[k], &voc.tx2[k], &voc.ty1[k], &voc.ty2[k], k);
        voc.env[k] = voc_env(voc.env[k], ya);            /* (ya: Q19) */
        sum += (int64_t)ys * (voc.env[k] >> 4);          /* x the envelope, Q15 (ys: the carrier's scale << 4) */
    }
    return (int32_t)(((sum >> 19) * VOC_LV_Q8[voc_level % VOC_LEVEL_N]) >> 8);
}

/* the track's block b (its own scale, n even) becomes the vocoded block, in place */
static void voc_process(int32_t *b, uint32_t n)
{
    uint32_t i, j;
    int32_t pin = voc.peak_in, pout = voc.peak_out;
    voc_detect();
    voc.blocks++;
    voc.uv_blocks += voc.uv;
    for (i = 0; i + 1u < n && i + 1u < VOC_MAXN; i += 2u) {
        int32_t o[2], d, k;
        int64_t a = 0;
        for (k = 0; k < 2; k++) {
            int32_t v = voc_in[i + (uint32_t)k], av = v < 0 ? -v : v;
            if (av > pin)
                pin = av;
            voc.vh[voc.hw & (VOC_HN - 1u)] = v;
            voc.ch[voc.hw & (VOC_HN - 1u)] = b[i + (uint32_t)k];
            voc.hw++;
        }
        d = voc_tick(voc_dec(voc.vh), voc_dec(voc.ch));
        voc.oh[voc.ow++ & (VOC_ON - 1u)] = d;
        /* 2:1 up: the even sample the one VOC_HB back, the odd one the half-band between it and the next */
        o[0] = voc.oh[(voc.ow - 1u - VOC_HB) & (VOC_ON - 1u)];
        for (j = 0; j < VOC_HB; j++)
            a += (int64_t)VOC_HBT[j] * (voc.oh[(voc.ow - 1u - VOC_HB - j) & (VOC_ON - 1u)] +
                                        voc.oh[(voc.ow - VOC_HB + j) & (VOC_ON - 1u)]);
        o[1] = (int32_t)((a + (1 << 13)) >> 14);         /* (x 2: the zeros between) */
        for (k = 0; k < 2; k++) {
            int32_t x = o[k];
            if (voc_dry)
                x += voc_in[i + (uint32_t)k] >> (voc_dry == 2u ? 1 : 2);   /* DRY: -6 / -12 dB */
            x = x > (1 << 22) ? 1 << 22 : x < -(1 << 22) ? -(1 << 22) : x;
            if ((x < 0 ? -x : x) > pout)
                pout = x < 0 ? -x : x;
            b[i + (uint32_t)k] = x;
        }
    }
    voc.peak_in = pin, voc.peak_out = pout;
}
