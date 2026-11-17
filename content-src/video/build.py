"""Writes the HyperFrames projects for the Fancy Plugs intro film.

    python content-src/video/build.py wide     16:9, 1920 x 1080
    python content-src/video/build.py tall     9:16, 1080 x 1920

The film's facts live in film.py (script, shots, timings); this file turns them
into index.html and one scene file per beat. Screenshots come from docs/images,
the logo from design/website, the fonts from assets/fonts. It also writes the
subtitle file, content/fancy-plugs-intro-<16x9|9x16>.srt. Needs Pillow.
"""
import json
import re
import shutil
import sys
import wave
from pathlib import Path

from PIL import Image

import film

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
FONT_CSS = """
@font-face{font-family:InterFP;font-weight:400;src:url('assets/fonts/Inter-Regular.ttf')}
@font-face{font-family:InterFP;font-weight:500;src:url('assets/fonts/Inter-Medium.ttf')}
@font-face{font-family:InterFP;font-weight:600;src:url('assets/fonts/Inter-SemiBold.ttf')}
"""
TEAL, GREY, WELL, PAGE, PANEL = "#00A985", "#8D8D8D", "#141414", "#1E1E1E", "#262626"
# Purple fails 4.5:1 as text on the dark grounds, so text uses the lighter one.
TEXT_ACCENT = {"#9A59B5": "#A970C2"}
MARKS = []


def n(x):
    return f"{x:.3f}".rstrip("0").rstrip(".")


def scene_file(cid, w, h, css, body, js):
    return f"""<template>
<style>{FONT_CSS}
#root{{position:absolute;inset:0;font-family:InterFP,sans-serif;color:#fff}}
{css}</style>
<div id="root" data-composition-id="{cid}" data-width="{w}" data-height="{h}">
{body}
</div>
<script>
(function(){{
const tl = gsap.timeline({{ paused: true }});
{js}
window.__timelines["{cid}"] = tl;
}})();
</script>
</template>
"""


def logo_svg(prefix, wordmark=True):
    """The lockup from design/website, with ids so the timeline can perform it."""
    s = (REPO / "design/website/logo-lockup.svg").read_text(encoding="utf-8")
    s = re.sub(r"<title>.*?</title>", "", s)
    s = s.replace('id="f"', f'id="{prefix}f"').replace("url(#f)", f"url(#{prefix}f)")
    s = s.replace('role="img" aria-label="Fancy Plugs"', f'id="{prefix}svg" role="img" aria-label="Fancy Plugs"')
    mask = (f'<mask id="{prefix}m" maskUnits="userSpaceOnUse" x="0" y="0" width="64" height="64">'
            f'<path id="{prefix}arc" d="M21.5 45.19A21 21 0 1 1 42.5 45.19" fill="none" stroke="#fff" '
            f'stroke-width="8" pathLength="100" stroke-dasharray="100" stroke-dashoffset="100"/></mask>')
    s = s.replace("</defs>", mask + "</defs>")
    rects = iter(["a", "b"])
    s = re.sub(r'<rect ', lambda m: f'<rect id="{prefix}prong{next(rects)}" ', s)
    caps = iter(["capa", "capb"])
    s = re.sub(r'<circle cx="(21|42)\.50"', lambda m: f'<circle id="{prefix}{next(caps)}" cx="{m.group(1)}.50"', s)
    s = s.replace('<g fill="none" stroke-width="5">', f'<g fill="none" stroke-width="5" mask="url(#{prefix}m)">')
    s = s.replace('<path d="M35.54 22.79', f'<path id="{prefix}ptr" d="M35.54 22.79')
    s = s.replace('fill="#8D8D8D"/>', f'fill="#8D8D8D" id="{prefix}w1"/>')
    s = s.replace('fill="#FFFFFF"/></svg>', f'fill="#FFFFFF" id="{prefix}w2"/></svg>')
    if not wordmark:
        s = re.sub(r'<path d="[^"]*" fill="#8D8D8D"[^>]*/><path d="[^"]*" fill="#FFFFFF"[^>]*/>', "", s)
        s = s.replace('viewBox="0 0 254 64"', 'viewBox="0 0 64 64"')
    return s


