/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLOOP menu (HOME held), in sections as the pages are (2.4): SCREEN (COLOR, ZOOM, KNOB ACCEL (2.7); SMK-37: KNOB
 * LAYOUT, then KNOB ACCEL), LIGHTS (LIGHTS, KEYS, NOTES), AUDIO (LOWCUT, USB AUDIO, USB SERIAL (FELUCCA_CDC builds
 * only), USB RATE (2.7 beta), FILTER RESO (2.7.2)), SMK-37: USB (USB LAYOUT: the stems), SEQ (2.7.1: STEP ADVANCE,
 * DRUM PREVIEW), SMK-37: PLAY (KEY
 * VELOCITY, BEND UP, BEND DOWN, LIMITER), MIDI (TRS MIDI OUT, MODE, CHANNEL, LOCAL), DAW (CC MAP, KNOBS, EDIT MAP,
 * RESET MAP), SYSTEM (2ND CORE (dualcore), HARDWARE CALIBRATION, LEARN CLEAR (2.6), ABOUT). The SMK-37 keeps every
 * row the SLOOP manual lists in its section; its own rows are in PLAY, MIDI and DAW.
 * SELECT goes to the section before / after (stopping at the ends), KNOB 1..4 set the section's settings in order (the knob's colour marks
 * its row), PRESETS moves the cursor; OCT+ steps the cursor's setting round or opens it (CALIBRATION, ABOUT),
 * OCT- closes (from ABOUT: back to the section). */
/* ------------------------------------------------------------ menu --- */
/* (2.7: USB SERIAL only in a build with the serial console, FELUCCA_CDC=1: the FM-1's build has none. SMK-37: four rows a
 * section (K5..K8); a FELUCCA_CDC=1 SMK-37 build, a developer's, has USB SERIAL in AUDIO where USB RATE is) */
#define MI_HAS_RATE !(BOARD_SMK37 && FELUCCA_CDC)
enum { MI_COLOR, MI_ZOOM,
#if BOARD_SMK37
       MI_KLAY,                                    /* SMK-37: SCREEN > KNOB LAYOUT (panel.c knob_enc), the third row as before */
#endif
       MI_ACCEL,
       MI_LIGHTS, MI_KEYS, MI_NOTES, MI_LOWCUT, MI_USB,
#if FELUCCA_CDC
       MI_SERIAL,
#endif
#if MI_HAS_RATE
       MI_RATE,
#endif
       MI_FRESO,
#if BOARD_SMK37
       MI_LAYOUT,                                  /* SMK-37: USB > USB LAYOUT (usb.c usb_layout; main.c re-plugs USB) */
       MI_SRATE,                                   /* SMK-37: USB > STEM RATE (usb.c stem_rate; main.c re-plugs USB) */
       MI_SFILT,                                   /* SMK-37: USB > STEM FILTER (usb.c stem_light, live) */
       MI_SPAIR,                                   /* SMK-37: USB > STEREO PAIR (fx.c smk_pair: SYNTHS+RET's 5-6; live;
                                                    * harmoniser-spec.md §10c) */
       MI_SFX,                                     /* SMK-37: PUNCH > STEM FX (smk_tpunch.h stem_fx, live) */
       MI_PFADE,                                   /* SMK-37: PUNCH > FADE (GLO G_FXFADE: as SELECT on the FX page) */
       MI_XFADE,                                   /* SMK-37: PUNCH > XFADE (smk_tpunch.h tp_xfl, live) */
       MI_CURVE,                                   /* SMK-37: PUNCH > CURVE (smk_tpunch.h tp_lin, live) */
#if SMK_UAC_IN_PROBE
       MI_VSETUP, MI_VCARR, MI_VINL, MI_VINR,      /* the vocoder build: SETUP > SETUP, CARRIER, IN L, IN R
                                                    * (smk_vocoder.h; kept, project.c) */
       MI_VTRL, MI_VTRR,                           /* INPUT > TRIM L, TRIM R, each with its meter (smk_vocoder.h voc_trim,
                                                    * uin_pk; kept, project.c) */
#if !SMK_UAC_PROBE
       MI_UIN,                                     /* INPUT > AUDIO IN (usb.c uin_mode, live; USB's until 2026-11-13, its
                                                    * place now STEREO PAIR's) */
#endif
       MI_VMODE, MI_VLVL, MI_VDRY, MI_VHF,         /* the vocoder build: VOCODER > MODE, LEVEL, DRY, HF (smk_vocoder.h:
                                                    * the audio track, track 3; live, not kept) */
       MI_VCUT, MI_VRES, MI_VVM, MI_VPINK,         /* CARRIER > CUTOFF, RESO (the saws' low-pass), VOICE, NOISE (the
                                                    * VSM201's voice mode, its pink filter); live, not kept */
       MI_VBR, MI_VTHR, MI_VAGC, MI_VMWA,          /* SILENCE > BRIDGE, THRESHOLD (the VSM201's silence bridging), AGC (the
                                                    * carrier's; kept, project.c) */
       MI_VTILT, MI_VCOL, MI_VBLEND, MI_VNORM,     /* BRIDGE TONE > TILT, COLOUR, BLEND, NORMAL (the whole output's: the
                                                    * VSM201's weights; smk_vocoder.h; kept, project.c) */
       MI_VEATT, MI_VEREL, MI_VGDEL, MI_VBRM,      /* ENVELOPE > ATTACK, RELEASE (the bands' envelopes), GATE DELAY (the
                                                    * external carrier's gate), BRIDGE MODE (smk_vocoder.h voc_brmode);
                                                    * kept, project.c */
       MI_HON, MI_HINT, MI_HNOTE,                  /* HARMONY > HARMONY (smk_harm.h hm_on: T1 the harmoniser; kept,
                                                    * project.c), INTERVALS (hm_int: FIXED / SCALE; kept),
                                                    * project.c), SUNG NOTE (the tracker's, a readout) */
#endif
#endif
       MI_ADV, MI_DPREV,
#if BOARD_SMK37
       MI_FRET,                                    /* SMK-37: SEQ > FLIP RETURN (smk_seqfx.h sq_carry, live) */
       MI_VEL, MI_BUP, MI_BDN, MI_LIMIT,           /* SMK-37: PLAY > KEY VELOCITY (smk_vel.h), BEND UP, BEND DOWN (smk_wheel.h),
                                                    * LIMITER (fx.c lim_classic) */
       MI_TRS, MI_MODE, MI_DCH, MI_LOCAL,          /* SMK-37: MIDI > TRS MIDI OUT (smk_midi_out.h), MODE, CHANNEL, LOCAL
                                                    * (smk_daw.h) */
       MI_DMAP, MI_KREL, MI_DEDIT, MI_DRESET,      /* SMK-37: DAW > CC MAP, KNOBS, EDIT MAP, RESET MAP (ui_daw.c) */
#endif
       MI_CPU1, MI_PANEL, MI_LCLEAR, MI_ABOUT, MI_COUNT };
#if !MI_HAS_RATE
#define MI_RATE 0xFFu                              /* (no row: never an item) */
#endif
static const char *const MI_NAME[MI_COUNT] = {"COLOR", "ZOOM",
#if BOARD_SMK37
                                              "KNOB LAYOUT",
#endif
                                              "KNOB ACCEL", "LIGHTS", "KEYS", "NOTES", "SPEAKER LOWCUT", "USB AUDIO",
#if FELUCCA_CDC
                                              "USB SERIAL",
#endif
#if MI_HAS_RATE
                                              "USB RATE",
#endif
                                              "FILTER RESO",
#if BOARD_SMK37
                                              "USB LAYOUT", "STEM RATE", "STEM FILTER", "STEREO PAIR",
                                              "STEM FX", "FADE", "XFADE", "CURVE",
#if SMK_UAC_IN_PROBE
                                              "SETUP", "CARRIER", "IN L", "IN R", "TRIM L", "TRIM R",
#if !SMK_UAC_PROBE
                                              "AUDIO IN",
#endif
                                              "MODE", "LEVEL", "DRY", "HF", "CUTOFF", "RESO", "VOICE", "NOISE",
                                              "BRIDGE", "THRESHOLD", "AGC", "MOD WHEEL", "TILT", "COLOUR", "BLEND", "RESPONSE",
                                              "ATTACK", "RELEASE", "GATE DELAY", "BRIDGE MODE", "HARMONY", "INTERVALS", "SUNG NOTE",
#endif
#endif
                                              "STEP ADVANCE", "DRUM PREVIEW",
#if BOARD_SMK37
                                              "FLIP RETURN", "KEY VELOCITY", "BEND UP", "BEND DOWN", "LIMITER", "TRS MIDI OUT",
                                              "MODE", "CHANNEL", "LOCAL", "CC MAP", "KNOBS", "EDIT MAP", "RESET MAP",
#endif
                                              "2ND CORE", "HARDWARE CALIBRATION", "MIDI LEARN CLEAR", "ABOUT"};
#define MI_SYS0 MI_CPU1
#if BOARD_SMK37
#if SMK_UAC_IN_PROBE
enum { MS_SCREEN, MS_LIGHTS, MS_AUDIO, MS_USB, MS_PUNCH, MS_SETUP, MS_INPUT, MS_VOC, MS_CARR, MS_SIL, MS_TONE, MS_ENV,
       MS_HARM, MS_SEQ, MS_PLAY, MS_MIDI, MS_DAW, MS_SYSTEM, MS_COUNT };
static const char *const MS_NAME[MS_COUNT] = {"SCREEN", "LIGHTS", "AUDIO", "USB", "PUNCH", "SETUP", "INPUT", "VOCODER",
                                              "CARRIER", "SILENCE", "BRIDGE TONE", "ENVELOPE", "HARMONY", "SEQ", "PLAY", "MIDI",
                                              "DAW", "SYSTEM"};
static const uint8_t MS_FIRST[MS_COUNT + 1] = {MI_COLOR, MI_LIGHTS, MI_LOWCUT, MI_LAYOUT, MI_SFX, MI_VSETUP, MI_VTRL,
                                               MI_VMODE, MI_VCUT, MI_VBR, MI_VTILT, MI_VEATT, MI_HON, MI_ADV, MI_VEL, MI_TRS,
                                               MI_DMAP, MI_SYS0, MI_COUNT};
