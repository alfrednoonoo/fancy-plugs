"""v1.7.2 mockups, drawn as w2b_mockups.py's, on CI run 45602564567's screenshots (commit 9196802, v1.7.1).

    py -3 design/phase4-mockup-src/w3_mockups.py [key ...]
"""
import sys

import w2b_mockups as m
from w2_mockups import caption
from w2b_mockups import cover, outline, shot, w1

m.PLUGINS.clear()
YELLOW = "#F1C315"
STRIP = 24  # the graph and the window grow this much in A


def freeze(x, y, on=False, bg="#262626"):
    return (f'<div class="o new" style="left:{x}px;top:{y}px;width:70px;height:22px;border-radius:3px;display:flex;align-items:center;'
            f'justify-content:center;font-size:14px;background:{YELLOW if on else bg};color:{"#141414" if on else "#fff"}">Freeze</div>')


def handle(x, y):
    """Tap 1's handle (selected), centre x, y, on its bar."""
    return (f'<div class="o" style="left:{x - 8.5}px;top:{y - 8.5}px;width:17px;height:17px;border-radius:50%;background:{YELLOW};color:#141414;'
            f'font-size:11px;font-weight:600;display:flex;align-items:center;justify-content:center">1</div>')


def text(x, y, w, label, align="left"):
    return f'<div class="o" style="left:{x}px;top:{y}px;width:{w}px;height:16px;line-height:16px;font-size:12px;color:#8D8D8D;text-align:{align}">{label}</div>'


def line(x, y, h):
    return f'<div class="o" style="left:{x}px;top:{y}px;width:1px;height:{h}px;background:#4C4C4C"></div>'


@m.plugin("delay-freeze", "67__delay_freeze_mockup.png")
def delay_freeze():
    W, H = m.grow("delay-v171", "delay-v171-tall", STRIP, [(0, 716, 88), (716, 820, 300)], axis="y")

    def strip(on):
        y = STRIP
        return (cover(46, 91 + y, 32, 17) + cover(106, 92 + y, 73, 24) + cover(490, 91 + y, 182, 17) + line(512, 91 + y, 18)
                + handle(160.5, 117.5 + y)
                + text(50, 91, 40, "dry") + freeze(107, 93, on) + text(479, 91, 190, "faded bars = feedback repeats", "right"))

    row = cover(106, 92, 73, 24) + handle(160.5, 117.5) + freeze(543, 243)
    return w1.page("Fancy Delay: v1.7.2 mockup (Freeze clear of the tap handles)",
                   "Freeze sits on the tap graph at x 107 to 177, so it covers the top of any loud tap's handle near 375 ms. "
                   "Changed things in dashed yellow, on the plugin's real screenshot (1x coordinates).", [
        (f"1 · A (recommended): the graph gets its own label strip, and the window grows {STRIP} px taller ({W} x {H})",
         "Freeze, dry and the caption stay exactly where they are, in a strip across the top of the graph that no handle enters. "
         "The handles keep their whole travel, because the graph is taller. Everything under the graph moves down; nothing shrinks.",
         shot("delay-v171-tall", W, H, strip(False),
              f"The minimum height grows by the same {STRIP} px. A handle can no longer cover \"dry\" or the caption either. No parameter changes.")),
        ("2 · A with Freeze on", "The same strip, with Freeze lit.", shot("delay-v171-tall", W, H, strip(True))),
        ("3 · B: Freeze moves to the tap row, and the window stays 820 x 448",
         "Freeze goes between Tone | Motion and S, and the graph keeps its size.",
         shot("delay-v171", 820, 448, row,
              "The tap row's other controls belong to the selected tap, while Freeze holds every tap, so it could read as freezing only this one. "
              "A loud tap's handle can still cover \"dry\" and the caption.")),
    ])


m.PAGE_HEIGHTS["delay-freeze"] = 2400

# ---------------------------------------------------------------------------------------------
# EQ's band list in a large window. The screenshot is eq@max.png of run 45514445844 (commit 81e8f1e): 1800 x 1320,
# graph 1732 x 1019 at y 73, strip 178 px at y 1092, checkboxes at y 1270.
EQ_W, EQ_H, EQ_CONTENT, EQ_STRIP_BOTTOM = 1800, 1320, 1732, 1270
EQ_TWELVE = [("#E74D3D", 1, "Low shelf", "67.0 Hz", "-6.0 dB", "0.71", "ST", "&mdash;", "&mdash;"),
             ("#9A59B5", 2, "Band", "487.7 Hz", "-5.1 dB", "2.0", "ST", "&mdash;", "A"),
             ("#3598DB", 3, "Band", "1.60 kHz", "6.5 dB", "0.80", "ST", "&mdash;", "A"),
             ("#1BBC9C", 4, "High shelf", "17.00 kHz", "-13.0 dB", "0.71", "ST", "&mdash;", "&mdash;")]
EQ_EXTRA = [("100.0 Hz", 245), ("200.0 Hz", 400), ("300.0 Hz", 502), ("1.00 kHz", 825), ("3.00 kHz", 1134), ("5.00 kHz", 1285), ("10.00 kHz", 1479), ("50.0 Hz", 117)]
EQ_COLOURS = ["#2DCC70", "#E74D3D", "#9A59B5", "#3598DB", "#1BBC9C", "#2DCC70", "#E74D3D", "#9A59B5"]
for _i, (_hz, _x) in enumerate(EQ_EXTRA):
    EQ_TWELVE.append((EQ_COLOURS[_i], 5 + _i, "Band", _hz, "0.0 dB", "1.0", "ST", "&mdash;", "&mdash;"))


def eq_tall_list(strip):
    """The max screenshot with the graph squeezed so the strip is taller."""
    from PIL import Image
    folder = m.pathlib.Path(__file__).resolve().parent / "img"
    im = Image.open(folder / "eq-max-w2b.png").convert("RGB")
    top, h = 73, EQ_STRIP_BOTTOM - strip - 73
    graph = im.crop((0, top, EQ_CONTENT, 1092)).resize((EQ_CONTENT, h), Image.LANCZOS)
    im.paste(graph, (0, top))
    im.paste((48, 48, 48), (0, top + h, EQ_CONTENT, EQ_STRIP_BOTTOM))
    im.save(folder / "eq-max-list-w2b.png")
    return top + h, h / 1019