def logo_js(p, t0, draw=1.2, wordmark=True):
    """The knob is turned up: the ring draws, the pointer follows, the prongs come out."""
    js = f"""
tl.fromTo("#{p}arc",{{attr:{{"stroke-dashoffset":100}}}},{{attr:{{"stroke-dashoffset":0}},duration:{n(draw)},ease:"power2.inOut"}},{n(t0)});
tl.fromTo("#{p}ptr",{{rotation:-300,svgOrigin:"32 27"}},{{rotation:0,svgOrigin:"32 27",duration:{n(draw)},ease:"power2.inOut"}},{n(t0)});
tl.fromTo("#{p}capb",{{opacity:0}},{{opacity:1,duration:0.05}},{n(t0 + draw - 0.08)});
tl.fromTo(["#{p}pronga","#{p}prongb"],{{scaleY:0,transformOrigin:"50% 0%"}},{{scaleY:1,transformOrigin:"50% 0%",duration:0.32,ease:"power3.out",stagger:0.06}},{n(t0 + draw)});
"""
    if wordmark:
        js += f"""
tl.fromTo("#{p}w1",{{opacity:0,filter:"blur(1.2px)"}},{{opacity:1,filter:"blur(0px)",duration:0.2,ease:"power2.out"}},{n(t0 + draw + 0.22)});
tl.fromTo("#{p}w2",{{opacity:0,filter:"blur(1.2px)"}},{{opacity:1,filter:"blur(0px)",duration:0.2,ease:"power2.out"}},{n(t0 + draw + 0.34)});
"""
    return js


def ring(colour, size, sw=3):
    """The knob ring: a 300 degree arc from 7 o'clock."""
    r = (size - sw) / 2
    c = size / 2
    return (f'<svg width="{size}" height="{size}" viewBox="0 0 {size} {size}" aria-hidden="true">'
            f'<circle cx="{c}" cy="{c}" r="{n(r)}" fill="none" stroke="{colour}" stroke-width="{sw}" '
            f'stroke-linecap="round" pathLength="360" stroke-dasharray="300 60" transform="rotate(120 {c} {c})"/></svg>')


class Fmt:
    def __init__(self, name):
        self.name = name
        self.tall = name == "tall"
        self.W, self.H = (1080, 1920) if self.tall else (1920, 1080)
        f = film.TALL if self.tall else film.WIDE
        self.f = f
        self.strip_h = f["strip_h"]
        self.stage = f["stage"]          # x, y, w, h
        self.out = HERE / name


