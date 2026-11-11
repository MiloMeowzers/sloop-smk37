# Mewcoder

SLOOP 2.8.4 with a vocoder, for the M-Vave SMK-37 Elite: **your voice, or any sound you choose, in over USB audio** on track 3, played dry or
through a **20-channel vocoder** modelled on the Sennheiser VSM201, played from the keys and the sequencer. One of the
two builds of [SLOOP for the SMK-37](https://github.com/MiloMeowzers/sloop-smk37).

**[Install](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/installer/)** ·
**[Editor](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/editor/)** ·
[Manual](/VOCODER.md) ·
[Release](https://github.com/MiloMeowzers/sloop-smk37/releases/tag/mewcoder-v0.1)

> Unofficial firmware, not affiliated with M-Vave. No warranty. **Back up the keyboard first** ([how](/SMK37.md#install)).

![The vocoder page: the 20 channels on the knobs and pads](/assets/screens/smk37-vocoder-page.png)

## Two inputs, routed

![HOME > SETUP: the defaults, your voice with your synth as the carrier, an instrument through T1](/assets/screens/smk37-voc-setup.png)

The USB input's left and right sides are **two sources of their own**. Your voice on one; on the other:
- **your synth as the carrier**, played from the SMK-37's keys over MIDI and gated by them: the classic studio vocoder;
- **an instrument through T1's or T2's effects** (filter, drive, sends, punch-ins) and onto its own USB stem;
- or nothing: a mono mic at full level.

**SETUP** chooses which of SLOOP's tracks play beside the vocoder. Turn T1 and T2 or the drums off when your DAW brings
its own, and the vocoder runs at **44.1 kHz (HQ)**. [How it all works](/VOCODER.md#2-setup-and-the-two-inputs).

## What you get

- **An audio track:** track 3 plays the computer's audio (your microphone, an instrument, any recording) dry or vocoded.
- **The vocoder:** 20 channels, per-channel emphasis, speech addition, silence bridging, voiced / unvoiced detection,
  ATTACK and RELEASE for smoother syllables.
- **The carrier:** saws played from the keys and the sequencer (two a note in VOC ALONE), or your own synth. Play
  chords: that is the classic sound.
- **The vocoder page:** the 20 channels on the knobs and pads (PATCH on the tracks screen).
- **The editor's Vocoder tab**, and everything else SLOOP and the Multichannel USB audio build have.

**Left out to make room:** the built-in samples (your own samples work), the TEXTURE, DIGITAL, LOFI, VOICE, TRIO and
NOISE engines. Need them? Use [Multichannel USB audio](https://github.com/MiloMeowzers/sloop-smk37).

## Start

1. Back up the keyboard's flash ([how](/SMK37.md#install)) and your projects (the editor's Backup).
2. [Install](https://milomeowzers.github.io/sloop-smk37/mewcoder/webapp/installer/) (Chrome or Edge, over USB).
3. On the computer, send your microphone to the SMK-37's audio output (peaks around −10 dBFS).
4. HOME > USB > **AUDIO IN: ON**, HOME > VOCODER > **MODE: VOCODER**, select track 3, play chords and speak.
5. A mono mic on one side: HOME > SETUP > **IN R: OFF** (or IN L), for its full level.

The [manual](/VOCODER.md) has the rest. Known: AUTOCONTROL can pop (leave it off).

**In development:** a harmoniser modelled on the Eventide H949, for this build, on the same routing: your voice,
another external sound, the drums or a synth engine pitch-shifted into up to three harmony voices.

Branch `mewcoder` · release tag `mewcoder-v0.1` · current build `mewcoder-baseline` ·
how it was built: [VOCODER-DESIGN.md](/VOCODER-DESIGN.md) · GPL-3.0-only
