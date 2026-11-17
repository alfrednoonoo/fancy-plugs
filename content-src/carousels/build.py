"""Builds a plugin's carousel (docs/content-plan.md, part B).

    .deps/tts-venv/Scripts/python content-src/carousels/build.py comp

The carousel is drawn as one wide HTML canvas (1080 px per slide, 1350 px tall), printed by
headless Edge and sliced, so the title strip, the meter and a window that crosses a seam line
up when swiped. Output goes to content/carousels/<slug>/ (git-ignored): one PNG per slide,
the whole canvas, a contact sheet and the caption. Needs Pillow.
"""
import html
import importlib
import json
import subprocess
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
# Posts point to the link, they do not spell the address (user, 17 November 2026).
LINK = "Link in bio"
W, H = 1080, 1350
PAD = 80                      # side margin of every slide
STRIP_H = 88
METER_Y, METER_H = 1300, 18
TEAL, GREY, WELL, PAGE, PANEL = "#00A985", "#8D8D8D", "#141414", "#1E1E1E", "#262626"
TEXT_ACCENT = {"#9A59B5": "#A970C2"}   # purple fails 4.5:1 as text on the dark grounds
METER_FROM, METER_TO = (0xFF, 0xCF, 0x07), (0x00, 0xFF, 0x96)


def esc(s):
    return html.escape(s, quote=True)


def ring(colour, size, sw):
    """The knob ring: a 300 degree arc from 7 o'clock."""
    r, c = (size - sw) / 2, size / 2
    return (f'<svg width="{size}" height="{size}" viewBox="0 0 {size} {size}" aria-hidden="true">'
            f'<circle cx="{c}" cy="{c}" r="{r}" fill="none" stroke="{colour}" stroke-width="{sw}" stroke-linecap="round" '
            f'pathLength="360" stroke-dasharray="300 60" transform="rotate(120 {c} {c})"/></svg>')


def marker(colour, x, y, size=132):
    """A ring closed round a control, with a dark halo so it reads on any part of a window."""
    return (f'<svg class="mk" style="left:{x - size / 2}px;top:{y - size / 2}px" width="{size}" height="{size}" viewBox="0 0 100 100" aria-hidden="true">'
            f'<circle cx="50" cy="50" r="44" fill="none" stroke="{WELL}" stroke-opacity=".6" stroke-width="11"/>'
            f'<circle cx="50" cy="50" r="44" fill="none" stroke="{colour}" stroke-width="6" stroke-linecap="round" '
            f'pathLength="360" stroke-dasharray="300 60" transform="rotate(120 50 50)"/></svg>')


def crop(image, box, scale, colour, point=None, at=None, size=132):
    """Part of a screenshot, with the ring on one control: box is (left, top, right, bottom) and point (x, y),
    both in pixels of the 2x image. With at=(x, y) it is placed on the canvas; without, it sits in the slide's column."""
    l, t, r, b = box
    iw, ih = Image.open(REPO / "docs/images" / f"{image}.png").size
    place = f"position:absolute;left:{at[0]}px;top:{at[1]}px;" if at else ""
    out = (f'<div class="cropw" style="{place}width:{(r - l) * scale:.1f}px;height:{(b - t) * scale:.1f}px"><div class="crop">'
           f'<img src="{(REPO / "docs/images" / f"{image}.png").as_uri()}" alt="" '
           f'style="width:{iw * scale:.1f}px;height:{ih * scale:.1f}px;left:{-l * scale:.1f}px;top:{-t * scale:.1f}px"></div>')
    if point:
        out += marker(colour, (point[0] - l) * scale, (point[1] - t) * scale, size)
    return out + "</div>"


def logo(width):
    s = (REPO / "design/website/logo-lockup.svg").read_text(encoding="utf-8")
    return s.replace("<svg ", f'<svg style="width:{width}px;height:{width * 64 / 254:.1f}px" ', 1)


