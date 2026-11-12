/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37, the vocoder build: the HARMONY page (the harmoniser's, Stage 1b: smk37-devtools harmoniser-spec.md §9;
 * HARMONISER.md). In the visualiser's place as the vocoder page (ui_vpage.c, included from it: vis_pg 1): PATCH on the
 * vocoder page goes on to it while HARMONY is on, PATCH on it to PUNCH IN. < > (and SELECT) step the groups:
 *   G1 INTERVALS  pads: the HK940's 16 intervals (top up, bottom down), held momentary / tapped latched, played as T1's
 *                 notes (smk_harm.h hm_pad); K1-K3 MICRO V1-V3, K4-K6 LEVEL V1-V3, K8 WET / DRY
 *   G2 PANEL      pads (the H949's switch row): FUNCTION SELECT, F1-F4 (IN: NORM, EXTEND, SHARP, FLAT), VOICE, MANUAL,
 *                 KEYS & MIDI (both together: K+M); MAIN 6.25 .. 200 ms (Stage 2), IN / OUT, REPEAT; K1 INPUT LEVEL,
 *                 K3 FB MAIN, K5 LOW EQ, K6 HIGH EQ, K8 WET / DRY
 *   G3 OUTPUTS    pads: DRY, V1, V2, V3 (on / off), DLY, SPLIT, ALGORITHM (Stage 2), FB LIMIT SAFE / H949; DELAY ONLY
 *                 (Stage 2), REPEAT; K1 DRY LEVEL, K8 WET / DRY
 * The pads carrying a signal are its meter (8 steps of 6 dB, orange from -6 dBFS, red from -1 held ~1 s). The screen:
 * the H949's PITCH RATIO readout (four red seven-segment digits: the readout voice's ratio), its tag, the control
 * method, the sung note, REP; the function lights; the three voices (interval, MICRO, LEVEL, meter); the input lights,
 * FB, FB LIMIT, the EQ, WET / DRY, IN / OUT; the group's knobs. Everything drawn comes from one view (hp_view), and the
 * page is drawn again only when that view changes (the 1a lesson: a redraw signature built from what is drawn). Stage 2's
 * switches are drawn dim and say so when pressed. FADER BANK E's FEEDBACK and WET / DRY are the same values as K3 / K8:
 * a knob turn leaves that fader waiting to pick up. */
#ifndef SMK_HARM
#error "ui_hpage.c: the harmoniser (smk_harm.h)"
#endif
#define HP_LONG_MS 400u                             /* REPEAT's pad held longer: momentary (as NOTE REPEAT) */
#define HP_IV_LONG_MS 600u                          /* an interval pad held longer: momentary (a slow tap latched at
                                                     * 400 ms let go, 2026-11-13: as the vocoder page's pads, 600) */
#define HP_PAD_ON 70u                               /* (a pad that is on: at least this bright, of 255) */
#define HP_RV_FR 90u                                /* (frames, ~3 s: the knob's voice on the readout) */
static const char *const HP_GN[3] = {"G1 INTERVALS", "G2 PANEL", "G3 OUTPUTS"};
static const uint8_t HP_VRGB[3][3] = {{255, 150, 30}, {40, 190, 255}, {180, 100, 255}};   /* V1 orange, V2 cyan, V3 violet */
#define HP_VCOL(v) ((v) == 0u ? RGB(255, 150, 30) : (v) == 1u ? RGB(40, 190, 255) : RGB(180, 100, 255))
#define HP_SEG_ON RGB(255, 40, 30)
#define HP_SEG_OFF RGB(46, 8, 8)
static struct {
    uint8_t grp, msg_t, rv, rv_t, red_t[6];         /* the group; the header's message frames; the knob's voice, its
                                                     * frames; each meter's red hold */
    uint8_t mk[6], mkt[6];                          /* the meters (HM_UP_*): their steps, held, and the hold's frames */
    uint16_t pend;                                  /* the pads down whose release counts (momentary, REPEAT) */
    uint32_t t0[16];
    char msg[24];
} hp;
/* everything the page draws (the signature is its hash) */
typedef struct {
    char head[24];
    uint16_t ratio;
    uint8_t grp, rv, rv_on, ctl, note, scale, rep, fn, fsel, vsel, byp, fbl, low, high, inl, fb, wd, dry, dmute, lamps,
        clip, h949_red;
    uint8_t on[3], fnv[3], lv[3], mute[3], mk[3], red[3];
    int8_t iv[3], mic[3];
} hp_view_t;

