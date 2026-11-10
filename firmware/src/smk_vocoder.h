/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 audio track and vocoder (SMK_UAC_IN_PROBE builds: the vocoder candidate, smk37-devtools README "Candidate"
 * and vocoder-harmoniser-reference.md). The owner's design (2026-10-30): while HOME > USB > AUDIO IN is on, track 3 is
 * the AUDIO TRACK (SLOOP keeps four: two engine tracks, the audio track, the drums). Its sound, made in voice.c
 * track_render before the track's own chain (DIST, INSERT, SLICER, FILTER, LEVEL, PAN, the sends, the punch-ins, its
 * stem), is set by HOME > VOCODER > MODE:
 *   AUDIO    the computer's audio (usb.c, UIN_EP OUT), mono, dry: external audio in the mix with SLOOP's effects
 *   VOCODER  the vocoder, its carrier plain band-limited saws on track 3's notes (keys, sequencer, arp; voice.c
 *            voc_saw), through the track's ADSR and a resonant low-pass (HOME > CARRIER > CUTOFF, RESO; the track's
 *            ENV > FLT and LFO > FLT move it)
 * Track 3's engine has no part in it (2026-10-30, the owner's: an ENGINE carrier is gone): while AUDIO IN is on the
 * track shows AUDIO or VOCODER, not its engine, and its engine and presets cannot be changed (ui.c); its engine, sound
 * and values stay as they were for when AUDIO IN is off again.
 * A change of state (MODE, AUDIO IN) fades the track out and in again (~5 ms each way): no click.
 * The two cores (fx.c mix_block): the analysis (the voice's bank, its envelopes) runs on core 0 while core 1 renders
 * track 3 (the synthesis), a block apart: the synthesis takes the envelopes core 0 made the block before (0.7 ms, well
 * inside an envelope's ~3 ms attack). Core 1 renders track 3 with the drums (fx.c units_split): the vocoder whole on it
 * made core 0 wait for it a third of the time (2026-10-30, SYNTHS+RET, 18 voices shed a second).
 * The vocoder, after the Sennheiser VSM201:
 *   rate       the voice and the carrier down 2:1 to 22050 Hz (the VSM201's range is 100 Hz - 8 kHz), the output up
 *              2:1 again, through a half-band FIR (27 taps, 8 not zero; ~0.3 ms)
 *   analysis   the voice (mono, Q15, this block: voc_in, usb.c uin_block) through 20 band-passes at the VSM201's
 *              channel centres (smk_vocoder_tab.h, tools/gen_vocoder.py), 4th order (two sections: 12 dB an octave,
 *              a band's envelope stays its own), each full-wave rectified and smoothed (attack ~3 ms, release ~8 ms:
 *              the VSM201's 20 - 50 Hz channel low-passes)
 *   synthesis  the carrier through the same 20 band-passes (4th order: a bright carrier's strong fundamental stays
 *              out of the bands above it), each scaled by its band's envelope, summed; LEVEL the make-up gain
 *   HF         the VSM201's speech addition: the voice's own bands above 4 kHz (4560, 5570, 6780 Hz) added to the
 *              output, for the consonants (the owner chose it over the hiss generator, which was hissy and imprecise)
 *   DRY        the plain voice blended back (the VSM201's bypass)
 *   VOICE      the VSM201's voice mode detector (manual 3.5, 3.7): channel 19's envelope (5570 Hz) against a channel 0
 *              of its own, a low-pass below 1 kHz (two one-poles), compared: voiced while channel 0 is over twice
 *              channel 19 (hysteresis: unvoiced again under 1.4 x), unvoiced otherwise (a high sound alone, or none).
 *              AUTO: voiced, the saws into the synthesis bank; unvoiced, the internal noise instead (switched over
 *              ~3 ms: no click). VOICED / UNVOICED: the one always. The noise is level-matched to the saws (manual:
 *              "by means of a rectifier and modulator": the carrier's rectified level, ~50 Hz, times a noise with the
 *              rms a saw has over its mean), white or through the pink filter (CARRIER > NOISE)
 *   BRIDGE     the VSM201's silence bridging (manual 2.9, 3.7: pause filling, its sum control): the voice's level
 *              (rectified, unfiltered), in dB: 0 above THRESHOLD + 24 dB, rising to full as the voice falls to
 *              THRESHOLD (the log amplifier and inverter), full below it (the speech / pause detector: a pause); that
 *              value x BRIDGE added to every band's envelope (into the analysis low-passes): the saws play on through
 *              the pauses. With it on the detector no longer switches: the saws stay, the noise is added on unvoiced
 *              speech (manual 3.7)
 *   Both the detector and the pause value are worked out once a block (1.4 kHz) on core 0 with the analysis, handed
 *   over with the envelopes.
 * Fixed point: the band-passes Q30 coefficients on 64-bit products, rounded, their signals 4 bits above the input's
 * (a low, narrow band's recursion: truncation read 0.6 dB high at 145 Hz); the envelopes Q19. The track's scale: a
 * voice at full scale 2 x VOICE_FS (dsp.c); the voice (Q15) is put on it as one such voice. */
#include "smk_vocoder_tab.h"

#ifndef VOC_MAXN
#define VOC_MAXN 32u                     /* the largest block (CTL); even */
#endif
#define VOC_TRK 2u                       /* track 3: the audio track */
#if !SMK_UAC_PROBE
static uint8_t uin_mode;                 /* HOME > USB > AUDIO IN (usb.c): 0 OFF, 1 ON, 2 TEST; live, not kept */
#define VOC_AUDIO_ON (uin_mode != 0u)
#else
#define VOC_AUDIO_ON 0                   /* (the loopback probe: track 3 its engine; the input on the monitor) */
#endif
enum { VM_OFF, VM_SAW, VM_N };
static uint8_t voc_mode;                 /* MODE: VM_OFF (AUDIO: the dry audio), VM_SAW (VOCODER) */
static uint8_t voc_cut = 127;            /* CARRIER > CUTOFF, RESO: the saws' low-pass (0 .. 127, dsp.c tsvf_coef) */
static uint8_t voc_res;
/* SETUP and the input's routing (vocoder-ui-spec.md §7i-2, §7j; HOME > SETUP; kept: project.c vocset rsv[1], smkset
 * spare). SETUP: which tracks render beside the audio track (FULL: all, as before) and the synthesis' rate (HQ: 44.1
 * kHz, voc_process_hq, in every SETUP but FULL). IN L / IN R: where each USB channel goes; one layer for every user of
 * the input (track 3, the carrier, T1 / T2 as audio tracks; the harmoniser later): usb.c uin_block splits the two
 * channels into a buffer a destination (uin_dst), summed where both go to one (both TRACK 3: L + R averaged, as
 * before). All only while AUDIO IN is on */
enum { VSU_FULL, VSU_DRUMS, VSU_SYNTH, VSU_ALONE, VSU_N };   /* FULL, VOC + DRUMS, VOC + SYNTH, VOC ALONE */
enum { VR_T3, VR_CARR, VR_T1, VR_T2, VR_OFF, VR_N };          /* IN L / IN R: TRACK 3, CARRIER, T1, T2, OFF */
enum { UD_T3, UD_CARR, UD_T1, UD_T2, UD_N };                  /* the destinations' buffers (VR_T3 .. VR_T2) */
static uint8_t voc_setup, voc_inl, voc_inr;
static uint8_t voc_gdel;                 /* GATE DELAY (external carrier): 0 .. 10, x 5 ms */
static uint8_t voc_hq;                   /* the synthesis' rate now (latched at a reset, voc_state): 1 HQ */
static uint8_t voc_dbg;                  /* (console "setup dbg N": bit 0 no HQ, bit 1 no two saws; 0 at boot) */
static int16_t uin_dst[UD_N][VOC_MAXN];  /* this block's input a destination (usb.c uin_block); [UD_T3] is the voice */
#define voc_in (uin_dst[UD_T3])
#define UIN_DST 1                        /* (usb.c: the destinations are here) */
#define VOC_SETUP_ON (VOC_AUDIO_ON ? voc_setup % VSU_N : (uint32_t)VSU_FULL)
#define VSU_UDR 8u                       /* (the drums' bit: fx.c U_DRUMS, asserted there) */
#define VSU_SKIP(s) ((s) == VSU_DRUMS ? 3u : (s) == VSU_SYNTH ? VSU_UDR : (s) == VSU_ALONE ? 3u | VSU_UDR : 0u)
#define VOC_ROUTE(r) (VOC_AUDIO_ON ? (r) % VR_N : (uint32_t)VR_T3)
/* track k (0, 1) plays an input: the SETUP has it off and IN L or IN R goes to it */
#define VOC_TRK_IN(k) ((VSU_SKIP(VOC_SETUP_ON) >> (k) & 1u) && \
                       (VOC_ROUTE(voc_inl) == VR_T1 + (k) || VOC_ROUTE(voc_inr) == VR_T1 + (k)))
#define VOC_EXT_CARR (VOC_ROUTE(voc_inl) == VR_CARR || VOC_ROUTE(voc_inr) == VR_CARR)
/* the units the SETUP turns off (fx.c voc_setup_block): want at once (no new notes there: voice.c trk_note_on; their
 * notes released), skip once their release has had VOC_SKIP_WAIT blocks (~0.4 s); bit k track k, bit NPART the drums */
static uint8_t voc_skip, voc_skip_want;
static uint16_t voc_skip_wait;
#define VOC_SKIP_WAIT 550u
static int32_t voc_tin_g[2];             /* T1 / T2 playing an input: its fade (~5 ms), Q15 */
static int32_t voc_xgate, voc_xhold;     /* the external carrier's gate (Q15) and its GATE DELAY left (samples) */
static uint8_t voc_xheld;                /* (a track 3 note held this block: voice.c) */
enum { VV_AUTO, VV_VOICED, VV_UNVOICED, VV_N };
static uint8_t voc_vmode;                /* CARRIER > VOICE: the VSM201's VOICE MODE (AUTOMATIC, VOICED / UNVOICED ONLY) */
static uint8_t voc_pink;                 /* CARRIER > NOISE: WHITE, PINK (the PINK FILTER switch), SOFT (2026-11-03: a saw's
                                          * -6 dB an octave, a one-pole low-pass at ~200 Hz, back to white's rms) */
#define VOC_NOISE_N 3u
#define VOC_SOFT_A 30953                 /* (exp(-2 pi 200 / 22050), Q15) */
#define VOC_SOFT_G 194000                /* (sqrt((1 + a) / (1 - a)) = 5.92, Q15: the rms back to white's) */
static uint8_t voc_bridge;               /* SILENCE > BRIDGE: OFF, -48 .. 0 dB in 3 dB steps (voc_br_q15); the manual's
                                          * basic: off */
#define VOC_BR_N 18u
static int32_t voc_br_q15(uint32_t k)    /* BRIDGE k's gain, Q15: 0 dB, 3 dB less a step down (no table) */
{
    int32_t g = 32767;
    uint32_t j;
    if (!k)
        return 0;
    for (j = k; j < VOC_BR_N - 1u; j++)
        g = (int32_t)(((int64_t)g * 23198) >> 15);       /* -3 dB */
    return g;
}
static uint8_t voc_thr = 2;              /* SILENCE > THRESHOLD: VOC_THR_DB[]; the speech / pause detector's */
#define VOC_THR_N 6u
static const int8_t VOC_THR_DB[VOC_THR_N] = {-54, -48, -42, -36, -30, -24};
/* BRIDGE TONE (the owner's, beyond the VSM201; smk37-devtools vocoder-ui-spec.md §7a): in a pause each band carries
 * the input's own envelope (COLOUR) and the bridge (BRIDGE, tilted per band: TILT), added (SUM) or the larger (FILL) */
static uint8_t voc_tilt = VOC_TILT_FLAT + 2u;   /* TILT: VOC_TILT_Q12[], -3 .. +3 dB an octave (default +1.0) */
static uint8_t voc_colour = 10;          /* COLOUR: 0 .. 10 (x 10 %): the envelopes in a pause; 0: the bands rest */
static uint8_t voc_blend;                /* BLEND: 0 SUM, 1 FILL */
static uint8_t voc_norm;                 /* NORMAL: 0 OFF (the default: the bands summed flat, as b0dde1c), 1 ON (the
                                          * VSM201's NORMAL weights, VOC_B0S: lows +4.7 dB, top -6 dB; vocoder-ui-spec.md
                                          * §7a: it took the presence the owner missed) */
#define VOC_COLOUR_N 11u
/* MOD WHEEL (VOCODER) (2026-11-06, the owner: the wheel's vibrato does not suit the carrier): 0 VIBRATO (the default, as
 * before), 1 BRIDGE TILT (wheel up: from TILT to +3 dB an octave), 2 NOISE BLEND (wheel up: from NOISE BLEND to the
 * carrier, no noise); while track 3 is selected, as the wheels act (voice.c sets voc_mw); kept (project.c rsv[0]) */
#define VOC_MWA_N 3u
static uint8_t voc_mwa;
static uint8_t voc_mw;                   /* (the wheel on the vocoder, 0 .. 127: 0 unless assigned and track 3 selected) */
#define VOC_REST_N 2205u                 /* COLOUR 0: the bands rest after 50 ms below THRESHOLD (44.1 kHz samples): ~6 of
                                          * the envelopes' release (~8 ms), each band then ~50 dB under where it began */
/* SILENCE > AGC: the carrier AGC's ceiling (mean |x|, the track's scale; vocoder-ui-spec.md §7a). OFF (the default,
 * 2026-11-01: at FULL the unit's summed carrier, the sequence and chords on track 3, sat at 25000 .. 45000 and the gain
 * at -4 .. -8.6 dB the whole time: chords could not stack or cut through), PEAKS (the hottest stacks only), LIGHT (~2 dB
 * on that playing), FULL (a saw voice at velocity ~85 passes, mean ~16000; one at full ~2.5 dB down, a chord to it) */
#define VOC_AGC_N 4u
static uint8_t voc_agc;
static const int32_t VOC_AGC_TQ[VOC_AGC_N] = {0, 48000, 32000, 18000};
#define VOC_BR_Q15 6300                  /* the bridging at full: the saws out at their own level (LEVEL 0 dB; every band
                                          * open at once: + 14.3 dB, measured, tests/smk_vocoder_test.c) */
#define VOC_PINK_Q15 11105               /* the pink filter's rms over white: 2.95 (Kellet's, at 22050 Hz): back to 1 */
static uint8_t voc_level = 4;            /* LEVEL: -12 .. +18 dB in 3 dB steps (default 0 dB: what the first listening
                                          * set at +16 .. 18 of the old scale) */
static uint8_t voc_dry;                  /* DRY: OFF, -18, -12, -6, 0 dB */
static uint8_t voc_hf = 2;               /* HF (speech addition): OFF, -18, -12, -6, 0 dB (default -12) */
#define VOC_LEVEL_N 11u
#define VOC_MIX_N 5u
static const int32_t VOC_LV_Q8[VOC_LEVEL_N] = {511, 722, 1020, 1441, 2035, 2875, 4061, 5737, 8104, 11447, 16170};
static const int32_t VOC_MIX_Q15[VOC_MIX_N] = {0, 4125, 8231, 16423, 32767};
#define VOC_VS(x) ((int32_t)(((int64_t)(x) * VOICE_FS) >> 14))   /* the voice (Q15) on the track's scale */
#define VOC_XMAX ((1 << 23) - 1)         /* the output's ceiling (-2^22 .. -3 x 2^20 read as the ROM's addresses
                                          * 0xFFC00000.., which the build's image check refuses: 2^23 is clear) */
#define VOC_HF0 17u                      /* the speech addition's first band (4560 Hz) */
/* the vocoder page (ui_vpage.c, vocoder-ui-spec.md §4-6): the VSM201's channel controls and selectors. The channels'
 * values are stored, saved and drawn; their DSP is the next build's. The selectors' defaults keep the sound as before
 * (BYPASS SPEECH with DRY OFF, BRIDGING SUM, ADDITION SUM). Positions 0 .. 100 as a pot's: EMPHASIS 50 = NORMAL's
 * (0 dB), 100 = +12 dB, 0 = OFF; ADDITION 100 = 0 dB, 50 = -12 dB (neutral), 0 = OFF; BRIDGING 0 .. 100 % */
#define VOC_CH 20u
#define VOC_PAIRS 10u                    /* (bridging: one control a pair, 1/2 .. 19/20) */
enum { VF_EMPH, VF_ADD, VF_BRG, VF_N };  /* a channel's function: the knob's and the bar lit */
enum { VB_OFF, VB_REPL, VB_SPEECH, VB_N };   /* BYPASS: off, REPLACEMENT (the carrier dry: the next build), SPEECH */
enum { VS_OFF, VS_SUM, VS_IND, VS_N };   /* BRIDGING, ADDITION: off, the SUM control, INDIVIDUAL (the next build: as SUM) */
typedef struct {
    uint32_t mute;                       /* bit per channel */
    uint8_t emph[VOC_CH], add[VOC_CH], brg[VOC_PAIRS], fn[VOC_CH];   /* (brg[n]: pair n (channels 2n+1/2n+2), on
                                                                     * channel n+1's knob: the VSM201's P1..P10) */
    uint8_t byp, brsel, addsel, actl, have, nzc;   /* BYPASS, BRIDGING, ADDITION, AUTOCONTROL; nzc: NOISE BLEND as the share
                                                   * taken off, 0 .. 100 (0: 100 %, as before; the byte spare before it) */
} vocpg_t;
static vocpg_t vpg = {0, {50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50},
                      {50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50},
                      {0}, {0}, VB_SPEECH, VS_SUM, VS_SUM, 0, 1, 0};
/* TILT and NOISE BLEND as the mod wheel moves them (MOD WHEEL (VOCODER)): the wheel down, the menu's */
static uint32_t voc_tilt_now(void)
{
    uint32_t tl = voc_tilt % VOC_TILT_N;
    return voc_mwa == 1u ? tl + (VOC_TILT_N - 1u - tl) * voc_mw / 127u : tl;
}
static uint32_t voc_nzc_now(void)
{
    uint32_t nz = vpg.nzc > 100u ? 100u : vpg.nzc;
    return voc_mwa == 2u ? nz + (100u - nz) * voc_mw / 127u : nz;
}
static uint8_t voc_emsel;                /* VOCODER EMPHASIS (with NORMAL): the channels' emphasis on NORMAL's weights */
/* the channels' gains from their positions (a pot's: the square): EMPHASIS Q12, 50 = 1.0 (4096), 100 = 4.0 (+12 dB),
 * 0 = OFF; ADDITION Q15, 100 = 1.0, 50 = 0.25 (-12 dB) */
static inline int32_t voc_emph_q12(uint32_t p) { return (int32_t)(p * p * 4096u / 2500u); }
static inline int32_t voc_add_q15(uint32_t p) { return (int32_t)(p * p * 32767u / 10000u); }
/* the synthesis bank's first sections' gains this block (voc_chan_block): NORMAL's weights (or none), times each
 * channel's EMPHASIS (VOCODER EMPHASIS) and 0 when muted; the constant tables themselves when nothing is applied (the
 * output as before, bit for bit) */
static int32_t voc_ga[VOC_CH];
static const int32_t *voc_bap = VOC_B0;
/* AUTOCONTROL (the VSM201's input AGC): the analysis's input gain, Q12 (1.0 .. -12 / +18 dB), towards the voice's level
 * at VOC_ACT in speech, held in a pause (no lift of the room's noise); the analysis only (DRY is the voice as it came) */
#define VOC_ACT 60000                    /* (the voice's mean |x| aimed at, the analysis's scale: full 2^19; ~ -19 dB) */
#define VOC_ACG_MAX 11565                /* (AUTOCONTROL's most: +9 dB, Q12) */
static int32_t voc_acg = 4096;

#define VOC_HN 32u                       /* the half-band's history at 44.1 kHz (27 taps), a power of two */
#define VOC_ON 16u                       /* the decimated outputs' history (the interpolator's: 2 HB), a power of two */
_Static_assert(VOC_HN >= 4u * VOC_HB && VOC_ON >= 2u * VOC_HB && !(VOC_HN & (VOC_HN - 1u)) && !(VOC_ON & (VOC_ON - 1u)),
               "the half-band's rings: powers of two, each its span");
/* a band's two sections' state: x its input, y the first's output (the second's input: its history is y's, kept once), z
 * the second's; each the last two */
typedef struct { int32_t x1, x2, y1, y2, z1, z2; } voc_bq;
static struct {                          /* the analysis (core 0, fx.c mix_block): the voice */
    voc_bq a[VOC_NB];                    /* the bands */
    int32_t env[VOC_NB];                 /* the band envelopes, Q19 */
    int32_t vh[VOC_HN];                  /* the last samples at 44.1 kHz (the decimator) */
    uint32_t hw;
    int32_t peak_in;
    int32_t lp1, lp2, env0, envs;        /* channel 0 (two one-poles, its envelope), the voice's level (unfiltered) */
    uint8_t voiced;                      /* the detector's state (hysteresis) */
    int32_t acg_now;                     /* AUTOCONTROL's gain as the last block ended (Q12): each block ramps from it */
    uint8_t rest, rest_zero;             /* the bands resting (SILENCE > ANALYSIS); voc_ab buffers still to clear */
    uint32_t quiet;                      /* samples below THRESHOLD in a row */
    uint32_t blocks, uv_blocks, pause_blocks, rest_blocks;   /* (the console) */
} voa;
/* what the analysis hands the synthesis: each 22.05 kHz sample's envelopes (Q15) and speech addition; two, a block
 * apart (voc_aw: the one core 0 writes this block, set before core 1's job starts; core 1 reads the other) */
static struct {
    int32_t env[VOC_MAXN / 2u][VOC_NB];
    int32_t hf[VOC_MAXN / 2u];
    int32_t pause;                       /* the pause-filling value, Q15 (0 at strong speech, full in a pause) */
    uint8_t unvoiced, speech;            /* the detector; the speech / pause detector */
} voc_ab[2];
static uint8_t voc_aw;
static struct {                          /* the synthesis (track 3's core): the carrier */
    voc_bq b[VOC_NB];                    /* the bands */
    int32_t ch[VOC_HN];                  /* the carrier's last samples at 44.1 kHz (the decimator) */
    int32_t oh[VOC_ON];                  /* the vocoded samples at 22.05 kHz (the interpolator) */
    uint32_t hw, ow;                     /* samples written into ch, into oh */
    int32_t hh[VOC_ON];                  /* (HQ) the speech addition at 22.05 kHz, up 2:1 to the bank's 44.1 */
    uint32_t hhw;
    uint32_t blocks;                     /* (the console) */
    int32_t peak_out;
    int32_t cenv, uvg;                   /* the carrier's rectified level (the noise's), the noise's share (Q15) */
    int32_t agc_env, agc_g;              /* the carrier AGC: its level (mean |x|), its gain (Q15, 1.0 = 32767) */
    int32_t bk[VOC_NB], cf;              /* BRIDGE TONE as the last block ended: each band's bridge, COLOUR's factor (the
                                          * next block ramps from them: no zipper) */
    int32_t pk0, pk1, pk2;               /* the pink filter */
    int32_t sf;                          /* the SOFT noise's low-pass */
    uint32_t rnd;
} voc;
/* the audio track's state: 0 the engine (AUDIO IN off), 1 + MODE; the one playing (cur) and its fade (Q15) */
static uint8_t voc_cur;
static int32_t voc_xg = 32767;

/* one band-pass section of band k at 22.05 kHz, rounded; b0 its input gain (the host test: the analysis bank's response) */
static inline int32_t voc_sec(int32_t x, int32_t b0, int32_t *x1, int32_t *x2, int32_t *y1, int32_t *y2, uint32_t k)
{
    int64_t a = (int64_t)b0 * (x - *x2) + (int64_t)VOC_C1[k] * *y1 - (int64_t)VOC_A2[k] * *y2 + (1 << 29);
    int32_t y = (int32_t)(a >> 30);
    *x2 = *x1, *x1 = x, *y2 = *y1, *y1 = y;
    return y;
}
static inline int32_t voc_env(int32_t e, int32_t y)      /* full-wave rectified (y Q19), one-pole: attack / release */
{
    int32_t t = y < 0 ? -y : y, d;
    if (t > (1 << 23))
        t = 1 << 23;
    d = t - e;
    return e + (int32_t)(((int64_t)d * (d > 0 ? VOC_ENV_ATT : VOC_ENV_REL)) >> 16);
}
/* a band's two sections (each as voc_sec; the second's input history the first's output history) */
static inline int32_t voc_band(int32_t x, int32_t b0a, int32_t b0b, int32_t c1, int32_t a2, voc_bq *s)
{
    int32_t y = (int32_t)(((int64_t)b0a * (x - s->x2) + (int64_t)c1 * s->y1 - (int64_t)a2 * s->y2 + (1 << 29)) >> 30);
    int32_t z = (int32_t)(((int64_t)b0b * (y - s->y2) + (int64_t)c1 * s->z1 - (int64_t)a2 * s->z2 + (1 << 29)) >> 30);
    s->x2 = s->x1, s->x1 = x, s->y2 = s->y1, s->y1 = y, s->z2 = s->z1, s->z1 = z;
    return z;
}
/* the half-band, 2:1 down: the sample 2 HB behind the next, filtered (h: a history, w the samples written; Q15 taps) */
static inline int32_t voc_dec(const int32_t *h, uint32_t w)
{
    uint32_t c = w - 2u * VOC_HB, j;
    int64_t a = (int64_t)h[c & (VOC_HN - 1u)] * 16384;
    for (j = 0; j < VOC_HB; j++)
        a += (int64_t)VOC_HBT[j] * (h[(c - 2u * j - 1u) & (VOC_HN - 1u)] + h[(c + 2u * j + 1u) & (VOC_HN - 1u)]);
    return (int32_t)((a + (1 << 14)) >> 15);
}

static void voc_reset(void)              /* the synthesis afresh (the state changed): no old ringing */
{
    memset(&voc, 0, sizeof voc);
    voc.rnd = 0x2545F491u;
    voc.agc_g = 32767;
    voc.cf = 32767;
}
static int32_t voc_log2_q8(uint32_t x)   /* log2 x, Q8 (x > 0); once a block: a loop, no clz */
{
    int32_t e = 0;
    while (x >= 512u)
        x >>= 1, e++;
    while (x < 256u)
        x <<= 1, e--;
    return (e + 8) * 256 + (int32_t)(x - 256u);
}
static void voc_ana_reset(void)          /* the analysis afresh (fx.c: as it starts) */
{
    memset(&voa, 0, sizeof voa);
    memset(voc_ab, 0, sizeof voc_ab);
    voa.acg_now = 4096;
}

/* core 0, before core 1's job (fx.c mix_block): the analysis writes the other buffer this block */
static void voc_ana_flip(void) { voc_aw ^= 1u; }
/* core 0, while core 1 renders: this block's voice (voc_in, n even) through the analysis bank into voc_ab[voc_aw] */
static void voc_analyse(uint32_t n)
{
    uint32_t i, k, j = 0, mu = vpg.mute & ((1u << VOC_NB) - 1u);
    uint32_t afast = vpg.addsel != VS_IND && !(mu >> VOC_HF0), act = vpg.actl & 1u;   /* (SUM, none of its bands
                                                                 * muted: the speech addition as before) */
    int32_t pin = voa.peak_in, ag[VOC_NB], acg = act ? voc_acg : 4096;
    int32_t ga = voa.acg_now, dga = (acg - ga) / (int32_t)(n >= 2u ? n / 2u : 1u);   /* (AUTOCONTROL ramped over the
                                                         * block from the last one's: 2026-11-11, a step a block popped) */
    uint32_t aon = act || ga != 4096;
    for (k = 0; k < VOC_NB; k++)                         /* the speech addition's gain a band: INDIVIDUAL each channel's, SUM
                                                          * the top bands; a muted channel none */
        ag[k] = (mu >> k) & 1u ? 0 : vpg.addsel == VS_IND ? voc_add_q15(vpg.add[k]) : k >= VOC_HF0 ? 32767 : 0;
    for (i = 0; i + 1u < n && i + 1u < VOC_MAXN; i += 2u, j++) {
        int32_t v, hf = 0;
        for (k = 0; k < 2u; k++) {
            int32_t x = voc_in[i + k], ax = x < 0 ? -x : x;
            if (ax > pin)
                pin = ax;
            voa.vh[voa.hw++ & (VOC_HN - 1u)] = x;
        }
        v = voc_dec(voa.vh, voa.hw) << 4;
        if (aon) {                                       /* AUTOCONTROL */
            int64_t g;
            ga += dga;
            g = ((int64_t)v * ga) >> 12;
            v = g > (1 << 23) ? 1 << 23 : g < -(1 << 23) ? -(1 << 23) : (int32_t)g;
        }
        voa.lp1 += (int32_t)(((int64_t)(v - voa.lp1) * VOC_LF_K) >> 16);   /* channel 0: below 1 kHz */
        voa.lp2 += (int32_t)(((int64_t)(voa.lp1 - voa.lp2) * VOC_LF_K) >> 16);
        voa.env0 = voc_env(voa.env0, voa.lp2);
        voa.envs = voc_env(voa.envs, v);                 /* the voice's level, unfiltered */
        if (voa.rest)
            continue;                                    /* (a pause: the bands at rest, voc_ab cleared below) */
        for (k = 0; k < VOC_NB; k++) {
            int32_t ya = voc_band(v, VOC_B0[k], VOC_B0[k], VOC_C1[k], VOC_A2[k], &voa.a[k]);
            voa.env[k] = voc_env(voa.env[k], ya);        /* (ya: Q19) */
            voc_ab[voc_aw].env[j][k] = voa.env[k] >> 4;  /* (Q15) */
            if (afast) {
                if (k >= VOC_HF0)
                    hf += ya;                            /* the speech addition: the voice's top bands */
            } else if (ag[k]) {
                hf += (int32_t)(((int64_t)ya * ag[k]) >> 15);   /* (INDIVIDUAL, or a top band muted) */
            }
        }
        voc_ab[voc_aw].hf[j] = hf;
    }
    voa.peak_in = pin;
    voa.acg_now = acg;
    if (act && voa.envs > 0 && n) {                      /* AUTOCONTROL (2026-11-03: the pops / booms gone): down at once
                                                          * above VOC_ACT, up slowly (~1.5 s) and only in clear speech
                                                          * (within 12 dB of it), held in a pause; -12 .. +9 dB */
        int64_t t = ((int64_t)VOC_ACT * acg) / voa.envs;
        int32_t gt = t < 1024 ? 1024 : t > 32767 ? 32767 : (int32_t)t;   /* (2026-11-04: +18 dB again, the owner:
                                                                          * +9 and the slow rise sounded thin) */
        if (gt < acg)
            voc_acg = acg + (gt - acg) / 4;              /* down quickly, ~3 ms (2026-11-11: at once, a step, popped) */
        else if (voc_ab[voc_aw].speech)
            voc_acg = acg + (gt - acg) / 16;             /* up as before (~90 ms), in speech */
    } else if (!act) {
        voc_acg = 4096;                                  /* (off: from 1.0 when it comes on) */
    }
    {   /* the detector: channel 0 against channel 19 (twice to turn voiced, 1.4 x to stay) */
        int32_t e19 = voa.env[18];
        voa.voiced = (uint8_t)(voa.env0 > (voa.voiced ? e19 + e19 * 2 / 5 : 2 * e19) && voa.env0 > 0);
        voc_ab[voc_aw].unvoiced = (uint8_t)!voa.voiced;
    }
    {   /* the pause value: the level in dB (Q8; full scale 2^19), full below THRESHOLD, 0 from 24 dB above it */
        int32_t thr = VOC_THR_DB[voc_thr % VOC_THR_N] * 256, p;
        int32_t db = voa.envs > 0 ? ((voc_log2_q8((uint32_t)voa.envs) - 19 * 256) * 1541) >> 8 : -120 * 256;
        p = db <= thr ? 32767 : db >= thr + 24 * 256 ? 0 : (int32_t)(((int64_t)(thr + 24 * 256 - db) * 32767) / (24 * 256));
        voc_ab[voc_aw].pause = p;
        voc_ab[voc_aw].speech = (uint8_t)(db > thr);
    }
    if (voa.rest && voa.rest_zero) {                     /* resting: both hand-over buffers 0, one a block */
        memset(voc_ab[voc_aw].env, 0, sizeof voc_ab[voc_aw].env);
        memset(voc_ab[voc_aw].hf, 0, sizeof voc_ab[voc_aw].hf);
        voa.rest_zero--;
    }
    voa.rest_blocks += voa.rest;
    voa.quiet = voc_ab[voc_aw].speech || voc_colour ? 0u : voa.quiet + n > VOC_REST_N ? VOC_REST_N : voa.quiet + n;
    if (voa.rest && !voa.quiet) {
        voa.rest = 0;                                    /* speech (or COLOUR up): the bands run from the next block, from rest */
    } else if (!voa.rest && voa.quiet >= VOC_REST_N) {   /* a pause long enough: the bands rest, their state cleared */
        memset(voa.a, 0, sizeof voa.a);
        memset(voa.env, 0, sizeof voa.env);
        voa.lp1 = voa.lp2 = voa.env0 = 0;
        voa.rest = 1;
        voa.rest_zero = 2;
    }
    voa.blocks++;
    voa.uv_blocks += !voa.voiced;
    voa.pause_blocks += voc_ab[voc_aw].speech == 0u;
}

/* the synthesis bank's bands k .. k1 - 1 on the carrier sample c (the carrier's scale << 4), each times its envelope:
 * the analysis's, times cf (COLOUR in a pause, Q15), with the band's bridge bk[k] (TILT) added (SUM) or the larger of the
 * two (FILL); the first section carries the VSM201's NORMAL weights. mode (chosen a block, voc_process): VT_PLAIN no
 * bridge and COLOUR full (speech: the loop as it was before BRIDGE TONE), VT_SUM the bridge added, VT_ALL COLOUR and FILL
 * too: the bridge costs only where it is heard */
enum { VT_PLAIN, VT_SUM, VT_ALL };
static int64_t voc_bands(int32_t c, uint32_t k, uint32_t k1, const int32_t *env, const int32_t *bk, int32_t cf,
                         uint32_t mode, uint32_t fill)
{
    int64_t sum = 0;
    voc_bq *s = &voc.b[k];
    const int32_t *ba = voc_bap;                         /* (the first section's gain: voc_chan_block) */
    if (mode == VT_PLAIN) {
        for (; k < k1; k++, s++)
            sum += (int64_t)voc_band(c, ba[k], VOC_B0[k], VOC_C1[k], VOC_A2[k], s) * env[k];   /* Q15 */
    } else if (mode == VT_SUM) {
        for (; k < k1; k++, s++)
            sum += (int64_t)voc_band(c, ba[k], VOC_B0[k], VOC_C1[k], VOC_A2[k], s) * (env[k] + bk[k]);
    } else {
        for (; k < k1; k++, s++) {
            int32_t e = (env[k] * cf) >> 15;
            sum += (int64_t)voc_band(c, ba[k], VOC_B0[k], VOC_C1[k], VOC_A2[k], s) *
                   (fill ? (e > bk[k] ? e : bk[k]) : e + bk[k]);
        }
    }
    return sum;
}
/* one vocoded sample at 22.05 kHz from the carrier c (the track's scale), j the sample's place in the block (the
 * analysis's envelopes of the block before) */
static int32_t voc_tick(int32_t c, uint32_t j, const int32_t *bk, int32_t cf, uint32_t mode, uint32_t fill)
{
    const int32_t *env = voc_ab[voc_aw ^ 1u].env[j];
    int32_t o = (int32_t)(((voc_bands(c << 4, 0, VOC_NB, env, bk, cf, mode, fill) >> 19) *
                           VOC_LV_Q8[voc_level % VOC_LEVEL_N]) >> 8);
    if (voc_hf && vpg.addsel != VS_OFF)
        o += (int32_t)(((int64_t)VOC_VS(voc_ab[voc_aw ^ 1u].hf[j] >> 4) * VOC_MIX_Q15[voc_hf % VOC_MIX_N]) >> 15);
    return o;
}

/* the first sections' gains this block: NORMAL's weights (or none), each channel's EMPHASIS (VOCODER EMPHASIS), 0 muted.
 * Nothing applied: the constant table itself */
_Static_assert(VOC_CH == VOC_NB, "the page's channels are the bands");
static void voc_chan_block(uint32_t hq)
{
    const int32_t *ba = hq ? (voc_norm ? VOC_B0SH : VOC_B0H) : voc_norm ? VOC_B0S : VOC_B0;
    uint32_t k, mu = vpg.mute & ((1u << VOC_NB) - 1u), em = voc_norm && voc_emsel;
    if (!mu && !em) {
        voc_bap = ba;
        return;
    }
    for (k = 0; k < VOC_NB; k++) {
        int32_t g = (mu >> k) & 1u ? 0 : em ? voc_emph_q12(vpg.emph[k]) : 4096;
        voc_ga[k] = g == 4096 ? ba[k] : (int32_t)(((int64_t)ba[k] * g) >> 12);
    }
    voc_bap = voc_ga;
}


/* the carrier block b (the track's scale, n even) becomes the vocoded block, in place */
static void voc_process(int32_t *b, uint32_t n)
{
    uint32_t i, j;
    int32_t pout = voc.peak_out, dg = VOC_MIX_Q15[vpg.byp == VB_SPEECH ? voc_dry % VOC_MIX_N : 0u];
    int32_t dr = VOC_MIX_Q15[vpg.byp == VB_REPL ? voc_dry % VOC_MIX_N : 0u];   /* BYPASS REPLACEMENT: the carrier dry */
    uint32_t bmu = vpg.mute & ((1u << VOC_NB) - 1u), bind = vpg.brsel == VS_IND;
    uint32_t vm = voc_vmode % VV_N, br = vpg.brsel != VS_OFF && voc_bridge % VOC_BR_N != 0u;
    int32_t badd = br ? (int32_t)(((int64_t)voc_ab[voc_aw ^ 1u].pause * voc_br_q15(voc_bridge % VOC_BR_N) >> 15) *
                                  VOC_BR_Q15 >> 15) : 0;
    int32_t bk[VOC_NB], dbk[VOC_NB], cf, dcf, ag, dag, m = (int32_t)(n / 2u);
    uint32_t fill = voc_blend & 1u, mode, ramp = 0, any = 0;
    int32_t uvt = vm == VV_UNVOICED ? 32767 : vm == VV_VOICED ? 0 :                 /* the noise's share wanted */
                  voc_ab[voc_aw ^ 1u].unvoiced && (!br || voc_ab[voc_aw ^ 1u].speech)
                      ? 32767 - (((int32_t)voc_nzc_now() * 20971) >> 6) : 0;   /* (AUTO: NOISE BLEND, x 327.67) */
    voc.blocks++;
    voc_chan_block(0);
    {   /* BRIDGE TONE: the bridge per band (TILT), the envelopes' share in a pause (COLOUR: full in speech), each
         * ramped over the block from where the last one ended (the pause value moves block by block: a step there is a
         * click, loudest where the carrier is strongest, the low bands a negative TILT lifts) */
        const uint16_t *tq = VOC_TILT_Q12[voc_tilt_now()];
        int32_t col = voc_colour >= VOC_COLOUR_N - 1u ? 32767 : (int32_t)voc_colour * 3277, to;
        if (m < 1)
            m = 1;
        for (j = 0; j < VOC_NB; j++) {
            to = (badd * (int32_t)tq[j]) >> 12;
            if ((bmu >> j) & 1u)
                to = 0;                                  /* (a muted channel: no bridge) */
            else if (bind)                               /* INDIVIDUAL: the pair's control (SUM above it) */
                to = (int32_t)(((int64_t)to * vpg.brg[(j / 2u) % VOC_PAIRS] * 32767 / 100) >> 15);
            bk[j] = voc.bk[j];
            dbk[j] = (to - bk[j]) / m;
            ramp |= (uint32_t)(dbk[j] != 0);
            any |= (uint32_t)(to | bk[j]);
            voc.bk[j] = to;
        }
        to = 32767 - (int32_t)(((int64_t)voc_ab[voc_aw ^ 1u].pause * (32767 - col)) >> 15);
        cf = voc.cf;
        dcf = (to - cf) / m;
        ramp |= (uint32_t)(dcf != 0);
        mode = cf < 32767 || to < 32767 || (fill && any) ? VT_ALL : any ? VT_SUM : VT_PLAIN;
        voc.cf = to;
    }
    {   /* the carrier AGC (the VSM201's, no control): the gain that keeps the summed carrier's level (agc_env, its mean
         * |x|) at most the ceiling (AGC), 1.0 under it; the gain follows it down in ~5 ms, back up in ~150 ms, ramped over the
         * block (no step) */
        int32_t ct = VOC_AGC_TQ[voc_agc % VOC_AGC_N];   /* (0: OFF, the gain back to 1.0 as from below the ceiling) */
        int32_t g = ct && voc.agc_env > ct ? (int32_t)(((int64_t)ct * 32767) / voc.agc_env) : 32767;
        ag = voc.agc_g;
        g = ag + (int32_t)(((int64_t)(g - ag) * (g < ag ? VOC_AGC_DN : VOC_AGC_UP) + (g > ag ? 65535 : 0)) >> 16);   /* (up:
                                                                 * rounded up, so it gets back to 1.0) */
        if (g > 32767)
            g = 32767;
        dag = n >= 2u ? (g - ag) / (int32_t)(n / 2u) : 0;
        voc.agc_g = g;
    }
    for (i = 0; i + 1u < n && i + 1u < VOC_MAXN; i += 2u) {
        int32_t o[2], d, k;
        int64_t a = 0;
        for (k = 0; k < 2; k++)
            voc.ch[voc.hw++ & (VOC_HN - 1u)] = b[i + (uint32_t)k];
        {   /* the replacement: the saws, the noise level-matched to them, switched (or, bridging, added) */
            int32_t c = voc_dec(voc.ch, voc.hw), ac = c < 0 ? -c : c, w, nz;
            voc.agc_env += (int32_t)(((int64_t)(ac - voc.agc_env) * VOC_AGC_LVL) >> 16);
            c = (int32_t)(((int64_t)c * ag) >> 15);              /* (the AGC, before the noise matched to it) */
            ag += dag;
            ac = c < 0 ? -c : c;
            voc.cenv += (int32_t)(((int64_t)(ac - voc.cenv) * VOC_CENV_K) >> 16);
            voc.uvg = voc.uvg < uvt ? (voc.uvg + 512 > uvt ? uvt : voc.uvg + 512)
                                    : (voc.uvg - 512 < uvt ? uvt : voc.uvg - 512);   /* ~3 ms */
            if (voc.uvg) {
                voc.rnd = voc.rnd * 1664525u + 1013904223u;
                w = (int32_t)voc.rnd >> 16;                      /* white, Q15 */
                if (voc_pink == 2u) {                            /* SOFT: -6 dB an octave over ~200 Hz, white's rms */
                    voc.sf = (int32_t)(((int64_t)voc.sf * VOC_SOFT_A) >> 15) + (int32_t)(((int64_t)w * (32768 - VOC_SOFT_A)) >> 15);
                    w = (int32_t)(((int64_t)voc.sf * VOC_SOFT_G) >> 15);
                } else if (voc_pink) {                           /* Kellet's economy pink, Q15 */
                    voc.pk0 = (int32_t)(((int64_t)voc.pk0 * 32690) >> 15) + ((w * 3245) >> 15);   /* (pk0 to ~1.4M) */
                    voc.pk1 = (int32_t)(((int64_t)voc.pk1 * 31556) >> 15) + ((w * 9716) >> 15);
                    voc.pk2 = (int32_t)(((int64_t)voc.pk2 * 18678) >> 15) + ((w * 34494) >> 15);
                    w = (int32_t)(((int64_t)(voc.pk0 + voc.pk1 + voc.pk2 + ((w * 6056) >> 15)) * VOC_PINK_Q15) >> 15);
                }
                nz = (int32_t)(((int64_t)w * voc.cenv) >> 14);  /* (uniform: rms 0.577; x 2: a saw's 1.155 its mean) */
                c = br && vm == VV_AUTO ? c + (int32_t)(((int64_t)nz * voc.uvg) >> 15)
                                        : c + (int32_t)(((int64_t)(nz - c) * voc.uvg) >> 15);
            }
            if (ramp) {                                  /* (only while the bridge or COLOUR moves) */
                for (j = 0; j < VOC_NB; j++)
                    bk[j] += dbk[j];
                cf += dcf;
            }
            d = voc_tick(c, i >> 1, bk, cf, mode, fill);
        }
        voc.oh[voc.ow++ & (VOC_ON - 1u)] = d;
        /* 2:1 up: the even sample the one VOC_HB back, the odd one the half-band between it and the next */
        o[0] = voc.oh[(voc.ow - 1u - VOC_HB) & (VOC_ON - 1u)];
        for (j = 0; j < VOC_HB; j++)
            a += (int64_t)VOC_HBT[j] * (voc.oh[(voc.ow - 1u - VOC_HB - j) & (VOC_ON - 1u)] +
                                        voc.oh[(voc.ow - VOC_HB + j) & (VOC_ON - 1u)]);
        o[1] = (int32_t)((a + (1 << 13)) >> 14);         /* (x 2: the zeros between) */
        for (k = 0; k < 2; k++) {
            int32_t x = o[k];
            if (dg)
                x += (int32_t)(((int64_t)VOC_VS(voc_in[i + (uint32_t)k]) * dg) >> 15);   /* DRY */
            if (dr)
                x += (int32_t)(((int64_t)b[i + (uint32_t)k] * dr) >> 15);   /* REPLACEMENT (b: still the carrier) */
            x = x > VOC_XMAX ? VOC_XMAX : x < -VOC_XMAX ? -VOC_XMAX : x;
            if ((x < 0 ? -x : x) > pout)
                pout = x < 0 ? -x : x;
            b[i + (uint32_t)k] = x;
        }
    }
    voc.peak_out = pout;
}

/* HQ (SETUP VOC + DRUMS, VOC + SYNTH, VOC ALONE: vocoder-ui-spec.md §7i-2): voc_process with the synthesis bank at
 * 44.1 kHz, no 2:1 down and up: the carrier straight in, every sample through the bank (VOC_B0H ..), its envelopes the
 * analysis's at 22.05 kHz, each held two samples (they move at ~50 Hz), the speech addition up 2:1 through the
 * half-band. The noise, the AGC's level, BRIDGE TONE's ramps a 44.1 kHz sample. The analysis is as it was */
static int32_t voc_bands_hq(int32_t c, const int32_t *env, const int32_t *bk, int32_t cf, uint32_t mode, uint32_t fill)
{
    int64_t sum = 0;
    voc_bq *s = &voc.b[0];
    const int32_t *ba = voc_bap;
    uint32_t k;
    if (mode == VT_PLAIN) {
        for (k = 0; k < VOC_NB; k++, s++)
            sum += (int64_t)voc_band(c, ba[k], VOC_B0H[k], VOC_C1H[k], VOC_A2H[k], s) * env[k];
    } else if (mode == VT_SUM) {
        for (k = 0; k < VOC_NB; k++, s++)
            sum += (int64_t)voc_band(c, ba[k], VOC_B0H[k], VOC_C1H[k], VOC_A2H[k], s) * (env[k] + bk[k]);
    } else {
        for (k = 0; k < VOC_NB; k++, s++) {
            int32_t e = (env[k] * cf) >> 15;
            sum += (int64_t)voc_band(c, ba[k], VOC_B0H[k], VOC_C1H[k], VOC_A2H[k], s) *
                   (fill ? (e > bk[k] ? e : bk[k]) : e + bk[k]);
        }
    }
    return sum;
}
static void __attribute__((noinline)) voc_process_hq(int32_t *b, uint32_t n)   /* (out of the hot code: FULL's cache
                                                         * as before, 2026-11-11: inlined, FULL shed 0.7 / s, not 0.25) */
{
    uint32_t i, j;
    int32_t pout = voc.peak_out, dg = VOC_MIX_Q15[vpg.byp == VB_SPEECH ? voc_dry % VOC_MIX_N : 0u];
    int32_t dr = VOC_MIX_Q15[vpg.byp == VB_REPL ? voc_dry % VOC_MIX_N : 0u];   /* BYPASS REPLACEMENT: the carrier dry */
    uint32_t bmu = vpg.mute & ((1u << VOC_NB) - 1u), bind = vpg.brsel == VS_IND;
    uint32_t vm = voc_vmode % VV_N, br = vpg.brsel != VS_OFF && voc_bridge % VOC_BR_N != 0u;
    int32_t badd = br ? (int32_t)(((int64_t)voc_ab[voc_aw ^ 1u].pause * voc_br_q15(voc_bridge % VOC_BR_N) >> 15) *
                                  VOC_BR_Q15 >> 15) : 0;
    int32_t bk[VOC_NB], dbk[VOC_NB], cf, dcf, ag, dag, m = (int32_t)n;
    uint32_t fill = voc_blend & 1u, mode, ramp = 0, any = 0;
    int32_t uvt = vm == VV_UNVOICED ? 32767 : vm == VV_VOICED ? 0 :                 /* the noise's share wanted */
                  voc_ab[voc_aw ^ 1u].unvoiced && (!br || voc_ab[voc_aw ^ 1u].speech)
                      ? 32767 - (((int32_t)voc_nzc_now() * 20971) >> 6) : 0;   /* (AUTO: NOISE BLEND, x 327.67) */
    voc.blocks++;
    voc_chan_block(1);
    {   /* BRIDGE TONE: the bridge per band (TILT), the envelopes' share in a pause (COLOUR: full in speech), each
         * ramped over the block from where the last one ended (the pause value moves block by block: a step there is a
         * click, loudest where the carrier is strongest, the low bands a negative TILT lifts) */
        const uint16_t *tq = VOC_TILT_Q12[voc_tilt_now()];
        int32_t col = voc_colour >= VOC_COLOUR_N - 1u ? 32767 : (int32_t)voc_colour * 3277, to;
        if (m < 1)
            m = 1;
        for (j = 0; j < VOC_NB; j++) {
            to = (badd * (int32_t)tq[j]) >> 12;
            if ((bmu >> j) & 1u)
                to = 0;                                  /* (a muted channel: no bridge) */
            else if (bind)                               /* INDIVIDUAL: the pair's control (SUM above it) */
                to = (int32_t)(((int64_t)to * vpg.brg[(j / 2u) % VOC_PAIRS] * 32767 / 100) >> 15);
            bk[j] = voc.bk[j];
            dbk[j] = (to - bk[j]) / m;
            ramp |= (uint32_t)(dbk[j] != 0);
            any |= (uint32_t)(to | bk[j]);
            voc.bk[j] = to;
        }
        to = 32767 - (int32_t)(((int64_t)voc_ab[voc_aw ^ 1u].pause * (32767 - col)) >> 15);
        cf = voc.cf;
        dcf = (to - cf) / m;
        ramp |= (uint32_t)(dcf != 0);
        mode = cf < 32767 || to < 32767 || (fill && any) ? VT_ALL : any ? VT_SUM : VT_PLAIN;
        voc.cf = to;
    }
    {   /* the carrier AGC (the VSM201's, no control): the gain that keeps the summed carrier's level (agc_env, its mean
         * |x|) at most the ceiling (AGC), 1.0 under it; the gain follows it down in ~5 ms, back up in ~150 ms, ramped over the
         * block (no step) */
        int32_t ct = VOC_AGC_TQ[voc_agc % VOC_AGC_N];   /* (0: OFF, the gain back to 1.0 as from below the ceiling) */
        int32_t g = ct && voc.agc_env > ct ? (int32_t)(((int64_t)ct * 32767) / voc.agc_env) : 32767;
        ag = voc.agc_g;
        g = ag + (int32_t)(((int64_t)(g - ag) * (g < ag ? VOC_AGC_DN : VOC_AGC_UP) + (g > ag ? 65535 : 0)) >> 16);   /* (up:
                                                                 * rounded up, so it gets back to 1.0) */
        if (g > 32767)
            g = 32767;
        dag = n ? (g - ag) / (int32_t)n : 0;
        voc.agc_g = g;
    }
    for (i = 0; i < n && i < VOC_MAXN; i++) {
        int32_t c = b[i], ac = c < 0 ? -c : c, w, nz, x, d;
        const int32_t *env = voc_ab[voc_aw ^ 1u].env[i >> 1];
        if (!(i & 1u))                                   /* the speech addition: a 22.05 kHz sample in, every other */
            voc.hh[voc.hhw++ & (VOC_ON - 1u)] = voc_ab[voc_aw ^ 1u].hf[i >> 1];
        voc.agc_env += (int32_t)(((int64_t)(ac - voc.agc_env) * VOC_AGC_LVLH) >> 16);
        c = (int32_t)(((int64_t)c * ag) >> 15);
        ag += dag;
        ac = c < 0 ? -c : c;
        voc.cenv += (int32_t)(((int64_t)(ac - voc.cenv) * VOC_CENV_KH) >> 16);
        voc.uvg = voc.uvg < uvt ? (voc.uvg + 256 > uvt ? uvt : voc.uvg + 256)
                                : (voc.uvg - 256 < uvt ? uvt : voc.uvg - 256);   /* ~3 ms */
        if (voc.uvg) {
            voc.rnd = voc.rnd * 1664525u + 1013904223u;
            w = (int32_t)voc.rnd >> 16;
            if (voc_pink == 2u) {
                voc.sf = (int32_t)(((int64_t)voc.sf * VOC_SOFT_AH) >> 15) + (int32_t)(((int64_t)w * (32768 - VOC_SOFT_AH)) >> 15);
                w = (int32_t)(((int64_t)voc.sf * VOC_SOFT_G) >> 15);
            } else if (voc_pink) {
                voc.pk0 = (int32_t)(((int64_t)voc.pk0 * 32690) >> 15) + ((w * 3245) >> 15);
                voc.pk1 = (int32_t)(((int64_t)voc.pk1 * 31556) >> 15) + ((w * 9716) >> 15);
                voc.pk2 = (int32_t)(((int64_t)voc.pk2 * 18678) >> 15) + ((w * 34494) >> 15);
                w = (int32_t)(((int64_t)(voc.pk0 + voc.pk1 + voc.pk2 + ((w * 6056) >> 15)) * VOC_PINK_Q15H) >> 15);
            }
            nz = (int32_t)(((int64_t)w * voc.cenv) >> 14);
            c = br && vm == VV_AUTO ? c + (int32_t)(((int64_t)nz * voc.uvg) >> 15)
                                    : c + (int32_t)(((int64_t)(nz - c) * voc.uvg) >> 15);
        }
        if (ramp) {
            for (j = 0; j < VOC_NB; j++)
                bk[j] += dbk[j];
            cf += dcf;
        }
        d = (int32_t)(((voc_bands_hq(c << 4, env, bk, cf, mode, fill) >> 19) * VOC_LV_Q8[voc_level % VOC_LEVEL_N]) >> 8);
        if (voc_hf && vpg.addsel != VS_OFF) {           /* the speech addition, up 2:1 (the half-band, as the output's) */
            int64_t a = 0;
            int32_t hf;
            if (i & 1u) {
                for (j = 0; j < VOC_HB; j++)
                    a += (int64_t)VOC_HBT[j] * (voc.hh[(voc.hhw - 1u - VOC_HB - j) & (VOC_ON - 1u)] +
                                                voc.hh[(voc.hhw - VOC_HB + j) & (VOC_ON - 1u)]);
                hf = (int32_t)((a + (1 << 13)) >> 14);
            } else {
                hf = voc.hh[(voc.hhw - 1u - VOC_HB) & (VOC_ON - 1u)];
            }
            d += (int32_t)(((int64_t)VOC_VS(hf >> 4) * VOC_MIX_Q15[voc_hf % VOC_MIX_N]) >> 15);
        }
        x = d;
        if (dg)
            x += (int32_t)(((int64_t)VOC_VS(voc_in[i]) * dg) >> 15);   /* DRY */
        if (dr)
            x += (int32_t)(((int64_t)b[i] * dr) >> 15);   /* REPLACEMENT (b: still the carrier) */
        x = x > VOC_XMAX ? VOC_XMAX : x < -VOC_XMAX ? -VOC_XMAX : x;
        if ((x < 0 ? -x : x) > pout)
            pout = x < 0 ? -x : x;
        b[i] = x;
    }
    voc.peak_out = pout;
}

/* voice.c track_render, track 3, at its start: the state to render this block. A change fades the old state out first
 * (voc_fade), then switches (the vocoder starts afresh) and fades the new one in */
/* HQ: every SETUP but FULL, and only once the units it turns off have stopped (fx.c voc_setup_block: they come back only
 * once HQ is off): the two never run together (2026-11-11: FULL's units and HQ at once, the switch broke up) */
#define VOC_HQ_WANT(req) ((req) == 1u + VM_SAW && VOC_SETUP_ON != VSU_FULL && !(voc_dbg & 1u) && !voc_skip_wait && \
                          voc_skip == VSU_SKIP(VOC_SETUP_ON))
static uint32_t voc_state(void)
{
    uint32_t req = VOC_AUDIO_ON ? 1u + voc_mode % VM_N : 0u, hq = VOC_HQ_WANT(req);
    if ((req != voc_cur || hq != voc_hq) && voc_xg == 0) {   /* (the rate too: faded out, the bank afresh) */
        voc_cur = (uint8_t)req;
        voc_hq = (uint8_t)hq;
        voc_reset();
    }
    return voc_cur;
}
static void voc_dry_block(int32_t *out, uint32_t n)      /* MODE OFF: the voice itself */
{
    uint32_t i;
    for (i = 0; i < n && i < VOC_MAXN; i++)
        out[i] = VOC_VS(voc_in[i]);
}
/* .. at its end: the fade (~5 ms either way: 148 a sample of Q15); out of the way when settled */
static void voc_fade(int32_t *out, uint32_t n)
{
    uint32_t req = VOC_AUDIO_ON ? 1u + voc_mode % VM_N : 0u, i;
    int32_t tgt = req == voc_cur && VOC_HQ_WANT(req) == voc_hq ? 32767 : 0;
    if (voc_xg == tgt && tgt)
        return;
    for (i = 0; i < n; i++) {
        voc_xg = voc_xg < tgt ? (voc_xg + 148 > tgt ? tgt : voc_xg + 148) : (voc_xg - 148 < tgt ? tgt : voc_xg - 148);
        out[i] = (int32_t)(((int64_t)out[i] * voc_xg) >> 15);
    }
}