#define MI_VOC0 MI_VSETUP
#else
enum { MS_SCREEN, MS_LIGHTS, MS_AUDIO, MS_USB, MS_PUNCH, MS_SEQ, MS_PLAY, MS_MIDI, MS_DAW, MS_SYSTEM, MS_COUNT };
static const char *const MS_NAME[MS_COUNT] = {"SCREEN", "LIGHTS", "AUDIO", "USB", "PUNCH", "SEQ", "PLAY", "MIDI", "DAW",
                                              "SYSTEM"};
static const uint8_t MS_FIRST[MS_COUNT + 1] = {MI_COLOR, MI_LIGHTS, MI_LOWCUT, MI_LAYOUT, MI_SFX, MI_ADV, MI_VEL, MI_TRS,
                                               MI_DMAP, MI_SYS0, MI_COUNT};
#define MI_VOC0 MI_ADV
#endif
_Static_assert(MI_LIGHTS - MI_COLOR <= 4 && MI_LAYOUT - MI_LOWCUT <= 4 && MI_SFX - MI_LAYOUT <= 4 && MI_VOC0 - MI_SFX <= 4 &&
#if SMK_UAC_IN_PROBE
               MI_VTRL - MI_VOC0 <= 4 && MI_VMODE - MI_VTRL <= 4 && MI_VCUT - MI_VMODE <= 4 && MI_VBR - MI_VCUT <= 4 && MI_VTILT - MI_VBR <= 4 && MI_VEATT - MI_VTILT <= 4 && MI_HON - MI_VEATT <= 4 && MI_ADV - MI_HON <= 4 &&
#else
               MI_ADV - MI_VOC0 <= 4 &&
#endif
               MI_VEL - MI_ADV <= 4 &&
               MI_TRS - MI_VEL <= 4 &&
               MI_DMAP - MI_TRS <= 4 && MI_SYS0 - MI_DMAP <= 4 && MI_COUNT - MI_SYS0 <= 4,
               "SMK-37: four rows a section (K5..K8)");
#define MS_GAP 10                                  /* (eight tabs: those around this one that fit, menu_tabs) */
#else
enum { MS_SCREEN, MS_LIGHTS, MS_AUDIO, MS_SEQ, MS_SYSTEM, MS_COUNT };   /* (2.7.1: SEQ) */
static const char *const MS_NAME[MS_COUNT] = {"SCREEN", "LIGHTS", "AUDIO", "SEQ", "SYSTEM"};   /* (AUDIO: and USB) */
static const uint8_t MS_FIRST[MS_COUNT + 1] = {MI_COLOR, MI_LIGHTS, MI_LOWCUT, MI_ADV, MI_CPU1, MI_COUNT};   /* rows of each */
#define MS_GAP 6                                   /* (2.7.1: five sections: 6, the last ends at 236) */
#endif

UI_SMALL static int mi_cpu1_have(void)                      /* the second core is running (cpu1.c; the host tests have no cpu1.c) */
{
#if FELUCCA_CPU1
    return cpu1_state == CPU1_OK;
#else
    return 0;
#endif
}
static const char *const LIGHTS_NAME[LIGHTS_N] = {"OFF", "LOW", "MID", "HIGH"};   /* every button lit, the labels readable */
static const char *const KEYS_NAME[KEYS_N] = {"OFF", "C KEYS", "WHITE KEYS", "ALL KEYS"};      /* keys lit too, at the LIGHTS level */
#define MI_Y0 26                                   /* the first row, under the section tabs */
#if BOARD_SMK37
#define MI_ARM_RESET 0xD5u                         /* ui.arm: RESET MAP armed (OCT+ again; ui_daw.c the rest) */
#endif
#define MI_DY 50                                   /* a row: the label, the value in large type */

UI_SMALL static uint32_t mi_sec(uint32_t i)                 /* the section of item i */
{
    uint32_t k = 0;
    while (k + 1u < MS_COUNT && i >= MS_FIRST[k + 1u])
        k++;
    return k;
}
UI_SMALL static uint32_t mi_row(uint32_t i) { return i - MS_FIRST[mi_sec(i)]; }   /* its row: KNOB 1 + row sets it */

#if BOARD_SMK37
/* USB LAYOUT (usb.c UL_*): the names as the menu shows them, the layouts this build makes and the knob's step
 * through them */
