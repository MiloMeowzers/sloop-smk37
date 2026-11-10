/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37, the vocoder build's console (SMK_TIMING): where an audio half's time goes, to find what lines up when it
 * sheds (audio.c: two halves in a row over 85 %, TIMER5 nested included). Core 0's mix_block in sections, each core's
 * units one by one, core 1's stems, the vocoder's synthesis, the blocks' tails (fx.c MIX_PIPE: on core 1, alongside
 * core 0's next block), summed over the half's blocks; at the half's end (audio.c
 * tim_half) each figure's worst, the worst half's own split, and the halves over the shed line: how many, their figures
 * summed. TIMER4 ticks; a section includes any TIMER5 nested in it (T5: the half's nested total). Console "timing"
 * (read, then cleared). Nothing in any other build */
#ifndef SMK_TIMING
#define SMK_TIMING (BOARD_SMK37 && FELUCCA_CDC && SMK_UAC_IN_PROBE && !SMK_UAC_PROBE)
#endif
#if SMK_TIMING
enum {
    TS_PEV,      /* the block's start: the sums cleared, the throws, events (the sequencer, the arp, notes), duck */
    TS_PIN,      /* .. the USB voice in (uin_block) */
    TS_PTP,      /* .. the stems' punch-ins' timeline (tp_block) */
    TS_TW,       /* .. waiting for the block before's tail (core 1), its output finished (audio.c mix_done) */
    TS_PGO,      /* .. the sums cleared, the throws, the analysis's buffers flipped, core 1's job handed over */
    TS_ANA,      /* the vocoder's analysis */
    TS_UNITS,    /* core 0's units (U1..UD) */
    TS_STEM,     /* core 0's share of the stems */
    TS_WAIT,     /* waiting for core 1, adding its sums, the stems committed */
    TS_TAIL,     /* the tail handed to core 1 (core 0's last section) */
    TS_FX,       /* the tail (core 1, alongside core 0's sections): the buses (chorus, delay, reverb) */
    TS_POST,     /* .. dust, punch-in, DJ filter, master, limiter, the stems staged */
    TS_EVX,      /* inside pre_ev (seq.c events_block): the transport, the count-in, CLK OUT, the arranger, the fills */
    TS_EVT,      /* .. the routing, a panic, the tracks' per-block checks (engine_block, the arp / KBD HOLD flags) */
    TS_EVK,      /* .. the keys (keyboard_block), the drum audition, the strums */
    TS_EVM,      /* .. USB / TRS MIDI in */
    TS_ES1, TS_ES2, TS_ES3, TS_ESD,   /* .. the sequencer (seq_tick): tracks 1..3, the drums */
    TS_EVA,      /* .. the metronome, ROLL, the arps, the clock */
    TS_U1, TS_U2, TS_U3, TS_UD,                         /* core 0's units: tracks 1..3, the drums (0: on core 1) */
    TS_C1,       /* core 1's job */
    TS_C1U1, TS_C1U2, TS_C1U3, TS_C1UD,                 /* .. its units (0: on core 0) */
    TS_C1ST,     /* .. its share of the stems */
    TS_VOC,      /* the vocoder's synthesis (voc_process, on whichever core track 3 renders: inside U3 or C1U3) */
    TS_REST,     /* the half outside mix_block's sections (shed, the USB render's start, the output) */
    TS_T5,       /* TIMER5 nested in the half (USB audio), inside the figures above */
    TS_ALL,      /* the whole half */
    TS_E1, TS_E2, TS_E3,  /* (§7g) the engines alone (voice.c track_render: every voice's render, the vocoder apart), tracks
                           * 1..3 on whichever core: the unit (u / c1_u) less this, the track's chain (filter, DIST, INSERT, mix) */
    TS_P1, TS_P2, TS_P3,  /* (the UI build, 2026-11-02) the chain's first part, tracks 1..3 (fx.c mix_part_run, heard): DIST,
                           * INSERT, SLICER, FILTER */
    TS_M1, TS_M2, TS_M3,  /* .. its loop: LEVEL, the mute / duck, STEM FX (tp_x), the stem, the sends, PAN */
    TS_HARM,     /* (2026-11-07) the harmoniser's probe (smk_harm.h hp_block, core 0, inside pre_in) */
    TS_TRK,      /* (2026-11-10) .. its tracker (hp_lags, inside harm) */
    TS_N
};
static const char *const TS_NAME[TS_N] = {"pre_ev", "pre_in", "pre_tp", "tail_wait", "pre_go", "ana", "units",
                                          "stem", "wait", "tail_go", "fx", "post", "ev_xport", "ev_trk", "ev_keys",
                                          "ev_midi", "ev_s1", "ev_s2", "ev_s3", "ev_sd", "ev_arp", "u1", "u2", "u3", "ud",
                                          "c1",
                                          "c1_u1", "c1_u2", "c1_u3", "c1_ud", "c1_stem", "voc", "rest", "t5", "all",
                                          "eng1", "eng2", "eng3", "pre1", "pre2", "pre3", "mix1", "mix2", "mix3", "harm", "trk"};
static struct {
    uint32_t t, ut[2];                                  /* the section's start; the unit's, each core */
    uint32_t cur[TS_N];                                 /* this half's */
    uint32_t max[TS_N], worst[TS_N], over[TS_N];        /* each worst; the worst half's; the shedding halves' summed */
    uint32_t sum[TS_N];                                 /* (§7g) every half's summed, in 16 ticks (2/3 us): 48 minutes of
                                                         * halves before it wraps (the console reads and clears it) */
    uint32_t halves, n_over;
} tim;
/* TIMER4 read in place (hal/fm1_time.h fm1_ticks_inl, as fm1_ticks): the tail's code runs from RAM (fx.c RAMHOT),
 * where a call to fm1_ticks in the flash is out of reach */
#define TIM_NOW() fm1_ticks_inl()
#define TIM_START() (tim.t = TIM_NOW())
#define TIM_MARK(s) do { uint32_t tm_ = TIM_NOW(); tim.cur[s] += tm_ - tim.t; tim.t = tm_; } while (0)
#define TIM_ADD(s, t) (tim.cur[s] += (t))
/* the units, on either core (mx: the core's sums, acc[0] core 0's): each core its own start and slots */
#define TIM_U0(mx) (tim.ut[(mx) != &acc[0]] = TIM_NOW())
#define TIM_UNIT(mx, k) do { uint32_t c_ = (mx) != &acc[0], tm_ = TIM_NOW(); \
                             tim.cur[(c_ ? TS_C1U1 : TS_U1) + (k)] += tm_ - tim.ut[c_]; tim.ut[c_] = tm_; } while (0)
#define TIM_VOC_T(v) uint32_t v = TIM_NOW()
#define TIM_VOC(v) TIM_ADD(TS_VOC, TIM_NOW() - (v))
#define TIM_LAP(s, v) do { uint32_t tm_ = TIM_NOW(); TIM_ADD(s, tm_ - (v)); (v) = tm_; } while (0)   /* (the tail) */
static void tim_half(uint32_t all, uint32_t t5, uint32_t over)   /* audio.c, each half: all and t5 in ticks */
{
    uint32_t i, s = 0;
    for (i = TS_PEV; i <= TS_TAIL; i++)                  /* (core 0's sections) */
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
    for (i = 0; i < TS_N; i++)
        tim.sum[i] += tim.cur[i] >> 4;
    tim.halves++;
    memset(tim.cur, 0, sizeof tim.cur);
}
#else
#define TIM_START() ((void)0)
#define TIM_MARK(s) ((void)0)
#define TIM_ADD(s, t) ((void)0)
#define TIM_U0(mx) ((void)0)
#define TIM_UNIT(mx, k) ((void)0)
#define TIM_VOC_T(v)
#define TIM_VOC(v) ((void)0)
#define TIM_LAP(s, v) ((void)0)
#endif
