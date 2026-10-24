#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""SMK-37: the full flash backup from the stock firmware (fm1-to-smk37, docs/02-update-and-recovery.md, section 3.1,
method A), and a check of the dumps.

  python3 tools/smk_backup_prep.py prep PACKAGE.fwsc
      The stock app checks PACKAGE (an SMK-37 package of this firmware) and restarts into its update loader: the
      check step only, nothing is written to the app area. The loader then gets the UBOOT key and the keyboard comes
      back as the USB disk "WL82 UBOOT1.00", the chip's mask-ROM bootloader.
  python3 tools/smk_backup_prep.py verify DUMP-1.bin DUMP-2.bin
      Two reads of the whole flash: 1 MiB each, byte for byte the same, the calibration's "trimdat" at 0xC1224.

Reading the flash is jl-uboot-tool's (https://github.com/kagaimiq/jl-uboot-tool), from an administrator shell on
Windows (root on Linux), twice:
  python3 jluboottool.py --device <the WL82 UBOOT disk> --chip wl82 "read 0 0x100000 smk37-dump-1.bin"
This tool never reads or writes the flash and never writes the app. After the dumps, power-cycle the keyboard: it
is in the update loader's update mode. Finish the install (tools/fm1_install.py PACKAGE, or the web installer), or
put the stock firmware back with M-Vave's M-UPGRADE. Needs mido and python-rtmidi, as fm1_install.py.
"""
import argparse
import hashlib
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fm1_install as I  # noqa: E402

UBOOT_KEY = bytes([0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7])   # the loader's soft key (usb.c UBOOT_KEY): mask-ROM UBOOT
FLASH_SIZE = 0x100000
TRIM_OFF, TRIM_TAG = 0xC1224, b"trimdat"                 # the factory calibration record's tag (USRTRIM)

AFTER = """the UBOOT key was sent: the keyboard comes back as the USB disk "WL82 UBOOT1.00" in a few seconds.
  1. In an administrator shell (Windows) or as root (Linux), read the whole flash twice with jl-uboot-tool:
       python3 jluboottool.py --device <the WL82 UBOOT disk> --chip wl82 "read 0 0x100000 smk37-dump-1.bin"
       python3 jluboottool.py --device <the WL82 UBOOT disk> --chip wl82 "read 0 0x100000 smk37-dump-2.bin"
     (Windows: \\\\.\\PhysicalDriveN, the disk named WL82 UBOOT; Linux: /dev/sgN.) Note the flash id it prints.
  2. Check them: python3 tools/smk_backup_prep.py verify smk37-dump-1.bin smk37-dump-2.bin
  3. Keep the dump in two places: 0xC0000-0xC1FFF and 0xFF000-0xFFFFF are this unit's own.
  4. Power-cycle the keyboard: it is in update mode. Finish the install, or M-UPGRADE back to stock."""


def prep(a, backend, out, ask):
    product, image = I.load_package(a.package, False)
    if not I.SMK_MODEL_RE.fullmatch(I.model_of(product)) or I.is_stock(image):
        raise I.InstallError("usage", f"{a.package}: {product} is not an SMK-37 package with this firmware's update "
                                      "loader (the loader is what takes the UBOOT key)")
    print(f"package: {product}  ({a.package})", file=out)
    up = I.Updater(backend, a.port)
    dev = up.find()
    if not dev:
        raise I.not_found(up)
    print(f"device:  {dev.id.text}  ({dev.name})", file=out)
    if not I.SMK_MODEL_RE.fullmatch(I.model_of(dev.id.text)) or not I.same_keyboard(dev.id.text, product):
        dev.link.close()
        raise I.InstallError("model", f"the device is {dev.id.text}, the package is for {product}: nothing was sent")
    if dev.id.loader:                                    # update mode already: only this firmware's own loader
        if not I.OWN_LOADER_RE.fullmatch(dev.id.text):
            dev.link.close()
            raise I.InstallError("foreign", f"the keyboard is in the update mode of another firmware ({dev.id.text}): "
                                            "finish that one (M-UPGRADE), then run prep again")
        ota = dev
        print("in this firmware's update loader already: the UBOOT key only", file=out)
    else:
        refusal = I.board_refusal(I.model_of(dev.id.text), image)
        if refusal:
            dev.link.close()
            raise refusal
        if not a.yes and not ask("The check step only (nothing is written to the app), then the UBOOT key. "
                                 "Go on? [y/N] "):
            dev.link.close()
            print("cancelled", file=out)
            return 1
        prog = I.Progress(out)
        try:
            ota = up.check(dev, image, prog)             # step 1 of the install, then the loader: no write step
        finally:
            prog.end()
        if not I.OWN_LOADER_RE.fullmatch(ota.id.text) or not I.same_keyboard(ota.id.text, product):
            ota.link.close()
            raise I.InstallError("foreign", f"the keyboard restarted into {ota.id.text}, not this package's loader: "
                                            "the UBOOT key was not sent")
    if not ota.link.send(UBOOT_KEY):
        ota.link.close()
        raise I.InstallError("lost", "the loader was disconnected before the UBOOT key; power-cycle and run prep again")
    time.sleep(I.DELAY["open"])
    ota.link.close()
    print(AFTER, file=out)
    return 0


def verify(paths, out):
    """-> 0 when the dumps are a good backup; prints each check"""
    blobs, bad = [], 0
    for p in paths:
        try:
            blobs.append(Path(p).read_bytes())
        except OSError as e:
            print(f"{p}: {e.strerror}", file=out)
            return 1
    for p, b in zip(paths, blobs):
        size_ok, tag = len(b) == FLASH_SIZE, b[TRIM_OFF:TRIM_OFF + len(TRIM_TAG)]
        print(f"{p}: {len(b)} B {'ok' if size_ok else f'FAIL (not {FLASH_SIZE})'}, sha256 "
              f"{hashlib.sha256(b).hexdigest()}, 0x{TRIM_OFF:X}: {tag!r} {'ok' if tag == TRIM_TAG else 'FAIL'}",
              file=out)
        bad += (not size_ok) + (tag != TRIM_TAG)
    same = len(blobs) > 1 and all(b == blobs[0] for b in blobs[1:])
    if len(blobs) > 1:
        print(f"the reads are {'the same: ok' if same else 'DIFFERENT: FAIL (read again)'}", file=out)
    bad += len(blobs) > 1 and not same
    print("a good backup: keep it in two places" if not bad and len(blobs) > 1 else
          "NOT a good backup yet" if bad else "give the two reads to compare them", file=out)
    return 0 if not bad and len(blobs) > 1 else 1


def main(argv=None, backend=None, out=sys.stdout, ask=I.ask_tty):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("prep", help="the check step only, then the loader's UBOOT key")
    p.add_argument("package", help="an SMK-37 package of this firmware (.fwsc)")
    p.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name)")
    p.add_argument("--yes", action="store_true", help="do not ask for confirmation")
    v = sub.add_parser("verify", help="check two dumps of the whole flash")
    v.add_argument("dumps", nargs="+", help="the reads of the flash (two)")
    a = ap.parse_args(argv)
    if a.cmd == "verify":
        return verify(a.dumps, out)
    try:
        if backend is None:
            try:
                backend = I.MidoBackend()
            except ImportError:
                raise I.InstallError("usage", "needs mido and python-rtmidi (pip install mido python-rtmidi)")
        return prep(a, backend, out, ask)
    except I.InstallError as e:
        print(f"error: {e}", file=sys.stderr)
        return I.EXIT.get(e.code, 1)
    except KeyboardInterrupt:
        print("\ninterrupted: if the keyboard is in update mode, run prep again (or finish the install)", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
