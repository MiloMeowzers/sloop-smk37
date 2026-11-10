# Mewcoder: SLOOP with a vocoder, for the M-Vave SMK-37 Elite

**Mewcoder** is one of the two builds of [SLOOP for the M-Vave SMK-37](https://github.com/MiloMeowzers/sloop-smk37)
(the other is **Audio stems**, on the default branch). It is SLOOP 2.8.4 with **your voice in over USB audio** on
track 3, played dry or through a **20-channel vocoder modelled on the Sennheiser VSM201**, played from the keys and the
sequencer. Everything else in SLOOP stays: four tracks, the sequencer, the punch-ins, the stems, the web editor.
**A harmoniser**, modelled on the Eventide H949, **is in development** for it.

> Unofficial firmware, not affiliated with or endorsed by M-Vave, Hügelton Instruments, Sennheiser, Eventide or
> SLOOP's author. No warranty: install at your own risk. **Back up the keyboard's flash first** (see [Install](#install)).

<p align="center">
<a href="https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/installer/"><b>Install from the browser</b></a> ·
<a href="https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/editor/"><b>Web editor</b></a> ·
<a href="/VOCODER.md"><b>The vocoder manual</b></a> ·
<a href="https://github.com/MiloMeowzers/sloop-smk37/releases/tag/mewcoder-v0.1">Release mewcoder-v0.1</a> ·
<a href="https://github.com/MiloMeowzers/sloop-smk37">Both builds</a>
</p>

**Model:** SMK-37 Elite only.

## What it adds

- **An audio track.** Track 3 takes your voice (or any audio) from the computer over USB audio: HOME > USB >
  AUDIO IN. It plays it dry (AUDIO) or vocoded (VOCODER).
- **A 20-channel vocoder** modelled on the Sennheiser VSM201 (1977): its 20 channels from 145 Hz to 6.8 kHz,
  per-channel emphasis, speech addition, silence bridging, voiced / unvoiced detection with noise replacement, the
  carrier's own level control.
- **The carrier:** a built-in saw played from the keys and the sequencer, with its own envelope and filter (FADER BANK
  B, FX K4). Always polyphonic while vocoding: chords are the classic sound. **MOD WHEEL** can be vibrato, bridge tilt
  or noise blend.
- **The vocoder page** (PATCH on the tracks screen): the 20 channels on the knobs and pads in three groups, the
  VSM201's selectors on the pads, the channels' levels as the pads' brightness.
- **USB > STEM RATE** at 16 bit, so the stems and the voice fit a single USB hub.
- **The web editor's Vocoder tab:** every setting and the 20 channels, kept with each project slot and in the full
  backup.
- **The build on the splash and ABOUT** (for example `2.8.4  49876d5  VOCODER`), so you can tell which build is on the
  keyboard.

## What it leaves out

Mewcoder is a lean build, to make room for the vocoder in the keyboard's memory and CPU:
- **No built-in samples:** the GM sample kits are not offered. Your own sample slots and kits work.
- **No TEXTURE, DIGITAL, LOFI, VOICE, TRIO or NOISE engines.**
- **The delay runs at 22 kHz.**
- **A console build:** the keyboard also shows up as a USB serial port.

If you need any of these, use **Audio stems** ([its installer](https://milomeowzers.github.io/sloop-smk37/)).

## Install

1. **Back up the whole 1 MiB flash** first: the calibration, the Bluetooth pairing and key_mac are your unit's own and
   no package contains them ([how](/SMK37.md#install)). Back up your projects with the editor's full backup as well.
2. Install with the [Mewcoder installer](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/installer/) (Chrome
   or Edge, over USB), or download `sloop-smk37-elite-vocoder.fwsc` from the
   [release](https://github.com/MiloMeowzers/sloop-smk37/releases/tag/mewcoder-v0.1) and run
   `python3 tools/fm1_install.py sloop-smk37-elite-vocoder.fwsc` from this branch (check its SHA-256 first; needs
   `pip install mido python-rtmidi`). The installer checks the package against the keyboard's model first.
3. **Use Mewcoder's editor**: [online](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/editor/), or the one
   file in the release. The Audio stems editor does not know the vocoder.
4. **Start:** connect USB to the computer, send your microphone to the SMK-37's audio output, HOME > USB > AUDIO IN: ON,
   HOME > VOCODER > MODE: VOCODER, select track 3 and play chords. The rest is in [the manual](/VOCODER.md).
5. **Going back:** install Audio stems from [its installer](https://milomeowzers.github.io/sloop-smk37/), or the stock
   firmware with M-Vave's own updater (M-UPGRADE). The port never writes the keyboard's protected flash.

Installing never erases your projects, presets, samples or settings.

## The manuals

| | |
|---|---|
| [VOCODER.md](/VOCODER.md) | **the vocoder:** getting sound in, the menus, the vocoder page, the carrier, saving, the editor, troubleshooting |
| [SMK37.md](/SMK37.md) | **the keyboard:** every control, the stems, the stem punch-in page, DAW mode (shared with Audio stems) |
| [GUIDE.md](/GUIDE.md) | SLOOP's own guide |
| [VOCODER-DESIGN.md](/VOCODER-DESIGN.md) | how the vocoder was built: the VSM201 modelling and its sources, the CPU and memory work, the tests |

## Known rough edges

- **AUTOCONTROL** can pop on loud starts: leave it OFF for now.
- **One carrier note sounds thin:** play chords (three or four notes) for the classic, rich sound.
- **The mod wheel half way up reads near full** on some units: HOME > SYSTEM > HARDWARE CALIBRATION.

## In development: the harmoniser

Your voice pitch-shifted into up to three harmony voices that **keep its own sound**, modelled on the **Eventide H949**
Harmonizer: its 400 ms memory, the MAIN and DELAY ONLY outputs, the feedback with its EQ, REVERSE and REPEAT, the big
PITCH RATIO readout on the screen and the front panel's switch rows on the pads. It is a track role (HARMONY on T1 or
T2), so the vocoder and the harmoniser can play together. It is being specified and measured on the unit before it is
built, and will land on this branch with its own manual.

## Branch and tags

- **`mewcoder`** (this branch): `ci` (Audio stems) brought to SLOOP 2.8.4, plus the USB audio input, the audio track
  and the vocoder, behind `SMK_UAC_IN_PROBE` and `SMK_VOC_LEAN`: the other builds are unchanged, the FM-1 byte for byte.
- **`mewcoder-v0.1`**: the first release. **`mewcoder-baseline`**: the current baseline, the build on the installer
  site (`smk37-voc-baseline` is its earlier name).

Built and tested as Audio stems is (the FM-1 byte-identical, the host and web tests) before each baseline is published.
Tested on an SMK-37 Elite.

## Provenance and licence

Built on [SLOOP](https://github.com/isod89/sloop-fm1) 2.8.4 by isod89, [Felucca](https://github.com/hugelton/Felucca)
by Leo Kuroshita / Hügelton Instruments, and the SMK-37 board layer by M. Geisler
([genkigenki](https://github.com/genkigenki/sloop-fm1)); the full table is on
[the main page](https://github.com/MiloMeowzers/sloop-smk37#provenance). The vocoder follows the Sennheiser VSM201's
published documents; the values taken from them are listed in the code's comments, and no vendor material is in this
repository. GPL-3.0-only ([LICENSE](/LICENSE), [LICENSING.md](/LICENSING.md)).
