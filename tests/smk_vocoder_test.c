/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the SMK-37 audio track and vocoder (firmware/src/smk_vocoder.h; the vocoder build, SMK_UAC_IN_PROBE):
 *   bands      the bank (4th order: two sections, at 22050 Hz) 0 dB at each of the VSM201's 20 centres, neighbours
 *              crossing near -3 dB between theirs, two octaves away at least 20 dB down
 *   vocoding   a sawtooth carrier (110 Hz) shaped by a 1 kHz voice: its 9th harmonic (990 Hz) in the output > 15 dB
 *              over its fundamental (110 Hz), > 40 dB over its 51st (5610 Hz), each measured exactly (Goertzel); silence in,
 *              silence out; LEVEL +3 dB a step; DRY adds the voice (as one voice at full scale, x 2 VOICE_FS)
 *   HF         the speech addition: a noise voice puts > 20 dB more above 4 kHz into the output than without it
 *   (the tests above with VOICE = VOICED: the bank alone)
 *   VOICE      the VSM201's detector: white noise unvoiced (> 80 % of blocks), a vowel-like voice voiced (< 5 %); AUTO on
 *              a noise voice puts > 10 dB more above 4 kHz out than VOICED (the noise replaces the saw); the noise is
 *              the carrier's level-matched: no carrier, no output (UNVOICED, white and pink)
 *   BRIDGE     no voice, a saw: BRIDGE 0 dB plays the saw on (within 6 dB of it), OFF nothing; a loud voice
 *              (-10 dBFS, THRESHOLD -42) leaves no bridging (the same output +-0.1 dB)
 *   TONE       BRIDGE TONE: COLOUR 0, the bands rest in a pause and run again with the voice (the voice after it as at
 *              100 %); COLOUR 100 % puts a voice under THRESHOLD on the bridge, 0 does not; FILL between the bridge alone
 *              and SUM; TILT +3 dB an octave lifts the bridge's top and drops its bottom
 *   NORMAL     the VSM201's weights (B11 / B12): ch1 10.6 dB over ch20, folded into the synthesis bank
 *   AGC        OFF (the default): a three-note chord untouched; FULL: one note at a voice's level untouched, a
 *              three-note chord levelled to the ceiling (VOC_AGC_TQ)
 *   zipper     a voice gated as speech goes, BRIDGE 0 dB, TILT -3: the output above 9 kHz (the bank stops at 6.78) > 60 dB
 *              under it (the bridge and COLOUR ramped over a block: 62.7 dB; stepped block by block they made 54.5)
 *   the track  MODE AUDIO: the voice itself, at a voice's full scale; a change of state (engine -> dry -> engine) fades out
 *              and in: no step over 200 a sample, settled within 15 blocks
 * 44.1 kHz, blocks of 32 as the mix. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define VOC_MAXN 32u
#define SMK_UAC_PROBE 0
#define VOICE_FS 24000                             /* (dsp.c) */
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
        y = voc_sec(x, VOC_B0[k], &x1, &x2, &y1, &y2, k);
        y = voc_sec(y, VOC_B0[k], &u1, &u2, &v1, &v2, k);
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

static void reset(void) { voc_reset(); voc_ana_reset(); }

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
        voc_ana_flip();                            /* (as fx.c mix_block: core 0's analysis, then track 3's synthesis
                                                    * on the envelopes of the block before) */
        voc_analyse(VOC_MAXN);
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

