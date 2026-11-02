/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37, the vocoder build (SMK_VOC_LEAN, smk37-devtools vocoder-ui-spec.md §7g): FM6's operator loops as that build
 * runs them (fm6_core.c FM6_FAST: the feedback operator in a += and a = loop, two samples a turn) and the sine from its
 * RAM copy (dsp.c sine_ram), against the loops as they were, copied here (ref_*): every output sample and the feedback
 * state the same, bit for bit, over random phases, frequencies, gain ramps (rising, falling, from and to 0, msfa's
 * full range), modulator buses and starting outputs, every feedback shift, add or not, with a bus or none.
 *   build/host/fm6_loops_test */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "felucca_tables.h"
#define SMK_VOC_LEAN 1
#define FELUCCA_SLICE 1
#include "core.h"                     /* (dsp.c's types) */
#include "../firmware/src/dsp.c"
#include "../firmware/src/fm6_core.c"

#if !FM6_FAST
#error "fm6_loops_test: FM6_FAST is off"
#endif

/* ---- the loops as they were (fm6_core.c before FM6_FAST), on the flash table ---- */
static inline int32_t ref_sin(int32_t ph)
{
    const int16_t *s = &SINE[((uint32_t)ph >> 14) & 1023u];
    return s[0] + (((s[1] - s[0]) * (int32_t)(ph & 0x3FFF)) >> 14);
}
#define REF_OP_LOOP(PH, STORE)                                                    \
    for (i = 0; i < FM6_N; i++) {                                               \
        g += dg;                                                                \
        out[i] STORE (int32_t)(((int64_t)ref_sin(PH) * g) >> 15);               \
        phase = (int32_t)((uint32_t)phase + (uint32_t)freq);                    \
    }
static void ref_op_run(int32_t *out, const int32_t *in, int32_t phase, int32_t freq, int32_t g1, int32_t g2, int add)
{
    int32_t dg = (g2 - g1 + (FM6_N >> 1)) >> FM6_LG_N, g = g1;
    uint32_t i;
    if (in && add)
        REF_OP_LOOP(phase + in[i], +=)
    else if (in)
        REF_OP_LOOP(phase + in[i], =)
    else if (add)
        REF_OP_LOOP(phase, +=)
    else
        REF_OP_LOOP(phase, =)
}
static void ref_op_fb(int32_t *out, int32_t phase, int32_t freq, int32_t g1, int32_t g2, int32_t *fb, int32_t shift,
                      int add)
{
    int32_t dg = (g2 - g1 + (FM6_N >> 1)) >> FM6_LG_N, g = g1, y0 = fb[0], y = fb[1];
    uint32_t i;
    for (i = 0; i < FM6_N; i++) {
        int32_t s = (y0 + y) >> (shift + 1);
        g += dg;
        y0 = y;
        y = (int32_t)(((int64_t)ref_sin(phase + s) * g) >> 15);
        out[i] = add ? out[i] + y : y;
        phase = (int32_t)((uint32_t)phase + (uint32_t)freq);
    }
    fb[0] = y0;
    fb[1] = y;
}

static uint32_t rs = 0x2468ACEu;
static uint32_t rnd(void)
{
    rs ^= rs << 13;
    rs ^= rs >> 17;
    rs ^= rs << 5;
    return rs;
}
static int32_t gain(void)                                  /* msfa's gains: 0 .. 2^27 (fm6_exp2 of up to 17 << 24) */
{
    switch (rnd() & 3u) {
    case 0: return 0;
    case 1: return (int32_t)(rnd() & 0x7FFFFFFu);
    case 2: return (int32_t)(rnd() & 0xFFFFFu);              /* quiet: around the threshold */
    default: return 1 << 27;
    }
}

int main(void)
{
    static int32_t in[FM6_N], a[FM6_N], b[FM6_N];
    uint32_t t, k, bad = 0, runs = 0, fbs = 0;
    sine_ram_load();
    if (memcmp(sine_ram, SINE, sizeof sine_ram)) {
        printf("fm6_loops: the RAM sine is SINE                                                   FAIL\n");
        return 1;
    }
    for (t = 0; t < 400000u; t++) {
        int32_t phase = (int32_t)rnd(), freq = (int32_t)(rnd() >> (rnd() & 15u)), g1 = gain(), g2 = gain();
        int add = (int)(rnd() & 1u), bus = (int)(rnd() & 1u);
        for (k = 0; k < FM6_N; k++) {
            in[k] = (int32_t)rnd() >> (rnd() & 7u);           /* (a modulator's Q24 output, up to full scale and past it) */
            a[k] = b[k] = (int32_t)rnd() >> 4;
        }
        if (t & 1u) {
            int32_t fa[2], fr[2], sh = (int32_t)(rnd() % 9u);   /* (fm6_core_render: 8 - fb, fb 1..7; 8: the lowest) */
            fa[0] = fr[0] = (int32_t)rnd() >> 3;
            fa[1] = fr[1] = (int32_t)rnd() >> 3;
            fm6_op_fb(a, phase, freq, g1, g2, fa, sh, add);
            ref_op_fb(b, phase, freq, g1, g2, fr, sh, add);
            bad += memcmp(a, b, sizeof a) != 0 || fa[0] != fr[0] || fa[1] != fr[1];
            fbs++;
        } else {
            fm6_op_run(a, bus ? in : 0, phase, freq, g1, g2, add);
            ref_op_run(b, bus ? in : 0, phase, freq, g1, g2, add);
            bad += memcmp(a, b, sizeof a) != 0;
            runs++;
        }
    }
    printf("fm6_loops: the RAM sine is SINE                                                   ok\n");
    printf("fm6_loops: %u operator blocks and %u feedback blocks the same as the loops before   %s\n", runs, fbs,
           bad ? "FAIL" : "ok");
    return bad != 0;
}
