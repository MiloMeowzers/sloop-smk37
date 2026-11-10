# Mewcoder

SLOOP 2.8.4 with a vocoder, for the M-Vave SMK-37 Elite: **your voice in over USB audio** on track 3, played dry or
through a **20-channel vocoder** modelled on the Sennheiser VSM201, played from the keys and the sequencer. One of the
two builds of [SLOOP for the SMK-37](https://github.com/MiloMeowzers/sloop-smk37).

**[Install](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/installer/)** ·
**[Editor](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/editor/)** ·
[Manual](/VOCODER.md) ·
[Release](https://github.com/MiloMeowzers/sloop-smk37/releases/tag/mewcoder-v0.1)

> Unofficial firmware, not affiliated with M-Vave. No warranty. **Back up the keyboard first** ([how](/SMK37.md#install)).

![The vocoder page: the 20 channels on the knobs and pads](/assets/screens/smk37-vocoder-page.png)

## What you get

- **An audio track:** track 3 plays the computer's audio (your microphone) dry or vocoded.
- **The vocoder:** 20 channels, per-channel emphasis, speech addition, silence bridging, voiced / unvoiced detection.
- **The carrier:** a saw played from the keys and the sequencer. Play chords: that is the classic sound.
- **The vocoder page:** the 20 channels on the knobs and pads (PATCH on the tracks screen).
- **The editor's Vocoder tab**, and everything else SLOOP and the Audio stems build have.

**Left out to make room:** the built-in samples (your own samples work), the TEXTURE, DIGITAL, LOFI, VOICE, TRIO and
NOISE engines. Need them? Use [Audio stems](https://github.com/MiloMeowzers/sloop-smk37).

## Start

1. Back up the keyboard's flash ([how](/SMK37.md#install)) and your projects (the editor's Backup).
2. [Install](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/installer/) (Chrome or Edge, over USB).
3. On the computer, send your microphone to the SMK-37's audio output.
4. HOME > USB > **AUDIO IN: ON**, HOME > VOCODER > **MODE: VOCODER**, select track 3, play chords and speak.

The [manual](/VOCODER.md) has the rest. Known: AUTOCONTROL can pop (leave it off); one note alone sounds thin.

**In development:** a harmoniser modelled on the Eventide H949, for this build.

Branch `mewcoder` · release tag `mewcoder-v0.1` · current build `mewcoder-baseline` ·
how it was built: [VOCODER-DESIGN.md](/VOCODER-DESIGN.md) · GPL-3.0-only
