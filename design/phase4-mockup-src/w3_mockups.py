"""v1.7.2 mockups, drawn as w2b_mockups.py's, on CI run 45602564567's screenshots (commit 9196802, v1.7.1).

    py -3 design/phase4-mockup-src/w3_mockups.py [key ...]
"""
import sys

import w2b_mockups as m
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

if __name__ == "__main__":
    for key in sys.argv[1:] or list(m.PLUGINS):
        m.render(key)