/* the output's rms above ~9 kHz over the last 0.5 s: two 2nd-order high-passes (RBJ, 9 kHz, Q 0.707), linear */
static double above9k(uint32_t n)
{
    double w = 2.0 * M_PI * 9000.0 / FS, al = sin(w) / (2.0 * 0.7071), a0 = 1.0 + al;
    double b0 = (1.0 + cos(w)) / 2.0 / a0, b1 = -(1.0 + cos(w)) / a0, a1 = -2.0 * cos(w) / a0, a2 = (1.0 - al) / a0;
    double x[2][2] = {{0}}, y[2][2] = {{0}}, s = 0;
    uint32_t i, k, m = 0;
    for (i = 0; i < n; i++) {
        double v = outbuf[i];
        for (k = 0; k < 2u; k++) {
            double o = b0 * v + b1 * x[k][0] + b0 * x[k][1] - a1 * y[k][0] - a2 * y[k][1];
            x[k][1] = x[k][0], x[k][0] = v, y[k][1] = y[k][0], y[k][0] = o;
            v = o;
        }
        if (i >= n - (uint32_t)(FS * 0.5))
            s += v * v, m++;
    }
    return sqrt(s / m);
}
/* a 1 kHz voice as speech goes: -20 dBFS for 70 ms, then silent for 80 ms (the pause value swings every block) */
static double vwobble(uint32_t i)
{
    double t = (double)i / FS;
    return fmod(t, 0.15) < 0.07 ? 3277.0 * sin(2.0 * M_PI * 1000.0 * t) : 0.0;
}

static double saw110(uint32_t i) { double p = fmod(110.0 * i / FS, 1.0); return 8000.0 * (2.0 * p - 1.0); }
static double sine220(uint32_t i) { return 8000.0 * sin(2.0 * M_PI * 220.0 * i / FS); }
static double v1k(uint32_t i) { return 16000.0 * sin(2.0 * M_PI * 1000.0 * i / FS); }
static double vzero(uint32_t i) { (void)i; return 0.0; }
static double vquiet(uint32_t i) { return 184.0 * sin(2.0 * M_PI * 1000.0 * i / FS); }   /* -45 dBFS */
/* a carrier voice as voice.c voc_saw makes it: peak VOICE_FS x 2 (mean |x| VOICE_FS / 2... x 2: 24000), at velocity
 * ~85 two thirds of that; one, and three (a major chord) */