def meter(i, n):
    """The progress meter: one well along the bottom of the whole canvas, a cell per slide,
    lit with the meter gradient up to this slide."""
    cell = W / n
    out = []
    for j in range(n):
        f = j / (n - 1)
        rgb = tuple(round(a + (b - a) * f) for a, b in zip(METER_FROM, METER_TO))
        lit = j <= i
        colour = "#%02X%02X%02X" % rgb if lit else PANEL
        out.append(f'<div class="cell" style="left:{i * W + j * cell + 3:.1f}px;width:{cell - 6:.1f}px;background:{colour}"></div>')
    return "".join(out)


def text_block(x, y, w, s, title_px=64):
    out = f'<div class="tb" style="left:{x}px;top:{y}px;width:{w}px">'
    if s.get("label"):
        out += f'<div class="label">{esc(s["label"])}</div>'
    out += f'<h2 style="font-size:{title_px}px">{esc(s["title"])}</h2>'
    if s.get("text"):
        out += f'<p>{esc(s["text"])}</p>'
    return out + "</div>"


def arrowed(text):
    """A hand-off line, with the arrow joined to its last word so it never wraps alone."""
    head, _, last = esc(text).rpartition(" ")
    return f'{head} <span class="nw">{last}&nbsp;&nbsp;<span class="ar">&#8594;</span></span>'


def next_line(x0, s, y=1196, w=W - 2 * PAD):
    if not s.get("next"):
        return ""
    return f'<div class="next" style="left:{x0 + PAD}px;top:{y}px;width:{w}px">{arrowed(s["next"])}</div>'


def cover_window(s):
    """Where the cover's window sits on the canvas: as large as fits under the cover text, and
    placed so it leaves slide 1 and finishes on slide 2. A spec can set x, y or w itself."""
    win = dict(s["window"])
    iw, ih = Image.open(REPO / "docs/images" / f'{win["image"]}.png').size
    win.setdefault("y", 500)
    win.setdefault("w", round(min(1340, (METER_Y - 30 - win["y"]) * iw / ih)))
    win.setdefault("x", round(max(360, W - 0.6 * win["w"])))
    return win


