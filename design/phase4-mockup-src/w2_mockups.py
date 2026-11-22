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
# Items 5 to 9. Every screenshot is from CI run 44951967529 (Wave 1, 2x), saved in img/.
IMG = "../../design/phase4-mockup-src/img/"
BY = 73  # the body starts under the header and the title strip


def caption(x, y, text, width=None):
    w = f"width:{width}px;white-space:normal;line-height:1.4;" if width else ""
    return f'<div class="o cap" style="left:{x}px;top:{y}px;{w}">{text}</div>'


def group_title(x, y, text):
    return f'<div class="o cap" style="left:{x}px;top:{y}px;font-size:10px;letter-spacing:.06em">{text}</div>'


@plugin("sature", 45)
def sature():
    W, H = 760, 465
    orange = "#F39C12"
    group = (
        '<div class="o new" style="left:236px;top:246px;width:390px;height:118px"></div>'
        + group_title(242, 252, "OUTER BANDS")
        + caption(242, 268, "Below Low", None) + caption(392, 268, "Above High", None)
        + w1.tight(240, 284, "Drive", "+6.0 dB", orange, 0.17) + w1.tight(306, 284, "Mix", "100 %", orange, 1.0)
        + w1.tight(390, 284, "Drive", "0.0 dB", orange, 0.0) + w1.tight(456, 284, "Mix", "100 %", orange, 1.0)
        + caption(530, 290, "Type, Character, Tone and Tape HF loss are the mid band's.", 92)
    )
    band = ('<div class="o new" style="left:394px;top:292px;width:248px;height:118px;background:rgba(243,156,18,.07)"></div>'
            f'<div class="o cap" style="left:470px;top:298px;color:{orange}">low band +6.0 dB</div>')
    return w1.page("Fancy Sature: Phase 4 Wave 2 mockup", "docs/sature.md, Phase 4 Wave 2: outer-band drive, Decision 3 A. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · The EDIT drawer: an OUTER BANDS group",
         "Beside Input trim and DC filter: Drive (0 to 36 dB, 0 by default) and Mix (0 to 100 %, 100 % by default) for the band below Low and the band above High, as small knobs. "
         "They share the mid band's Type, Character, Tone and Tape HF loss (Decision 1 A).",
         w1.shot(IMG + "sature-edit-w1.png", W, H, group,
                 "Both Drives at 0 (the default) don't run the outer paths: today's sound to the bit. Auto gain covers the outer bands too. "
                 "With High at Off there's no band above it, so High's pair dims.")),
        ("2 · The main window: the BAND card shades while an outer band is driven",
         "There's no band graph on the main window, so the BAND card takes a light orange tint and a one-line caption naming the driven bands while either outer Drive is up.",
         w1.shot(IMG + "sature-w1.png", W, H, band, "Nothing moves; the tint and caption go when both Drives are back at 0.")),
    ])


@plugin("comp_optical", 46)
def comp_optical():
    W, H = 934, 547
    teal = "#00A985"
    opt = ('<div class="o new" style="left:236px;top:368px"><div class="cb" style="font-size:14px"><i class="x"></i>Optical</div></div>'
           + caption(258, 392, "Attack slows as it closes in, release has a fast and a slow stage, and a long squash recovers slowly. "
                               "Classic and Auto release don't apply while it's on.", 230))
    return w1.page("Fancy Comp: Phase 4 Wave 2 mockup (Optical)", "docs/comp.md, Phase 4 Wave 2: Optical mode, Decisions 1 and 2 A. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("The Options page: an Optical checkbox",
         "Under \"Attack = 90 % rise\", with the other ballistics options. Off by default. While it's on, the Classic and Auto release checkboxes on the Detector page dim. "
         "No memory knob (Decision 3 A).",
         w1.shot(IMG + "comp-options-w1.png", W, H, opt,
                 "Attack then means the time at about 12 dB over, which the tooltip says. Weird knee, Hold and Range still work.")),
    ])