def eq_large(span):
    """span: how wide the columns spread. 734 is today's width."""
    strip = 44 + 13 * 22
    y0, squeeze = eq_tall_list(strip)
    x0, f = (EQ_CONTENT - span) / 2, span / 734
    cols = [(n, round(x * f), w, field) for n, x, w, field in m.EQ_COLS]
    saved, m.EQ_BANDS = m.EQ_BANDS, EQ_TWELVE
    cells = m.seg(14, y0 + 2, 112, 20, ["Bands", "List"], 1, "#4C4C4C", fs=11.5, new=False, text="#fff")
    cells += f'<div class="o" style="left:0;top:{y0 + 25}px;width:{EQ_CONTENT}px;height:1px;background:#262626"></div>'
    cells += m.eq_rows(x0, y0 + 28, span, cols, "")
    m.EQ_BANDS = saved
    zero = 73 + (582 - 73) * squeeze
    for i, (_hz, x) in enumerate(EQ_EXTRA):
        cells += (f'<div class="o" style="left:{x - 7.5}px;top:{zero - 7.5}px;width:15px;height:15px;border-radius:50%;background:{EQ_COLOURS[i]};color:#141414;'
                  f'font-size:9.5px;font-weight:700;display:flex;align-items:center;justify-content:center">{5 + i}</div>')
    cells += outline(4, y0, EQ_CONTENT - 8, strip - 2)
    return shot("eq-max-list", EQ_W, EQ_H, cells)


@m.plugin("eq-list-large", "68__eq_list_large_mockup.png")
def eq_list_large():
    return w1.page("Fancy EQ: v1.7.2 mockup (the band list in a large window)",
                   "Drawn on the real screenshot of the largest window (1800 x 1300 and its title strip), with twelve bands. The graph is squeezed to its new height "
                   "to show the space it keeps; in the plugin it redraws, so its text and handles stay round. Changed area in dashed yellow.", [
        ("1 · A (recommended): the list grows with its rows, and the columns spread up to 1100 px, centred",
         "With List on, the list is as tall as its rows need, taking height from the graph, which never gets shorter than it is at the default size (344 px). "
         "Six rows or fewer look as today; here twelve bands show, and from about 1040 px tall all 24 do. "
         "The columns spread from 734 px up to 1100 px wide and stay centred, so a row stays easy to read across. No control changes size.",
         eq_large(1100)),
        ("2 · B: as A, with the columns spread over the whole width",
         "The list lines up with both edges of the graph, but at this size a row's number and its delete cross are about 1600 px apart.",
         eq_large(EQ_CONTENT)),
        ("3 · The default window (802 x 645): unchanged in both",
         "The graph is already at its smallest, so the list keeps its six rows and scrolls, and 734 px is the columns' normal width.",
         shot("eq-list-v171", 802, 645, "", "The strip is 178 px in Bands at every size, so switching between Bands and List moves the graph's bottom edge only when the list "
              "is taller. Adding a seventh band or more makes the list 22 px taller while there's room. Compact mode still has no list. No parameter changes.")),
    ])


m.PAGE_WIDTHS["eq-list-large"] = 1880
m.PAGE_HEIGHTS["eq-list-large"] = 3800

# ---------------------------------------------------------------------------------------------
# Delay's Tone page with Pitch and Reverse (docs/delay.md, Phase 4 Wave 3, decision 4 A). Screenshots: delay@2x.png and
# delay-motion@2x.png of run 45649213148 (commit a74c0a1), 820 x 472. The Tone knobs' faces are centred at x 246 + 64 i
# (1x); the Motion page's five at 383, 441, 499, 555 and 618. The window grows DELAY_GROW px wider, so Pitch gets a
# column at today's 64 px spacing and the Motion knobs spread to the same spacing (user, 26 November 2026: "don't clamp").
TONE_X0, TONE_STEP, DELAY_GROW = 246, 64, 64
MOTION_X = [383, 441, 499, 555, 618]
# Uniform columns in each band of the 820 px shot: title, header, graph, tap row, card, checkboxes.
DELAY_BANDS = [(0, 20, 600), (20, 73, 555), (73, 262, [405, 445, 480]), (262, 292, 600), (292, 442, 600), (442, 472, 610)]


def delay_wide():
    W, H = m.grow("delay-v172", "delay-v172-wide", DELAY_GROW, DELAY_BANDS)
    m.grow("delay-v172-motion", "delay-v172-motion-wide", DELAY_GROW, DELAY_BANDS)
    from PIL import Image
    folder = m.pathlib.Path(__file__).resolve().parent / "img"
    src = Image.open(folder / "delay-v172-motion-w2b.png").convert("RGB")
    im = Image.open(folder / "delay-v172-motion-wide-w2b.png").convert("RGB")
    top, bottom = 600, 870
    # Each column runs to the midpoints between knobs; Rate's keeps the whole wave switch (to x 414).
    edges = [352, 415, 470, 527, 586, 652]
    im.paste((38, 38, 38), (2 * edges[0], top, 2 * (655 + DELAY_GROW) - 4, bottom))
    for i, x in enumerate(MOTION_X):
        col = src.crop((2 * edges[i], top, 2 * edges[i + 1], bottom))
        im.paste(col, (2 * (MOTION_X[0] + TONE_STEP * i - (x - edges[i])), top))
    im.save(folder / "delay-v172-motion-spread-w2b.png")
    return W, H


def tab_badge(x, y, glyph, colour=YELLOW):
    """An 11 px ringed badge, as ColourTabs draws a Character letter."""
    return (f'<div class="o" style="left:{x}px;top:{y}px;width:11px;height:11px;border-radius:50%;box-sizing:border-box;background:#141414;'
            f'border:1px solid {colour};color:{colour};font-size:6.5px;font-weight:600;display:flex;align-items:center;justify-content:center">{glyph}</div>')


def direction(selected):
    return m.seg(TONE_X0 - 25, 408, 50, 17, ["&#9654;&#xFE0E;", "&#9664;&#xFE0E;"], selected, YELLOW, fs=9) + outline(TONE_X0 - 27, 406, 54, 21)


def pitch(value, v):
    x = TONE_X0 + 6 * TONE_STEP
    return m.knob(x, 305, "Pitch", value, YELLOW, v, size=50, fs=13, bi=True, width=60) + outline(x - 31, 305, 62, 98)