def shots_scene(F, sc):
    """A group scene: a chapter word, then one plugin window after another on one stage."""
    sid = sc["id"]
    sx, sy, sw, sh = F.stage
    css = f"""
#{sid}-stage{{position:absolute;left:{sx}px;top:{sy}px;width:{sw}px;height:{sh}px;overflow:hidden;border-radius:{F.f.get('stage_r', 0)}px}}
.{sid}-shot{{position:absolute;inset:0}}
.{sid}-in,.{sid}-cam{{position:absolute;inset:0;display:flex;align-items:center;justify-content:center}}
.{sid}-win{{position:relative;flex:none;border-radius:8px;box-shadow:0 22px 56px rgba(0,0,0,.5)}}
.{sid}-win img{{position:absolute;inset:0;width:100%;height:100%;border-radius:8px;display:block}}
.{sid}-mk{{position:absolute;width:0;height:0}}
.{sid}-mk svg{{position:absolute;overflow:visible}}
#{sid}-chap{{position:absolute;left:0;right:0;top:{sy}px;height:{sh}px}}
#{sid}-chapin{{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;gap:{F.f['chap_gap']}px;
 font-weight:600;font-size:{F.f['chap_px']}px;letter-spacing:-0.02em}}
"""
    body, js = [], []
    if sc.get("chapter"):
        a, b = sc["chapter_at"]
        body.append(f'<div id="{sid}-chap" class="clip" data-start="{n(a)}" data-duration="{n(b - a)}" data-track-index="2">'
                    f'<div id="{sid}-chapin">{ring(TEAL, F.f["chap_ring"], F.f["chap_ring_sw"])}<span>{sc["chapter"]}</span></div></div>')
        js.append(f'tl.fromTo("#{sid}-chapin",{{opacity:0,filter:"blur(10px)"}},{{opacity:1,filter:"blur(0px)",duration:0.2,ease:"power2.out"}},{n(a)});')
        js.append(f'tl.to("#{sid}-chapin",{{opacity:0,filter:"blur(8px)",duration:0.16,ease:"power2.in"}},{n(b - 0.17)});')
    body.append(f'<div id="{sid}-stage" data-layout-allow-overflow>')
    for i, sh_ in enumerate(sc["shots"]):
        k = f"{sid}-{i}"
        iw, ih = Image.open(REPO / "docs/images" / f"{sh_['img']}.png").size
        fit = min(sw / iw, sh / ih, 1.0)
        if sh_.get("crop") == "fill":
            fit = max(sw / iw, sh / ih)      # the 9:16 cut: a wide window covers the tall stage and the camera picks the part
        elif sh_.get("crop"):
            fit = sh_["crop"]
        ww, wh = iw * fit, ih * fit
        t0, t1 = sh_["t"]
        body.append(f'<div id="{k}" class="clip {sid}-shot" data-start="{n(t0)}" data-duration="{n(t1 - t0)}" data-track-index="1">'
                    f'<div id="{k}-in" class="{sid}-in"><div id="{k}-cam" class="{sid}-cam">'
                    f'<div class="{sid}-win" style="width:{n(ww)}px;height:{n(wh)}px">'
                    f'<img id="{k}-a" src="assets/screens/{sh_["img"]}.png" alt="{sh_["alt"]}" width="{iw}" height="{ih}">')
        if sh_.get("img2"):
            body.append(f'<img id="{k}-b" src="assets/screens/{sh_["img2"]}.png" alt="{sh_["alt2"]}" width="{iw}" height="{ih}">')
        cams = sh_.get("cams", [])
        marks = sh_.get("marks", [])
        msz = F.f["marker"]
        for j, (ma, mb, mx_, my_) in enumerate(marks):
            body.append(f'<div id="{k}-m{j}" class="{sid}-mk" style="left:{mx_}%;top:{my_}%">'
                        f'<svg width="{msz}" height="{msz}" viewBox="0 0 100 100" style="left:-{msz // 2}px;top:-{msz // 2}px" aria-hidden="true">'
                        f'<circle cx="50" cy="50" r="44" fill="none" stroke="#141414" stroke-opacity=".55" stroke-width="10"/>'
                        f'<circle id="{k}-r{j}" cx="50" cy="50" r="44" fill="none" stroke="{sh_["accent"]}" stroke-width="6" stroke-linecap="round" '
                        f'pathLength="360" stroke-dasharray="360" stroke-dashoffset="60" transform="rotate(120 50 50)"/></svg></div>')
        body.append("</div></div></div></div>")
        first = i == 0
        if first:
            js.append(f'tl.fromTo("#{k}-in",{{opacity:0,scale:0.965,filter:"blur(10px)"}},{{opacity:1,scale:1,filter:"blur(0px)",duration:0.45,ease:"expo.out"}},{n(t0)});')
        else:
            js.append(f'tl.fromTo("#{k}-in",{{opacity:0,x:{F.f["step"]},filter:"blur(10px)"}},{{opacity:1,x:0,filter:"blur(0px)",duration:0.4,ease:"expo.out"}},{n(t0)});')
        last = i == len(sc["shots"]) - 1
        if not (last and sc.get("hold_last")):
            js.append(f'tl.to("#{k}-in",{{opacity:0,x:-{F.f["step"]},filter:"blur(8px)",duration:0.2,ease:"power2.in"}},{n(t1 - 0.21)});')
        else:
            js.append(f'tl.to("#{k}-in",{{opacity:0,filter:"blur(8px)",duration:0.22,ease:"power2.in"}},{n(t1 - 0.23)});')
        if sh_.get("img2"):
            js.append(f'tl.fromTo("#{k}-b",{{opacity:0}},{{opacity:1,duration:0.3,ease:"power1.inOut"}},{n(sh_["xfade"])});')
        def pos(fx, fy, z):
            # Move toward the point, but never so far that empty stage shows beside a window that could fill it.
            # The 9:16 cut allows a margin of "slack" px there, so a control at a window's edge can sit nearer the middle.
            kx, ky = F.f["pull"]
            mx, my = max(0, (ww * z - sw) / 2), max(0, (wh * z - sh) / 2)
            if mx:
                mx += F.f.get("slack", 0)
            return (max(-mx, min(mx, (0.5 - fx / 100) * ww * z * kx)),
                    max(-my, min(my, (0.5 - fy / 100) * wh * z * ky)), z)

        x, y, z = pos(*sh_["start"]) if sh_.get("start") else (0, 0, 1)
        js.append(f'tl.set("#{k}-cam",{{x:{n(x)},y:{n(y)},scale:{n(z)}}},0);')
        for t, fx, fy, z in cams:
            x, y, z = pos(fx, fy, z)
            js.append(f'tl.to("#{k}-cam",{{x:{n(x)},y:{n(y)},scale:{n(z)},duration:0.6,ease:"power2.inOut"}},{n(t)});')
        for j, (ma, mb, _, _) in enumerate(marks):
            MARKS.append((sid, sh_["img"], round(ma + 0.5, 2)))
            js.append(f'tl.fromTo("#{k}-m{j}",{{opacity:0}},{{opacity:1,duration:0.14}},{n(ma)});')
            js.append(f'tl.fromTo("#{k}-r{j}",{{attr:{{"stroke-dashoffset":60}}}},{{attr:{{"stroke-dashoffset":0}},duration:0.3,ease:"power2.out"}},{n(ma + 0.06)});')
            js.append(f'tl.to("#{k}-m{j}",{{opacity:0,duration:0.12}},{n(mb - 0.13)});')
    body.append("</div>")
    return scene_file(sid, F.W, F.H, css, "\n".join(body), "\n".join(js))