static double sawv(double f, uint32_t i) { double p = fmod(f * i / FS, 1.0); return 32000.0 * (2.0 * p - 1.0); }
static double saw24k(uint32_t i) { return sawv(110.0, i); }
static double chord3(uint32_t i) { return sawv(110.0, i) + sawv(138.6, i) + sawv(164.8, i); }
static uint32_t nrnd = 12345u;
static double vowel(uint32_t i)                   /* harmonics of 150 Hz, -12 dB an octave, to 4 kHz */
{
    double s = 0;
    int h;
    for (h = 1; 150 * h < 4000; h++)
        s += sin(2.0 * M_PI * 150.0 * h * i / FS) / ((double)h * h);
    return 9000.0 * s;
}
static double vnoise(uint32_t i)
{
    (void)i;
    nrnd = nrnd * 1664525u + 1013904223u;
    return (double)(int16_t)(nrnd >> 16) * 0.25;
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

    voc_level = 4, voc_dry = 0, voc_hf = 0, voc_vmode = VV_VOICED;
    reset();
    n = run(1.0, v1k, saw110);
    {
        double near = tone(n, 990.0), lo = tone(n, 110.0), hi = tone(n, 5610.0);
        printf("   1 kHz voice on a 110 Hz saw: 990 Hz %.1f dB, 110 Hz %.1f dB, 5610 Hz %.1f dB\n", near, lo, hi);
        double nw = 20.0 * log10((double)VOC_NORM_Q12[0] / VOC_NORM_Q12[7]);   /* (NORMAL: band 1 over band 8, 980 Hz) */
        check("vocoding: the harmonic where the voice is (990 Hz) > 15 dB over the fundamental (NORMAL's lift on band 1"
              " aside), > 40 dB over 5610 Hz",
              near - lo + nw > 15.0 && near - hi > 40.0);   /* (the fundamental: band 1 is broad, as the VSM201's K1: a 1 kHz
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
    voc_dry = 3;                                    /* -6 dB (OFF, -18, -12, -6, 0) */
    reset();
    n = run(1.0, v1k, vzero);
    printf("   DRY -6 dB: %.2f dB re a voice's full scale\n", db(rms_last(n, 0.5) / (16000.0 * 2.0 * VOICE_FS / 32768.0 / sqrt(2.0))));
    check("DRY -6 dB: the voice on its own (no carrier), a voice's full scale halved (+-0.1 dB)",
          fabs(db(rms_last(n, 0.5) / (16000.0 * 2.0 * VOICE_FS / 32768.0 / sqrt(2.0))) + 6.02) < 0.1);
    voc_dry = 0;
    {
        double on, off;
        voc_hf = 2;                                     /* -12 dB */
        reset();
        n = run(1.0, vnoise, sine220);
        on = hiss(n);
        voc_hf = 0;
        reset();
        n = run(1.0, vnoise, sine220);
        off = hiss(n);
        printf("   noise voice on a 220 Hz sine: above 4 kHz %.1f dB with HF (-12 dB), %.1f dB without\n", on, off);
        check("HF: the speech addition carries the voice's top above 4 kHz (> 20 dB more than without)", on - off > 20.0);
    }
    {   /* the VSM201's voice mode detector and the noise replacement */
        double a, v;
        voc_hf = 0;
        voc_vmode = VV_AUTO;
        reset();
        voa.blocks = voa.uv_blocks = 0;
        run(1.0, vnoise, saw110);
        a = voa.blocks ? 100.0 * voa.uv_blocks / voa.blocks : 0;
        reset();
        voa.blocks = voa.uv_blocks = 0;
        run(1.0, vowel, saw110);
        v = voa.blocks ? 100.0 * voa.uv_blocks / voa.blocks : 0;
        printf("   detector: white noise unvoiced %.0f %% of blocks, a vowel (150 Hz, -12 dB an octave) %.0f %%\n", a, v);
        check("VOICE: the detector calls white noise unvoiced (> 80 %), a vowel voiced (< 5 % unvoiced)", a > 80.0 && v < 5.0);
        reset();
        n = run(1.0, vnoise, saw110);
        a = hiss(n);
        voc_vmode = VV_VOICED;
        reset();
        n = run(1.0, vnoise, saw110);
        v = hiss(n);
        printf("   a noise voice on a 110 Hz saw: above 4 kHz %.1f dB AUTO, %.1f dB VOICED\n", a, v);
        check("VOICE AUTO: on unvoiced sound the noise replaces the saws (> 10 dB more above 4 kHz than VOICED)",
              a - v > 10.0);
        {   /* 2026-11-03: NOISE SOFT (-6 dB an octave), NOISE BLEND (AUTO: the noise's share) */
            double sw, bl;
            voc_vmode = VV_AUTO, voc_pink = 2;
            reset();
            n = run(1.0, vnoise, saw110);
            sw = hiss(n);
            voc_pink = 0, vpg.nzc = 100;                /* BLEND 0 %: no noise */
            reset();
            n = run(1.0, vnoise, saw110);
            bl = hiss(n);
            vpg.nzc = 0;
            printf("   above 4 kHz: WHITE %.1f dB, SOFT %.1f dB, BLEND 0 %% %.1f dB (VOICED %.1f)\n", a, sw, bl, v);
            check("NOISE SOFT: less above 4 kHz than WHITE (> 6 dB)", a - sw > 6.0);
            check("NOISE BLEND 0 %: as VOICED above 4 kHz (+-3 dB)", fabs(bl - v) < 3.0);
        }
        voc_vmode = VV_UNVOICED;
        reset();
        n = run(1.0, vnoise, vzero);
        a = rms_last(n, 0.5);
        voc_pink = 1;
        reset();
        n = run(1.0, vnoise, vzero);
        v = rms_last(n, 0.5);
        voc_pink = 0;
        check("VOICE UNVOICED: the noise is the carrier's level-matched: no carrier, no output (white, pink: < 1)",
              a < 1.0 && v < 1.0);
        voc_vmode = VV_VOICED;
    }
    {   /* the silence bridging */
        double on, off, ref = 8000.0 / sqrt(3.0);       /* (the saw's rms) */
        voc_bridge = VOC_BR_N - 1u;                     /* 0 dB */
        reset();
        n = run(1.0, vzero, saw110);
        on = rms_last(n, 0.5);
        voc_bridge = 0;
        reset();
        n = run(1.0, vzero, saw110);
        off = rms_last(n, 0.5);
        printf("   no voice, a 110 Hz saw: BRIDGE 0 dB %.1f dB re the saw, OFF %.1f\n", db(on / ref), off);
        check("BRIDGE: in a pause the saws play on (0 dB: within 6 dB of the saw), OFF nothing (< 1)",
              fabs(db(on / ref)) < 6.0 && off < 1.0);
        voc_bridge = VOC_BR_N - 1u;
        reset();
        n = run(1.0, v1k, saw110);
        on = rms_last(n, 0.5);
        voc_bridge = 0;
        reset();
        n = run(1.0, v1k, saw110);
        off = rms_last(n, 0.5);
        check("BRIDGE: a loud voice (-10 dBFS, THRESHOLD -42 dB) is not bridged (the same output, +-0.1 dB)",
              fabs(db(on / off)) < 0.1);
    }
    {   /* BRIDGE TONE > COLOUR 0 (the bands rest in a long pause) against 100 %: a voice, a pause, the voice again */
        double t_rest = 0, t_all = 0, b_rest = 0, b_all = 0;
        uint32_t rest_gap, rest_voice, k;
        for (k = 0; k < 2u; k++) {
            voc_colour = (uint8_t)(k == 0u ? 0u : 10u);
            voc_bridge = VOC_BR_N - 1u;                 /* (0 dB: the pause is heard, bridged) */
            reset();
            run(0.3, v1k, saw110);
            n = run(0.2, vzero, saw110);
            rest_gap = voa.rest;
            *(k ? &b_all : &b_rest) = rms_last(n, 0.1);
            n = run(0.6, v1k, saw110);                  /* (tone: the last 0.5 s) */
            rest_voice = voa.rest;
            *(k ? &t_all : &t_rest) = tone(n, 990.0);
            if (!k)
                check("COLOUR 0: the bands rest in a pause (0.2 s), run again with the voice", rest_gap && !rest_voice);
            else
                check("COLOUR 100 %: the bands never rest", !rest_gap && !rest_voice);
        }
        printf("   after a pause, 990 Hz: COLOUR 0 %.2f dB, 100 %% %.2f dB; a silent bridged pause: %.1f, %.1f\n",
               t_rest, t_all, b_rest, b_all);
        check("COLOUR 0: the voice after a pause vocoded as at 100 % (990 Hz, +-0.5 dB)", fabs(t_rest - t_all) < 0.5);
        check("COLOUR 0: a silent bridged pause as at 100 % (nothing to colour it, +-0.5 dB)", fabs(db(b_rest / b_all)) < 0.5);
        voc_bridge = 0;
        voc_colour = 0;
        reset();
        run(0.3, v1k, saw110);
        n = run(0.2, vzero, saw110);
        voc_colour = 10;
        check("COLOUR 0, BRIDGE OFF: a resting pause silent (< 1)", voa.rest && rms_last(n, 0.1) < 1.0);
    }
    {   /* BRIDGE TONE in a pause with something under THRESHOLD (a 1 kHz voice at -45 dBFS; THRESHOLD -42): COLOUR puts
         * its band on the bridge, 0 leaves the bridge alone; FILL the larger of the two, SUM both */
        double c0, c100, cf, r0;
        voc_bridge = VOC_BR_N - 13u;                    /* -36 dB (the voice's band over it, the others under) */
        voc_tilt = VOC_TILT_FLAT;
        voc_colour = 0;
        reset();
        n = run(1.0, vquiet, saw110);
        c0 = tone(n, 990.0), r0 = rms_last(n, 0.5);
        voc_colour = 10;
        reset();
        n = run(1.0, vquiet, saw110);
        c100 = tone(n, 990.0);
        voc_blend = 1;
        reset();
        n = run(1.0, vquiet, saw110);
        cf = tone(n, 990.0);
        voc_blend = 0;
        printf("   a -45 dBFS voice in a pause, BRIDGE -36 dB, at 990 Hz: COLOUR 0 %.1f dB; 100 %% SUM %.1f dB, FILL %.1f dB\n",
               c0, c100, cf);
        check("COLOUR 100 %: the input under THRESHOLD colours the bridge (990 Hz > 3 dB over COLOUR 0)", c100 - c0 > 3.0);
        check("BLEND: FILL the larger of the two a band, not both (the voice's band: over the bridge alone, under SUM)",
              cf > c0 + 3.0 && cf < c100 - 0.5 && r0 > 0.0);
    }
    {   /* TILT: the bridge alone (no voice, BRIDGE 0 dB) at +3 dB an octave against FLAT: more top, less bottom */
        double fl_hi, fl_lo, up_hi, up_lo;
        voc_bridge = VOC_BR_N - 1u;
        voc_tilt = VOC_TILT_FLAT;
        reset();
        n = run(1.0, vzero, saw110);
        fl_hi = hiss(n), fl_lo = tone(n, 110.0);
        voc_tilt = VOC_TILT_N - 1u;
        reset();
        n = run(1.0, vzero, saw110);
        up_hi = hiss(n), up_lo = tone(n, 110.0);
        voc_tilt = VOC_TILT_FLAT + 2u;
        voc_bridge = 0;
        printf("   the bridge alone, TILT +3 dB/oct against FLAT: above 4 kHz %+.1f dB, 110 Hz %+.1f dB\n",
               up_hi - fl_hi, up_lo - fl_lo);
        check("TILT +3 dB an octave: the bridge's top up (> 4 dB above 4 kHz), its bottom down (> 5 dB at 110 Hz)",
              up_hi - fl_hi > 4.0 && fl_lo - up_lo > 5.0);
    }
    {   /* no zipper: a voice rising and falling through THRESHOLD (the pause value moving every block), BRIDGE 0 dB, TILT
         * -3 dB an octave (the most bridge where the saw is strongest): what the output has above 9 kHz (the bank stops
         * at 6.78 kHz: there only the steps of a gain changed block by block) against its level */
        double z, r, z0, r0;
        voc_bridge = VOC_BR_N - 1u;
        voc_tilt = 0;
        voc_norm = 1;                                   /* (as measured for the ramp: 62.7 dB, stepped 54.5) */
        reset();
        n = run(1.0, vwobble, saw110);
        z = above9k(n), r = rms_last(n, 0.5);
        voc_norm = 0;                                   /* NORMAL OFF: the top band 6 dB up, the ramp's corners with it */
        reset();
        n = run(1.0, vwobble, saw110);
        z0 = above9k(n), r0 = rms_last(n, 0.5);
        voc_tilt = VOC_TILT_FLAT + 2u;
        voc_bridge = 0;
        printf("   a voice through THRESHOLD, BRIDGE 0 dB, TILT -3: above 9 kHz %.1f dB under the output (NORMAL OFF %.1f)\n",
               db(r / z), db(r0 / z0));
        check("BRIDGE TONE: no zipper (the bridge and COLOUR ramped over a block): above 9 kHz > 60 dB under the output",
              db(r / z) > 60.0);
        check("BRIDGE TONE, NORMAL OFF: above 9 kHz > 50 dB under the output (measured 52.4)", db(r0 / z0) > 50.0);
    }
    {   /* NORMAL (the VSM201's B11 / B12 weights, rms 1) and the carrier AGC */
        double d = 20.0 * log10((double)VOC_NORM_Q12[0] / VOC_NORM_Q12[VOC_NB - 1u]), worst = 0;
        uint32_t k;
        for (k = 0; k < VOC_NB; k++) {
            double r = (double)VOC_B0S[k] / VOC_B0[k] - VOC_NORM_Q12[k] / 4096.0;
            if (fabs(r) > worst)
                worst = fabs(r);
        }
        printf("   NORMAL: ch1 over ch20 %.2f dB (9.53k / 2.94k against 9.53k / 10.0k: 10.63)\n", d);
        check("NORMAL: ch1 10.6 dB over ch20 (+-0.1), each folded into the synthesis bank's first section",
              fabs(d - 10.63) < 0.1 && worst < 0.001);
        {   /* the switch: the bridge alone (no voice) on a saw, NORMAL ON against OFF (the default) */
            double lo0, hi0, lo1, hi1;
            uint32_t tl = voc_tilt;
            voc_bridge = VOC_BR_N - 1u, voc_tilt = VOC_TILT_FLAT;
            reset();
            n = run(1.0, vzero, saw110);
            lo0 = tone(n, 220.0), hi0 = tone(n, 5500.0);
            voc_norm = 1;
            reset();
            n = run(1.0, vzero, saw110);
            lo1 = tone(n, 220.0), hi1 = tone(n, 5500.0);
            voc_norm = 0, voc_tilt = (uint8_t)tl;
            printf("   NORMAL ON against OFF, the bridge on a saw: 220 Hz %+.1f dB, 5500 Hz %+.1f dB\n", lo1 - lo0, hi1 - hi0);
            check("NORMAL: OFF the default (flat); ON lifts the bottom (220 Hz > +2 dB) and drops the top (5.5 kHz < -4 dB)",
                  lo1 - lo0 > 2.0 && hi1 - hi0 < -4.0);
        }
        voc_bridge = 0;
        reset();
        run(1.0, v1k, chord3);
        check("AGC OFF (the default): a three-note chord untouched (gain 1.0)", voc_agc == 0 && voc.agc_g == 32767);
        voc_agc = VOC_AGC_N - 1u;                       /* FULL */
        reset();
        run(1.0, v1k, saw24k);
        check("AGC FULL: one note at a voice's level untouched (gain 1.0)", voc.agc_g == 32767);
        reset();
        run(1.0, v1k, chord3);
        printf("   AGC FULL on a three-note chord (3 x a voice): gain %.2f dB, the carrier's level after it %.0f (ceiling %d)\n",
               db(voc.agc_g / 32767.0), (double)voc.agc_env * voc.agc_g / 32767.0, (int)VOC_AGC_TQ[voc_agc]);
        check("AGC FULL: a chord levelled to the ceiling (down to -10 %: the gain drops fast, recovers slowly), never above it",
              voc.agc_g < 32767 && (double)voc.agc_env * voc.agc_g / 32767.0 <= VOC_AGC_TQ[voc_agc] * 1.01 &&
              (double)voc.agc_env * voc.agc_g / 32767.0 > VOC_AGC_TQ[voc_agc] * 0.9);
        voc_agc = 0;
    }
    {   /* the audio track: engine (a DC 30000) -> AUDIO IN on, MODE OFF (the voice: DC 10000) -> off again */
        int32_t b[VOC_MAXN], prev = 30000, step = 0;
        uint32_t blk, j, settled = 0, dry_ok = 1;
        voc_cur = 0, voc_xg = 32767, voc_mode = VM_OFF;
        for (blk = 0; blk < 60u; blk++) {
            uint32_t st;
            uin_mode = blk >= 10u && blk < 35u;        /* on for 25 blocks */
            st = voc_state();
            for (j = 0; j < VOC_MAXN; j++)
                voc_in[j] = 13653, b[j] = 30000;          /* (the voice 13653 Q15: 10000 on the track's scale) */
            if (st == 1u + VM_OFF)
                voc_dry_block(b, VOC_MAXN);
            voc_fade(b, VOC_MAXN);
            for (j = 0; j < VOC_MAXN; j++) {
                int32_t d = b[j] - prev;
                step = (d < 0 ? -d : d) > step ? (d < 0 ? -d : d) : step;
                prev = b[j];
            }
            if (blk == 24u)
                dry_ok = voc_cur == 1u + VM_OFF && voc_xg == 32767 && b[VOC_MAXN - 1u] == VOC_VS(13653);
            if (blk == 49u)
                settled = voc_cur == 0u && voc_xg == 32767 && b[VOC_MAXN - 1u] == 30000;
        }
        printf("   the track: the largest step between samples across both changes %d\n", step);
        check("MODE AUDIO: the voice itself, as one voice at full scale (VOC_VS), settled within 15 blocks", dry_ok);
        check("a change of state fades out and in: no step over 200 a sample (no click), back to the engine settled",
              step <= 200 && settled);
        uin_mode = 0;
    }
    {   /* the vocoder page's channel controls (vocoder-ui-spec.md §4-6, 2026-11-02): defaults as before, bit for bit;
         * EMPHASIS on NORMAL's weights; every channel muted; BYPASS REPLACEMENT; AUTOCONTROL */
        static double ref[66214];   /* (1 s: tone() reads the last 0.5 s) */
        uint32_t n, k, same;
        double b0, b1;
        voc_agc = 0, voc_bridge = 0, voc_norm = 1, voc_emsel = 0;
        reset();
        n = run(1.0, v1k, saw110);
        memcpy(ref, outbuf, n * sizeof ref[0]);
        b0 = tone(n, 990.0);
        voc_emsel = 1;                                  /* EMPHASIS, every channel at its middle */
        reset();
        run(1.0, v1k, saw110);
        same = !memcmp(ref, outbuf, n * sizeof ref[0]);
        check("page: VOCODER EMPHASIS with every channel at its middle is NORMAL, bit for bit", same);
        for (k = 0; k < VOC_CH; k++)
            vpg.emph[k] = 100;                          /* every channel +12 dB */
        reset();
        run(1.0, v1k, saw110);
        b1 = tone(n, 990.0);
        printf("   EMPHASIS every channel at the top: %+.2f dB (a pot's square: +12.04)\n", b1 - b0);
        check("page: EMPHASIS at the top +12 dB (+-0.3)", fabs(b1 - b0 - 12.04) < 0.3);
        for (k = 0; k < VOC_CH; k++)
            vpg.emph[k] = 50;
        voc_emsel = 0;
        vpg.mute = (1u << VOC_CH) - 1u;                 /* every channel muted: no synthesis, no speech addition */
        reset();
        run(1.0, v1k, saw110);
        b1 = tone(n, 990.0);
        printf("   every channel muted: %+.1f dB\n", b1 - b0);
        check("page: every channel muted, the vocoded tone gone (> 60 dB down)", b1 - b0 < -60.0);
        vpg.mute = 0;
        vpg.byp = VB_SPEECH, voc_dry = 0;
        reset();
        n = run(1.0, vzero, saw110);
        b0 = tone(n, 110.0);
        vpg.byp = VB_REPL, voc_dry = VOC_MIX_N - 1u;   /* the carrier dry at 0 dB, no voice */
        reset();
        run(1.0, vzero, saw110);
        b1 = tone(n, 110.0);
        vpg.byp = VB_SPEECH, voc_dry = 0;
        printf("   BYPASS REPLACEMENT at 0 dB, no voice: 110 Hz %+.1f dB over none\n", b1 - b0);
        check("page: BYPASS REPLACEMENT adds the carrier dry (no voice: the saw heard)", b1 - b0 > 30.0);
        vpg.actl = 1;
        reset();
        run(1.0, v1k, saw110);
        printf("   AUTOCONTROL on a voice at -6 dB: gain %.1f dB\n", db(voc_acg / 4096.0));
        check("page: AUTOCONTROL turns a hot voice down (-12 .. 0 dB)", voc_acg < 4096 && voc_acg >= 1024);
        vpg.actl = 0;
        reset();
        run(0.1, v1k, saw110);
        check("page: AUTOCONTROL off, back to 1.0", voc_acg == 4096);
        voc_norm = 0;
    }
    printf(fails ? "VOCODER TEST FAILED (%d)\n" : "smk_vocoder: all ok\n", fails);
    return fails != 0;
}