@m.plugin("delay-w3", "69__delay_w3_mockup.png")
def delay_w3():
    W, H = delay_wide()
    rev_tab = tab_badge(24, 278, "&#9664;") + outline(11, 265, 26, 28)
    return w1.page("Fancy Delay: v1.7.2 mockup (Reverse and Pitch on the Tone page)",
                   m.lead("docs/delay.md, Phase 4 Wave 3: reverse and pitch-shifted taps", "Decisions 1 to 4 A (user, 26 November 2026)"), [
        (f"1 · The Tone page, tap 1 as it loads: forward, 0 st. The window grows {DELAY_GROW} px wider, to {W} x {H}",
         "<b>Pitch</b> (&minus;12 to +12 st, 0 by default, centred) is a seventh knob after Pan, at the six knobs' own 64 px spacing: none of them moves. "
         f"To give it its column, <b>the window grows {DELAY_GROW} px wider, from 820 to {W} px</b> (its minimum too): the tap graph and the knob card widen, "
         "and Volume, the meters and the faders move across, as in Wave 2. "
         "<b>Reverse</b> is a two-way direction switch, <b>&#9654; | &#9664;</b>, under Feedback, where the wave switch sits under Rate on the Motion page.",
         shot("delay-v172-wide", W, H, direction(0) + pitch("0.00 st", 0.5),
              "Forward and 0 st are today's sound, to the bit. Both are per tap, like Feedback and Pan. The graph keeps 0 to 2000 ms across its new width.")),
        ("2 · Tap 1 reversed, +7 st",
         "With &#9664; chosen the tap plays each window of one tap time backwards, so a note swells in and ends on its attack. "
         "Its tab gets a small <b>&#9664;</b> in its bottom-right corner, so a reversed tap shows on every page.",
         shot("delay-v172-wide", W, H, direction(1) + pitch("+7.00 st", 19 / 24) + rev_tab,
              "Pitch shifts each repeat again, inside the feedback loop: +12 st gives rising octaves, &minus;12 falling ones, &plusmn;0.1 st a slow drift. "
              "Typed values take 7, -12 or -0.1. A tooltip on the switch reads \"Forward\" or \"Reverse: plays each window of one tap time backwards\".")),
        ("3 · The Motion page of the same tap, in the wider window",
         "Rate, Depth, Random, Stereo and Diffusion spread to the same 64 px spacing as the Tone page (today 56 to 63 px), using the width the window gains; "
         "the wave switch stays under Rate. The Character letter keeps the tab's top-right corner and the &#9664; takes the bottom-right one, so a Tape tap that's reversed shows both.",
         shot("delay-v172-motion-spread", W, H, rev_tab + outline(MOTION_X[0] - 34, 305, 4 * TONE_STEP + 68, 128),
              "Only the positions change; no knob, label or switch changes size.")),
    ])


m.PAGE_HEIGHTS["delay-w3"] = 2000

# ---------------------------------------------------------------------------------------------
# Verb's Freeze and Shimmer (docs/verb.md, Phase 4 Wave 3, decisions A). Screenshots: verb@2x.png and verb-edit@2x.png of
# run 45799546744 (commit db30e13), 866 x 697. The IR display is x 0 to 734, y 73 to 273; its info text ends at x 718,
# y 100. The SETUP drawer is the rack's bounds (y 273 to 625); Ducking's knob is centred at x 378, the Gate row's
# knobs at 466, 546 and 626, and the Gate row ends at y 571. The chain strip is y 625 to 697.
VERB_W, VERB_H, VERB_GROW = 866, 697, 96
PURPLE, TEAL = "#9A59B5", "#00A985"


def verb_freeze(on):
    button = (f'<div class="o new" style="left:648px;top:108px;width:70px;height:22px;border-radius:3px;display:flex;align-items:center;'
              f'justify-content:center;font-size:14px;background:{PURPLE if on else "#303030"};color:#fff;font-weight:{600 if on else 400}">Freeze</div>')
    return (m.dim(0, 73, 734, 200, "154,89,181", 0.10) if on else "") + button


def shimmer_row(top, interval="Octave up", value="0 %", v=0.0):
    """The Shimmer row under the Gate row: the knob in Ducking's column, Interval and its caption beside it."""
    return (m.knob(378, top, "Shimmer", value, TEAL, v, size=42, fs=13)
            + f'<div class="o" style="left:428px;top:{top + 24}px;font-size:13px;color:#fff">Interval</div>'
            + f'<div class="o" style="left:490px;top:{top + 20}px">{m.dd(interval, 150)}</div>'
            + caption(428, top + 52, "Feeds the reverb back into itself an interval higher, so the tail rises in octaves. "
                                     "It always dies away; Freeze holds it.", 290)
            + outline(334, top - 4, 400, 108))


@m.plugin("verb-w3", "72__verb_w3_mockup.png")
def verb_w3():
    W2, H2 = m.grow("verb-edit-v172", "verb-edit-v172-tall", VERB_GROW, [(0, 734, 622), (734, 866, 630)], axis="y")
    over_strip = cover(0, 625, 734, 72, "#262626") + shimmer_row(584, "Octave up", "50 %", 0.5)
    return w1.page("Fancy Verb: v1.7.2 mockup (Freeze and Shimmer)",
                   m.lead("docs/verb.md, Phase 4 Wave 3: Freeze and shimmer", "Decisions 1 to 4 A (user, 26 November 2026)"), [
        ("1 · The main window: Freeze at the top right of the IR display",
         "<b>Freeze</b> is a toggle button under the display's info line, 70 x 22 like Delay's. "
         "It holds what the reverb is playing for as long as it's on; new input no longer enters the reverb, and the dry signal is unaffected. Automatable.",
         shot("verb-v172", VERB_W, VERB_H, verb_freeze(False),
              "Off is today's sound, to the bit. The tooltip reads \"Freeze: holds the reverb as it is, for as long as it's on. "
              "Turned off, it dies away at the IR's own decay.\" The window keeps its size.")),
        ("2 · Freeze on",
         "The button lights in Verb's purple and the display takes a faint purple tint, so a frozen reverb shows even when the button is out of sight of the mouse.",
         shot("verb-v172", VERB_W, VERB_H, verb_freeze(True),
              "The waveform, trim handles and the right-click menu work as usual while frozen; a change to the IR is heard once Freeze is off.")),
        ("3 · A (recommended): the SETUP drawer reaches over the chain strip, and Shimmer gets its own row under the Gate",
         "The drawer covers the rack today; with SETUP open the chain strip has nothing to show, so the drawer takes its 72 px too and the window keeps 866 x 697. "
         "<b>Shimmer</b> (0 to 100 %, 0 by default) sits in Ducking's column, with <b>Interval</b> (Octave up, Fifth up, Fourth up, Octave and fifth up, "
         "Two octaves up, Octave down) beside it.",
         shot("verb-edit-v172", VERB_W, VERB_H, over_strip,
              "At 0 % the output is today's, to the bit, and Interval does nothing. No existing control moves or changes size. "
              "Closing SETUP (Done, or the SETUP button) brings the strip back.")),
        (f"4 · B: the window grows {VERB_GROW} px taller ({W2} x {H2}), and the strip stays",
         f"The drawer and the module cards grow {VERB_GROW} px; the cards' knobs stay where they are, with the new height empty under them. "
         "The Wet and Dry faders keep their length.",
         shot("verb-edit-v172-tall", W2, H2, shimmer_row(616),
              "Costs height on every screen for one row that only SETUP shows.")),
    ])