def build(slug):
    spec = importlib.import_module(slug)
    data = json.loads((REPO / "site-src/data/plugins.json").read_text(encoding="utf-8"))
    rel = json.loads((REPO / "site-src/data/release.json").read_text(encoding="utf-8"))
    p = data["plugins"][slug]
    accent = p["accent"]
    text_accent = TEXT_ACCENT.get(accent, accent)
    slides = spec.SLIDES
    n = len(slides)
    body = []
    for i, s in enumerate(slides):
        x0 = i * W
        # The title strip: one bar across the canvas; on each slide the plugin, the slide's heading and its number.
        body.append(f'<div class="strip-t" style="left:{x0 + PAD}px;width:{W - 2 * PAD}px">'
                    f'<span><span class="f">Fancy</span><span style="color:{text_accent}">{esc(p["name"])}</span>'
                    f'{"" if s["kind"] == "cover" else f"<span class=h>{esc(s["heading"])}</span>"}</span>'
                    f'<span class="num">{i + 1} / {n}</span></div>')
        body.append(meter(i, n))
        k = s["kind"]
        if k == "cover":
            win = cover_window(s)
            iw, ih = Image.open(REPO / "docs/images" / f'{win["image"]}.png').size
            wh = win["w"] * ih / iw
            body.append(f'<img class="win" src="{(REPO / "docs/images" / (win["image"] + ".png")).as_uri()}" alt="" '
                        f'style="left:{win["x"]}px;top:{win["y"]}px;width:{win["w"]}px;height:{wh:.1f}px">')
            body.append(f'<div class="tb" style="left:{x0 + PAD}px;top:150px;width:{W - 2 * PAD}px">'
                        f'<div class="label">{esc(s["heading"])}</div>'
                        f'<div class="name" style="font-size:{128 if len(p["name"]) <= 6 else 104}px">Fancy <span style="color:{text_accent}">{esc(p["name"])}</span></div>'
                        f'<p class="lead">{esc(s["sentence"])}</p></div>')
        elif k == "job":
            body.append(text_block(x0 + PAD, 150, W - 2 * PAD, s))
            # The window from slide 1 ends near x 620 of this slide; the hand-off line sits beside it.
            win = cover_window(slides[0])
            right = win["x"] + win["w"] - x0
            body.append(f'<div class="next side" style="left:{x0 + right + 56}px;top:{win["y"]}px;'
                        f'width:{W - PAD - right - 56}px;height:{METER_Y - 60 - win["y"]}px"><span>{arrowed(s["next"])}</span></div>')
        elif k == "feature":
            l, t, r, b = s["crop"]
            cw, ch = (r - l) * s["scale"], (b - t) * s["scale"]
            shot = crop(s["image"], s["crop"], s["scale"], accent, s.get("ring"), size=s.get("ring_size", 132))
            if s["layout"] == "side":
                body.append(crop(s["image"], s["crop"], s["scale"], accent, s.get("ring"), at=(x0 + PAD, 150), size=s.get("ring_size", 132)))
                tx = PAD + cw + 64
                body.append(text_block(x0 + tx, 160, W - PAD - tx, s, title_px=56))
            else:
                # One column: the text, and the crop centred in the space the text leaves. "below" puts the
                # text first, "above" the crop first, so neighbouring slides differ in shape.
                words = text_block(0, 0, W - 2 * PAD, s, title_px=64 if s["layout"] == "below" else 56)
                parts = [words, f'<div class="mid">{shot}</div>']
                body.append(f'<div class="col" style="left:{x0 + PAD}px">{"".join(parts if s["layout"] == "below" else parts[::-1])}</div>')
            body.append(next_line(x0, s))
        elif k == "pairs":
            rows = []
            for other, why in p["pairs"]:
                q = data["plugins"][other]
                rows.append(f'<div class="pair">{ring(q["accent"], 64, 9)}<div><div class="pn">Fancy {esc(q["name"])}</div>'
                            f'<div class="pw">{esc(why)}</div></div></div>')
            # The last carousel of the series has no next one, so its hand-off line goes to the download.
            hand = ("Next in the series: Fancy " + data["plugins"][s["next_plugin"]]["name"]) if s.get("next_plugin") else "All fourteen are free"
            body.append(f'<div class="tb" style="left:{x0 + PAD}px;top:150px;width:{W - 2 * PAD}px">'
                        f'<h2 style="font-size:64px">{esc(s["title"])}</h2><div class="pairs">{"".join(rows)}</div></div>')
            body.append(f'<div class="next" style="left:{x0 + PAD}px;top:1196px;width:{W - 2 * PAD}px">'
                        f'{arrowed(hand)}</div>')
        elif k == "download":
            body.append(f'<div class="end" style="left:{x0}px">{logo(560)}'
                        f'<div class="free">Free to download and use.</div>'
                        f'<div class="btn">Download {esc(rel["version"])}</div>'
                        f'<div class="os">Windows · Mac (beta)</div>'
                        f'<div class="url">{LINK}</div></div>')

    font = (REPO / "assets/fonts").as_uri()
    page = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>Fancy {esc(p["name"])} carousel</title>
