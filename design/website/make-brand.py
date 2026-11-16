import math, sys, os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

repo, out = sys.argv[1], sys.argv[2]
CX, CY, R, SW = 32, 27, 21, 5
def pt(deg, r=R):
    a = math.radians(deg); return (CX + r*math.sin(a), CY - r*math.cos(a))
def arc(a0, a1, r=R):
    x0, y0 = pt(a0, r); x1, y1 = pt(a1, r)
    large = 1 if (a1 - a0) > 180 else 0
    return f"M{x0:.2f} {y0:.2f}A{r} {r} 0 {large} 1 {x1:.2f} {y1:.2f}"

TEAL = "#00A985"
SUITE = ["#E74D3D", "#E77E23", "#F1C315", "#2DCC70", "#00A985", "#3598DB", "#9A59B5"]
PRONGS = '<rect x="25.75" y="38" width="3.75" height="22" rx="1.6" fill="{p}"/><rect x="34.5" y="38" width="3.75" height="22" rx="1.6" fill="{p}"/>'
FACE = ('<circle cx="32" cy="27" r="14.5" fill="url(#f)"/>'
        '<circle cx="32" cy="27" r="14" fill="none" stroke="#000" stroke-opacity=".2"/>')
DEFS = '<defs><linearGradient id="f" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4F4F4F"/><stop offset="1" stop-color="#454545"/></linearGradient></defs>'
def pointer():
    (x0, y0), (x1, y1) = pt(40, 5.5), pt(40, 11)
    return f'<path d="M{x0:.2f} {y0:.2f}L{x1:.2f} {y1:.2f}" stroke="#FFF" stroke-width="2.4" stroke-linecap="round"/>'

def mark(kind="teal", with_pointer=True, prong="#FFFFFF"):
    if kind == "suite":
        seg, gap, a = (300 - 6*7) / 7, 7, 210 - 360
        arcs = ""
        for c in SUITE:
            arcs += f'<path d="{arc(a, a+seg)}" fill="none" stroke="{c}" stroke-width="{SW}" stroke-linecap="round"/>'
            a += seg + gap
    else:
        arcs = f'<path d="{arc(-150, 150)}" fill="none" stroke="{TEAL}" stroke-width="{SW}" stroke-linecap="round"/>'
    return PRONGS.format(p=prong) + arcs + FACE + (pointer() if with_pointer else "")

def mono(colour):
    # One-colour version: ring, solid face cut by a pointer, prongs.
    return (PRONGS.format(p=colour)
            + f'<path d="{arc(-150, 150)}" fill="none" stroke="{colour}" stroke-width="{SW}" stroke-linecap="round"/>'
            + f'<circle cx="32" cy="27" r="14.5" fill="{colour}"/>')

def svg(body, w=64, h=64, defs=True, title="Fancy Plugs"):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" role="img" aria-label="{title}">'
            f'<title>{title}</title>{DEFS if defs else ""}{body}</svg>\n')

def tile(inner, scale=0.80):
    off = (64 - 64*scale) / 2
    return (f'<rect width="64" height="64" rx="14" fill="#1E1E1E"/>'
            f'<g transform="translate({off:.2f} {off+0.6:.2f}) scale({scale})">{inner}</g>')

def fav(rx):
    # Small sizes: thicker ring, wider prongs, no pointer or face border, filling the tile.
    return (f'<rect width="64" height="64" rx="{rx}" fill="#1E1E1E"/>'
            '<rect x="24" y="36" width="5.5" height="22" rx="2" fill="#FFF"/><rect x="34.5" y="36" width="5.5" height="22" rx="2" fill="#FFF"/>'
            '<path d="M21.50 44.19A21 21 0 1 1 42.50 44.19" fill="none" stroke="#00A985" stroke-width="7.5" stroke-linecap="round"/>'
            '<circle cx="32" cy="26" r="12" fill="url(#f)"/>')

# Wordmark outlined from Inter SemiBold, tracking 0.08 em: the title strip's own setting.
font = TTFont(os.path.join(repo, "assets/fonts/Inter-SemiBold.ttf"))
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

def lockup(kind, c1, c2, mk=None):
    size = 22
    a, x = text("FANCY", 78, 40, size, c1)
    b, x = text("PLUGS", x + size*0.22, 40, size, c2)
    w = math.ceil(x - 0.08*size + 4)
    return svg((mk or mark(kind)) + a + b, w, 64, defs=mk is None)

files = {
    "logo-mark.svg": svg(mark("teal")),
    "logo-mark-suite.svg": svg(mark("suite")),
    "logo-mark-mono-white.svg": svg(mono("#FFFFFF"), defs=False),
    "logo-mark-mono-black.svg": svg(mono("#141414"), defs=False),
    "logo-lockup.svg": lockup("teal", "#8D8D8D", "#FFFFFF"),
    "logo-lockup-on-light.svg": lockup("teal", "#6B6B6B", "#141414", mk=None).replace('fill="#FFFFFF"/><rect', 'fill="#141414"/><rect').replace('rx="1.6" fill="#FFFFFF"', 'rx="1.6" fill="#141414"'),
    "logo-mark-on-light.svg": svg(mark("teal", prong="#141414")),
    "favicon.svg": svg(fav(14)),
    "favicon-square.svg": svg(fav(0)),
    "app-icon.svg": svg(tile(mark("teal"), 0.74)),
}
for n, s in files.items():
    open(os.path.join(out, n), "w", encoding="utf-8", newline="\n").write(s)
print("\n".join(f"{n} {len(s)}" for n, s in files.items()))
