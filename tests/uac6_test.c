/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the SMK-37 stems in src/usb.c (smk37-port.yaml follow_up.stems). -DT_STEMS=0: the probe build
 * (SMK_UAC_PROBE=1, the first tests on the unit); -DT_STEMS=1: the SYNTHS+RET layout as the firmware makes it:
 *   descriptors  the configuration walked as a host does (lengths, total), the input terminal 6 channels named by
 *                strings 4..9, the type I format 6 x 24 bit (3-byte subframes) at 48000 only, EP4 882 bytes,
 *                bcdDevice x.7x; with CDC (-DT_CDC=1) and without (-DT_CDC=0)
 *   packets      uac6_probe_packet over 10 s: 47..49 frames, 48 a ms on average, never over EP4's size; every sample as
 *                uac6_check.py (smk37-devtools) expects it: the counter on channel 5 steps by one, the triangles of
 *                channels 0..4 follow it, the low bytes are there (24 bit, not 16)
 *   stems        (T_STEMS) renders of 256 frames at 44.1 kHz through uac6_tap (6 channels: sines, DC, silence) against
 *                packets at 1 kHz (uac6_ring_packet), 3 s: 48 frames a ms, no underrun or overrun once primed, DC
 *                x 32 (the stereo output's full scale at -18 dBFS: 18 dB of headroom), the sines' level and frequency
 *                (no frame lost or doubled), the level m scales, the 24-bit ceiling; the same with STEM FILTER = LIGHT
 *                (8 taps) and at STEM RATE 44.1 (no resampling: 44.1 frames a ms, every sample x 32 exactly)
 *   exact        (T_STEMS) FULL and LIGHT: every output sample as 64-bit sums give it; each phase's taps sum to unity
 *   44.1         (T_STEMS) descriptors at STEM RATE 44.1: the format 44100, bcdDevice x.8x .. x.Ax, serials -D4 -Y4 -A4
 *   playback     (-DT_UIN=1, the probe with SMK_UAC_IN_PROBE, CDC on) the descriptors: interfaces 0..5 in order, the
 *                AC collection 2, 3, 1 with terminals 3 (USB streaming) -> 4 (speaker), interface 3's format 2 x 16 bit
 *                at 48000, EP 0x04 isochronous adaptive 196 bytes, the stems' format still 6 x 24, CDC on 4 and 5,
 *                bcdDevice 4.7x, serial -PI; the loopback: frames written to the ring as uin_rx does come back on
 *                channels 3 and 4 of the stems probe's packets, every one, in order, as the top 16 of 24 bits, with
 *                the other channels as before; the monitor (uin_mix): 48 frames a ms in, renders of 256 at 44.1 kHz
 *                (the engines' clock 0.04 % fast), 10 s: the servo holds the fill, no underrun or resync, a 1 kHz
 *                sine comes out at 1 kHz and half its level
 * The SIE is never touched. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define BOARD_SMK37 1
#ifndef T_STEMS
#define T_STEMS 0
#endif
#define SMK_UAC_PROBE (!T_STEMS)
#ifndef T_UIN
#define T_UIN 0
#endif
#define SMK_UAC_IN_PROBE T_UIN
#if T_UIN
#define FM1_TICKS_PER_US 24u
static uint32_t fm1_ticks(void) { return 0; }      /* (uin_rx's timing: the SIE is never touched here) */
#if T_STEMS
static uint8_t uin_mode;                          /* (voice.c smk_vocoder.h: HOME > USB > AUDIO IN) */
#endif
#endif
#ifndef FS
#define FS 44100                                  /* (core.h) */
#endif
#define CTL_LOG2 5                                /* (dsp.c: the mix's blocks of 32) */
#define FELUCCA_OTA 0
#define FELUCCA_CDC (T_CDC != 0)
#define FELUCCA_UAC 1
#ifndef HALF_FRAMES
#error "-DHALF_FRAMES=n (src/core.h; run_tests.sh passes it)"
#endif
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
static void fm1_irq_off(void) {}                  /* (usb_unplug: the host has one thread) */
static void fm1_irq_on(void) {}
static void smo_put(uint32_t pkt) { (void)pkt; }   /* (the SMK's TRS MIDI out: not here) */
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"   /* SIE register macros (never touched here) */
#include "../firmware/src/usb.c"

static int fails;
static int check(const char *what, int ok)
{
    printf("%-72s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
    return ok;
}
static uint32_t le16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }

static void test_descriptors(void)
{
    const uint8_t *c, *dv, *s;
    uint16_t cl, dl, sl;
    uint32_t off, i;
    int lens_ok = 1, it_ok = 0, fmt_ok = 0, ep_ok = 0, names_ok = 1;
    uint32_t cur_if = 0;
#if T_STEMS
    static const char *const NAMES[6] = {"SYN 1", "SYN 2", "SYN 3", "DRUMS", "RET L", "RET R"};
    usb_layout_mode = UL_SYNTHS;                       /* (main.c, from the menu) */
#else
    static const char *const NAMES[6] = {"KICK", "SNARE", "HATS", "PERC", "RET L", "RET R"};
#endif
#if T_CDC
    usb_cdc_on = 1;
#endif
    get_desc(0x0100, &dv, &dl);
    get_desc(0x0200, &c, &cl);
    check(T_STEMS ? "device descriptor: 18 bytes, bcdDevice x.5x (SYNTHS+RET's own version)" :
                    "device descriptor: 18 bytes, bcdDevice x.7x (the probe's own version)",
          dl == 18 && dv[0] == 18 && (dv[12] & 0xF0u) == (T_STEMS ? 0x50u : 0x70u) && dv[13] == (T_UIN ? 4 : 3));
    check("configuration: wTotalLength = the bytes given", le16(c + 2) == cl);
    for (off = 0; off < cl; off += c[off]) {
        const uint8_t *p = c + off;
        if (p[0] < 2u || off + p[0] > cl) {
            lens_ok = 0;
            break;
        }
        if (p[1] == 4u)
            cur_if = p[2];
        if (p[0] == 12u && p[1] == 0x24u && p[2] == 2u && p[3] == 1u)
            it_ok = p[7] == 6u && le16(p + 8) == 0 && p[10] == 4u;
        if (p[1] == 0x24u && p[2] == 2u && p[3] == 1u && p[0] >= 8u && cur_if == UAC_AS_IF)
            fmt_ok = p[0] == 11u && p[4] == 6u && p[5] == 3u && p[6] == 24u && p[7] == 1u &&
                     (p[8] | p[9] << 8 | p[10] << 16) == 48000;
        if (p[0] == 9u && p[1] == 5u && p[2] == 0x84u)
            ep_ok = (p[3] & 3u) == 1u && le16(p + 4) == 882u;
    }
    check("descriptors walk to the end, each length in bounds", lens_ok && off == cl);
    check("input terminal: 6 channels, no spatial positions, names from string 4", it_ok);
    check("type I format: 6 ch, 3-byte subframes, 24 bit, one rate 48000", fmt_ok);
    check("EP4 isochronous, wMaxPacketSize 882 (49 frames x 6 x 3)", ep_ok);
    for (i = 0; i < 6u; i++) {
        uint32_t k, n = (uint32_t)strlen(NAMES[i]);
        if (!get_desc(0x0300 | (4u + i), &s, &sl) || sl != 2u + 2u * n || s[0] != sl || s[1] != 3u) {
            names_ok = 0;
            continue;
        }
        for (k = 0; k < n; k++)
            names_ok &= s[2u + 2u * k] == (uint8_t)NAMES[i][k] && s[3u + 2u * k] == 0;
    }
    check(T_STEMS ? "strings 4..9: SYN 1 SYN 2 SYN 3 DRUMS RET L RET R" : "strings 4..9: KICK SNARE HATS PERC RET L RET R",
          names_ok);
    check("string 10: none", !get_desc(0x0300 | 10u, &s, &sl));
    {   /* the serial number (SMK_SN): string 3, the flash's unique ID folded, the probe's layout tag */
        static const uint8_t UID[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 1, 2, 3, 4, 5, 6, 7, 8};
        const char *want = T_STEMS ? (T_UIN ? "SMK37-C9C2507299B8-VY" : "SMK37-C9C2507299B8-SY")
                                   : T_UIN ? "SMK37-C9C2507299B8-PI" : "SMK37-C9C2507299B8-P6";   /* byte k of the fold: the
                                                         * XOR of bytes k, k + 6, k + 12 */
        uint32_t k, n = (uint32_t)strlen(want);
        int sn_ok;
        memcpy(smk_uid, UID, 16);
        sn_ok = get_desc(0x0303, &s, &sl) && sl == 2u + 2u * n && s[0] == sl && s[1] == 3u;
        for (k = 0; sn_ok && k < n; k++)
            sn_ok = s[2u + 2u * k] == (uint8_t)want[k] && s[3u + 2u * k] == 0;
        get_desc(0x0100, &dv, &dl);
        check("device descriptor: iSerialNumber 3", dv[16] == 3u);
        check(T_STEMS ? (T_UIN ? "string 3: SMK37-<the unique ID folded>-VY (the playback input's own)" :
                                 "string 3: SMK37-<the unique ID folded>-SY") :
              T_UIN ? "string 3: SMK37-<the unique ID folded>-PI" : "string 3: SMK37-<the unique ID folded>-P6", sn_ok);
#if T_STEMS
        {   /* STEREO: AUTO ST, USB RATE 44.1 S4, 48 S8 (their own descriptors: their own devices) */
#if T_UIN
            static const char *const W[3] = {"SMK37-C9C2507299B8-VS", "SMK37-C9C2507299B8-V4", "SMK37-C9C2507299B8-V8"};
#else
            static const char *const W[3] = {"SMK37-C9C2507299B8-ST", "SMK37-C9C2507299B8-S4", "SMK37-C9C2507299B8-S8"};
#endif
            uint32_t r, ok = 1;
            usb_layout_mode = UL_STEREO;
            for (r = 0; r < 3u; r++) {
                usb_rate_mode = (uint8_t)r;
                ok &= get_desc(0x0303, &s, &sl) && sl == 2u + 2u * 21u;
                for (k = 0; ok && k < 21u; k++)
                    ok = s[2u + 2u * k] == (uint8_t)W[r][k];
            }
            usb_rate_mode = 0, usb_layout_mode = UL_SYNTHS;
            check("string 3, STEREO: -ST (AUTO), -S4 (44.1 KHZ), -S8 (48 KHZ)", ok);
        }
        {   /* STEM RATE 44.1: each layout its own version, serial and format */
#if T_UIN
            static const char *const W[3] = {"SMK37-C9C2507299B8-WD", "SMK37-C9C2507299B8-WY", "SMK37-C9C2507299B8-WA"};
#else
            static const char *const W[3] = {"SMK37-C9C2507299B8-D4", "SMK37-C9C2507299B8-Y4", "SMK37-C9C2507299B8-A4"};
#endif
            uint32_t l, ok = 1, fmt = 1;
            stem_rate_mode = 1;
            for (l = UL_DRUMS; l <= UL_DRY; l++) {
                usb_layout_mode = (uint8_t)l;
                ok &= get_desc(0x0303, &s, &sl) && sl == 2u + 2u * 21u;
                for (k = 0; ok && k < 21u; k++)
                    ok = s[2u + 2u * k] == (uint8_t)W[l - 1u][k];
                get_desc(0x0100, &dv, &dl);
                ok &= (dv[12] & 0xF0u) == 0x70u + 0x10u * l;
                get_desc(0x0200, &c, &cl);
                for (off = 0, cur_if = 0; off < cl; off += c[off]) {
                    if (c[off + 1] == 4u)
                        cur_if = c[off + 2];
                    if (c[off] == 11u && c[off + 1] == 0x24u && c[off + 2] == 2u && c[off + 3] == 1u &&
                        cur_if == UAC_AS_IF)        /* (the stems' format: the playback input keeps 48000) */
                        fmt &= (c[off + 8] | c[off + 9] << 8 | c[off + 10] << 16) == 44100;
                }
            }
            stem_rate_mode = 0, usb_layout_mode = UL_SYNTHS;
            check("STEM RATE 44.1: bcdDevice x.8x .. x.Ax, string 3 -D4 -Y4 -A4", ok);
            check("STEM RATE 44.1: the type I format 6 x 24 bit at 44100 only", fmt);
        }
#endif
    }
}

static int32_t s24(const uint8_t *p) { return (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8; }

#if !T_STEMS
static void test_packets(void)
{
    static const uint32_t P[5] = {200, 160, 120, 100, 80};
    uint32_t k, i, ch, f = 0, frames = 0, nmin = 99, nmax = 0, bytes_max = 0, bad = 0, lowb = 1;
    int32_t prev = 0;
    for (k = 0; k < 10000u; k++) {
        uint32_t n = uac6_probe_packet(ep4tx6);
        nmin = n < nmin ? n : nmin, nmax = n > nmax ? n : nmax;
        bytes_max = n * 18u > bytes_max ? n * 18u : bytes_max;
        for (i = 0; i < n; i++, f++) {
            const uint8_t *fr = ep4tx6 + 18u * i;
            int32_t ctr = s24(fr + 15);
            if (f && ((uint32_t)((ctr - prev) >> 3) & 0xFFFFFu) != 1u)
                bad++;
            prev = ctr;
            if ((uint32_t)((ctr + (1 << 22)) >> 3) != (f & 0xFFFFFu))
                bad++;
            for (ch = 0; ch < 5u; ch++) {
                uint32_t ph = f % P[ch], t = ph < P[ch] / 2u ? ph : P[ch] - ph;
                if (T_UIN && (ch == 3u || ch == 4u))
                    continue;                           /* (the playback probe: the loopback there, test_uin_loopback) */
                int32_t v = (int32_t)((t << 22) / P[ch]) - (1 << 20), got = s24(fr + 3u * ch);
                v = (v & ~0xFF) | (int32_t)(ch + 1u);
                bad += got != v;
                lowb &= (got & 0xFF) == (int32_t)(ch + 1u);
            }
        }
        frames += n;
    }
    check("packets of 47..49 frames", nmin == 47u && nmax == 49u);
    check("48 frames a ms on average (10 s: 480000)", frames == 480000u);
    check("never over EP4's 882 bytes, and the largest packet sent", bytes_max == 882u);
    check("channel 5 counts every frame, channels 0..4 follow it", bad == 0);
    check("the low bytes there (24-bit samples)", lowb);
    check("the probe's counters: packets and frames", uac.pkts == 10000u && uac.frames == 480000u);
}
#else
#define ST_SECS 3u
static int32_t got[ST_SECS * 48000u + 4000u][6];
/* renders (256 frames, 44.1 kHz) against packets (1 kHz), level m; returns the frames received */
static uint32_t stems_run(int32_t m, int32_t amp_dc)
{
    static int32_t c[6][32];
    const int32_t *cp[6] = {c[0], c[1], c[2], c[3], c[4], c[5]};
    uint64_t t_ns = 0, render_ns = 0, pkt_ns = 0, end = (uint64_t)ST_SECS * 1000000000u;
    uint32_t g = 0, nf = 0, b, i, k;
    memset(&uac, 0, sizeof uac);
    ua_w = ua_r = 0;
    usb.up = 1, usb.config = 1, uac.alt = 1, uac.flowing = 1;
    uac_rate_set(48000);
    while (t_ns < end) {
        if (t_ns >= render_ns) {
            uac_render_start();
            for (b = 0; b < HALF_FRAMES; b += 32u) {
                for (i = 0; i < 32u; i++, g++) {
                    double ts = g / 44100.0;
                    c[0][i] = (int32_t)lrint(8000.0 * sin(2 * M_PI * 1000.0 * ts));
                    c[1][i] = 0;
                    c[2][i] = 1000;
                    c[3][i] = -2000;
                    c[4][i] = (int32_t)lrint(4000.0 * sin(2 * M_PI * 3000.0 * ts));
                    c[5][i] = amp_dc;
                }
                uac6_tap(cp, 32u, m, m);
            }
            render_ns += (uint64_t)HALF_FRAMES * 1000000000u / 44100u;
        }
        if (t_ns >= pkt_ns) {
            uint32_t n = uac6_ring_packet(ep4tx6);
            for (i = 0; i < n && nf < sizeof got / sizeof got[0]; i++, nf++)
                for (k = 0; k < 6u; k++)
                    got[nf][k] = s24(ep4tx6 + 18u * i + 3u * k);
            pkt_ns += 1000000u;
        }
        t_ns = render_ns < pkt_ns ? render_ns : pkt_ns;
    }
    return nf;
}
/* the channels shared between the cores (fx.c stem_k): any split, block by block, the same bytes as all 6 at once */
static void test_split(void)
{
    static int32_t c[6][32];
    static uint8_t whole[UA_N * U6_FB];
    const int32_t *cp[6] = {c[0], c[1], c[2], c[3], c[4], c[5]};
    uint32_t pass, b, i, ch, rnd, w0 = 0, nb = 0;
    for (pass = 0; pass < 2u; pass++) {
        memset(&uac, 0, sizeof uac);
        ua_w = ua_r = 0;
        usb.up = 1, usb.config = 1, uac.alt = 1, uac.flowing = 1;
        uac_rate_set(48000);
        uac_render_start();
        w0 = ua_w;
        rnd = 777u;
        for (b = 0; b < 10u; b++) {
            uint32_t k;
            for (i = 0; i < 32u; i++)
                for (ch = 0; ch < 6u; ch++) {
                    rnd = rnd * 1664525u + 1013904223u;
                    c[ch][i] = (int32_t)(rnd >> 12) - (1 << 19);
                }
            rnd = rnd * 1664525u + 1013904223u;
            k = (rnd >> 16) % 7u;                       /* core 1's share this block, 0..6 */
            if (!pass) {
                uac6_tap(cp, 32u, 4096, 3000);
            } else if (uac6_room(32u)) {
                uac6_tap_ch(cp, 32u, 4096, 3000, 0, k);   /* (core 1) */
                uac6_tap_ch(cp, 32u, 4096, 3000, k, 6);   /* (core 0) */
                uac6_commit(32u);
            }
        }
        nb = ua_w - w0;
        if (!pass)
            for (i = 0; i < nb; i++)
                memcpy(whole + i * U6_FB, u6_ring + ((w0 + i) & (UA_N - 1u)) * U6_FB, U6_FB);
    }
    for (i = 0, b = 0; i < nb; i++)
        b += memcmp(whole + i * U6_FB, u6_ring + ((w0 + i) & (UA_N - 1u)) * U6_FB, U6_FB) != 0;
    printf("   %u frames compared\n", nb);
    check("the channels shared between the cores, any split: the same bytes as on one core", b == 0 && nb > 340u);
}
/* the 32-bit split sums against 64-bit ones: random full-range input and levels, every output sample; STEM FILTER
 * FULL (12 taps) or LIGHT (8) */
static void test_exact(int light)
{
    static int32_t c[6][32], hist[6][32];
    const int32_t *cp[6] = {c[0], c[1], c[2], c[3], c[4], c[5]};
    const uint32_t nt = light ? UAC_FIR8_N : UAC_FIR_N;
    const int16_t *taps = light ? &r6_taps8[0][0] : &r6_taps[0][0];
    uint32_t rnd = 12345u, b, i, ch, k, ph = 0, h = 0, w0, out = 0, bad = 0;
    stem_light = (uint8_t)light;
    memset(&uac, 0, sizeof uac);
    ua_w = ua_r = 0;
    usb.up = 1, usb.config = 1, uac.alt = 1, uac.flowing = 1;
    uac_rate_set(48000);
    uac_render_start();                                 /* primes the ring (silence), the resampler from 0 */
    w0 = ua_w;
    memset(hist, 0, sizeof hist);
    for (b = 0; b < 10u; b++) {
        int32_t m0, m1;
        rnd = rnd * 1664525u + 1013904223u, m0 = (int32_t)(rnd >> 20);   /* 0 .. 4095 */
        rnd = rnd * 1664525u + 1013904223u, m1 = (int32_t)(rnd >> 20);
        for (i = 0; i < 32u; i++)
            for (ch = 0; ch < 6u; ch++) {
                rnd = rnd * 1664525u + 1013904223u;
                c[ch][i] = (int32_t)(rnd >> 12) - (1 << 19);   /* +-2^19 */
            }
        uac6_tap(cp, 32u, m0, m1);
        for (i = 0; i < 32u; i++) {                     /* the reference, in 64 bits (an overflow would show) */
            int32_t m = m0 + (((m1 - m0) * (int32_t)i) >> 5);
            for (ch = 0; ch < 6u; ch++) {
                int32_t x = c[ch][i] >> 1;
                hist[ch][h & 31u] = x > 262143 ? 262143 : x < -262144 ? -262144 : x;
            }
            h++;
            do {
                const uint8_t *f = u6_ring + ((w0 + out) & (UA_N - 1u)) * U6_FB;
                for (ch = 0; ch < 6u; ch++) {
                    int64_t a = 2048, v;
                    for (k = 0; k < nt; k++)
                        a += (int64_t)taps[ph * nt + k] * hist[ch][(h - nt + k) & 31u];
                    v = ((a >> 12) * m) >> 6;
                    v = v > 8388607 ? 8388607 : v < -8388608 ? -8388608 : v;
                    bad += s24(f + 3u * ch) != (int32_t)v;
                }
                out++;
                ph += UAC_FIR_M;
            } while (ph < UAC_FIR_L);
            ph -= UAC_FIR_L;
        }
    }
    printf("   %u frames compared\n", out);
    check(light ? "LIGHT (8 taps): every sample as 64-bit sums give it (random full-range input and levels)" :
                  "one 32-bit multiply a tap: every sample as 64-bit sums give it (random full-range input and levels)",
          bad == 0 && out == ua_w - w0 && out > 340u);
    {   /* the taps in RAM: each phase sums to exactly 4096 (unity), each tap near the table's */
        const int16_t *src = light ? &UAC_FIR8[0][0] : &UAC_FIR[0][0];
        uint32_t p, j, unity = 1, near = 1;
        for (p = 0; p < UAC_FIR_L; p++) {
            int32_t sum = 0;
            for (j = 0; j < nt; j++) {
                int32_t t = p < UAC_FIR_L / 2u ? src[p * nt + j] : src[(UAC_FIR_L - 1u - p) * nt + nt - 1u - j];
                sum += taps[p * nt + j];
                near &= abs(taps[p * nt + j] * 8 - t) <= 4 + 8 * 6;   /* (the remainder on the largest: a few steps) */
            }
            unity &= sum == 4096;
        }
        check(light ? "LIGHT's taps in RAM: every phase sums to unity, each tap as smk_uac_fir8.h's" :
                      "the taps in RAM: every phase sums to unity, each tap as uac_fir.h's within the rounding",
              unity && near);
    }
    stem_light = 0;
}
static void test_stems(void)
{
    uint32_t nf, i, from = 9600, zc = 0, und, ovr;   /* (from 0.2 s: primed, the resampler settled) */
    int32_t pk0 = 0, pk4 = 0, e2 = 0, e3 = 0, e5 = 0, z1 = 0;
    nf = stems_run(4096, 30000);
    und = uac.underruns, ovr = uac.overruns;
    for (i = from; i < nf; i++) {
        int32_t d2 = got[i][2] - 32000, d3 = got[i][3] + 64000, d5 = got[i][5] - 960000;
        e2 = abs(d2) > e2 ? abs(d2) : e2, e3 = abs(d3) > e3 ? abs(d3) : e3, e5 = abs(d5) > e5 ? abs(d5) : e5;
        z1 = abs(got[i][1]) > z1 ? abs(got[i][1]) : z1;
        pk0 = abs(got[i][0]) > pk0 ? abs(got[i][0]) : pk0;
        pk4 = abs(got[i][4]) > pk4 ? abs(got[i][4]) : pk4;
        if (i + 48000u <= nf && i < from + 48000u && got[i][0] < 0 && got[i + 1][0] >= 0)
            zc++;                                       /* rising zero crossings over one second of 48 kHz frames */
    }
    printf("   frames %u, underruns %u, overruns %u, DC errors %d %d %d, peaks %d %d, crossings %u\n", nf, und, ovr,
           e2, e3, e5, pk0, pk4, zc);
    check("SYNTHS+RET: 48 frames a ms (3 s: 144000 +-100)", nf > 143900u && nf < 144100u);
    check("no underrun, no overrun", und == 0 && ovr == 0);
    check("DC x 32: the stereo output's full scale at -18 dBFS (24 bit)", e2 <= 2 && e3 <= 2 && e5 <= 3);
    check("a silent channel stays 0", z1 == 0);
    check("1 kHz at 8000 -> 256000 (+-2 %), 3 kHz at 4000 -> 128000 (+-3 %)",
          abs(pk0 - 256000) <= 5120 && abs(pk4 - 128000) <= 3840);
    check("1 kHz: 1000 rising zero crossings in a second of 48 kHz frames (none lost or doubled)", zc >= 999u && zc <= 1001u);
    nf = stems_run(2048, 30000);                        /* half the level */
    e2 = 0;
    for (i = from; i < nf; i++) {
        int32_t d2 = got[i][2] - 16000;
        e2 = abs(d2) > e2 ? abs(d2) : e2;
    }
    check("the level m: 2048 halves it", e2 <= 2);
    nf = stems_run(4096, 300000);                       /* a channel past the ceiling */
    e5 = 0;
    for (i = from; i < nf; i++)
        e5 = got[i][5] > e5 ? got[i][5] : e5;
    check("the 24-bit ceiling: 300000 x 32 clamps at 8388607", e5 == 8388607);
}
/* STEM FILTER = LIGHT, and STEM RATE 44.1 (no resampling): the same stems, their level and frequency */
static void test_stems_light_441(void)
{
    uint32_t pass;
    for (pass = 0; pass < 2u; pass++) {
        uint32_t nf, i, fps = pass ? 44100u : 48000u, from = fps / 5u, zc = 0, und, ovr;
        int32_t pk0 = 0, e2 = 0, e3 = 0, z1 = 0;
        stem_light = (uint8_t)!pass, stem_rate_mode = (uint8_t)pass;
        nf = stems_run(4096, 30000);
        und = uac.underruns, ovr = uac.overruns;
        for (i = from; i < nf; i++) {
            int32_t d2 = got[i][2] - 32000, d3 = got[i][3] + 64000;
            e2 = abs(d2) > e2 ? abs(d2) : e2, e3 = abs(d3) > e3 ? abs(d3) : e3;
            z1 = abs(got[i][1]) > z1 ? abs(got[i][1]) : z1;
            pk0 = abs(got[i][0]) > pk0 ? abs(got[i][0]) : pk0;
            if (i + fps <= nf && i < from + fps && got[i][0] < 0 && got[i + 1][0] >= 0)
                zc++;
        }
        printf("   %s: frames %u, underruns %u, overruns %u, DC errors %d %d, peak %d, crossings %u\n",
               pass ? "44.1" : "LIGHT", nf, und, ovr, e2, e3, pk0, zc);
        if (pass) {
            check("STEM RATE 44.1: 44.1 frames a ms (3 s: 132300 +-100), no underrun or overrun",
                  nf > 132200u && nf < 132400u && und == 0 && ovr == 0);
            check("STEM RATE 44.1: DC x 32 exactly, silence 0, 1 kHz at 8000 -> 256000 (+-0.5 %), 1000 crossings",
                  e2 == 0 && e3 == 0 && z1 == 0 && abs(pk0 - 256000) <= 1280 && zc >= 999u && zc <= 1001u);
        } else {
            check("STEM FILTER LIGHT: 48 frames a ms, no underrun or overrun", nf > 143900u && nf < 144100u && und == 0 && ovr == 0);
            check("STEM FILTER LIGHT: DC x 32, silence 0, 1 kHz at 8000 -> 256000 (+-2 %), 1000 crossings",
                  e2 <= 2 && e3 <= 2 && z1 == 0 && abs(pk0 - 256000) <= 5120 && zc >= 999u && zc <= 1001u);
        }
    }
    stem_light = 0, stem_rate_mode = 0;
}
#endif

#if T_UIN
static void test_uin_desc(void)
{
    const uint8_t *c;
    uint16_t cl;
    uint32_t off, cur_if = 0, alt = 0, nif = 0, seen = 0, ok_seq = 1;
    int hdr = 0, it3 = 0, ot4 = 0, fmt = 0, ep = 0, cdc = 0, iad = 0, alts = 0;
    get_desc(0x0200, &c, &cl);
    for (off = 0; off < cl; off += c[off]) {
        const uint8_t *p = c + off;
        if (p[1] == 2u)
            nif = p[4];
        if (p[1] == 0x0Bu && p[2] == 0u)
            iad = p[3] == 4u;                                   /* audio + MIDI: interfaces 0..3 */
        if (p[1] == 0x0Bu && p[2] != 0u)
            cdc = p[2] == 4u && p[3] == 2u;                      /* CDC: 4, 5 */
        if (p[1] == 4u) {
            cur_if = p[2], alt = p[3];
            if (alt == 0u) {
                ok_seq &= cur_if == seen;                       /* each interface's alt 0, in order */
                seen++;
            }
            if (cur_if == UIN_AS_IF)
                alts |= 1u << alt;
        }
        if (p[1] == 0x24u && p[2] == 1u && p[0] == 11u && cur_if == 0u)
            hdr = le16(p + 5) == 53u && p[7] == 3u && p[8] == UAC_AS_IF && p[9] == UIN_AS_IF && p[10] == 1u;
        if (p[0] == 12u && p[1] == 0x24u && p[2] == 2u && p[3] == 3u)
            it3 = le16(p + 4) == 0x0101u && p[7] == 2u;
        if (p[0] == 9u && p[1] == 0x24u && p[2] == 3u && p[3] == 4u)
            ot4 = le16(p + 4) == 0x0301u && p[7] == 3u;
        if (p[1] == 0x24u && p[2] == 2u && p[3] == 1u && p[0] >= 8u && cur_if == UIN_AS_IF)
            fmt = p[0] == 11u && p[4] == 2u && p[5] == 2u && p[6] == 16u && p[7] == 1u &&
                  (p[8] | p[9] << 8 | p[10] << 16) == 48000;
        if (p[0] == 9u && p[1] == 5u && p[2] == 0x04u)
            ep = cur_if == UIN_AS_IF && alt == 1u && p[3] == 0x09u && le16(p + 4) == 196u && p[6] == 1u;
    }
    check("playback: 6 interfaces, each in order (audio 0-3, CDC 4-5)", nif == 6u && seen == 6u && ok_seq);
    check("playback: the audio IAD 4 interfaces, CDC's IAD from 4", iad && cdc);
    check("playback: AC header 53 bytes, collection 2, 3, 1", hdr);
    check("playback: input terminal 3 USB streaming 2 ch, output terminal 4 speaker from 3", it3 && ot4);
    check("playback: interface 3 alt 0 and alt 1, format 2 x 16 bit 48000", alts == 3 && fmt);
    check("playback: EP 0x04 OUT isochronous adaptive, 196 bytes, 1 ms", ep);
}

#if !T_STEMS
static void test_uin_loopback(void)
{
    uint32_t pk, i, f = 0, want = 0, n, ok = 1, others = 1, frames = 0;
    uint8_t d[U6_MAXP];
    uint32_t f0 = u6_f;
    uin_stream(1);
    for (pk = 0; pk < 3000u; pk++) {
        for (i = 0; i < 48u; i++, f++)                          /* the host: 48 frames a ms */
            uin_ring[(uin_w + i) & (UIN_N - 1u)] = (f & 0xFFFFu) | ((0xFFFFu - (f & 0xFFFFu)) << 16);
        uin_w += 48u;
        n = uac6_probe_packet(d);
        for (i = 0; i < n; i++) {
            const uint8_t *q = d + i * U6_FB;
            int32_t l = s24(q + 9), r = s24(q + 12);
            if (pk > 4u) {                                      /* (the first 2 ms: silence while the ring fills) */
                if (!want && l)
                    want = (uint32_t)(l >> 8) & 0xFFFFu;
                ok &= ((uint32_t)(l >> 8) & 0xFFFFu) == (want & 0xFFFFu) && (l & 0xFF) == 0 &&
                      ((uint32_t)(r >> 8) & 0xFFFFu) == (0xFFFFu - (want & 0xFFFFu)) && (r & 0xFF) == 0;
                want++;
            }
            others &= (s24(q) & 0xFF) == 1 && (s24(q + 3) & 0xFF) == 2 && (s24(q + 6) & 0xFF) == 3 &&
                      s24(q + 15) == (int32_t)(((f0 + frames) & 0xFFFFFu) << 3) - (1 << 22);
            frames++;
        }
    }
    check("loopback: every frame received, in order, on channels 3 (L) and 4 (R), 16 bit << 8", ok && want > 140000u);
    check("loopback: channels 0..2 and the counter on 5 as before", others);
    check("loopback: no underrun or resync (the host's 48 a ms both ways)", !uin.lb_under && !uin.lb_resync);
    uin_stream(0);
}

#endif

static void test_uin_monitor(void)
{
    static int32_t out[2u * 256u];
    double acc_in = 0.0, ph = 0.0, s2 = 0.0, s2w = 0.0;
    uint32_t r, i, k, zc = 0, rendered = 0, prev_neg = 0;
    const double fs_eng = 44117.6;                              /* the engines' clock, a little fast */
#if T_STEMS
    uin_mode = 1;                                               /* (the vocoder build: AUDIO IN starts OFF) */
#endif
    uin_stream(1);
    for (r = 0; r < 1723u; r++) {                               /* 10 s of renders */
        acc_in += 256.0 * 48000.0 / fs_eng;                      /* the frames the host sent meanwhile */
        while (acc_in >= 1.0) {
            int16_t v = (int16_t)lrint(16000.0 * sin(ph));
            ph += 2.0 * M_PI * 1000.0 / 48000.0;
            uin_ring[uin_w & (UIN_N - 1u)] = (uint16_t)v | (uint32_t)(uint16_t)v << 16;
            uin_w++;
            acc_in -= 1.0;
        }
        memset(out, 0, sizeof out);
        for (k = 0; k < 256u; k += 32u)
            uin_take(out + 2u * k, 0, 32u);           /* (the monitor's resampler: the vocoder build's audio track
                                                       * takes it mono, uin_block) */
        if (r > 172u)                                           /* after 1 s: measured */
            for (i = 0; i < 256u; i++, rendered++) {
                double x = out[2u * i];
                s2 += x * x;
                s2w += x * x;
                if (x >= 0 && prev_neg)
                    zc++;
                prev_neg = x < 0;
            }
    }
    {
        double rms = sqrt(s2 / rendered), f = zc * fs_eng / rendered;
        printf("   monitor: fill %u..%u (target %u), step %u, rms %.0f (want %.0f), %.1f Hz, under %u, resync %u\n", uin.mon_lo, uin.mon_hi,
               UIN_MON_T, uin.mon_step, rms, 8000.0 / sqrt(2.0), f, uin.mon_under, uin.mon_resync);
        check("monitor: the servo holds the fill clear of both ends (a render's ~279 frames at once), no underrun or resync",
              uin.mon_lo >= 64u && uin.mon_hi <= UIN_N - 2u * UIN_MAXF && !uin.mon_under && !uin.mon_resync);
        check("monitor: 1 kHz in, 1 kHz out (+-0.5 %), at -6 dB (+-0.2 dB)",
              fabs(f - 1000.0) < 5.0 && fabs(20.0 * log10(rms / (8000.0 / sqrt(2.0)))) < 0.2);
    }
    (void)s2w;
    uin_stream(0);
}
#endif

int main(void)
{
    printf(T_STEMS ? "-- the SMK-37 stems, SYNTHS+RET, CDC %s\n" : "-- the SMK-37 stems probe (SMK_UAC_PROBE), CDC %s\n",
           T_CDC ? "on" : "off");
    test_descriptors();
#if T_STEMS
    test_exact(0);
    test_exact(1);
    test_split();
    test_stems();
    test_stems_light_441();
#else
    test_packets();
#endif
#if T_UIN
    test_uin_desc();
#if !T_STEMS
    test_uin_loopback();                              /* (the stems probe's packets: the loopback) */
#endif
    test_uin_monitor();
#endif
    printf(fails ? "UAC6 TEST FAILED (%d)\n" : "uac6: all ok\n", fails);
    return fails != 0;
}