def intro_scene(F, sc):
    sid = sc["id"]
    lw = F.f["logo_w"]
    css = f"""
#{sid}-logo{{position:absolute;left:{F.f['safe_x']}px;right:{F.f['safe_r']}px;top:{F.f['logo_y']}px;height:{n(lw * 64 / 254)}px}}
#{sid}-logoin{{position:absolute;inset:0;display:flex;justify-content:center}}
#{sid}-logoin svg{{width:{lw}px;height:{n(lw * 64 / 254)}px;overflow:visible}}
"""
    body = (f'<div id="{sid}-logo" class="clip" data-start="0" data-duration="{n(sc["dur"])}" data-track-index="1">'
            f'<div id="{sid}-logoin">{logo_svg(sid + "-")}</div></div>')
    js = logo_js(sid + "-", 0.15, 1.25)
    js += f'tl.to("#{sid}-logoin",{{opacity:0,filter:"blur(8px)",duration:0.22,ease:"power2.in"}},{n(sc["dur"] - 0.24)});'
    if sc.get("headline"):
        # A vertical clip's first frame is its thumbnail, so the line is on screen from frame 0.
        css += f"""
#{sid}-head{{position:absolute;left:{F.f['safe_x']}px;right:{F.f['safe_r']}px;top:{F.f['head_y']}px;height:{F.f['head_h']}px}}
#{sid}-headin{{position:absolute;inset:0;display:flex;align-items:flex-start;justify-content:center;text-align:center;
 font-weight:600;font-size:{F.f['head_px']}px;line-height:1.08;letter-spacing:-0.02em}}
"""
        body += (f'<div id="{sid}-head" class="clip" data-start="0" data-duration="{n(sc["dur"])}" data-track-index="2">'
                 f'<div id="{sid}-headin">{sc["headline"]}</div></div>')
        js += f'tl.to("#{sid}-headin",{{opacity:0,filter:"blur(8px)",duration:0.22,ease:"power2.in"}},{n(sc["dur"] - 0.24)});'
    return scene_file(sid, F.W, F.H, css, body, js)


