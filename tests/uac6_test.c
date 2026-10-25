/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the SMK-37 stems probe in src/usb.c (SMK_UAC_PROBE=1, smk37-port.yaml follow_up.stems first_tests):
 *   descriptors  the configuration walked as a host does (lengths, total), the input terminal 6 channels named by
 *                strings 4..9, the type I format 6 x 24 bit (3-byte subframes) at 48000 only, EP4 882 bytes,
 *                bcdDevice x.7x; with CDC (-DT_CDC=1) and without (-DT_CDC=0)
 *   packets      uac6_packet over 10 s: 47..49 frames, 48 a ms on average, never over EP4's size; every sample as
 *                uac6_check.py (smk37-devtools) expects it: the counter on channel 5 steps by one, the triangles of
 *                channels 0..4 follow it, the low bytes are there (24 bit, not 16)
 * The SIE is never touched. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define BOARD_SMK37 1
#define SMK_UAC_PROBE 1
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
    static const char *const NAMES[6] = {"KICK", "SNARE", "HATS", "PERC", "RET L", "RET R"};
#if T_CDC
    usb_cdc_on = 1;
#endif
    get_desc(0x0100, &dv, &dl);
    get_desc(0x0200, &c, &cl);
    check("device descriptor: 18 bytes, bcdDevice x.7x (the probe's own version)",
          dl == 18 && dv[0] == 18 && (dv[12] & 0xF0u) == 0x70u && dv[13] == 3);
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
    check("strings 4..9: KICK SNARE HATS PERC RET L RET R", names_ok);
    check("string 10: none", !get_desc(0x0300 | 10u, &s, &sl));
    {   /* the serial number (SMK_SN): string 3, the flash's unique ID folded, the probe's layout tag */
        static const uint8_t UID[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 1, 2, 3, 4, 5, 6, 7, 8};
        const char *want = "SMK37-C9C2507299B8-P6";   /* byte k of the fold: the XOR of bytes k, k + 6, k + 12 */
        uint32_t k, n = (uint32_t)strlen(want);
        int sn_ok;
        memcpy(smk_uid, UID, 16);
        sn_ok = get_desc(0x0303, &s, &sl) && sl == 2u + 2u * n && s[0] == sl && s[1] == 3u;
        for (k = 0; sn_ok && k < n; k++)
            sn_ok = s[2u + 2u * k] == (uint8_t)want[k] && s[3u + 2u * k] == 0;
        get_desc(0x0100, &dv, &dl);
        check("device descriptor: iSerialNumber 3", dv[16] == 3u);
        check("string 3: SMK37-<the unique ID folded>-P6", sn_ok);
    }
}

static int32_t s24(const uint8_t *p) { return (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8; }

static void test_packets(void)
{
    static const uint32_t P[5] = {200, 160, 120, 100, 80};
    uint32_t k, i, ch, f = 0, frames = 0, nmin = 99, nmax = 0, bytes_max = 0, bad = 0, lowb = 1;
    int32_t prev = 0;
    for (k = 0; k < 10000u; k++) {
        uint32_t n = uac6_packet(ep4tx6);
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

int main(void)
{
    printf("-- the SMK-37 stems probe (SMK_UAC_PROBE), CDC %s\n", T_CDC ? "on" : "off");
    test_descriptors();
    test_packets();
    printf(fails ? "UAC6 TEST FAILED (%d)\n" : "uac6: all ok\n", fails);
    return fails != 0;
}
