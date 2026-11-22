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
@plugin("comp", 43)
def comp():
    W, H = 934, 547
    teal = "#00A985"
    # The Options page from CI run 44930852915 (Wave 1), saved beside this script.
    image = "../../design/phase4-mockup-src/img/comp-options-w1.png"
    detector = ('<div class="cover" style="left:129px;top:86px;width:131px;height:24px;--bg:#262626"></div>'
                '<div class="o new dd" style="left:129px;top:86px;width:131px;height:24px">Host sync</div>')
    group = (
        '<div class="o new" style="left:236px;top:366px;width:250px;height:110px"></div>'
        '<div class="o cap" style="left:240px;top:370px;font-size:10px;letter-spacing:.06em">HOST SYNC</div>'
        '<div class="o lbl" style="left:240px;top:390px;line-height:24px">Rate</div>'
        '<div class="o dd" style="left:284px;top:390px;width:100px;height:24px">1/4</div>'
        + w1.tight(410, 372, "Shape", "25 %", teal, 0.2) +
        '<div class="o cap" style="left:240px;top:422px;width:160px;white-space:normal;line-height:1.35">'
        'A hit on every step of the host&rsquo;s grid. Release shapes the duck most.</div>'
    )
    return w1.page("Fancy Comp: Phase 4 Wave 2 mockup", "docs/comp.md, Phase 4 Wave 2: tempo-synced trigger, Decision 3 A. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("Host sync on the Options page",
         "Host sync is appended to the Detector input menu (here chosen). While it's chosen, the Options page shows a HOST SYNC group in the free space under the checkboxes: "
         "Rate (1/1 to 1/16, each also dotted and triplet; 1/4 by default) and Shape (5 to 100 % of the step, 25 % by default), a small knob. "
         "The detector then hears a virtual kick on the host's grid, so Threshold, Ratio, Attack, Release, Hold and Range shape the duck as they would with a real kick.",
         w1.shot(image, W, H, detector + group,
                 "With another Detector input the group is hidden and the page is as now. Without host tempo the caption reads \"No host tempo: 120 BPM\". "
                 "The History page's beat lines line up with the hits.")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("gate", 44)
def gate():
    W, H = 740, 441
    yellow = "#F1C315"
    # The main window from CI run 44951967529 (Wave 1), saved beside this script. The body starts
    # 73 px down (header and title strip); the drawer is drawn from GateEditor's as-built bounds.
    image = "../../design/phase4-mockup-src/img/gate-w1.png"
    by = 73
    link = (f'<div class="cover" style="left:380px;top:{by + 9}px;width:62px;height:24px;--bg:#303030"></div>'
            f'<div class="o" style="left:388px;top:{by + 12}px;font-size:14px;color:{yellow}">&#9881; EDIT</div>')
    detector = (f'<div class="cover" style="left:90px;top:{by + 9}px;width:106px;height:24px;--bg:#303030"></div>'
                f'<div class="o new dd" style="left:90px;top:{by + 9}px;width:106px;height:24px;font-size:11px;background:transparent">Host sync</div>')
    menu = (f'<div class="o new menu" style="left:90px;top:{by + 36}px;width:150px">'
            '<div class="cap" style="padding:3px 12px">MAIN INPUT</div><div>Main input L+R</div><div>Main L</div><div>Main R</div>'
            '<div class="cap" style="padding:3px 12px">SIDECHAIN</div><div>Sidechain L+R</div><div>Sidechain L</div><div>Sidechain R</div>'
            '<div class="cap" style="padding:3px 12px">MID/SIDE</div><div>Mid</div><div>Side</div>'
            '<div class="cap" style="padding:3px 12px">HOST SYNC</div><div class="on">&#10003; Host sync</div></div>')

    dx, dy = 120, by + 114
    drawer = (
        f'<div class="o" style="left:{dx}px;top:{dy}px;width:456px;height:204px;background:#262626">'
        '<div class="o" style="left:18px;top:14px;font-size:14px;font-weight:600;letter-spacing:.06em">EDIT</div>'
        '<div class="o btn" style="left:380px;top:10px;width:62px;height:24px;border-radius:12px">Done</div>'
        f'<div class="o" style="left:18px;top:50px"><div class="cb" style="--a:{yellow};font-size:12px"><i></i>MIDI trigger</div></div>'
        '<div class="o cap" style="left:40px;top:72px;width:190px;white-space:normal;line-height:1.4">Note-on opens, note-off closes. The detector is ignored.</div>'
        f'<div class="o" style="left:18px;top:118px;display:flex;align-items:center;gap:8px"><span class="btn" style="width:84px;height:26px;background:{yellow};color:#141414;font-weight:600">Learn</span>'
        '<span class="cap" style="width:130px;white-space:normal;line-height:1.4">Listens for 3 s and sets Threshold</span></div>'
        f'<div class="o" style="left:246px;top:40px;width:90px" data-k data-v="1" data-c="{yellow}" data-s="40" data-l="Ratio" data-t="inf:1"></div>'
        '<div class="o" style="left:346px;top:40px;width:90px" data-k data-v="0" data-c="#3598DB" data-s="40" data-l="De-bleed" data-t="0 %"></div>'
        '</div>'
    )
    group = (
        f'<div class="o new" style="left:{dx + 246}px;top:{dy + 140}px;width:196px;height:62px"></div>'
        f'<div class="o cap" style="left:{dx + 250}px;top:{dy + 146}px;font-size:10px;letter-spacing:.06em">HOST SYNC</div>'
        f'<div class="o lbl" style="left:{dx + 250}px;top:{dy + 166}px;line-height:24px;font-size:12px">Rate</div>'
        f'<div class="o dd" style="left:{dx + 282}px;top:{dy + 166}px;width:88px;height:24px;font-size:12px">1/8</div>'
        + w1.tight(dx + 380, dy + 138, "Shape", "50 %", yellow, 0.47, size=24)
    )
    return w1.page("Fancy Gate: Phase 4 Wave 2 mockup", "docs/gate.md, Phase 4 Wave 2: tempo-synced trigger, Decision 3 A. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · Host sync in the Detector input menu",
         "Host sync is appended after Side, in its own HOST SYNC section. The EDIT link is drawn at its fixed width: today's build clips it to \"EDI\", fixed in the same push.",
         w1.shot(image, W, H, link + detector + menu,
                 "Nothing else on the main window moves. The Lowpass and Highpass cards stay live but don't touch the pulse; "
                 "the \"No sidechain signal\" notice never shows for Host sync.")),
        ("2 · The EDIT drawer with Host sync chosen",
         "Under Ratio and De-bleed, in place of the De-bleed note (De-bleed works on audio and never touches the pulse): a HOST SYNC group with Rate "
         "(1/1 to 1/16, each also dotted and triplet; 1/8 by default) and Shape (5 to 100 % of the step, 50 % by default), a small knob. "
         "Each step opens the gate for Shape of the step whatever Threshold is; Attack, Hold, Release, Range and Ratio shape each opening.",
         w1.shot(image, W, H, link + drawer + group,
                 "With another Detector input the group is hidden and the De-bleed note is back, as now. "
                 "Without host tempo the caption reads \"NO HOST TEMPO: 120 BPM\". "
                 "With MIDI trigger ticked, MIDI wins and the group dims (Decision 2 A).")),
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