m.PAGE_HEIGHTS["verb-w3"] = 3400

# ---------------------------------------------------------------------------------------------
# EQ's SPECTRAL chip (docs/eq.md, Phase 4 Wave 3, decision 4 A). Screenshots: eq-dynamic@2x.png,
# eq-dynamic-compact@2x.png and eq-list@2x.png of run 45799546744 (commit db30e13), 802 x 645. In the band
# editor ST is at (22, 523) and DYN (66, 523, 50 x 24); under them the column is empty down to the strip's
# end (y 594). In Compact mode they sit at (19, 513) and (63, 513), and the panel ends at y 570.
EQ_PURPLE = "#9A59B5"


def spectral_chip(x, y, state):
    """state: off, on or disabled (Linear phase)."""
    bg, fg, extra = {"off": ("#262626", "#FFFFFF", ""), "on": (EQ_PURPLE, "#1E1E1E", ";font-weight:500"),
                     "disabled": ("#262626", "#5A5A5A", "")}[state]
    return (f'<div class="o new" style="left:{x}px;top:{y}px;width:94px;height:24px;border-radius:3px;display:flex;align-items:center;'
            f'justify-content:center;font-size:13px;letter-spacing:.04em;background:{bg};color:{fg}{extra}">SPECTRAL</div>')


def eq_tip(x, y, text):
    return (f'<div class="o" style="left:{x}px;top:{y}px;padding:4px 8px;border-radius:3px;background:#141414;border:1px solid #4C4C4C;'
            f'font-size:12px;color:#fff;white-space:nowrap">{text}</div>')


@m.plugin("eq-w3", "71__eq_w3_mockup.png")
def eq_w3():
    W, H = 802, 645
    s_badge = tab_badge(532, 511, "S", "#3598DB") + outline(474, 504, 76, 26)
    return w1.page("Fancy EQ: v1.7.2 mockup (the SPECTRAL chip)",
                   m.lead("docs/eq.md, Phase 4 Wave 3: spectral dynamics", "Decisions 1 to 5 A (user, 26 November 2026)"), [
        ("1 · The band editor, band 2 dynamic: SPECTRAL off, as it loads",
         "<b>SPECTRAL</b> (per band, off by default) sits under ST and DYN, spanning both (94 x 24, the DYN chip's height), in the column's empty space: "
         "no control moves or changes size, and the window stays 802 x 645. It shows only while DYN is on, like Threshold to Release.",
         shot("eq-v172-dyn", W, H, spectral_chip(22, 553, "off"),
              "Off is today's dynamic band, to the bit, with no latency. Tooltip: \"Spectral: works per frequency inside the band, so a resonance is cut "
              "on its own and the rest of the band is left alone. Adds 21 ms of latency.\"")),
        ("2 · SPECTRAL on",
         "The chip lights in the band's colour. The band's Threshold, Range, Attack, Release, Above | Below, Sidechain and Reach act per frequency. "
         "On the graph the white total curve shows a narrow dip at whatever is being cut, instead of the whole bell moving; \"now\" reads the deepest point.",
         shot("eq-v172-dyn", W, H, spectral_chip(22, 553, "on"),
              "The first spectral band adds 1024 samples of latency (21 ms at 48 kHz), reported to the host; the last one off takes it away. "
              "Threshold is the level at one frequency, so set it by the resonance's level.")),
        ("3 · Linear phase: the chip is disabled",
         "Spectral needs Minimum phase, as DYN does; in Linear mode the chip greys out and spectral bands play their static settings.",
         shot("eq-v172-dyn", W, H, spectral_chip(22, 553, "disabled") + eq_tip(22, 584, "Spectral bands need Minimum phase"),
              "The tooltip shows on hover.")),
        ("4 · Compact mode", "The same chip under ST and DYN in the floating panel, at the same size, inside its bottom edge.",
         shot("eq-v172-dyn-compact", W, H, spectral_chip(19, 541, "on"))),
        ("5 · The band list: a spectral band's DYN cell",
         "Band 3 made spectral: its DYN cell keeps the reach and adds a ringed <b>S</b> in the band's colour.",
         shot("eq-v172-list", W, H, s_badge, "Dynamic bands that aren't spectral show their reach as today.")),
    ])


m.PAGE_HEIGHTS["eq-w3"] = 3600

# ---------------------------------------------------------------------------------------------
# Room's Shimmer (docs/room.md, Phase 4 Wave 3, decisions A). Screenshot: room-edit@2x.png of run 45826938789
# (commit 3848b23), 760 x 529. The EDIT drawer is x 0 to 650. GATE's title is at (24, 336), its knobs' labels at
# y 348 and values at y 404. DECAY SHAPE's title is at (398, 424); Low decay and Attack are 30 px knobs centred at
# x 448 and 538, labels at y 438. Under the Gate row, x 20 to 380 and y 418 to 529 are empty.
ROOM_W, ROOM_H, ROOM_GROW = 760, 529, 80
ROOM_PURPLE = "#9A59B5"
ROOM_NOTE = "Feeds the tail back an interval higher on each pass. It always dies away."
INTERVALS = ["Octave up", "Fifth up", "Fourth up", "Octave and fifth up", "Two octaves up", "Octave down"]


def room_shimmer_a(value="0 %", v=0.0):
    """A: the SHIMMER group in the drawer's empty bottom-left, level with DECAY SHAPE. Knob in Ducking's column."""
    return (m.group_title(24, 424, "SHIMMER")
            + m.knob(68, 438, "Shimmer", value, ROOM_PURPLE, v, size=30, fs=12, width=80)
            + '<div class="o" style="left:120px;top:446px;font-size:12px;color:#fff">Interval</div>'
            + '<div class="o dd" style="left:180px;top:442px;width:150px;height:24px;font-size:12px">Octave up</div>'
            + caption(120, 474, ROOM_NOTE, 250)
            + outline(20, 420, 364, 92))


def room_shimmer_b():
    """B: under DECAY SHAPE in the right-hand column of the grown drawer. Knob in Low decay's column."""
    return (m.group_title(398, 512, "SHIMMER")
            + m.knob(448, 526, "Shimmer", "0 %", ROOM_PURPLE, 0.0, size=30, fs=12, width=80)
            + '<div class="o" style="left:494px;top:526px;font-size:12px;color:#fff">Interval</div>'
            + '<div class="o dd" style="left:494px;top:544px;width:150px;height:24px;font-size:12px">Octave up</div>'
            + outline(394, 508, 254, 92))


