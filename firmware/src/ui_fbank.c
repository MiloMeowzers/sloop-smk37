/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* The FADER BANK screen: the four banks, the active one lit, what faders 1..4 set (track colours).
 * FADER BANK opens it and then steps the bank, as < > and SELECT (K2, or K6) do; PATCH closes it, or it closes by itself. */
static const char *const FB_NAME[6] = {"LEVEL", "FILTER", "REVERB", "DELAY", "HARM", "LANE"};   /* (SEND: SENDS was wider than
                                                    * a sixth of the screen) */
static const param_desc_t FB_NONE = {"--", F_PCT, 0, 0, 0, 0, 0};   /* (a fader with nothing to set: min == max) */
/* (FB_N, the banks, and the pin and PICKUP state: ui_input.c; a test that includes this file alone: four banks, and
 * the state here) */
#ifndef SMK_FB_STATE
static uint8_t fb_pin, smk_fpos[4], smk_flive;
#endif
#ifndef FB_N
#define FB_N 4u
#endif
static uint8_t fb_open;
static uint8_t fb_page, fb_home;                   /* the page it came up over (another page closes it) */
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
    if (!fb_open)
        fb_page = ui.page, fb_home = ui.home;
    fb_open = 1;
    fb_t = fm1_ms;
    ui.force = 1;
}
static void fb_close(void) { fb_open = 0, fb_pin = 0; }
/* a fader moved (main.c smk_faders): the screen up (it stays until closed), so what the fader set is seen */
static void fb_touch(void)
{
    if (!fb_open)
        fb_open = 1, fb_page = ui.page, fb_home = ui.home, ui.force = 1;
    fb_t = fm1_ms;
}
/* no time-out (the owner, 2026-11-13: "out of keeping with other UI screen behaviours"): it stays until FADER BANK is
 * tapped again, or another screen comes: the menu, a layer, a hold to confirm, another page */
