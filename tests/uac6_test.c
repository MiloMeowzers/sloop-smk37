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
 *                x 64 (the stereo output's full scale at -12 dBFS), the sines' level and frequency (no frame lost or
 *                doubled), the level m scales, the 24-bit ceiling
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
#define CTL_LOG2 5                                /* (dsp.c: the mix's blocks of 32) */
#define FELUCCA_OTA 0
#define FELUCCA_CDC (T_CDC != 0)
#define FELUCCA_UAC 1
#ifndef HALF_FRAMES
#error "-DHALF_FRAMES=n (src/core.h; run_tests.sh passes it)"
#endif
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
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
          dl == 18 && dv[0] == 18 && (dv[12] & 0xF0u) == (T_STEMS ? 0x50u : 0x70u) && dv[13] == 3);
    check("configuration: wTotalLength = the bytes given", le16(c + 2) == cl);
    for (off = 0; off < cl; off += c[off]) {
        const uint8_t *p = c + off;
        if (p[0] < 2u || off + p[0] > cl) {
            lens_ok = 0;
            break;
        }
        if (p[0] == 12u && p[1] == 0x24u && p[2] == 2u)
            it_ok = p[7] == 6u && le16(p + 8) == 0 && p[10] == 4u;
        if (p[1] == 0x24u && p[2] == 2u && p[3] == 1u && p[0] >= 8u)
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
        const char *want = T_STEMS ? "SMK37-C9C2507299B8-SY" : "SMK37-C9C2507299B8-P6";   /* byte k of the fold: the
                                                         * XOR of bytes k, k + 6, k + 12 */
        uint32_t k, n = (uint32_t)strlen(want);
        int sn_ok;
        memcpy(smk_uid, UID, 16);
        sn_ok = get_desc(0x0303, &s, &sl) && sl == 2u + 2u * n && s[0] == sl && s[1] == 3u;
        for (k = 0; sn_ok && k < n; k++)
            sn_ok = s[2u + 2u * k] == (uint8_t)want[k] && s[3u + 2u * k] == 0;
        get_desc(0x0100, &dv, &dl);
        check("device descriptor: iSerialNumber 3", dv[16] == 3u);
        check(T_STEMS ? "string 3: SMK37-<the unique ID folded>-SY" : "string 3: SMK37-<the unique ID folded>-P6", sn_ok);
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
/* the 32-bit split sums against 64-bit ones: random full-range input and levels, every output sample */
static void test_exact(void)
{
    static int32_t c[6][32], hist[6][32];
    const int32_t *cp[6] = {c[0], c[1], c[2], c[3], c[4], c[5]};
    uint32_t rnd = 12345u, b, i, ch, k, ph = 0, h = 0, w0, out = 0, bad = 0;
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
                    for (k = 0; k < UAC_FIR_N; k++)
                        a += (int64_t)r6_taps[ph][k] * hist[ch][(h - UAC_FIR_N + k) & 31u];
                    v = ((a >> 12) * m) >> 5;
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
    check("one 32-bit multiply a tap: every sample as 64-bit sums give it (random full-range input and levels)",
          bad == 0 && out == ua_w - w0 && out > 340u);
    {   /* the taps in RAM: each phase sums to exactly 4096 (unity), each tap near uac_fir.h's */
        uint32_t p, j, unity = 1, near = 1;
        for (p = 0; p < UAC_FIR_L; p++) {
            int32_t sum = 0;
            for (j = 0; j < UAC_FIR_N; j++) {
                int32_t t = p < UAC_FIR_L / 2u ? UAC_FIR[p][j] : UAC_FIR[UAC_FIR_L - 1u - p][UAC_FIR_N - 1u - j];
                sum += r6_taps[p][j];
                near &= abs(r6_taps[p][j] * 8 - t) <= 4 + 8 * 6;   /* (the remainder on the largest: a few steps) */
            }
            unity &= sum == 4096;
        }
        check("the taps in RAM: every phase sums to unity, each tap as uac_fir.h's within the rounding", unity && near);
    }
}
static void test_stems(void)
{
    uint32_t nf, i, from = 9600, zc = 0, und, ovr;   /* (from 0.2 s: primed, the resampler settled) */
    int32_t pk0 = 0, pk4 = 0, e2 = 0, e3 = 0, e5 = 0, z1 = 0;
    nf = stems_run(4096, 30000);
    und = uac.underruns, ovr = uac.overruns;
    for (i = from; i < nf; i++) {
        int32_t d2 = got[i][2] - 64000, d3 = got[i][3] + 128000, d5 = got[i][5] - 1920000;
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
    check("DC x 64: the stereo output's full scale at -12 dBFS (24 bit)", e2 <= 2 && e3 <= 2 && e5 <= 3);
    check("a silent channel stays 0", z1 == 0);
    check("1 kHz at 8000 -> 512000 (+-2 %), 3 kHz at 4000 -> 256000 (+-3 %)",
          abs(pk0 - 512000) <= 10240 && abs(pk4 - 256000) <= 7680);
    check("1 kHz: 1000 rising zero crossings in a second of 48 kHz frames (none lost or doubled)", zc >= 999u && zc <= 1001u);
    nf = stems_run(2048, 300000);                       /* half the level; a channel past the ceiling */
    e2 = 0, e5 = 0;
    for (i = from; i < nf; i++) {
        int32_t d2 = got[i][2] - 32000;
        e2 = abs(d2) > e2 ? abs(d2) : e2;
        e5 = got[i][5] > e5 ? got[i][5] : e5;
    }
    check("the level m: 2048 halves it", e2 <= 2);
    check("the 24-bit ceiling: 300000 x 32 clamps at 8388607", e5 == 8388607);
}
#endif

int main(void)
{
    printf(T_STEMS ? "-- the SMK-37 stems, SYNTHS+RET, CDC %s\n" : "-- the SMK-37 stems probe (SMK_UAC_PROBE), CDC %s\n",
           T_CDC ? "on" : "off");
    test_descriptors();
#if T_STEMS
    test_exact();
    test_split();
    test_stems();
#else
    test_packets();
#endif
    printf(fails ? "UAC6 TEST FAILED (%d)\n" : "uac6: all ok\n", fails);
    return fails != 0;
}