@plugin("verb_decay", 47)
def verb_decay():
    W, H = 866, 697
    blue = "#3598DB"
    x0, y0 = 452, 273
    card = (
        f'<div class="o" style="left:{x0}px;top:{y0}px;width:202px;height:352px;background:#2c2c2c"></div>'
        f'<div class="o new" style="left:{x0}px;top:{y0}px;width:202px;height:352px"></div>'
        f'<div class="o" style="left:{x0 + 14}px;top:{y0 + 14}px;font-size:16px;font-weight:600;display:flex;gap:8px;align-items:center"><span style="color:{blue}">&#9211;</span>DECAY</div>'
        f'<div class="o" style="left:{x0 + 178}px;top:{y0 + 11}px;font-size:16px;color:#8D8D8D">&times;</div>'
        + group_title(x0 + 14, y0 + 50, "DECAY TIME")
        + w1.tight(x0 + 8, y0 + 64, "Low", "0.50 &times;", blue, 0.25, size=30)
        + w1.tight(x0 + 72, y0 + 64, "Mid", "1.00 &times;", blue, 0.5, size=30)
        + w1.tight(x0 + 136, y0 + 64, "High", "1.50 &times;", blue, 0.65, size=30)
        + caption(x0 + 14, y0 + 140, "2.1 &rarr; 1.1 s", None).replace('class="o cap"', 'class="o cap" data-x')
        + caption(x0 + 82, y0 + 140, "1.9 s", None) + caption(x0 + 138, y0 + 140, "1.7 &rarr; 2.4 s", None)
        + caption(x0 + 14, y0 + 158, "High can't grow past the IR's own tail: 2.4 of 2.6 s.", 176)
        + group_title(x0 + 14, y0 + 204, "CROSSOVERS")
        + w1.tight(x0 + 30, y0 + 218, "Low", "250 Hz", blue, 0.45, size=26)
        + w1.tight(x0 + 112, y0 + 218, "High", "4.0 kHz", blue, 0.5, size=26)
        + caption(x0 + 14, y0 + 300, "RT60 measured per band, before &rarr; after.", 176)
    )
    chip = ('<div class="cover" style="left:382px;top:650px;width:162px;height:22px;--bg:#262626"></div>'
            '<div class="o new btn" style="left:382px;top:650px;width:162px;height:22px;font-size:12px;justify-content:flex-start;padding-left:8px">'
            f'<span style="color:{blue}">&#9211;</span>&nbsp;DECAY</div>')
    return w1.page("Fancy Verb: Phase 4 Wave 2 mockup (Decay module)", "docs/verb.md, Phase 4 Wave 2: decay module, Decisions 1 to 3 A. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("A DECAY module card in the rack",
         "Picked from the Add panel like any module, here after FILE and LP/HP. Three decay multipliers (0.25 &times; to 4 &times;, 1 &times; by default) and two crossovers "
         "(Low 20 Hz to 1 kHz, 250 Hz; High 1 to 16 kHz, 4 kHz), with each band's measured RT60 before and after under its knob.",
         w1.shot(IMG + "verb-eq-w1.png", W, H, card + chip,
                 "Built offline with the IR, so it costs no CPU while playing. A band whose decay can't be measured shows \"can't measure\" under its knob and is left as it is. "
                 "All at 1 &times; gives back the same IR.")),
    ])


@plugin("verb_duck", 48)
def verb_duck():
    W, H = 866, 697
    purple, teal = "#9A59B5", "#00A985"
    x0, y0 = 340, 322
    group = (
        f'<div class="o new" style="left:{x0}px;top:{y0}px;width:376px;height:220px"></div>'
        + group_title(x0 + 8, y0 + 6, "AFTER THE REVERB")
        + '<div class="o lbl" style="left:{0}px;top:{1}px;line-height:24px;font-size:13px">Source</div>'.format(x0 + 8, y0 + 24)
        + f'<div class="o dd" style="left:{x0 + 64}px;top:{y0 + 24}px;width:110px;height:24px;font-size:13px">Main</div>'
        + caption(x0 + 184, y0 + 28, "Sidechain when routed (pins 3/4)")
        + w1.tight(x0 + 8, y0 + 62, "Ducking", "40 %", teal, 0.4, size=30)
        + caption(x0 + 76, y0 + 76, "Turns the reverb down while the source plays, up to 24 dB at 100 %.", 270)
        + f'<div class="o" style="left:{x0 + 8}px;top:{y0 + 140}px"><div class="cb" style="font-size:13px"><i class="x"></i>Gate</div></div>'
        + w1.tight(x0 + 84, y0 + 132, "Threshold", "&minus;30 dB", teal, 0.5, size=26)
        + w1.tight(x0 + 160, y0 + 132, "Hold", "80 ms", teal, 0.15, size=26)
        + w1.tight(x0 + 236, y0 + 132, "Decay", "40 ms", teal, 0.07, size=26)
    )
    return w1.page("Fancy Verb: Phase 4 Wave 2 mockup (Ducking and gate)", "docs/verb.md, Phase 4 Wave 2: ducking and level gate, Decisions 1 to 3 A. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("The SETUP drawer: an AFTER THE REVERB group",
         "In the drawer's free space beside the existing settings: the detector's Source (Main or Sidechain), Ducking (0 to 100 %, 0 by default) and the Gate "
         "(off by default; Threshold &minus;60 to 0 dB, Hold 10 to 500 ms, Decay 5 to 500 ms). Not modules: they act live on the wet signal after the convolver, whatever the rack's order.",
         w1.shot(IMG + "verb-edit-w1.png", W, H, group,
                 "Ducking 0 and Gate off (the defaults) leave today's sound to the bit. The detector waits for the reported latency, so in Low CPU mode it still lines up with the wet. "
                 "Gate's three knobs dim while Gate is off. With no sidechain routed, Sidechain acts as Main.")),
    ])


