/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the SMK-37 sequencer effects (src/smk_seqfx.h, the stem punch-in page's row 4):
 *   FLIP       backwards from the step playing; SLIP back to the grid's step, CARRY on from where it was left
 *   LOOP BACK  the steps that just played looped (1/4 on 1/16: 4 steps), forwards; back to the grid's step; every
 *              length 2 bars .. 1/16 on 1/16 (32 .. 1 steps), on 1/8 (16 .. 1: under a step, the step) and on 8T
 *   RAMP       on a 1/16 grid at 120 BPM: steps only on the grid, 240 (double) after a bar of it and no more; DOWN to
 *              the floor in a bar, 1 BPM (20 with CLOCK OUT); let go, the tempo back at once
 *   the DAW's clock  a RAMP held with the clock in: let go, the sequencer is back at the DAW's place (to the block)
 *   MIDI       FX CH notes 80..83: the velocity's setting is used (LOOP BACK 1/2 bar from velocity 3)
 * Stubs stand in for core.h and seq.c. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NTRK 4
#define FS 44100
#define BEAT_U ((uint32_t)FS * 60u)
#define SEQ_NONE 0xFFFFFFFFu
enum { G_BPM, G_CLKOUT, G_SYNC, G_N };
static struct { int16_t g[G_N]; uint8_t playing; } song;
typedef struct { uint32_t seq_abs; uint16_t seq_idx; uint32_t len, div; } track_t;
static track_t trk[NTRK];
static uint32_t clk_beat, clk_pos;
static uint8_t t_ext;
static int mclk_on(void) { return t_ext; }
static uint32_t trk_index(const track_t *t) { return (uint32_t)(t - trk); }
static uint32_t trk_len(const track_t *t) { return t->len; }
static uint32_t trk_div(const track_t *t) { return t->div; }
static const uint8_t DIV_DEN[6] = {1, 2, 4, 8, 3, 6};
static uint32_t div_units(uint32_t div) { return BEAT_U / DIV_DEN[div % 6u]; }

#include "../firmware/src/smk_seqfx.h"

static int fails;
static void check(const char *what, int ok)
{
    printf("%-100s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static void play_step(track_t *t, uint32_t abs)   /* (seq.c seq_tick: a step fires) */
{
    t->seq_abs = abs;
    t->seq_idx = (uint16_t)sq_idx(t, abs, t->len);
}

static void setup(void)
{
    memset(trk, 0, sizeof trk);
    memset(&sq_r, 0, sizeof sq_r);
    sq_reset();
    sq_held = 0;
    for (uint32_t i = 0; i < NTRK; i++)
        trk[i].len = 16, trk[i].div = 2, trk[i].seq_abs = SEQ_NONE;   /* (16 steps of 1/16) */
    song.playing = 1;
    song.g[G_BPM] = 120, song.g[G_CLKOUT] = 0, song.g[G_SYNC] = 0;
    clk_beat = 0, clk_pos = 0;
    t_ext = 0;
}

static void test_flip(void)
{
    static const uint8_t WANT_SLIP[8] = {4, 3, 2, 1, 0, 15, 12, 13};   /* abs 6..11 flipped, let go after 11: 12, 13 */
    static const uint8_t WANT_CARRY[8] = {4, 3, 2, 1, 0, 15, 0, 1};    /* .. CARRY: on from 15 */
    uint32_t pass, a, ok;
    for (pass = 0; pass < 2u; pass++) {
        track_t *t = &trk[0];
        setup();
        for (a = 0; a <= 5u; a++)
            play_step(t, a);
        sq_set(SQ_FLIP, 1, pass);                 /* after step 5: backwards from it */
        ok = 1;
        for (a = 6; a <= 13u; a++) {
            if (a == 12u)
                sq_set(SQ_FLIP, 0, 0);
            play_step(t, a);
            ok &= t->seq_idx == (pass ? WANT_CARRY : WANT_SLIP)[a - 6u];
        }
        check(pass ? "FLIP, CARRY: 4 3 2 1 0 15, let go: on from there (0 1)" :
                     "FLIP, SLIP: 4 3 2 1 0 15, let go: back where the grid is (12 13)", ok);
        if (pass) {
            setup();
            play_step(t, 0);
            check("CARRY's offset gone when the transport starts again", t->seq_idx == 0 && !sq_t[0].off);
        }
    }
}

static void test_loopback(void)
{
    track_t *t = &trk[0];
    uint32_t a, ok = 1;
    setup();
    sq_lb = 3;                                    /* 1/4: 4 steps of 1/16 */
    for (a = 0; a <= 9u; a++)
        play_step(t, a);
    sq_set(SQ_LOOPBK, 1, sq_lb);
    for (a = 10; a <= 19u; a++) {
        play_step(t, a);
        ok &= t->seq_idx == 6u + (a - 10u) % 4u;
    }
    check("LOOP BACK 1/4 after step 9: 6 7 8 9 6 7 8 9 6 7", ok);
    sq_set(SQ_LOOPBK, 0, 0);
    play_step(t, 20);
    check("let go (SLIP): back where the grid is (step 20 = 4)", t->seq_idx == 4u);
    setup();
    for (a = 0; a <= 9u; a++)
        play_step(t, a);
    sq_midi(SQ_NOTE0 + SQ_LOOPBK, 1, 3u);         /* velocity 3: the third length, 1/2 bar = 8 steps */
    for (a = 10, ok = 1; a <= 25u; a++) {
        play_step(t, a);
        ok &= t->seq_idx == 2u + (a - 10u) % 8u;
    }
    check("FX CH note 81 velocity 3: LOOP BACK 1/2 bar (8 steps: 2 .. 9)", ok);
    sq_midi(SQ_NOTE0 + SQ_LOOPBK, 0, 0);
    check("its note off lets it go", !sq_mode && !sq_held);
    {   /* every length, on three grids: the loop's steps */
        static const uint32_t DIVS[3] = {2, 1, 4};      /* 1/16, 1/8, 8T */
        static const uint16_t WANT[3][SQ_LB_N] = {{32, 16, 8, 4, 2, 1}, {16, 8, 4, 2, 1, 1}, {24, 12, 6, 3, 1, 1}};
        uint32_t d, l;
        ok = 1;
        for (d = 0; d < 3u; d++)
            for (l = 0; l < SQ_LB_N; l++) {
                setup();
                t->div = DIVS[d], t->len = 64;
                play_step(t, 40);
                sq_set(SQ_LOOPBK, 1, l);
                ok &= sq_t[0].lb == WANT[d][l];
                sq_set(SQ_LOOPBK, 0, 0);
            }
        check("LOOP BACK 2 bars .. 1/16: 32 .. 1 steps on 1/16, 16 .. 1 on 1/8, 24 .. 1 on 8T (under a step: one)", ok);
        check("LOOP BACK's default: 1 bar", SQ_LB_Q[1] == 16u && !strcmp(SQ_LB_NAME[1], "1 bar"));
    }
}

/* blocks of 32 samples through sq_adv as events_block runs it; the clock moves by what it returns */
static uint32_t run_blocks(uint32_t nb, uint64_t *master)
{
    uint32_t b, changes = 0, last = sq_r.q8;
    for (b = 0; b < nb; b++) {
        uint32_t own = 32u * (uint32_t)song.g[G_BPM], adv;
        uint64_t pos;
        if (master)
            *master += own;                       /* (the DAW's pulses: its tempo, 120) */
        adv = sq_adv(32u, own);
        pos = (uint64_t)clk_beat * BEAT_U + clk_pos + adv;
        clk_beat = (uint32_t)(pos / BEAT_U), clk_pos = (uint32_t)(pos % BEAT_U);
        if (sq_r.q8 != last) {
            uint32_t g = (uint32_t)(pos % (BEAT_U / 4u));
            changes++;
            if (g >= adv)                         /* (a change must come in the block that crossed the grid) */
                changes += 1000u;
            last = sq_r.q8;
        }
    }
    return changes;
}

static void test_ramp(void)
{
    uint32_t ch;
    uint64_t pos0;
    setup();
    sq_set(SQ_UP, 1, 2u);                         /* 1/16 */
    pos0 = (uint64_t)clk_beat * BEAT_U + clk_pos;
    ch = 0;
    while ((uint64_t)clk_beat * BEAT_U + clk_pos - pos0 < 4ull * BEAT_U)
        ch += run_blocks(1, 0);
    printf("   RAMP UP: %u tempo steps in the first bar, at %u BPM\n", ch, (sq_r.q8 + 128u) >> 8);
    check("RAMP UP 1/16 at 120: steps on the grid only, 240 (double) after a bar", ch < 1000u && sq_tempo_shown() == 240u);
    run_blocks(3000, 0);
    check("RAMP UP: no further than double", sq_tempo_shown() == 240u);
    sq_set(SQ_UP, 0, 0);
    check("let go: the tempo back at once", !sq_tempo_shown() && sq_adv(32u, 32u * 120u) == 32u * 120u);
    setup();
    sq_set(SQ_DOWN, 1, 2u);
    run_blocks(20000, 0);
    check("RAMP DOWN 1/16: the floor, 1 BPM", sq_tempo_shown() == 1u);
    setup();
    song.g[G_CLKOUT] = 1;
    sq_set(SQ_DOWN, 1, 0u);
    run_blocks(20000, 0);
    check("RAMP DOWN with CLOCK OUT: the floor 20 BPM (a DAW's least)", sq_tempo_shown() == 20u);
    sq_set(SQ_DOWN, 0, 0);
}

static void test_rejoin(void)
{
    uint64_t master = 0, pos;
    uint32_t i, d;
    setup();
    t_ext = 1;
    for (i = 0; i < NTRK; i++)
        trk[i].seq_abs = 5;
    sq_set(SQ_UP, 1, 1u);                         /* 1/8, the DAW's clock in */
    run_blocks(4000, &master);                    /* (2.9 s: well past double) */
    pos = (uint64_t)clk_beat * BEAT_U + clk_pos;
    printf("   ahead of the DAW by %.2f beats while held\n", (double)(int64_t)(pos - master) / BEAT_U);
    sq_set(SQ_UP, 0, 0);
    pos = (uint64_t)clk_beat * BEAT_U + clk_pos;
    d = (uint32_t)(pos > master ? pos - master : master - pos);
    check("the DAW's clock: ahead while the RAMP is held, back at the DAW's place let go", d == 0u);
    check("every track from the step the grid is in (no catch-up burst)", trk[0].seq_abs == SEQ_NONE && trk[3].seq_abs == SEQ_NONE);
    setup();
    t_ext = 1;
    master = 0;
    sq_set(SQ_DOWN, 1, 2u);
    run_blocks(4000, &master);
    sq_set(SQ_DOWN, 0, 0);
    pos = (uint64_t)clk_beat * BEAT_U + clk_pos;
    check("RAMP DOWN the same: let go, at the DAW's place", pos == master);
}

int main(void)
{
    printf("-- the SMK-37 sequencer effects (FLIP, LOOP BACK, RAMP)\n");
    test_flip();
    test_loopback();
    test_ramp();
    test_rejoin();
    printf(fails ? "SMK SEQFX TEST FAILED (%d)\n" : "smk_seqfx: all ok\n", fails);
    return fails != 0;
}