static const char *const UL_MENU_NAME[UL_N] = {"STEREO", "DRUMS+RET", "SYNTHS+RET", "ALL DRY"};
static int replug_pending(void);                   /* (main.c: a new layout or rate is about to be plugged) */
/* the guide under STEM RATE and STEM FILTER (two short lines beside the value): what each choice costs */
#if SMK_UAC_IN_PROBE
/* SETUP > CARRIER as shown: EXT L / EXT R (3, 4) when IN L / IN R is CARRIER, else the saws' choice (0 .. 2) */
UI_SMALL static uint32_t mi_carr_now(void)
{
    return voc_inl % VR_N == VR_CARR ? 3u : voc_inr % VR_N == VR_CARR ? 4u : voc_saws % VCS_N;
}
#endif
#if BOARD_SMK37
#ifndef SMK_PAIR_STATE                                  /* (fx.c's STEREO PAIR; a host test of this file alone) */
#define SP_RET 0u
#define SP_DRUMS 1u
#define SP_T1 2u
#define SP_N 5u
static uint8_t smk_pair;
#define SMK_PAIR_STATE 1
#endif
#endif
UI_SMALL static const char *mi_guide(uint32_t i, uint32_t line)
{
    static const char *const G[4][2] = {{"RESAMPLED ON", "THE KEYBOARD"}, {"LIGHTEST:", "PC CONVERTS"},
                                        {"CLEANEST", "TOP END"}, {"1/3 LESS CPU", "BUSY PATTERNS"}};
    if (i == MI_SRATE)
        return stem_rate & 2u ? (line ? "HUB SAFE" : "16 BIT:") : G[stem_rate & 1u][line & 1u];
    if (i == MI_SFILT)
        return stem_rate & 1u ? (line ? "AT 44.1 KHZ" : "NOT USED") : G[2u + (stem_light & 1u)][line & 1u];
#if SMK_UAC_IN_PROBE
    if (i == MI_VCARR) {                                /* (2 SAWS: where it fits) */
        uint32_t v = mi_carr_now(), su = voc_setup % VSU_N;
        if (v == 2u && su == VSU_FULL)
            return line ? "NOT IN FULL" : "NEEDS A SETUP";
        if (v == 2u && su == VSU_SYNTH)
            return line ? "CPU TIGHT" : "VOC + SYNTH:";
        if (v == 0u)
            return line ? "IN VOC ALONE" : "TWO SAWS";
        return "";
    }
    if (i == MI_VMODE && hm_on)                         /* (HARMONY on: the vocoder off) */
        return line ? "HARMONY ON" : "LOCKED:";
    if (i == MI_SPAIR) {                                /* (SYNTHS+RET only; the return mono where the source was) */
        static const char *const CH[SP_N] = {"", "ON CH 4", "ON CH 1", "ON CH 2", "ON CH 3"};
        if (usb_layout != UL_SYNTHS)
            return line ? "SYNTHS+RET" : "NEEDS";
        return smk_pair % SP_N ? (line ? CH[smk_pair % SP_N] : "RET MONO") : (line ? "ON 5-6" : "EFFECTS");
    }
    if (i == MI_VINL || i == MI_VINR) {                 /* (what the route needs or does) */
        uint32_t r = (i == MI_VINL ? voc_inl : voc_inr) % VR_N;
        if (r == VR_HARM)
            return hm_on ? (line ? "HARMONISER" : "TO THE") : (line ? "HARMONY ON" : "NEEDS");   /* (13 characters
                                                         * at most beside HARMONY: "THE HARMONISER" ran off the screen) */
        if (r == VR_T1 && hm_on)
            return line ? "HARMONY ON" : "T1 IS";
        if ((r == VR_T1 || r == VR_T2) && !(VSU_SKIP(voc_setup % VSU_N) >> (r - VR_T1) & 1u))
            return line ? (r == VR_T1 ? "T1 OFF" : "T2 OFF") : "NEEDS A SETUP";
        if (r == VR_CARR)
            return hm_on ? (line ? "HARMONY ON" : "VOCODER OFF:") : (line ? "GATE IT" : "T3 NOTES");
        if (r == VR_T1 || r == VR_T2)
            return line ? "ITS CHAIN" : "AUDIO THROUGH";
        if (r == VR_T3 && hm_on)
            return line ? "ITS LANE" : "T3 AUDIO:";      /* (HARMONY: T3 an audio track, harmonised on its own lane) */
        if (r == VR_T3)                                 /* (the owner, 2026-11-12: TRACK 3 read as the vocoder only; it
                                                         * is T3's input, dry through its chain in MODE AUDIO) */
            return voc_mode % VM_N == VM_SAW ? (line ? "ITS VOICE" : "T3 VOCODER:") : (line ? "DRY, ITS FX" : "T3 AUDIO:");
        return "";
    }
#endif
    return 0;
}
static int ul_offered(uint32_t l) { return l < UL_N; }
UI_SMALL static uint32_t ul_step(uint32_t l, int32_t s)
{
    uint32_t k, n = l % UL_N;
    for (k = 0; k < UL_N; k++) {
        uint32_t m = s > 0 ? n + 1u : s < 0 ? n + UL_N - 1u : n + 1u;   /* (OCT+: round) */
        if (s > 0 && m >= UL_N)
            return l;                                   /* (the knob stops at the ends) */
        if (s < 0 && n == 0u)
            return l;
        n = m % UL_N;
        if (ul_offered(n))
            return n;
    }
    return l;
}
#endif
/* item i's value as shown, and its colour */
UI_SMALL static const char *mi_value(uint32_t i, uint16_t *c)
{
    *c = C_HI;
    switch (i) {
    case MI_COLOR:                                    /* (2.7) your theme, by its name */
        if (theme_shown()) {
            static char b[9];
            uint32_t k;
            for (k = 0; k < 8u && theme.name[k]; k++)
                b[k] = theme.name[k];
            b[k] = 0;
            return k ? b : "MY THEME";
        }
        return PALETTES[settings.palette % NPALETTES].name;
    case MI_ZOOM: return settings.zoom ? "ON" : "OFF";
    case MI_ACCEL: return knob_accel_off ? "OFF" : "ON";   /* (2.7) the knobs speed up with a fast turn */
    case MI_ADV: return step_adv_off ? "OFF" : "ON";      /* (2.7.1) the STEP page's cursor moves on when the keys are let go */
    case MI_DPREV: return drum_preview ? "ON" : "OFF";     /* (2.7.1) a drum sound picked with a knob is heard */
    case MI_LIGHTS: return LIGHTS_NAME[lights_lvl % LIGHTS_N];
    case MI_KEYS:
        *c = lights_lvl ? C_HI : C_DIM;               /* (needs LIGHTS) */
        return KEYS_NAME[lights_keys % KEYS_N];
    case MI_NOTES: return lights_notes ? "ON" : "OFF";
    case MI_LOWCUT: return settings.lowcut ? "ON" : "OFF";
    case MI_USB: return usb_full ? "FULL" : "MASTER";
#if FELUCCA_CDC
    case MI_SERIAL: return usb_serial ? "ON" : "OFF";
#endif
    case MI_RATE: {                                   /* (2.7 beta) the rates the USB audio input offers (usb.c) */
        static const char *const R[3] = {"AUTO", "44.1 KHZ", "48 KHZ"};
        return R[usb_rate % 3u];
    }
    case MI_FRESO: {                                  /* (2.7.2) the track filter's resonance, 0 (clean) .. 10 (fx.c) */
        static char b[4];
        if (!flt_reso) {
            *c = C_DIM;
            return "OFF";
        }
        fmt_int(b, flt_reso > 10u ? 10 : flt_reso);
        return b;
    }
    case MI_CPU1:                                     /* the render on both cores (fx.c); live, the sound is the same */
        *c = mi_cpu1_have() ? C_HI : C_DIM;
        return cpu1_off ? "OFF" : "ON";
#if BOARD_SMK37
    case MI_VEL: return VEL_NAME[vel_mode % VEL_N];
    case MI_TRS:
        if (trs_out && smo.dead) {                    /* (the UART never finished a byte: OCT+ tries again) */
            *c = C_AMB;
            return "NOT SENDING";
        }
        return trs_out ? "ON" : "OFF";
    case MI_KLAY: return knob_layout ? "SMK" : "FM-1";
    case MI_MODE: return daw_on ? "DAW" : "SYNTH";
    case MI_DCH: {
        static char b[4];
        fmt_int(b, daw_ch);
        return b;
    }
    case MI_LOCAL:
        *c = daw_on ? C_HI : C_DIM;                   /* (DAW mode only) */
        return daw_local ? "ON" : "OFF";
    case MI_DMAP: return daw_user ? "USER" : "DEFAULT";
    case MI_KREL: return daw_rel ? "RELATIVE" : "ABSOLUTE";
    case MI_DRESET: {                                 /* the USER map's entries not as DEFAULT; OCT+ twice resets it */
        static char b[16];
        uint32_t n = daw_user_changed();
        if (ui.arm == MI_ARM_RESET && ui.arm_t) {
            *c = C_AMB;
            return "OCT+ AGAIN";
        }
        if (!n) {
            *c = C_DIM;
            return "NONE CHANGED";
        }
        fmt_int(b, (int32_t)n);
        str_cpy(b + str_len(b), " CHANGED", 9);
        return b;
    }
    case MI_LIMIT: return lim_classic ? "CLASSIC" : "SOFT";
    case MI_LAYOUT: return UL_MENU_NAME[usb_layout % UL_N];   /* (usb.c: at the next start) */
    case MI_SFX: {                                    /* the stems' effects: the SLICER's STUT, or the punch-ins */
        static const char *const N[SFX_N] = {"SLICER", "PUNCH", "PUNCH HQ"};
        *c = usb_layout_now() ? C_HI : C_DIM;          /* (STEREO: no stems, no effect) */
        return N[stem_fx % SFX_N];
    }
    case MI_PFADE: {                                  /* the punch-ins' fade (0 the shortest .. 15) */
        static char b[4];
        fmt_int(b, song.g[G_FXFADE]);
        return b;
    }
    case MI_XFADE: {                                  /* the stems' shortest stitch (dim without STEM FX) */
        static const char *const N[TPX_N] = {"2.9 MS", "1.5 MS", "5.8 MS"};
        *c = usb_layout_now() && stem_fx ? C_HI : C_DIM;
        return N[tp_xfl % TPX_N];
    }
    case MI_CURVE:
        *c = usb_layout_now() && stem_fx ? C_HI : C_DIM;
        return tp_lin ? "LINEAR" : "EQUAL";
#if SMK_UAC_IN_PROBE
    case MI_VCARR: {                                  /* the carrier: the saws, or a channel (dim: not vocoding; 2 SAWS
                                                       * in FULL: amber, one plays) */
        static const char *const N[5] = {"AUTO", "1 SAW", "2 SAWS", "EXT L", "EXT R"};
        uint32_t v = mi_carr_now();
        *c = !(VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW) ? C_DIM : v == 2u && voc_setup % VSU_N == VSU_FULL ? C_AMB : C_HI;
        return N[v];
    }
    case MI_VEATT: {                                  /* the bands' envelopes' attack (dim: not vocoding) */
        static const char *const N[VOC_EATT_N] = {"3 MS", "5 MS", "8 MS", "12 MS", "20 MS", "30 MS"};
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW ? C_HI : C_DIM;
        return N[voc_eatt % VOC_EATT_N];
    }
    case MI_VEREL: {                                  /* .. their release */
        static const char *const N[VOC_EREL_N] = {"8 MS", "15 MS", "25 MS", "40 MS", "60 MS", "100 MS"};
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW ? C_HI : C_DIM;
        return N[voc_erel % VOC_EREL_N];
    }
    case MI_VSETUP: {                                 /* the tracks beside the audio track, and HQ (dim: AUDIO IN off) */
        static const char *const N[VSU_N] = {"FULL", "VOC + DRUMS", "VOC + SYNTH", "VOC ALONE"};
        static const char *const NH[VSU_N] = {"FULL", "NO SYNTH", "NO DRUMS", "INPUTS ONLY"};   /* (HARMONY on: no
                                                       * vocoder; the same four settings, harmoniser-spec.md §10a-2) */
        *c = VOC_AUDIO_ON ? C_HI : C_DIM;
        return (hm_on ? NH : N)[voc_setup % VSU_N];
    }
    case MI_VINL:                                     /* each USB channel's destination (dim: AUDIO IN off; a track
                                                       * the SETUP leaves on: amber, not used) */
    case MI_VINR: {
        static const char *const N[VR_N] = {"TRACK 3", "CARRIER", "T1", "T2", "OFF", "HARMONY"};
        uint32_t r = (i == MI_VINL ? voc_inl : voc_inr) % VR_N;
        *c = !VOC_AUDIO_ON ? C_DIM : ((r == VR_T1 || r == VR_T2) && !(VSU_SKIP(voc_setup % VSU_N) >> (r - VR_T1) & 1u)) ||
                                         (r == VR_T1 && hm_on) || (r == VR_HARM && !hm_on) || (r == VR_CARR && hm_on)
                                         ? C_AMB : C_HI;
        return N[r];
    }
    case MI_VTRL:                                     /* each side's level before its route, -18 .. +6 dB (dim: AUDIO
                                                       * IN off) */
    case MI_VTRR: {
        static char b[8];
        int32_t db = (int32_t)(voc_trim[i == MI_VTRR] % VOC_TRIM_N) - (int32_t)VOC_TRIM_0;
        *c = VOC_AUDIO_ON ? C_HI : C_DIM;
        b[0] = db > 0 ? '+' : db < 0 ? '-' : 0;
        fmt_int(b + (db != 0), db < 0 ? -db : db);
        str_cpy(b + str_len(b), " DB", 4);
        return b;
    }
    case MI_VBRM:                                     /* FIXED, FOLLOW (dim: no bridging: the same) */
        *c = VOC_AUDIO_ON && voc_bridge ? C_HI : C_DIM;
        return voc_brmode ? "FOLLOW" : "FIXED";
    case MI_HON:                                      /* track 1 the harmoniser, or a synth */
        *c = C_HI;
        return hm_on ? "ON: T1" : "OFF";
    case MI_HINT:                                     /* the keys: fixed intervals from C4, or from the sung note in the
                                                       * song's key (dim: HARMONY off) */
        *c = hm_on ? C_HI : C_DIM;
        return hm_int ? "SCALE" : "FIXED";
    case MI_HNOTE: {                                  /* the tracker's note (dim: it runs with INTERVALS SCALE only) */
        static char b[6];
        uint32_t nt = hm.tnote;
        *c = hm_on && hm_int ? C_HI : C_DIM;
        if (!hm_on || !hm_int || !nt)
            return "--";
        note_name(b, nt);
        return b;
    }
    case MI_VGDEL: {                                  /* the external carrier's gate held after the notes, 0 .. 50 ms
                                                       * (dim: no channel to CARRIER) */
        static const char *const N[11] = {"0 MS", "5 MS", "10 MS", "15 MS", "20 MS", "25 MS", "30 MS", "35 MS",
                                          "40 MS", "45 MS", "50 MS"};
        *c = VOC_EXT_CARR ? C_HI : C_DIM;
        return N[voc_gdel % 11u];
    }
    case MI_VMODE: {                                  /* track 3, the audio track: the input dry, or vocoded over the
                                                       * saws (dim: AUDIO IN off, track 3 its engine) */
        static const char *const N[VM_N] = {"AUDIO", "VOCODER"};
        *c = !VOC_AUDIO_ON ? C_DIM : hm_on ? C_AMB : C_HI;   /* (HARMONY on: AUDIO, locked) */
        return N[voc_mode % VM_N];
    }
    case MI_VCUT:                                     /* the carrier's low-pass, 0 .. 127 (dim: not vocoding) */
    case MI_VRES: {
        static char b[6];
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW ? C_HI : C_DIM;
        fmt_int(b, i == MI_VCUT ? voc_cut : voc_res);
        return b;
    }
    case MI_VVM: {                                    /* the VSM201's VOICE MODE */
        static const char *const N[VV_N] = {"AUTO", "VOICED", "UNVOICED"};
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW ? C_HI : C_DIM;
        return N[voc_vmode % VV_N];
    }
    case MI_VPINK:                                    /* the unvoiced replacement: WHITE, PINK (dim: VOICED only) */
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW && voc_vmode % VV_N != VV_VOICED ? C_HI : C_DIM;
        return voc_pink == 2u ? "SOFT" : voc_pink ? "PINK" : "WHITE";
    case MI_VMWA: {                                   /* MOD WHEEL (VOCODER): what the wheel does on track 3 */
        static const char *const N[VOC_MWA_N] = {"VIBRATO", "BRIDGE TILT", "NOISE BLEND"};
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW ? C_HI : C_DIM;
        return N[voc_mwa % VOC_MWA_N];
    }
    case MI_VAGC: {                                   /* the carrier AGC's ceiling */
        static const char *const N[VOC_AGC_N] = {"OFF", "PEAKS", "LIGHT", "FULL"};
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW ? C_HI : C_DIM;
        return N[voc_agc % VOC_AGC_N];
    }
    case MI_VBR: {                                    /* the silence bridging's level: OFF, -48 .. 0 dB */
        static char b[10];
        uint32_t k = voc_bridge % VOC_BR_N;
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW ? C_HI : C_DIM;
        if (!k)
            return "OFF";
        b[0] = '-';
        fmt_int(b + (k < VOC_BR_N - 1u), (int32_t)(3u * (VOC_BR_N - 1u - k)));
        str_cpy(b + str_len(b), " DB", 4);
        return b;
    }
    case MI_VTHR: {                                   /* the speech / pause detector's threshold (dim: no bridging) */
        static char b[10];
        int32_t db = VOC_THR_DB[voc_thr % VOC_THR_N];
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW && voc_bridge ? C_HI : C_DIM;
        b[0] = '-';
        fmt_int(b + 1, -db);
        str_cpy(b + str_len(b), " DB", 4);
        return b;
    }
    case MI_VTILT: {                                  /* the bridge's tilt, -3 .. +3 dB an octave (dim: no bridging) */
        static char b[12];
        int32_t h = (int32_t)(voc_tilt % VOC_TILT_N) - (int32_t)VOC_TILT_FLAT;   /* half dB */
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW && voc_bridge ? C_HI : C_DIM;
        if (!h)
            return "FLAT";
        b[0] = h < 0 ? '-' : '+';
        fmt_int(b + 1, (h < 0 ? -h : h) / 2);
        str_cpy(b + str_len(b), h & 1 ? ".5 DB/OCT" : ".0 DB/OCT", 10);
        return b;
    }
    case MI_VCOL: {                                   /* the input's envelopes in a pause, 0 .. 100 % */
        static char b[6];
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW ? C_HI : C_DIM;
        fmt_int(b, (int32_t)(voc_colour % VOC_COLOUR_N) * 10);
        str_cpy(b + str_len(b), " %", 3);
        return b;
    }
    case MI_VBLEND:                                   /* SUM, FILL (dim: COLOUR 0 or no bridging: the same) */
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW && voc_bridge && voc_colour ? C_HI : C_DIM;
        return voc_blend ? "FILL" : "SUM";
    case MI_VNORM:                                    /* FLAT, NORMAL (the VSM201's weights), EMPHASIS (the vocoder page's
                                                       * channels: the next build, as NORMAL); the page's VOCODER selector */
        *c = VOC_AUDIO_ON && voc_mode % VM_N == VM_SAW ? C_HI : C_DIM;
        return voc_emsel ? "EMPHASIS" : voc_norm ? "NORMAL" : "FLAT";
    case MI_VLVL: {                                   /* make-up gain, -12 .. +18 dB in 3 dB steps */
        static char b[10];
        int32_t db = 3 * (int32_t)(voc_level % VOC_LEVEL_N) - 12;
        b[0] = db < 0 ? '-' : '+';
        fmt_int(b + 1, db < 0 ? -db : db);
        str_cpy(b + str_len(b), " DB", 4);
        return b;
    }
    case MI_VDRY:                                     /* the plain voice blended back, the speech addition */
    case MI_VHF: {
        static const char *const N[VOC_MIX_N] = {"OFF", "-30 DB", "-27 DB", "-24 DB", "-21 DB", "-18 DB", "-15 DB",
                                                 "-12 DB", "-9 DB", "-6 DB", "-3 DB", "0 DB"};
        return N[(i == MI_VDRY ? voc_dry : voc_hf) % VOC_MIX_N];
    }
#endif
#if SMK_UAC_IN_PROBE && !SMK_UAC_PROBE
    case MI_UIN: {                                    /* the computer's audio: OFF, ON, TEST (dim: none arriving) */
        static const char *const N[3] = {"OFF", "ON", "TEST"};
        *c = uin.alt ? C_HI : C_DIM;
        return N[uin_mode % 3u];
    }
#endif
    case MI_FRET: return sq_carry ? "CARRY" : "SLIP";   /* FLIP / LOOP BACK let go: the grid's place, or on from theirs */
    case MI_SRATE:                                    /* the stems' rate (STEREO: USB RATE's, this one dim) */
        *c = usb_layout != UL_STEREO ? C_HI : C_DIM;
    {
        static const char *const N[4] = {"48 KHZ", "44.1 KHZ", "48 / 16 BIT", "44.1 / 16"};   /* (16 bit: a hub's) */
        return N[stem_rate & 3u];
    }
    case MI_SFILT:                                    /* the resampler's filter (44.1: none, dim) */
        *c = usb_layout != UL_STEREO && !(stem_rate & 1u) ? C_HI : C_DIM;
        return stem_light ? "LIGHT" : "FULL";
    case MI_SPAIR: {                                  /* channels 5-6: the effects return, or a source in stereo (amber
                                                       * outside SYNTHS+RET: not used there) */
        static const char *const N[SP_N] = {"RET", "DRUMS", "T1", "T2", "T3"};
        *c = usb_layout == UL_SYNTHS ? C_HI : C_AMB;
        return N[smk_pair % SP_N];
    }
    case MI_BUP:
    case MI_BDN: {                                    /* semitones of the pitch wheel up / down, the internal sounds */
        static char b[2][16];
        uint32_t st = i == MI_BUP ? bend_up : bend_dn;
        char *t = b[i == MI_BDN];
        if (!st)
            return "OFF";
        t[0] = i == MI_BUP ? '+' : '-';
        fmt_int(t + 1, (int32_t)st);
        str_cpy(t + str_len(t), st == 1u ? " SEMITONE" : " SEMITONES", 11);
        return t;
    }
#endif
    case MI_LCLEAR: {                                 /* (2.6) the CCs MIDI LEARN holds; OCT+ clears them */
        static char b[8];
        uint32_t n = ml_count();
        if (!n) {
            *c = C_DIM;
            return "NONE";
        }
        fmt_int(b, (int32_t)n);
        str_cpy(b + str_len(b), " CC", 4);
        return b;
    }
    default:
        *c = C_DIM;
        return "";                                    /* (an action: OCT+ opens it) */
    }
}