@plugin("limit", 49)
def limit():
    W, H = 760, 465
    red, orange = "#E74D3D", "#F39C12"
    clip = (
        '<div class="o new" style="left:396px;top:374px;width:228px;height:78px"></div>'
        f'<div class="o" style="left:402px;top:404px"><div class="cb" style="font-size:14px;--a:{red}"><i class="x"></i>Clip before limiting</div></div>'
        + w1.tight(560, 376, "Clip", "3.0 dB", red, 0.25, size=28)
    )
    peaks = "".join(f'<div class="o" style="left:{x}px;top:118px;width:4px;height:7px;background:{orange}"></div>'
                    for x in (119, 164, 209, 255, 301, 347, 393, 439))
    history = (peaks + '<div class="o new" style="left:120px;top:114px;width:360px;height:22px"></div>'
               '<div class="cover" style="left:124px;top:255px;width:350px;height:16px;--bg:#141414"></div>'
               f'<div class="o cap" style="left:127px;top:257px;font-size:10px">input (grey) &middot; output (white) &middot; reduction (red) &middot; <span style="color:{orange}">clipped (orange)</span> &middot; ceiling</div>')
    return w1.page("Fancy Limit: Phase 4 Wave 2 mockup", "docs/limit.md, Phase 4 Wave 2: clipper, Decisions 1 to 3 A. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · The EDIT drawer: Clip before limiting",
         "Beside the DC filter and Release curve rows: a checkbox (off by default) and one Clip knob, 0 to 12 dB (3 dB by default), how far the soft clipper's ceiling sits under Ceiling + 12 dB.",
         w1.shot(IMG + "limit-edit-w1.png", W, H, clip,
                 "Off, the clipper isn't run: a saved project renders as before to the bit. The Clip knob dims while it's off. 2 to 4 dB is the usual range, as the tooltip says.")),
        ("2 · The main window: clipped peaks in orange",
         "The history draws what the clipper takes in orange on top of the limiter's red reduction, and the legend names it.",
         w1.shot(IMG + "limit-w1.png", W, H, history, "With the clipper off the legend and history are as now.")),
    ])