static void hp_say(const char *a, const char *b)    /* the header, a second and a half */
{
    str_cpy(hp.msg, a, sizeof hp.msg);
    if (b[0]) {
        str_cpy(hp.msg + str_len(hp.msg), " ", sizeof hp.msg - str_len(hp.msg));
        str_cpy(hp.msg + str_len(hp.msg), b, sizeof hp.msg - str_len(hp.msg));
    }
    hp.msg_t = 45;
}
static void hp_db(char *b, uint32_t pct)            /* a linear 0 .. 100 % level as dB, "-6.0 DB", "OFF" */
{
    int32_t t;
    if (!pct) {
        str_cpy(b, "OFF", 4);
        return;
    }
    t = (VP_DB[pct > 100u ? 100u : pct] - 120) / 2;  /* (VP_DB: 400 log10(p / 50); 20 log10(p / 100) in tenths) */
    b[0] = t < 0 ? '-' : '+';
    fmt_fix(b + 1, t < 0 ? -t : t, 1);
    str_cpy(b + str_len(b), " DB", 4);
}
static void hp_sdb(char *b, int32_t d)              /* signed whole dB, "+3 DB" */
{
    b[0] = d < 0 ? '-' : '+';
    fmt_int(b + 1, d < 0 ? -d : d);
    str_cpy(b + str_len(b), " DB", 4);
}
static void hp_pct(char *b, uint32_t v127)          /* 0 .. 127 as %, 64: 50 % */
{
    fmt_int(b, (int32_t)((v127 * 100u + 64u) / 128u));
    str_cpy(b + str_len(b), " %", 3);
}
static void hp_unpick(uint32_t col)                 /* a knob set FADER BANK E's value: its fader waits to pick up */
{
    if (smk_fbank % FB_N == 4u)
        smk_flive &= (uint8_t)~(1u << col);
}
static uint32_t hp_vmask(void) { return hs.vsel ? 1u << ((hs.vsel - 1u) % 3u) : 7u; }   /* the VOICE-selected */
static void hp_vname(char *b, uint32_t v) { b[0] = 'V', b[1] = (char)('1' + v), b[2] = 0; }
static void hp_ctlname(char *b)
{
    uint32_t c = hs.ctl & 3u;
    str_cpy(b, c == 3u ? "K+M" : c == HC_MAN ? "MAN" : "KEYS", 5);
}

/* ---- the knobs ---- */
static void hp_turn(uint32_t k, int32_t s)
{
    uint32_t g = hp.grp % 3u;
    int32_t f = s > 1 || s < -1 ? 3 : 1, n;
    char a[12], b[16];
    if (k == 7u) {                                      /* K8, every group: WET / DRY (= bank E's F4) */
        hm_wd = (int16_t)clamp(hm_wd + s * f, 0, 127);
        hp_unpick(3u);
        hp_pct(b, (uint32_t)hm_wd);
        hp_say("WET/DRY", b);
        return;
    }
    if (g == 0u && k < 3u) {                            /* MICRO V1 .. V3 (a detent at 0) */
        int32_t o = hs.mic[k];
        n = clamp(o + s * f, -100, 100);
        if ((o < 0 && n > 0) || (o > 0 && n < 0))
            n = 0;
        hs.mic[k] = (int8_t)n;
        hp.rv = (uint8_t)k, hp.rv_t = HP_RV_FR;
        hp_vname(a, k);
        str_cpy(a + 2, " MICRO", 8);
        if (hs.fn[k] == HF_SHARP || hs.fn[k] == HF_FLAT) {
            fmt_fix(b, (int32_t)hm_ratio1000(k), 3);   /* (SHARP / FLAT: the ratio it makes) */
        } else {
            b[0] = n < 0 ? '-' : '+';
            fmt_int(b + 1, n < 0 ? -n : n);
            str_cpy(b + str_len(b), " C", 3);
        }
        hp_say(a, b);
    } else if (g == 0u && k >= 3u && k < 6u) {          /* LEVEL V1 .. V3 */
        uint32_t v = k - 3u;
        hs.lv[v] = (uint8_t)clamp((int32_t)hs.lv[v] + s * 2 * f, 0, 100);
        hp.rv = (uint8_t)v, hp.rv_t = HP_RV_FR;
        hp_vname(a, v);
        str_cpy(a + 2, " LEVEL", 8);
        hp_db(b, hs.lv[v]);
        hp_say(a, b);
    } else if (g == 1u && k == 0u) {                    /* INPUT LEVEL */
        hs.inl = (uint8_t)clamp((int32_t)hs.inl + s * f, 0, (int32_t)HM_INL_N - 1);
        if (hs.inl)
            hp_sdb(b, (int32_t)hs.inl - (int32_t)HM_INL_0);
        else
            str_cpy(b, "OFF", 4);
        hp_say("INPUT", b);
    } else if (g == 1u && k == 2u) {                    /* FB MAIN (= bank E's F2) */
        hm_fb = (int16_t)clamp(hm_fb + s * f, 0, 127);
        hp_unpick(1u);
        hp_pct(b, (uint32_t)hm_fb);
        hp_say("FEEDBACK", b);
    } else if (g == 1u && (k == 4u || k == 5u)) {       /* the feedback's LOW / HIGH EQ */
        uint8_t *e = k == 4u ? &hs.low : &hs.high;
        uint32_t nn = k == 4u ? HM_LOW_N : HM_HIGH_N, z = k == 4u ? HM_LOW_0 : HM_HIGH_0;
        *e = (uint8_t)clamp((int32_t)*e + s, 0, (int32_t)nn - 1);
        hp_sdb(b, (int32_t)*e - (int32_t)z);
        hp_say(k == 4u ? "LOW EQ" : "HIGH EQ", b);
    } else if (g == 2u && k == 0u) {                    /* DRY LEVEL */
        hs.dry = (uint8_t)clamp((int32_t)hs.dry + s * 2 * f, 0, 100);
        hp_db(b, hs.dry);
        hp_say("DRY", b);
    } else {
        hp_say("STAGE 2", "");                          /* (RATE, FB DLY ONLY, DLY ONLY LEVEL, the pans) */
    }
}