static void fb_expire(void)
{
    if (fb_open && (ui.menu || ui.layer != LY_PLAY || ui.hold_kind || ui.page != fb_page || ui.home != fb_home))
        fb_open = 0, fb_pin = 0, ui.force = 1;
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
    static int16_t zero, hv[4];
    if (b == 4u) {                                 /* E, HARM: PITCH (semitones, fb_draw), FEEDBACK, VOICE IN, WET / DRY */
        static const param_desc_t PIT = {"PITCH", F_BIPCT, -64, 63, 0, 0, 0}, PCT = {"HARM", F_PCT, 0, 127, 0, 0, 0};
        if (i == 0u) {
            *vp = &hm_pitch;
            return &PIT;
        }
        if (i == 2u) {
            hv[0] = (int16_t)hm_send[0];
            *vp = &hv[0];
        } else {
            *vp = i == 1u ? &hm_fb : &hm_wd;
        }
        return &PCT;
    }
    if (b == 5u) {                                 /* F, SENDS: T2, T3, the drums into the harmoniser (T1: it) */
        static const param_desc_t SND = {"SEND", F_PCT, 0, 127, 0, 0, 0};
        if (i == 0u) {
            *vp = &zero;
            return &FB_NONE;
        }
        hv[i & 3u] = (int16_t)hm_send[i & 3u];
        *vp = &hv[i & 3u];
        return &SND;
    }
    if ((b == 1u || b == 3u) && i == 0u && (hm_on || hm_want)) {   /* HARMONY: T1 has no FILTER, no DELAY SEND (E) */
        *vp = &zero;
        return &FB_NONE;
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

/* v (>= 0) with dec decimals and its leading zero: "0.5", "0.930" (libc.c fmt_fix writes ".5": its callers on the
 * FM-1 never pass less than 1, and it stays as it is there) */
static void fmt_fixz(char *b, uint32_t v, int dec)
{
    uint32_t p = 1;
    int k;
    for (k = 0; k < dec; k++)
        p *= 10u;
    if (v >= p) {
        fmt_fix(b, (int32_t)v, dec);
        return;
    }
    fmt_fix(b, (int32_t)(v + p), dec);                 /* ("1.5" for 0.5) */
    b[0] = '0';
}
/* fader i of bank b: its label (10) and its value with the unit (20), as this screen shows them (the HARMONY page's
 * header too: ui_hpage.c hp_fader); its descriptor, *vp its value */
static const param_desc_t *fb_text(uint32_t b, uint32_t i, char *lab, char *v2, int16_t **vp)
{
    char val[12];
    const char *unit = "";
    const param_desc_t *d = fb_param(b, i, vp);
    str_cpy(lab, i == TRK_DRUM ? "DRUMS" : "TRACK 1", 10);
    if (i != TRK_DRUM)
        lab[6] = (char)('1' + i);
#if SMK_UAC_IN_PROBE
    if (b == 1u && VOC_TRK_AUDIO(&trk[i]) && voc_mode % VM_N == VM_SAW)
        str_cpy(lab, "T3 CUTOFF", 10);
    if (b == 4u) {
        static const char *const HL[4] = {"PITCH", "FEEDBACK", "VOICE IN", "WET/DRY"};
        str_cpy(lab, HL[i & 3u], 10);
    }
    if (b == 5u) {
        static const char *const SL[4] = {"T1: HARM", "T2 LANE", "T3 LANE", "DRUM LANE"};   /* (Stage 1c: each a lane's
                                                         * input, 0 off) */
        str_cpy(lab, SL[i & 3u], 10);
    }
    if (d == &FB_NONE && b != 5u)
        str_cpy(lab, "T1: HARM", 10);
#endif
    param_format(d, **vp, val, &unit);
    if (d == &FB_NONE)
        str_cpy(val, "--", sizeof val), unit = "";
#if SMK_UAC_IN_PROBE
    if (b == 4u && i == 0u) {                          /* PITCH in semitones (smk_harm.h hm_ratios: -24 .. +12) */
        int32_t f = **vp, t10 = f < 0 ? f * 240 / 64 : f * 120 / 63;
        val[0] = t10 > 0 ? '+' : t10 < 0 ? '-' : ' ';
        fmt_fixz(val + 1, (uint32_t)(t10 < 0 ? -t10 : t10), 1);   /* ("0.0", not ".0") */
        unit = " ST";
    }
#endif
    str_cpy(v2, val, 20);
    str_cpy(v2 + str_len(v2), unit, 20 - str_len(v2));
    return d;
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
        sig = sig * 31u + smk_fpos[i];             /* (the faders' places: the ticks, the arrows) */
    }
    sig = sig * 31u + smk_flive;
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
            for (i = 0; i < nb; i++) {             /* the banks (six with HARMONY: 40 px each) */
                int32_t bw = (int32_t)(240u / nb), x = (int32_t)i * bw + 2, on = i == b, nw = text_w(&FONT_S, FB_NAME[i]);
                char l[2] = {(char)('A' + i), 0};
                cv_rect(x, 42, bw - 4, 74, on ? C_WHITE : TE_G2);
                if (!on)
                    cv_rect(x + 1, 43, bw - 6, 72, TE_G1);
                cv_text(x + (bw - 4) / 2 - text_w(&FONT_L, l) / 2, 48, &FONT_L, l, on ? C_BLACK : TE_G3);
                if (nw <= bw - 6 || on)                /* (a long name in a narrow box: the open bank's only) */
                    cv_text(x + (bw - 4) / 2 - nw / 2, 90, &FONT_S, FB_NAME[i], on ? C_BLACK : TE_G3);
            }
        } else {
            for (i = 0; i < 4u; i++) {             /* the faders: F1 TRACK 1 ... value, a bar */
                int32_t y = 4 + (int32_t)i * 24, w;
                char lab[10], v2[20];
                const param_desc_t *d = fb_text(b, i, lab, v2, &vp);
                char f[3] = {'F', (char)('1' + i), 0};
                {   /* PICKUP's box: live, filled in the track's colour; waiting, an amber outline and an arrow the way
                     * to move the fader (to the value the bank holds); nothing to set, dim */
                    int32_t none = d == &FB_NONE, live = (smk_flive >> i) & 1u, t = clamp(*vp - d->min, 0, 127),
                            up = smk_fpos[i] < t, k;
                    if (none) {
                        cv_rect(2, y - 1, 24, 18, TE_G1);
                        cv_text(4, y, &FONT_S, f, TE_G2);
                    } else if (live) {
                        cv_rect(2, y - 1, 24, 18, TE_COL[i]);
                        cv_text(4, y, &FONT_S, f, C_BLACK);
                    } else {
                        cv_rect(2, y - 1, 24, 1, C_AMB), cv_rect(2, y + 16, 24, 1, C_AMB);
                        cv_rect(2, y - 1, 1, 18, C_AMB), cv_rect(25, y - 1, 1, 18, C_AMB);
                        for (k = 0; k < 5; k++)        /* (a triangle, its point the way to go) */
                            cv_rect(14 - k, up ? y + 4 + k : y + 12 - k, 2 * k + 1, 1, C_AMB);
                    }
                    cv_text(32, y, &FONT_S, lab, none ? TE_G2 : TE_COL[i]);
                    cv_text(236 - text_w(&FONT_S, v2), y, &FONT_S, v2, none ? TE_G2 : C_WHITE);
                    w = none ? 0 : RATIO(d, *vp) * 204 / 1000;
                    cv_rect(32, y + 17, 204, 3, TE_G1);
                    if (!none) {
                        cv_rect(32, y + 17, w < 1 ? 1 : w, 3, live ? TE_COL[i] : TE_G3);
                        cv_rect(32 + (int32_t)smk_fpos[i] * 202 / 127, y + 14, 2, 9, C_WHITE);   /* (the fader) */
                    }
                }
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