#if SMK_UAC_IN_PROBE
/* INPUT's meters: each side's peak after its TRIM (usb.c uin_pk, taken and cleared each frame), on a -48 .. 0 dBFS bar:
 * the level falls back ~24 dB a second, the peak holds ~1.5 s; green to -12, amber to -3, red above (0 dBFS: CLIP); a
 * white tick at -10 dBFS, the level to aim for (VOCODER.md) */
#define MTR_W 96                                   /* (the bar's pixels: 2 a dB) */
static uint8_t mtr_lv[2], mtr_pk[2], mtr_hold[2], mtr_clip[2];
static uint32_t mtr_px(uint32_t p)                 /* |sample| (0 .. 32768) -> 0 .. MTR_W (2 px a dB above -48 dBFS) */
{
    static const uint16_t T[12] = {32768, 30935, 29205, 27571, 26029, 24573, 23198, 21900, 20675, 19519, 18427, 17396};
    uint32_t e = 0, k = 0;                         /* (T: 0, -0.5 .. -5.5 dB) */
    if (p == 0u)
        return 0;
    while (p < 16384u && e < 9u)                   /* (6.02 dB an octave: 12 px) */
        p <<= 1, e++;
    while (k < 11u && p < T[k + 1u])
        k++;
    k = e * 12u + k;                               /* (half dBs down) */
    return k >= 96u ? 0u : MTR_W - k;
}
UI_SMALL static void mtr_update(void)
{
    uint32_t c, p;
    for (c = 0; c < 2u; c++) {
        p = uin_pk[c];
        uin_pk[c] = 0;                             /* (taken: the audio core sets it again) */
        p = mtr_px(p);
        mtr_lv[c] = (uint8_t)(p > mtr_lv[c] ? p : mtr_lv[c] > 2u ? mtr_lv[c] - 2u : 0u);   /* (~24 dB/s at 30 fps) */
        if (p >= mtr_pk[c] || !mtr_hold[c])
            mtr_pk[c] = (uint8_t)p, mtr_hold[c] = 45u;
        else
            mtr_hold[c]--;
        mtr_clip[c] = (uint8_t)(p >= MTR_W ? 45u : mtr_clip[c] ? mtr_clip[c] - 1u : 0u);
    }
}
UI_SMALL static void mtr_draw(int32_t x, int32_t y, uint32_t c)
{
    int32_t lv = mtr_lv[c], pk = mtr_pk[c], g = MTR_W - 24, a = MTR_W - 6;   /* (-12, -3 dBFS) */
    cv_rect(x, y, MTR_W, 10, C_LINE);
    cv_rect(x, y, lv < g ? lv : g, 10, RGB(70, 210, 100));
    if (lv > g)
        cv_rect(x + g, y, (lv < a ? lv : a) - g, 10, RGB(255, 190, 0));
    if (lv > a)
        cv_rect(x + a, y, lv - a, 10, RGB(255, 60, 60));
    if (pk > 0)
        cv_rect(x + pk - 1, y - 2, 2, 14, pk > a ? RGB(255, 60, 60) : pk > g ? RGB(255, 190, 0) : RGB(70, 210, 100));
    cv_rect(x + MTR_W - 20, y + 11, 1, 4, C_WHITE);   /* (-10 dBFS: aim here) */
    if (mtr_clip[c])
        cv_text(x, y + 14, &FONT_S, "CLIP", RGB(255, 60, 60));
    else
        cv_text(x, y + 14, &FONT_S, "AIM -10", C_DIM);
}
#endif
UI_SMALL static void draw_menu(void)
{
    uint32_t i, pass, sec = mi_sec(ui.menu_sel % MI_COUNT);
    uint32_t sig = ui.menu * 7u + ui.menu_sel * 131u + settings.palette * 1009u + settings.lowcut * 7919u +
                   settings.zoom * 104729u + lights_lvl * 1299709u + lights_keys * 15485863u + ml_count() * 2654435u +
                   lights_notes * 32452843u + usb_full * 49979687u + usb_serial * 86028121u + usb_rate * 15486719u + theme_shown() * 32452867u + theme.crc + cpu1_off * 122949829u +
                   knob_accel_off * 179424673u + drum_preview * 236887691u + step_adv_off * 373587883u + flt_reso * 512927357u;
#if BOARD_SMK37
    sig += vel_mode * 179424673u + trs_out * 373587883u + smo.dead * 573259391u + bend_up * 817504243u +
           bend_dn * 941083987u + lim_classic * 1500450271u + knob_layout * 2147483647u + usb_layout * 433494437u +
           stem_fx * 160481183u + stem_rate * 275604541u + stem_light * 393342743u + tp_xfl * 694847539u +
           tp_lin * 795918341u + (uint32_t)song.g[G_FXFADE] * 899809363u + sq_carry * 961748927u +
           (uint32_t)usb_layout_now() * 452930459u + (uint32_t)replug_pending() * 553105243u;
#if SMK_UAC_IN_PROBE
    sig += voc_mode * 1000033u + voc_level * 1000037u + voc_dry * 1000039u + voc_hf * 1000081u + uin.alt * 1000099u +
           (uint32_t)UIN_TEST * 1000117u + voc_cut * 1000121u + voc_res * 1000133u + (uint32_t)VOC_AUDIO_ON * 1000151u +
           voc_vmode * 1000159u + voc_pink * 1000171u + voc_bridge * 1000183u + voc_thr * 1000187u +
           voc_tilt * 1000193u + voc_colour * 1000199u + voc_blend * 1000211u + voc_agc * 1000213u +
           voc_norm * 1000231u + voc_emsel * 1000249u + voc_mwa * 1000253u + voc_setup * 1000273u +
           voc_inl * 1000289u + voc_inr * 1000291u + voc_gdel * 1000303u + voc_eatt * 1000313u + voc_erel * 1000333u + voc_saws * 1000357u +
           voc_brmode * 1000409u;
    sig += voc_trim[0] * 1000363u + voc_trim[1] * 1000367u;
#ifdef SMK_HARM
    sig += hm_on * 1000427u + hm_want * 1000429u + (uint32_t)hm.tnote * 1000433u +   /* (HARMONY, SUNG NOTE: 67ba07e left
                                                         * them out, the screen stayed on OFF though the knob turned it) */
           (uint32_t)(trk[0].p[P_SCALE] != 0) * 1000439u;
#endif
    if (ui.menu == 1 && sec == MS_INPUT) {          /* (the meters move: a frame each time they do) */
        mtr_update();
        sig += (uint32_t)(mtr_lv[0] >> 1) * 1000373u + (uint32_t)(mtr_lv[1] >> 1) * 1000379u + mtr_pk[0] * 1000381u +
               mtr_pk[1] * 1000393u + (uint32_t)(mtr_clip[0] != 0) * 1000397u + (uint32_t)(mtr_clip[1] != 0) * 1000403u;
    }   /* (2026-11-11: SETUP's four: without them
                                                         * the screen did not follow the knob) */
#endif
    sig += daw_on * 1000003u + daw_ch * 2000029u + daw_local * 3000017u + daw_user * 4000037u + daw_rel * 5000011u +
           daw_user_changed() * 6000047u + (ui.arm == MI_ARM_RESET && ui.arm_t) * 7000003u;
#endif
#if BOARD_SMK37
    if (ui.menu == 1) {                             /* (2026-11-12) every row of the section as it would be drawn: its
                                                     * value, the value's colour and the guide lines. The list above is
                                                     * kept by hand and twice missed new items (SETUP's four in
                                                     * 2b96130, HARMONY in 67ba07e: the knob worked, the screen stayed),
                                                     * this does not depend on it: whatever a row shows, a change of it
                                                     * redraws the menu */
        uint32_t k;
        for (k = MS_FIRST[sec]; k < MS_FIRST[sec + 1u]; k++) {
            uint16_t c = 0;
            const char *v = mi_value(k, &c), *g0 = mi_guide(k, 0), *g1 = mi_guide(k, 1);
            sig = str_hash(sig, v ? v : "") * 31u + c;
            sig = str_hash(sig, g0 ? g0 : "");
            sig = str_hash(sig, g1 ? g1 : "");
        }
    }
#endif
    if (!ui.force && sig == ui.menu_sig)
        return;
    ui.menu_sig = sig;
    if (ui.force)                                   /* head + rule + two bands cover rows 0..229 */
        lcd_fill(0, H_HEAD + 1 + 124 + 95, 240, 240 - (H_HEAD + 1 + 124 + 95), C_BLACK);
    cv_begin(240, H_HEAD, C_BLACK);
    cv_text(4, 1, &FONT_S, ui.menu == 2 ? "ABOUT" : "MENU", C_HI);
    if (ui.menu != 2) {                             /* the section and its number, as a page's "ENV DEST 2/2" */
        char t[16];
        str_cpy(t, MS_NAME[sec], sizeof t);
        str_cpy(t + str_len(t), " ", 2);
        fmt_int(t + str_len(t), (int32_t)sec + 1);
        str_cpy(t + str_len(t), "/", 2);
        fmt_int(t + str_len(t), MS_COUNT);
        cv_text(236 - text_w(&FONT_S, t), 1, &FONT_S, t, C_GRAY);
    }
    cv_blit(0, Y_HEAD);
    lcd_fill(0, H_HEAD, 240, 1, C_LINE);
    for (pass = 0; pass < 2u; pass++) {             /* the canvas holds 124 rows: draw in two bands */
        cv_begin(240, pass ? 95u : 124u, C_BLACK);
        cv_oy = pass ? -124 : 0;
        if (ui.menu == 2) {
            cv_text(4, 4, &FONT_L, BRAND_NAME, C_WHITE);
            cv_rect(BRAND_SAIL_X, 10, 8, 4, TE_COL[0]), cv_rect(BRAND_SAIL_X, 16, 12, 4, TE_COL[1]);   /* the sail */
            cv_rect(BRAND_SAIL_X, 22, 16, 4, TE_COL[2]), cv_rect(BRAND_SAIL_X, 28, 20, 4, TE_COL[3]);
            cv_text(4, 36, &FONT_S, BRAND_BASED, C_AMB);
#ifdef SMK_BUILD_ID
            {   /* SMK-37: the commit built (and the vocoder build's mark), right of BASED ON */
                const char *id = SMK_UAC_IN_PROBE ? SMK_BUILD_ID " VOC" : SMK_BUILD_ID;
                cv_text(236 - text_w(&FONT_S, id), 36, &FONT_S, id, C_GRAY);
            }
#endif
            cv_text(4, 54, &FONT_S, FELUCCA_VERSION, C_HI);
            cv_text(236 - text_w(&FONT_S, __DATE__), 54, &FONT_S, __DATE__, C_GRAY);   /* build date */
            cv_text(cv_text(4, 72, &FONT_S, "LEO KUROSHITA", C_HI) + 8, 72, &FONT_S, "@KUROGEDELIC", C_AMB);
            cv_text(4, 88, &FONT_S, "H\xDCGELTON INSTRUMENTS", C_HI);   /* Latin-1 U-umlaut */
            cv_text(4, 104, &FONT_S, "HUGELTON.COM", C_AMB);
#if FELUCCA_CPU1
            {
                char sv[12];
                cpu1_supplies(sv);                  /* (2.7) SYSVDD, VDC14 now: 1.38V 1.60V (fm1_sys.h) */
                cv_text(236 - text_w(&FONT_S, sv), 104, &FONT_S, sv, C_GRAY);
            }
#endif
            cv_text(4, 119, &FONT_S, "GPL-3.0, NO WARRANTY", C_HI);
#if FELUCCA_CPU1                                    /* (the host tests include this file without cpu1.c) */
            cv_text(236 - text_w(&FONT_S, cpu1_label()), 119, &FONT_S, cpu1_label(), C_AMB);   /* 2 CORES / 1 CORE (cpu1.c) */
#endif
#ifdef BRAND_URL
            cv_text(4, 132, &FONT_S, BRAND_URL, C_AMB);
#else
            cv_text(4, 132, &FONT_S, "GITHUB.COM/ISOD89/SLOOP-FM1", C_AMB);   /* (the source of this firmware) */
#endif
            cv_text(4, 146, &FONT_S, "FONT: TERMINUS (OFL)", C_DIM);
            cv_text(4, 159, &FONT_S, "SAMPLES: VERSILIAN (CC0)", C_DIM);
            cv_text(4, 172, &FONT_S, "+ SONIC PI (CC0)", C_DIM);
            cv_text(4, 185, &FONT_S, "PHASE: CRISPYZEBRA (GPL)", C_DIM);
            cv_text(4, 198, &FONT_S, "VOICE: REF. KLATTSCH (MIT)", C_DIM);
        } else {
            int32_t x = 4;
#if BOARD_SMK37
            uint32_t a = sec, z = sec;              /* the tabs around this one that fit (232 px): < > where there are more */
            int32_t tw = text_w(&FONT_S, MS_NAME[sec]);
            for (;;) {
                int32_t wr = z + 1u < MS_COUNT ? text_w(&FONT_S, MS_NAME[z + 1u]) + MS_GAP : 999;
                int32_t wl = a > 0u ? text_w(&FONT_S, MS_NAME[a - 1u]) + MS_GAP : 999;
                if (tw + wr <= 208)
                    tw += wr, z++;
                else if (tw + wl <= 208)
                    tw += wl, a--;
                else
                    break;
            }
            x = 120 - tw / 2;
            if (a > 0u)
                cv_text(2, 4, &FONT_S, "<", C_DIM);
            if (z + 1u < MS_COUNT)
                cv_text(230, 4, &FONT_S, ">", C_DIM);
            for (i = a; i <= z; i++) {
#else
            for (i = 0; i < MS_COUNT; i++) {        /* the sections as tabs: this one lit */
#endif
                int32_t w = text_w(&FONT_S, MS_NAME[i]);
                if (i == sec)
                    cv_rect(x - 2, 3, w + 4, 18, C_LINE);
                cv_text(x, 4, &FONT_S, MS_NAME[i], i == sec ? C_WHITE : C_DIM);
                x += w + MS_GAP;
            }
            for (i = MS_FIRST[sec]; i < MS_FIRST[sec + 1u]; i++) {
                uint32_t r = i - MS_FIRST[sec];
                int32_t y = MI_Y0 + (int32_t)r * MI_DY, xv;
                int cur = i == ui.menu_sel;
                uint16_t vc;
                const char *v = mi_value(i, &vc);
#if BOARD_SMK37
                char k[4] = {'K', knob_digit(EN_K1 + r), 0, 0};   /* (KNOB 1..4: K5..K8, or K1..K4, KNOB LAYOUT) */
#else
                char k[4] = {'K', (char)('1' + r), 0, 0};
#endif
                cv_rect(4, y + 2, 4, MI_DY - 8, cur ? C_WHITE : TE_COL[r & 3u]);   /* the knob's colour */
                xv = cv_text(14, y, &FONT_S, k, TE_COL[r & 3u]);
                cv_text(xv + 6, y, &FONT_S, MI_NAME[i], cur ? C_WHITE : C_GRAY);
                if (v[0] && text_w(&FONT_L, v) <= 222) {
                    xv = cv_text(14, y + 16, &FONT_L, v, cur && vc == C_HI ? C_WHITE : vc);
                } else if (v[0]) {                      /* (too wide for the large type: WHITE KEYS) */
                    xv = cv_text(14, y + 24, &FONT_S, v, cur && vc == C_HI ? C_WHITE : vc);
                } else {
                    xv = cv_text(14, y + 24, &FONT_S, "OCT+ OPENS", cur ? C_WHITE : C_DIM);
                }
                if (i == MI_COLOR) {                    /* the palette's colours */
                    uint32_t q;
                    for (q = 0; q < 5u; q++)
                        cv_rect(xv + 10 + (int32_t)q * 14, y + 28, 10, 10, pal[q]);
                }
                if (i == MI_RATE && usb_rate != usb_rate_now())
                    cv_text(xv + 8, y + 30, &FONT_S, "RESTART", C_AMB);   /* (usb.c: at the next start) */
#if BOARD_SMK37
                if ((i == MI_LAYOUT || i == MI_SRATE) && replug_pending())
                    cv_text(xv + 8, y + 30, &FONT_S, "RE-PLUG", C_AMB);   /* (main.c: a moment after the knob) */
                else if (i == MI_SRATE || i == MI_SFILT
#if SMK_UAC_IN_PROBE
                         || i == MI_VINL || i == MI_VINR || i == MI_VCARR || i == MI_SPAIR || (i == MI_VMODE && hm_on)
#endif
                         ) {                    /* (STEREO PAIR and MODE's lock: their guides were set, never drawn) */
                    uint16_t gc = vc == C_HI ? C_GRAY : C_DIM;
                    cv_text(xv + 8, y + 16, &FONT_S, mi_guide(i, 0), gc);   /* (y + 30: a fourth row's second line
                                                         * inside the screen; at y + 34 it ran off the bottom) */
                    cv_text(xv + 8, y + 30, &FONT_S, mi_guide(i, 1), gc);
                }
#endif
#if FELUCCA_CDC
                if (i == MI_SERIAL && usb_serial != usb_cdc_now())
                    cv_text(xv + 8, y + 30, &FONT_S, "RESTART", C_AMB);   /* (usb.c: at the next start) */
#endif
#if SMK_UAC_IN_PROBE
                if (i == MI_VTRL || i == MI_VTRR)
                    mtr_draw(xv + 10 > 130 ? xv + 10 : 130, y + 20, i == MI_VTRR);
#endif
                if (i == MI_CPU1 && !mi_cpu1_have())
                    cv_text(xv + 8, y + 30, &FONT_S, "NOT RUNNING", C_AMB);   /* (cpu1.c: ABOUT says 1 CORE) */
            }
#if BOARD_SMK37
            if (MS_FIRST[sec + 1u] - MS_FIRST[sec] <= 3u) {   /* (a fourth row: no room for the help) */
                char h[24] = "K2 SECTION  K5-K8 SET";       /* the knobs as KNOB LAYOUT has them */
                h[1] = knob_digit(EN_SELECT), h[13] = knob_digit(EN_K1), h[16] = knob_digit(EN_K4);
                cv_text(4, MI_Y0 + 3 * MI_DY + 6, &FONT_S, h, C_DIM);
                cv_text(4, MI_Y0 + 3 * MI_DY + 22, &FONT_S, "OCT+ OK   OCT- CLOSE", C_DIM);
            }
#else
            if (MS_FIRST[sec + 1u] - MS_FIRST[sec] <= 3u) {   /* (SYSTEM has four rows with 2ND CORE: its fourth
                                                               * row, ABOUT, takes the help's place) */
                cv_text(4, MI_Y0 + 3 * MI_DY + 6, &FONT_S, "SELECT SECTION  KNOB SETS", C_DIM);
                cv_text(4, MI_Y0 + 3 * MI_DY + 22, &FONT_S, "OCT+ OK   OCT- CLOSE", C_DIM);   /* (rows .. 218) */
            }
#endif
        }
        cv_oy = 0;
        cv_blit(0, H_HEAD + 1 + pass * 124u);
    }
}

UI_SMALL static void enc_drop(void)                             /* knob turns nobody takes */
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        panel_enc(k);
}

