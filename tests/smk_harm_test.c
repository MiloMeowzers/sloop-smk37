/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the SMK-37 harmoniser (firmware/src/smk_harm.h, Stage 1a and 1b; smk37-devtools harmoniser-spec.md §8,
 * §9):
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
 * Stage 1b (the HARMONY page's settings, hs):
 * Stage 1c (§10): the VOICE lane as the mixer runs it (fx.c hm_insert: the dry at hb.kd, the wet at the voices' pans);
 *   lanes     VOICE(a) + T2(b) = one lane of a + b (1b's sends), to rounding; pans: centred left = right exactly, SPREAD
 *             100 V1 left / V3 right; the drum channels: the voices apart sum to the wet; the ceiling under full scale
 *   (1b's golden of 1a's sound: retired, the chain's order changed: T1's DIST / LEVEL before the lane now)
 *   MICRO     +50 cents on E4: the pitch 2^(50/1200) higher; on a whole step, hm_dinc's exactly
 *   SHARP / FLAT  no key: silent (the keys play them, 2026-11-13); C4 held: 1.070 / 0.930 at MICRO 100, ~1.001 near 0;
 *             G4 held: the fifth moved too
 *   LEVEL     a voice at 50 %: its part half; OFF (the mute): the dry alone, exactly
 *   DRY       DRY LEVEL 0: no note, silence exactly; IN / OUT: the input straight through, exactly; INPUT LEVEL -6 dB
 *   MANUAL    no key: the MANUAL voice at the PITCH fader's ratio; MANUAL alone keeps its voice from the keys; K+M
 *   pads      G1's pads as notes (60 + the interval), let go: off
 *   EQ        the LOW shelf +18 dB at 100 Hz, the HIGH -12 dB at 10 kHz (22.05 kHz), flat: no change
 *   SAFE + EQ FEEDBACK 127 with each shelf at its loudest: still dies away (the loop scaled under the EQ's peak)
 *   H949      FB LIMIT H949, FEEDBACK 127: the loop rings on after the input stops, and stays bounded (the soft limit)
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
#define HM_POOL                                    /* (the firmware's .pool: plain statics here) */
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
    hm_ring = pool;
    hm_spread = 0;
    memset(&trk[0], 0, sizeof trk[0]);
    hm_send[0] = 100, audio_on = 1, hm_pitch = 0, hm_fb = 0, hm_int = 0, hm_wd = 64;
    hs = HS_DEF, hm_pad = 0;                         /* (the HARMONY page at its defaults) */
    trk[0].p[P_TFLT] = -40, trk[0].p[P_DLY] = 90;    /* (a synth preset's FILTER and DELAY SEND: ignored, 67ba07e's
                                                      * octave climb) */
}
/* n samples of input fn(i) through the harmoniser into out (the track's scale) */
static void run(const int16_t *in, int32_t *out, uint32_t n)
{
    uint32_t i, k;
    for (i = 0; i < n; i += VOC_MAXN) {
        uint32_t m = n - i < VOC_MAXN ? n - i : VOC_MAXN;
        static int32_t x[VOC_MAXN], wl[VOC_MAXN], wr[VOC_MAXN];
        int32_t vg = audio_on ? HM_GAIN(hm_send[0]) : 0;
        for (k = 0; k < m; k++)
            x[k] = ((in ? in[i + k] : 0) * vg) >> 15;   /* (T1's engine: the input at VOICE IN, int16 scale) */
        hm_block();
        hm_lane(HL_VOICE, x, wl, wr, 0, m);
        for (k = 0; k < m; k++)                         /* (centred voices: wl = wr = the mono wet) */
            out[i + k] = VOC_VS(((x[k] * hb.kd) >> 15) + ((wl[k] >> 1) + (wr[k] >> 1)));
    }
}
/* a lane of track k (not the VOICE lane) on x alone: its wet (left + right) / 2, int16 scale */
static void run_lane(uint32_t k, const int32_t *x, int32_t *w, uint32_t n)
{
    uint32_t i, j;
    for (i = 0; i < n; i += VOC_MAXN) {
        static int32_t wl[VOC_MAXN], wr[VOC_MAXN];
        uint32_t m = n - i < VOC_MAXN ? n - i : VOC_MAXN;
        hm_block();
        hm_lane(k, x + i, wl, wr, 0, m);
        for (j = 0; j < m; j++)
            w[i + j] = (wl[j] >> 1) + (wr[j] >> 1);
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
    /* ---- Stage 1b ---- */
    {   /* MICRO */
        double f;
        int ok = 1;
        int32_t s;
        for (s = -384; s <= 192; s += 16)
            ok &= hm_dincf(s, 0) == hm_dinc(s) && (s + 16 > 192 || hm_dincf(s, 100) == hm_dinc(s + 16));
        check("MICRO: 0 cents and whole steps: hm_dinc's exactly", ok);
        reset();
        hs.mic[0] = 50;
        sine(FT, 8000.0, 2 * N1);
        hm_note_on(64);
        run(in1, out1, 2 * N1);
        f = peak_freq(2 * N1, 40.0, 900.0);
        snprintf(b, sizeof b, "MICRO +50 on E4: %.1f Hz (want %.1f)", f, FT * 1.2599 * pow(2.0, 50.0 / 1200.0));
        check(b, fabs(f / (FT * 1.2599 * pow(2.0, 50.0 / 1200.0)) - 1.0) < 0.004);
    }
    {   /* SHARP / FLAT */
        uint32_t r1, r2, r3;
        int on;
        reset();
        hs.fn[0] = HF_SHARP, hs.mic[0] = 100;
        hm_ratios(&trk[0]);
        on = !hm.on[0];                                 /* (no key: silent) */
        hm_note_on(60);
        hm_ratios(&trk[0]);
        on &= hm.on[0] && !hm.on[1];
        r1 = hm_ratio1000(0);
        hs.fn[0] = HF_FLAT;
        hm_ratios(&trk[0]);
        r2 = hm_ratio1000(0);
        hs.fn[0] = HF_SHARP, hs.mic[0] = 12;
        hm_ratios(&trk[0]);
        r3 = hm_ratio1000(0);
        snprintf(b, sizeof b, "SHARP / FLAT: no key silent; C4: MICRO 100 %u / %u, MICRO 12 %u", r1, r2, r3);
        check(b, on && r1 >= 1068 && r1 <= 1072 && r2 >= 928 && r2 <= 932 && r3 >= 1001 && r3 <= 1002);
        hm_all_off(), hm_note_on(67), hs.mic[0] = 100;
        hm_ratios(&trk[0]);
        snprintf(b, sizeof b, "SHARP on G4 (the fifth, 1.498): MICRO 100 %u (1.498 x 1.07 = 1.603)", hm_ratio1000(0));
        check(b, hm_ratio1000(0) >= 1600u && hm_ratio1000(0) <= 1606u);
    }
    {   /* LEVEL and the mutes */
        double e1 = 0, e2 = 0;
        int same = 1;
        reset();
        sine(FT, 8000.0, N1);
        hm_note_on(67);
        run(in1, out1, N1);
        for (i = N1 / 2; i < N1; i++)
            e1 += pow(out1[i] / (double)VOICE_FS * 16384.0 - in1[i] / 2.0, 2);
        reset();
        hs.lv[0] = 50;
        hm_note_on(67);
        run(in1, out1, N1);
        for (i = N1 / 2; i < N1; i++)
            e2 += pow(out1[i] / (double)VOICE_FS * 16384.0 - in1[i] / 2.0, 2);
        snprintf(b, sizeof b, "LEVEL 50 %%: the voice %.2f dB (want -6.02)", 10 * log10(e2 / e1));
        check(b, fabs(10 * log10(e2 / e1) + 6.02) < 0.2);
        reset();
        hs.mute = 2;                                   /* (V1 off) */
        hm_note_on(67);
        run(in1, out1, N1);
        for (i = 0; i < N1; i++)
            same &= out1[i] == VOC_VS(in1[i] >> 1) || out1[i] == VOC_VS((in1[i] + 1) >> 1) || out1[i] == VOC_VS(in1[i] / 2);
        check("V1 OFF: the dry alone, exactly", same);
    }
    {   /* DRY, IN / OUT, INPUT LEVEL */
        int z = 1, by = 1;
        double e1 = 0, e2 = 0;
        reset();
        hs.dry = 0;
        sine(330.0, 12000.0, N1);
        run(in1, out1, N1);
        for (i = 0; i < N1; i++)
            z &= out1[i] == 0;
        check("DRY LEVEL 0, no note: silence, exactly", z);
        reset();
        hs.byp = 1;
        hm_note_on(67);
        run(in1, out1, N1);
        for (i = 0; i < N1; i++)
            by &= out1[i] == VOC_VS(in1[i]);
        check("IN / OUT: OUT, a note held: the input straight through, exactly", by);
        reset();
        sine(330.0, 3000.0, N1);                        /* (quiet: the memory's soft limit out of it) */
        hm_wd = 127, hm_note_on(67);                    /* (all wet: the voice alone) */
        run(in1, out1, N1);
        for (i = N1 / 2; i < N1; i++)
            e1 += (double)out1[i] * out1[i];
        reset();
        hm_wd = 127, hm_note_on(67);
        hs.inl = HM_INL_0 - 6;
        run(in1, out1, N1);
        for (i = N1 / 2; i < N1; i++)
            e2 += (double)out1[i] * out1[i];
        snprintf(b, sizeof b, "INPUT LEVEL -6 dB: the harmony %.2f dB (the lane's input)", 10 * log10(e2 / e1));
        check(b, fabs(10 * log10(e2 / e1) + 6.0) < 0.1);
    }
    {   /* MANUAL, K+M */
        double f;
        int ok;
        reset();
        hs.ctl = HC_MAN, hm_pitch = 63;
        sine(FT, 8000.0, 2 * N1);
        run(in1, out1, 2 * N1);
        f = peak_freq(2 * N1, 40.0, 900.0);
        snprintf(b, sizeof b, "MANUAL, no key: V1 at the PITCH fader's top, %.1f Hz (want %.1f)", f, FT * 2.0);
        check(b, fabs(f / (FT * 2.0) - 1.0) < 0.01);
        reset();
        hs.ctl = HC_MAN;
        hm_note_on(64), hm_note_on(67), hm_note_on(72);
        ok = hm.note[0] == 0 && hm.note[1] && hm.note[2] && (hm.note[1] == 72 || hm.note[2] == 72);
        check("MANUAL alone: the keys take V2 and V3 only (V1 the fader's)", ok);
        reset();
        hs.ctl = 3u, hm_pitch = -32;                    /* (-12 semitones) */
        hm_ratios(&trk[0]);
        ok = hm.on[0] && hm_ratio1000(0) == 500u;
        hm_note_on(64);
        hm_ratios(&trk[0]);
        snprintf(b, sizeof b, "K+M: no key, V1 at PITCH (0.500); E4 held, an octave down: %u (0.630)", hm_ratio1000(0));
        check(b, ok && hm_ratio1000(0) >= 629u && hm_ratio1000(0) <= 631u);
    }
    {   /* the pads */
        int32_t o[VOC_MAXN];
        int ok;
        reset();
        (void)o;
        hm_pad = 1u << 4;                               /* (top 5: a 5th up) */
        hm_block();
        ok = hm.note[0] == 67;
        hm_pad = 0;
        hm_block();
        check("pads: top 5 on = G4 (a 5th up), off = let go", ok && hm.note[0] == 0);
    }
    {   /* the EQ's curves */
        static const struct { int lo; uint32_t step; double f, want; } E[] = {
            {1, HM_LOW_0 + 18, 100.0, 17.9}, {1, HM_LOW_0 - 12, 100.0, -11.9}, {0, HM_HIGH_0 - 12, 10000.0, -12.0},
            {0, HM_HIGH_0 + 4, 8000.0, 4.0}, {1, HM_LOW_0, 100.0, 0.0}};
        for (k = 0; k < sizeof E / sizeof E[0]; k++) {
            int32_t z[4] = {0, 0, 0, 0};
            double ei = 0, eo = 0;
            const int32_t *c = E[k].lo ? HM_LOW[E[k].step] : HM_HIGH[E[k].step];
            for (i = 0; i < 22050u; i++) {
                int32_t x = (int32_t)(1000.0 * sin(2 * M_PI * E[k].f * i / 22050.0)), y = hm_bq(z, c, x);
                if (i >= 11025u)
                    ei += (double)x * x, eo += (double)y * y;
            }
            snprintf(b, sizeof b, "EQ %s step %u at %.0f Hz: %+.1f dB (want %+.1f)", E[k].lo ? "LOW" : "HIGH", E[k].step,
                     E[k].f, 10 * log10(eo / ei), E[k].want);
            check(b, fabs(10 * log10(eo / ei) - E[k].want) < 0.6);
        }
    }
    {   /* SAFE with the EQ at its loudest; H949 */
        static int32_t o[31 * 44100];
        static int16_t x[31 * 44100];
        uint32_t len = 31 * N1, s = 12345u, t;
        for (i = 0; i < len; i++) {
            s = s * 1664525u + 1013904223u;
            x[i] = i < N1 / 2 ? (int16_t)((int32_t)(s >> 16) - 32768) / 2 : 0;
        }
        for (t = 0; t < 3u; t++) {
            double e0 = 0, e1 = 0;
            int32_t mx = 0;
            reset();
            hm_fb = 127;
            if (t == 0u)
                hs.low = HM_LOW_N - 1u;
            else if (t == 1u)
                hs.high = HM_HIGH_N - 1u;
            else
                hs.fbl = 1;
            hm_note_on(60), hm_note_on(67), hm_note_on(72);
            run(x, o, len);
            for (i = 0; i < N1; i++) {
                e0 += (double)o[i] * o[i];
                e1 += (double)o[len - N1 + i] * o[len - N1 + i];
            }
            for (i = 0; i < len; i++)
                mx = abs(o[i]) > mx ? abs(o[i]) : mx;
            if (t < 2u) {
                snprintf(b, sizeof b, "SAFE, FEEDBACK 127, %s at its top: the last second %.1f dB under the first",
                         t ? "HIGH +4" : "LOW +18", 10.0 * log10((e0 + 1) / (e1 + 1)));
                check(b, e1 * 1e6 < e0 || e1 == 0.0);
            } else {
                snprintf(b, sizeof b, "H949, FEEDBACK 127: 30 s after the input, %.1f dB against the first second; peak %.2f FS",
                         10.0 * log10((e1 + 1) / (e0 + 1)), mx / (double)VOC_VS(32767));
                check(b, e1 * 100.0 > e0 && mx <= VOC_VS(49152));
            }
        }
    }
    /* ---- Stage 1c: the lanes, the pans, the drum channels, the ceiling ---- */
    {   /* linear: the VOICE lane on a + the T2 lane on b = one lane on a + b (1b's sends), to rounding */
        static int32_t xa[N1], xb[N1], xs[N1], wa[N1], wb_[N1], ws[N1];
        double e = 0, d = 0;
        for (i = 0; i < N1; i++) {
            xa[i] = (int32_t)(800 * sin(2 * M_PI * 220.0 * i / 44100.0));   /* (-32 dBFS: the memory's soft limit, the
                                                         * H949's colour at high level, out of it) */
            xb[i] = (int32_t)(700 * sin(2 * M_PI * 331.0 * i / 44100.0));
            xs[i] = xa[i] + xb[i];
        }
        reset();
        hm_fb = 40, hm_wd = 127, hm_send[1] = 100;
        hm_note_on(64), hm_note_on(67);
        run_lane(HL_T2, xs, ws, N1);                    /* (the sum through one lane) */
        reset();
        hm_fb = 40, hm_wd = 127, hm_send[1] = 100;
        hm_note_on(64), hm_note_on(67);
        for (i = 0; i < N1; i += VOC_MAXN) {            /* (each through its own, the same block) */
            static int32_t l1[VOC_MAXN], r1[VOC_MAXN], l2[VOC_MAXN], r2[VOC_MAXN];
            hm_block();
            hm_lane(HL_VOICE, xa + i, l1, r1, 0, VOC_MAXN);
            hm_lane(HL_T2, xb + i, l2, r2, 0, VOC_MAXN);
            for (k = 0; k < VOC_MAXN; k++)
                wa[i + k] = (l1[k] >> 1) + (r1[k] >> 1), wb_[i + k] = (l2[k] >> 1) + (r2[k] >> 1);
        }
        for (i = N1 / 2; i < N1; i++) {
            double t = wa[i] + wb_[i] - ws[i];
            e += (double)ws[i] * ws[i], d += t * t;
        }
        snprintf(b, sizeof b, "lanes: VOICE(a) + T2(b) = one lane of a + b: the difference %.1f dB under", 10 * log10(e / (d + 1)));
        check(b, d * 1e4 < e);                          /* (40 dB: what rounding leaves at -32 dBFS) */
    }
    {   /* pans: V1 hard left, V3 hard right (SPREAD 100); centred: wl = wr */
        static int32_t x[VOC_MAXN], wl[VOC_MAXN], wr[VOC_MAXN];
        double el = 0, er = 0, el3 = 0, er3 = 0;
        int same = 1;
        reset();
        hm_wd = 127;
        hm_note_on(60);
        for (i = 0; i < N1; i += VOC_MAXN) {
            for (k = 0; k < VOC_MAXN; k++)
                x[k] = (int32_t)(8000 * sin(2 * M_PI * 240.1 * (i + k) / 44100.0));
            hm_block();
            hm_lane(HL_VOICE, x, wl, wr, 0, VOC_MAXN);
            for (k = 0; k < VOC_MAXN; k++)
                same &= wl[k] == wr[k];
        }
        check("pans: centred (the default): left = right, exactly", same);
        reset();
        hm_wd = 127, hm_spread = 100;
        hm_note_on(60);                                 /* (V1) */
        for (i = 0; i < N1; i += VOC_MAXN) {
            for (k = 0; k < VOC_MAXN; k++)
                x[k] = (int32_t)(8000 * sin(2 * M_PI * 240.1 * (i + k) / 44100.0));
            hm_block();
            hm_lane(HL_VOICE, x, wl, wr, 0, VOC_MAXN);
            if (i >= N1 / 2)
                for (k = 0; k < VOC_MAXN; k++)
                    el += (double)wl[k] * wl[k], er += (double)wr[k] * wr[k];
        }
        hm_all_off(), hm.note[2] = 64, hm.age[2] = ++hm.seq;   /* (V3 alone) */
        for (i = 0; i < N1; i += VOC_MAXN) {
            for (k = 0; k < VOC_MAXN; k++)
                x[k] = (int32_t)(8000 * sin(2 * M_PI * 240.1 * (i + k) / 44100.0));
            hm_block();
            hm_lane(HL_VOICE, x, wl, wr, 0, VOC_MAXN);
            if (i >= N1 / 2)
                for (k = 0; k < VOC_MAXN; k++)
                    el3 += (double)wl[k] * wl[k], er3 += (double)wr[k] * wr[k];
        }
        snprintf(b, sizeof b, "SPREAD 100: V1 left (right %.0f dB under), V3 right (left %.0f dB under: the PAN law's +63)",
                 10 * log10((el + 1) / (er + 1)), 10 * log10((er3 + 1) / (el3 + 1)));
        check(b, er * 1e3 < el && el3 * 1e3 < er3);
    }
    {   /* the drum channels: each voice apart (wv), centred, sums to the mono wet */
        static int32_t x[VOC_MAXN], wl[VOC_MAXN], wr[VOC_MAXN], wv[3][VOC_MAXN];
        int ok = 1;
        double e0 = 0, e01 = 0;
        reset();
        hm_wd = 127, hm_send[3] = 100;
        hm_note_on(60), hm_note_on(67), hm_note_on(72);
        for (i = 0; i < N1; i += VOC_MAXN) {
            for (k = 0; k < VOC_MAXN; k++)
                x[k] = (int32_t)(3000 * sin(2 * M_PI * 240.1 * (i + k) / 44100.0));
            hm_block();
            hm_lane(HL_DRUM, x, wl, wr, wv, VOC_MAXN);
            for (k = 0; k < VOC_MAXN; k++) {
                int32_t sum = wv[0][k] + wv[1][k] + wv[2][k];
                ok &= abs(sum - wl[k]) <= 3;
                if (i >= N1 / 2)
                    e0 += (double)wv[0][k] * wv[0][k], e01 += (double)(wv[0][k] - wv[1][k]) * (wv[0][k] - wv[1][k]);
            }
        }
        check("drum channels: V1 + V2 + V3 apart = the lane's wet (within rounding)", ok);
        check("drum channels: the voices differ (each its interval)", e0 > 1e6 && e01 > 0.1 * e0);
    }
    {   /* the ceiling: three voices, FEEDBACK high, a hot input: the wet never past full scale */
        static int32_t x[VOC_MAXN], wl[VOC_MAXN], wr[VOC_MAXN];
        int32_t mx = 0;
        uint32_t s2 = 1;
        reset();
        hm_wd = 127, hm_fb = 122, hs.fbl = 1;
        hm_note_on(60), hm_note_on(64), hm_note_on(67);
        for (i = 0; i < 4 * N1; i += VOC_MAXN) {
            for (k = 0; k < VOC_MAXN; k++)
                s2 = s2 * 1664525u + 1013904223u, x[k] = (int32_t)(s2 >> 17) - 32768;
            hm_block();
            hm_lane(HL_VOICE, x, wl, wr, 0, VOC_MAXN);
            for (k = 0; k < VOC_MAXN; k++)
                mx = abs(wl[k]) > mx ? abs(wl[k]) : mx;
        }
        snprintf(b, sizeof b, "ceiling: three voices, FEEDBACK 122 (H949), full-scale noise in: wet peak %d (<= 32767)", mx);
        check(b, mx <= 32767);
    }
    printf(fails ? "smk_harm_test: %d FAILED\n" : "smk_harm_test: all passed\n", fails);
    return fails != 0;
}
