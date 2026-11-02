/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37, the vocoder build: the vocoder page (smk37-devtools vocoder-ui-spec.md §3e, §4, §5), in the visualiser's place
 * (its hooks: vis_on, vis_shown, vis_open, vis_draw, vis_select; ui_vis.c stays in the other builds). PATCH tapped on
 * TRACKS opens it; PATCH tapped on it goes on to PUNCH IN (the FX layer locked open, as FX + FADER BANK) and from there
 * back to TRACKS. It stays up until a layer, the menu (PATCH held), the picker or another page opens.
 *   < >          the groups: G1 = channels 1-7, G2 = 8-14, G3 = 15-20
 *   K1..K7       a channel's value for its own function (G3: K1..K6; K7 THRESHOLD)
 *   pad above    the channel's function, EMPHASIS -> ADDITION -> BRIDGING (ch 11-20: EMPHASIS <-> ADDITION)
 *   pad below    the channel's mute
 *   K8           G1 LEVEL (with the pad above held: the BYPASS level), G2 BRIDGING SUM, G3 ADDITION SUM
 *   G1 pads 8    above BYPASS OFF / REPLACEMENT / SPEECH (on release, if K8 was not turned), below VOCODER OFF / FLAT /
 *                NORMAL / EMPHASIS
 *   G2 pads 8    above AUTOCONTROL, below BRIDGING OFF / SUM / INDIVIDUAL
 *   G3 pads 7, 8 above VOICE MODE, (free); below PINK, ADDITION OFF / SUM / INDIVIDUAL
 * The masters and selectors are the menus' settings (one set of values, ui_menu.c mi_set / mi_value); the channels'
 * values (smk_vocoder.h vpg) are stored, saved and drawn, their DSP the next build's, as are REPLACEMENT, INDIVIDUAL,
 * EMPHASIS and AUTOCONTROL. The keys, PLAY, REC and OCT work as always; the pads play nothing here. */
#if !SMK_UAC_IN_PROBE
#error "ui_vpage.c: the vocoder build (SMK_UAC_IN_PROBE)"
#endif
#define VP_COL_E RGB(255, 150, 30)                  /* EMPHASIS */
#define VP_COL_A RGB(40, 190, 255)                  /* ADDITION */
#define VP_COL_B RGB(180, 100, 255)                 /* BRIDGING */
#define VP_DIM(c) ((uint16_t)(((c) >> 2) & 0x39E7u))   /* a quarter (RGB565): the functions not on the knob */
static const uint8_t VP_RGB[VF_N][3] = {{255, 150, 30}, {40, 190, 255}, {180, 100, 255}};   /* (the pads: the same) */
static const char *const VP_FN[VF_N] = {"EMPHASIS", "ADDITION", "BRIDGING"};
static const char *const VP_SEL[VS_N] = {"OFF", "SUM", "INDIVIDUAL"};
static const char *const VP_BYP[VB_N] = {"OFF", "REPLACEMENT", "SPEECH"};
static const char *const VP_VOC[4] = {"OFF", "FLAT", "NORMAL", "EMPHASIS"};
/* a position 1 .. 100 in tenths of a dB, 400 log10(p / 50): EMPHASIS as it is, ADDITION 120 under */
static const int16_t VP_DB[101] = {0, -680, -559, -489, -439, -400, -368, -342, -318, -298, -280, -263, -248, -234,
    -221, -209, -198, -187, -177, -168, -159, -151, -143, -135, -128, -120, -114, -107, -101, -95, -89, -83, -78, -72,
    -67, -62, -57, -52, -48, -43, -39, -34, -30, -26, -22, -18, -14, -11, -7, -4, 0, 3, 7, 10, 13, 17, 20, 23, 26, 29,
    32, 35, 37, 40, 43, 46, 48, 51, 53, 56, 58, 61, 63, 66, 68, 70, 73, 75, 77, 79, 82, 84, 86, 88, 90, 92, 94, 96, 98,
    100, 102, 104, 106, 108, 110, 112, 113, 115, 117, 119, 120};
static struct {
    uint8_t grp, msg_t, hold, hold_prev, bp_down, bp_used;
    char msg[28];
} vp;