UI_SMALL static void menu_close(void)
{
    if (song.playing || transport_req)
        settings_later = 1;                            /* (a flash write stops the audio: once stopped) */
    else
        settings_save();                               /* palette / panel table, if changed */
    ui.menu = 0;
    ui.force = 1;
    go_home();
}

/* item i: a knob turned s (> 0 right, < 0 left), or 0: OCT+ (steps round, toggles, or opens) */
UI_SMALL static void mi_set(uint32_t i, int32_t s)
{
    switch (i) {
    case MI_COLOR: {                                   /* both ways round; OCT+ the next. 2.7: your theme after MONO
                                                        * (the palette number stays the last built-in one: an older
                                                        * SLOOP shows that) */
        uint32_t n = NPALETTES + (theme_valid ? 1u : 0u), at = theme_shown() ? NPALETTES : settings.palette % NPALETTES;
        at = (at + (s < 0 ? n - 1u : 1u)) % n;
        theme_on = (uint8_t)(at == NPALETTES);
        if (!theme_on)
            settings.palette = at;
        palette_set(settings.palette);
        ui.force = 1;
        break;
    }
    case MI_ACCEL:                                     /* (2.7) right ON, left OFF; OCT+ toggles (ui_input.c accel) */
        knob_accel_off = (uint8_t)(s > 0 ? 0u : s < 0 ? 1u : !knob_accel_off);
        break;
    case MI_ADV:                                       /* (2.7.1) right ON, left OFF; OCT+ toggles (ui_input.c seq_entry) */
        step_adv_off = (uint8_t)(s > 0 ? 0u : s < 0 ? 1u : !step_adv_off);
        break;
    case MI_DPREV:                                     /* (2.7.1) right ON, left OFF; OCT+ toggles (ui_studio.c drum_hear) */
        drum_preview = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !drum_preview);
        break;
    case MI_ZOOM:
    case MI_LOWCUT: {                                  /* right ON, left OFF; OCT+ toggles */
        uint32_t *v = i == MI_LOWCUT ? &settings.lowcut : &settings.zoom;
        *v = s > 0 ? 1u : s < 0 ? 0u : !*v;
        fx_lowcut = (uint8_t)(settings.lowcut != 0);
        break;
    }
    case MI_USB:                                       /* right FULL, left MASTER */
        usb_full = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !usb_full);
        break;