/* ---- the pads (reading order: 0 .. 7 the top row) ---- */
static void hp_rep_press(uint32_t p)                /* REPEAT, as NOTE REPEAT: a tap latches, held it springs back */
{
    hm.rep = (uint8_t)!hm.rep;
    hm_rep_pad = 1;
    hp.pend |= (uint16_t)(1u << p), hp.t0[p] = fm1_ms;
    hp_say("REPEAT", hm.rep ? "ON" : "OFF");
}
static void hp_press(uint32_t p)
{
    uint32_t g = hp.grp % 3u, bit = 1u << p, v;
    static const char *const FIN[4] = {"NORM", "EXTEND", "SHARP", "FLAT"};
    static const char *const FOUT[4] = {"DELAY", "RANDOM", "FLANGE", "REVERSE"};
    static const char *const MS[6] = {"6.25 MS", "12.5 MS", "25 MS", "50 MS", "100 MS", "200 MS"};
    char a[8];
    if (g == 0u) {                                      /* an interval: latched again = let go; else on, maybe held */
        uint32_t nt = (uint32_t)(60 + HM_PADIV[p]), held = 0;
        for (v = 0; v < 3u; v++)
            held |= hm.note[v] == nt;
        if ((hm_pad & bit) && !held) {                  /* latched, but its voice was taken (a key, the sequence):
                                                         * played again, still latched */
            hm_padre |= (uint16_t)bit;
            return;
        }
        if (hm_pad & bit) {
            hm_pad &= (uint16_t)~bit;
            hp.pend &= (uint16_t)~bit;
        } else {
            hm_pad |= (uint16_t)bit;
            hp.pend |= (uint16_t)bit, hp.t0[p] = fm1_ms;
        }
        return;
    }
    if (p == 15u) {
        hp_rep_press(p);
        return;
    }
    if (g == 1u) {
        if (p == 0u) {
            hs.fsel ^= 1u;
            hp_say("FUNCTION", hs.fsel ? "OUT" : "IN");
        } else if (p <= 4u) {
            if (hs.fsel) {
                hp_say(FOUT[p - 1u], "STAGE 2");
            } else if (p == 2u) {
                hp_say("EXTEND", "STAGE 2");
            } else {
                uint32_t m = hp_vmask(), f = p == 1u ? HF_NORM : p == 3u ? HF_SHARP : HF_FLAT;
                for (v = 0; v < 3u; v++)
                    if ((m >> v) & 1u)
                        hs.fn[v] = (uint8_t)f;
                hp_say(FIN[p - 1u], hs.vsel ? "" : "ALL");
            }
        } else if (p == 5u) {
            hs.vsel = (uint8_t)((hs.vsel + 1u) & 3u);
            if (hs.vsel) {
                hp_vname(a, hs.vsel - 1u);
                hp.rv = (uint8_t)(hs.vsel - 1u), hp.rv_t = HP_RV_FR;
            } else {
                str_cpy(a, "ALL", 4);
            }
            hp_say("VOICE", a);
        } else if (p == 6u || p == 7u) {                /* MANUAL / KEYS & MIDI: the other held too, both */
            uint32_t other = p == 6u ? 7u : 6u;
            hs.ctl = (uint8_t)((smk_pick_down >> other) & 1u ? 3u : p == 6u ? HC_MAN : HC_KEYS);
            hp_ctlname(a);
            hp_say("CONTROL", a);
        } else if (p < 14u) {
            hp_say(MS[p - 8u], "STAGE 2");
        } else {
            hs.byp ^= 1u;
            hp_say("IN/OUT", hs.byp ? "OUT" : "IN");
        }
        return;
    }
    if (p < 4u) {                                       /* G3: DRY, V1 .. V3 on / off */
        hs.mute ^= (uint8_t)(1u << p);
        if (p)
            hp_vname(a, p - 1u);
        else
            str_cpy(a, "DRY", 4);
        hp_say(a, (hs.mute >> p) & 1u ? "OFF" : "ON");
    } else if (p == 7u) {
        hs.fbl ^= 1u;
        hp_say("FB LIMIT", hs.fbl ? "H949" : "SAFE");
    } else if (p == 4u || p == 5u || p == 6u || p < 14u) {
        hp_say(p == 4u ? "DELAY ONLY" : p == 5u ? "SPLIT" : p == 6u ? "ALGORITHM" : "DELAY ONLY", "STAGE 2");
    }
}
static void hp_release(uint32_t p, uint32_t held)
{
    if (p == 15u && hp.grp % 3u) {                      /* REPEAT: held, momentary */
        hm_rep_pad = 0;
        if (held > HP_LONG_MS)
            hm.rep = (uint8_t)!hm.rep;
        return;
    }
    if (hp.grp % 3u == 0u && held > HP_IV_LONG_MS)       /* an interval held: momentary */
        hm_pad &= (uint16_t)~(1u << p);
}
/* once a pass with the page up (vp_input): the knobs, the pads hit (h), the pads let go */
static void hp_input(uint32_t h)
{
    uint32_t k;
    for (k = 0; k < 8u; k++) {
        int32_t s = vp_knob(k);
        if (s)
            hp_turn(k, s);
    }
    enc_hold |= (1u << NE) - 1u;                       /* (as the vocoder page: no other knob code this pass) */
    for (k = 0; h && k < 16u; k++)
        if ((h >> k) & 1u)
            hp_press(k);
    for (k = 0; hp.pend && k < 16u; k++)
        if ((hp.pend >> k) & 1u && !((smk_pick_down >> k) & 1u)) {
            hp.pend &= (uint16_t)~(1u << k);
            hp_release(k, fm1_ms - hp.t0[k]);
        }
}
static void hp_select(int32_t s)                    /* < > (and SELECT): the groups */
{
    hp.grp = (uint8_t)((hp.grp + (s > 0 ? 1u : 2u)) % 3u);
    if (hp.pend & 0x8000u)
        hm_rep_pad = 0;
    hp.pend = 0;                                        /* (a pad held over the change: it stays as it is) */
}

