"""Draws the Fancy Plugs logo and favicon files into this folder.

    py -3.14 design/website/make-brand.py            SVGs only (needs fonttools)
    py -3.14 design/website/make-brand.py --png      also the PNG and ICO files (needs Pillow and Edge)

The mark is a knob that is a plug: the plugins' 300-degree ring from 7 o'clock, in the suite's
seven accents as one gradient, with a plug's two prongs coming out through the ring's gap.
"""
import math, os, subprocess, sys, tempfile
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
# docs/DESIGN_SYSTEM.md section 2, in spectrum order from 7 o'clock, clockwise.
ACCENTS = ["#E74D3D", "#E77E23", "#F1C315", "#2DCC70", "#00A985", "#3598DB", "#9A59B5"]
START, SWEEP = -150.0, 300.0


def rgb(h):
    return [int(h[i:i + 2], 16) for i in (1, 3, 5)]


def colour_at(t):
    x = min(max(t, 0), 1) * (len(ACCENTS) - 1)
    i = min(int(x), len(ACCENTS) - 2)
    f = x - i
    a, b = rgb(ACCENTS[i]), rgb(ACCENTS[i + 1])
    return "#%02X%02X%02X" % tuple(round(a[k] + (b[k] - a[k]) * f) for k in range(3))


def ring(cx, cy, r, sw, steps=100):
    """SVG can't stroke with a conic gradient, so the ring is short arcs, each one step of it."""
    def pt(deg):
        a = math.radians(deg)
        return cx + r * math.sin(a), cy - r * math.cos(a)
    step, out = SWEEP / steps, []
    for i in range(steps):
        a0 = START + i * step
        a1 = a0 + step + (0.5 if i < steps - 1 else 0)   # the overlap hides the seams
        (x0, y0), (x1, y1) = pt(a0), pt(a1)
        out.append(f'<path d="M{x0:.2f} {y0:.2f}A{r} {r} 0 0 1 {x1:.2f} {y1:.2f}" stroke="{colour_at((i + .5) / steps)}"/>')
    (xs, ys), (xe, ye) = pt(START), pt(START + SWEEP)    # round ends
    caps = (f'<circle cx="{xs:.2f}" cy="{ys:.2f}" r="{sw / 2}" fill="{ACCENTS[0]}"/>'
            f'<circle cx="{xe:.2f}" cy="{ye:.2f}" r="{sw / 2}" fill="{ACCENTS[-1]}"/>')
    return caps + f'<g fill="none" stroke-width="{sw}">' + "".join(out) + "</g>"


DEFS = ('<defs><linearGradient id="f" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4F4F4F"/>'
        '<stop offset="1" stop-color="#454545"/></linearGradient></defs>')


def mark(prong="#FFFFFF", pointer=True, draw_id=None):
    """draw_id: wrap the ring in a mask whose path the site animates, so the ring draws itself."""
    r = ring(32, 27, 21, 5)
    if draw_id:
        r = (f'<mask id="{draw_id}" maskUnits="userSpaceOnUse" x="0" y="0" width="64" height="64">'
             '<path class="draw-path" d="M21.50 45.19A21 21 0 1 1 42.50 45.19" pathLength="1" fill="none" stroke="#fff" '
             'stroke-width="9" stroke-linecap="round"/></mask>' f'<g mask="url(#{draw_id})">{r}</g>')
    s = (f'<rect x="25.75" y="38" width="3.75" height="22" rx="1.6" fill="{prong}"/>'
         f'<rect x="34.5" y="38" width="3.75" height="22" rx="1.6" fill="{prong}"/>'
         + r
         + '<circle cx="32" cy="27" r="14.5" fill="url(#f)"/>'
           '<circle cx="32" cy="27" r="14" fill="none" stroke="#000" stroke-opacity=".2"/>')
    if pointer:
        s += '<path d="M35.54 22.79L39.07 18.57" stroke="#FFF" stroke-width="2.4" stroke-linecap="round"/>'
    return s


def mono(c):
    return (f'<rect x="25.75" y="38" width="3.75" height="22" rx="1.6" fill="{c}"/>'
            f'<rect x="34.5" y="38" width="3.75" height="22" rx="1.6" fill="{c}"/>'
            f'<path d="M21.50 45.19A21 21 0 1 1 42.50 45.19" fill="none" stroke="{c}" stroke-width="5" stroke-linecap="round"/>'
            f'<circle cx="32" cy="27" r="14.5" fill="{c}"/>')