@m.plugin("room-w3", "73__room_w3_mockup.png")
def room_w3():
    W2, H2 = m.grow("room-edit-v172", "room-edit-v172-tall", ROOM_GROW, [(0, 650, 516), (650, 760, 380)], axis="y")
    items = "".join(f'<div class="on">&#10003; {i}</div>' if i == "Octave up" else f"<div>{i}</div>" for i in INTERVALS)
    menu = f'<div class="o new menu" style="left:180px;top:300px;width:150px;font-size:12px">{items}</div>'
    return w1.page("Fancy Room: v1.7.2 mockup (Shimmer)",
                   m.lead("docs/room.md, Phase 4 Wave 3: shimmer", "Decisions 1 to 3 A (user, 26 November 2026)"), [
        ("1 · A (recommended): a SHIMMER group in the EDIT drawer's empty corner, under the Gate row; the window stays 760 x 529",
         "<b>Shimmer</b> (0 to 100 %, 0 by default) is a small knob like Low decay's, in Ducking's column, with <b>Interval</b> (Octave up, Fifth up, Fourth up, "
         "Octave and fifth up, Two octaves up, Octave down) beside it, the same list as Verb's. The group sits level with DECAY SHAPE, so the drawer's bottom row reads as two groups.",
         shot("room-edit-v172", ROOM_W, ROOM_H, room_shimmer_a(),
              "At 0 % the output is today's, to the bit, and Interval does nothing. No existing control moves or changes size; the main window doesn't change. "
              "The tooltip reads \"Shimmer: feeds the tail back into the room through a pitch shifter, so each pass comes back an interval higher. "
              "0 % is off; Freeze holds it.\"")),
        ("2 · A at 50 %, Interval open",
         "The six intervals, Octave up ticked. With no room below, the list opens upwards.",
         shot("room-edit-v172", ROOM_W, ROOM_H, room_shimmer_a("50 %", 0.5) + menu, "", crop=(0, 280, 400, 249))),
        (f"3 · B: beside the reverb EQ, under DECAY SHAPE; the window grows {ROOM_GROW} px taller ({W2} x {H2})",
         f"The note's literal place: the right-hand column, below DECAY SHAPE. It has no room there today, so the drawer and the window grow {ROOM_GROW} px "
         "(the minimum too); the meters and faders lengthen and the main page's cards get the height evenly, as in Wave 2.",
         shot("room-edit-v172-tall", W2, H2, room_shimmer_b(),
              "Costs height on the main page too, for one row that only EDIT shows, and leaves the bottom-left corner empty. "
              "The note goes in the tooltip only.")),
    ])


m.PAGE_HEIGHTS["room-w3"] = 2200

# ---------------------------------------------------------------------------------------------
# Limit's multiband limiting (docs/enhancements.md, section 10 item 6, ▲: layout first). Screenshot: limit@2x.png of
# run 45855736195 (commit 00a2d21), 760 x 465. Measured at 1x: header to y 72; the display card is x 119 to 629,
# y 117 to 276 (history x 119 to 479, loudness panel x 480 to 629); the LIMITER card is x 119 to 629, y 285 to 411,
# its knobs centred at x 183, 310, 437 and 559, labels at y 317, values at y 403; the left column (Threshold, GR)
# is x 0 to 118 and the right one (Out, LUFS) x 640 to 760; the footer's checkboxes are at y 444.
LIMIT_W, LIMIT_H = 760, 465
LIMIT_RED = "#E74D3D"
LIMIT_GROW = 172  # A: the BANDS card (164) and its gap
BAND_COLOURS = [LIMIT_RED, "#9A59B5", "#3598DB", "#1BBC9C"]


def limit_graph(x, y, w, h, splits, gr, scale_db=12.0, labels=True):
    """The crossover strip: bands shaded in their colours, each one's reduction falling from the top, the splits as
    handles. splits in Hz, gr in dB per band."""
    import math

    def fx(hz):
        return w * math.log10(hz / 20.0) / 3.0

    edges = [0.0] + [fx(s) for s in splits] + [w]
    parts = [f'<rect width="{w}" height="{h}" fill="#141414"/>']
    for i in range(len(edges) - 1):
        x0, x1, c = edges[i], edges[i + 1], BAND_COLOURS[i]
        drop = h * min(gr[i], scale_db) / scale_db
        parts.append(f'<rect x="{x0:.1f}" y="0" width="{x1 - x0:.1f}" height="{h}" fill="{c}" fill-opacity=".10"/>')
        parts.append(f'<rect x="{x0 + 1:.1f}" y="0" width="{x1 - x0 - 2:.1f}" height="{drop:.1f}" fill="{c}" fill-opacity=".75"/>')
        cx = (x0 + x1) / 2
        parts.append(f'<circle cx="{cx:.1f}" cy="{h - 13}" r="7.5" fill="#141414" stroke="{c}" stroke-width="1.5"/>'
                     f'<text x="{cx:.1f}" y="{h - 9.5}" fill="#fff" font-size="9.5" font-weight="600" text-anchor="middle">{i + 1}</text>')
    for s in splits:
        sx = fx(s)
        parts.append(f'<line x1="{sx:.1f}" y1="0" x2="{sx:.1f}" y2="{h}" stroke="#3598DB" stroke-width="1.5"/>'
                     f'<rect x="{sx - 3:.1f}" y="{h - 8}" width="6" height="8" rx="1.5" fill="#3598DB"/>')
    full = h + (13 if labels else 0)
    if labels:
        for hz, text in [(50, "50"), (100, "100"), (200, "200"), (500, "500"), (1000, "1k"), (2000, "2k"), (5000, "5k"), (10000, "10k")]:
            parts.append(f'<text x="{fx(hz):.1f}" y="{h + 11}" fill="#8D8D8D" font-size="9" text-anchor="middle">{text}</text>')
    return (f'<svg class="o" style="left:{x}px;top:{y}px" width="{w}" height="{full}" viewBox="0 0 {w} {full}">'
            + "".join(parts) + "</svg>")


def band_chip(x, y, i, on=False, size=18):
    c = BAND_COLOURS[i]
    style = f"background:{c};color:#fff" if on else f"border:1.5px solid {c};color:#fff;box-sizing:border-box"
    return (f'<div class="o" style="left:{x}px;top:{y}px;width:{size}px;height:{size}px;border-radius:3px;{style};font-size:11px;'
            f'font-weight:600;display:flex;align-items:center;justify-content:center">{i + 1}</div>')


