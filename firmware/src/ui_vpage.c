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
    uint8_t grp, msg_t, hold, hold_prev, bp_used;
    uint16_t pend, fired;                           /* the deferred pads down, those whose hold has acted */
    uint32_t t0[16];                                /* .. when each went down (ms) */
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
    vp.pend = 0;                                    /* (a pad held over the change: nothing) */
}
static uint32_t vp_voc(void) { return voc_mode % VM_N == VM_OFF ? 0u : 1u + voc_norm + voc_emsel; }
static void vp_say(const char *a, const char *b, const char *c)   /* the readout above the bars, a second */
{
    char *m = vp.msg;
    str_cpy(m, a, sizeof vp.msg);
    if (b[0]) {
        str_cpy(m + str_len(m), " ", sizeof vp.msg - str_len(m));
        str_cpy(m + str_len(m), b, sizeof vp.msg - str_len(m));
    }
    if (c[0]) {
        str_cpy(m + str_len(m), " ", sizeof vp.msg - str_len(m));
        str_cpy(m + str_len(m), c, sizeof vp.msg - str_len(m));
    }
    if (str_len(m) > 3u && str_eq(m + str_len(m) - 3u, " DB"))
        m[str_len(m) - 3u] = 0;                         /* (the header holds 15: no unit) */
    vp.msg_t = 40;
}
static void vp_db(char *b, int32_t t)               /* tenths of a dB: "+4.2" */
{
    b[0] = t < 0 ? '-' : '+';
    fmt_fix(b + 1, t < 0 ? -t : t, 1);
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
        vp_say(a, f == VF_BRG ? "" : f == VF_ADD ? "ADD" : "EMP", b);
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
        vp_master(MI_VBR, "BRIDGE", s);
    } else if (k == 6u) {
        vp_master(MI_VTHR, "THRESH", s);
    } else {
        vp_master(MI_VHF, "ADD SUM", s);
    }
}
/* the pads that act on release, so a hold can do something else: a channel's function pad (held: the channel back to
 * basic), G1's BYPASS pad (held: K8 its level) and VOCODER pad (held: every channel back to basic) */
