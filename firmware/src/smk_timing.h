/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37, the vocoder build's console (SMK_TIMING): where an audio half's time goes, to find what lines up when it
 * sheds (audio.c: two halves in a row over 85 %, TIMER5 nested included). Core 0's mix_block in sections and its units
 * one by one (core 1's are in its job's time), summed over the half's blocks; at the half's end (audio.c tim_half)
 * each figure's worst, the worst half's own split, and the halves over the shed line: how many, their figures summed.
 * TIMER4 ticks; a section includes any TIMER5 nested in it (T5: the half's nested total). Console "timing" (read,
 * then cleared). Nothing in any other build */
#ifndef SMK_TIMING
#define SMK_TIMING (BOARD_SMK37 && FELUCCA_CDC && SMK_UAC_IN_PROBE && !SMK_UAC_PROBE)
#endif
#if SMK_TIMING
enum {
    TS_PRE,      /* the block's start: events, the input, the stems' punch-ins, core 1's job handed over */
    TS_ANA,      /* the vocoder's analysis */
    TS_UNITS,    /* core 0's units (TU_* below one by one) */
    TS_STEM,     /* core 0's share of the stems */
    TS_WAIT,     /* waiting for core 1, adding its sums, the stems committed */
    TS_FX,       /* the buses (chorus, delay, reverb) */
    TS_POST,     /* dust, punch-in, DJ filter, master, limiter, the stems staged */
    TS_U1, TS_U2, TS_U3, TS_UD,                         /* core 0's units: tracks 1..3, the drums (0: on core 1) */
    TS_C1,       /* core 1's job (its units and stems) */
    TS_REST,     /* the half outside mix_block's sections (shed, the USB render's start, the output) */
    TS_T5,       /* TIMER5 nested in the half (USB audio), inside the figures above */
    TS_ALL,      /* the whole half */
    TS_N
};
static const char *const TS_NAME[TS_N] = {"pre", "ana", "units", "stem", "wait", "fx", "post", "u1", "u2", "u3", "ud",
                                          "c1", "rest", "t5", "all"};
static struct {
    uint32_t t, ut;                                     /* the section's start; the unit's */
    uint32_t cur[TS_N];                                 /* this half's */
    uint32_t max[TS_N], worst[TS_N], over[TS_N];        /* each worst; the worst half's; the shedding halves' summed */
    uint32_t halves, n_over;
} tim;
#define TIM_START() (tim.t = fm1_ticks())
#define TIM_MARK(s) do { uint32_t tm_ = fm1_ticks(); tim.cur[s] += tm_ - tim.t; tim.t = tm_; } while (0)
#define TIM_U0(mx) do { if ((mx) == &acc[0]) tim.ut = fm1_ticks(); } while (0)
#define TIM_UNIT(mx, k) do { if ((mx) == &acc[0]) { uint32_t tm_ = fm1_ticks(); tim.cur[TS_U1 + (k)] += tm_ - tim.ut; \
                                                    tim.ut = tm_; } } while (0)
#define TIM_C1(t) (tim.cur[TS_C1] += (t))
static void tim_half(uint32_t all, uint32_t t5, uint32_t over)   /* audio.c, each half: all and t5 in ticks */
{
    uint32_t i, s = 0;
    for (i = TS_PRE; i <= TS_POST; i++)
        s += tim.cur[i];
    tim.cur[TS_REST] = all > s ? all - s : 0u;
    tim.cur[TS_T5] = t5;
    tim.cur[TS_ALL] = all;
    for (i = 0; i < TS_N; i++)
        if (tim.cur[i] > tim.max[i])
            tim.max[i] = tim.cur[i];
    if (all > tim.worst[TS_ALL])
        memcpy(tim.worst, tim.cur, sizeof tim.worst);
    if (over) {
        for (i = 0; i < TS_N; i++)
            tim.over[i] += tim.cur[i];
        tim.n_over++;
    }
    tim.halves++;
    memset(tim.cur, 0, sizeof tim.cur);
}
#else
#define TIM_START() ((void)0)
#define TIM_MARK(s) ((void)0)
#define TIM_U0(mx) ((void)0)
#define TIM_UNIT(mx, k) ((void)0)
#define TIM_C1(t) ((void)0)
#endif
