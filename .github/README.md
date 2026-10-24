# sloop37: build and hardware-test repository

This private repository builds and tests a port of **SLOOP 2.8.1** to the **M-Vave SMK-37**, so the packages can be
tried on a real keyboard before the work goes anywhere public. It is not a product, a fork with its own name, or a
release channel: the port is meant to go back to the projects it comes from.

## Provenance

| Part | From | Licence |
|---|---|---|
| SLOOP, the firmware | [isod89/sloop-fm1](https://github.com/isod89/sloop-fm1), based on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments | GPL-3.0-only |
| The SMK-37 board layer (input, screen, pad LEDs, flash map, update path, DAW mode) | M. Geisler, [genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1) branch `smk37` (isod89/sloop-fm1#112); its commits keep their authorship here | GPL-3.0-only |
| The porting method: the playbook, the hardware docs, the `smk37-port.yaml` template | M. Geisler, [genkigenki/fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) | see that repository |
| Carrying the layer to 2.8.1, the four 40 KiB user sample slots, punch-in FX MIDI out, the editor's slot sizing | the commits on `smk37` after the merge of the layer | GPL-3.0-only |

The decisions behind the port are in `smk37-port.yaml`; what the SMK-37 build does is in `SMK37.md`.

## Branches

- **`smk37`**: the port. Sources only, everything SMK-37-specific behind `BOARD_SMK37`, the FM-1 build unchanged byte
  for byte. This is the branch that may go upstream (through genkigenki's work or to isod89), once agreed with them.
- **`ci`** (the default branch here): `smk37` plus this README and `.github/workflows/smk37-build.yml`. Never part of a
  pull request; rebased onto `smk37` after each change.

## What a run does

Each push to `ci` (or **Run workflow** on the Actions page) builds, in one run, so the embedded build dates match:

1. upstream SLOOP 2.8.1 for the FM-1: the baseline hashes and test results;
2. the SMK-37 packages: `sloop-smk37.fwsc` (SMK-37 Pro, also the Starrykey 37 Play), `-elite`, `-mke`;
3. the FM-1 again from `smk37`: the run fails unless it is byte-identical to the baseline;
4. the host and web tests: the run fails on any failure the baseline does not have.

The run's artifact holds the packages, `packages.sha256`, `fm1-identical.txt` and the logs.

## Hardware testing

The packages are **test builds for the owner's own unit**, not releases: do not pass them on.

- Before the first install: a verified **full flash backup** of the keyboard (fm1-to-smk37,
  `docs/02-update-and-recovery.md`, section 3). Installing is always done by hand by the owner, never by a script here.
- Check the package's SHA-256 against `packages.sha256`, install it with the SLOOP installer, then work through the
  bring-up list (keys, pads, faders, wheels, saving and reloading, samples and kits from the web editor) and report back.
- The way back to the stock firmware stays M-Vave's own updater (M-UPGRADE). The port never writes the keyboard's
  protected flash (calibration, Bluetooth pairing, stock resources, key_mac).

Unofficial firmware, not affiliated with or endorsed by M-Vave. No warranty.
