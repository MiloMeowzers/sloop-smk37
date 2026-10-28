# SLOOP for the M-Vave SMK-37

**SLOOP 2.8.1**, the groovebox firmware for the Hügelton FM-1, running on the **M-Vave SMK-37** (also sold as the
SMK-37 Elite, MKE-P37 and Starrykey 37 Play: the same chip). It has the whole of SLOOP: four tracks, the step
sequencer, the synth engines, the 27 punch-in effects and the web editor. It also uses the keyboard's 37 keys, 16 RGB
pads, faders and wheels, and adds USB audio stems for a DAW.

> Unofficial firmware, not affiliated with or endorsed by M-Vave, Hügelton Instruments or SLOOP's author. No
> warranty: install at your own risk. **Back up the keyboard's flash first** (see [Install](#install)).

<p align="center">
<a href="https://milomeowzers.github.io/sloop-smk37/"><b>Install from the browser</b></a> ·
<a href="https://milomeowzers.github.io/sloop-smk37/elite/webapp/editor/">Web editor</a> ·
<a href="https://github.com/MiloMeowzers/sloop-smk37/releases">Releases</a> ·
<a href="/SMK37.md"><b>The SMK-37 manual</b></a> ·
<a href="/SMK37.md#what-this-build-adds">What this build adds</a>
</p>

[SLOOP's own guide](/GUIDE.md) · [Building](/BUILDING.md)

## What it adds

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

## Install

1. **Back up the whole 1 MiB flash** first: the calibration, the Bluetooth pairing and key_mac are your unit's own and
   no package contains them ([how](/SMK37.md#install)).
2. Install the package for your model with the [web installer](https://milomeowzers.github.io/sloop-smk37/)
   (Chrome or Edge, over USB: pick your model), or download it from
   [Releases](https://github.com/MiloMeowzers/sloop-smk37/releases) and run
   `python3 tools/fm1_install.py sloop-smk37-elite.fwsc` (check its SHA-256 first).
3. **Going back:** M-Vave's own updater (M-UPGRADE) installs the stock firmware as usual. The port never writes the
   keyboard's protected flash.

Your projects, presets, samples and settings are kept when you update.

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

The decisions behind the port, and what was tested on a real unit, are in [`smk37-port.yaml`](/smk37-port.yaml).

## Branches

The work is a stack, so each part can be offered upstream on its own:

- **`smk37`**: the port itself. Everything SMK-37-specific is behind `BOARD_SMK37`, and the FM-1 build is unchanged
  byte for byte.
- **`editor-slot-readback`**: the web editor reads a user slot back from the keyboard.
- **`stems`**: the USB audio stems and everything built on them (STEM FX, the stem punch-in page, the sequencer
  effects). SMK-37 only.
- **`ci`** (the default branch): `stems` plus this README and the build workflow. Never part of a pull request.

## Builds and tests

Each push to `ci` builds, in one run: upstream SLOOP 2.8.1 for the FM-1 (the baseline); the SMK-37 packages (Pro,
Elite, MKE, and an Elite build with a USB serial console); the FM-1 again from this source, which **must be
byte-identical** to the baseline or the run fails; the installer sites and the offline editor; and the host and web
tests, which fail the run on any failure the baseline does not have. The run's artifact holds the packages,
`packages.sha256`, `fm1-identical.txt` and the logs. Only once every check has passed does the run publish the
installer sites and the editor on [GitHub Pages](https://milomeowzers.github.io/sloop-smk37/).

Tested on an SMK-37 Elite. The Pro, MKE-P37 and Starrykey builds differ only in the name the keyboard reports, but
have not been tried on those units: reports are welcome.

## Licence

GPL-3.0-only, as SLOOP and Felucca ([LICENSE](/LICENSE), [LICENSING.md](/LICENSING.md)). No M-Vave firmware or
other vendor material is in this repository.