def favicon(rx):
    # Small sizes: thicker ring, wider prongs, no pointer or face border, filling the tile.
    return (f'<rect width="64" height="64" rx="{rx}" fill="#1E1E1E"/>'
            '<rect x="24" y="36" width="5.5" height="22" rx="2" fill="#FFF"/>'
            '<rect x="34.5" y="36" width="5.5" height="22" rx="2" fill="#FFF"/>'
            + ring(32, 26, 21, 7.5, 60) + '<circle cx="32" cy="26" r="12" fill="url(#f)"/>')


def app_icon():
    return ('<rect width="64" height="64" rx="14" fill="#1E1E1E"/>'
            f'<g transform="translate(8.3 8.9) scale(.74)">{mark()}</g>')


def svg(body, w=64, h=64, defs=True):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" role="img" aria-label="Fancy Plugs">'
            f'<title>Fancy Plugs</title>{DEFS if defs else ""}{body}</svg>\n')


# The wordmark is the plugins' title strip: Inter SemiBold, tracking 0.08 em, outlined.
font = TTFont(os.path.join(REPO, "assets/fonts/Inter-SemiBold.ttf"))
gs, cmap, upm = font.getGlyphSet(), font.getBestCmap(), font["head"].unitsPerEm


def text(s, x, baseline, size, fill):
    k, d = size / upm, ""
    for ch in s:
        g = gs[cmap[ord(ch)]]
        pen = SVGPathPen(gs, ntos=lambda v: f"{v:.2f}")
        g.draw(TransformPen(pen, (k, 0, 0, -k, x, baseline)))
        d += pen.getCommands()
        x += g.width * k + 0.08 * size
    return f'<path d="{d}" fill="{fill}"/>', x


def lockup(c1, c2, prong, draw_id=None):
    a, x = text("FANCY", 78, 40, 22, c1)
    b, x = text("PLUGS", x + 4.84, 40, 22, c2)
    return svg(mark(prong, draw_id=draw_id) + a + b, math.ceil(x + 2.24), 64)


def main():
    FILES = {
        "logo-mark.svg": svg(mark()),
        "logo-mark-on-light.svg": svg(mark("#141414")),
        "logo-mark-mono-white.svg": svg(mono("#FFFFFF"), defs=False),
        "logo-mark-mono-black.svg": svg(mono("#141414"), defs=False),
        "logo-lockup.svg": lockup("#8D8D8D", "#FFFFFF", "#FFFFFF"),
        "logo-lockup-on-light.svg": lockup("#6B6B6B", "#141414", "#141414"),
        "favicon.svg": svg(favicon(14)),
        "favicon-square.svg": svg(favicon(0)),
        "app-icon.svg": svg(app_icon()),
    }
    for name, s in FILES.items():
        open(os.path.join(HERE, name), "w", encoding="utf-8", newline="\n").write(s)

    if "--png" in sys.argv:
        from PIL import Image
        edge = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

        def shot(name):
            with tempfile.TemporaryDirectory() as t:
                page, png = os.path.join(t, "p.html"), os.path.join(t, "p.png")
                src = "file:///" + os.path.join(HERE, name).replace("\\", "/")
                open(page, "w").write('<!doctype html><style>html,body{margin:0;background:transparent}img{display:block}</style>'
                                      f'<img src="{src}" width="512" height="512">')
                subprocess.run([edge, "--headless", "--disable-gpu", "--hide-scrollbars", "--default-background-color=00000000",
                                "--force-device-scale-factor=1", "--window-size=512,512", f"--screenshot={png}",
                                "file:///" + page.replace("\\", "/")], check=True, capture_output=True)
                return Image.open(png).convert("RGBA")

        a, q = shot("favicon.svg"), shot("favicon-square.svg").convert("RGB")
        a.save(os.path.join(HERE, "favicon.ico"), sizes=[(16, 16), (32, 32), (48, 48)])
        a.resize((32, 32), Image.LANCZOS).save(os.path.join(HERE, "favicon-32.png"))
        a.resize((16, 16), Image.LANCZOS).save(os.path.join(HERE, "favicon-16.png"))
        q.resize((180, 180), Image.LANCZOS).save(os.path.join(HERE, "apple-touch-icon.png"))
        q.resize((192, 192), Image.LANCZOS).save(os.path.join(HERE, "icon-192.png"))
        q.save(os.path.join(HERE, "icon-512.png"))
    print("wrote", len(FILES), "SVGs" + (" and the PNG/ICO files" if "--png" in sys.argv else ""))


if __name__ == "__main__":
    main()
