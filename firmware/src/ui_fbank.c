/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* The FADER BANK screen: the four banks, the active one lit, what faders 1..4 set (track colours).
 * FADER BANK opens it and then steps the bank, as < > and SELECT (K2, or K6) do; PATCH closes it, or it closes by itself. */
static const char *const FB_NAME[5] = {"LEVEL", "FILTER", "REVERB", "DELAY", "HARM"};
/* (FB_N, the banks: ui_input.c; a test that includes this file alone: four) */
#ifndef FB_N
#define FB_N 4u
#endif
#define FB_SHOW_MS 4000u                           /* up this long after the last change */
static uint8_t fb_open;
/* (fb_pin: ui_input.c, beside smk_fbank) */
static uint32_t fb_t, fb_frame;                    /* last action (ms); the frame drawn last */

static int fb_shown(void)
{
    return fb_open && !ui.menu && ui.layer == LY_PLAY && !ui.hold_kind;
}
static void fb_set(uint32_t b)                     /* bank b, the screen up */
{
    smk_fbank = (uint8_t)(b % FB_N);
    smk_fbank_new = 1;
    fb_open = 1;
    fb_t = fm1_ms;
    ui.force = 1;
}
static void fb_close(void) { fb_open = 0, fb_pin = 0; }
/* a fader moved (main.c smk_faders): the screen up, or its time-out restarted, so what the fader set is seen */
static void fb_touch(void)
{
    if (!fb_open)
        fb_open = 1, ui.force = 1;
    fb_t = fm1_ms;
}
static void fb_expire(void)                        /* up FB_SHOW_MS after the last change */
{
    if (fb_open && !fb_pin && fm1_ms - fb_t > FB_SHOW_MS)
        fb_open = 0, ui.force = 1;
}
static int fb_left(void)                           /* drawn last frame, not now: the page needs a redraw */
{
    int l = fb_frame && fb_frame + 1u == ui.frame;
    if (l)
        fb_frame = 0;
    return l;
}

/* what fader i sets in bank b (main.c smk_faders) */
static const param_desc_t *fb_param(uint32_t b, uint32_t i, int16_t **vp)
{
    static const uint8_t TID[4] = {P_LEVEL, P_TFLT, P_REV, P_DLY}, GID[4] = {G_DRLVL, 0, G_DRREV, G_DRDLY};
#if SMK_UAC_IN_PROBE
    if (b == 4u) {                                 /* E: T1's column the harmoniser's VOICE, the others' HARM sends */
        static int16_t hv[4];
        static const param_desc_t HS = {"HARM", F_PCT, 0, 127, 0, 0, 0};
        hv[i & 3u] = (int16_t)hm_send[i & 3u];
        *vp = &hv[i & 3u];
        return &HS;
    }
    if (b == 1u && i == 0u && hm_on) {             /* HARMONY: T1's B is PITCH (the FILTER is off, fx.c), shown in
                                                    * semitones (fb_draw) */
        static const param_desc_t PIT = {"PITCH", F_BIPCT, -64, 63, 0, 0, 0};
        *vp = &trk[0].p[P_TFLT];
        return &PIT;
    }
    if (b == 1u && VOC_TRK_AUDIO(&trk[i]) && voc_mode % VM_N == VM_SAW) {   /* track 3 vocoding: CARRIER CUTOFF */
        static int16_t cut;
        static const param_desc_t CUT = {"CUTOFF", F_PCT, 0, 127, 127, 0, 0};
        cut = (int16_t)voc_cut;
        *vp = &cut;
        return &CUT;
    }
#endif
    if (i == TRK_DRUM && b != 1u) {
        *vp = &song.g[GID[b & 3u]];
        return &GP[GID[b & 3u]];
    }
    *vp = &trk[i].p[TID[b & 3u]];
    return &TP[TID[b & 3u]];
}