def roster_scene(F, sc, data):
    sid = sc["id"]
    by, bh = F.f.get("roster_box", (F.stage[1], F.stage[3]))
    mid = "center" if F.f.get("centred") else "normal"
    rx = F.f.get("roster_x", F.f["safe_x"])
    css = f"""
#{sid}-box{{position:absolute;left:{rx}px;right:{rx}px;top:{by}px;height:{bh}px}}
#{sid}-in{{position:absolute;inset:0;display:flex;flex-direction:column;justify-content:center;gap:{F.f['roster_gap']}px}}
.{sid}-g{{display:flex;flex-direction:column;align-items:{mid};gap:{F.f['roster_lab_gap']}px}}
.{sid}-lab{{font-size:{F.f['roster_lab']}px;color:{GREY};font-weight:500}}
.{sid}-row{{display:flex;flex-wrap:wrap;justify-content:{mid};gap:{F.f['roster_row_gap']}}}
.{sid}-n{{display:flex;align-items:center;gap:{F.f['roster_ring_gap']}px;font-size:{F.f['roster_px']}px;font-weight:600;white-space:nowrap}}
"""
    body = [f'<div id="{sid}-box" class="clip" data-start="0" data-duration="{n(sc["dur"])}" data-track-index="1"><div id="{sid}-in">']
    for g in data["groups"]:
        body.append(f'<div class="{sid}-g"><div class="{sid}-lab {sid}-fade">{g["name"]}</div><div class="{sid}-row">')
        for slug in g["plugins"]:
            p = data["plugins"][slug]
            body.append(f'<div class="{sid}-n {sid}-fade">{ring(p["accent"], F.f["roster_ring"], F.f["roster_ring_sw"])}<span>{p["name"]}</span></div>')
        body.append("</div></div>")
    body.append("</div></div>")
    js = (f'tl.fromTo(".{sid}-fade",{{opacity:0,filter:"blur(8px)"}},{{opacity:1,filter:"blur(0px)",duration:0.22,ease:"power2.out",stagger:0.045}},0.1);\n'
          f'tl.to("#{sid}-in",{{opacity:0,filter:"blur(8px)",duration:0.2,ease:"power2.in"}},{n(sc["dur"] - 0.22)});')
    return scene_file(sid, F.W, F.H, css, "\n".join(body), js)


def end_scene(F, sc, rel):
    sid = sc["id"]
    lw = F.f["end_logo_w"]
    css = f"""
#{sid}-card{{position:absolute;inset:0}}
#{sid}-col{{position:absolute;left:{F.f['safe_x']}px;right:{F.f['safe_r']}px;top:{F.f['end_y']}px;display:flex;flex-direction:column;align-items:center}}
#{sid}-col svg.lock, #{sid}-lock svg{{width:{lw}px;height:{n(lw * 64 / 254)}px;overflow:visible}}
#{sid}-btn{{margin-top:{F.f['end_gap']}px;background:{TEAL};color:{WELL};font-weight:600;font-size:{F.f['btn_px']}px;
 padding:{F.f['btn_pad']};border-radius:999px}}
#{sid}-os{{margin-top:{F.f['end_gap2']}px;font-size:{F.f['os_px']}px;color:{GREY};font-weight:500}}
#{sid}-url{{margin-top:{F.f['end_gap3']}px;font-size:{F.f['url_px']}px;font-weight:500}}
"""
    parts = dict(btn=f'<div id="{sid}-btn">Download {rel["version"]}</div>',
                 os=f'<div id="{sid}-os">Windows · Mac (beta)</div>',
                 url=f'<div id="{sid}-url">{film.LINK[F.name]}</div>')
    order = ("btn", "os", "url")
    body = (f'<div id="{sid}-card" class="clip" data-start="0" data-duration="{n(sc["dur"])}" data-track-index="1"><div id="{sid}-col">'
            f'<div id="{sid}-lock">{logo_svg(sid + "-")}</div>' + "".join(parts[k] for k in order) + '</div></div>')
    js = logo_js(sid + "-", 0.1, 0.8)
    js += (f'tl.fromTo("#{sid}-btn",{{opacity:0,scale:0.9}},{{opacity:1,scale:1,duration:0.4,ease:"expo.out"}},1.25);\n'
           f'tl.fromTo("#{sid}-os",{{opacity:0,filter:"blur(6px)"}},{{opacity:1,filter:"blur(0px)",duration:0.2,ease:"power2.out"}},1.6);\n'
           f'tl.fromTo("#{sid}-url",{{opacity:0,filter:"blur(6px)"}},{{opacity:1,filter:"blur(0px)",duration:0.2,ease:"power2.out"}},1.78);')
    return scene_file(sid, F.W, F.H, css, body, js)