/* a fader moved while the page is up (main.c smk_faders): the page stays, its header says what the fader did (the
 * owner, 2026-11-13: "we should be able to see the large pitch dial and info when operating the faders"), as the
 * FADER BANK screen would: the value, or which way to move it to pick up. 0: the page is not up (the screen shows) */
static int hp_fader(uint32_t i, int waiting, int up)
{
    char lab[10], v2[20];
    int16_t *vp;
    if (!vis_pg || !vis_shown())
        return 0;
    (void)fb_text(smk_fbank % FB_N, i, lab, v2, &vp);
    if (waiting)
        hp_say(lab, up ? "MOVE UP" : "MOVE DOWN");     /* (PICKUP: the fader waits for the bank's value) */
    else
        hp_say(lab, v2);
    return 1;
}

/* ---- the meters (once a frame, from hp_leds) ---- */
static uint32_t hp_step(int32_t pk)                 /* a peak (0 .. 32767) in 8 steps of 6 dB */
{
    uint32_t s = 0, x = (uint32_t)(pk > 0 ? pk : 0) >> 7;
    for (; x && s < 7u; x >>= 1)
        s++;
    return s;
}
static void hp_meters(void)
{
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        int32_t pk = hm.up[k];
        uint32_t s = hp_step(pk);
        hm.up[k] = 0;
        if (pk >= 29204)                                /* -1 dBFS: red, held ~1 s */
            hp.red_t[k] = 30;
        else if (hp.red_t[k])
            hp.red_t[k]--;
        if (s >= hp.mk[k])
            hp.mk[k] = (uint8_t)s, hp.mkt[k] = 0;
        else if (++hp.mkt[k] >= 4u)
            hp.mk[k]--, hp.mkt[k] = 0;
    }
}
static void hp_padmeter(uint32_t p, const uint8_t *c, uint32_t k)   /* pad p: colour c by meter k's level, never under
                                                     * HP_PAD_ON: a pad that is on shows its colour in silence too (the
                                                     * 2026-11-13 screenshots: a quiet active pad was darker than an
                                                     * inactive one, an unmuted voice darker than a muted one) */
{
    uint32_t b = VP_MB[hp.mk[k] & 7u] < HP_PAD_ON ? HP_PAD_ON : VP_MB[hp.mk[k] & 7u];
    if (hp.red_t[k])
        padled_set(p, 255, 20, 20);
    else if (hp.mk[k] >= 7u)
        padled_set(p, 255, 120, 0);                     /* (-6 dBFS and over: orange) */
    else
        padled_set(p, (uint8_t)(c[0] * b / 255u), (uint8_t)(c[1] * b / 255u), (uint8_t)(c[2] * b / 255u));
}
static void hp_leds(void)
{
    static const uint8_t RED[3] = {255, 30, 20}, GRN[3] = {40, 230, 60}, AMB[3] = {255, 170, 0}, WHT[3] = {200, 200, 200};
    uint32_t g = hp.grp % 3u, p, v;
    hp_meters();
    for (p = 0; p < 16u; p++)
        padled_set(p, 4, 4, 4);                         /* (Stage 2's and the free: nearly dark) */
    if (g == 0u) {
        for (p = 0; p < 16u; p++) {
            uint32_t nt = (uint32_t)(60 + HM_PADIV[p]), u = 3u;
            for (v = 0; v < 3u; v++)
                if (hm.note[v] == nt)
                    u = v;
            if ((smk_pick_down >> p) & 1u && (hm_pad >> p) & 1u)
                padled_set(p, 255, 255, 255);           /* (held: white) */
            else if (u < 3u)
                hp_padmeter(p, HP_VRGB[u], HM_UP_V + u);
            else
                padled_set(p, 16, 16, 16);
        }
        return;
    }
    if (g == 1u) {
        uint32_t m = hp_vmask(), fv = m == 7u ? 0u : (uint32_t)__builtin_ctz(m), fn = hs.fn[fv];
        padled_set(0, hs.fsel ? 40 : 255, hs.fsel ? 230 : 30, hs.fsel ? 60 : 20);
        for (p = 1; p <= 4u; p++) {
            if (hs.fsel)
                padled_set(p, 4, 20, 6);                /* (OUT: Stage 2) */
            else if (p == 2u)
                padled_set(p, 6, 2, 2);                 /* (EXTEND: Stage 2, nearly dark as the others) */
            else if ((p == 1u && fn == HF_NORM) || (p == 3u && fn == HF_SHARP) || (p == 4u && fn == HF_FLAT))
                hp_padmeter(p, RED, HM_UP_OUT);         /* (the active function: the voices' level) */
            else
                padled_set(p, 22, 3, 2);                /* (an IN function not chosen: dim red) */
        }
        if (hs.vsel)
            vp_padrgb(5, HP_VRGB[(hs.vsel - 1u) % 3u], 0);
        else
            vp_padrgb(5, WHT, 0);
        vp_padrgb(6, AMB, hs.ctl & HC_MAN ? 0u : 4u);
        vp_padrgb(7, AMB, hs.ctl & HC_KEYS ? 0u : 4u);
        if (hs.byp)
            padled_set(14, 60, 6, 6);                   /* (OUT: the input straight through) */
        else
            hp_padmeter(14, GRN, HM_UP_IN);             /* (IN: the input's level) */
    } else {
        if (hs.mute & 1u)
            padled_set(0, 30, 2, 2);
        else
            hp_padmeter(0, WHT, HM_UP_IN);
        for (v = 0; v < 3u; v++) {
            if ((hs.mute >> (v + 1u)) & 1u)
                padled_set(1u + v, 30, 2, 2);
            else
                hp_padmeter(1u + v, HP_VRGB[v], HM_UP_V + v);
        }
        if (hs.fbl)
            hp_padmeter(7, AMB, HM_UP_FB);              /* (H949: amber, the loop's level) */
        else
            hp_padmeter(7, GRN, HM_UP_FB);
    }
    if (hm_rep_shown())
        padled_set(15, 220, 120, 255);                  /* REPEAT (G2, G3): violet, flashing while latched */
    else
        padled_set(15, 30, 16, 40);
}

