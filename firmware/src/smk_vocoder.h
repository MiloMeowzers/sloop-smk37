/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 audio track and vocoder (SMK_UAC_IN_PROBE builds: the vocoder candidate, smk37-devtools README "Candidate"
 * and vocoder-harmoniser-reference.md). The owner's design (2026-10-30): while HOME > USB > AUDIO IN is on, track 3 is
 * the AUDIO TRACK (SLOOP keeps four: two engine tracks, the audio track, the drums). Its sound, made in voice.c
 * track_render before the track's own chain (DIST, INSERT, SLICER, FILTER, LEVEL, PAN, the sends, the punch-ins, its
 * stem), is set by HOME > VOCODER > MODE:
 *   AUDIO    the computer's audio (usb.c, UIN_EP OUT), mono, dry: external audio in the mix with SLOOP's effects
 *   VOCODER  the vocoder, its carrier plain band-limited saws on track 3's notes (keys, sequencer, arp; voice.c
 *            voc_saw), through the track's ADSR and a resonant low-pass (HOME > CARRIER > CUTOFF, RESO; the track's
 *            ENV > FLT and LFO > FLT move it)
 * Track 3's engine has no part in it (2026-10-30, the owner's: an ENGINE carrier is gone): while AUDIO IN is on the
 * track shows AUDIO or VOCODER, not its engine, and its engine and presets cannot be changed (ui.c); its engine, sound
 * and values stay as they were for when AUDIO IN is off again.
 * A change of state (MODE, AUDIO IN) fades the track out and in again (~5 ms each way): no click.
 * The two cores (fx.c mix_block): the analysis (the voice's bank, its envelopes) runs on core 0 while core 1 renders
 * track 3 (the synthesis), a block apart: the synthesis takes the envelopes core 0 made the block before (0.7 ms, well
 * inside an envelope's ~3 ms attack). Core 1 renders track 3 with the drums (fx.c units_split): the vocoder whole on it
 * made core 0 wait for it a third of the time (2026-10-30, SYNTHS+RET, 18 voices shed a second).
 * The vocoder, after the Sennheiser VSM201:
 *   rate       the voice and the carrier down 2:1 to 22050 Hz (the VSM201's range is 100 Hz - 8 kHz), the output up
 *              2:1 again, through a half-band FIR (27 taps, 8 not zero; ~0.3 ms)
 *   analysis   the voice (mono, Q15, this block: voc_in, usb.c uin_block) through 20 band-passes at the VSM201's
 *              channel centres (smk_vocoder_tab.h, tools/gen_vocoder.py), 4th order (two sections: 12 dB an octave,
 *              a band's envelope stays its own), each full-wave rectified and smoothed (attack ~3 ms, release ~8 ms:
 *              the VSM201's 20 - 50 Hz channel low-passes)
 *   synthesis  the carrier through the same 20 band-passes (4th order: a bright carrier's strong fundamental stays
 *              out of the bands above it), each scaled by its band's envelope, summed; LEVEL the make-up gain
 *   HF         the VSM201's speech addition: the voice's own bands above 4 kHz (4560, 5570, 6780 Hz) added to the
 *              output, for the consonants (the owner chose it over the hiss generator, which was hissy and imprecise)
 *   DRY        the plain voice blended back (the VSM201's bypass)
 * Fixed point: the band-passes Q30 coefficients on 64-bit products, rounded, their signals 4 bits above the input's
 * (a low, narrow band's recursion: truncation read 0.6 dB high at 145 Hz); the envelopes Q19. The track's scale: a
 * voice at full scale 2 x VOICE_FS (dsp.c); the voice (Q15) is put on it as one such voice. */
#include "smk_vocoder_tab.h"

#ifndef VOC_MAXN
#define VOC_MAXN 32u                     /* the largest block (CTL); even */
#endif
#define VOC_TRK 2u                       /* track 3: the audio track */
#if !SMK_UAC_PROBE
static uint8_t uin_mode;                 /* HOME > USB > AUDIO IN (usb.c): 0 OFF, 1 ON, 2 TEST; live, not kept */
#define VOC_AUDIO_ON (uin_mode != 0u)
#else
#define VOC_AUDIO_ON 0                   /* (the loopback probe: track 3 its engine; the input on the monitor) */
#endif
enum { VM_OFF, VM_SAW, VM_N };
static uint8_t voc_mode;                 /* MODE: VM_OFF (AUDIO: the dry audio), VM_SAW (VOCODER) */
static uint8_t voc_cut = 127;            /* CARRIER > CUTOFF, RESO: the saws' low-pass (0 .. 127, dsp.c tsvf_coef) */
static uint8_t voc_res;
static uint8_t voc_level = 4;            /* LEVEL: -12 .. +18 dB in 3 dB steps (default 0 dB: what the first listening
                                          * set at +16 .. 18 of the old scale) */
static uint8_t voc_dry;                  /* DRY: OFF, -18, -12, -6, 0 dB */
static uint8_t voc_hf = 2;               /* HF (speech addition): OFF, -18, -12, -6, 0 dB (default -12) */
#define VOC_LEVEL_N 11u
#define VOC_MIX_N 5u
static const int32_t VOC_LV_Q8[VOC_LEVEL_N] = {511, 722, 1020, 1441, 2035, 2875, 4061, 5737, 8104, 11447, 16170};
static const int32_t VOC_MIX_Q15[VOC_MIX_N] = {0, 4125, 8231, 16423, 32767};
#define VOC_VS(x) ((int32_t)(((int64_t)(x) * VOICE_FS) >> 14))   /* the voice (Q15) on the track's scale */
#define VOC_XMAX ((1 << 23) - 1)         /* the output's ceiling (-2^22 .. -3 x 2^20 read as the ROM's addresses
                                          * 0xFFC00000.., which the build's image check refuses: 2^23 is clear) */
#define VOC_HF0 17u                      /* the speech addition's first band (4560 Hz) */

#define VOC_HN 32u                       /* the half-band's history at 44.1 kHz (27 taps), a power of two */
#define VOC_ON 16u                       /* the decimated outputs' history (the interpolator's: 2 HB), a power of two */
_Static_assert(VOC_HN >= 4u * VOC_HB && VOC_ON >= 2u * VOC_HB && !(VOC_HN & (VOC_HN - 1u)) && !(VOC_ON & (VOC_ON - 1u)),
               "the half-band's rings: powers of two, each its span");
static int16_t voc_in[VOC_MAXN];         /* this block's voice: mono, Q15, the engines' rate */
static struct {                          /* the analysis (core 0, fx.c mix_block): the voice */
    int32_t ax1[VOC_NB], ax2[VOC_NB], ay1[VOC_NB], ay2[VOC_NB];   /* section 1 */
    int32_t bx1[VOC_NB], bx2[VOC_NB], by1[VOC_NB], by2[VOC_NB];   /* section 2 */
    int32_t env[VOC_NB];                 /* the band envelopes, Q19 */
    int32_t vh[VOC_HN];                  /* the last samples at 44.1 kHz (the decimator) */
    uint32_t hw;
    int32_t peak_in;
} voa;
/* what the analysis hands the synthesis: each 22.05 kHz sample's envelopes (Q15) and speech addition; two, a block
 * apart (voc_aw: the one core 0 writes this block, set before core 1's job starts; core 1 reads the other) */
static struct {
    int32_t env[VOC_MAXN / 2u][VOC_NB];
    int32_t hf[VOC_MAXN / 2u];
} voc_ab[2];
static uint8_t voc_aw;
static struct {                          /* the synthesis (track 3's core): the carrier */
    int32_t sx1[VOC_NB], sx2[VOC_NB], sy1[VOC_NB], sy2[VOC_NB];   /* section 1 */
    int32_t tx1[VOC_NB], tx2[VOC_NB], ty1[VOC_NB], ty2[VOC_NB];   /* section 2 */
    int32_t ch[VOC_HN];                  /* the carrier's last samples at 44.1 kHz (the decimator) */
    int32_t oh[VOC_ON];                  /* the vocoded samples at 22.05 kHz (the interpolator) */
    uint32_t hw, ow;                     /* samples written into ch, into oh */
    uint32_t blocks;                     /* (the console) */
    int32_t peak_out;
} voc;
/* the audio track's state: 0 the engine (AUDIO IN off), 1 + MODE; the one playing (cur) and its fade (Q15) */
static uint8_t voc_cur;
static int32_t voc_xg = 32767;

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
/* the half-band, 2:1 down: the sample 2 HB behind the next, filtered (h: a history, w the samples written; Q15 taps) */
static inline int32_t voc_dec(const int32_t *h, uint32_t w)
{
    uint32_t c = w - 2u * VOC_HB, j;
    int64_t a = (int64_t)h[c & (VOC_HN - 1u)] * 16384;
    for (j = 0; j < VOC_HB; j++)
        a += (int64_t)VOC_HBT[j] * (h[(c - 2u * j - 1u) & (VOC_HN - 1u)] + h[(c + 2u * j + 1u) & (VOC_HN - 1u)]);
    return (int32_t)((a + (1 << 14)) >> 15);
}

static void voc_reset(void)              /* the synthesis afresh (the state changed): no old ringing */
{
    memset(&voc, 0, sizeof voc);
}
static void voc_ana_reset(void)          /* the analysis afresh (fx.c: as it starts) */
{
    memset(&voa, 0, sizeof voa);
    memset(voc_ab, 0, sizeof voc_ab);
}

/* core 0, before core 1's job (fx.c mix_block): the analysis writes the other buffer this block */
static void voc_ana_flip(void) { voc_aw ^= 1u; }
/* core 0, while core 1 renders: this block's voice (voc_in, n even) through the analysis bank into voc_ab[voc_aw] */
static void voc_analyse(uint32_t n)
{
    uint32_t i, k, j = 0;
    int32_t pin = voa.peak_in;
    for (i = 0; i + 1u < n && i + 1u < VOC_MAXN; i += 2u, j++) {
        int32_t v, hf = 0;
        for (k = 0; k < 2u; k++) {
            int32_t x = voc_in[i + k], ax = x < 0 ? -x : x;
            if (ax > pin)
                pin = ax;
            voa.vh[voa.hw++ & (VOC_HN - 1u)] = x;
        }
        v = voc_dec(voa.vh, voa.hw) << 4;
        for (k = 0; k < VOC_NB; k++) {
            int32_t ya = voc_sec(v, &voa.ax1[k], &voa.ax2[k], &voa.ay1[k], &voa.ay2[k], k);
            ya = voc_sec(ya, &voa.bx1[k], &voa.bx2[k], &voa.by1[k], &voa.by2[k], k);
            voa.env[k] = voc_env(voa.env[k], ya);        /* (ya: Q19) */
            voc_ab[voc_aw].env[j][k] = voa.env[k] >> 4;  /* (Q15) */
            if (k >= VOC_HF0)
                hf += ya;                                /* the speech addition: the voice's top bands */
        }
        voc_ab[voc_aw].hf[j] = hf;
    }
    voa.peak_in = pin;
}

/* one vocoded sample at 22.05 kHz from the carrier c (the track's scale), j the sample's place in the block: the
 * analysis's envelopes of the block before */
static int32_t voc_tick(int32_t c, uint32_t j)
{
    const int32_t *env = voc_ab[voc_aw ^ 1u].env[j];
    int64_t sum = 0;
    int32_t o;
    uint32_t k;
    c <<= 4;
    for (k = 0; k < VOC_NB; k++) {
        int32_t ys = voc_sec(c, &voc.sx1[k], &voc.sx2[k], &voc.sy1[k], &voc.sy2[k], k);
        ys = voc_sec(ys, &voc.tx1[k], &voc.tx2[k], &voc.ty1[k], &voc.ty2[k], k);
        sum += (int64_t)ys * env[k];                     /* x the envelope, Q15 (ys: the carrier's scale << 4) */
    }
    o = (int32_t)(((sum >> 19) * VOC_LV_Q8[voc_level % VOC_LEVEL_N]) >> 8);
    if (voc_hf)
        o += (int32_t)(((int64_t)VOC_VS(voc_ab[voc_aw ^ 1u].hf[j] >> 4) * VOC_MIX_Q15[voc_hf % VOC_MIX_N]) >> 15);
    return o;
}

/* the carrier block b (the track's scale, n even) becomes the vocoded block, in place */
static void voc_process(int32_t *b, uint32_t n)
{
    uint32_t i, j;
    int32_t pout = voc.peak_out, dg = VOC_MIX_Q15[voc_dry % VOC_MIX_N];
    voc.blocks++;
    for (i = 0; i + 1u < n && i + 1u < VOC_MAXN; i += 2u) {
        int32_t o[2], d, k;
        int64_t a = 0;
        for (k = 0; k < 2; k++)
            voc.ch[voc.hw++ & (VOC_HN - 1u)] = b[i + (uint32_t)k];
        d = voc_tick(voc_dec(voc.ch, voc.hw), i >> 1);
        voc.oh[voc.ow++ & (VOC_ON - 1u)] = d;
        /* 2:1 up: the even sample the one VOC_HB back, the odd one the half-band between it and the next */
        o[0] = voc.oh[(voc.ow - 1u - VOC_HB) & (VOC_ON - 1u)];
        for (j = 0; j < VOC_HB; j++)
            a += (int64_t)VOC_HBT[j] * (voc.oh[(voc.ow - 1u - VOC_HB - j) & (VOC_ON - 1u)] +
                                        voc.oh[(voc.ow - VOC_HB + j) & (VOC_ON - 1u)]);
        o[1] = (int32_t)((a + (1 << 13)) >> 14);         /* (x 2: the zeros between) */
        for (k = 0; k < 2; k++) {
            int32_t x = o[k];
            if (dg)
                x += (int32_t)(((int64_t)VOC_VS(voc_in[i + (uint32_t)k]) * dg) >> 15);   /* DRY */
            x = x > VOC_XMAX ? VOC_XMAX : x < -VOC_XMAX ? -VOC_XMAX : x;
            if ((x < 0 ? -x : x) > pout)
                pout = x < 0 ? -x : x;
            b[i + (uint32_t)k] = x;
        }
    }
    voc.peak_out = pout;
}

/* voice.c track_render, track 3, at its start: the state to render this block. A change fades the old state out first
 * (voc_fade), then switches (the vocoder starts afresh) and fades the new one in */
static uint32_t voc_state(void)
{
    uint32_t req = VOC_AUDIO_ON ? 1u + voc_mode % VM_N : 0u;
    if (req != voc_cur && voc_xg == 0) {
        voc_cur = (uint8_t)req;
        voc_reset();
    }
    return voc_cur;
}
static void voc_dry_block(int32_t *out, uint32_t n)      /* MODE OFF: the voice itself */
{
    uint32_t i;
    for (i = 0; i < n && i < VOC_MAXN; i++)
        out[i] = VOC_VS(voc_in[i]);
}
/* .. at its end: the fade (~5 ms either way: 148 a sample of Q15); out of the way when settled */
static void voc_fade(int32_t *out, uint32_t n)
{
    uint32_t req = VOC_AUDIO_ON ? 1u + voc_mode % VM_N : 0u, i;
    int32_t tgt = req == voc_cur ? 32767 : 0;
    if (voc_xg == tgt && tgt)
        return;
    for (i = 0; i < n; i++) {
        voc_xg = voc_xg < tgt ? (voc_xg + 148 > tgt ? tgt : voc_xg + 148) : (voc_xg - 148 < tgt ? tgt : voc_xg - 148);
        out[i] = (int32_t)(((int64_t)out[i] * voc_xg) >> 15);
    }
}
