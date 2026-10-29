/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the SMK-37 vocoder (firmware/src/smk_vocoder.h; the playback probe's, SMK_UAC_IN_PROBE):
 *   bands      the bank (4th order: two sections, at 22050 Hz) 0 dB at each of the VSM201's 20 centres, neighbours
 *              crossing near -3 dB between theirs, two octaves away at least 20 dB down
 *   vocoding   a sawtooth carrier (110 Hz) shaped by a 1 kHz voice: its 9th harmonic (990 Hz) in the output > 15 dB
 *              over its fundamental (110 Hz), > 40 dB over its 51st (5610 Hz), each measured exactly (Goertzel); silence in,
 *              silence out; LEVEL +3 dB a step; DRY adds the voice
 *   voicing    white noise unvoiced (> 80 % of blocks), a vowel-like voice (harmonics of 150 Hz falling 12 dB an octave)
 *              voiced (< 5 %); with SIBILANCE the noise reaches the output's top band, without it does not
 * 44.1 kHz, blocks of 32 as the mix. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define VOC_MAXN 32u
#include "../firmware/src/smk_vocoder.h"

static int fails;
static void check(const char *what, int ok)
{
    printf("%-96s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

#define FS 44100.0
static double db(double x) { return 20.0 * log10(x > 1e-12 ? x : 1e-12); }

static double outbuf[66214];                      /* (1.5 s at 44.1 kHz)
 */
/* one band's gain at f (dB): a sine (x 16 as the vocoder runs them) through its two sections at 22050 Hz, its peak
 * after it settles */
static double band_gain(uint32_t k, double f)
{
    int32_t x1 = 0, x2 = 0, y1 = 0, y2 = 0, u1 = 0, u2 = 0, v1 = 0, v2 = 0, pk = 0;
    uint32_t i, n = VOC_FS;
    for (i = 0; i < n; i++) {
        int32_t x = (int32_t)lrint(16.0 * 16000.0 * sin(2.0 * M_PI * f * i / VOC_FS)), y;
        y = voc_sec(x, &x1, &x2, &y1, &y2, k);
        y = voc_sec(y, &u1, &u2, &v1, &v2, k);
        if (i > n - VOC_FS / 10u && (y < 0 ? -y : y) > pk)
            pk = y < 0 ? -y : y;
    }
    return db(pk / (16.0 * 16000.0));
}
/* the output's amplitude at f exactly (Goertzel over the last 0.5 s: a whole number of 110 Hz periods), dB re 1 */
static double tone(uint32_t n, double f)
{
    uint32_t m = (uint32_t)lrint(FS * 0.5 / (FS / 110.0)) * (uint32_t)lrint(FS / 110.0), i;
    double w = 2.0 * M_PI * f / FS, c = 2.0 * cos(w), s1 = 0, s2 = 0, re, im;
    for (i = n - m; i < n; i++) {
        double s0 = outbuf[i] + c * s1 - s2;
        s2 = s1, s1 = s0;
    }
    re = s1 - s2 * cos(w), im = s2 * sin(w);
    return db(2.0 * sqrt(re * re + im * im) / m);
}

static void reset(void)
{
    uint32_t r = voc.rnd;
    memset(&voc, 0, sizeof voc);
    voc.rnd = r | 1u;
}

/* run the vocoder for secs: voice(i) and carrier(i) per sample; the output into a buffer (returned length) */
static uint32_t run(double secs, double (*voice)(uint32_t), double (*carrier)(uint32_t))
{
    uint32_t i = 0, n = (uint32_t)(FS * secs), k;
    int32_t b[VOC_MAXN];
    while (i < n) {
        for (k = 0; k < VOC_MAXN; k++) {
            voc_in[k] = (int16_t)lrint(voice(i + k));
            b[k] = (int32_t)lrint(carrier(i + k));
        }
        voc_process(b, VOC_MAXN);
        for (k = 0; k < VOC_MAXN && i + k < n; k++)
            outbuf[i + k] = b[k];
        i += VOC_MAXN;
    }
    return n;
}
static double rms_last(uint32_t n, double secs)
{
    double s = 0;
    uint32_t i, m = 0;
    for (i = n - (uint32_t)(FS * secs); i < n; i++)
        s += outbuf[i] * outbuf[i], m++;
    return sqrt(s / m);
}

/* the output's level above ~4 kHz (dB): a 2nd-order high-pass (RBJ, 4 kHz, Q 0.707) over the last 0.5 s */
static double hiss(uint32_t n)
{
    double w = 2.0 * M_PI * 4000.0 / FS, al = sin(w) / (2.0 * 0.7071), a0 = 1.0 + al;
    double b0 = (1.0 + cos(w)) / 2.0 / a0, b1 = -(1.0 + cos(w)) / a0, a1 = -2.0 * cos(w) / a0, a2 = (1.0 - al) / a0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0, s = 0;
    uint32_t i, m = 0;
    for (i = 0; i < n; i++) {
        double y = b0 * outbuf[i] + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
        x2 = x1, x1 = outbuf[i], y2 = y1, y1 = y;
        if (i >= n - (uint32_t)(FS * 0.5))
            s += y * y, m++;
    }
    return db(sqrt(s / m));
}

static double saw110(uint32_t i) { double p = fmod(110.0 * i / FS, 1.0); return 8000.0 * (2.0 * p - 1.0); }
static double sine220(uint32_t i) { return 8000.0 * sin(2.0 * M_PI * 220.0 * i / FS); }
static double v1k(uint32_t i) { return 16000.0 * sin(2.0 * M_PI * 1000.0 * i / FS); }
static double vzero(uint32_t i) { (void)i; return 0.0; }
static uint32_t nrnd = 12345u;
static double vnoise(uint32_t i)
{
    (void)i;
    nrnd = nrnd * 1664525u + 1013904223u;
    return (double)(int16_t)(nrnd >> 16) * 0.25;
}
static double vowel(uint32_t i)                   /* harmonics of 150 Hz, -12 dB an octave, to 4 kHz */
{
    double s = 0;
    int h;
    for (h = 1; 150 * h < 4000; h++)
        s += sin(2.0 * M_PI * 150.0 * h * i / FS) / ((double)h * h);
    return 9000.0 * s;
}

int main(void)
{
    uint32_t k, n, worst_k = 0;
    double worst = 0, cross_lo = 99, cross_hi = -99, skirt = -99;
    printf("-- the SMK-37 vocoder (the VSM201's 20 bands, 4th order at 22050 Hz)\n");
    for (k = 0; k < VOC_NB; k++) {
        double g = band_gain(k, VOC_FC[k]), lo = band_gain(k, VOC_FC[k] / 4.0);
        if (fabs(g) > fabs(worst))
            worst = g, worst_k = k;
        if (k + 1u < VOC_NB && k > 0u) {               /* (the end bands: one-sided widths) */
            double mid = sqrt((double)VOC_FC[k] * VOC_FC[k + 1u]);
            double a = band_gain(k, mid), b = band_gain(k + 1u, mid);
            cross_lo = a < cross_lo ? a : cross_lo, cross_hi = a > cross_hi ? a : cross_hi;
            cross_lo = b < cross_lo ? b : cross_lo, cross_hi = b > cross_hi ? b : cross_hi;
        }
        skirt = lo > skirt ? lo : skirt;               /* two octaves down (up: past the top for the high bands) */
        if (VOC_FC[k] * 4u < VOC_FS / 2u) {
            double hi = band_gain(k, VOC_FC[k] * 4.0);
            skirt = hi > skirt ? hi : skirt;
        }
    }
    printf("   centre gains within %.3f dB (band %u); neighbours at their midpoints %.1f .. %.1f dB;"
           " two octaves away at most %.1f dB\n", worst, worst_k + 1u, cross_lo, cross_hi, skirt);
    check("bands: 0 dB at each of the 20 VSM201 centres (+-0.1 dB)", fabs(worst) < 0.1);
    check("bands: neighbours cross between -4.5 and -1.5 dB (the VSM201's curves meet near their tops)",
          cross_lo > -4.5 && cross_hi < -1.5);
    check("bands: two octaves away at least 20 dB down (a band stays its own)", skirt < -20.0);

    voc_level = 4, voc_dry = 0, voc_sib = 1;
    reset();
    n = run(1.0, v1k, saw110);
    {
        double near = tone(n, 990.0), lo = tone(n, 110.0), hi = tone(n, 5610.0);
        printf("   1 kHz voice on a 110 Hz saw: 990 Hz %.1f dB, 110 Hz %.1f dB, 5610 Hz %.1f dB\n", near, lo, hi);
        check("vocoding: the harmonic where the voice is (990 Hz) > 15 dB over the fundamental, > 40 dB over 5610 Hz",
              near - lo > 15.0 && near - hi > 40.0);    /* (the fundamental: band 1 is broad, as the VSM201's K1: a 1 kHz
                                                         * tone opens it ~35 dB down, and a saw's 110 Hz is 19 dB over its
                                                         * 9th harmonic; speech spreads over every band) */
        voc_level = 5;
        reset();
        n = run(1.0, v1k, saw110);
        check("LEVEL: one step +3 dB (+-0.2)", fabs(tone(n, 990.0) - near - 3.0) < 0.2);
        voc_level = 4;
    }
    reset();
    n = run(1.0, vzero, saw110);
    check("silence in, silence out (the carrier alone: < 1)", rms_last(n, 0.5) < 1.0);
    voc_dry = 2;
    reset();
    n = run(1.0, v1k, vzero);
    check("DRY -6 dB: the voice on its own (no carrier) at half its level (+-0.1 dB)",
          fabs(db(rms_last(n, 0.5) / (16000.0 / sqrt(2.0))) + 6.02) < 0.1);
    voc_dry = 0;

    reset();
    run(1.0, vnoise, sine220);
    {
        uint32_t uv = voc.blocks ? voc.uv_blocks * 100u / voc.blocks : 0;
        printf("   white noise: unvoiced %u %% of blocks\n", uv);
        check("voicing: white noise unvoiced (> 80 % of blocks)", uv > 80u);
    }
    reset();
    run(1.0, vowel, sine220);
    {
        uint32_t uv = voc.blocks ? voc.uv_blocks * 100u / voc.blocks : 0;
        printf("   a vowel (150 Hz, -12 dB an octave): unvoiced %u %% of blocks\n", uv);
        check("voicing: a vowel voiced (< 5 % of blocks unvoiced)", uv < 5u);
    }
    {
        double on, off;
        voc_sib = 1;
        reset();
        n = run(1.0, vnoise, sine220);
        on = hiss(n);
        voc_sib = 0;
        reset();
        n = run(1.0, vnoise, sine220);
        off = hiss(n);
        printf("   noise voice on a 220 Hz sine: above 4 kHz %.1f dB with SIBILANCE, %.1f dB without\n", on, off);
        check("SIBILANCE: the noise carries the hiss above 4 kHz (> 20 dB more than without)", on - off > 20.0);
        voc_sib = 1;
    }
    printf(fails ? "VOCODER TEST FAILED (%d)\n" : "smk_vocoder: all ok\n", fails);
    return fails != 0;
}