/* ---- the view ---- */
static uint32_t hp_rvoice(void)                     /* the readout's voice: the knob's, MANUAL's, the newest */
{
    uint32_t v, best = 3u;
    if (hp.rv_t)
        return hp.rv % 3u;
    if ((hs.ctl & HC_MAN) && hm.on[hm_mvoice()])
        return hm_mvoice();
    for (v = 0; v < 3u; v++)
        if (hm.on[v] && hm.note[v] && (best == 3u || hm.age[v] > hm.age[best]))
            best = v;
    return best < 3u ? best : hs.vsel ? (hs.vsel - 1u) % 3u : 0u;
}
static void hp_mkview(hp_view_t *w)
{
    uint32_t v, ctl = hs.ctl & 3u, m = hm_mvoice(), pk = (uint32_t)hp.mk[HM_UP_IN];
    memset(w, 0, sizeof *w);
    str_cpy(w->head, ui.msg_t ? ui.msg : hp.msg_t ? hp.msg : HP_GN[hp.grp % 3u], sizeof w->head);
    w->grp = (uint8_t)(hp.grp % 3u);
    w->rv = (uint8_t)hp_rvoice();
    w->rv_on = hm.on[w->rv];
    w->ratio = (uint16_t)hm_ratio1000(w->rv);
    w->ctl = (uint8_t)(ctl ? ctl : HC_KEYS);
    w->scale = hm_int;
    w->note = hm_int ? hm.tnote : 0;
    w->rep = (uint8_t)hm_rep_shown();
    w->fn = hs.fn[w->rv];
    w->fsel = hs.fsel, w->vsel = hs.vsel, w->byp = hs.byp, w->fbl = hs.fbl, w->low = hs.low, w->high = hs.high;
    w->inl = hs.inl, w->fb = (uint8_t)hm_fb, w->wd = (uint8_t)hm_wd, w->dry = hs.dry, w->dmute = hs.mute & 1u;
    w->lamps = (uint8_t)(pk >= 7u ? 5u : pk >= 6u ? 4u : pk >= 5u ? 3u : pk >= 4u ? 2u : pk >= 2u ? 1u : 0u);
    w->clip = hp.red_t[HM_UP_IN] != 0;
    w->h949_red = hp.red_t[HM_UP_FB] != 0;
    for (v = 0; v < 3u; v++) {
        int32_t s = hm.note[v] ? (int32_t)hm.note[v] - 60 : 0;
        w->on[v] = hm.on[v];
        w->fnv[v] = hs.fn[v];
        w->lv[v] = hs.lv[v];
        w->mute[v] = (uint8_t)((hs.mute >> (v + 1u)) & 1u);
        w->mk[v] = hp.mk[HM_UP_V + v];
        w->red[v] = hp.red_t[HM_UP_V + v] != 0;
        w->mic[v] = hs.mic[v];
        w->iv[v] = (int8_t)((ctl & HC_MAN) && v == m && (ctl == HC_MAN || !hm.note[v]) ? 127 : clamp(s, -60, 60));
    }
}
static uint32_t hp_sig(const hp_view_t *w)
{
    const uint8_t *q = (const uint8_t *)w;
    uint32_t i, h = 2166136261u;
    for (i = 0; i < sizeof *w; i++)
        h = (h ^ q[i]) * 16777619u;
    return h;
}