def limit_bands_card(top, bands, releases, grs, splits):
    """A: the BANDS card, x 119 to 629, 164 tall, between the display and the LIMITER card."""
    x0, w = 119, 511
    html = f'<div class="o" style="left:{x0}px;top:{top}px;width:{w}px;height:164px;background:#262626;border-radius:4px"></div>'
    html += ('<div class="o" style="left:131px;top:%dpx;font-size:12px;font-weight:600;letter-spacing:.04em;color:#8D8D8D">BANDS</div>' % (top + 11))
    html += m.seg(186, top + 8, 116, 20, ["1", "2", "3", "4"], bands - 1, LIMIT_RED, text="#fff")
    html += '<div class="o" style="left:410px;top:%dpx;font-size:12px;color:#8D8D8D">Crossover</div>' % (top + 11)
    html += '<div class="o dd" style="left:474px;top:%dpx;width:143px;height:22px;font-size:12px">LR 24 dB &middot; Min phase</div>' % (top + 7)
    if bands == 1:
        html += limit_graph(131, top + 34, 486, 44, [], [grs[0]])
        html += caption(131, top + 100, "One band is today's limiter, to the bit: the band row is empty and the LIMITER card does everything.", 486)
        return html + outline(x0 - 2, top - 2, w + 4, 168)
    html += limit_graph(131, top + 34, 486, 44, splits, grs)
    cell = 486 / 4
    for i in range(4):
        cx = 131 + cell * i
        if i >= bands:
            continue
        html += band_chip(cx + 4, top + 118, i)
        html += m.knob(cx + 58, top + 94, "Release", releases[i], BAND_COLOURS[i], min(1.0, 0.15 + 0.2 * i), size=28, fs=12, width=70)
        html += (f'<div class="o" style="left:{cx + 88:.0f}px;top:{top + 112}px;width:30px;font-size:10px;color:#8D8D8D;text-align:center">GR</div>'
                 f'<div class="o" style="left:{cx + 84:.0f}px;top:{top + 126}px;width:38px;font-size:12px;color:#fff;text-align:center">-{grs[i]:.1f}</div>')
    return html + outline(x0 - 2, top - 2, w + 4, 168)


def limit_b():
    """B: the window stays 760 x 465. The display card's history gets a Bands view; the LIMITER card gets band chips,
    and its Release follows the selected band."""
    html = cover(119, 117, 361, 160, "#141414")
    html += m.seg(125, 122, 112, 20, ["History", "Bands"], 1, "#424242", fs=11, text="#fff")
    html += m.seg(330, 122, 140, 20, ["1", "2", "3", "4"], 2, LIMIT_RED, fs=11, text="#fff")
    html += '<div class="o" style="left:286px;top:125px;font-size:11px;color:#8D8D8D">Bands</div>'
    html += limit_graph(125, 148, 349, 112, [150, 3000], [4.0, 1.5, 2.5])
    html += outline(117, 115, 365, 164)
    # The LIMITER card's title row: band chips; Release shows band 2's.
    html += band_chip(204, 292, 0) + band_chip(226, 292, 1, on=True) + band_chip(248, 292, 2)
    html += outline(200, 289, 72, 24)
    html += cover(262, 312, 96, 100)
    html += m.knob(310, 317, "Release", "45 ms", BAND_COLOURS[1], 0.25, size=46, fs=13, width=96)
    html += outline(262, 314, 96, 98)
    return html


@m.plugin("limit-w3", "74__limit_w3_layout_mockup.png")
def limit_w3():
    WA, HA = m.grow("limit-v172", "limit-v172-tall", LIMIT_GROW, [(0, 119, 260), (119, 640, 281), (640, 760, 266)], axis="y")
    splits3, grs3 = [150, 3000], [4.0, 1.5, 2.5]
    return w1.page("Fancy Limit: v1.7.2 layout mockup (multiband limiting, ▲)",
                   "docs/enhancements.md, section 10 item 6: 2 to 4 bands with a release each. It changes the visible layout, so the layout comes before "
                   "the design note. Changed things in dashed yellow, on the plugin's real screenshot (1x coordinates). The parameters drawn here "
                   "(band count, crossovers, a release per band) are proposals the note will set out as decisions.", [
        (f"1 · A (recommended): a BANDS card between the display and the LIMITER card; the window grows {LIMIT_GROW} px taller ({WA} x {HA})",
         "<b>Bands</b> 1 to 4 (1 by default, today's limiter), the <b>Crossover</b> type (MultiComp's list), and a strip with each band's reduction "
         "falling from the top in its colour. Drag a blue handle to move a crossover, or click its frequency to type one. Under the strip, each band "
         "has its own small <b>Release</b> knob and GR read-out. The bands are limited first and summed; the LIMITER card's Ceiling, Release, "
         "Lookahead and True peak then hold the final ceiling across the whole signal, as now.",
         shot("limit-v172-tall", WA, HA, limit_bands_card(285, 3, ["40 ms", "80 ms", "120 ms", ""], grs3, splits3),
              f"Nothing existing moves sideways or shrinks: the LIMITER card and footer move down {LIMIT_GROW} px, the Threshold, GR, Out and LUFS "
              f"meters lengthen with the window (their scales are respaced evenly in the build; the mockup only stretches the picture), and the minimum height grows to {HA}. With Auto release on, each band's knob reads Auto.")),
        ("2 · A with four bands", "A fourth band; each crossover can be dragged or typed.",
         shot("limit-v172-tall", WA, HA, limit_bands_card(285, 4, ["30 ms", "60 ms", "100 ms", "150 ms"], [4.5, 2.0, 1.0, 2.5], [120, 1000, 6000]),
              "", crop=(105, 275, 540, 180))),
        ("3 · A at its default, one band", "The card stays, so the window doesn't jump when the count changes; the strip shows the one reduction.",
         shot("limit-v172-tall", WA, HA, limit_bands_card(285, 1, ["", "", "", ""], [3.0], []), "", crop=(105, 275, 540, 180))),
        ("4 · B: the window stays 760 x 465; the display card switches between History and Bands, and the LIMITER card's Release follows a band",
         "A <b>History | Bands</b> switch, as MultiComp's Spectrum | GR history, puts the crossover strip in the history's place with the band count beside it. "
         "Band chips in the LIMITER card's title row choose whose Release the knob shows (band 2's here, in its colour). The crossover type goes in the EDIT drawer.",
         shot("limit-v172", LIMIT_W, LIMIT_H, limit_b(),
              "No size change, but the history and the bands can't be seen together, and one knob changes meaning with the selected chip, "
              "so a band's release is never in view beside the others'.")),
    ])


m.PAGE_HEIGHTS["limit-w3"] = 3000