#if FELUCCA_CDC
    case MI_SERIAL:
        usb_serial = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !usb_serial);
        break;
#endif
    case MI_RATE:                                      /* (2.7 beta) AUTO 44.1 48: the knob steps (stops at the ends),
                                                        * OCT+ round; at the next start (usb.c usb_rate_mode) */
        if (s > 0 && usb_rate < 2u)
            usb_rate++;
        else if (s < 0 && usb_rate > 0u)
            usb_rate--;
        else if (!s)
            usb_rate = (uint8_t)((usb_rate + 1u) % 3u);
        break;
    case MI_FRESO:                                     /* (2.7.2) 0 (OFF/clean) .. 10, stops at the ends; OCT+ steps round */
        if (s > 0 && flt_reso < 10u)
            flt_reso++;
        else if (s < 0 && flt_reso > 0u)
            flt_reso--;
        else if (!s)
            flt_reso = (uint8_t)((flt_reso >= 10u ? 0u : flt_reso + 1u));
        break;
    case MI_CPU1:                                      /* right ON, left OFF; at once (the next block renders so) */
        cpu1_off = (uint8_t)(s > 0 ? 0u : s < 0 ? 1u : !cpu1_off);
        cpu1_units = cpu1_off ? 0 : -1;
        if (!cpu1_off && !cpu1_on())                   /* 2.7: core 1 starts only at power-on */
            ui_message("ON AT NEXT POWER-ON");
        break;
    case MI_NOTES:
        lights_notes = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !lights_notes);
        break;