#define VP_LONG_MS 600u
static int vp_deferred(uint32_t p)
{
    uint32_t c = p & 7u, nch = vp.grp < 2u ? 7u : 6u;
    return (p < 8u && c < nch) || (vp.grp == 0u && c == 7u);
}
static void vp_basic(uint32_t ch)                   /* a channel back to the basic setting (its mute stays) */
{
    vpg.emph[ch] = 50, vpg.add[ch] = 50;
    if (ch < 2u * VOC_PAIRS)
        vpg.brg[ch / 2u] = 0;
}
static void vp_tap(uint32_t p)                      /* a deferred pad let go before VP_LONG_MS */
{
    uint32_t c = p & 7u;
    char a[16];
    if (p < 8u && c < (vp.grp < 2u ? 7u : 6u)) {
        uint32_t ch = vp.grp * 7u + c;
        vpg.fn[ch] = (uint8_t)((vpg.fn[ch] + 1u) % (ch < VOC_PAIRS ? VF_N : VF_BRG));
        vp_chname(a, ch, VF_EMPH);
        vp_say(a, VP_FN[vpg.fn[ch] % VF_N], "");
    } else if (p == 7u) {
        if (!vp.bp_used) {
            vpg.byp = (uint8_t)((vpg.byp + 1u) % VB_N);
            vp_say("BYPASS", VP_BYP[vpg.byp], "");
        }
    } else {
        uint32_t v = (vp_voc() + 1u) % 4u;
        if (!v)
            mi_set(MI_VMODE, -1);
        else if (voc_mode % VM_N == VM_OFF)
            mi_set(MI_VMODE, 1);
        voc_norm = (uint8_t)(v >= 2u), voc_emsel = (uint8_t)(v == 3u);
        vp_say("VOCODER", VP_VOC[v], "");
    }
}
static void vp_long(uint32_t p)                     /* a deferred pad held VP_LONG_MS */
{
    uint32_t c = p & 7u, ch;
    char a[16];
    if (p < 8u && c < (vp.grp < 2u ? 7u : 6u)) {
        ch = vp.grp * 7u + c;
        vp_basic(ch);
        vp_chname(a, ch, VF_EMPH);
        vp_say(a, "BASIC", "");
    } else if (p == 15u) {
        for (ch = 0; ch < VOC_CH; ch++)
            vp_basic(ch);
        vpg.mute = 0;
        vp_say("ALL BASIC", "", "");
    }
}
static void vp_pad(uint32_t p)                      /* pad p hit (reading order: 0..7 the top row) */
{
    uint32_t g = vp.grp, c = p & 7u, top = p < 8u, nch = g < 2u ? 7u : 6u;
    uint16_t col;
    char a[16];
    if (vp_deferred(p)) {
        vp.pend |= (uint16_t)(1u << p), vp.fired &= (uint16_t)~(1u << p), vp.t0[p] = fm1_ms;
        if (p == 7u)
            vp.bp_used = 0;
        return;
    }
    if (c < nch) {                                      /* (the pad below: the mute) */
        uint32_t ch = g * 7u + c;
        vp_chname(a, ch, VF_EMPH);
        vpg.mute ^= 1u << ch;
        vp_say(a, (vpg.mute >> ch) & 1u ? "MUTED" : "ON", "");
        return;
    }
    if (g == 1u) {
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
    enc_hold |= (1u << NE) - 1u;                       /* (turns arriving later this pass reach no other knob
                                                     * code: PRESETS, ALGORITHM, SELECT jumped out, or changed group) */
    for (k = 0; h && k < 16u; k++)
        if ((h >> k) & 1u)
            vp_pad(k);
    for (k = 0; vp.pend && k < 16u; k++) {             /* the deferred pads: let go (a tap), or held long */
        uint32_t bit = 1u << k;
        if (!(vp.pend & bit))
            continue;
        if (!(smk_pick_down & bit)) {
            vp.pend &= (uint16_t)~bit;
            if (!(vp.fired & bit))
                vp_tap(k);
        } else if (!(vp.fired & bit) && fm1_ms - vp.t0[k] >= VP_LONG_MS) {
            vp.fired |= (uint16_t)bit;
            vp_long(k);
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
/* a cluster's value in 3 characters (FONT_S: 8 px each, a column 30): "+12", "-4", "OFF", "60%", "100" */
static void vp_short(char *b, uint32_t ch, uint32_t f)
{
    uint32_t v = f == VF_BRG ? vpg.brg[ch / 2u] : f == VF_ADD ? vpg.add[ch] : vpg.emph[ch];
    if (f == VF_BRG) {
        fmt_int(b, (int32_t)v);
        if (v < 100u)
            str_cpy(b + str_len(b), "%", 2);
    } else if (!v) {
        str_cpy(b, "OFF", 4);
    } else {
        int32_t t = VP_DB[v] - (f == VF_ADD ? 120 : 0), d = (t < 0 ? -t + 5 : t + 5) / 10;
        b[0] = t < 0 && d ? '-' : '+';
        fmt_int(b + 1, d);
    }
}
/* the screen: the header (FONT_L: the group, or the readout of what was touched), a column a knob (K1..K8, 30 px:
 * the channels' clusters, the masters' bars), the strip (the group's masters and selectors, by knob or pad) */
#define VP_COLW 30
#define VP_Y0 35                                    /* the clusters: the number, the mute light, the bars, the value */
#define VP_BT 60                                    /* .. the bars' top, bottom, middle (EMPHASIS, ADDITION: from it) */
#define VP_BB 150
#define VP_BM ((VP_BT + VP_BB) / 2)
#define VP_Y1 180                                   /* the strip: three rows of 20 */
static void vp_frame(int32_t x, uint32_t col)       /* a column's background: alternate shades, framed */
{
    int32_t h = VP_Y1 - 4 - VP_Y0;
    cv_rect(x + 1, VP_Y0, VP_COLW - 2, h, col & 1u ? TE_G1 : RGB(14, 14, 18));
    cv_rect(x + 1, VP_Y0, VP_COLW - 2, 1, TE_G2);
    cv_rect(x + 1, VP_Y0 + h - 1, VP_COLW - 2, 1, TE_G2);
    cv_rect(x + 1, VP_Y0, 1, h, TE_G2);
    cv_rect(x + VP_COLW - 2, VP_Y0, 1, h, TE_G2);
}
static void vp_ctext(int32_t x, int32_t y, const char *t, uint16_t c)   /* centred in a column */
{
    cv_text(x + (VP_COLW - text_w(&FONT_S, t)) / 2, y, &FONT_S, t, c);
}
static void vp_bar(int32_t x, int32_t w, uint32_t v, int mid, uint16_t c)   /* v 0..100: from the middle or the bottom */
{
    if (mid) {
        int32_t h = ((int32_t)v - 50) * (VP_BM - VP_BT) / 50;
        if (!v)
            return;                                 /* (OFF: nothing) */
        if (h >= 0)
            cv_rect(x, VP_BM - h, w, h ? h : 1, c);
        else
            cv_rect(x, VP_BM, w, -h, c);
    } else {
        int32_t h = (int32_t)v * (VP_BB - VP_BT) / 100;
        cv_rect(x, VP_BB - h, w, h ? h : 1, c);
    }
}
/* a channel's narrow fader, always shown: its track the full height (dark), the level filled from the middle (EMPHASIS,
 * ADDITION: the basic setting) or the bottom (BRIDGING), a cap at the level (OFF: at the bottom, grey) */
static void vp_fader(int32_t x, int32_t w, uint32_t v, int mid, uint16_t fill, uint16_t cap)
{
    int32_t y;
    cv_rect(x, VP_BT, w, VP_BB - VP_BT, (uint16_t)((fill >> 2) & 0x39E7u));
    if (mid && !v) {
        cv_rect(x, VP_BB - 2, w, 2, TE_G3);
        return;
    }
    vp_bar(x, w, v, mid, fill);
    y = mid ? VP_BM - ((int32_t)v - 50) * (VP_BM - VP_BT) / 50 : VP_BB - (int32_t)v * (VP_BB - VP_BT) / 100;
    cv_rect(x - 1, y > VP_BB - 2 ? VP_BB - 2 : y - 1, w + 2, 2, cap);
}
static void vp_cluster(uint32_t col, uint32_t ch)   /* channel ch in column col */
{
    int32_t x = (int32_t)col * VP_COLW, mu = (int32_t)((vpg.mute >> ch) & 1u);
    uint32_t fn = vpg.fn[ch] % VF_N, nb = ch < VOC_PAIRS ? 3u : 2u, f;
    int32_t bw = nb == 3u ? 5 : 7, gap = nb == 3u ? 3 : 5, x0 = x + (nb == 3u ? 4 : 5);
    char b[8];
    vp_frame(x, col);
    fmt_int(b, (int32_t)ch + 1);
    vp_ctext(x, VP_Y0 + 2, b, mu ? TE_G3 : TE_G4);
    cv_rect(x + 6, VP_Y0 + 20, VP_COLW - 12, 3, mu ? RGB(255, 40, 40) : TE_G2);   /* the mute light: red, muted */
    for (f = 0; f < nb; f++) {
        uint16_t c = f == VF_EMPH ? VP_COL_E : f == VF_ADD ? VP_COL_A : VP_COL_B, fc = c;
        uint32_t v = f == VF_BRG ? vpg.brg[ch / 2u] : f == VF_ADD ? vpg.add[ch] : vpg.emph[ch];
        if (mu)
            fc = c = f == fn ? TE_G3 : TE_G2;
        else if (f != fn)
            fc = (uint16_t)((c >> 1) & 0x7BEFu);    /* (the others half: the three still compare) */
        vp_fader(x0 + (int32_t)f * (bw + gap), bw, v, f != VF_BRG, fc, f == fn || mu ? c : fc);
    }
    vp_short(b, ch, fn);
    vp_ctext(x, VP_BB + 4, b, mu ? TE_G3 : fn == VF_EMPH ? VP_COL_E : fn == VF_ADD ? VP_COL_A : VP_COL_B);
}
static void vp_mcol(uint32_t col, const char *name, uint32_t mi, uint32_t pos, uint32_t n, uint16_t c)   /* a master */
{
    int32_t x = (int32_t)col * VP_COLW;
    uint16_t dc;
    char b[10];
    vp_frame(x, col);
    vp_ctext(x, VP_Y0 + 2, name, c);
    cv_rect(x + 9, VP_BT, VP_COLW - 18, VP_BB - VP_BT, TE_G2);
    vp_bar(x + 9, VP_COLW - 18, n > 1u ? pos * 100u / (n - 1u) : 0u, 0, c);
    vp_nodb(b, mi_value(mi, &dc), sizeof b);
    vp_ctext(x, VP_BB + 4, b, c);
}
/* the strip: the knobs on the left ("K8" and the name: the value is in the knob's column above), the pads on the right
 * (the pad's colour with a white mark on the side it sits, above / below; the name in its colour; the value) */
static void vp_item(uint32_t slot, int knob, uint32_t num, int below, uint16_t c, const char *name, const char *val)
{
    int32_t x = slot >= 3u ? 100 : 2, y = VP_Y1 + (int32_t)(slot % 3u) * 20;
    char b[12];
    if (knob) {
        char k[3] = {'K', (char)('0' + num), 0};
        cv_rect(x, y, 19, 17, TE_G2);
        cv_text(x + 2, y, &FONT_S, k, C_WHITE);
        cv_text(x + 23, y, &FONT_S, name, c);
        return;
    }
    cv_rect(x + 1, y + 2, 13, 13, c);
    cv_rect(x + 1, below ? y + 13 : y + 2, 13, 2, C_WHITE);
    x = cv_text(x + 18, y, &FONT_S, name, c) + 6;
    vp_nodb(b, val, sizeof b);
    cv_text(x, y, &FONT_S, b, C_HI);
}
static void vp_paint(void)                          /* the whole page; the canvas band shows its part (cv_oy) */
{
    static const char *const GN[3] = {"G1  CH 1-7", "G2  CH 8-14", "G3  CH 15-20"};
    static const char *const VOC4[4] = {"OFF", "FLAT", "NORMAL", "EMPH"};
    static const char *const SEL3[VS_N] = {"OFF", "SUM", "IND"};
    static const char *const BYP3[VB_N] = {"OFF", "REPL", "SPCH"};
    uint32_t g = vp.grp % 3u, nch = g < 2u ? 7u : 6u, k;
    uint16_t c, W = RGB(220, 220, 220);
    const char *m = ui.msg_t ? ui.msg : vp.msg_t ? vp.msg : GN[g];
    char b[16];
    if (text_w(&FONT_L, m) <= 236)                  /* the header: large; a long message small */
        cv_text(120 - text_w(&FONT_L, m) / 2, 0, &FONT_L, m, vp.msg_t || ui.msg_t ? C_WHITE : TE_G4);
    else
        cv_text(120 - text_w(&FONT_S, m) / 2, 8, &FONT_S, m, C_WHITE);
    for (k = 0; k < nch; k++)
        vp_cluster(k, g * 7u + k);
    if (g == 0u) {
        vp_mcol(7, "LVL", MI_VLVL, voc_level % VOC_LEVEL_N, VOC_LEVEL_N, W);
        vp_item(0, 1, 8, 0, W, "LEVEL", mi_value(MI_VLVL, &c));
        str_cpy(b, BYP3[vpg.byp % VB_N], sizeof b);
        if (vpg.byp != VB_OFF) {
            str_cpy(b + str_len(b), " ", 2);
            vp_nodb(b + str_len(b), mi_value(MI_VDRY, &c), 8);
        }
        vp_item(3, 0, 8, 0, W, "BYPASS", b);
        vp_item(4, 0, 8, 1, VP_COL_E, "VOCODER", VOC4[vp_voc()]);
        cv_text(2, VP_Y1 + 20, &FONT_S, "HOLD", TE_G3);   /* (the BYPASS pad held: K8 is its level) */
        cv_rect(38, VP_Y1 + 22, 13, 13, W);
        cv_rect(38, VP_Y1 + 22, 13, 2, C_WHITE);
        cv_text(56, VP_Y1 + 20, &FONT_S, "+K8", TE_G3);
    } else if (g == 1u) {
        vp_mcol(7, "BRG", MI_VBR, voc_bridge % VOC_BR_N, VOC_BR_N, VP_COL_B);
        vp_item(0, 1, 8, 0, VP_COL_B, "BRIDGE", mi_value(MI_VBR, &c));
        vp_item(3, 0, 8, 0, W, "AUTO", vpg.actl ? "ON" : "OFF");
        vp_item(4, 0, 8, 1, VP_COL_B, "BRIDGING", SEL3[vpg.brsel % VS_N]);
    } else {
        static const char *const VM3[VV_N] = {"AUTO", "VOIC", "UNV"};
        vp_mcol(6, "THR", MI_VTHR, voc_thr % VOC_THR_N, VOC_THR_N, W);
        vp_mcol(7, "ADD", MI_VHF, voc_hf % VOC_MIX_N, VOC_MIX_N, VP_COL_A);
        vp_item(0, 1, 7, 0, W, "THRESH", mi_value(MI_VTHR, &c));
        vp_item(1, 1, 8, 0, VP_COL_A, "ADD SUM", mi_value(MI_VHF, &c));
        vp_item(3, 0, 7, 0, RGB(40, 220, 90), "VOICE", VM3[voc_vmode % VV_N]);
        vp_item(4, 0, 7, 1, RGB(255, 90, 160), "NOISE", voc_pink ? "PINK" : "WHITE");
        vp_item(5, 0, 8, 1, VP_COL_A, "ADDITION", SEL3[vpg.addsel % VS_N]);
    }
}
/* what the page shows, as one number: drawn again only when it changes (no clearing: no flash) */
static uint32_t vp_sig(void)
{
    const uint8_t *q = (const uint8_t *)&vpg;
    uint32_t i, h = 2166136261u;
    for (i = 0; i < sizeof vpg; i++)
        h = (h ^ q[i]) * 16777619u;
    h = (h ^ (voc_level | (uint32_t)voc_dry << 4 | (uint32_t)voc_hf << 8 | (uint32_t)voc_bridge << 12 |
              (uint32_t)voc_thr << 18 | (uint32_t)voc_vmode << 21 | (uint32_t)voc_pink << 23 | (uint32_t)voc_mode << 24 |
              (uint32_t)voc_norm << 26 | (uint32_t)voc_emsel << 27 | (uint32_t)vp.grp << 28)) * 16777619u;
    h = (h ^ (vp.msg_t ? 1u : 0u) ^ (ui.msg_t ? 2u : 0u)) * 16777619u;
    for (i = 0; vp.msg_t && vp.msg[i]; i++)
        h = (h ^ (uint8_t)vp.msg[i]) * 16777619u;
    for (i = 0; ui.msg_t && ui.msg[i]; i++)
        h = (h ^ (uint8_t)ui.msg[i]) * 16777619u;
    return h;
}
/* in two bands of 120 rows; the screen cleared once, when the page comes up */
static void vis_draw(void)
{
    static uint32_t shown;
    uint32_t pass, sig;
    if (vp.msg_t)
        vp.msg_t--;
    sig = vp_sig();
    if (!vis_shown_last) {
        vis_shown_last = 1;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        shown = sig + 1u;
    }
    if (sig == shown)
        return;
    shown = sig;
    for (pass = 0; pass < 2u; pass++) {
        cv_begin(240, 120, C_BLACK);
        cv_oy = -(int32_t)(pass * 120u);
        vp_paint();
        cv_oy = 0;
        cv_blit(0, pass * 120u);
    }
}
