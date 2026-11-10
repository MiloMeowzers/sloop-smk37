# SLOOP for the M-Vave SMK-37

The SLOOP groovebox firmware on the M-Vave SMK-37 keyboard, in two builds. Install and edit from the browser:
**[milomeowzers.github.io/sloop-smk37](https://milomeowzers.github.io/sloop-smk37/)**

> Unofficial firmware, not affiliated with M-Vave. No warranty. **Back up the keyboard first** ([how](/SMK37.md#install)).

## Audio stems

![The stem punch-in page, HOME > USB and HOME > PUNCH](/assets/screens/smk37-stems-screens.png)

SLOOP 2.8.1 with **six USB audio stems** for your DAW (the drums in four groups, the synths, the reverb / delay return),
**punch-ins on every stem**, the stem punch-in page on the pads, sequencer effects (FLIP, LOOP BACK, RAMP) and DAW
mode. Every engine and the built-in samples. For the SMK-37 Pro, Elite, MKE-P37 and Starrykey 37 Play.

**[Install](https://milomeowzers.github.io/sloop-smk37/)** ·
**[Editor](https://milomeowzers.github.io/sloop-smk37/elite/webapp/editor/)** ·
[Manual](/SMK37.md) ·
[Release](https://github.com/MiloMeowzers/sloop-smk37/releases/tag/v2.8.1-smk37.1)

## Mewcoder (vocoder)

![The vocoder page: the 20 channels on the knobs and pads](/assets/screens/smk37-vocoder-page.png)

SLOOP 2.8.4 with **your voice in over USB audio** on track 3, played dry or through a **20-channel vocoder** modelled
on the Sennheiser VSM201, played from the keys and the sequencer. A lean build: some engines and the built-in samples
make room for it. For the SMK-37 Elite. **In development:** a harmoniser modelled on the Eventide H949.

**[Install](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/installer/)** ·
**[Editor](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/editor/)** ·
[Manual](https://github.com/MiloMeowzers/sloop-smk37/blob/mewcoder/VOCODER.md) ·
[More](https://github.com/MiloMeowzers/sloop-smk37/tree/mewcoder) ·
[Release](https://github.com/MiloMeowzers/sloop-smk37/releases/tag/mewcoder-v0.1)

## Install

1. Back up the keyboard's flash ([how](/SMK37.md#install)) and your projects (the editor's Backup).
2. Open the build's installer in Chrome or Edge, connect the keyboard by USB, press Install.
3. Use the editor of the build you installed. To go back: M-Vave's own updater (M-UPGRADE).

## Branches and tags

| | Audio stems | Mewcoder |
|---|---|---|
| Branch | `ci` (built on `smk37`, `editor-slot-readback`, `stems`) | `mewcoder` |
| Release tag | `v2.8.1-smk37.1` | `mewcoder-v0.1` |
| Current build | `stems-baseline` | `mewcoder-baseline` |

Every build is checked before it is published: the FM-1 build byte-identical to SLOOP's, the host and web tests clean.
How it is built: [BUILDING.md](/BUILDING.md); the port's decisions: [smk37-port.yaml](/smk37-port.yaml).

## Credits

[SLOOP](https://github.com/isod89/sloop-fm1) by isod89, on [Felucca](https://github.com/hugelton/Felucca) by Leo
Kuroshita / Hügelton Instruments; the SMK-37 board layer and the porting method by M. Geisler
([genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1),
[fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37)). GPL-3.0-only ([LICENSE](/LICENSE),
[LICENSING.md](/LICENSING.md)). No vendor material is in this repository.