/* ---- the drawing (from the view only) ---- */
static const uint8_t HP_SEG7[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
static void hp_digit(int32_t x, int32_t y, uint32_t d, uint16_t on)   /* a seven-segment digit, 32 x 62 */
{
    const int32_t w = 32, t = 6, hh = (62 - 3 * 6) / 2;
    uint32_t m = HP_SEG7[d % 10u];
#define HP_S(bit, X, Y, W, H) cv_rect(X, Y, W, H, (m >> (bit)) & 1u ? on : HP_SEG_OFF)
    HP_S(0, x + t, y, w - 2 * t, t);                    /* a */
    HP_S(1, x + w - t, y + t, t, hh);                   /* b */
    HP_S(2, x + w - t, y + 2 * t + hh, t, hh);          /* c */
    HP_S(3, x + t, y + 2 * t + 2 * hh, w - 2 * t, t);   /* d */
    HP_S(4, x, y + 2 * t + hh, t, hh);                  /* e */
    HP_S(5, x, y + t, t, hh);                           /* f */
    HP_S(6, x + t, y + t + hh, w - 2 * t, t);           /* g */
#undef HP_S
}
static void hp_ivname(char *b, int32_t s)           /* an interval: "U", "+M3", "-P5", "+8VA", "-15MA", "+17" */
{
    static const char *const N[13] = {"U", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7", "8VA"};
    int32_t a = s < 0 ? -s : s;
    if (!s) {
        str_cpy(b, "U", 2);
        return;
    }
    b[0] = s < 0 ? '-' : '+';
    if (a <= 12)
        str_cpy(b + 1, N[a], 4);
    else if (a == 24)
        str_cpy(b + 1, "15MA", 5);
    else
        fmt_int(b + 1, a);
}
static void hp_paint(const hp_view_t *w)
{
    static const char *const FL[8] = {"NRM", "EXT", "SHP", "FLT", "DLY", "RND", "FLG", "REV"};
    static const char *const LEG[3] = {"K1-3 MICRO K4-6 LVL K8 W/D", "K1 IN K3 FB K5 LO K6 HI", "K1 DRY  PADS: OUTPUTS"};
    uint32_t v, i, r = w->ratio;
    uint16_t on = w->rv_on ? HP_SEG_ON : RGB(120, 24, 18), W = RGB(220, 220, 220);
    char b[16];
    /* the header: the group, or what was just set */
    if (text_w(&FONT_L, w->head) <= 236)
        cv_text(120 - text_w(&FONT_L, w->head) / 2, 0, &FONT_L, w->head, hp.msg_t || ui.msg_t ? C_WHITE : TE_G4);
    else
        cv_text(120 - text_w(&FONT_S, w->head) / 2, 8, &FONT_S, w->head, C_WHITE);
    /* the PITCH RATIO readout: d.ddd */
    hp_digit(76, 36, r / 1000u, on);
    cv_rect(112, 92, 6, 6, on);                         /* (the decimal point) */
    hp_digit(122, 36, r / 100u % 10u, on);
    hp_digit(162, 36, r / 10u % 10u, on);
    hp_digit(202, 36, r % 10u, on);
    /* at its left: the voice, the control method, the sung note, REP */
    hp_vname(b, w->rv);
    cv_text(4, 34, &FONT_L, b, HP_VCOL(w->rv));
    str_cpy(b, w->ctl == 3u ? "K+M" : w->ctl == HC_MAN ? "MAN" : "KEYS", 5);
    cv_text(4, 66, &FONT_S, b, W);
    if (w->scale && w->note)
        note_name(b, w->note);
    else
        str_cpy(b, w->scale ? "--" : "FIX", 4);
    cv_text(4, 84, &FONT_S, b, w->scale ? W : TE_G3);
    if (w->rep)
        cv_text(36, 84, &FONT_S, "REP", RGB(220, 120, 255));
    /* the function lights: the readout voice's (red: pitch; green: delay, Stage 2) */
    for (i = 0; i < 8u; i++) {
        int32_t x = 2 + (int32_t)i * 30;
        int lit = i < 4u && ((i == 0u && w->fn == HF_NORM) || (i == 2u && w->fn == HF_SHARP) || (i == 3u && w->fn == HF_FLAT));
        uint16_t c = i < 4u ? (i == 1u ? RGB(70, 16, 12) : RGB(160, 30, 24)) : RGB(14, 60, 20);
        if (lit) {
            cv_rect(x, 103, 28, 18, HP_SEG_ON);
            cv_text(x + 2, 104, &FONT_S, FL[i], C_BLACK);
        } else {
            cv_text(x + 2, 104, &FONT_S, FL[i], c);
        }
    }
    /* the voices */
    for (v = 0; v < 3u; v++) {
        int32_t x = (int32_t)v * 80, lw = 70 * w->lv[v] / 100, mw = w->mk[v] ? 10 * (int32_t)w->mk[v] : 0;
        uint16_t vc = HP_VCOL(v), tc = w->on[v] ? C_WHITE : TE_G2;
        hp_vname(b, v);
        cv_text(x + 4, 126, &FONT_S, b, w->mute[v] ? TE_G2 : vc);
        if (w->fnv[v] == HF_SHARP || w->fnv[v] == HF_FLAT)
            str_cpy(b, w->fnv[v] == HF_SHARP ? "SHP" : "FLT", 4);
        else if (w->iv[v] == 127)
            str_cpy(b, "MAN", 4);
        else if (w->on[v])
            hp_ivname(b, w->iv[v]);
        else
            str_cpy(b, "-", 2);
        cv_text(x + 26, 126, &FONT_S, b, tc);
        if (w->mic[v]) {
            b[0] = w->mic[v] < 0 ? '-' : '+';
            fmt_int(b + 1, w->mic[v] < 0 ? -w->mic[v] : w->mic[v]);
            cv_text(x + 26, 142, &FONT_S, b, RGB(255, 190, 0));   /* (MICRO, under its interval) */
        }
        cv_rect(x + 4, 160, 70, 4, TE_G1);              /* LEVEL */
        if (!w->mute[v] && lw)
            cv_rect(x + 4, 160, lw, 4, vc);
        cv_rect(x + 4, 166, 70, 4, TE_G1);              /* the meter */
        if (w->mute[v])
            cv_text(x + 4, 142, &FONT_S, "OFF", RGB(255, 40, 40));
        else if (mw)
            cv_rect(x + 4, 166, mw, 4, w->red[v] ? RGB(255, 30, 30) : w->mk[v] >= 7u ? RGB(255, 140, 0) : W);
    }
    /* the input lights (-40, -18, -12, -6, -1 dBFS), FB, FB LIMIT */
    cv_text(2, 178, &FONT_S, "IN", TE_G3);
    for (i = 0; i < 5u; i++) {
        uint16_t c = i == 0u ? RGB(255, 210, 0) : i == 4u ? RGB(255, 30, 30) : RGB(40, 230, 60);
        cv_rect(24 + (int32_t)i * 12, 181, 9, 10, (i < w->lamps || (i == 4u && w->clip)) ? c : TE_G1);
    }
    str_cpy(b, "FB ", 4);
    hp_pct(b + 3, w->fb);
    cv_text(90, 178, &FONT_S, b, W);
    cv_text(176, 178, &FONT_S, w->fbl ? "H949" : "SAFE", w->fbl ? (w->h949_red ? RGB(255, 40, 40) : RGB(255, 150, 0)) : TE_G4);
    /* the EQ, WET / DRY, IN / OUT */
    if (w->low == HM_LOW_0 && w->high == HM_HIGH_0) {
        cv_text(2, 196, &FONT_S, "EQ FLAT", TE_G3);
    } else {
        str_cpy(b, "LO", 3);
        hp_sdb(b + 2, (int32_t)w->low - (int32_t)HM_LOW_0);
        b[str_len(b) - 3u] = 0;                         /* (no " DB": the line is narrow) */
        cv_text(2, 196, &FONT_S, b, W);
        str_cpy(b, "HI", 3);
        hp_sdb(b + 2, (int32_t)w->high - (int32_t)HM_HIGH_0);
        b[str_len(b) - 3u] = 0;
        cv_text(52, 196, &FONT_S, b, W);
    }
    str_cpy(b, "W/D ", 5);
    hp_pct(b + 4, w->wd);
    cv_text(110, 196, &FONT_S, b, W);
    if (w->byp)
        cv_text(196, 196, &FONT_S, "OUT", RGB(255, 40, 40));
    /* the group's knobs */
    cv_rect(0, 216, 240, 1, TE_G2);
    cv_text(2, 220, &FONT_S, LEG[w->grp % 3u], TE_G3);
}
/* the page, drawn as the vocoder page is (vis_draw: two bands of 120 rows), only when its view changed */
static uint32_t hp_frame(hp_view_t *w)
{
    if (hp.msg_t)
        hp.msg_t--;
    if (hp.rv_t)
        hp.rv_t--;
    hp_mkview(w);
    return hp_sig(w) ^ 0x9E3779B9u;
}