#if BOARD_SMK37
    case MI_TRS:                                       /* right ON, left OFF; OCT+ toggles (ON tries the UART again) */
        smo_on(s > 0 ? 1u : s < 0 ? 0u : !trs_out);
        break;
    case MI_BUP:                                       /* 0 (OFF), 1, 2, 3, 4, 5, 7, 12 (stops at the ends); OCT+ steps round */
        bend_up = (uint8_t)bend_step(bend_up, s);
        break;
    case MI_BDN:
        bend_dn = (uint8_t)bend_step(bend_dn, s);
        break;
    case MI_SFX:                                       /* SLICER PUNCH PUNCH HQ, the knob stops at the ends, OCT+
                                                        * round; at once (fx.c: the memory shared out again) */
        stem_fx = (uint8_t)(s > 0 ? (stem_fx + 1u < SFX_N ? stem_fx + 1u : stem_fx) : s < 0 ? (stem_fx ? stem_fx - 1u : 0u)
                                  : (stem_fx + 1u) % SFX_N);
        break;
    case MI_LAYOUT:                                    /* the layouts this build makes (ul_offered), the knob steps
                                                        * (stops at the ends), OCT+ round; main.c re-plugs USB once
                                                        * it settles */
        usb_layout = (uint8_t)ul_step(usb_layout, s);
        break;
    case MI_PFADE:                                     /* 0 .. 15, the knob stops at the ends; OCT+ back to 0 */
        song.g[G_FXFADE] = (int16_t)(s ? clamp(song.g[G_FXFADE] + s, 0, 15) : 0);
        break;
    case MI_XFADE: {                                   /* 1.5, 2.9, 5.8 ms (stops at the ends); OCT+ round */
        static const uint8_t ORD[TPX_N] = {TPX_64, TPX_128, TPX_256};
        uint32_t k = tp_xfl == TPX_64 ? 0u : tp_xfl == TPX_128 ? 1u : 2u;
        k = s > 0 ? (k < 2u ? k + 1u : k) : s < 0 ? (k ? k - 1u : 0u) : (k + 1u) % TPX_N;
        tp_xfl = ORD[k];
        break;
    }
    case MI_CURVE:                                     /* right LINEAR, left EQUAL; OCT+ toggles */
        tp_lin = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !tp_lin);
        break;
#if SMK_UAC_IN_PROBE
    case MI_VCARR: {                                   /* AUTO .. EXT R (stops at the ends); OCT+ back to AUTO. EXT L / R
                                                        * set that channel to CARRIER (the other one off it: OFF); the
                                                        * saws take any channel off CARRIER (OFF: no synth in the voice) */
        uint32_t v = mi_carr_now();
        v = s > 0 ? (v < 4u ? v + 1u : v) : s < 0 ? (v ? v - 1u : 0u) : 0u;
        if (v <= 2u) {
            voc_saws = (uint8_t)v;
            if (voc_inl % VR_N == VR_CARR)
                voc_inl = VR_OFF;
            if (voc_inr % VR_N == VR_CARR)
                voc_inr = VR_OFF;
        } else {
            uint8_t *on = v == 3u ? &voc_inl : &voc_inr, *off = v == 3u ? &voc_inr : &voc_inl;
            *on = VR_CARR;
            if (*off % VR_N == VR_CARR)
                *off = VR_OFF;
        }
        break;
    }
    case MI_VEATT:                                     /* 3 .. 30 ms (stops at the ends); OCT+ back to 3 ms */
    case MI_VEREL: {                                   /* 8 .. 100 ms (stops at the ends); OCT+ back to 8 ms */
        uint8_t *x = i == MI_VEATT ? &voc_eatt : &voc_erel;
        uint32_t m = i == MI_VEATT ? VOC_EATT_N : VOC_EREL_N, v = *x % m;
        *x = (uint8_t)(s > 0 ? (v + 1u < m ? v + 1u : v) : s < 0 ? (v ? v - 1u : 0u) : 0u);
        break;
    }
    case MI_VSETUP:                                    /* FULL .. VOC ALONE (stops at the ends); OCT+ back to FULL; at
                                                        * once (fx.c voc_setup_block: the tracks off after their release,
                                                        * the vocoder faded and restarted at its rate) */
        voc_setup = (uint8_t)(s > 0 ? (voc_setup + 1u < VSU_N ? voc_setup + 1u : voc_setup)
                                    : s < 0 ? (voc_setup ? voc_setup - 1u : 0u) : VSU_FULL);
        break;
    case MI_VINL:                                      /* TRACK 3, CARRIER, T1, T2, HARMONY, OFF (stops at the ends);
                                                        * OCT+ back to TRACK 3 */
    case MI_VINR: {
        static const uint8_t ORD[VR_N] = {VR_T3, VR_CARR, VR_T1, VR_T2, VR_HARM, VR_OFF};
        uint8_t *x = i == MI_VINL ? &voc_inl : &voc_inr;
        uint32_t r = *x % VR_N, k = 0;
        while (k + 1u < VR_N && ORD[k] != r)
            k++;
        k = s > 0 ? (k + 1u < VR_N ? k + 1u : k) : s < 0 ? (k ? k - 1u : 0u) : 0u;
        *x = ORD[k];
        break;
    }
    case MI_VTRL:                                      /* -18 .. +6 dB in 1 (stops at the ends); OCT+ back to 0 dB */
    case MI_VTRR: {
        int32_t v = s ? (int32_t)(voc_trim[i == MI_VTRR] % VOC_TRIM_N) + s : (int32_t)VOC_TRIM_0;
        voc_trim[i == MI_VTRR] = (uint8_t)(v < 0 ? 0 : v > (int32_t)VOC_TRIM_N - 1 ? (int32_t)VOC_TRIM_N - 1 : v);
        break;
    }
    case MI_VBRM:                                      /* right FOLLOW, left FIXED; OCT+ toggles */
        voc_brmode = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !voc_brmode);
        break;
    case MI_HON:                                       /* right ON, left OFF; OCT+ toggles (fx.c: T1's notes released,
                                                        * the harmoniser started afresh) */
        hm_set(s > 0 ? 1u : s < 0 ? 0u : !hm_on);
        if (hm_want)
            fb_set(4u);                                /* (on: the FADER BANK screen on E, HARM, when the menu closes) */
        break;
    case MI_HINT:                                      /* right SCALE, left FIXED; OCT+ toggles */
        hm_int = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !hm_int);
        break;
    case MI_HNOTE:                                     /* (a readout) */
        break;
    case MI_VGDEL: {                                   /* 0 .. 50 ms in 5 (stops at the ends); OCT+ back to 0 */
        int32_t v = s ? (int32_t)(voc_gdel % 11u) + s : 0;
        voc_gdel = (uint8_t)(v < 0 ? 0 : v > 10 ? 10 : v);
        break;
    }
    case MI_VCUT:                                      /* 0 .. 127, 4 a step; OCT+ back (open, no resonance) */
    case MI_VRES: {
        uint8_t *x = i == MI_VCUT ? &voc_cut : &voc_res;
        int32_t v = s ? (int32_t)*x + 4 * s : (i == MI_VCUT ? 127 : 0);
        *x = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
        break;
    }
    case MI_VVM:                                       /* AUTO, VOICED, UNVOICED (stops at the ends); OCT+ round */
        voc_vmode = (uint8_t)(s > 0 ? (voc_vmode + 1u < VV_N ? voc_vmode + 1u : voc_vmode)
                                    : s < 0 ? (voc_vmode ? voc_vmode - 1u : 0u) : (voc_vmode + 1u) % VV_N);
        break;
    case MI_VPINK:                                     /* right PINK, left WHITE; OCT+ toggles */
        voc_pink = (uint8_t)(s > 0 ? (voc_pink + 1u < VOC_NOISE_N ? voc_pink + 1u : voc_pink)   /* WHITE, PINK, SOFT */
                             : s < 0 ? (voc_pink ? voc_pink - 1u : 0u) : (voc_pink + 1u) % VOC_NOISE_N);
        break;
    case MI_VMWA:                                      /* VIBRATO, BRIDGE TILT, NOISE BLEND (stops at the ends); OCT+ round */
        voc_mwa = (uint8_t)(s > 0 ? (voc_mwa + 1u < VOC_MWA_N ? voc_mwa + 1u : voc_mwa)
                                  : s < 0 ? (voc_mwa ? voc_mwa - 1u : 0u) : (voc_mwa + 1u) % VOC_MWA_N);
        break;
    case MI_VAGC:                                      /* OFF, PEAKS, LIGHT, FULL (stops at the ends); OCT+ OFF */
        voc_agc = (uint8_t)(s > 0 ? (voc_agc + 1u < VOC_AGC_N ? voc_agc + 1u : voc_agc)
                                  : s < 0 ? (voc_agc ? voc_agc - 1u : 0u) : 0u);
        break;
    case MI_VBR:                                       /* OFF, -48 .. 0 dB in 3 dB (stops at the ends); OCT+ round */
        voc_bridge = (uint8_t)(s > 0 ? (voc_bridge + 1u < VOC_BR_N ? voc_bridge + 1u : voc_bridge)
                                     : s < 0 ? (voc_bridge ? voc_bridge - 1u : 0u) : (voc_bridge + 1u) % VOC_BR_N);
        break;
    case MI_VTHR:                                      /* -54 .. -24 dB in 6 dB steps (stops at the ends); OCT+ -42 */
        voc_thr = (uint8_t)(s > 0 ? (voc_thr + 1u < VOC_THR_N ? voc_thr + 1u : voc_thr)
                                  : s < 0 ? (voc_thr ? voc_thr - 1u : 0u) : 2u);
        break;
    case MI_VTILT:                                     /* -3 .. +3 dB an octave (stops at the ends); OCT+ +1.0 */
        voc_tilt = (uint8_t)(s > 0 ? (voc_tilt + 1u < VOC_TILT_N ? voc_tilt + 1u : voc_tilt)
                                   : s < 0 ? (voc_tilt ? voc_tilt - 1u : 0u) : VOC_TILT_FLAT + 2u);
        break;
    case MI_VCOL:                                      /* 0 .. 100 % (stops at the ends); OCT+ 100 % */
        voc_colour = (uint8_t)(s > 0 ? (voc_colour + 1u < VOC_COLOUR_N ? voc_colour + 1u : voc_colour)
                                     : s < 0 ? (voc_colour ? voc_colour - 1u : 0u) : VOC_COLOUR_N - 1u);
        break;
    case MI_VBLEND:                                    /* right FILL, left SUM; OCT+ toggles */
        voc_blend = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !voc_blend);
        break;
    case MI_VNORM: {                                   /* FLAT, NORMAL, EMPHASIS (stops at the ends); OCT+ round */
        uint32_t r = voc_norm + voc_emsel;
        r = s > 0 ? (r < 2u ? r + 1u : 2u) : s < 0 ? (r ? r - 1u : 0u) : (r + 1u) % 3u;
        voc_norm = (uint8_t)(r != 0u), voc_emsel = (uint8_t)(r == 2u);
        break;
    }
    case MI_VMODE:                                     /* AUDIO, VOCODER (stops at the ends); OCT+ round; faded */
        if (hm_on)
            break;                                     /* (HARMONY on: the vocoder off, MODE AUDIO, locked) */
        voc_mode = (uint8_t)(s > 0 ? (voc_mode + 1u < VM_N ? voc_mode + 1u : voc_mode) : s < 0 ? (voc_mode ? voc_mode - 1u : 0u)
                                   : (voc_mode + 1u) % VM_N);
        break;
    case MI_VLVL:                                      /* -12 .. +18 dB (stops at the ends); OCT+ back to 0 dB */
        voc_level = (uint8_t)(s > 0 ? (voc_level + 1u < VOC_LEVEL_N ? voc_level + 1u : voc_level)
                                    : s < 0 ? (voc_level ? voc_level - 1u : 0u) : 4u);
        break;
    case MI_VDRY:                                      /* OFF, -30 .. 0 dB (stops at the ends); OCT+ round */
    case MI_VHF: {
        uint8_t *x = i == MI_VDRY ? &voc_dry : &voc_hf;
        *x = (uint8_t)(s > 0 ? (*x + 1u < VOC_MIX_N ? *x + 1u : *x) : s < 0 ? (*x ? *x - 1u : 0u) : (*x + 1u) % VOC_MIX_N);
        break;
    }