static int vis_shown(void)
{
    return vis_on && !ui.home && !ui.menu && cur_page()->scope == SC_TRK && ui.layer == LY_PLAY && ly_lock == LY_PLAY &&
           !pick_shown() && !fb_shown();
}
static void vis_open(void)
{
    vis_on = 1;
    ui.force = 1;
}
static void vis_select(int32_t s)                   /* < > (and SELECT): the groups, round */
{
    vp.grp = (uint8_t)((vp.grp + (s > 0 ? 1u : 2u)) % 3u);
    vp.bp_down = 0;
    ui.force = 1;
}
static uint32_t vp_voc(void) { return voc_mode % VM_N == VM_OFF ? 0u : 1u + voc_norm + voc_emsel; }
static void vp_say(const char *a, const char *b, const char *c)   /* the readout above the bars, a second */
{
    char *m = vp.msg;
    str_cpy(m, a, sizeof vp.msg);
    if (b[0]) {
        str_cpy(m + str_len(m), "  ", sizeof vp.msg - str_len(m));
        str_cpy(m + str_len(m), b, sizeof vp.msg - str_len(m));
    }
    if (c[0]) {
        str_cpy(m + str_len(m), "  ", sizeof vp.msg - str_len(m));
        str_cpy(m + str_len(m), c, sizeof vp.msg - str_len(m));
    }
    vp.msg_t = 40;
    ui.force = 1;
}
static void vp_db(char *b, int32_t t)               /* tenths of a dB: "+4.2 DB" */
{
    b[0] = t < 0 ? '-' : '+';
    fmt_fix(b + 1, t < 0 ? -t : t, 1);
    str_cpy(b + str_len(b), " DB", 4);
}
static void vp_val(char *b, uint32_t ch, uint32_t f)   /* channel ch's value for function f */
{
    uint32_t v = f == VF_BRG ? vpg.brg[ch / 2u] : f == VF_ADD ? vpg.add[ch] : vpg.emph[ch];
    if (f == VF_BRG) {
        fmt_int(b, (int32_t)v);
        str_cpy(b + str_len(b), " %", 3);
    } else if (!v) {
        str_cpy(b, "OFF", 4);
    } else {
        vp_db(b, VP_DB[v] - (f == VF_ADD ? 120 : 0));
    }
}
static void vp_chname(char *b, uint32_t ch, uint32_t f)
{
    if (f == VF_BRG) {                                  /* "BRIDGE 7/8" */
        str_cpy(b, "BRIDGE ", 8);
        fmt_int(b + 7, (int32_t)(ch & ~1u) + 1);
        str_cpy(b + str_len(b), "/", 2);
        fmt_int(b + str_len(b), (int32_t)(ch | 1u) + 1);
    } else {
        str_cpy(b, "CH ", 4);
        fmt_int(b + 3, (int32_t)ch + 1);
    }
}
/* the knob printed k (0 = K1): its turn this pass, whatever role it has (smk_knobs.h); the volume knob too (the page
 * holds it: smk_master_hold) */