# ---------------------------------------------------------------------------------------------
# Gate multiband (▲, docs/enhancements.md section 9 item 7: 2 to 3 bands), on Gate's screenshot from run 45897768844
# (commit a24dff5), img/gate-v172-w2b.png, 790 x 465 at 1x. Measured from it and GateEditor.cpp (header 73 px):
# the history graph is x 120 to 626, y 113 to 183; the envelope card (Pre-open to Hysteresis) starts at y 191 and the
# Level and filter cards at y 295, all x 120 to 626; the footer's checkboxes are at y 433; the left column (Threshold,
# Gain) is x 0 to 119 and the right one (Out, Wet, Dry, Noise) x 640 to 790. A: a BANDS card at y 191, as Limit's.
GATE_W, GATE_H = 790, 465
GATE_GROW = 172  # A: the BANDS card (164) and its gap, as Limit's


def gate_bands_card(top, bands, thresholds, ranges, states, splits, attenuation):
    """A: the BANDS card, x 120 to 626, 164 tall, between the graph and the envelope card."""
    x0, w = 120, 506
    html = f'<div class="o" style="left:{x0}px;top:{top}px;width:{w}px;height:164px;background:#262626;border-radius:4px"></div>'
    html += ('<div class="o" style="left:132px;top:%dpx;font-size:12px;font-weight:600;letter-spacing:.04em;color:#8D8D8D">BANDS</div>' % (top + 11))
    html += m.seg(186, top + 8, 90, 20, ["1", "2", "3"], bands - 1, YELLOW, text="#141414")
    html += '<div class="o" style="left:393px;top:%dpx;font-size:12px;color:#8D8D8D">Crossover</div>' % (top + 11)
    html += '<div class="o dd" style="left:457px;top:%dpx;width:157px;height:22px;font-size:12px">LR 24 dB &middot; Min phase</div>' % (top + 7)
    if bands == 1:
        html += limit_graph(132, top + 34, 482, 44, [], attenuation[:1], scale_db=40.0)
        html += caption(132, top + 100, "One band is today's gate, to the bit: the band row is empty and the cards below do everything.", 482)
        return html + outline(x0 - 2, top - 2, w + 4, 168)
    html += limit_graph(132, top + 34, 482, 44, splits, attenuation, scale_db=40.0)
    cell = 482 / 3
    for i in range(bands):
        cx = 132 + cell * i
        html += band_chip(cx + 2, top + 108, i)
        on = states[i] == "open"
        html += (f'<div class="o" style="left:{cx - 2:.0f}px;top:{top + 130}px;width:26px;font-size:10px;text-align:center;'
                 f'color:{YELLOW if on else "#8D8D8D"}">{states[i]}</div>')
        html += m.knob(cx + 58, top + 94, "Threshold", thresholds[i], BAND_COLOURS[i], 0.5, size=28, fs=12, width=64, bi=True)
        html += m.knob(cx + 124, top + 94, "Range", ranges[i], BAND_COLOURS[i], 0.45, size=28, fs=12, width=64)
    return html + outline(x0 - 2, top - 2, w + 4, 168)


def gate_b():
    """B: the window stays 790 x 465. The history graph switches between History and Bands; each band's Threshold
    and Range go in the EDIT drawer."""
    html = cover(120, 113, 506, 70, "#141414")
    html += m.seg(126, 116, 112, 18, ["History", "Bands"], 1, "#424242", fs=11, text="#fff")
    html += '<div class="o" style="left:452px;top:117px;font-size:11px;color:#8D8D8D">Bands</div>'
    html += m.seg(494, 116, 126, 18, ["1", "2", "3"], 2, YELLOW, fs=11, text="#141414")
    html += limit_graph(126, 138, 494, 32, [180, 4000], [0.0, 40.0, 12.0], scale_db=40.0, labels=False)
    return html + outline(118, 111, 510, 74)


@m.plugin("gate-w3", "75__gate_w3_layout_mockup.png")
def gate_w3():
    WA, HA = m.grow("gate-v172", "gate-v172-tall", GATE_GROW, [(0, 120, 275), (120, 640, 187), (640, 790, 300)], axis="y")
    splits3 = [180, 4000]
    return w1.page("Fancy Gate: v1.7.2 layout mockup (multiband gating, ▲)",
                   "docs/enhancements.md, section 9 item 7: 2 to 3 bands. It changes the visible layout, so the layout comes before the design "
                   "note. Changed things in dashed yellow, on the plugin's real screenshot (1x coordinates). The parameters drawn here (band count, "
                   "crossovers, each band's Threshold and Range) are proposals the note will set out as decisions.", [
        (f"1 · A (recommended): a BANDS card between the graph and the envelope card, as Limit's; the window grows {GATE_GROW} px taller ({WA} x {HA})",
         "<b>Bands</b> 1 to 3 (1 by default, today's gate), the <b>Crossover</b> type, and a strip with each band's attenuation falling from the top "
         "in its colour: here a drum overhead, the lows open, the mids shut on bleed, the highs part-way. Drag a blue line to move a crossover, or "
         "click it to type one. Under the strip each band has its own <b>Threshold</b> (an offset from the Threshold fader, so the fader still moves "
         "them all) and <b>Range</b>, and its open or shut state. Attack, Hold, Release, Hysteresis and the detector stay shared, in the cards below.",
         shot("gate-v172-tall", WA, HA,
              gate_bands_card(191, 3, ["+0.0 dB", "+6.0 dB", "+3.0 dB"], ["-40 dB", "-40 dB", "-20 dB"], ["open", "shut", "open"], splits3, [0.0, 40.0, 12.0]),
              f"Nothing existing moves sideways or shrinks: the envelope, Level and filter cards and the footer move down {GATE_GROW} px; the "
              f"Threshold, Gain, Out, Wet, Dry and Noise meters and faders lengthen with the window (their scales are respaced evenly in the "
              f"build; the mockup only stretches the picture), and the minimum height grows to {HA}. The Crossover dropdown is 157 px, "
              "the width MultiComp and Limit need for its text.")),
        ("2 · A with two bands", "One crossover; the second band's controls take its place, the third cell stays empty.",
         shot("gate-v172-tall", WA, HA,
              gate_bands_card(191, 2, ["+0.0 dB", "+6.0 dB", ""], ["-40 dB", "-30 dB", ""], ["open", "shut", ""], [250], [0.0, 30.0]),
              "", crop=(105, 181, 540, 180))),
        ("3 · A at its default, one band", "The card stays, so the window doesn't jump when the count changes; the strip shows the one attenuation.",
         shot("gate-v172-tall", WA, HA, gate_bands_card(191, 1, [], [], [], [], [8.0]), "", crop=(105, 181, 540, 180))),
        ("4 · B: the window stays 790 x 465; the graph switches between History and Bands, and each band's Threshold and Range go in the EDIT drawer",
         "A <b>History | Bands</b> switch, as Limit's option B and MultiComp's Spectrum | GR history, puts the band strip in the graph's place with "
         "the band count beside it. The crossover type and the six band knobs go in the EDIT drawer.",
         shot("gate-v172", GATE_W, GATE_H, gate_b(),
              "No size change, but the history and the bands can't be seen together, and the per-band settings are a drawer away from the strip "
              "that shows what they do.")),
    ])


