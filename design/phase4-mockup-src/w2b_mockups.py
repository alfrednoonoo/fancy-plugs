"""Phase 4 Wave 2 mockups for v1.7.1 (items 11 to 45 and Kicker's Duck), drawn as w2_mockups.py's:
the plugin's real screenshot at 1x with the new parts over it, outlined in dashed yellow, and notes beside it.

    py -3 design/phase4-mockup-src/w2b_mockups.py [key ...]

Screenshots are from CI run 45087209567 (commit 9390927, v1.7.0 plus TS and Chorus clips), 2x, saved in
img/ as <name>-w2b.png. Writes design/phase4-mockup-src/w2b-<key>.html and reference/phase4/<file>.
"""
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import w1_mockups as w1  # noqa: E402
from w2_mockups import caption, group_title  # noqa: E402

IMG = "../../design/phase4-mockup-src/img/"
PLUGINS = {}


def plugin(key, filename):
    def register(fn):
        PLUGINS[key] = (filename, fn)
        return fn
    return register


def grow(name, out, d, bands, axis="x"):
    """Makes a larger copy of img/<name>-w2b.png as img/<out>-w2b.png: the window grown by d px (1x).
    axis "x": bands are (y0, y1, cuts); in each band the columns at the cuts are repeated, d px in all,
    shared equally. axis "y": bands are (x0, x1, cuts) and rows are repeated. A cut can be one number.
    Bands must cover the whole height (x) or width (y), in 1x units."""
    from PIL import Image
    folder = pathlib.Path(__file__).resolve().parent / "img"
    src = Image.open(folder / f"{name}-w2b.png").convert("RGB")
    if axis == "y":
        src = src.transpose(Image.Transpose.TRANSPOSE)
    w, h = src.size
    dst = Image.new("RGB", (w + 2 * d, h))
    for a0, a1, cuts in bands:
        cuts = sorted([cuts] if isinstance(cuts, (int, float)) else cuts)
        a0, a1 = 2 * a0, 2 * a1
        shares = [2 * d // len(cuts) + (1 if i < 2 * d % len(cuts) else 0) for i in range(len(cuts))]
        x, at = 0, 0
        for c, share in zip(cuts, shares):
            c = int(2 * c)
            dst.paste(src.crop((x, a0, c, a1)), (at, a0))
            at += c - x
            dst.paste(src.crop((c - 1, a0, c, a1)).resize((share, a1 - a0)), (at, a0))
            at += share
            x = c
        dst.paste(src.crop((x, a0, w, a1)), (at, a0))
    if axis == "y":
        dst = dst.transpose(Image.Transpose.TRANSPOSE)
    dst.save(folder / f"{out}-w2b.png")
    return dst.size[0] // 2, dst.size[1] // 2


def shot(name, w, h, overlays, side="", crop=None):
    return w1.shot(IMG + name + "-w2b.png", w, h, overlays, side, crop)


def cover(x, y, w, h, bg="#262626"):
    return f'<div class="cover" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px;--bg:{bg}"></div>'


def outline(x, y, w, h):
    return f'<div class="o new" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px"></div>'


def seg(x, y, w, h, choices, selected, accent, fs=12, new=True, dark="#141414", text="#141414"):
    """A segmented switch like DS's Band | Shelf. choices are HTML strings; selected is an index."""
    cells = "".join(
        f'<div style="flex:1 1 auto;padding:0 5px;border-radius:3px;display:flex;align-items:center;justify-content:center;font-size:{fs}px;'
        + (f'background:{accent};color:{text};font-weight:600' if i == selected else 'color:#fff') + f'">{c}</div>'
        for i, c in enumerate(choices))
    return (f'<div class="o{" new" if new else ""}" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px;background:{dark};'
            f'border-radius:4px;display:flex;padding:2px;gap:2px">{cells}</div>')


def knob(cx, top, label, value, colour, v, size=42, fs=13, bi=False, width=None):
    """A knob whose face is centred on cx; top is the label's top."""
    w = width or size + 40
    return (f'<div class="o" style="left:{cx - w / 2:.0f}px;top:{top}px;width:{w}px" data-k data-v="{v}" data-c="{colour}" '
            f'data-s="{size}" data-fs="{fs}" data-l="{label}" data-t="{value}"{" data-bi" if bi else ""}></div>')


def dim(x, y, w, h, bg="38,38,38", a=0.62):
    return f'<div class="o" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px;background:rgba({bg},{a})"></div>'


def row(x, y, w, label, control_html, label_w=None):
    return (f'<div class="o" style="left:{x}px;top:{y}px;width:{w}px;display:flex;justify-content:space-between;align-items:center">'
            f'<span class="lbl" style="color:#8D8D8D;font-size:13px">{label}</span>{control_html}</div>')


def dd(text, w, h=24, fs=13, colour="#fff"):
    return f'<span class="dd" style="width:{w}px;height:{h}px;font-size:{fs}px;color:{colour}">{text}</span>'


def lead(note, decisions):
    return f"{note}, {decisions}. New things in dashed yellow, on the plugin's real screenshot (1x coordinates)."


# ---------------------------------------------------------------------------------------------
GATE_Y = "#F1C315"


GATE_BANDS = [(0, 73, 600), (73, 113, 384), (113, 188, 384), (188, 292, [211, 302, 393, 484]), (292, 395, [233, 465]), (395, 441, 424)]
GATE_DRAWER_BANDS = [(0, 73, 600), (73, 113, 384), (113, 188, 384), (188, 292, 384), (292, 395, 384), (395, 441, 424)]
GATE_GROW = 50  # Upward's third Mode choice: the window grows from 740 to 790 px


def gate_mode_row(selected):
    """Mode grows to Gate | Duck | Upward at full size, in the 50 px the window gains (cut at x 384)."""
    d = GATE_GROW
    return (cover(384, 80, 196 + d, 28, "#303030")
            + f'<div class="o" style="left:390px;top:83px;font-size:14px;color:{GATE_Y};font-weight:500">&#9881; EDIT</div>'
            + '<div class="o cap" style="left:450px;top:85px;font-size:12px">Mode</div>'
            + seg(492, 82, 84 + d, 24, ["Gate", "Duck", "Upward"], selected, GATE_Y))


GATE_TALL = 24  # the Range card's title row for the Level | Transient switch: 441 to 465 px


def gate_images():
    """Gate at 790 x 465: 50 px wider for Upward (items 11 and 12 ship together), 24 px taller for the Detector switch."""
    grow("gate", "gate-wide", GATE_GROW, GATE_BANDS)
    grow("gate-wide", "gate-wt", GATE_TALL, [(0, 640, 299), (640, 790, 380)], axis="y")
    grow("gate-drawer", "gate-drawer-wide", GATE_GROW, GATE_DRAWER_BANDS)
    return grow("gate-drawer-wide", "gate-drawer-wt", GATE_TALL, [(0, 640, 303), (640, 790, 380)], axis="y")


def gate_detector_switch(mode, new=True):
    """The Range card's new title row: "Detector" and a Level | Transient switch over the RMS size column."""
    return ('<div class="o cap" style="left:206px;top:302px;font-size:11px">Detector</div>'
            + seg(262, 299, 106, 18, ["Level", "Transient"], 0 if mode == "level" else 1, GATE_Y, fs=11, new=new))


@plugin("gate_transient", "53__gate_w2_transient_mockup.png")
def gate_transient():
    W, H = gate_images()
    hyst = dim(488 + GATE_GROW, 193, 88, 96)
    rise = cover(262, 322, 106, 94) + knob(315, 325, "Rise", "8.0 dB", GATE_Y, 0.24, size=46, fs=14) + outline(264, 324, 102, 90)
    size_note = ("<b>The window is 790 &times; 465</b>, 50 px wider for Upward's third Mode choice (mockup 54, same release) "
                 "and 24 px taller for the Range card's title row; every control keeps its size.")
    return w1.page("Fancy Gate: Phase 4 Wave 2 mockup (Transient detector)",
                   lead("docs/gate.md, Phase 4 Wave 2: Transient detector mode", "Decisions 1 to 3 A"), [
        ("1 · Level chosen: today's detector",
         "The Range card gets a title row with a <b>Detector: Level | Transient</b> switch over RMS size. Level is today's detector: peak at RMS size 0, RMS above it. " + size_note,
         shot("gate-wt", W, H, gate_detector_switch("level"),
              "Level is the default: saved projects render as before, to the bit. The Lowpass card grows with the Range card and keeps its knobs where they were.")),
        ("2 · Transient chosen: RMS size becomes Rise",
         "The knob under the switch now sets <b>Rise</b> (3 to 24 dB, 8 dB by default): how far the level must jump above its own 50 ms average to count as a hit. "
         "Threshold becomes a floor. Hysteresis dims, because Transient closes Hold after the last rise whatever the level (Decision 2 A).",
         shot("gate-wt", W, H, gate_detector_switch("transient") + rise + hyst,
              "The tooltip on Transient says it's for slow-onset bleed (bass, pads, held notes), not cymbals; the Lowpass card is the tool for those. "
              "With MIDI trigger or Host sync the switch dims, since they bypass the detector.")),
    ])


@plugin("gate_upward", "54__gate_w2_upward_mockup.png")
def gate_upward():
    W, H = gate_images()
    t = GATE_TALL
    boost = (cover(124, 297 + t, 108, 92) + knob(176, 300 + t, "Boost", "+4.0 dB", GATE_Y, 0.33, size=46, fs=13) + outline(126, 299 + t, 104, 90)
             + gate_detector_switch("level", new=False))
    hyst = dim(488 + GATE_GROW, 193, 88, 96)
    main = gate_mode_row(2) + boost + hyst
    # The drawer, with Main input as the detector: Host sync's group is replaced by the De-bleed note, as built.
    drawer = (
        cover(98, 84, 74, 24, "#303030") + '<div class="o lbl" style="left:98px;top:87px;font-size:11px">Main input L+R</div>'
        + cover(362, 322 + GATE_TALL, 214 + GATE_GROW, 68)
        + '<div class="o cap" style="left:420px;top:358px;width:196px;white-space:normal;line-height:1.4">'
          "De-bleed needs Lowpass and Highpass set: out-of-band thumps can't open the gate.</div>"
        + cover(364 + GATE_GROW, 228, 96, 96) + knob(411 + GATE_GROW, 228, "Up ratio", "1.5:1", GATE_Y, 0.14, size=46, fs=14)
        + outline(366 + GATE_GROW, 228, 92, 94)
        + row(138, 378, 210, "Ceiling", dd("&minus;1.0 dBTP", 120, 24, 12)) + outline(136, 376, 214, 28)
    )
    return w1.page("Fancy Gate: Phase 4 Wave 2 mockup (Upward mode)",
                   lead("docs/gate.md, Phase 4 Wave 2: Upward mode", "Decisions 1 to 3 A"), [
        ("1 · The main window in Upward",
         "Mode gains a third choice, <b>Upward</b>, at the switch's full size. To fit it, <b>the window grows 50 px wider, to 790 &times; 465</b> with item 11's title row (its minimum too): "
         "the graph, the knob cards and the footer widen, and every control keeps its size. "
         "While Upward is chosen, Range's knob becomes <b>Boost</b> (0 to 12 dB, 4 dB by default), the most it lifts, and Hysteresis dims.",
         shot("gate-wt", W, H, main,
              "The graph's yellow line is the gain as now, rising above the top while it boosts. The lights read \"boost\" for \"open\". "
              "In Gate and Duck the knob is Range again and everything is as today, to the bit.")),
        ("2 · The EDIT drawer in Upward",
         "Ratio's knob becomes <b>Up ratio</b> (1.1:1 to 4:1, 1.5:1 by default) while Upward is chosen, as Range becomes Boost, so Gate's Ratio keeps its own value. "
         "<b>Ceiling</b> (Off, or &minus;12 to 0 dBTP; Off by default) goes under Learn in every mode, since it can catch any peak, but Upward is what needs it.",
         shot("gate-drawer-wt", W, H, gate_mode_row(2) + drawer,
              "Ceiling Off isn't in the path and adds no latency. On, it reports its latency and the dry path follows. "
              "Here the Detector input is Main input L+R, so the De-bleed note is back where Host sync's group was.")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("comp_sceq", "55__comp_w2_sceq_mockup.png")
def comp_sceq():
    W, H = 934, 547
    blue, teal = "#3598DB", "#00A985"

    def tab(i, on, off_band):
        style = (f"background:{blue};color:#141414;font-weight:700" if on else
                 ("color:#5a5a5a" if off_band else "color:#ddd"))
        return f'<div style="width:20px;height:16px;border-radius:3px;display:flex;align-items:center;justify-content:center;font-size:10.5px;{style}">{i}</div>'

    tabs = ('<div class="o new" style="left:242px;top:180px;display:flex;gap:2px;background:#262626;border-radius:4px;padding:2px">'
            + "".join(tab(i, i == 2, i > 4) for i in range(1, 7)) + '</div>')
    # Band 4, a bell switched on at 400 Hz, -4 dB: a handle on the curve's left half, and its dip drawn.
    band4 = ('<svg class="o" style="left:236px;top:175px" width="476" height="104">'
             '<path d="M150,51 C175,51 185,63 197,63 C209,63 219,51 244,51" stroke="#3598DB" stroke-width="1.6" fill="none" stroke-dasharray="4 3"/>'
             '<circle cx="197" cy="63" r="5" fill="#141414" stroke="#3598DB" stroke-width="1.6"/>'
             '<text x="203" y="78" fill="#3598DB" font-size="9" font-family="Inter">4</text></svg>'
             + outline(421, 226, 24, 24))
    slope = (cover(348, 262, 60, 14, "#141414")
             + '<div class="o new cap" style="left:351px;top:262px;font-size:9px;color:#3598DB">HP 93 Hz &middot; 24 dB/oct</div>')
    editor = (
        cover(236, 284, 476, 118)
        + group_title(242, 288, "BAND 2")
        + '<div class="o lbl" style="left:242px;top:304px;font-size:12px">Type</div>'
        + f'<div class="o dd" style="left:242px;top:324px;width:108px;height:24px;font-size:12px">Bell</div>'
        + knob(392, 300, "Freq", "3.0 kHz", blue, 0.62, size=44, fs=12)
        + knob(452, 300, "Gain", "+12.0 dB", blue, 0.75, size=44, fs=12, bi=True)
        + knob(512, 300, "Q", "1.00", blue, 0.3, size=44, fs=12)
        + group_title(572, 288, "CUT SLOPES")
        + '<div class="o lbl" style="left:572px;top:310px;font-size:12px;color:#8D8D8D">Highpass</div>'
        + f'<div class="o dd" style="left:634px;top:305px;width:78px;height:22px;font-size:11.5px">24 dB</div>'
        + '<div class="o lbl" style="left:572px;top:344px;font-size:12px;color:#8D8D8D">Lowpass</div>'
        + f'<div class="o dd" style="left:634px;top:339px;width:78px;height:22px;font-size:11.5px">12 dB</div>'
        + outline(238, 286, 472, 112)
    )
    menu = ('<div class="o new menu" style="left:242px;top:350px;width:108px;font-size:12px">'
            '<div>Off</div><div>Low shelf</div><div class="on">&#10003; Bell</div><div>High shelf</div></div>')

    link = (row(520, 416, 190, "Link mode", dd("Mid/Side", 104, 24, 12)) + outline(518, 414, 194, 28)
)
    gr = (cover(116, 452, 92, 20, "#303030")
          + '<div class="o new val" style="left:118px;top:455px;font-size:10.5px;color:#fff">M &minus;2.8&nbsp;&nbsp;S &minus;2.6</div>')

    return w1.page("Fancy Comp: Phase 4 Wave 2 mockup (six-band detector EQ and link modes)",
                   lead("docs/comp.md, Phase 4 Wave 2: six-band sidechain EQ (item 13) and link modes (item 14)", "Decisions all A"), [
        ("1 · The Detector page: six bands, one edited at a time",
         "Band tabs <b>1 to 6</b> sit in the curve's top-left corner (bands that are Off are dimmed: 5 and 6 here). Each band that is on has a handle on the curve; here band 4 is "
         "a bell at 400 Hz, &minus;4 dB, added to today's three. Under the curve, the selected band's <b>Type</b> (Off, Low shelf, Bell, High shelf), Freq, Gain and Q "
         "(Q only for Bell), and the <b>cut slopes</b> for the main window's Highpass and Lowpass knobs: 12, 24 or 48 dB/oct.",
         shot("comp-detector", W, H, tabs + band4 + slope + editor + menu,
              "Bands 1 to 3 are today's low shelf, bell and high shelf, with their values; 4 to 6 start Off. A band that is Off or at 0 dB isn't run, "
              "so saved projects render as before, to the bit. Clicking a handle selects its tab; dragging moves Freq and Gain, the wheel sets Q. "
              "Auto threshold stays where it is, below.")),
        ("2 · The Options page: Link mode, and the GR meter labelled M and S",
         "A <b>Link mode</b> dropdown (Stereo, Mid/Side, M&gt;S, S&gt;M; Stereo by default) under Hold and Range, the free space nearest Link amount, which now links "
         "mid and side in the M/S modes. While the mode isn't Stereo, the GR meter's read-outs are labelled M and S.",
         shot("comp-options", W, H, link + gr,
              "Stereo is today's sound, to the bit. M&gt;S compresses only the side from the mid's level, so the width narrows while the vocal is loud; "
              "S&gt;M the other way round. Each mode's tooltip says this in one line.")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("verb_attack_width", "64__verb_w2_attack_width_mockup.png")
def verb_attack_width():
    W, H = 866, 697
    teal = "#00A985"
    attack = knob(309, 432, "Attack", "150 ms", teal, 0.3, size=46, fs=12) + outline(280, 432, 58, 94)
    extra = (knob(60, 528, "Extra width", "150 %", teal, 0.5, size=40, fs=12) + outline(22, 526, 300, 84)
             + caption(112, 548, "Multiplies Width (the header's W knob). Mono stays the same; each side rises up to 3 dB. "
                                 "Check in mono; 120 to 150 % is the useful range.", 200))
    return w1.page("Fancy Verb: Phase 4 Wave 2 mockup (Attack and Extra width)",
                   lead("docs/verb.md, Phase 4 Wave 2: Attack (item 17) and Width to 200 % (item 18)", "Decisions all A"), [
        ("1 · The Reverb generator card: Attack",
         "<b>Attack</b> (0 to 500 ms, 0 by default) takes the free tenth place in the card's knob grid, after Build-up, so the card keeps its size. "
         "It fades the whole generated tail in over Attack ms (raised cosine), early reflections included: a bloom instead of a start at full level.",
         shot("verb-generators", W, H, attack,
              "At 0 the IR is today's, to the bit. Changing it rebuilds the IR with Verb's crossfade, as the card's other knobs do. "
              "A long Attack costs level (&minus;3 dB at 150 ms with Decay 1.8 s); Equal loudness in SETUP evens it out, which the tooltip says.")),
        ("2 · The SETUP drawer: Extra width",
         "<b>Extra width</b> (100 to 200 %, 100 % by default) under Equal loudness. It multiplies the header's Width, which keeps its range and meaning.",
         shot("verb-edit", W, H, extra,
              "At 100 % the output is today's, to the bit. It raises only the wet signal's side, so the mono sum doesn't change (Decision 2 A).")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("room", "56__room_w2_mockup.png")
def room():
    W, H = grow("room-edit", "room-edit-tall", 64, [(0, 650, 418), (650, 760, 380)], axis="y")
    purple = "#9A59B5"
    duck = (group_title(262, 222, "DUCK FROM")
            + f'<div class="o dd" style="left:262px;top:238px;width:120px;height:24px;font-size:12px">Sidechain</div>'
            + '<div class="o" style="left:262px;top:274px"><div class="cb" style="font-size:12px;--a:' + purple + '"><i></i>Listen to ducking</div></div>'
            + outline(258, 218, 130, 76))
    shape = (group_title(398, 424, "DECAY SHAPE")
             + knob(448, 438, "Low decay", "&times;2.00", purple, 0.75, size=30, fs=12, bi=True, width=80)
             + knob(538, 438, "Attack", "150 ms", purple, 0.3, size=30, fs=12, width=80)
             + outline(394, 420, 236, 100))
    menu = ('<div class="o new menu" style="left:262px;top:266px;width:120px;font-size:12px">'
            '<div>Input</div><div class="on">&#10003; Sidechain</div></div>')
    return w1.page("Fancy Room: Phase 4 Wave 2 mockup",
                   lead("docs/room.md, Phase 4 Wave 2: Low decay, Attack and a sidechain for Ducking", "Decisions 1 to 4 A"), [
        ("1 · The EDIT drawer",
         "<b>Duck from</b> (Input or Sidechain; Input by default) and a temporary <b>Listen to ducking</b> switch go in the free space right of Early reflections, beside Ducking. "
         "A <b>DECAY SHAPE</b> group under Auto gain holds <b>Low decay</b> (&times;0.25 to &times;4, &times;1 by default, centred) and <b>Attack</b> (0 to 500 ms, 0 by default) as small knobs. To give it room, <b>the window grows 64 px taller, from 465 to 529 px</b> (its minimum too); the meters and faders lengthen and the main page's cards get the height evenly.",
         shot("room-edit-tall", W, H, duck + shape,
              "Low decay &times;1, Attack 0 and Duck from Input are today's sound, to the bit. Listen to ducking plays what Ducking takes away and is never saved (&sect;0.3); "
              "it dims while Ducking is 0. With Sidechain chosen and nothing routed, the main window shows the \"No sidechain signal\" notice, as Gate and DS do.")),
        ("2 · Duck from, open",
         "Two choices. Sidechain reads Room's optional stereo sidechain bus (pins 3/4 in REAPER), off until routed.",
         shot("room-edit", W, H, duck + menu, "", crop=(0, 80, 400, 260))),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("delay", "57__delay_w2_mockup.png")
def delay():
    W, H = grow("delay-motion", "delay-motion-wide", 60,
                [(0, 73, 646), (73, 236, 400), (236, 270, 545), (270, 418, 525), (418, 448, 582)])
    y = "#F1C315"
    sine = ('<svg width="18" height="10" viewBox="0 0 18 10"><path d="M1,5 C3.5,-1.5 5.5,-1.5 9,5 C12.5,11.5 14.5,11.5 17,5" '
            'stroke="currentColor" stroke-width="1.5" fill="none"/></svg>')
    tri = ('<svg width="18" height="10" viewBox="0 0 18 10"><path d="M1,5 L4.6,1 Q5,0.6 5.4,1 L12.6,9 Q13,9.4 13.4,9 L17,5" '
           'stroke="currentColor" stroke-width="1.5" fill="none" stroke-linejoin="round"/></svg>')
    switch = seg(364, 384, 50, 17, [sine, tri], 1, y, fs=10)
    stereo = knob(555, 286, "Stereo", "90&deg;", y, 0.5, size=46, fs=13, width=60) + outline(525, 280, 60, 98)
    tip = ('<div class="o menu" style="left:426px;top:383px;padding:3px 8px;font-size:11px">Triangle: a steady detune that flips each half cycle</div>')
    return w1.page("Fancy Delay: Phase 4 Wave 2 mockup (LFO shape and stereo phase)",
                   lead("docs/delay.md, Phase 4 Wave 2: LFO shape and stereo phase (item 22)", "Decisions 1 to 3 A, with wave symbols (user, 22 November 2026)"), [
        ("The Motion page, tap 1",
         "A small two-way switch drawn as a <b>sine wave</b> and a <b>triangle wave</b> sits under Rate (here Triangle, with its tooltip). "
         "A <b>Stereo</b> knob (0 to 180&deg;, 90&deg; by default) comes after Random: how far ahead the right channel's LFO runs. "
         "To fit it at full size, <b>the window grows 60 px wider, from 760 to 820 px</b> (its minimum too): the tap graph and the Motion card widen, and no knob shrinks.",
         shot("delay-motion-wide", W, H, stereo + switch + outline(362, 382, 54, 21) + tip,
              "Both are per tap, like Rate and Depth. Sine at 90&deg; is today's sound, to the bit. 0&deg; moves both sides together; 180&deg; moves them in opposition, the widest. "
              "The triangle's corners are rounded over 10 % of the cycle, so its flips don't jump.")),
    ])


# ---------------------------------------------------------------------------------------------
MC = ["#E74D3D", "#9A59B5", "#24AEC6", "#0CC08A"]


@plugin("multicomp", "58__multicomp_w2_mockup.png")
def multicomp():
    import math
    W, H = 760, 609
    purple = MC[1]
    # 1 · main window: the band row's Look knob, the view switch, a slope menu on crossover 2.
    look = knob(574, 327, "Look", "2.0 ms", purple, 0.2, size=46, fs=13) + outline(544, 318, 60, 92)
    view = (seg(50, 112, 150, 18, ["Spectrum", "GR history"], 0, "#8D8D8D", fs=10.5, dark="#1E1E1E"))
    slope_menu = ('<div class="o new menu" style="left:390px;top:200px;width:150px;font-size:12px">'
                  '<div class="cap" style="padding:3px 12px">CROSSOVER 2 &middot; 1.20 kHz</div>'
                  '<div>LR 24 dB/oct</div><div class="on">&#10003; LR 48 dB/oct</div></div>')
    # 2 · the graph in GR history: four lanes, each band's reduction over 3 s, peak at the right.
    gx, gy, gw, gh = 14, 83, 608, 188
    lanes = ""
    peaks = ["&minus;2.1", "&minus;6.8", "&minus;4.0", "&minus;1.2"]
    depth = [3, 9, 6, 2]
    for i, colour in enumerate(MC):
        top = 22 + i * 41
        pts = []
        for k in range(0, 541, 4):
            t = k / 540
            beat = (t * 6) % 1
            v = depth[i] * (math.exp(-beat * (4 + i)) * (0.75 + 0.25 * math.sin(7 * t + i)))
            pts.append(f"{40 + k:.0f},{top + 2 + v * 3.2:.1f}")
        lanes += (f'<line x1="40" y1="{top}" x2="580" y2="{top}" stroke="#3a3a3a"/>'
                  f'<text x="8" y="{top + 12}" fill="{colour}" font-size="11" font-family="Inter" font-weight="600">{i + 1}</text>'
                  f'<path d="M40,{top} L' + " L".join(pts) + f' L580,{top} Z" fill="{colour}" fill-opacity=".25"/>'
                  f'<path d="M' + " L".join(pts) + f'" stroke="{colour}" stroke-width="1.5" fill="none"/>'
                  f'<text x="586" y="{top + 12}" fill="{colour}" font-size="10" font-family="Inter">{peaks[i]}</text>')
    history = (f'<div class="o" style="left:{gx}px;top:{gy}px;width:{gw}px;height:{gh}px;background:#141414;border-radius:6px"></div>'
               f'<svg class="o" style="left:{gx}px;top:{gy + 16}px" width="{gw}" height="{gh - 16}">{lanes}</svg>'
               + seg(50, 89, 150, 18, ["Spectrum", "GR history"], 1, "#8D8D8D", fs=10.5, dark="#1E1E1E")
               + f'<div class="o cap" style="left:470px;top:91px;font-size:10px">last 3 s &middot; peak dB at right</div>'
               + outline(gx, gy, gw, gh))
    # 3 · the drawer: Set all bands, band 2's detector EQ, the crossover slopes.
    drawer = (cover(400, 334, 110, 20) + '<div class="o lbl" style="left:404px;top:336px;font-size:14px">Set all bands</div>'
              + caption(398, 457, "moves every band's Look")
              + outline(396, 332, 134, 132)
              + group_title(24, 428, f'<span style="color:{purple}">BAND 2</span> &middot; DETECTOR EQ')
              + knob(60, 446, "HP", "80 Hz", purple, 0.3, size=36, fs=13, width=70)
              + knob(136, 446, "LP", "Off", purple, 1.0, size=36, fs=13, width=70)
              + knob(216, 446, "Tilt", "+1.5 dB/oct", purple, 0.62, size=36, fs=13, bi=True, width=84)
              + caption(24, 540, "Shapes only what band 2's detector hears, never its audio. Follows the selected band tab.", 260)
              + outline(20, 422, 276, 152)
              + group_title(340, 486, "CROSSOVER SLOPES")
              + row(340, 503, 270, f'1&ndash;2 &middot; 120 Hz', dd("LR 24 dB/oct", 128, 22, 12))
              + row(340, 530, 270, f'2&ndash;3 &middot; 1.20 kHz', dd("LR 48 dB/oct", 128, 22, 12))
              + row(340, 560, 270, f'3&ndash;4 &middot; 6.00 kHz', dd("LR 24 dB/oct", 128, 22, 12))
              + outline(336, 482, 278, 106))
    return w1.page("Fancy MultiComp: Phase 4 Wave 2 mockup",
                   lead("docs/multicomp.md, Phase 4 Wave 2: per-band lookahead, detector EQ, crossover slope and GR history (items 23 to 26)", "Decisions 1 to 4 A"), [
        ("1 · The main window: a Look knob per band, the view switch and a crossover's slope",
         "<b>Look</b> (0 to 10 ms) joins the band row after Gain, in the row's free space. A <b>Spectrum | GR history</b> switch sits at the graph's top left. "
         "Right-clicking a crossover line opens its <b>slope</b> menu (here crossover 2 set to LR 48).",
         shot("multicomp", W, H, look + view + slope_menu,
              "The window keeps its size: the band card already had room after Gain. The latency is the largest Look of any band, and the bands stay lined up. "
              "Saved projects load every band at their old Lookahead, so they render as before, to the bit. Per-crossover slopes apply to the LR crossover types; "
              "Linear and Dynamic phase keep their own shapes, so the menu dims there.")),
        ("2 · The graph in GR history",
         "Each band's gain reduction over the last 3 s, one lane per band in its colour, with its peak at the right. It replaces the spectrum while chosen and draws only while the window is open.",
         shot("multicomp", W, H, history, "A view switch, not a parameter: it isn't saved with the project.", crop=(0, 73, 640, 210))),
        ("3 · The EDIT drawer",
         "Lookahead becomes <b>Set all bands</b>: turning it moves every band's Look at once. A <b>detector EQ</b> section for the selected band "
         "(HP and LP, Off or 20 Hz to 20 kHz at 24 dB/oct, and Tilt, &minus;6 to +6 dB/oct; all off by default) and a row per crossover for its <b>slope</b> fill the drawer's free lower half.",
         shot("multicomp-edit", W, H, drawer,
              "Detector EQ off isn't run. Picking LR 24 or LR 48 in the main window's Crossover menu still sets all three slopes at once.")),
    ])


# ---------------------------------------------------------------------------------------------
FIR_GROW = 70  # the Mid | Side switch beside the Mode row: 760 to 830 px


def fir_wide(name):
    """FIR 70 px wider. The graph is stretched (not padded), so its frequency axis stays continuous; its labels are redrawn."""
    from PIL import Image
    folder = pathlib.Path(__file__).resolve().parent / "img"
    src = Image.open(folder / f"{name}-w2b.png").convert("RGB")
    w, h = src.size
    d = 2 * FIR_GROW
    dst = Image.new("RGB", (w + d, h))
    for y0, y1, cut in ((0, 73, 535), (73, 112, 452), (312, 432, 440), (432, 465, 560)):
        a0, a1, c = 2 * y0, 2 * y1, 2 * cut
        dst.paste(src.crop((0, a0, c, a1)), (0, a0))
        dst.paste(src.crop((c - 1, a0, c, a1)).resize((d, a1 - a0)), (c, a0))
        dst.paste(src.crop((c, a0, w, a1)), (c + d, a0))
    a0, a1, s0, s1 = 2 * 112, 2 * 312, 2 * 46, 2 * 620
    dst.paste(src.crop((0, a0, s0, a1)), (0, a0))
    dst.paste(src.crop((s0, a0, s1, a1)).resize((s1 - s0 + d, a1 - a0), Image.LANCZOS), (s0, a0))
    dst.paste(src.crop((s1, a0, w, a1)), (s1 + d, a0))
    dst.save(folder / f"{name}-wide-w2b.png")
    return dst.size[0] // 2, dst.size[1] // 2


def fir_x(hz):
    import math
    return 46 + (122 + 57 * math.log2(hz / 50.0) - 46) * (574 + FIR_GROW) / 574


def fir_y(db):
    return 160 + db * -34 / 24


def fir_compressor(edit, side):
    """Compressor mode, redrawn over the EQ screenshot (CI has no Compressor screenshot): the switches, the cards, the curves."""
    import math
    import random
    teal, green, d = "#00A985", "#2DCC70", FIR_GROW
    random.seed(4)
    # Mode switch: only the selection moves, from EQ to Compressor.
    out = (cover(52, 86, 36, 22, "#141414") + '<div class="o lbl" style="left:61px;top:88px;font-size:12.5px">EQ</div>'
           + cover(129, 86, 82, 22, "#141414")
           + f'<div class="o" style="left:130px;top:86px;width:80px;height:22px;border-radius:3px;background:{teal};color:#fff;'
             'font-size:12.5px;display:flex;align-items:center;justify-content:center">Compressor</div>')
    out += seg(456, 85, 66, 24, ["Mid", "Side"], 1 if side else 0, teal, fs=12)
    out += cover(548, 84, 54, 26, "#303030")
    out += f'<div class="o btn" style="left:{455 + d}px;top:85px;width:48px;height:25px;font-size:12px">Learn</div>'
    out += f'<div class="o lbl" style="left:{506 + d}px;top:89px;font-size:12px;color:#8D8D8D">Edit</div>'

    x0, x1 = 46, 620 + d
    plot = f'<div class="o" style="left:{x0}px;top:118px;width:{x1 - x0}px;height:176px;background:#262626"></div>'
    svg = []
    for db in (24, 0, -24, -48, -72):
        svg.append(f'<line x1="{x0}" y1="{fir_y(db):.1f}" x2="{x1}" y2="{fir_y(db):.1f}" stroke="#333"/>')
    for hz in (50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000):
        svg.append(f'<line x1="{fir_x(hz):.1f}" y1="118" x2="{fir_x(hz):.1f}" y2="292" stroke="#2e2e2e"/>')
    pts_in, pts_out = [], []
    hz = 20.0
    while hz <= 20000:
        base = -40 - 8 * math.log10(hz / 400) ** 2 + random.uniform(-2.5, 2.5)
        bump = 12 * math.exp(-(math.log2(hz / 3000) / 0.5) ** 2)
        thr = -30 - 6 * math.exp(-(math.log2(hz / 3000) / 0.6) ** 2)
        lvl = base + bump
        ratio = 1 + 3 * math.exp(-(math.log2(hz / 3000) / 0.55) ** 4)
        red = max(0.0, (lvl - thr) * (1 - 1 / ratio))
        x = max(x0, fir_x(hz))
        pts_in.append(f"{x:.1f},{fir_y(lvl):.1f}")
        pts_out.append(f"{x:.1f},{fir_y(lvl - red):.1f}")
        hz *= 1.02
    svg.append(f'<path d="M{x0},292 L' + " L".join(pts_in) + f' L{x1},292 Z" fill="#8D8D8D" fill-opacity=".18"/>')
    svg.append('<path d="M' + " L".join(pts_in) + '" stroke="#8D8D8D" stroke-width="1" fill="none"/>')
    svg.append('<path d="M' + " L".join(pts_out) + '" stroke="#F39C12" stroke-width="1.2" fill="none"/>')
    thr_pts = [(20, -30), (1200, -30), (3000, -36), (8000, -30), (20000, -30)]
    rat_pts = [(20, 1.0), (1800, 1.0), (2200, 4.0), (5000, 4.0), (6000, 1.0), (20000, 1.0)]

    def ry(r):
        return 290 - math.log(r) / math.log(20) * 140

    def smooth(points, yfun):
        path, f = [], 20.0
        while f <= 20000:
            for (h0, v0), (h1, v1) in zip(points, points[1:]):
                if h0 <= f <= h1:
                    t = math.log(f / h0) / math.log(h1 / h0)
                    t = 0.5 - 0.5 * math.cos(math.pi * t)
                    path.append(f"{max(x0, fir_x(f)):.1f},{yfun(v0 + (v1 - v0) * t):.1f}")
                    break
            f *= 1.02
        return "M" + " L".join(path)

    thr_on, rat_on = edit == "threshold", edit == "ratio"
    dash = ' stroke-dasharray="4 3"'
    svg.append(f'<path d="{smooth(thr_pts, fir_y)}" stroke="{teal}" stroke-width="{2 if thr_on else 1.2}" fill="none" '
               f'opacity="{1 if thr_on else .45}"{"" if thr_on else dash}/>')
    svg.append(f'<path d="{smooth(rat_pts, ry)}" stroke="{green}" stroke-width="{2 if rat_on else 1.2}" fill="none" '
               f'opacity="{1 if rat_on else .45}"{"" if rat_on else dash}/>')
    for f, v in (thr_pts[1:-1] if thr_on else rat_pts[1:-1]):
        y = fir_y(v) if thr_on else ry(v)
        svg.append(f'<circle cx="{fir_x(f):.1f}" cy="{y:.1f}" r="4.5" fill="#fff" stroke="{teal if thr_on else green}" stroke-width="2"/>')
    if rat_on:
        for r in (1, 2, 4, 20):
            svg.append(f'<text x="{x1 - 4}" y="{ry(r) + 3:.1f}" fill="{green}" font-size="9.5" font-family="Inter" text-anchor="end">{r}:1</text>')
        svg.append(f'<rect x="{fir_x(3000) + 8:.0f}" y="{ry(4) - 30:.0f}" width="112" height="18" rx="3" fill="#262626" stroke="#434343"/>'
                   f'<text x="{fir_x(3000) + 14:.0f}" y="{ry(4) - 17:.0f}" fill="#fff" font-size="11" font-family="Inter">3.00 kHz &#183; 4.0:1</text>')
    plot += f'<svg class="o" style="left:0;top:0" width="{760 + d}" height="320">' + "".join(svg) + '</svg>'
    labels = cover(x0, 296, x1 - x0, 14, "#262626") + "".join(
        f'<div class="o cap" style="left:{fir_x(f) - 14:.0f}px;top:296px;width:28px;text-align:center;font-size:11px">{t}</div>'
        for f, t in ((50, "50"), (100, "100"), (200, "200"), (500, "500"), (1000, "1k"), (2000, "2k"), (5000, "5k"), (10000, "10k"), (20000, "20k")))
    legend = (f'<div class="o cap" style="left:{x1 - 290}px;top:126px;width:284px;text-align:right;font-size:11px;background:#262626">'
              f'<span style="color:#8D8D8D">&#9644;</span> input &nbsp;<span style="color:#F39C12">&#9644;</span> output &nbsp;'
              f'<span style="color:{teal}">&#9644;</span> threshold &nbsp;<span style="color:{green}">&#9644;</span> ratio</div>')
    switch = (seg(54, 124, 128, 18, ["Threshold", "Ratio"], 0 if thr_on else 1, teal if thr_on else green, fs=10.5, dark="#1E1E1E")
              + outline(54, 124, 128, 18))
    cards = (cover(18, 318, 168, 112) + '<div class="o cap" style="left:26px;top:326px">THRESHOLD CURVE</div>'
             + '<div class="o btn" style="left:26px;top:348px;width:152px;height:24px">Reset curve</div>'
             + '<div class="o btn new" style="left:26px;top:382px;width:152px;height:24px">Reset ratio curve</div>'
             + cover(204, 318, 330, 108)
             + knob(239, 326, "Ratio", "4.0:1", teal, 0.4, size=40, fs=12, width=70)
             + knob(303, 326, "Attack", "20 ms", teal, 0.3, size=40, fs=12, width=70)
             + knob(367, 326, "Release", "200 ms", teal, 0.4, size=40, fs=12, width=70)
             + caption(412, 342, "Ratio moves the whole<br>ratio curve.", None))
    footer = ('<div class="o" style="left:225px;top:442px"><div class="cb" style="font-size:12px"><i></i>Sidechain detector</div></div>'
              '<div class="o" style="left:365px;top:442px"><div class="cb" style="font-size:12px"><i></i>Listen to reduced</div></div>')
    ms_outline = outline(456, 85, 66, 24)
    return out + ms_outline + plot + labels + legend + switch + cards + footer


@plugin("fir", "59__fir_w2_mockup.png")
def fir():
    W, H = fir_wide("fir-eq")
    fir_wide("fir-edit")
    d = FIR_GROW
    ms = (cover(258, 360, 380 + d, 90)
          + '<div class="o" style="left:270px;top:372px"><div class="cb" style="font-size:14px;--a:#00A985"><i class="x"></i>Mid/Side</div></div>'
          + caption(296, 396, "Every mode works on mid and side instead of left and right, each with its own curve, "
                              "Learn and gains. The Mid | Side switch beside the Mode row picks which one you edit. Convolve L/R stays left and right.", 400)
          + outline(264, 366, 440, 86))
    return w1.page("Fancy FIR: Phase 4 Wave 2 mockup (ratio curve and Mid/Side)",
                   lead("docs/fir.md, Phase 4 Wave 2: ratio curve (item 27, with item 29 folded in) and Mid/Side (item 28)", "Decisions 1 to 3 A"), [
        ("1 · Compressor mode, editing the threshold curve",
         "A <b>Threshold | Ratio</b> switch at the graph's top left picks which curve the points edit; the other stays drawn, dimmed and dashed. "
         "With Mid/Side on, a <b>Mid | Side</b> switch sits beside the Mode row. To fit it at full size, <b>the window grows 70 px wider, from 760 to 830 px</b> "
         "(its minimum too): the graph gets wider rather than padded, so its frequency axis stays continuous. "
         "Compressor mode's cards are redrawn as built, because CI has no Compressor screenshot; the EQ screenshot is underneath.",
         shot("fir-eq-wide", W, H, fir_compressor("threshold", False),
              "The threshold curve dips at 3 kHz, where the mix gets harsh, and the ratio curve (dimmed) is 4:1 from 2 to 5 kHz and 1:1 elsewhere: "
              "the model's <b>Dynamic de-harsh</b> factory preset. A saved project's ratio curve loads flat at its Ratio, so it renders as before, to the bit.")),
        ("2 · Compressor mode, editing the ratio curve, Side chosen",
         "The ratio curve's points now edit, on its own scale at the right (1:1 at the bottom to 20:1 at the top; below 1:1 is upward, as the Ratio knob allows). "
         "The selected point shows its frequency and ratio.",
         shot("fir-eq-wide", W, H, fir_compressor("ratio", True),
              "The Ratio knob moves the whole ratio curve, as it moved the single ratio before. <b>Reset ratio curve</b> flattens it to the knob's value.")),
        ("3 · The EDIT drawer: Mid/Side",
         "A <b>Mid/Side</b> checkbox (off by default) beside Learn length and Output. Off, FIR is as it was, bit for bit, and the Mid | Side switch isn't shown.",
         shot("fir-edit-wide", W, H, ms, "")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("ds_w2b", "60__ds_w2b_mockup.png")
def ds_w2b():
    W, H = 760, 465
    green, blue = "#2DCC70", "#3598DB"
    detection = cover(214, 292, 122, 22) + seg(216, 293, 118, 20, ["Band", "Shelf", "Hiss"], 2, green, fs=11.5, dark="#141414") + outline(216, 293, 118, 20)
    width_dim = dim(232, 316, 100, 96)
    drawer = (cover(20, 288, 360, 80)
              + row(24, 293, 346, "Reduce", dd("Mid", 170, 24, 13))
              + outline(22, 291, 350, 28)
              + '<div class="o lbl" style="left:24px;top:338px;font-size:14px;color:#8D8D8D">Ceiling</div>')
    ceiling = ('<div class="o" style="left:206px;top:326px;width:56px" data-k data-v="0" data-c="' + green + '" data-s="40" data-fs="12" data-l="Ceiling" data-t=""></div>')
    menu = ('<div class="o new menu" style="left:384px;top:290px;width:120px;font-size:13px">'
            '<div>Both</div><div class="on">&#10003; Mid</div><div>Side</div></div>')
    profiles = ["Female voice", "Male voice", "Sibilance", "Harshness", "Acoustic guitar", "Bowed strings"]
    mini_sel = (cover(40, 88, 200, 24, "#303030") + '<div class="o lbl" style="left:40px;top:90px;width:200px;text-align:center;font-size:14px;font-weight:600">Hiss</div>'
                + cover(30, 118, 220, 16) + '<div class="o cap" style="left:30px;top:119px;width:220px;text-align:center;font-size:12px">Hiss &middot; above 5.0 kHz, in the gaps</div>'
                + outline(18, 86, 240, 50))
    mini_menu = ('<div class="o new menu" style="left:60px;top:140px;width:170px;font-size:13px">'
                 + "".join(f"<div>{p}</div>" for p in profiles)
                 + '<div style="border-top:1px solid #434343;margin-top:3px;padding-top:6px" class="on">&#10003; Hiss</div></div>')
    return w1.page("Fancy DS and DS-Mini: Phase 4 Wave 2 mockup (Mid/Side reduction and Hiss)",
                   lead("docs/ds.md, Phase 4 Wave 2: Mid/Side reduction, Hiss mode and DS-Mini's Hiss profile (items 32 to 34)", "Decisions 1 to 3 A"), [
        ("1 · DS: Band | Shelf | Hiss in the DETECTION card",
         "<b>Hiss</b> is a third choice in the Detection switch, which keeps its size and text. Hiss uses the shelf above Freq and turns it down while it falls <b>below</b> "
         "Threshold, by up to Range. Width dims, as it does in Shelf.",
         shot("ds", W, H, detection + width_dim,
              "Attack is how fast the cut lets go when the highs come back, and Release how fast it closes in a gap, so they keep their meaning. "
              "The history then shows the reduction in the gaps, between the &quot;s&quot; sounds. Band and Shelf are as today, to the bit.")),
        ("2 · DS: the EDIT drawer's Reduce row",
         "<b>Reduce: Both | Mid | Side</b> (Both by default) goes under Filter slope; Ceiling moves down a row. With Mid, only the centre is de-essed, so wide cymbals keep their highs.",
         shot("ds-edit", W, H, drawer + ceiling + menu, "Both is today's path, to the bit. The main window doesn't change.")),
        ("3 · DS-Mini: a seventh profile, Hiss",
         "<b>Hiss</b> comes after the six sibilance and instrument profiles, after a divider: Hiss mode with the shelf at 5 kHz. Amount sets how deep it cuts in the gaps.",
         shot("dsmini", 392, 492, mini_sel + mini_menu,
              "The caption names the band, as for the other profiles. <b>Save as Fancy DS preset</b> carries it over to DS as Hiss mode with the same settings.")),
    ])


# ---------------------------------------------------------------------------------------------
SAT = "#F39C12"


def sature_type(selected):
    names = ["Tape", "Tube", "Transistor", "Clip", "Diode", "Fold"]
    return (cover(50, 84, 342, 24, "#303030")
            + seg(52, 84, 336, 24, names, selected, SAT, fs=12.5, new=False)
            + outline(52 + 236, 84, 100, 24))


def sature_tabs(selected):
    return seg(120, 126, 210, 22, ["Shape", "Tone and stereo"], selected, "#4C4C4C", fs=12.5, text="#fff")


@plugin("sature_w2b", "61__sature_w2b_mockup.png")
def sature_w2b():
    W, H = 760, 465
    main = sature_type(0)
    page1 = (sature_type(0) + sature_tabs(0)
             + knob(60, 362, "Offset", "0.10", SAT, 0.6, size=40, fs=14, bi=True, width=80)
             + caption(104, 388, "Bias before the curve:<br>even harmonics,<br>the 2nd first.", None)
             + outline(22, 358, 200, 90))
    p2 = (sature_type(0) + sature_tabs(1) + cover(14, 160, 634, 300)
          + group_title(24, 168, "LOW CUT")
          + knob(62, 184, "Freq", "200 Hz", SAT, 0.3, size=40, fs=13, width=80)
          + '<div class="o lbl" style="left:118px;top:194px;font-size:12px;color:#8D8D8D">Slope</div>'
          + '<div class="o dd" style="left:160px;top:189px;width:110px;height:24px;font-size:12px">24 dB/oct</div>'
          + seg(160, 224, 110, 22, ["Pre", "Post"], 0, SAT, fs=12)
          + group_title(330, 168, "HIGH CUT")
          + knob(368, 184, "Freq", "Off", SAT, 1.0, size=40, fs=13, width=80)
          + '<div class="o lbl" style="left:424px;top:194px;font-size:12px;color:#8D8D8D">Slope</div>'
          + '<div class="o dd" style="left:466px;top:189px;width:110px;height:24px;font-size:12px;color:#8D8D8D">12 dB/oct</div>'
          + seg(466, 224, 110, 22, ["Pre", "Post"], 1, SAT, fs=12)
          + caption(24, 276, "Pre shapes what's driven: a Low cut before the curve stops a heavy bass from muddying everything above it. "
                             "Post shapes what's heard. Off (the default) isn't run.", 560)
          + group_title(24, 324, "STEREO")
          + knob(62, 340, "Spread", "35 %", SAT, 0.7, size=40, fs=13, width=80)
          + caption(118, 352, "Widens only the added harmonics; the fundamental stays centred. 0 to 50 %, so mono keeps them (&minus;3 dB at most).", 220)
          + '<div class="o lbl" style="left:370px;top:356px;font-size:13px;color:#8D8D8D">Channels</div>'
          + '<div class="o dd" style="left:440px;top:351px;width:136px;height:24px;font-size:12.5px">Mid only</div>'
          + caption(370, 384, "Stereo (today), Mid only or Side only.", 210)
          + outline(18, 162, 626, 284))
    return w1.page("Fancy Sature: Phase 4 Wave 2 mockup (curve shaping, Diode and Fold, cuts, spread, Mid/Side)",
                   lead("docs/sature.md, Phase 4 Wave 2: items 35 to 39", "Decisions 1 to 3 A: Offset only (no Knee), Spread 0 to 50 %"), [
        ("1 · The main window: Diode and Fold in the Type switch",
         "<b>Diode</b> (even and odd harmonics, a rectified buzz) and <b>Fold</b> (past the ceiling the wave folds back: metallic, synth-like) are added after Clip. "
         "The switch has room for six in its row.",
         shot("sature", W, H, main,
              "The curve and the harmonic bars show the chosen Type, as they do now. Both new curves use ADAA, as the others do. Existing Types sound as today, to the bit.")),
        ("2 · The EDIT drawer, page Shape",
         "The drawer gets two pages, <b>Shape | Tone and stereo</b>, beside its title, as Comp's drawer has. Shape keeps today's contents and adds <b>Offset</b> "
         "(&minus;0.5 to +0.5, 0 by default) under Input trim.",
         shot("sature-edit", W, H, page1,
              "At Offset 0.25 on Tape, H2 rises to about &minus;21 dB while H3 stays put. The DC it adds is filtered out after the curve.")),
        ("3 · The EDIT drawer, page Tone and stereo",
         "<b>Low cut</b> and <b>High cut</b> (Off, or 20 Hz to 20 kHz; 12 or 24 dB/oct; Pre or Post), <b>Spread</b> (0 to 50 %) and <b>Channels</b> (Stereo, Mid only, Side only).",
         shot("sature-edit", W, H, p2,
              "Every new setting defaults to today's sound, to the bit: cuts Off, Spread 0, Channels Stereo.")),
    ])


# ---------------------------------------------------------------------------------------------
EXC = "#E74D3D"


def exciter_tabs(selected):
    return cover(20, 88, 44, 20) + seg(22, 86, 150, 22, ["Main", "Harmonics"], selected, "#4C4C4C", fs=12, text="#fff")


@plugin("exciter", "62__exciter_w2_mockup.png")
def exciter():
    W, H = 392, 452
    teal = "#00A985"
    page1 = (exciter_tabs(0) + cover(14, 288, 248, 140)
             + '<div class="o lbl" style="left:32px;top:296px;font-size:13px">Oversampling filter</div>'
             + '<div class="o dd" style="left:156px;top:292px;width:96px;height:24px;font-size:12.5px">Linear</div>'
             + outline(28, 290, 228, 28)
             + knob(77, 328, "Ceiling", "Off", teal, 0.0, size=40, fs=14, width=80)
             + '<div class="o" style="left:142px;top:372px"><div class="cb" style="font-size:13px"><i></i>Auto gain</div></div>')
    page2 = (exciter_tabs(1) + cover(14, 118, 248, 330)
             + group_title(24, 126, "BAND 2")
             + knob(72, 140, "Freq 2", "400 Hz", EXC, 0.35, size=44, fs=13, width=84)
             + knob(180, 140, "Harmonics 2", "30 %", EXC, 0.3, size=44, fs=13, width=96)
             + caption(24, 242, "Adds harmonics from Freq 2 up to Freq: warmth and body under today's band.", 230)
             + group_title(24, 284, "SHAPE")
             + knob(56, 298, "Punch", "50 %", EXC, 0.5, size=36, fs=12, width=72)
             + knob(138, 298, "High cut", "12 kHz", EXC, 0.7, size=36, fs=12, width=72)
             + knob(220, 298, "Width", "120 %", EXC, 0.8, size=36, fs=12, width=72)
             + caption(24, 386, "Punch lifts the harmonics on attacks; High cut and Width act on the added harmonics only.", 230)
             + outline(18, 120, 242, 322))
    return w1.page("Fancy Exciter: Phase 4 Wave 2 mockup",
                   lead("docs/exciter.md, Phase 4 Wave 2: two bands, Punch, a high cut, harmonics width and minimum-phase oversampling (items 40 to 44)", "Decisions 1 to 4 A"), [
        ("1 · The EDIT drawer, page Main",
         "The drawer's title becomes two pages, <b>Main | Harmonics</b>, as Sature's drawer gets. Main keeps today's controls and adds "
         "<b>Oversampling filter</b> (Linear phase, the default, or Minimum phase) under Oversampling; Ceiling and Auto gain move down a row.",
         shot("exciter-edit", W, H, page1,
              "Minimum phase cuts the latency from 69 samples to a few, but the parallel sum then loses up to 0.7 dB near the top, which the tooltip says. "
              "The main window keeps its four knobs and its size.")),
        ("2 · The EDIT drawer, page Harmonics",
         "<b>Band 2</b>: Freq 2 (100 Hz up to Freq) and Harmonics 2 (0 to 100 %, 0 by default). <b>Punch</b> (0 to 100 %), <b>High cut</b> (Off, or 4 to 20 kHz; Off by default) "
         "and <b>Width</b> (0 to 150 %, 100 % by default).",
         shot("exciter-edit", W, H, page2,
              "Every new setting defaults to today's sound, to the bit: Harmonics 2 at 0 isn't run, Punch 0, High cut Off, Width 100 %, Linear phase.")),
    ])


# ---------------------------------------------------------------------------------------------
@plugin("kicker_duck", "63__kicker_duck_mockup.png")
def kicker_duck():
    # 40 px wider (392 to 432) so the DUCK tag fits beside EDIT: the knob card widens evenly on both sides.
    W, H = grow("kicker", "kicker-wide", 40, [(0, 73, 280), (73, 440, [12, 250]), (440, 492, 250)])
    grow("kicker-edit", "kicker-edit-wide", 40, [(0, 73, 280), (73, 492, 262)])
    red = "#E74D3D"
    tabs = cover(20, 88, 48, 20) + seg(22, 86, 160, 22, ["Main", "Duck and clip"], 0, "#4C4C4C", fs=12, text="#fff")
    tabs2 = cover(20, 88, 48, 20) + seg(22, 86, 160, 22, ["Main", "Duck and clip"], 1, "#4C4C4C", fs=12, text="#fff")
    tag = (cover(84, 452, 180, 24, "#303030")
           + '<div class="o new pill" style="left:84px;top:455px;width:48px;height:18px;background:' + red + ';color:#141414;'
             'font-weight:700;font-size:10px;letter-spacing:.06em">DUCK</div>'
           + '<div class="o cap" style="left:142px;top:456px;font-size:12px;color:#A0A0A0">Boost: lifts its own low end</div>')
    page2 = (tabs2 + cover(14, 118, 250, 370)
             + group_title(24, 126, "DUCK")
             + knob(64, 140, "Duck", "6.0 dB", red, 0.25, size=44, fs=13, width=84)
             + knob(176, 140, "Release", "120 ms", red, 0.2, size=44, fs=13, width=84)
             + row(24, 244, 230, "Duck from", dd("Host sync", 120, 24, 12.5))
             + group_title(24, 282, "HOST SYNC")
             + '<div class="o lbl" style="left:24px;top:304px;font-size:13px">Rate</div>'
             + '<div class="o dd" style="left:62px;top:300px;width:84px;height:24px;font-size:12.5px">1/4</div>'
             + knob(206, 282, "Shape", "50 %", red, 0.47, size=30, fs=12, width=70)
             + outline(18, 120, 242, 246)
             + group_title(24, 384, "CLIPPER")
             + row(24, 402, 230, "Clip oversampling", dd("4x", 70, 24, 12.5))
             + caption(24, 434, "Latency is reported while it's on.", 230)
             + outline(18, 378, 242, 76))
    return w1.page("Fancy Kicker: Phase 4 mockup (sidechain Duck and the oversampled clipper)",
                   lead("docs/kicker.md: sidechain Duck (Decisions 1 to 3 A) and Wave 2 item 45, Clip oversampling", "Decisions 1 and 2 A"), [
        ("1 · The main window: a DUCK tag beside EDIT, while Duck is above 0",
         "To fit the tag beside the EDIT link with the mode caption after it, <b>the window grows 40 px wider, from 392 to 432 px</b> (its minimum too; user, 23 November 2026): "
         "the knob card widens evenly on both sides, so Freq, Wet and Dry stay centred. "
         "The tag shows only while Duck is above 0, so a ducking Kicker is never a surprise; clicking it opens the drawer's second page.",
         shot("kicker-wide", W, H, tag, "In Sub mode the row is the same, with Sub's caption.")),
        ("2 · The EDIT drawer, page Duck and clip",
         "The drawer is full, so it gets two pages, <b>Main | Duck and clip</b>, as Sature's and Exciter's do; Main is today's drawer. "
         "<b>Duck</b> (0 to 24 dB, 0 by default) and <b>Release</b> (20 to 500 ms, 120 by default), <b>Duck from</b> (Sidechain or Host sync), "
         "with Rate (1/1 to 1/16) and Shape while Host sync is chosen: a pulse on the host's grid, for ducking on the beat without a kick track. <b>Clip oversampling</b> (Off, 2x, 4x; Off by default) is here too, since Main has no row free under Ceiling.",
         shot("kicker-edit-wide", W, H, page2,
              "Duck ducks the whole output, the dry bass with the added low end (Decision 1 A). Duck 0 and Clip oversampling Off are today's sound, to the bit. "
              "With Sidechain chosen and nothing routed, the main window shows the \"No sidechain signal\" notice.")),
        ("3 · The EDIT drawer, page Main",
         "Today's drawer, unchanged, under the new page switch.",
         shot("kicker-edit-wide", W, H, tabs, "")),
    ])


# ---------------------------------------------------------------------------------------------
# Comp's level history on a dB scale (docs/comp.md, "Level history in dB"). history_svg() redraws
# fancy::ui::LevelHistory::paint at 1x, linear (today) or in dB, from columns like the editor's preview.
def beat_columns():
    """CompEditor's screenshot preview: a beat at 120 BPM into 3:1 at -12 dB, 952 columns (3 s)."""
    import math
    cols, reduction, previous = [], 0.0, -1
    for c in range(952):
        t = c * 3.0 / 952
        beat_pos = (t - 0.11) / 0.5 + 10.0
        phase = beat_pos - math.floor(beat_pos)
        a = 0.95 if phase < 0.03 else 0.95 * math.exp(-phase * 3.2)
        a = a * (0.8 + 0.2 * abs(math.sin(c * 0.85))) + 0.03
        level = 20 * math.log10(a)
        target = max(0.0, level + 12.0) * (1 - 1 / 3)
        reduction += (0.7 if target > reduction else 0.03) * (target - reduction)
        beat = math.floor(beat_pos)
        mark = 0 if previous < 0 or beat == previous else (2 if beat % 4 == 0 else 1)
        previous = beat
        cols.append((a, a * 10 ** (-reduction / 20), reduction, -12.0, mark))
    return cols


def quiet_columns():
    """A spoken phrase: syllables peaking at -14 to -24 dBFS, breaths at -45, into 3:1 at -24 dB, 3 s."""
    import math
    import random
    rng = random.Random(7)
    syllables = [(0.10, 0.16, -16), (0.30, 0.12, -22), (0.47, 0.20, -14), (0.72, 0.10, -24), (0.86, 0.18, -18),
                 (1.62, 0.14, -15), (1.80, 0.22, -20), (2.07, 0.12, -14), (2.24, 0.16, -23), (2.45, 0.24, -17)]
    cols, reduction = [], 0.0
    for c in range(952):
        t = c * 3.0 / 952
        a = 10 ** ((-52 + 4 * math.sin(t * 9)) / 20)  # room tone
        if 1.18 < t < 1.50:
            a = max(a, 10 ** (-44 / 20) * math.sin(math.pi * (t - 1.18) / 0.32))  # a breath
        for start, length, peak in syllables:
            if start <= t < start + length + 0.15:
                x = (t - start) / length
                env = math.sin(math.pi * min(x, 1.0) / 2) if x < 0.3 else math.exp(-(x - 0.3) * 3.5)
                a = max(a, 10 ** (peak / 20) * env)
        a *= 0.75 + 0.25 * rng.random()
        level = 20 * math.log10(max(a, 1e-6))
        target = max(0.0, level + 24.0) * (1 - 1 / 3)
        reduction += (0.5 if target > reduction else 0.02) * (target - reduction)
        cols.append((a, a * 10 ** (-reduction / 20), reduction, -24.0, 0))
    return cols


def history_svg(cols, scale, floor_db=-60.0, w=476, h=260):
    """One graph at 1x. scale "lin" is today's paint; "db" maps a peak's level linearly in dB from the
    centre line (floor_db) to the edges (0 dB), with faint level lines and their labels on the left."""
    import math
    mid, amp = h / 2, h / 2 - 8
    per = len(cols) / w

    def dist(peak):
        if scale == "lin":
            return min(1.0, peak) * amp
        if peak <= 0:
            return 0.0
        return max(0.0, min(1.0, (20 * math.log10(peak) - floor_db) / -floor_db)) * amp

    def ry(db):
        return 4 + (h - 8) * max(0.0, min(1.0, db / 24))

    px = []
    for x in range(w):
        chunk = cols[int(x * per):max(int(x * per) + 1, int((x + 1) * per))]
        px.append((max(c[0] for c in chunk), max(c[1] for c in chunk), max(c[2] for c in chunk), max(c[4] for c in chunk)))
    s = [f'<svg width="{w}" height="{h}" style="position:absolute;left:0;top:0" font-family="Inter" font-size="9">',
         f'<rect width="{w}" height="{h}" rx="6" fill="#141414"/>']
    one = scale == "db1"  # one-sided: floor at the bottom, 0 dB at the top
    if one:
        amp = h - 8

        def spans(d):
            return h - d, d
    else:
        def spans(d):
            return mid - d, 2 * d
    labels = []
    if scale != "lin":
        for db in list(range(-12, int(floor_db), -12)) + [int(floor_db)]:
            d = dist(10 ** (db / 20)) if db > floor_db else 0.0
            ys = (h - d,) if one else ((mid - d, mid + d) if d else (mid,))
            for y in ys:
                s.append(f'<rect x="0" y="{y:.2f}" width="{w}" height="1" fill="#fff" fill-opacity=".07"/>')
            if d or not one:
                labels.append((db, ys[0] - 2))
    for colour, alpha, i in (("#5a5a5a", .9, 0), ("#00A985", .85, 1)):
        for x, p in enumerate(px):
            d = dist(p[i])
            if d > 0.25:
                y0, hh = spans(d)
                s.append(f'<rect x="{x}" y="{y0:.2f}" width="1" height="{hh:.2f}" fill="{colour}" fill-opacity="{alpha}"/>')
    for db, y in labels:
        s.append(f'<text x="5" y="{y:.1f}" fill="#B0B0B0" stroke="#141414" stroke-width="2.5" paint-order="stroke">{db}</text>')
    for x, p in enumerate(px):
        if p[3]:
            s.append(f'<rect x="{x}" y="0" width="1" height="{h}" fill="#fff" fill-opacity="{.35 if p[3] == 2 else .14}"/>')
    d = dist(10 ** (cols[-1][3] / 20))
    for y in ((h - d,) if one else (mid - d, mid + d)):
        s.append(f'<line x1="0" y1="{y:.2f}" x2="{w}" y2="{y:.2f}" stroke="#fff" stroke-opacity=".45" stroke-dasharray="4 4"/>')
    s.append('<polyline fill="none" stroke="#FFCF07" stroke-width="2" points="'
             + " ".join(f"{x + .5},{ry(p[2]):.2f}" for x, p in enumerate(px)) + '"/>')
    for db in (6, 12, 18):
        s.append(f'<text x="{w - 4}" y="{ry(db) - 4:.1f}" text-anchor="end" fill="#FFCF07" fill-opacity=".55">{db}</text>')
    ly = h - 14
    s.append(f'<rect x="6" y="{ly + 2}" width="8" height="8" fill="#5a5a5a"/><text x="17" y="{ly + 9}" fill="#8D8D8D">in</text>'
             f'<rect x="34" y="{ly + 2}" width="8" height="8" fill="#00A985"/><text x="45" y="{ly + 9}" fill="#00A985">out</text>'
             f'<rect x="68" y="{ly + 5}" width="8" height="2" fill="#FFCF07"/><text x="79" y="{ly + 9}" fill="#FFCF07">GR</text>')
    s.append("</svg>")
    return "".join(s)


def graph_box(svg, outlined=True):
    return (f'<div class="o{" new" if outlined else ""}" style="left:236px;top:213px;width:476px;height:260px">{svg}</div>')


@plugin("comp_db", "65__comp_db_history_mockup.png")
def comp_db():
    W, H = 934, 547
    beat, quiet = beat_columns(), quiet_columns()
    after = graph_box(history_svg(beat, "db"))

    def pair(cols):
        return ('<div class="row">'
                + "".join(f'<div><div class="cap" style="margin-bottom:6px">{t}</div><div class="shot" style="width:476px;height:260px">'
                          f'{history_svg(cols, s)}</div></div>' for t, s in (("Today: linear", "lin"), ("Proposed: dB, floor -60", "db")))
                + '<div class="side">The same phrase both ways. On the linear scale a syllable at -24 dBFS reaches 6 % of the half-height and the -24 dB '
                  'threshold sits 8 px from the centre, so neither the reduction nor the breath at -44 can be read. In dB every syllable, the breath and '
                  'the room tone at -52 are visible, and the threshold sits 60 % of the way out.</div></div>')

    return w1.page("Fancy Comp: level history on a dB scale (mockup)",
                   lead("docs/comp.md, \"Level history in dB\"", "Decisions 1 to 3 A"), [
        ("1 · Before: the History page today (v1.7.0, linear)",
         "The CI screenshot, unchanged: a beat at 120 BPM into 3:1 at -12 dB. The threshold is a quarter of the way out, and everything under about -30 dBFS is a hairline at the centre.",
         shot("comp-history", W, H, "", "The page, the zoom switch, Grid, peak and the GR line from the top (0 to 24 dB, right) all stay as they are.")),
        ("2 · After: the same beat on a dB scale",
         "Only the waveforms and the threshold move: each column's peak is drawn at its level in dB, from <b>-60 dB at the centre line</b> to <b>0 dB at the edges</b> (Decision 1 A, mirrored; Decision 2 A, a fixed 60 dB). "
         "Faint level lines every 12 dB, with their values in grey at the left edge; the GR scale stays at the right in yellow. "
         "On this scale the gap between the grey and teal edges is the gain reduction itself, at 3.5 px per dB.",
         shot("comp-history", W, H, after,
              "No new control (Decision 3 A: dB only, the linear view goes). Not a parameter and nothing saved; the recorder and its columns are unchanged, "
              "so this is a paint change in <b>fancy::ui::LevelHistory</b> only. The tooltip and the manual say the scale.")),
        ("3 · Why: a quiet passage, both ways",
         "A spoken phrase, syllables at -14 to -24 dBFS and a breath at -44, into 3:1 at -24 dB.",
         pair(quiet)),
        ("4 · Decision 1 B for comparison: one-sided, level up from the bottom",
         "The same two signals with the level drawn up from -60 dB at the bottom edge to 0 dB at the top, as a level meter over time: twice the height per dB (7 px), "
         "one threshold line, but it is no longer a waveform, and loud peaks run into the GR line coming down from the top.",
         '<div class="row">' + "".join(f'<div class="shot" style="width:476px;height:260px">{history_svg(c, "db1")}</div>' for c in (beat, quiet))
         + '<div class="side">Left: the beat. Right: the spoken phrase.</div></div>'),
    ])


# ---------------------------------------------------------------------------------------------
# Fancy EQ's band list and groups (Wave 2 item 46, ▲: layout approval before its design note).
# Drawn on EQ's screenshot from run 45451969543 (commit 5c8681c), img/eq-w2b.png. The band strip
# (tabs and editor) is y 417 to 595 at 1x, the tabs x 298 to 469, the fader column from x 734.
EQ_BANDS = [  # colour, number, type, freq, gain, Q, channel, dyn, group
    ("#E74D3D", 1, "High pass", "68 Hz", "&mdash;", "0.71", "ST", "&mdash;", "&mdash;"),
    ("#9A59B5", 2, "Band", "487.7 Hz", "&minus;5.1 dB", "2.0", "ST", "&mdash;", "A"),
    ("#3598DB", 3, "Band", "2.0 kHz", "+5.4 dB", "1.0", "ST", "&minus;6 dB", "A"),
    ("#2DCC70", 4, "Low pass", "14.5 kHz", "&mdash;", "0.71", "ST", "&mdash;", "&mdash;"),
]
# Full columns (A and C): name, x, width, field index in EQ_BANDS (None: drawn specially).
EQ_COLS = [("", 16, 30, None), ("On", 50, 26, "on"), ("Type", 84, 104, 2), ("Freq", 196, 84, 3), ("Gain", 288, 76, 4), ("Q", 372, 46, 5),
           ("Ch", 426, 46, 6), ("Dyn", 480, 64, 7), ("Group", 552, 62, 8), ("Solo", 622, 40, "solo"), ("", 670, 30, "x")]
# B's narrower list, 300 px: Channel, Dyn, Solo and delete stay in the band editor.
EQ_COLS_NARROW = [("", 8, 20, None), ("On", 30, 18, "on"), ("Type", 50, 58, 2), ("Freq", 126, 58, 3), ("Gain", 186, 56, 4),
                  ("Q", 244, 28, 5), ("Grp", 272, 26, 8)]


def eq_switch(selected):
    return seg(14, 419, 112, 20, ["Bands", "List"], selected, "#4C4C4C", fs=11.5, text="#fff")


def eq_group_tag(x, y, letter="A"):
    return (f'<div class="o new pill" style="left:{x}px;top:{y}px;width:15px;height:14px;background:#4C4C4C;color:#fff;'
            f'font-size:9.5px;font-weight:700;border-radius:4px">{letter}</div>')


def eq_rows(x0, y0, width, cols, note):
    """The band table: a header at y0, then a 22 px row per band, then + Add band and a note."""
    cells = ""
    for name, x, w, _ in cols:
        cells += f'<div class="o cap" style="left:{x0 + x}px;top:{y0}px;width:{w}px;font-size:10px;letter-spacing:.06em">{name.upper()}</div>'
    for row, band in enumerate(EQ_BANDS):
        colour, number = band[0], band[1]
        y = y0 + 16 + row * 22
        if number == 2:
            cells += (f'<div class="o" style="left:{x0 + 4}px;top:{y}px;width:{width - 8}px;height:21px;background:#3A3A3A;border-radius:3px"></div>'
                      f'<div class="o" style="left:{x0 + 4}px;top:{y}px;width:3px;height:21px;background:{colour};border-radius:2px"></div>')
        for name, x, w, field in cols:
            left = x0 + x
            if field is None:
                cells += (f'<div class="o pill" style="left:{left + 2}px;top:{y + 3}px;width:15px;height:15px;border-radius:8px;background:{colour};'
                          f'color:#fff;font-size:10px;font-weight:700">{number}</div>')
            elif field == "on":
                cells += f'<div class="o" style="left:{left + 4}px;top:{y + 2}px;font-size:13px;color:{colour}">&#x23FB;</div>'
            elif field == "solo":
                cells += f'<div class="o btn" style="left:{left}px;top:{y + 1}px;width:22px;height:19px;font-size:11px;background:#262626;color:#8D8D8D">S</div>'
            elif field == "x":
                cells += f'<div class="o" style="left:{left + 6}px;top:{y + 1}px;font-size:14px;color:#8D8D8D">&times;</div>'
            elif field in (2, 8) and w < 60:
                # Too narrow for a menu box: the text alone, which opens the menu when clicked.
                cells += f'<div class="o val" style="left:{left}px;top:{y + 3}px;width:{w}px;font-size:12.5px;color:#fff">{band[field]}</div>'
            elif field in (2, 8):
                cells += (f'<div class="o dd" style="left:{left}px;top:{y + 1}px;width:{w - 4}px;height:19px;font-size:12px;'
                          f'background:#262626;padding:0 5px">{band[field]}</div>')
            else:
                text = band[field]
                tint = "#8D8D8D" if text == "&mdash;" else "#fff" if field == 6 else colour
                cells += f'<div class="o val" style="left:{left}px;top:{y + 3}px;width:{w}px;font-size:12.5px;color:{tint}">{text}</div>'
    y = y0 + 16 + len(EQ_BANDS) * 22
    cells += f'<div class="o" style="left:{x0 + 10}px;top:{y + 4}px;font-size:12px;color:#8D8D8D">+ Add band</div>'
    cells += caption(x0 + 100, y + 5, note)
    return cells


def eq_list():
    cells = cover(0, 417, 734, 178, "#303030") + eq_switch(1)
    cells += '<div class="o" style="left:0;top:442px;width:734px;height:1px;background:#262626"></div>'
    cells += eq_rows(0, 446, 734, EQ_COLS, "Six rows show; with more bands the list scrolls (up to 24).")
    cells += outline(8, 417, 722, 176)
    cells += eq_group_tag(273, 290) + eq_group_tag(412, 148)  # group A's handles carry its letter
    return cells


def eq_bands_view():
    cells = cover(14, 419, 120, 20, "#303030") + eq_switch(0) + outline(14, 419, 112, 20)
    cells += eq_group_tag(374, 420) + eq_group_tag(417, 420)  # group A's tabs
    cells += eq_group_tag(273, 290) + eq_group_tag(412, 148)  # and handles
    return cells


EQ_B_GROW = 300   # B: a list column right of the graph
EQ_C_GROW = 150   # C: a list under the band editor


def eq_wide():
    """B: 300 px wider. The header's gap after Compare and the graph's right edge are stretched; the
    stretched graph strip is covered by the list, so the graph keeps its width."""
    return grow("eq", "eq-wide", EQ_B_GROW, [(0, 19, 400), (19, 73, 538), (73, 645, 733)])


def eq_tall():
    """C: 150 px taller, rows repeated under the band editor; the fader's track lengthens."""
    return grow("eq", "eq-tall", EQ_C_GROW, [(0, 734, 594), (734, 802, 560)], axis="y")


def eq_list_b():
    x0 = 734
    cells = cover(x0, 73, EQ_B_GROW, 572, "#262626")
    cells += f'<div class="o" style="left:{x0}px;top:73px;width:1px;height:572px;background:#141414"></div>'
    cells += group_title(x0 + 12, 84, "BANDS")
    cells += eq_rows(x0, 104, EQ_B_GROW, EQ_COLS_NARROW, "")
    cells += caption(x0 + 12, 230, "With more bands the column fills: 23 rows show, then it scrolls. Type and Group open their menus when clicked. Channel, Dyn and Solo stay in the band editor.", EQ_B_GROW - 24)
    cells += outline(x0 + 4, 76, EQ_B_GROW - 8, 566)
    cells += eq_group_tag(273, 290) + eq_group_tag(412, 148) + eq_group_tag(374, 420) + eq_group_tag(417, 420)
    return cells


def eq_list_c():
    y0 = 595
    cells = cover(0, y0, 734, EQ_C_GROW, "#2A2A2A")
    cells += f'<div class="o" style="left:0;top:{y0}px;width:734px;height:1px;background:#141414"></div>'
    cells += eq_rows(0, y0 + 6, 734, EQ_COLS, "Six rows show; with more bands the list scrolls (up to 24).")
    cells += outline(6, y0 + 2, 722, EQ_C_GROW - 6)
    cells += eq_group_tag(273, 290) + eq_group_tag(412, 148) + eq_group_tag(374, 420) + eq_group_tag(417, 420)
    return cells


@plugin("eq_list", "66__eq_band_list_mockup.png")
def eq_band_list():
    W, H = 802, 645
    WB, HB = eq_wide()
    WC, HC = eq_tall()
    groups = ("<b>Groups</b> (A to D, or none) link bands: moving one moves the rest of its group the same way, Freq by the same ratio, Gain by the same dB and Q by the same ratio, "
              "from the handle, the knobs or the list. Hold Alt to move one band on its own. Each grouped handle and tab carries its letter in a small grey tag. "
              "Group is a new parameter per band, appended (24), default none, so saved projects load and render as before, to the bit. "
              "The handle's right-click menu gets a <b>Group</b> submenu too. Solo is the existing temporary band solo, never saved (&sect;0.3).")
    return w1.page("Fancy EQ: Phase 4 mockup, band list and groups (item 46, layout approval)",
                   lead("docs/eq.md, Phase 4 Wave 2 item 46 (&#9650;, a visible layout change), for your approval before the design note: three ways, A, B and C", "no decisions yet"), [
        ("1 · A (recommended): a Bands | List switch; List shows every band in a table where the band editor is (802 x 645, as today)",
         "The window keeps its size and nothing on it moves. A small <b>Bands | List</b> switch goes at the left end of the tab row, which is empty today. "
         "<b>Bands</b> is today's editor, unchanged. <b>List</b> puts a table in the same place: one row per band, with its colour and number, power, Type, Freq, Gain, Q, "
         "Channel, Dyn (its dynamic range, or &mdash;), <b>Group</b>, a temporary <b>Solo</b> and delete. Each value is the shared control: drag it, or click it to type, as on the knobs. "
         "Clicking a row selects that band on the graph; double-clicking it opens it in Bands.",
         shot("eq", W, H, eq_list(), groups)),
        ("2 · A, the Bands view: the switch and the group tags",
         "Today's editor with the switch at the left of the tab row; bands 2 and 3 are in group A, so their tabs and handles carry an A.",
         shot("eq", W, H, eq_bands_view(),
              "Compact mode hides the band strip today; there the switch hides with it, and groups still work from the handles.")),
        (f"3 · B: the window grows {EQ_B_GROW} px wider ({WB} x {HB}) and the list is a column right of the graph, always visible",
         "The graph, the band editor and the fader keep their sizes; the header's right-hand buttons move right with the window's edge. "
         "Both the editor and the list show at once, so there's no switch. In 300 px the list fits the number, power, Type, Freq, Gain, Q and Group; "
         "Channel, Dyn and Solo stay in the band editor only. Groups work as in A.",
         shot("eq-wide", WB, HB, eq_list_b(),
              f"The EQ's minimum width becomes {WB} px (724 today). The column shows 23 bands before it scrolls.")),
        (f"4 · C: the window grows {EQ_C_GROW} px taller ({WC} x {HC}) and the list sits under the band editor, always visible",
         "Everything above stays as it is; the checkbox row moves down under the list and the Gain fader grows with the window. "
         "The list has every column, as A's; no switch. Groups work as in A.",
         shot("eq-tall", WC, HC, eq_list_c(),
              f"The EQ's minimum height becomes {HC} px. With a host's own title bar, track list and transport around it, that's more than many laptop screens show "
              "at 100 %, so it would often need the 'Prm' menu's smaller UI size.")),
    ])


# ---------------------------------------------------------------------------------------------
PAGE_WIDTHS = {}
PAGE_HEIGHTS = {"eq_list": 4400}  # pages taller than the default render window (1x px)


def render(key):
    filename, fn = PLUGINS[key]
    html = w1.SRC / f"w2b-{key.replace('_', '-')}.html"
    html.write_text(fn(), encoding="utf-8")
    png = w1.OUT / filename
    w1.OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run([w1.EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--force-device-scale-factor=2",
                    f"--window-size={PAGE_WIDTHS.get(key, 1548)},{PAGE_HEIGHTS.get(key, 2600)}", f"--screenshot={png}", html.as_uri()], capture_output=True)
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
