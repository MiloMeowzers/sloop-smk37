/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the SMK-37 stem punch rings (src/smk_tpunch.h, STEM FX = PUNCH / PUNCH HQ): a ramp through every ring
 * (the sample's time as its value), an effect held for many ring lengths; each slot the effect reads must hold audio
 * from its span, never what the writer laid over it later (REVERSE once read the writer's new audio: the pops).
 *   REVERSE  HQ and PUNCH, every layout, 60 BPM (the span cut to the ring) and 120 BPM: only times inside the span
 *   loops    LOOP 4 and STUTTER: only times inside the captured span (and the seam's, just before it; REVERSE's just
 *            after its turn)
 *   seams    noise through LOOP 4, STUTTER, HALF, REVERSE (HQ, PUNCH): the level never dips at a loop's wrap (it faded
 *            to silence there each pass: clicks, the owner)
 *   stitches rapid REVERSE punches on noise under ~5 kHz (in and out every 10 blocks): with CURVE = EQUAL the level holds through
 *            each stitch; LINEAR (the old ramp) shown for comparison
 * Times are storage samples (PUNCH: an output pair).
 * Stubs stand in for punch.c, slicer.c, dsp.c and core.h. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define NTRK 4
#define SL_LEN 4096u
#define PUNCH_N 32768u
#define CTL 32u
#define FS 44100
enum { G_BPM, G_FXFADE, G_N };
static struct { int32_t g[G_N]; uint8_t playing; } song;
static uint32_t clk_pos;
static int16_t smk_fxpool[NTRK * SL_LEN + PUNCH_N];
static uint8_t sl_mem_lent;
static struct { uint32_t loop, rec_on, rec, w; } sl[NTRK];
static struct { int8_t cur; int32_t g; uint32_t w; } punch;
static int16_t *punch_ring;
static uint32_t punch_cap;
static int32_t clamp(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : x > hi ? hi : x; }
static int32_t sine_i(uint32_t ph) { return (int32_t)lrint(32767.0 * sin(ph * (6.283185307179586 / 4294967296.0))); }
/* (punch.c) */
enum { PX_LOOP4, PX_LOOP8, PX_LOOP16, PX_LOOP32, PX_STUT, PX_REV, PX_STOP, PX_HALF,
       PX_LPF, PX_HPF, PX_TEL, PX_CRUSH, PX_DOWN, PX_GATE, PX_ECHO, PX_WOBBLE,
       PX_KLOW, PX_KMID, PX_KHIGH, PX_RWD, PX_OCT, PX_FLANGE, PX_PAN, PX_FREEZE, PX_LOOP8T, PX_MONO, PX_FADE };
#define PX_RWD_ACC 17
#define PX_RWD_MAX (3 << 16)
static uint32_t clk_samples(void) { return clk_pos / (uint32_t)song.g[G_BPM]; }
static int px_loop(int32_t fx) { return fx <= PX_STUT || fx == PX_LOOP8T; }
static uint32_t beat_samples(void) { return (uint32_t)FS * 60u / (uint32_t)song.g[G_BPM]; }

#include "../firmware/src/smk_tpunch.h"

static int fails;
static void check(const char *what, int ok)
{
    printf("%-100s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

#define MOD 32000u                              /* the ramp's period: the stored value is the time mod MOD */
/* one run: the effect held from output sample t0 for `blocks` blocks, the transport stopped (ph < 0) or playing ph
 * samples into the beat (the grid); returns the reads outside [lo, hi] (storage times relative to t0, mod MOD), and
 * how many reads were checked */
static uint32_t run(uint32_t mode, uint32_t layout, int32_t bpm, int32_t fx, uint32_t blocks, int32_t ph, int32_t lo,
                    int32_t hi, uint32_t *checked)
{
    uint32_t u = 0, b, i, r, bad = 0;
    memset(&tp, 0, sizeof tp);
    tp.mode = 0xFFu, tp.cur = -1;
    song.g[G_BPM] = bpm, song.g[G_FXFADE] = 0;
    song.playing = ph >= 0, clk_pos = ph >= 0 ? (uint32_t)ph * (uint32_t)bpm : 0u;   /* (clk_samples: clk_pos / BPM) */
    tp_setup(mode, layout);
    *checked = 0;
    for (b = 0; b < 4000u + blocks; b++) {      /* (4000 blocks: every ring full before the effect) */
        int32_t want = b >= 4000u ? fx : -1;
        uint32_t t0 = 4000u * CTL;
        tp_block(CTL, want);
        for (i = 0; i < CTL; i++, u++) {
            int32_t x = (int32_t)(u / tp.s % MOD) * 8;   /* (PUNCH: a pair's mean is its storage time) */
            for (r = 0; r < tp.n; r++)
                (void)tp_x(r, i, x);
            if (tp_md[i] == 1u || tp_md[i] == 3u) {
                int32_t v[4], k, nv = 0;
                v[nv++] = tp_ring[0][tp_ia[i]];
                if (tp_fr[i])
                    v[nv++] = tp_ring[0][tp_ib[i]];
                if (tp_md[i] == 3u) {                   /* (the seam's second position, where it counts) */
                    if (tp_gb[i])
                        v[nv++] = tp_ring[0][tp_ic[i]];
                    if (tp_gb[i] && tp_fr2[i])
                        v[nv++] = tp_ring[0][tp_id[i]];
                }
                for (k = 0; k < nv; k++) {
                    int32_t d = (int32_t)((uint32_t)v[k] - t0 / tp.s % MOD + MOD) % (int32_t)MOD;   /* its time - t0 */
                    if (d > (int32_t)MOD / 2)
                        d -= (int32_t)MOD;
                    bad += d < lo || d > hi;
                    ++*checked;
                }
            }
        }
    }
    return bad;
}

/* noise through ring 0 at 120 BPM, the effect held from 1 s; the output's level in windows of 96 samples (from 0.2 s
 * into the effect, every wrap many times over): the quietest window against the mean */
static double seam_level(uint32_t mode, int32_t fx)
{
    static double ms[4000];
    uint32_t u = 0, b, i, r, rnd = 99u, nw = 0, w = 0;
    double acc = 0, sum = 0, lo = 1e30;
    memset(&tp, 0, sizeof tp);
    tp.mode = 0xFFu, tp.cur = -1;
    song.g[G_BPM] = 120, song.g[G_FXFADE] = 0, song.playing = 0, clk_pos = 0;
    tp_setup(mode, 1u);
    for (b = 0; b < 1400u + 6000u; b++) {
        tp_block(CTL, b >= 1400u ? fx : -1);
        for (i = 0; i < CTL; i++, u++) {
            int32_t x, y = 0;
            rnd = rnd * 1664525u + 1013904223u;
            x = (int32_t)(rnd >> 14) - (1 << 17);       /* (+-131072 mix units) */
            for (r = 0; r < tp.n; r++) {
                int32_t o = tp_x(r, i, x);
                if (!r)
                    y = o;
            }
            if (b >= 1400u + 280u) {                    /* (0.2 s on: the fade done, the loop wrapped) */
                acc += (double)y * y;
                if (++w == 96u) {
                    if (nw < 4000u)
                        ms[nw++] = acc / 96.0;
                    acc = 0, w = 0;
                }
            }
        }
        if (b == 1400u + 279u)
            w = 0, acc = 0;
    }
    for (i = 0; i < nw; i++)
        sum += ms[i];
    for (i = 0; i < nw; i++)
        lo = ms[i] < lo ? ms[i] : lo;
    return sqrt(lo / (sum / nw));
}

/* rapid punches: noise through ring 0, fx held 10 blocks, let go 10, over and over (fx -1: none, the baseline); the
 * quietest 64-sample window against the mean */
static double stitch_level(uint32_t mode, uint32_t lin, int32_t fx)
{
    static double ms[4000];
    uint32_t u = 0, b, i, r, rnd = 7u, nw = 0, w = 0;
    int32_t lp = 0;
    double acc = 0, sum = 0, lo = 1e30;
    memset(&tp, 0, sizeof tp);
    tp.mode = 0xFFu, tp.cur = -1;
    tp_lin = (uint8_t)lin, tp_xfl = TPX_128;
    song.g[G_BPM] = 120, song.g[G_FXFADE] = 0, song.playing = 0, clk_pos = 0;
    tp_setup(mode, 1u);
    for (b = 0; b < 1400u + 4000u; b++) {
        tp_block(CTL, b >= 1400u && (b / 10u) % 2u ? fx : -1);
        for (i = 0; i < CTL; i++, u++) {
            int32_t x, y = 0;
            rnd = rnd * 1664525u + 1013904223u;
            lp += (((int32_t)(rnd >> 13) - (1 << 18)) - lp) >> 2;   /* (noise under ~5 kHz: what PUNCH's 22 kHz keeps) */
            x = lp;
            for (r = 0; r < tp.n; r++) {
                int32_t o = tp_x(r, i, x);
                if (!r)
                    y = o;
            }
            if (b >= 1400u) {
                acc += (double)y * y;
                if (++w == 64u) {
                    if (nw < 4000u)
                        ms[nw++] = acc / 64.0;
                    acc = 0, w = 0;
                }
            }
        }
    }
    tp_lin = 0;
    for (i = 0; i < nw; i++)
        sum += ms[i];
    for (i = 0; i < nw; i++)
        lo = ms[i] < lo ? ms[i] : lo;
    return sqrt(lo / (sum / nw));
}

int main(void)
{
    static const char *const LN[4] = {"", "DRUMS+RET", "SYNTHS+RET", "ALL DRY"};
    uint32_t mode, layout, n, bad;
    int32_t bpm;
    char what[160];
    (void)stem_fx;                              /* (the menu's: not used here) */
    printf("-- the SMK-37 stem punch rings\n");
    for (mode = SFX_PUNCH; mode <= SFX_HQ; mode++)
        for (layout = 1; layout <= 3u; layout++)
            for (bpm = 60; bpm <= 120; bpm += 60) {
                bad = run(mode, layout, bpm, PX_REV, 2000u, -1, -(int32_t)MOD / 2 + 1,
                          (int32_t)(TP_SEAM / (mode == SFX_HQ ? 1u : 2u)) + 2, &n);   /* (64 000 samples; the seam
                                                         * reads just after the turn) */
                snprintf(what, sizeof what, "REVERSE %s %s %d BPM: %u reads, all from its span and seam (%u not)",
                         mode == SFX_HQ ? "HQ" : "PUNCH", LN[layout], bpm, n, bad);
                check(what, n > 50000u && bad == 0);
            }
    for (mode = SFX_PUNCH; mode <= SFX_HQ; mode++) {
        static const int32_t PH[3] = {-1, 1000, 3600};  /* stopped; playing: joined 1000 samples into the span, and
                                                         * 75 samples before STUTTER's division (it waits, dry) */
        static const char *const PN[3] = {"stopped", "on the grid, joined late", "on the grid, waiting"};
        int32_t st = mode == SFX_HQ ? 1 : 2, p;
        for (p = 0; p < 3; p++) {
            int32_t back = (PH[p] < 0 ? 0 : PH[p] / st + 1) + (int32_t)TP_SEAM / st + 2;   /* (joined late: the span
                                                         * began before the press; the seam reads before the span) */
            int32_t wait = PH[p] < 0 ? 0 : 1536 / st;          /* (waiting: it begins after) */
            bad = run(mode, layout = 1u, 120, PX_LOOP4, 2000u, PH[p], -back - 1, 22050 / st + wait + 2, &n);
            snprintf(what, sizeof what, "LOOP 4 %s DRUMS+RET %s: %u reads, all inside the captured beat (%u not)",
                     mode == SFX_HQ ? "HQ" : "PUNCH", PN[p], n, bad);
            check(what, n > 50000u && bad == 0);
            bad = run(mode, 2u, 120, PX_STUT, 2000u, PH[p], -back - 1, 22050 / 6 / st + wait + 2, &n);
            snprintf(what, sizeof what, "STUTTER %s SYNTHS+RET %s: %u reads, all inside the captured span (%u not)",
                     mode == SFX_HQ ? "HQ" : "PUNCH", PN[p], n, bad);
            check(what, n > 50000u && bad == 0);
        }
    }
    {   /* the seams: noise in, the effect held; the level in windows of 96 samples never far under the mean */
        static const int32_t FX[4] = {PX_LOOP4, PX_STUT, PX_HALF, PX_REV};
        static const char *const FN[4] = {"LOOP 4", "STUTTER", "HALF", "REVERSE"};
        uint32_t f;
        for (mode = SFX_PUNCH; mode <= SFX_HQ; mode++)
            for (f = 0; f < 4u; f++) {
                double lo = seam_level(mode, FX[f]);
                snprintf(what, sizeof what, "%s %s, noise: the quietest 96-sample window %.2f of the mean (a dip at the wraps: < 0.6)",
                         FN[f], mode == SFX_HQ ? "HQ" : "PUNCH", lo);
                check(what, lo > 0.6);
            }
    }
    for (mode = SFX_PUNCH; mode <= SFX_HQ; mode++) {   /* the stitches: rapid punches */
        double eq = stitch_level(mode, 0, PX_REV), li = stitch_level(mode, 1, PX_REV), no = stitch_level(mode, 0, -1);
        snprintf(what, sizeof what, "rapid REVERSE punches %s: the quietest window %.2f of the mean EQUAL, %.2f LINEAR, %.2f none",
                 mode == SFX_HQ ? "HQ" : "PUNCH", eq, li, no);
        check(what, eq >= li && eq > 0.75 * no);
    }
    printf(fails ? "SMK TPUNCH TEST FAILED (%d)\n" : "smk_tpunch: all ok\n", fails);
    return fails != 0;
}