static void fb_draw(void)
{
    static uint32_t shown;
    uint32_t b = smk_fbank % FB_N, i, half, sig = b * 7919u, nb = FB_N;
    int16_t *vp;
    if (fb_frame + 1u != ui.frame) {               /* another screen was up: all of it */
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
    }
    fb_frame = ui.frame;
    for (i = 0; i < 4u; i++) {
        fb_param(b, i, &vp);
        sig = sig * 31u + (uint32_t)(*vp + 200);
    }
    sig = sig * 31u + nb;                          /* (the bank count and the labels, which change with HARMONY: E,
                                                    * T1 PITCH / FEEDBK; not only the values) */
#if SMK_UAC_IN_PROBE
    sig = sig * 31u + hm_on + (uint32_t)(voc_mode % VM_N) * 2u + (uint32_t)VOC_AUDIO_ON * 4u;
#endif
    if (!ui.force && sig == shown)
        return;
    shown = sig;
    for (half = 0; half < 2u; half++) {
        cv_begin(240, 120, C_BLACK);
        if (!half) {
            cv_text(4, 2, &FONT_L, "FADER BANK", C_WHITE);
            for (i = 0; i < nb; i++) {             /* the banks (E: 48 px each) */
                int32_t bw = (int32_t)(240u / nb), x = (int32_t)i * bw + 2, on = i == b;
                char l[2] = {(char)('A' + i), 0};
                cv_rect(x, 42, bw - 4, 74, on ? C_WHITE : TE_G2);
                if (!on)
                    cv_rect(x + 1, 43, bw - 6, 72, TE_G1);
                cv_text(x + (bw - 4) / 2 - text_w(&FONT_L, l) / 2, 48, &FONT_L, l, on ? C_BLACK : TE_G3);
                cv_text(x + (bw - 4) / 2 - text_w(&FONT_S, FB_NAME[i]) / 2, 90, &FONT_S, FB_NAME[i], on ? C_BLACK : TE_G3);
            }
        } else {
            for (i = 0; i < 4u; i++) {             /* the faders: F1 TRACK 1 ... value, a bar */
                int32_t y = 4 + (int32_t)i * 24, w;
                char lab[10] = "TRACK 1", val[12], v2[20];
                const char *unit = "";
                const param_desc_t *d = fb_param(b, i, &vp);
                char f[3] = {'F', (char)('1' + i), 0};
                if (i == TRK_DRUM)
                    str_cpy(lab, "DRUMS", sizeof lab);
                else
                    lab[6] = (char)('1' + i);
#if SMK_UAC_IN_PROBE
                if (b == 1u && VOC_TRK_AUDIO(&trk[i]) && voc_mode % VM_N == VM_SAW)
                    str_cpy(lab, "T3 CUTOFF", sizeof lab);
                if (b == 4u) {
                    static const char *const HL[4] = {"VOICE IN", "T2 SEND", "T3 SEND", "DRUM SEND"};
                    str_cpy(lab, HL[i & 3u], sizeof lab);
                }
                if (hm_on && i == 0u && (b == 1u || b == 3u))
                    str_cpy(lab, b == 1u ? "T1 PITCH" : "T1 FEEDBK", sizeof lab);
#endif
                param_format(d, *vp, val, &unit);
#if SMK_UAC_IN_PROBE
                if (hm_on && i == 0u && b == 1u) {     /* PITCH in semitones (smk_harm.h hm_ratios: -24 .. +12) */
                    int32_t f = *vp, t10 = f < 0 ? f * 240 / 64 : f * 120 / 63;
                    val[0] = t10 > 0 ? '+' : t10 < 0 ? '-' : ' ';
                    fmt_fix(val + 1, t10 < 0 ? -t10 : t10, 1);
                    unit = " ST";
                }
#endif
                str_cpy(v2, val, sizeof v2);
                str_cpy(v2 + str_len(v2), unit, sizeof v2 - str_len(v2));
                cv_text(4, y, &FONT_S, f, TE_COL[i]);
                cv_text(32, y, &FONT_S, lab, TE_COL[i]);
                cv_text(236 - text_w(&FONT_S, v2), y, &FONT_S, v2, C_WHITE);
                w = RATIO(d, *vp) * 204 / 1000;
                cv_rect(32, y + 17, 204, 3, TE_G1);
                cv_rect(32, y + 17, w < 1 ? 1 : w, 3, TE_COL[i]);
            }
            {
                char h[24] = "< >  K2  FADER BANK";
                h[6] = knob_digit(EN_SELECT);          /* (SELECT: K2, or K6, HOME menu > SCREEN > KNOB LAYOUT) */
                cv_text((240 - text_w(&FONT_S, h)) / 2, 100, &FONT_S, h, TE_G3);
            }
        }
        cv_blit(0, half * 120u);
    }
}
