"""Phase 4 Wave 2 mockups, drawn as Wave 1's were (w1_mockups.py): the plugin's real screenshot at
1x with the new parts over it, outlined in dashed yellow, and notes beside it.

    py -3 design/phase4-mockup-src/w2_mockups.py [plugin ...]

Writes design/phase4-mockup-src/w2-<plugin>.html and reference/phase4/<nn>__<plugin>_w2_mockup.png.
"""
import math
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import w1_mockups as w1  # noqa: E402

PLUGINS = {}


def plugin(key, number):
    def register(fn):
        PLUGINS[key] = (number, fn)
        return fn
    return register


# ---------------------------------------------------------------------------------------------
@plugin("fir", 42)
def fir():
    W, H = 760, 465
    green = "#2DCC70"

    # The graph in fir-resonance.png, measured: 0 dB at y 161, 1.46 px per dB, 50 Hz at x 122
    # and 56.9 px per octave.
    def x_of(hz):
        return 122 + 56.9 * math.log2(hz / 50.0)

    def y_of(db):
        return 161 + 1.46 * -db

    depth = 6.0
    nodes = [(100.0, 1.0), (300.0, 0.5), (3000.0, 2.0), (10000.0, 1.5), (20000.0, 1.0)]

    # Smooth in log frequency between the nodes (cosine), flat at 100 % outside them.
    def factor(hz):
        if hz <= nodes[0][0]:
            return nodes[0][1]
        for (h0, f0), (h1, f1) in zip(nodes, nodes[1:]):
            if hz <= h1:
                t = math.log(hz / h0) / math.log(h1 / h0)
                t = 0.5 - 0.5 * math.cos(math.pi * t)
                return f0 + (f1 - f0) * t
        return nodes[-1][1]

    points = []
    hz = 100.0
    while hz <= 20000.0:
        points.append(f"{x_of(hz):.1f},{y_of(-depth * factor(hz)):.1f}")
        hz *= 1.03

    zero = y_of(0.0)
    fill = f"M{x_of(100.0):.1f},{zero:.1f} L" + " L".join(points) + f" L{x_of(20000.0):.1f},{zero:.1f} Z"
    line = "M" + " L".join(points)

    dots = "".join(
        f'<circle cx="{x_of(h):.1f}" cy="{y_of(-depth * f):.1f}" r="{5 if i == 2 else 4}" fill="{"#fff" if i == 2 else green}" '
        f'stroke="{green}" stroke-width="2"/>'
        for i, (h, f) in enumerate(nodes[1:-1], start=1)
    )

    svg = (f'<svg class="o" style="left:0;top:0" width="{W}" height="{H}">'
           f'<path d="{fill}" fill="{green}" fill-opacity="0.10"/>'
           f'<path d="{line}" stroke="{green}" stroke-width="1.6" fill="none" stroke-dasharray="5 3"/>{dots}</svg>')

    tip_x, tip_y = x_of(3000.0) + 10, y_of(-depth * 2.0) + 8
    tip = (f'<div class="o menu" style="left:{tip_x:.0f}px;top:{tip_y:.0f}px;padding:4px 8px;font-size:11px">'
           f'3.00 kHz &middot; 200 % &middot; ceiling &minus;12.0 dB</div>')

    outline = (f'<div class="o new" style="left:{x_of(100.0) - 6:.0f}px;top:{zero - 6:.0f}px;'
               f'width:{x_of(20000.0) - x_of(100.0) + 12:.0f}px;height:{y_of(-13.0) - zero + 12:.0f}px"></div>')

    reset = '<div class="o new btn" style="left:74px;top:326px;width:88px;height:18px;font-size:11px">Reset curve</div>'

    return w1.page("Fancy FIR: Phase 4 Wave 2 mockup", "docs/fir.md, Phase 4 Wave 2: Resonance depth nodes. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("The depth ceiling and its nodes",
         "In Resonance mode the graph draws the most each frequency may be cut (Depth &times; the nodes &times; Tilt), hanging from the 0 dB line between Low and High, "
         "with the green reduction inside it. Nodes are a multiplier on Depth, 0 to 200 % (Decision 1 A), drawn where they land: here Depth 6 dB with 50 % at 300 Hz, "
         "200 % at 3 kHz (the selected node, with its read-out) and 150 % at 10 kHz. Double-click adds a node, drag moves it, right-click deletes it; "
         "the Edit menu's Points (smooth), Points (flat) and Freehand work as on the Gate and Compressor curves. Reset curve, in the Range card's top row, goes back to flat.",
         w1.shot("fir-resonance.png", W, H, svg + tip + outline + reset,
                 "No nodes is a flat 100 %, today's sound, to the bit. The nodes are saved with the project (not automatable, like the other curves); "
                 "an older FIR ignores them.")),
    ])


# ---------------------------------------------------------------------------------------------
def render(key):
    number, fn = PLUGINS[key]
    html = w1.SRC / f"w2-{key}.html"
    html.write_text(fn(), encoding="utf-8")
    png = w1.OUT / f"{number}__{key}_w2_mockup.png"
    w1.OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run([w1.EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--force-device-scale-factor=2",
                    "--window-size=1548,2400", f"--screenshot={png}", html.as_uri()], capture_output=True)
    from PIL import Image
    image = Image.open(png).convert("RGB")
    pixels = image.load()
    bottom = image.height - 1
    while bottom > 0 and all(pixels[x, bottom] == (18, 18, 18) for x in range(0, image.width, 7)):
        bottom -= 1
    image.crop((0, 0, image.width, min(image.height, bottom + 40))).save(png)
    print("wrote", png.relative_to(w1.ROOT))


if __name__ == "__main__":
    for key in sys.argv[1:] or PLUGINS:
        render(key)
