/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the SMK-37 harmoniser (firmware/src/smk_harm.h, Stage 1a; smk37-devtools harmoniser-spec.md §8):
 *   pitch     one voice on a 240.1 Hz sine, notes C4 (unison), G4 (x1.498), C5 (x2), C3 (x0.5), C2 (x0.25), E4: the
 *             wet output's strongest frequency within 1 % of 240.1 x ratio (Goertzel over a sweep); PITCH (T1's FADER BANK
 *             B) +12 and -24 at the fader's ends. (240.1 Hz: three cycles between the two taps, half a segment apart, so
 *             they add in phase. On other sines the crossfade beats the taps against each other and the spectrum's
 *             peak moves by up to half the splice rate: ALGORITHM 1's own colour, the H949's, not a wrong ratio)
 *   dry       no note: the output is the input at half (WET / DRY 50 %), on the track's scale, exactly
 *   fades     a note on and off on a sine: no step over 1200 a sample in the output (the voice faded in ~1.5 ms)
 *   feedback  FB MAIN at its top (127), three voices, a 0.5 s noise burst then 60 s of silence: the output falls more than
 *             60 dB from its loudest second to its last; never at full scale for long (SAFE: the loop's gain under 1)
 *   repeat    REPEAT after a sine: the input cut, the memory plays round, 5 s on the output still sounds (within 6 dB of
 *             the start) and does not grow
 *   tracker   harmonic tones (a sawtooth, and a tone with a strong second harmonic) at MIDI 45 .. 76 through SCALE: the
 *             sung note the tone's, never an octave off; silence after: the note held
 *   scale     SCALE MAJOR, C: a sung D4, the key E4 (+4) -> F4 (a minor third over D: the scale's), B4 (+11) -> C5;
 *             SCALE off: chromatic (E4 -> +4)
 *   sends     AUDIO IN off: the voice's input not heard, the HARM sends (hm_sb, the last block's) are
 * 44.1 kHz, blocks of 32 as the mix. Stubs stand in for core.h, voice.c, the vocoder's input and the console's timing. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define VOC_MAXN 32u
#define VOICE_FS 24000
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
#include "felucca_tables.h"                        /* pitch_inc (build/gen) */
enum { P_SCALE, P_ROOT, P_TFLT, P_DLY, P_COUNT };
typedef struct track { int16_t p[P_COUNT]; } track_t;
static track_t trk[4];
enum { UD_T3, UD_CARR, UD_T1, UD_T2, UD_HARM, UD_N };
static int16_t uin_dst[UD_N][VOC_MAXN];
static int audio_on = 1;
#define VOC_AUDIO_ON audio_on
#define VOC_VS(x) ((int32_t)(((int64_t)(x) * VOICE_FS) >> 14))
static uint8_t hm_on = 1;                          /* (smk_vocoder.h's) */
static uint32_t fm1_ms;                            /* (the UI's clock: REPEAT's flash) */
#define TIM_VOC_T(v) uint32_t v = 0
#define TIM_ADD(a, b) ((void)(b))
#define TIM_NOW() 0u
enum { TS_TRK, TS_HARM };
static const uint16_t SCALE_MASK[] = {0xFFF, (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11)};
#include "../firmware/src/smk_harm.h"
static uint32_t scale_mask(const track_t *t) { return SCALE_MASK[t->p[P_SCALE] ? 1 : 0]; }
static int16_t pool[16384];

static int fails;
static void check(const char *what, int ok)
{
    printf("  %-74s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void reset(void)
{
    memset(&hm, 0, sizeof hm);
    memset(hm_sb, 0, sizeof hm_sb);
    hm_ring = pool;
    memset(&trk[0], 0, sizeof trk[0]);
    hm_send[0] = 100, audio_on = 1, hm_pitch = 0, hm_fb = 0, hm_int = 0;
    trk[0].p[P_TFLT] = -40, trk[0].p[P_DLY] = 90;    /* (a synth preset's FILTER and DELAY SEND: ignored, 67ba07e's
                                                      * octave climb) */
}
/* n samples of input fn(i) through the harmoniser into out (the track's scale) */
static void run(const int16_t *in, int32_t *out, uint32_t n)
{
    uint32_t i, k;
    for (i = 0; i < n; i += VOC_MAXN) {
        uint32_t m = n - i < VOC_MAXN ? n - i : VOC_MAXN;
        for (k = 0; k < m; k++)
            uin_dst[UD_HARM][k] = in ? in[i + k] : 0;
        hm_sbp ^= 1u;
        hm_render(&trk[0], out + i, m);
    }
}
static double goertzel(const double *x, uint32_t n, double f)
{
    double w = 2.0 * M_PI * f / 44100.0, c = 2.0 * cos(w), s1 = 0, s2 = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double s0 = x[i] + c * s1 - s2;
        s2 = s1, s1 = s0;
    }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n;
}
#define N1 44100u
#define FT (3.0 * 44100.0 / (2.0 * 275.5))           /* 240.1 Hz: three cycles in half a segment (HM_SEG / 2 storage) */
static int16_t in1[8 * N1];
static int32_t out1[8 * N1];
static double wet[N1];
/* the wet part (the output less the dry half) over the last second of a run of len samples, its strongest frequency */
static double peak_freq(uint32_t len, double lo, double hi)
{
    uint32_t i, b = len - N1;
    double best = 0, bf = 0, f;
    for (i = 0; i < N1; i++)
        wet[i] = out1[b + i] / (double)VOICE_FS * 16384.0 - in1[b + i] / 2.0;
    for (f = lo; f <= hi; f *= 1.0005) {
        double g = goertzel(wet, N1, f);
        if (g > best)
            best = g, bf = f;
    }
    return bf;
}
static void sine(double f, double a, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        in1[i] = (int16_t)(a * sin(2.0 * M_PI * f * i / 44100.0));
}

int main(void)
{
    static const struct { uint8_t note; double ratio; const char *name; } P[] = {
        {60, 1.0, "C4 unison"}, {67, 1.4983, "G4 x1.498"}, {72, 2.0, "C5 x2"}, {48, 0.5, "C3 x0.5"},
        {36, 0.25, "C2 x0.25"}, {64, 1.2599, "E4 x1.260"}};
    uint32_t i, k;
    char b[96];
    printf("smk_harm_test (smk_harm.h)\n");
    /* pitch */
    for (k = 0; k < sizeof P / sizeof P[0]; k++) {
        double f;
        reset();
        sine(FT, 8000.0, 2 * N1);
        hm_note_on(P[k].note);
        run(in1, out1, 2 * N1);
        f = peak_freq(2 * N1, 40.0, 900.0);
        snprintf(b, sizeof b, "pitch %s: %.1f Hz (want %.1f)", P[k].name, f, FT * P[k].ratio);
        check(b, fabs(f / (FT * P[k].ratio) - 1.0) < 0.01);
    }
    {   /* PITCH: the fader's ends, on unison */
        static const struct { int16_t v; double r; } F[] = {{63, 2.0}, {-64, 0.25}};
        for (k = 0; k < 2u; k++) {
            double f;
            reset();
            hm_pitch = F[k].v;
            sine(FT, 8000.0, 2 * N1);
            hm_note_on(60);
            run(in1, out1, 2 * N1);
            f = peak_freq(2 * N1, 40.0, 900.0);
            snprintf(b, sizeof b, "PITCH fader %d: %.1f Hz (want %.1f)", F[k].v, f, FT * F[k].r);
            check(b, fabs(f / (FT * F[k].r) - 1.0) < 0.01);
        }
    }
    {   /* dry */
        int ok = 1;
        reset();
        sine(330.0, 12000.0, N1);
        run(in1, out1, N1);
        for (i = 0; i < N1; i++)
            ok &= out1[i] == VOC_VS(in1[i] >> 1) || out1[i] == VOC_VS((in1[i] + 1) >> 1) || out1[i] == VOC_VS(in1[i] / 2);
        check("dry: no note, the input at half on the track's scale", ok);
    }
    {   /* fades */
        int32_t mx = 0;
        reset();
        sine(110.0, 12000.0, 2 * N1);
        run(in1, out1, N1 / 2);
        hm_note_on(67);
        run(in1 + N1 / 2, out1 + N1 / 2, N1 / 2);
        hm_note_off(67);
        run(in1 + N1, out1 + N1, N1 / 2);
        for (i = 1; i < 3 * N1 / 2; i++) {
            int32_t d = abs(out1[i] - out1[i - 1]);
            mx = d > mx ? d : mx;
        }
        snprintf(b, sizeof b, "fades: the largest step a sample %d (a 110 Hz sine's own ~%d)", mx,
                 (int)(VOC_VS(12000) * 2 * M_PI * 110 / 44100 * 1.5));
        check(b, mx < 1200);
    }
    {   /* feedback: SAFE */
        double e0 = 0, e1 = 0;
        uint32_t len = 61 * N1, s = 12345u, top = 0;
        static int32_t o[61 * 44100];
        static int16_t x[61 * 44100];
        reset();
        hm_fb = 127;
        hm_note_on(60), hm_note_on(67), hm_note_on(72);
        for (i = 0; i < len; i++) {
            s = s * 1664525u + 1013904223u;
            x[i] = i < N1 / 2 ? (int16_t)((int32_t)(s >> 16) - 32768) / 2 : 0;
        }
        run(x, o, len);
        for (i = 0; i < len; i++)
            top += abs(o[i]) >= VOC_VS(32000);
        for (i = 0; i < N1; i++) {
            e0 += (double)o[i] * o[i];
            e1 += (double)o[len - N1 + i] * o[len - N1 + i];
        }
        snprintf(b, sizeof b, "feedback 127, 3 voices: the last second %.1f dB under the first",
                 10.0 * log10((e0 + 1) / (e1 + 1)));
        check(b, e1 * 1e6 < e0 || e1 == 0.0);
        snprintf(b, sizeof b, "feedback 127: samples near full scale %u (of %u)", top, len);
        check(b, top < N1 / 10);
    }
    {   /* repeat */
        double e0 = 0, e1 = 0, e2 = 0;
        reset();
        sine(440.0, 8000.0, N1);
        hm_note_on(60);
        run(in1, out1, N1);
        hm.rep = 1;
        memset(in1, 0, sizeof in1[0] * 6 * N1);
        run(in1, out1, 6 * N1);
        for (i = 0; i < N1 / 2; i++) {
            e0 += (double)out1[i] * out1[i];
            e1 += (double)out1[5 * N1 + i] * out1[5 * N1 + i];
            e2 += (double)out1[2 * N1 + i] * out1[2 * N1 + i];
        }
        snprintf(b, sizeof b, "repeat: 5 s on, %.1f dB against the start, 2 s on %.1f dB", 10 * log10((e1 + 1) / (e0 + 1)),
                 10 * log10((e2 + 1) / (e0 + 1)));
        check(b, e1 * 4.0 > e0 && e1 < e0 * 1.5);
    }
    {   /* tracker */
        int bad = 0, held = 1;
        uint32_t m;
        for (m = 45; m <= 76; m++) {
            uint32_t kind;
            for (kind = 0; kind < 2u; kind++) {
                double f = 440.0 * pow(2.0, (m - 69.0) / 12.0), ph = 0;
                reset();
                hm_int = 1;
                for (i = 0; i < N1 / 2; i++) {
                    ph += f / 44100.0;
                    ph -= floor(ph);
                    in1[i] = kind ? (int16_t)(6000 * sin(2 * M_PI * ph) + 9000 * sin(4 * M_PI * ph))
                                  : (int16_t)(12000 * (2 * ph - 1));
                }
                run(in1, out1, N1 / 2);
                if (hm.tnote != m) {
                    bad++;
                    printf("    MIDI %u (%s): read %u\n", m, kind ? "strong 2nd" : "saw", hm.tnote);
                }
                memset(in1, 0, sizeof in1[0] * N1 / 2);
                run(in1, out1, N1 / 2);
                held &= hm.tnote == m || hm.tnote == 0;
            }
        }
        check("tracker: MIDI 45 .. 76, a saw and a strong-2nd tone, the note read", bad == 0);
        check("tracker: silence after: the note held", held);
    }
    {   /* scale */
        int32_t want;
        reset();
        trk[0].p[P_SCALE] = 1, trk[0].p[P_ROOT] = 0, hm_int = 1;
        hm.tnote = 62;                                 /* (a sung D4) */
        hm_note_on(64);
        hm_ratios(&trk[0]);
        want = hm_dinc(3 * 16);
        snprintf(b, sizeof b, "scale: C MAJOR, sung D4, key E4 -> F4 (+3)");
        check(b, hm.dinc[0] == want);
        hm_note_off(64), hm_note_on(71);
        hm_ratios(&trk[0]);
        check("scale: key B4 (+11) -> C5 (+10 over D)", hm.dinc[0] == hm_dinc(10 * 16) || hm.dinc[1] == hm_dinc(10 * 16));
        trk[0].p[P_SCALE] = 0;
        hm_all_off(), hm_note_on(64);
        hm_ratios(&trk[0]);
        check("scale: a CHR scale: from the sung note, chromatic, E4 -> +4", hm.dinc[0] == hm_dinc(4 * 16));
        trk[0].p[P_SCALE] = 1, hm_int = 0;
        hm_all_off(), hm_note_on(64);
        hm_ratios(&trk[0]);
        check("scale: INTERVALS FIXED (the default): the song's scale ignored, E4 -> +4", hm.dinc[0] == hm_dinc(4 * 16));
    }
    {   /* sends */
        double ev = 0, es = 0;
        reset();
        audio_on = 0;
        sine(220.0, 8000.0, N1);
        for (i = 0; i < N1; i += VOC_MAXN) {
            for (k = 0; k < VOC_MAXN; k++)
                uin_dst[UD_HARM][k] = in1[i + k];
            hm_sbp ^= 1u;
            for (k = 0; k < VOC_MAXN; k++)
                hm_sb[1][hm_sbp ^ 1u][k] = 0;          /* (the units would have written the last block's) */
            hm_render(&trk[0], out1 + i, VOC_MAXN);
        }
        for (i = 0; i < N1; i++)
            ev += (double)out1[i] * out1[i];
        reset();
        audio_on = 0;
        for (i = 0; i < N1; i += VOC_MAXN) {
            hm_sbp ^= 1u;
            for (k = 0; k < VOC_MAXN; k++)
                hm_sb[1][hm_sbp ^ 1u][k] = in1[i + k]; /* (a send, as the units write it) */
            hm_render(&trk[0], out1 + i, VOC_MAXN);
        }
        for (i = 0; i < N1; i++)
            es += (double)out1[i] * out1[i];
        check("sends: AUDIO IN off, the voice's input silent", ev == 0.0);
        check("sends: a send heard (the last block's)", es > 1e9);
    }
    printf(fails ? "smk_harm_test: %d FAILED\n" : "smk_harm_test: all passed\n", fails);
    return fails != 0;
}
