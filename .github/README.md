# SLOOP for the M-Vave SMK-37

**SLOOP**, the groovebox firmware for the Hügelton FM-1, running on the **M-Vave SMK-37** (also sold as the SMK-37
Elite, MKE-P37 and Starrykey 37 Play: the same chip). It has the whole of SLOOP: four tracks, the step sequencer, the
synth engines, the punch-in effects and the web editor, and it uses the keyboard's 37 keys, 16 RGB pads, faders and
wheels. It comes as **two builds**, each with its own installer, editor and manuals:

<table>
<tr>
<th width="50%">Audio stems</th>
<th width="50%">Mewcoder (vocoder)</th>
</tr>
<tr valign="top">
<td>

SLOOP 2.8.1 with **USB audio stems** for a DAW, punch-ins on every stem, the stem punch-in page, sequencer effects and
DAW mode. Every SLOOP engine and the built-in samples.

**Models:** SMK-37 Pro, Elite, MKE-P37, Starrykey 37 Play

</td>
<td>

SLOOP 2.8.4 with **your voice in over USB audio** on track 3, played dry or through a **20-channel vocoder** modelled
on the Sennheiser VSM201, played from the keys and the sequencer. A lean build: some engines and the built-in samples
make room for it. **A harmoniser** (modelled on the Eventide H949) **is in development** for this build.

**Models:** SMK-37 Elite

</td>
</tr>
<tr valign="top">
<td>

<a href="https://milomeowzers.github.io/sloop-smk37/"><b>Install from the browser</b></a> ·
<a href="https://milomeowzers.github.io/sloop-smk37/elite/webapp/editor/">Web editor</a><br>
<a href="/SMK37.md"><b>The SMK-37 manual</b></a> ·
<a href="https://github.com/MiloMeowzers/sloop-smk37/releases/tag/v2.8.1-smk37.1">Release v2.8.1-smk37.1</a><br>
Branch <code>ci</code> (this page)

</td>
<td>

<a href="https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/installer/"><b>Install from the browser</b></a> ·
<a href="https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/editor/">Web editor</a><br>
<a href="https://github.com/MiloMeowzers/sloop-smk37/tree/mewcoder"><b>Mewcoder's page</b></a> ·
<a href="https://github.com/MiloMeowzers/sloop-smk37/blob/mewcoder/VOCODER.md">The vocoder manual</a> ·
<a href="https://github.com/MiloMeowzers/sloop-smk37/releases/tag/mewcoder-v0.1">Release mewcoder-v0.1</a><br>
Branch <code>mewcoder</code>

</td>
</tr>
</table>

> Unofficial firmware, not affiliated with or endorsed by M-Vave, Hügelton Instruments or SLOOP's author. No
> warranty: install at your own risk. **Back up the keyboard's flash first** (see [Install](#install)).

**Which one?** For a DAW and the full set of sounds, **Audio stems**. To sing or speak through the keyboard,
**Mewcoder**. You can move between them: install the other package. Installing never erases your projects, presets,
samples or settings; back them up with the editor first all the same, as the two builds are different SLOOP versions.

[SLOOP's own guide](/GUIDE.md) · [Building](/BUILDING.md)

## Audio stems: what it adds

- **The keyboard as SLOOP's panel.** 37 velocity keys, 16 pads (drum kit or steps), 4 faders, 8 knobs (FM-1 or SMK
  layout), pitch and mod wheels with a settable bend range, and TRS MIDI out.
- **USB audio stems.** Six mono channels, 24 bit, to the DAW: the drums in four groups, the synths, and the reverb /
  delay return. 48 or 44.1 kHz; a new layout re-plugs USB by itself, with no power cycle.
- **Punch-ins on the stems.** The rhythmic punch-ins (loops, stutter, reverse, stop, half, rewind, gate) play on every
  stem with seamless loop joins. The stitch length and curve are settable (HOME > PUNCH).
- **The stem punch-in page.** Hold FX, or lock it with FX + FADER BANK, and the pads become four colour rows of
  effects. FREEZE holds the reverb.
- **Sequencer effects.** FLIP (play backwards), LOOP BACK (2 bars down to 1/16, changeable while held) and RAMP UP /
  DOWN (tempo ramps, with the DAW staying the clock master). Each is recorded and replayed as MIDI.
- **DAW mode.** < + > turns the keyboard into a MIDI controller with an editable CC map.
- **Punch-in MIDI out.** Every punch-in sends a note on FX CH, so a DAW can record a performance and play it back.

All of it is in [SMK37.md](/SMK37.md), with a quick reference for the stem page.

## Mewcoder: what it adds

Everything above for the SMK-37 Elite, on SLOOP 2.8.4, plus:

- **An audio track.** Track 3 takes your voice (or any audio) from the computer over USB audio, played dry (AUDIO) or
  vocoded (VOCODER).
- **A 20-channel vocoder** modelled on the Sennheiser VSM201: per-channel emphasis, speech addition, silence bridging,
  voiced / unvoiced detection with noise replacement, a built-in saw carrier played from the keys and the sequencer,
  always polyphonic (chords are the classic sound).
- **The vocoder page:** the 20 channels on the knobs and pads, in three groups, with the VSM201's selectors on the pads.
- **USB > STEM RATE** at 16 bit, so the stems and the voice fit a single USB hub.
- **The web editor's Vocoder tab**, and the vocoder's settings kept with each project.
- **The build on the splash and ABOUT**, so you can tell which build is on the keyboard.

**Left out to make room:** the built-in samples (your own sample slots work), the TEXTURE, DIGITAL, LOFI, VOICE, TRIO
and NOISE engines, and full-rate delay (it runs at 22 kHz). It is also a console build: the keyboard shows up as a
serial port as well.