#endif
#if SMK_UAC_IN_PROBE && !SMK_UAC_PROBE
    case MI_UIN:                                       /* OFF, ON, TEST (stops at the ends); OCT+ round; at once */
        uin_mode = (uint8_t)(s > 0 ? (uin_mode < 2u ? uin_mode + 1u : 2u) : s < 0 ? (uin_mode ? uin_mode - 1u : 0u)
                                   : (uin_mode + 1u) % 3u);
        break;
#endif
    case MI_FRET:                                      /* right CARRY, left SLIP; OCT+ toggles */
        sq_carry = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !sq_carry);
        break;
    case MI_SRATE:                                     /* right 44.1, left 48; OCT+ toggles; re-plugged as LAYOUT */
        stem_rate = (uint8_t)(s > 0 ? (stem_rate < 3u ? stem_rate + 1u : 3u)   /* 48, 44.1, 48 / 16, 44.1 / 16 */
                             : s < 0 ? (stem_rate ? stem_rate - 1u : 0u) : (stem_rate + 1u) % 4u);
        break;
    case MI_SPAIR:                                     /* RET, DRUMS, T1, T2, T3 (stops at the ends); OCT+ back to RET;
                                                        * at once (the DAW's channel names: at the next plug) */
        smk_pair = (uint8_t)(s > 0 ? (smk_pair + 1u < SP_N ? smk_pair + 1u : smk_pair) : s < 0 ? (smk_pair ? smk_pair - 1u : 0u)
                                   : SP_RET);
        break;
    case MI_SFILT:                                     /* right LIGHT, left FULL; OCT+ toggles; at once */
        stem_light = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !stem_light);
        break;
    case MI_LIMIT:                                     /* right CLASSIC, left SOFT; OCT+ toggles */
        lim_classic = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !lim_classic);
        break;
    case MI_KLAY:                                      /* right SMK, left FM-1; OCT+ toggles. The rows' knobs move with it */
        knob_layout = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !knob_layout);
        knob_layout_apply();
        ui.force = 1;
        break;
    case MI_MODE:                                      /* right DAW, left SYNTH; OCT+ toggles (the menu stays up) */
        daw_set(s > 0 ? 1u : s < 0 ? 0u : !daw_on, 0);
        break;
    case MI_DCH:                                       /* 1..16 (stops at the ends); OCT+ steps round */
        daw_ch = (uint8_t)(s > 0 ? (daw_ch < 16u ? daw_ch + 1u : 16u) : s < 0 ? (daw_ch > 1u ? daw_ch - 1u : 1u)
                                 : daw_ch % 16u + 1u);
        break;
    case MI_LOCAL:                                     /* right ON, left OFF; OCT+ toggles */
        daw_local = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !daw_local);
        break;
    case MI_DMAP:                                      /* right USER, left DEFAULT; OCT+ toggles */
        daw_user = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !daw_user);
        break;
    case MI_KREL:                                      /* right RELATIVE, left ABSOLUTE; OCT+ toggles */
        daw_rel = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !daw_rel);
        break;
    case MI_DEDIT:                                     /* OCT+: the map editor (DAW mode, CC MAP USER) */
        if (!s)
            daw_edit_open();
        break;
    case MI_DRESET:                                    /* OCT+ arms, OCT+ again within ~1.5 s: USER = DEFAULT */
        if (s)
            break;
        if (ui.arm != MI_ARM_RESET || !ui.arm_t) {
            ui.arm = MI_ARM_RESET;
            ui.arm_t = 90;
        } else {
            ui.arm = 0;
            daw_reset_user();
            ui_message("USER MAP RESET");
        }
        break;
    case MI_VEL:                                       /* DYNAMIC .. FIXED 127 (stops at the ends); OCT+ steps round */
        if (s > 0 && vel_mode + 1u < VEL_N)
            vel_mode++;
        else if (s < 0 && vel_mode > 0u)
            vel_mode--;
        else if (!s)
            vel_mode = (uint8_t)((vel_mode + 1u) % VEL_N);
        break;
#endif
    case MI_LIGHTS:
    case MI_KEYS: {                                    /* brighter / dimmer (stops at the ends); OCT+ steps round */
        uint8_t *v = i == MI_LIGHTS ? &lights_lvl : &lights_keys;
        uint32_t n = i == MI_LIGHTS ? LIGHTS_N : KEYS_N;
        if (s > 0 && *v + 1u < n)
            (*v)++;
        else if (s < 0 && *v > 0u)
            (*v)--;
        else if (!s)
            *v = (uint8_t)((*v + 1u) % n);
        if (i == MI_KEYS && lights_keys && !lights_lvl)
            lights_lvl = LIGHTS_LOW;                   /* keys lit need a level: the lowest */
        break;
    }
    case MI_LCLEAR:                                    /* (2.6) every learned CC: OCT+ */
        if (!s)
            ui_message(ml_clear_all() ? "LEARN CLEARED" : "NOTHING LEARNED");
        break;
    case MI_PANEL:                                     /* actions: OCT+ only */
        if (!s) {
            panel_setup();
            ui.force = 1;
        }
        break;
    case MI_ABOUT:
        if (!s) {
            ui.menu = 2;
            ui.force = 1;
        }
        break;
    default:
        break;
    }
}

/* menu: SELECT the section, KNOB 1..3 its settings, PRESETS the cursor, OCT+ ok, OCT- close (ABOUT -> the section) */
UI_SMALL static void menu_input(uint32_t pressed)
{
    int32_t s;
    uint32_t k, ok = (pressed >> panel.btn[B_OCTUP]) & 1u, back = (pressed >> panel.btn[B_OCTDN]) & 1u;
    uint32_t sel = ui.menu_sel % MI_COUNT, sec = mi_sec(sel), n = MS_FIRST[sec + 1u] - MS_FIRST[sec];
    if (back) {
        if (ui.menu == 2)
            ui.menu = 1, ui.force = 1;
        else
            menu_close();
        return;
    }
    if (ui.menu != 1) {
        enc_drop();
        return;
    }
    if ((s = panel_enc(EN_SELECT)) != 0) {             /* the section before / after (stops at the ends) */
        int32_t to = clamp((int32_t)sec + (s > 0 ? 1 : -1), 0, MS_COUNT - 1);
        if ((uint32_t)to != sec) {
            ui.menu_sel = MS_FIRST[to];
            ui.force = 1;
        }
        enc_drop();
        return;
    }
    if ((s = panel_enc(EN_PRESET)) != 0)               /* the cursor, round the section's rows */
        ui.menu_sel = (uint8_t)(MS_FIRST[sec] + (sel - MS_FIRST[sec] + (s > 0 ? 1u : n - 1u)) % n);
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0 && k < n) {
            ui.menu_sel = (uint8_t)(MS_FIRST[sec] + k);   /* (the cursor follows the knob turned) */
            mi_set(MS_FIRST[sec] + k, s);
        }
    if (ok)
        mi_set(ui.menu_sel % MI_COUNT, 0);
    enc_drop();                                        /* swallow the rest while the menu is up */
}