static int32_t vp_knob(uint32_t k)
{
    uint32_t r;
    for (r = 0; r < NE && knob_of(r) != k; r++)
        ;
    return r < NE ? panel_enc(r) : fm1_enc_take(smk_master_enc % FM1_NENC) * smk_master_dir;
}
static void vp_master(uint32_t mi, const char *name, int32_t s)
{
    uint16_t c;
    mi_set(mi, s > 0 ? 1 : -1);
    vp_say(name, mi_value(mi, &c), "");
}
static void vp_turn(uint32_t k, int32_t s)          /* knob k of the group turned s */
{
    uint32_t g = vp.grp, nch = g < 2u ? 7u : 6u;
    char a[16], b[12];
    if (k < nch) {
        uint32_t ch = g * 7u + k, f = vpg.fn[ch] % VF_N;
        uint8_t *v = f == VF_BRG ? &vpg.brg[ch / 2u] : f == VF_ADD ? &vpg.add[ch] : &vpg.emph[ch];
        int32_t n = (int32_t)*v + s * (s > 1 || s < -1 ? 3 : 1);
        *v = (uint8_t)(n < 0 ? 0 : n > 100 ? 100 : n);
        vp_chname(a, ch, f);
        vp_val(b, ch, f);
        vp_say(a, f == VF_BRG ? "" : f == VF_ADD ? "ADD" : "EMPH", b);
    } else if (g == 0u && k == 7u) {
        if (smk_pick_down & 1u << 7) {                  /* the BYPASS pad held: its level */
            uint16_t c;
            vp.bp_used = 1;
            mi_set(MI_VDRY, s > 0 ? 1 : -1);
            vp_say("BYPASS", VP_BYP[vpg.byp % VB_N], mi_value(MI_VDRY, &c));
        } else {
            vp_master(MI_VLVL, "LEVEL", s);
        }
    } else if (g == 1u && k == 7u) {
        vp_master(MI_VBR, "BRIDGING SUM", s);
    } else if (k == 6u) {
        vp_master(MI_VTHR, "THRESHOLD", s);
    } else {
        vp_master(MI_VHF, "ADDITION SUM", s);
    }
}
static void vp_pad(uint32_t p)                      /* pad p hit (reading order: 0..7 the top row) */
{
    uint32_t g = vp.grp, c = p & 7u, top = p < 8u, nch = g < 2u ? 7u : 6u;
    uint16_t col;
    char a[16];
    if (c < nch) {
        uint32_t ch = g * 7u + c;
        vp_chname(a, ch, VF_EMPH);
        if (top) {
            vpg.fn[ch] = (uint8_t)((vpg.fn[ch] + 1u) % (ch < VOC_PAIRS ? VF_N : VF_BRG));
            vp_say(a, VP_FN[vpg.fn[ch] % VF_N], "");
        } else {
            vpg.mute ^= 1u << ch;
            vp_say(a, (vpg.mute >> ch) & 1u ? "MUTED" : "ON", "");
        }
        return;
    }
    if (g == 0u) {
        if (top) {                                      /* BYPASS: on its release (held, K8 is its level) */
            vp.bp_down = 1, vp.bp_used = 0;
        } else {
            uint32_t v = (vp_voc() + 1u) % 4u;
            if (!v)
                mi_set(MI_VMODE, -1);
            else if (voc_mode % VM_N == VM_OFF)
                mi_set(MI_VMODE, 1);
            voc_norm = (uint8_t)(v >= 2u), voc_emsel = (uint8_t)(v == 3u);
            vp_say("VOCODER", VP_VOC[v], "");
        }
    } else if (g == 1u) {
        if (top) {
            vpg.actl = (uint8_t)!vpg.actl;
            vp_say("AUTOCONTROL", vpg.actl ? "ON" : "OFF", "");
        } else {
            vpg.brsel = (uint8_t)((vpg.brsel + 1u) % VS_N);
            vp_say("BRIDGING", VP_SEL[vpg.brsel], "");
        }
    } else if (c == 6u) {
        mi_set(top ? MI_VVM : MI_VPINK, 0);
        vp_say(top ? "VOICE MODE" : "NOISE", mi_value(top ? MI_VVM : MI_VPINK, &col), "");
    } else if (!top) {
        vpg.addsel = (uint8_t)((vpg.addsel + 1u) % VS_N);
        vp_say("ADDITION", VP_SEL[vpg.addsel], "");
    }
}
/* once a pass, after the knobs are readable again (ui_input.c): closes the page when something else opened, else takes
 * PATCH (on to PUNCH IN), < > are taken where they were (vis_select), the knobs and the pads hit (vp_hits) */
