#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The SMK-37 installer pages (the CI's Pages step): web/make_site.py builds SLOOP's installer, whose text is the FM-1's.
This rewrites a built installer page (OUT_DIR/webapp/installer/index.html) for the SMK-37 and for one of the two builds:
its heading, its lead and tiles, and the FM-1's words in the messages (both languages); the FM-1 Transporter's
recovery becomes the SMK-37's own (its USB recovery screen). SLOOP's web/ is not changed.

  .github/smk_site.py OUT_DIR stems|mewcoder MODEL_NAME
"""
import re
import shutil
import sys
from pathlib import Path

out, build, model = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
page = out / "webapp" / "installer" / "index.html"
html = page.read_text(encoding="utf-8")

HEAD = {
    "stems": (f"SLOOP for the {model}: Multichannel USB audio",
              "SLOOP 2.8.1 as a 6-channel USB audio interface: your DAW records each track and drum group on a channel "
              "of its own. Punch-in effects on every channel, the punch-in page on the pads, sequencer effects and DAW "
              "mode. Every engine, 153 sounds and the built-in samples.",
              [("6", "USB audio channels to the DAW"), ("27", "punch-in effects, on every channel"),
               ("16", "pads: the stem punch-in page"), ("DAW", "mode, with your own CC map")]),
    "mewcoder": (f"Mewcoder for the {model}",
                 "SLOOP 2.8.4 with your voice, or any sound you choose, in over USB audio on track 3, played dry or through a 20-channel vocoder "
                 "modelled on the Sennheiser VSM201, played from the keys and the sequencer. A lean build: the built-in "
                 "samples and some engines make room for it.",
                 [("20", "vocoder channels"), ("2", "inputs: voice, carrier, tracks"),
                  ("EXT", "your synth as the carrier"), ("HQ", "44.1 kHz vocoder")]),
}
title, lead, tiles = HEAD[build]
COLS = ["blue", "green", "yellow", "orange"]


def sub1(pat, rep, s, flags=0, expand=False):
    s2, n = re.subn(pat, (lambda m: m.expand(rep)) if expand else (lambda m: rep), s, count=1, flags=flags)
    if n != 1:
        raise SystemExit(f"smk_site.py: not found: {pat[:60]}")
    return s2


html = sub1(r"<h1>.*?</h1>", f"<h1>{title}</h1>", html, re.S)
html = sub1(r'<p class="lead">.*?</p>', f'<p class="lead">{lead}</p>', html, re.S)
html = sub1(r'<div class="tiles">.*?</div>\s*</div>',
            '<div class="tiles">\n' + "".join(
                f'    <div class="tile" style="--c: var(--{c})"><b>{b}</b><span>{t}</span></div>\n'
                for (b, t), c in zip(tiles, COLS)) + "  </div>", html, re.S)
# the FM-1 Transporter link: the SMK-37 recovers by itself
html = sub1(r'\s*<a href="https://github.com/kurogedelic/FM-1-transporter"[^>]*>FM-1-transporter</a>', "", html)
html = sub1(r"M-VAVE and FM-1 are trademarks", "M-VAVE, SMK-37 and FM-1 are trademarks", html)
if build == "mewcoder":   # no built-in samples in this build
    html = sub1(r" Samples: Versilian Studios[^<]*?not CC0\)\.", "", html)

# the manual (a link beside the editor's) and the screens, under the editor links. The screens are the firmware's own
# 240 x 240 screens (drawn from its menu and page code), in .github/pages/shots/, copied next to the page
REPO = "https://github.com/MiloMeowzers/sloop-smk37"
MANUAL = {"stems": (f"{REPO}/blob/ci/SMK37.md", "The SMK-37 manual"),
          "mewcoder": (f"{REPO}/blob/mewcoder/VOCODER.md", "The vocoder manual")}[build]
SITE = "https://milomeowzers.github.io/sloop-smk37/" + ("mewcoder/webapp/installer/" if build == "mewcoder" else "")
SHOTS = {
    "stems": [("stems-punch", "The stem punch-in page", "FX held: REV playing on every stem"),
              ("stems-usb", "HOME > USB", "six stems: the drums in four groups"),
              ("stems-menu-punch", "HOME > PUNCH", "the punch-ins on the stems, seamless joins")],
    "mewcoder": [("voc-channels", "The vocoder page", "the 20 channels on the knobs and pads"),
                 ("voc-bridge", "Channels 8-14 and BRIDGE", "silence bridging, AUTOCONTROL"),
                 ("voc-setup", "HOME > SETUP", "voice on IN L, your synth as the carrier on IN R"),
                 ("voc-envelope", "HOME > ENVELOPE", "attack, release, BRIDGE MODE")],
}[build]
shots_dir = page.parent / "shots"
shots_dir.mkdir(exist_ok=True)
for name, _, _ in SHOTS:
    shutil.copyfile(Path(__file__).parent / "pages" / "shots" / f"{name}.png", shots_dir / f"{name}.png")
html = sub1(r'(\s*<p><button id="app" class="ed" hidden data-t="appInstall"></button></p>)',
            f'\n  <p><a class="ed" href="{MANUAL[0]}" target="_blank" rel="noopener">{MANUAL[1]}</a></p>'
            r'\1' + '\n  <h2 class="shots-h">On the keyboard</h2>\n  <div class="shots">\n' + "".join(
                f'    <figure><img src="shots/{n}.png" width="240" height="240" alt="{c}: {s}" loading="lazy">'
                f'<figcaption><b>{c}</b>{s}</figcaption></figure>\n' for n, c, s in SHOTS) + "  </div>", html, expand=True)
html = sub1(r"(\s*\[hidden\] \{ display: none !important; \})",
            r"\1" + "\n  .shots-h { font-size: 13px; font-weight: 600; letter-spacing: .08em; text-transform: uppercase;"
            " color: var(--dim); margin: 28px 0 10px; }\n"
            "  .shots { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; }\n"
            "  .shots figure { margin: 0; }\n"
            "  .shots img { display: block; width: 100%; height: auto; image-rendering: pixelated; background: #000; }\n"
            "  .shots figcaption { font-size: 12px; color: var(--dim); margin-top: 6px; line-height: 1.35; }\n"
            "  .shots figcaption b { display: block; color: var(--fg); font-weight: 600; }", html, expand=True)

EN = {
    "intro": f"Connect the {model} by USB. (Works in Chrome or Edge.)",
    "direct": "Connect the USB cable directly to the computer.",
    "brick": "If an install fails and the keyboard no longer starts: after two failed starts it opens its USB recovery "
             "screen (or hold OCTAVE− while switching on). Then install again from here.",
    "beta": "Unofficial firmware, not affiliated with M-Vave, and it may have faults. The author accepts no responsibility "
            "if your keyboard is damaged or misbehaves because of it. Use it at your own risk.",
    "back": "To go back to M-Vave's firmware, use M-Vave's own updater, M-UPGRADE (link below). If it does not find the "
            "keyboard, close every other app and browser tab that uses MIDI.",
    "notfound": "Keyboard not found. Check the USB cable (a data cable), close other apps that use MIDI, and press "
                "Install again.",
    "model": f"The connected keyboard is not an {model}. Pick the page for your model.",
    "noloader": "The keyboard in update mode was not found. Replug the USB cable and press Install again (it stays in "
                "update mode until it is finished).",
    "noreturn": "The keyboard did not come back. Switch it off and on.",
    "mismatch": "Written, but the keyboard reports another version: ",
    "foreign": "The keyboard is in the update mode of another firmware. Finish that update with its own updater "
               "(M-UPGRADE), then press Install again: ",
    "editor": "Editor (after installing: sounds, samples, projects" + (", the vocoder" if build == "mewcoder" else "")
              + ", from the computer)",
}
JA = {
    "intro": f"{model} を USB で接続してください。（Chrome か Edge で使えます）",
    "direct": "USB ケーブルはパソコンに直接つないでください。",
    "brick": "書き込みに失敗してキーボードが起動しなくなった場合: 起動に 2 回失敗すると USB の復旧画面が開きます"
             "（または OCTAVE− を押しながら電源を入れます）。そのあと、ここからもう一度インストールしてください。",
    "beta": "M-Vave とは関係のない非公式のファームウェアで、不具合がある場合があります。これを使ったことでキーボードが壊れたり"
            "不具合が起きたりしても、作者は一切の責任を負いません。自己責任でお使いください。",
    "back": "M-Vave のファームウェアに戻すには、M-Vave のアップデーター M-UPGRADE（下のリンク）を使ってください。"
            "キーボードが見つからない場合は、MIDI を使う他のアプリとブラウザのタブを閉じてください。",
    "notfound": "キーボードが見つかりません。USB ケーブル（データ通信用か）を確かめ、MIDI を使う他のアプリを閉じてから、"
                "もう一度押してください。",
    "model": f"接続されている機器は {model} ではありません。お使いのモデルのページを選んでください。",
    "noloader": "書き込みモードのキーボードが見つかりません。USB を挿し直してもう一度押してください（終わるまで書き込みモードのままです）。",
    "noreturn": "キーボードが戻ってきません。電源を入れ直してください。",
    "mismatch": "書き込みは終わりましたが、キーボードが別のバージョンを報告しています: ",
    "foreign": "キーボードは別のファームウェアの書き込みモードです。そのアップデーター（M-UPGRADE）で書き込みを終えてから、"
               "もう一度インストールしてください: ",
}


def strings(block, table):
    for k, v in table.items():
        v = v.replace("\\", "\\\\").replace('"', '\\"')
        block, n = re.subn(r'(\b' + k + r':\s*)"(?:[^"\\]|\\.)*"', lambda m: m.group(1) + '"' + v + '"', block)
        if not n:
            raise SystemExit(f"smk_site.py: string {k} not found")
    return block


# the language button and the "other firmware" message shared the key "other" (the message, later in each table, won:
# the button showed the whole message): the button gets a key of its own
html = sub1(r'\$\("lang"\)\.textContent = t\("other"\);', '$("lang").textContent = t("langName");', html)
html = sub1(r'install: "インストール", other: "English",', 'install: "インストール", langName: "English",', html)
html = sub1(r'install: "Install", other: "日本語",', 'install: "Install", langName: "日本語",', html)

i = html.index("\n  en: {")
html = strings(html[:i], JA) + strings(html[i:], EN)
page.write_text(html, encoding="utf-8")
print(f"smk_site.py: {page} ({build}, {model})")

# the editor (both copies: online and the one file for the disk): the device it talks to is the SMK-37. The FM-1's name
# only ever appears in its words (help, messages, labels): the device is found by a lower-case pattern, never by it
KEEP = r"FM-1(?![-_.]| V15)"
for ed in (out / "webapp" / "editor" / "index.html", out / "webapp" / "editor" / "sloop-editor.html"):
    if not ed.exists():
        continue
    e = ed.read_text(encoding="utf-8")
    e = e.replace("M-VAVE and FM-1 are trademarks", "M-VAVE, SMK-37 and FM-1 are trademarks")
    e, n = re.subn(KEEP, "SMK-37", e)
    e = e.replace("M-VAVE, SMK-37 and SMK-37 are trademarks", "M-VAVE, SMK-37 and FM-1 are trademarks")
    # the one file for the disk linked SLOOP's FM-1 installer: this build's own instead
    e = e.replace('href="https://isod89.github.io/sloop-fm1/webapp/installer/"', f'href="{SITE}"')
    # the manual, beside the installer link under the connect button
    e = sub1(r'(<a class="dim" href="[^"]*"(?: target="_blank" rel="noopener")? data-t="installer"></a>)',
             rf'\1<a class="dim" href="{MANUAL[0]}" target="_blank" rel="noopener">Manual</a>', e, expand=True)
    ed.write_text(e, encoding="utf-8")
    print(f"smk_site.py: {ed}: {n} x FM-1 -> SMK-37")
