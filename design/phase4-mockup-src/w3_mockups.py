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

if __name__ == "__main__":
    for key in sys.argv[1:] or list(m.PLUGINS):
        m.render(key)