static void vp_input(uint32_t *pressed, uint32_t *home)
{
    uint32_t k, h = vp_hits;
    vp_hits = 0;
    (void)pressed;
    if (vis_on && !vis_shown())
        vis_on = 0;                                     /* (another layer / page: the page goes, as a latched one) */
    if (!vis_shown()) {
        if (vp.hold)
            smk_master_hold = vp.hold_prev, vp.hold = 0;
        return;
    }
    if (!vp.hold)
        vp.hold_prev = smk_master_hold, vp.hold = 1;
    smk_master_hold = 1;                                /* (the volume knob is a K here) */
    if (*home == BT_TAP) {                              /* PATCH: on to PUNCH IN (PATCH there: TRACKS, layer_unlock) */
        *home = BT_NONE;
        vis_on = 0;
        smk_master_hold = vp.hold_prev, vp.hold = 0;
        ly_lock = LY_FX;
        ui.layer = LY_FX;
        ui.force = 1;
        return;
    }
    for (k = 0; k < 8u; k++) {
        int32_t s = vp_knob(k);
        if (s)
            vp_turn(k, s);
    }
    for (k = 0; h && k < 16u; k++)
        if ((h >> k) & 1u)
            vp_pad(k);
    if (vp.bp_down && !(smk_pick_down & 1u << 7)) {    /* the BYPASS pad let go: a tap steps it */
        vp.bp_down = 0;
        if (!vp.bp_used && vp.grp == 0u) {
            vpg.byp = (uint8_t)((vpg.byp + 1u) % VB_N);
            vp_say("BYPASS", VP_BYP[vpg.byp], "");
        }
    }
}
static void vp_padrgb(uint32_t p, const uint8_t *c, uint32_t sh)   /* a colour, >> sh */
{
    padled_set(p, (uint8_t)(c[0] >> sh), (uint8_t)(c[1] >> sh), (uint8_t)(c[2] >> sh));
}
static void vp_leds(void)                           /* the group's pads: functions and mutes, the selectors lit when on */
{
    static const uint8_t W[3] = {150, 150, 150}, G[3] = {40, 220, 90}, P[3] = {255, 90, 160};
    uint32_t g = vp.grp, c, nch = g < 2u ? 7u : 6u;
    for (c = 0; c < 8u; c++) {
        if (c < nch) {
            uint32_t ch = g * 7u + c;
            vp_padrgb(c, VP_RGB[vpg.fn[ch] % VF_N], (vpg.mute >> ch) & 1u ? 3u : 1u);
            vp_padrgb(8u + c, W, (vpg.mute >> ch) & 1u ? 5u : 1u);
        } else if (g == 0u) {
            vp_padrgb(c, W, vpg.byp == VB_OFF ? 4u : vpg.byp == VB_REPL ? 2u : 1u);
            vp_padrgb(8u + c, VP_RGB[VF_EMPH], vp_voc() ? 3u - (vp_voc() > 2u ? 2u : vp_voc() - 1u) : 5u);
        } else if (g == 1u) {
            vp_padrgb(c, W, vpg.actl ? 1u : 5u);
            vp_padrgb(8u + c, VP_RGB[VF_BRG], vpg.brsel == VS_OFF ? 5u : vpg.brsel == VS_SUM ? 2u : 1u);
        } else if (c == 6u) {
            vp_padrgb(c, G, voc_vmode % VV_N == VV_AUTO ? 1u : 2u);
            vp_padrgb(8u + c, P, voc_pink ? 1u : 5u);
        } else {
            padled_set(c, 0, 0, 0);
            vp_padrgb(8u + c, VP_RGB[VF_ADD], vpg.addsel == VS_OFF ? 5u : vpg.addsel == VS_SUM ? 2u : 1u);
        }
    }
}
static void vp_nodb(char *d, const char *s, uint32_t max)   /* a menu value without its " DB" (the strip is narrow) */
{
    uint32_t n;
    str_cpy(d, s, max);
    n = str_len(d);
    if (n > 3u && d[n - 3u] == ' ' && d[n - 2u] == 'D' && d[n - 1u] == 'B')
        d[n - 3u] = 0;
}
static void vp_strip(int32_t x, int32_t y, const char *lab, const char *a, const char *b, uint16_t col)
{
    char v[16];
    x = cv_text(x, y, &FONT_S, lab, TE_G3) + 4;
    x = cv_text(x, y, &FONT_S, a, col) + 4;
    if (b[0]) {
        vp_nodb(v, b, sizeof v);
        cv_text(x, y, &FONT_S, v, col);
    }
}
static void vp_paint(void)                          /* the whole page; the canvas band shows its part (cv_oy) */
{
    uint32_t ch, f;
    uint16_t c;
    char b[24];
    /* the header: the page and the group, or the readout */
    if (vp.msg_t || ui.msg_t) {
        const char *m = ui.msg_t ? ui.msg : vp.msg;
        cv_text(120 - text_w(&FONT_S, m) / 2, 2, &FONT_S, m, C_WHITE);
    } else {
        static const char *const GN[3] = {"G1  CH 1-7", "G2  CH 8-14", "G3  CH 15-20"};
        cv_text(4, 2, &FONT_S, "VOCODER", C_HI);
        if (vpg.actl)
            cv_text(116 - text_w(&FONT_S, "AUTO") / 2, 2, &FONT_S, "AUTO", TE_G3);
        cv_text(236 - text_w(&FONT_S, GN[vp.grp % 3u]), 2, &FONT_S, GN[vp.grp % 3u], TE_G4);
    }
    cv_rect(0, 21, 240, 1, TE_G1);
    /* the channels: emphasis and addition bars (the function's bright, the other dim; muted grey), the active group on
     * tiles; the bridging below, one bar a pair across both its channels */
    for (ch = 0; ch < VOC_CH; ch++) {
        int32_t x = (int32_t)ch * 12, mu = (int32_t)((vpg.mute >> ch) & 1u);
        uint32_t fn = vpg.fn[ch] % VF_N;
        if (ch / 7u == vp.grp)
            cv_rect(x, 25, 12, 128, TE_G1);
        for (f = 0; f < 2u; f++) {
            int32_t h = (int32_t)(f ? vpg.add[ch] : vpg.emph[ch]) * 120 / 100;
            uint16_t col = mu ? (fn == f ? TE_G3 : TE_G2) : fn == f ? (f ? VP_COL_A : VP_COL_E)
                                                                   : VP_DIM(f ? VP_COL_A : VP_COL_E);
            cv_rect(x + 2 + (int32_t)f * 4, 150 - h, 3, h ? h : 1, col);
        }
    }
    for (ch = 0; ch < VOC_PAIRS; ch++) {
        int32_t x = (int32_t)ch * 24, h = (int32_t)vpg.brg[ch] * 28 / 100;
        uint32_t lit = vpg.fn[2u * ch] % VF_N == VF_BRG || vpg.fn[2u * ch + 1u] % VF_N == VF_BRG;
        uint32_t mu = ((vpg.mute >> (2u * ch)) & 3u) == 3u;
        cv_rect(x + 3, 186 - h, 18, h ? h : 1, mu ? TE_G2 : lit ? VP_COL_B : VP_DIM(VP_COL_B));
    }
    cv_rect(0, 189, 240, 1, TE_G1);
    /* the masters and selectors (the menus' values) */
    vp_strip(4, 192, "VOC", VP_VOC[vp_voc()], "", vp_voc() ? C_HI : C_DIM);
    vp_strip(124, 192, "LEVEL", mi_value(MI_VLVL, &c), "", C_HI);
    vp_strip(4, 208, "BYP", vpg.byp == VB_SPEECH ? "SPEECH" : vpg.byp == VB_REPL ? "REPL" : "OFF",
             vpg.byp != VB_OFF ? mi_value(MI_VDRY, &c) : "", vpg.byp != VB_OFF ? C_HI : C_DIM);
    vp_strip(124, 208, "BRG", VP_SEL[vpg.brsel % VS_N] [0] == 'I' ? "IND" : VP_SEL[vpg.brsel % VS_N],
             vpg.brsel != VS_OFF ? mi_value(MI_VBR, &c) : "", vpg.brsel != VS_OFF ? C_HI : C_DIM);
    str_cpy(b, mi_value(MI_VVM, &c), 10);
    str_cpy(b + str_len(b), voc_pink ? " PINK" : "", 6);
    vp_strip(4, 224, "THR", mi_value(MI_VTHR, &c), b, C_HI);
    vp_strip(124, 224, "ADD", VP_SEL[vpg.addsel % VS_N][0] == 'I' ? "IND" : VP_SEL[vpg.addsel % VS_N],
             vpg.addsel != VS_OFF ? mi_value(MI_VHF, &c) : "", vpg.addsel != VS_OFF ? C_HI : C_DIM);
}
/* every other frame, in two bands of 120 rows (as the visualiser was) */
static void vis_draw(void)
{
    uint32_t pass;
    if (!vis_shown_last || ui.force) {
        vis_shown_last = 1;
        lcd_fill(0, 0, 240, 240, C_BLACK);
    } else if (ui.frame & 1u) {
        return;
    }
    for (pass = 0; pass < 2u; pass++) {
        cv_begin(240, 120, C_BLACK);
        cv_oy = -(int32_t)(pass * 120u);
        vp_paint();
        cv_oy = 0;
        cv_blit(0, pass * 120u);
    }
    if (vp.msg_t)
        vp.msg_t--;
}