m.PAGE_HEIGHTS["gate-w3"] = 3000


# ---------------------------------------------------------------------------------------------
# Comp's History on the main page (user, 27 November 2026, a late Wave 3 addition): the waveform
# from EDIT > History in view while the knobs turn. On Comp's screenshot from run 45902595407
# (commit da9d0f3), img/comp-v172-w2b.png, 934 x 527 at 1x. Measured from it and CompEditor.cpp
# (header 73 px, gridLeft 212, gridWidth 524, rowTop 50, rowHeight 374): the knob cards are x 212 to
# 736, y 123 to 497; Threshold x 0 to 107 and GR x 113 to 207, y 123 to 497; Out, Wet and Dry x 736
# to 934; the footer's checkboxes at y 523. The drawer covers the knob cards (y 123 to 497).
COMP_GROW = 200  # the HISTORY card (194) and its 6 px gap
CARD_W, GRAPH_W, GRAPH_H = 524, 500, 152


def comp_check(x, y, label, on=True):
    box = (f'<div style="width:14px;height:14px;border-radius:3px;display:flex;align-items:center;justify-content:center;'
           f'background:{"#00A985" if on else "transparent"};border:{"none" if on else "1px solid #8D8D8D"};color:#141414;font-size:11px;'
           f'font-weight:700">{"&#10003;" if on else ""}</div>')
    return (f'<div class="o" style="left:{x}px;top:{y}px;height:16px;display:flex;align-items:center;gap:7px;font-size:13px;color:#fff">'
            f'{box}{label}</div>')


def comp_history_card(top):
    """The HISTORY card, x 212 to 736, 194 tall: the title row (zoom, Grid, peak) and the graph."""
    html = f'<div class="o" style="left:212px;top:{top}px;width:{CARD_W}px;height:194px;background:#262626;border-radius:6px"></div>'
    html += ('<div class="o" style="left:224px;top:%dpx;font-size:12px;font-weight:600;letter-spacing:.06em;color:#8D8D8D">HISTORY</div>' % (top + 11))
    html += m.seg(300, top + 7, 176, 22, ["1.5 s", "3 s", "6 s", "12 s"], 1, "#4C4C4C", fs=12, new=False, dark="#1E1E1E", text="#fff")
    html += comp_check(492, top + 10, "Grid")
    html += ('<div class="o" style="left:600px;top:%dpx;width:124px;font-size:12px;color:#FFCF07;text-align:right">peak 7.7 dB</div>' % (top + 11))
    html += (f'<div class="o" style="left:224px;top:{top + 36}px;width:{GRAPH_W}px;height:{GRAPH_H}px">'
             f'{m.history_svg(m.beat_columns(), "db", w=GRAPH_W, h=GRAPH_H)}</div>')
    return html + outline(210, top - 2, CARD_W + 4, 198)


@m.plugin("comp-w3", "76__comp_history_main_mockup.png")
def comp_w3():
    # A: rows repeated above the knob cards in the centre (y 121 is the gap under the Detector row),
    # and inside the meters' wells and the faders' tracks at the sides.
    WA, HA = m.grow("comp-v172", "comp-v172-a", COMP_GROW, [(0, 212, 300), (212, 736, 121), (736, 934, 300)], axis="y")
    # B: rows repeated under the knob cards (y 499 is the gap above the footer).
    WB, HB = m.grow("comp-v172", "comp-v172-b", COMP_GROW, [(0, 212, 300), (212, 736, 499), (736, 934, 300)], axis="y")
    return w1.page("Fancy Comp: v1.7.2 layout mockup (History on the main page)",
                   "User, 27 November 2026: the waveform from EDIT &gt; History on the main page, so you see what turning a knob does to the audio "
                   "without opening the drawer. It changes the visible layout, so the layout comes before the design note. Changed things in dashed "
                   "yellow, on the plugin's real screenshot (1x coordinates). The graph is today's History, as built in v1.7.1 (input in grey, "
                   "output in teal before make-up, both in dB from -60 at the centre line to 0 at the edges; the gain reduction in yellow from the "
                   "top; the threshold dashed), here a beat at 120 BPM into 3:1 at -12 dB.", [
        (f"1 · A (recommended): a HISTORY card at the top of the centre column, above the knobs; the window grows {COMP_GROW} px taller ({WA} x {HA})",
         "The card holds everything the History page has: the <b>1.5 s, 3 s, 6 s, 12 s</b> zoom, <b>Grid</b>, <b>peak</b> (click to clear) and the "
         "graph (click to pause), 500 x 152 px against the page's 476 x 260. It sits beside the Threshold meter, at the same height as its handle, "
         "so the dashed threshold lines and the handle are read together, and right above Ratio, Knee, Attack and Release.",
         shot("comp-v172-a", WA, HA, comp_history_card(123),
              f"Nothing moves sideways or shrinks: the knob cards and the footer move down {COMP_GROW} px, and the Threshold, GR and Out meters and the "
              f"Wet and Dry faders lengthen with the window (their scales are respaced evenly in the build; the mockup only stretches the picture). "
              f"The minimum height grows to {HA}. The EDIT drawer still covers only the knob cards, so the waveform stays in view while you work "
              "in it; its History page goes, and its switch keeps Detector, Character, Upward and Options.")),
        (f"2 · B: the same card under the knob cards; the window grows {COMP_GROW} px taller ({WB} x {HB})",
         "The knobs stay exactly where they are today, and the waveform sits under them, above the footer's checkboxes.",
         shot("comp-v172-b", WB, HB, comp_history_card(503),
              "The same card, size and behaviour. The knobs don't move, but the waveform is further from the Threshold handle and the dashed "
              "threshold lines it draws, and it sits between the knobs and the checkboxes (Auto make-up, Limit output) that also change it.")),
    ])


m.PAGE_HEIGHTS["comp-w3"] = 1800

if __name__ == "__main__":
    for key in sys.argv[1:] or list(m.PLUGINS):
        m.render(key)