**In development: the harmoniser.** Your voice pitch-shifted into up to three harmony voices that keep its own sound,
modelled on the Eventide H949: its delay memory, feedback, REVERSE and REPEAT, its big ratio readout on the screen, the
pads as its switch row. It will come to Mewcoder when it is ready.

All of it is on [Mewcoder's page](https://github.com/MiloMeowzers/sloop-smk37/tree/mewcoder).

## Install

1. **Back up the whole 1 MiB flash** first: the calibration, the Bluetooth pairing and key_mac are your unit's own and
   no package contains them ([how](/SMK37.md#install)).
2. Install with the browser (Chrome or Edge, over USB): the
   [Audio stems installer](https://milomeowzers.github.io/sloop-smk37/) (pick your model) or the
   [Mewcoder installer](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/installer/) (Elite).
   Or download a package from [Releases](https://github.com/MiloMeowzers/sloop-smk37/releases) and run
   `python3 tools/fm1_install.py PACKAGE.fwsc` (check its SHA-256 first).
3. **Use the editor that matches the build:** each installer has its own, and the release holds it as one file.
4. **Going back:** M-Vave's own updater (M-UPGRADE) installs the stock firmware as usual. The port never writes the
   keyboard's protected flash.

Your projects, presets, samples and settings are kept when you update or change build.

## Provenance

Everything here descends from **Felucca**: SLOOP is built on it, and genkigenki keeps the same SMK-37 layer for Felucca
itself ([genkigenki/Felucca](https://github.com/genkigenki/Felucca) branch `smk37`, hugelton/Felucca#237).

| Part | From | Licence |
|---|---|---|
| Felucca, the root: the FM-1 firmware platform (HAL, engines, update path) SLOOP and the SMK-37 work build on | [hugelton/Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (@kurogedelic) / Hügelton Instruments | GPL-3.0-only |
| SLOOP, the firmware | [isod89/sloop-fm1](https://github.com/isod89/sloop-fm1), based on Felucca | GPL-3.0-only |
| The SMK-37 board layer (input, screen, pad LEDs, flash map, update path, DAW mode) | M. Geisler, [genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1) branch `smk37` (isod89/sloop-fm1#112); its commits keep their authorship here | GPL-3.0-only |
| The porting method: the playbook, the hardware docs, the `smk37-port.yaml` template | M. Geisler, [genkigenki/fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) | see that repository |
| Carrying the layer to 2.8.1, the user sample slots, punch-in MIDI out, the editor's slot read-back, the USB audio stems, STEM FX, the stem punch-in page, the sequencer effects | the commits on `smk37`, `editor-slot-readback` and `stems` | GPL-3.0-only |
| Mewcoder: SLOOP 2.8.4, the USB audio input, the audio track, the vocoder and its page, STEM RATE 16 bit, the build display | the commits on `mewcoder` | GPL-3.0-only |

The decisions behind the port, and what was tested on a real unit, are in [`smk37-port.yaml`](/smk37-port.yaml).

## Branches

The work is a stack, so each part can be offered upstream on its own:

- **`smk37`**: the port itself. Everything SMK-37-specific is behind `BOARD_SMK37`, and the FM-1 build is unchanged
  byte for byte.
- **`editor-slot-readback`**: the web editor reads a user slot back from the keyboard.
- **`stems`**: the USB audio stems and everything built on them (STEM FX, the stem punch-in page, the sequencer
  effects). SMK-37 only.
- **`ci`** (the default branch): `stems` plus this page, the build workflow and the installer site. **The Audio stems
  build.** Never part of a pull request.
- **`mewcoder`**: `ci` brought to SLOOP 2.8.4, plus the USB audio input, the audio track and the vocoder (behind
  `SMK_UAC_IN_PROBE` and `SMK_VOC_LEAN`: the other builds are unchanged, the FM-1 byte for byte). **The Mewcoder
  build**, with its own page and manuals. The harmoniser is developed separately and will land here.

## Tags

| Tag | What |
|---|---|
| `v2.8.1-smk37.1` | the first Audio stems release |
| `stems-baseline` | the current Audio stems baseline (the build on the installer site) |
| `mewcoder-v0.1` | the first Mewcoder release (the vocoder) |
| `mewcoder-baseline` | the current Mewcoder baseline (the build on the installer site); `smk37-voc-baseline` is its earlier name |

## Builds and tests

Each push to `ci` builds, in one run: upstream SLOOP 2.8.1 for the FM-1 (the baseline); the SMK-37 packages (Pro,
Elite, MKE, and an Elite build with a USB serial console); the FM-1 again from this source, which **must be
byte-identical** to the baseline or the run fails; the installer sites and the offline editor; and the host and web
tests, which fail the run on any failure the baseline does not have. The same run builds **Mewcoder** from the
`mewcoder` branch (the Elite vocoder package, its installer and its editor). The run's artifact holds the packages,
`packages.sha256`, `fm1-identical.txt` and the logs. Only once every check has passed does the run publish both
installer sites and their editors on [GitHub Pages](https://milomeowzers.github.io/sloop-smk37/). Mewcoder itself is
built and tested the same way before each of its baselines is published.

Tested on an SMK-37 Elite. The Pro, MKE-P37 and Starrykey builds of Audio stems differ only in the name the keyboard
reports, but have not been tried on those units: reports are welcome.

## Licence

GPL-3.0-only, as SLOOP and Felucca ([LICENSE](/LICENSE), [LICENSING.md](/LICENSING.md)). No M-Vave firmware or
other vendor material is in this repository.