def strip_scene(F, segs, t0, t1):
    """The plugins' title strip, along the top of the film: FANCY and a name, with a note at the right."""
    h = F.strip_h
    centred = F.f.get("centred")
    css = f"""
#strip-bar{{position:absolute;left:0;top:{F.f['strip_y']}px;width:100%;height:{h}px}}
#strip-bg{{position:absolute;inset:0;background:{WELL};border-bottom:2px solid {PANEL}}}
.strip-seg{{position:absolute;left:{F.f['safe_x']}px;right:{F.f['safe_r']}px;top:{F.f['strip_y']}px;height:{h}px}}
.strip-in{{position:absolute;inset:0;display:flex;align-items:center;justify-content:space-between;
 font-weight:600;font-size:{F.f['strip_px']}px;letter-spacing:.08em;text-transform:uppercase}}
.strip-in .f{{color:{GREY};margin-right:.5em}}
.strip-in .r{{color:{GREY};letter-spacing:0;text-transform:none;font-weight:500}}
"""
    body = [f'<div id="strip-bar" class="clip" data-start="{n(t0)}" data-duration="{n(t1 - t0)}" data-track-index="1"><div id="strip-bg"></div></div>']
    js = [f'tl.fromTo("#strip-bg",{{yPercent:-100}},{{yPercent:0,duration:0.35,ease:"power3.out"}},{n(t0)});',
          f'tl.to("#strip-bg",{{yPercent:-100,duration:0.25,ease:"power2.in"}},{n(t1 - 0.26)});']
    if centred:
        # The 9:16 cut has no bar: the group and the kind of plugin are one centred line above the caption.
        css += ".strip-in{justify-content:center;gap:.9em}"
        body, js = [], []
    for i, (a, b, name, colour, right) in enumerate(segs):
        body.append(f'<div id="strip-{i}" class="clip strip-seg" data-start="{n(a)}" data-duration="{n(b - a)}" data-track-index="2">'
                    f'<div id="strip-{i}-in" class="strip-in"><span><span class="f">Fancy</span>'
                    f'<span style="color:{TEXT_ACCENT.get(colour, colour)}">{name}</span></span><span class="r">{right}</span></div></div>')
        js.append(f'tl.fromTo("#strip-{i}-in",{{opacity:0}},{{opacity:1,duration:0.2,ease:"power1.out"}},{n(a + (0.15 if i == 0 else 0))});')
        js.append(f'tl.to("#strip-{i}-in",{{opacity:0,duration:0.14,ease:"power1.in"}},{n(b - 0.15)});')
    return scene_file("strip", F.W, F.H, css, "\n".join(body), "\n".join(js))


def captions_scene(F, caps):
    css = f"""
.cap{{position:absolute;left:{F.f['cap_x']}px;right:{F.f['cap_r']}px;top:{F.f['cap_y']}px;height:{F.f['cap_h']}px}}
.cap-in{{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;text-align:center;
 font-weight:{F.f.get('cap_w', 500)};font-size:{F.f['cap_px']}px;line-height:1.2;text-wrap:balance}}
"""
    body, js = [], []
    for i, (a, b, text) in enumerate(caps):
        body.append(f'<div id="cap-{i}" class="clip cap" data-start="{n(a)}" data-duration="{n(b - a)}" data-track-index="1">'
                    f'<div id="cap-{i}-in" class="cap-in">{text}</div></div>')
        js.append(f'tl.fromTo("#cap-{i}-in",{{opacity:0}},{{opacity:1,duration:0.1,ease:"power1.out"}},{n(a)});')
        js.append(f'tl.to("#cap-{i}-in",{{opacity:0,duration:0.1,ease:"power1.in"}},{n(b - 0.11)});')
    return scene_file("captions", F.W, F.H, css, "\n".join(body), "\n".join(js))


def wav_len(path):
    with wave.open(str(path)) as w:
        return w.getnframes() / w.getframerate()


def stamp(t):
    ms = round(t * 1000)
    return f"{ms // 3600000:02}:{ms // 60000 % 60:02}:{ms // 1000 % 60:02},{ms % 1000:03}"


