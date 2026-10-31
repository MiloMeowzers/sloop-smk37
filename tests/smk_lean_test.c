/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37, the vocoder build lean (SMK_VOC_LEAN=1, FELUCCA_SLICE=1; core.h): what it leaves out stays out, what it
 * keeps still works (the ui_pages_test harness, the samples header made with SMK_VOC_LEAN=1).
 *   presets  every PRESETS entry (ui.c BANK) found, of an engine the build has; every preset of such an engine there once
 *   numbers  the engines left out keep their numbers and come in as ANALOG (a project's: said on the screen);
 *            KNOB 2 on the PRESETS page steps past them
 *   sources  SAMPLE, GRAIN, SLICE: the user slots only; the drum kits from DRUM_SAMPLED (no GM kit samples)
 *   delay    the line stored at 22.05 kHz: a level comes back after the delay time at that level, nothing before,
 *            at 1/8 at 120 BPM and at the longest (1/4D at 61 BPM, the whole line) */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
#define PROJ_HOST 1
#include "../firmware/src/project.c"         /* (the project formats' part: proj_capture, proj_apply) */

#if !SMK_VOC_LEAN || !FELUCCA_SLICE
#error "build with -DSMK_VOC_LEAN=1 -DFELUCCA_SLICE=1"
#endif

/* G_DTIME d at bpm: the delay's samples, and the wet output where a level of 8000 sent at sample 0 (for 400 samples)
 * comes back; 1 = it is back at its level (2 %) from the delay time on, silent before */
static int delay_back(uint32_t d, uint32_t bpm)
{
    static int32_t in[32], zero[32], wl[32], wr[32];
    uint32_t i, t, dl, ok = 1;
    song.g[G_BPM] = (int16_t)bpm;
    song.g[G_DTIME] = (int16_t)d;
    song.g[G_DFDBK] = 0;
    song.g[G_DMIX] = 127;
    song.g[G_DCOLOR] = 127;
    memset(dly_buf, 0, sizeof dly_buf);
    fx.dly_lp = 0;
    dl = delay_samples();
    for (t = 0; t < dl + 400u; t += 32u) {
        for (i = 0; i < 32u; i++)
            in[i] = t + i < 400u ? 8000 : 0;
        fx_buses(zero, in, zero, wl, wr, 32);
        for (i = 0; i < 32u; i++) {
            uint32_t s = t + i;
            int32_t want = mulq15(8000, 127 * 258);     /* (as the 44.1 kHz line: x = in / 2, out = 2x * MIX) */
            if (s + 2u < dl && wl[i] != 0)
                ok = 0;
            if (s > dl + 2u && s + 2u < dl + 400u && (wl[i] < want * 98 / 100 || wl[i] > want * 102 / 100))
                ok = 0;
        }
    }
    printf("lean: delay G_DTIME %u at %u BPM: %u samples (%u ms)\n", d, bpm, dl, dl * 1000u / 44100u);
    return (int)ok;
}

int main(void)
{
    uint32_t e, k, n, hits, bad = 0, gone = 0;
    {   /* the PRESETS list */
        bank_resolve();
        for (n = 0; n < NBANK; n++)
            if (bank_pi[n] == 0xFF || eng_live(BANK[n].e) != BANK[n].e) {
                printf("lean: BANK %s (engine %u): not in this build\n", BANK[n].name, BANK[n].e);
                bad++;
            }
        for (e = 0; e < NENGINES; e++) {
            if (eng_live(e) != e) {
                gone++;
                continue;
            }
            for (k = 0; k < ENGINES[e]->npresets; k++) {
                for (hits = 0, n = 0; n < NBANK; n++)
                    hits += BANK[n].e == e && bank_pi[n] == k;
                if (hits != 1) {
                    printf("lean: preset %s (engine %u) is %u times in BANK\n", ENGINES[e]->presets[k].name, e, hits);
                    bad++;
                }
            }
        }
        check(!bad, "PRESETS: every entry of an engine built, every preset of one once");
        printf("lean: %u entries in PRESETS\n", (unsigned)NBANK);
    }
    check(gone == 5u && eng_live(1) == 0 && eng_live(3) == 0 && eng_live(5) == 0 && eng_live(6) == 0 &&
          eng_live(ENGI_NOISE) == 0, "DIGITAL, LOFI, VOICE, TRIO, NOISE left out: their numbers come in as ANALOG");
    check(eng_live(0) == 0 && eng_live(2) == 2 && eng_live(4) == 4 && eng_live(7) == 7 && eng_live(ENGI_GRAIN) ==
          ENGI_GRAIN && eng_live(ENGI_FM6) == ENGI_FM6 && eng_live(ENGI_PHYS) == ENGI_PHYS &&
          eng_live(ENGI_SLICE) == ENGI_SLICE, "ANALOG, PHASE, SAMPLE, WHEEL, GRAIN, FM6, PHYS, SLICE kept");
    {   /* a project with DIGITAL on track 1 */
        static project_t p;
        proj_capture(&p);
        p.t[0].engine = 1;
        proj_gone = 0;
        proj_apply(&p, 1);
        check(trk[0].eng_req == 0 && proj_gone, "a project's DIGITAL track loads as ANALOG, and says so");
        proj_gone = 0;
    }
    {   /* KNOB 2 on the PRESETS page: every step an engine the build has */
        uint32_t ok = 1, seen = 0;
        trk[0].eng_req = 0;
        for (n = 0; n < 2u * NENGINES; n++) {
            uint32_t s = trk[0].eng_req;
            do
                s = (s + 1u) % NENGINES;
            while (s != eng_live(s));
            ok &= s == eng_live(s);
            seen |= 1u << s;
            trk[0].eng_req = (uint8_t)s;
        }
        check(ok && seen == (1u << 0 | 1u << 2 | 1u << 4 | 1u << 7 | 1u << ENGI_GRAIN | 1u << ENGI_FM6 |
                              1u << ENGI_PHYS | 1u << ENGI_SLICE), "the engines stepped: the eight built, each");
    }
    check(SMP_NSETS == 8 && SMP_SETS[0].nz == 0 && SMP_SETS[7].nz == 0 && ENG_SAMPLE.edit[0].min == SMP_NSETS &&
          ENG_GRAIN.edit[0].min == SMP_NSETS && ENG_SAMPLE.npresets == 1 && ENG_GRAIN.npresets == 1 &&
          ENG_SAMPLE.presets[0].e[0] == SMP_NSETS && ENG_GRAIN.presets[0].e[0] == SMP_NSETS,
          "SAMPLE, GRAIN: the 8 sets' numbers kept, empty; USR1..4 only, one sound each on USR1");
    check(ENG_SLICE.edit[0].min == 1 && ENG_SLICE.npresets == 1 && SLC_BREAK.len == 0,
          "SLICE: no built-in BREAK, the user slots only");
    check(DRUM_KIT_DESC.min == DRUM_SAMPLED && DRUM_KIT_DESC.def == DRUM_SAMPLED && drum_set() < 0,
          "drum kits: the sampled GM kits out, from the 808 on (the user kits kept)");
    check(sizeof dly_buf == DLY_LEN, "the delay line: 32768 samples (64 KB), the same 1.49 s");
    check(delay_back(1, 120), "the delay at 22.05 kHz: back after 1/8 at 120 BPM, at its level");
    check(delay_back(8, 61), "the delay at 22.05 kHz: back after its longest (1/4D, 61 BPM), at its level");
    printf("smk lean: %s\n", fails ? "FAIL" : "all ok");
    return fails != 0;
}