<style>
@font-face{{font-family:InterFP;font-weight:400;src:url('{font}/Inter-Regular.ttf')}}
@font-face{{font-family:InterFP;font-weight:500;src:url('{font}/Inter-Medium.ttf')}}
@font-face{{font-family:InterFP;font-weight:600;src:url('{font}/Inter-SemiBold.ttf')}}
*{{margin:0;padding:0;box-sizing:border-box}}
html,body{{width:{W * n}px;height:{H}px;overflow:hidden;background:{PAGE}}}
body{{position:relative;font-family:InterFP,sans-serif;color:#fff;-webkit-font-smoothing:antialiased}}
body>*{{position:absolute}}
.strip{{left:0;top:0;width:{W * n}px;height:{STRIP_H}px;background:{WELL};border-bottom:2px solid {PANEL}}}
.strip-t{{top:0;height:{STRIP_H}px;display:flex;align-items:center;justify-content:space-between;
 font-weight:600;font-size:28px;letter-spacing:.08em;text-transform:uppercase}}
.strip-t .f{{color:{GREY};margin-right:.5em}}
.strip-t .h{{color:{GREY};font-weight:500;letter-spacing:0;text-transform:none;margin-left:1.1em}}
.strip-t .num{{color:{GREY};font-weight:500;letter-spacing:.02em}}
.well{{left:0;top:{METER_Y}px;width:{W * n}px;height:{METER_H}px;background:{WELL}}}
.cell{{top:{METER_Y + 4}px;height:{METER_H - 8}px;border-radius:2px}}
.tb .label{{font-weight:500;font-size:34px;color:{GREY};margin-bottom:14px}}
.tb .name{{font-weight:600;font-size:128px;line-height:1.02;letter-spacing:-0.025em}}
.tb .lead{{font-weight:500;font-size:46px;line-height:1.22;color:#fff;margin-top:28px;max-width:860px}}
.tb h2{{font-weight:600;line-height:1.1;letter-spacing:-0.02em;text-wrap:balance}}
.tb p{{font-size:36px;line-height:1.38;color:#C8C8C8;margin-top:26px;text-wrap:pretty}}
.next{{font-weight:500;font-size:34px;line-height:1.3;color:{GREY}}}
.next .nw{{white-space:nowrap}}
.next .ar{{color:{text_accent}}}
.next.side{{display:flex;flex-direction:column;justify-content:flex-end}}
.cropw{{position:relative;flex:none}}
.crop{{position:absolute;inset:0;overflow:hidden;border-radius:14px;box-shadow:0 18px 48px rgba(0,0,0,.45);outline:2px solid {PANEL}}}
.col{{top:150px;width:{W - 2 * PAD}px;height:1010px;display:flex;flex-direction:column}}
.col .tb{{position:static;flex:none}}
.col .mid{{flex:1;display:flex;align-items:center;justify-content:center;min-height:0}}
.crop img{{position:absolute;max-width:none}}
.win{{border-radius:14px;box-shadow:0 22px 56px rgba(0,0,0,.5)}}
.mk{{position:absolute;overflow:visible}}
.pairs{{margin-top:72px;display:flex;flex-direction:column;gap:56px}}
.pair{{display:flex;gap:32px;align-items:flex-start}}
.pair svg{{flex:none;margin-top:4px}}
.pn{{font-weight:600;font-size:48px;line-height:1.1}}
.pw{{font-size:34px;line-height:1.35;color:#C8C8C8;margin-top:10px}}
.end{{top:0;width:{W}px;height:{METER_Y}px;display:flex;flex-direction:column;align-items:center;justify-content:center}}
.end .free{{font-weight:600;font-size:60px;letter-spacing:-0.02em;margin-top:84px}}
.end .btn{{margin-top:56px;background:{TEAL};color:{WELL};font-weight:600;font-size:48px;padding:28px 72px;border-radius:999px}}
.end .os{{margin-top:44px;font-weight:500;font-size:36px;color:{GREY}}}
.end .url{{margin-top:18px;font-weight:500;font-size:36px}}
</style></head><body>
<div class="strip"></div>
<div class="well"></div>
{chr(10).join(body)}
</body></html>
"""
    out = REPO / "content/carousels" / slug
    out.mkdir(parents=True, exist_ok=True)
    src = out / f"{slug}-canvas.html"
    src.write_text(page, encoding="utf-8", newline="\n")
    canvas = out / f"{slug}-canvas.png"
    subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--force-device-scale-factor=1",
                    f"--window-size={W * n},{H}", "--virtual-time-budget=4000", f"--screenshot={canvas}", src.as_uri()],
                   check=True, capture_output=True)
    im = Image.open(canvas).convert("RGB")
    assert im.size == (W * n, H), im.size
    for i in range(n):
        im.crop((i * W, 0, (i + 1) * W, H)).save(out / f"{slug}-{i + 1:02}.png", optimize=True)
    # A contact sheet: the slides side by side at a third of their size, as they're swiped.
    sheet = im.resize((W * n // 3, H // 3), Image.LANCZOS)
    sheet.save(out / f"{slug}-sheet.png", optimize=True)
    (out / f"{slug}-caption.txt").write_text(spec.CAPTION.format(link=LINK.lower()) + "\n", encoding="utf-8", newline="\n")
    print(f"{slug}: {n} slides in {out}")


if __name__ == "__main__":
    sys.path.insert(0, str(HERE))
    for name in sys.argv[1:] or ["comp"]:
        build(name)
