/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37: the sequencer effects, row 4 of the stem punch-in page (smk37-port.yaml next_spec seq_fx, tempo_sync). What
 * plays changes, so every stem, the stereo mix and MIDI out follow; no RAM, no audio work. Held, each:
 *   FLIP       the tracks play backwards from the step each is on
 *   LOOP BACK  each track loops the steps that just played: the loop ends at the playhead and starts LOOP BACK's length
 *              before it, played forwards; the lengths halve, 2 bars .. 1/16 (PRESETS; the owner: the short end as break
 *              slices, 1/8 a shuffle, 1/16 a single hit). Longer than a track's pattern it plays the pattern on; shorter
 *              than its step, the step it is on
 *   RAMP UP / DOWN  the tempo steps on a grid (1/4, 1/8, 1/16, straight or dotted: ALGORITHM), each step 1/(steps a bar)
 *              of the tempo at the press: in a bar it doubles (the most) or reaches the floor (1 BPM; 20 with CLOCK OUT,
 *              a DAW's least); let go, it snaps back at once
 * FLIP RETURN (< >): SLIP, the tracks come back where they would have been; CARRY, they go on from where the effect
 * left them (an offset per track until the transport stops). With the DAW's clock in (SYNC), the DAW stays the master:
 * a RAMP takes the sequencer off its clock only while held (the DAW's pulses still counted) and, let go, puts it back
 * at the DAW's place; FLIP and LOOP BACK never leave the clock. The step a track plays and the step a note records into
 * are the same (sq_idx), so a recording while an effect plays lands where it is heard.
 * MIDI on FX CH (as the punch-ins): notes 80 FLIP, 81 LOOP BACK, 82 RAMP UP, 83 RAMP DOWN, held as long as the pad; the
 * velocity carries the setting (FLIP 1 SLIP / 2 CARRY; LOOP BACK 1..6 the length; RAMP 1..6 the grid), so a DAW's replay
 * is exact whatever the knobs are set to then. */
enum { SQ_FLIP, SQ_LOOPBK, SQ_UP, SQ_DOWN, SQ_N };
#define SQ_NOTE0 80u
static uint8_t sq_carry;                 /* FLIP RETURN: 0 SLIP, 1 CARRY (< > on the page; project.c smkset) */
static uint8_t sq_lb = 1;                /* LOOP BACK's length: SQ_LB_Q index (PRESETS on the page; 1 bar) */
static uint8_t sq_grid;                  /* RAMP's grid: SQ_GRID_Q index (ALGORITHM on the page) */
#define SQ_LB_N 6u
#define SQ_GRID_N 6u
static const uint8_t SQ_LB_Q[SQ_LB_N] = {32, 16, 8, 4, 2, 1};   /* in 1/16 notes: 2 bars, 1 bar, 1/2 bar, 1/4, 1/8, 1/16 */
static const char *const SQ_LB_NAME[SQ_LB_N] = {"2 bars", "1 bar", "1/2 bar", "1/4", "1/8", "1/16"};
static const uint8_t SQ_GRID_Q[SQ_GRID_N] = {16, 8, 4, 24, 12, 6};   /* a grid step in 1/16 beats: 1/4 .. 1/16. */
static const char *const SQ_GRID_NAME[SQ_GRID_N] = {"1/4", "1/8", "1/16", "1/4.", "1/8.", "1/16."};
static uint8_t sq_mode;                  /* the step effect playing: 0 none, 1 FLIP, 2 LOOP BACK */
static uint8_t sq_mode_carry;            /* .. its FLIP RETURN (the menu's, or a MIDI note's) */
static uint8_t sq_held;                  /* bit k: effect k held (a pad or an FX CH note) */
static struct {
    uint32_t a0;                         /* FLIP: the step last played at the press; LOOP BACK: the loop's end (the next) */
    uint16_t i0;                         /* FLIP: the index it played */
    uint16_t lb;                         /* LOOP BACK: the loop's steps */
    uint16_t off;                        /* CARRY: steps ahead of the grid (mod the length) */
    uint8_t on;                          /* the effect anchored on this track */
} sq_t[NTRK];
static struct {
    int8_t dir;                          /* 0 none, 1 up, -1 down */
    uint8_t ext;                         /* the DAW's clock drove the sequencer at the press */
    uint32_t q8, q0, lo;                 /* the tempo now, at the press, the floor; BPM x 256 */
    uint32_t g;                          /* a grid step, clock units */
    uint32_t gp;                         /* units into the grid step */
    uint32_t rem;                        /* (the units' fraction, / 256) */
    uint64_t own, ext_u;                 /* units: the sequencer's, the DAW's, since the press */
    uint32_t last;                       /* the units of the last block (CLOCK OUT follows the ramp) */
} sq_r;

/* the step track t plays (or records into) at grid step abs */
static uint32_t sq_idx(const track_t *t, uint32_t abs, uint32_t len)
{
    uint32_t i = trk_index(t) % NTRK, x = abs;
    if (sq_mode && sq_t[i].on && abs != SEQ_NONE && (int32_t)(abs - sq_t[i].a0) >= 0) {
        uint32_t d = abs - sq_t[i].a0;
        if (sq_mode == 1u)
            return (sq_t[i].i0 + len - d % len) % len;
        x = sq_t[i].a0 - sq_t[i].lb + d % sq_t[i].lb;
    }
    return (x + sq_t[i].off) % len;
}

static void sq_reset(void)                /* the transport starts: no effect, no offset, no ramp */
{
    memset(sq_t, 0, sizeof sq_t);
    sq_mode = 0;
    sq_r.dir = 0;
}

static void sq_step_on(uint32_t mode, uint32_t carry, uint32_t lb_q)   /* FLIP (1) / LOOP BACK (2, lb_q 1/16 notes) */
{
    uint32_t i;
    if (!song.playing)
        return;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        uint32_t lb = lb_q * (BEAT_U / 4u) / div_units(trk_div(t));
        sq_t[i].on = (uint8_t)(t->seq_abs != SEQ_NONE);
        if (!sq_t[i].on)
            continue;
        if (mode == 1u) {
            sq_t[i].a0 = t->seq_abs;
            sq_t[i].i0 = (uint16_t)(t->seq_idx % trk_len(t));
        } else {
            sq_t[i].a0 = t->seq_abs + 1u;
            sq_t[i].lb = (uint16_t)(lb < 1u ? 1u : lb > 4096u ? 4096u : lb);
        }
    }
    sq_mode = (uint8_t)mode;
    sq_mode_carry = (uint8_t)carry;
}

static void sq_step_off(void)             /* FLIP / LOOP BACK let go: SLIP or CARRY */
{
    uint32_t i;
    if (!sq_mode)
        return;
    if (sq_mode_carry)
        for (i = 0; i < NTRK; i++) {
            track_t *t = &trk[i];
            uint32_t len = trk_len(t);
            if (sq_t[i].on && t->seq_abs != SEQ_NONE)   /* (from here, the step after the one it played) */
                sq_t[i].off = (uint16_t)((t->seq_idx % len + len - t->seq_abs % len) % len);
        }
    sq_mode = 0;
    for (i = 0; i < NTRK; i++)
        sq_t[i].on = 0;
}

static void sq_ramp_on(int32_t dir, uint32_t grid)
{
    uint32_t q16 = SQ_GRID_Q[grid % SQ_GRID_N];
    if (!song.playing)
        return;
    sq_r.dir = (int8_t)dir;
    sq_r.ext = (uint8_t)mclk_on();
    sq_r.q0 = sq_r.q8 = (uint32_t)song.g[G_BPM] << 8;
    sq_r.lo = (song.g[G_CLKOUT] && !song.g[G_SYNC] ? 20u : 1u) << 8;
    sq_r.g = BEAT_U / 16u * q16;
    sq_r.gp = (uint32_t)(((uint64_t)(clk_beat % 48u) * BEAT_U + clk_pos) % sq_r.g);   /* (on the song's grid) */
    sq_r.rem = 0;
    sq_r.own = sq_r.ext_u = 0;
}

static void sq_ramp_off(void)             /* let go: the tempo back; on the DAW's clock again, at its place */
{
    uint32_t i;
    if (!sq_r.dir)
        return;
    sq_r.dir = 0;
    if (sq_r.ext && song.playing) {
        int64_t d = (int64_t)sq_r.ext_u - (int64_t)sq_r.own, pos = (int64_t)clk_beat * BEAT_U + clk_pos + d;
        if (pos < 0)
            pos = 0;
        clk_beat = (uint32_t)(pos / BEAT_U);
        clk_pos = (uint32_t)(pos % BEAT_U);
        for (i = 0; i < NTRK; i++)
            trk[i].seq_abs = SEQ_NONE;       /* (each from the step the grid is in: no catch-up burst) */
    }
}

/* events_block: the units the clock moves this block (adv: what it would without a ramp) */
static uint32_t sq_adv(uint32_t n, uint32_t adv)
{
    uint32_t a, step;
    if (!sq_r.dir || !song.playing)
        return adv;
    if (sq_r.ext)
        sq_r.ext_u += adv;                   /* (the DAW's pulses, still counted) */
    a = n * sq_r.q8 + sq_r.rem;
    sq_r.rem = a & 255u;
    a >>= 8;
    sq_r.own += a;
    sq_r.last = a;
    step = (uint32_t)((uint64_t)sq_r.q0 * sq_r.g / (4u * BEAT_U));   /* 1 / (steps a bar) of the tempo at the press */
    for (sq_r.gp += a; sq_r.gp >= sq_r.g; sq_r.gp -= sq_r.g) {
        if (sq_r.dir > 0)
            sq_r.q8 = sq_r.q8 + step > 2u * sq_r.q0 ? 2u * sq_r.q0 : sq_r.q8 + step;
        else
            sq_r.q8 = sq_r.q8 < sq_r.lo + step ? sq_r.lo : sq_r.q8 - step;
    }
    return a;
}
static uint32_t sq_tempo_shown(void) { return sq_r.dir ? (sq_r.q8 + 128u) >> 8 : 0u; }   /* (the page: 0 none) */

/* an effect held or let go: k SQ_*, the setting it plays with (FLIP: carry; LOOP BACK: SQ_LB_Q index; RAMP: SQ_GRID) */
static void sq_set(uint32_t k, int on, uint32_t setting)
{
    uint32_t bit = 1u << k;
    if (on) {
        sq_held |= (uint8_t)bit;
        if (k == SQ_FLIP)
            sq_step_on(1u, setting & 1u, 0u);
        else if (k == SQ_LOOPBK)
            sq_step_on(2u, sq_carry, SQ_LB_Q[setting % SQ_LB_N]);
        else
            sq_ramp_on(k == SQ_UP ? 1 : -1, setting);
        return;
    }
    if (!(sq_held & bit))
        return;
    sq_held &= (uint8_t)~bit;
    if (k <= SQ_LOOPBK) {
        if (sq_mode == (k == SQ_FLIP ? 1u : 2u))
            sq_step_off();
    } else if ((k == SQ_UP) == (sq_r.dir > 0)) {
        sq_ramp_off();
    }
}
static uint32_t sq_setting(uint32_t k)    /* the page's setting for effect k */
{
    return k == SQ_FLIP ? sq_carry : k == SQ_LOOPBK ? sq_lb : sq_grid;
}

/* FX CH (seq.c midi_fx's channel): notes 80..83, the velocity the setting */
static void sq_midi(uint32_t note, int on, uint32_t vel)
{
    uint32_t k = note - SQ_NOTE0, s;
    if (k >= SQ_N)
        return;
    s = vel ? vel - 1u : sq_setting(k);
    sq_set(k, on, k == SQ_FLIP ? (vel == 2u) : k == SQ_LOOPBK ? s % SQ_LB_N : s % SQ_GRID_N);
}