@plugin("ds", 52)
def ds():
    W, H = 760, 465
    green, light, blue = "#2DCC70", "#F1C315", "#3598DB"

    # The frequency ruler in ds-w2.png, measured: 1 kHz at x 133, 20 kHz at x 636, log.
    def x_of(hz):
        return 133 + 503 * math.log(hz / 1000.0) / math.log(20.0)

    def tabs(selected):
        on = f"background:{green};color:#141414;font-weight:600"
        off = "color:#fff"
        power = (f'<svg width="11" height="11" viewBox="0 0 12 12" style="margin-left:5px"><path d="M4 3.2A3.8 3.8 0 1 0 8 3.2" '
                 f'stroke="{green if selected != "low" else "#141414"}" stroke-width="1.5" fill="none" stroke-linecap="round"/>'
                 f'<line x1="6" y1="1.2" x2="6" y2="5.6" stroke="{green if selected != "low" else "#141414"}" stroke-width="1.5" stroke-linecap="round"/></svg>')
        return (
            '<div class="o cap" style="left:252px;top:89px;font-size:12px">Band</div>'
            '<div class="o new" style="left:290px;top:84px;width:128px;height:24px;background:#141414;border-radius:4px;display:flex;padding:2px;gap:2px">'
            f'<div style="flex:1;border-radius:3px;display:flex;align-items:center;justify-content:center;font-size:12px;{on if selected == "high" else off}">High</div>'
            f'<div style="flex:1;border-radius:3px;display:flex;align-items:center;justify-content:center;font-size:12px;{on if selected == "low" else off}">Low{power}</div>'
            '</div>')

    low_lo, low_hi = x_of(3500 / math.sqrt(2)), x_of(3500 * math.sqrt(2))
    ruler = (
        f'<div class="o" style="left:{low_lo:.0f}px;top:252px;width:{low_hi - low_lo:.0f}px;height:8px;background:{light};border-radius:3px"></div>'
        f'<div class="o" style="left:{x_of(3500) - 3:.0f}px;top:247px;width:6px;height:16px;background:{blue};border-radius:3px;box-shadow:0 0 0 1.5px #141414"></div>'
        f'<div class="o new" style="left:{low_lo - 2:.0f}px;top:238px;width:{low_hi - low_lo + 4:.0f}px;height:25px"></div>'
    )

    # Low's reduction on the history: notches in the light shade between High's, at the "sh" sounds.
    notches = "".join(
        f'<svg class="o" style="left:{x}px;top:118px" width="{w}" height="{h}"><path d="M0,0 L{w},0 L{w - 3},{h * 0.7:.0f} L{w / 2 + 2:.0f},{h} L{w / 2 - 2:.0f},{h - 2} L3,{h * 0.7:.0f} Z" fill="{light}" fill-opacity=".85"/></svg>'
        for x, w, h in ((270, 16, 22), (392, 14, 16), (514, 16, 26)))
    history = (
        notches + '<div class="o new" style="left:262px;top:114px;width:276px;height:38px"></div>'
        f'<div class="o cap" style="left:121px;top:213px;font-size:10px;background:rgba(20,20,20,.92);padding:2px 6px;border-radius:3px">sibilance level (grey) &middot; reduction: '
        f'<span style="color:{green}">High</span>, <span style="color:{light}">Low</span> &middot; threshold</div>'
    )

    def values(threshold, freq, width, range_db, gr):
        covers = (
            '<div class="cover" style="left:14px;top:384px;width:58px;height:13px;--bg:#262626"></div>'
            '<div class="cover" style="left:80px;top:388px;width:32px;height:14px;--bg:#262626"></div>'
            '<div class="cover" style="left:140px;top:396px;width:68px;height:16px;--bg:#262626"></div>'
            '<div class="cover" style="left:256px;top:396px;width:56px;height:16px;--bg:#262626"></div>'
            '<div class="cover" style="left:368px;top:396px;width:56px;height:16px;--bg:#262626"></div>'
        )
        text = (
            f'<div class="o val" style="left:17px;top:383px;color:{green};font-size:12px">{threshold}</div>'
            f'<div class="o val" style="left:84px;top:388px;color:#fff;font-size:12px">{gr}</div>'
            f'<div class="o val" style="left:148px;top:397px;color:{blue};font-size:12px">{freq}</div>'
            f'<div class="o val" style="left:264px;top:397px;color:{green};font-size:12px">{width}</div>'
            f'<div class="o val" style="left:378px;top:397px;color:{green};font-size:12px">{range_db}</div>'
        )
        return covers + text

    dim_shape = ('<div class="o" style="left:226px;top:292px;width:100px;height:21px;background:#262626;opacity:.6;border-radius:4px"></div>'
                 '<div class="o new" style="left:226px;top:292px;width:100px;height:21px"></div>')

    high = tabs("high") + ruler + history
    low = tabs("low") + ruler + history + values("-20.0 dB", "3.50 kHz", "1.0 oct", "-9 dB", "-3.1") + dim_shape

    return w1.page("Fancy DS: Phase 4 Wave 2 mockup (dual-band detection)", "docs/ds.md, Phase 4 Wave 2: dual-band detection, Decisions 1 to 4 A. New things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        ("1 · High selected, Low switched on",
         "Band tabs High | Low in the top row, between Detector and Mode. The power switch on the Low tab turns Dual band on; off, Low's tab shows only its name and the plugin works as now. "
         "The frequency ruler shows Low's band in yellow with its own blue handle, and the history shows Low's reduction in yellow beside High's green (user, 22 November 2026).",
         w1.shot(IMG + "ds-w2.png", W, H, high,
                 "<b>High</b> is today's band, unchanged: the Threshold fader and meter, GR, Freq, Width, Band | Shelf and Range all show it. "
                 "Both bars on the ruler drag: the handle sets that band's Freq, the edges its Width. Each drag is one undo step.")),
        ("2 · Low selected",
         "The same controls now show Low: Threshold (-20 dB), GR, Freq (3.50 kHz, 1 to 8 kHz), Width and Range (-9 dB). Band | Shelf dims: Low is always a band. "
         "Attack, Release, Detector, Mode, Relative threshold, Link channels and Learn are shared; Learn sets both thresholds.",
         w1.shot(IMG + "ds-w2.png", W, H, low,
                 "The selected tab is the window's state, not a parameter, like EQ's selected band. Clicking either bar on the ruler also selects its tab.")),
    ])


STEMS = {"comp_optical": "comp_w2_optical", "verb_decay": "verb_w2_decay", "verb_duck": "verb_w2_duck"}


# ---------------------------------------------------------------------------------------------
def render(key):
    number, fn = PLUGINS[key]
    html = w1.SRC / f"w2-{key.replace('_', '-')}.html"
    html.write_text(fn(), encoding="utf-8")
    png = w1.OUT / f"{number}__{STEMS.get(key, key + '_w2')}_mockup.png"
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