def main():
    F = Fmt(sys.argv[1] if len(sys.argv) > 1 else "wide")
    data = json.loads((REPO / "site-src/data/plugins.json").read_text(encoding="utf-8"))
    rel = json.loads((REPO / "site-src/data/release.json").read_text(encoding="utf-8"))
    scenes = film.scenes(F.name, data)
    out = F.out
    (out / "compositions").mkdir(parents=True, exist_ok=True)
    for d in ("assets/screens", "assets/fonts", "assets/voice"):
        (out / d).mkdir(parents=True, exist_ok=True)
    for f in (REPO / "assets/fonts").glob("Inter-*.ttf"):
        shutil.copy(f, out / "assets/fonts" / f.name)

    hosts, audio, caps, segs, subs = [], [], [], [], []
    t = 0.0
    for sc in scenes:
        sc["at"] = t
        kind = sc["kind"]
        if kind == "intro":
            html = intro_scene(F, sc)
        elif kind == "shots":
            for s in sc["shots"]:
                for key in ("img", "img2"):
                    if s.get(key):
                        shutil.copy(REPO / "docs/images" / f"{s[key]}.png", out / "assets/screens" / f"{s[key]}.png")
            html = shots_scene(F, sc)
        elif kind == "roster":
            html = roster_scene(F, sc, data)
        else:
            html = end_scene(F, sc, rel)
        (out / "compositions" / f"{sc['id']}.html").write_text(html, encoding="utf-8", newline="\n")
        hosts.append(f'<div id="{sc["id"]}" data-composition-id="{sc["id"]}" data-composition-src="compositions/{sc["id"]}.html" '
                     f'data-start="{n(t)}" data-duration="{n(sc["dur"])}" data-track-index="1" data-track-kind="graphics" '
                     f'data-width="{F.W}" data-height="{F.H}"></div>')
        if sc.get("voice"):
            audio.append(f'<audio id="vo-{sc["id"]}" class="clip" src="assets/voice/{sc["voice"]}.wav" data-start="{n(t + sc["vo"])}" data-duration="{n(wav_len(out / "assets/voice" / (sc["voice"] + ".wav")))}" '
                         f'data-track-index="4" data-volume="1.65"></audio>')
        for a, b, text in sc.get("caps", []):
            caps.append((t + sc["vo"] + a, t + sc["vo"] + b, text))
        # The subtitle file holds every spoken line: the captions, and a line shown another way (the 9:16 headline).
        for a, b, text in sc.get("subs", sc.get("caps", [])):
            subs.append((t + sc["vo"] + a, t + sc["vo"] + b, text))
        for a, b, name, colour, right in sc.get("strip", []):
            segs.append((t + a, t + b, name, colour, right))
        t += sc["dur"]
    total = t
    (out / "compositions/strip.html").write_text(strip_scene(F, segs, segs[0][0], segs[-1][1]), encoding="utf-8", newline="\n")
    (out / "compositions/captions.html").write_text(captions_scene(F, caps), encoding="utf-8", newline="\n")
    hosts.append(f'<div id="strip" data-composition-id="strip" data-composition-src="compositions/strip.html" data-start="0" '
                 f'data-duration="{n(total)}" data-track-index="2" data-track-kind="graphics" data-width="{F.W}" data-height="{F.H}"></div>')
    hosts.append(f'<div id="captions" data-composition-id="captions" data-composition-src="compositions/captions.html" data-start="0" '
                 f'data-duration="{n(total)}" data-track-index="3" data-track-kind="captions" data-width="{F.W}" data-height="{F.H}"></div>')
    index = f"""<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width={F.W}, height={F.H}" />
    <title>Fancy Plugs: the intro film ({F.name})</title>
    <script src="https://cdn.jsdelivr.net/npm/gsap@3.15.0/dist/gsap.min.js"></script>
    <style>
      * {{ margin: 0; padding: 0; box-sizing: border-box; }}
      html, body {{ margin: 0; width: {F.W}px; height: {F.H}px; overflow: hidden; background: {PAGE}; }}
      #ground {{ position: absolute; inset: 0; background: {PAGE}; }}
      #strip {{ z-index: 5; }}
      #captions {{ z-index: 6; }}
    </style>
  </head>
  <body>
    <div id="root" data-composition-id="main" data-start="0" data-duration="{n(total)}" data-width="{F.W}" data-height="{F.H}">
      <div id="ground"></div>
      {chr(10).join('      ' + h for h in hosts).strip()}
      {chr(10).join('      ' + a for a in audio).strip()}
    </div>
    <script>
      window.__timelines = window.__timelines || {{}};
      window.__timelines["main"] = gsap.timeline({{ paused: true }});
    </script>
  </body>
</html>
"""
    (out / "index.html").write_text(index, encoding="utf-8", newline="\n")
    (REPO / "content").mkdir(exist_ok=True)
    srt = REPO / "content" / f"fancy-plugs-intro-{'9x16' if F.tall else '16x9'}.srt"
    srt.write_text("".join(f"{i}\n{stamp(a)} --> {stamp(b)}\n{text}\n\n" for i, (a, b, text) in enumerate(subs, 1)),
                   encoding="utf-8", newline="\n")
    at = {sc["id"]: sc["at"] for sc in scenes}
    print("camera stops:", ",".join(n(at[a] + c) for a, _, c in MARKS))
    print(f"{F.name}: {len(scenes)} scenes, {len(caps)} captions, {total:.2f} s")


if __name__ == "__main__":
    main()
