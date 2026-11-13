"""Fancy Room: every label, read-out and caption against its box (docs/room.md, Phase 4).

Widths come from the Inter fonts the plugins embed (assets/fonts), read from their
cmap and hmtx tables. JUCE's withPointHeight (h) sets the em size, so a glyph is
advance * h / unitsPerEm wide. Kerning is left out, so each box keeps a 2 px margin.
Run:

    python -I docs/research/room-textfit.py
"""
import os
import struct
import sys

FONTS = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "fonts")
MARGIN = 2.0


class Face:
    def __init__(self, path):
        with open(path, "rb") as f:
            data = f.read()
        num_tables = struct.unpack(">H", data[4:6])[0]
        tables = {}
        for i in range(num_tables):
            tag, _, off, length = struct.unpack(">4sIII", data[12 + 16 * i:28 + 16 * i])
            tables[tag.decode("latin-1")] = (off, length)
        head = tables["head"][0]
        self.units = struct.unpack(">H", data[head + 18:head + 20])[0]
        hhea = tables["hhea"][0]
        n_metrics = struct.unpack(">H", data[hhea + 34:hhea + 36])[0]
        hmtx = tables["hmtx"][0]
        self.advances = [struct.unpack(">H", data[hmtx + 4 * i:hmtx + 4 * i + 2])[0] for i in range(n_metrics)]
        self.cmap = self._cmap(data, tables["cmap"][0])

    @staticmethod
    def _cmap(data, base):
        n = struct.unpack(">H", data[base + 2:base + 4])[0]
        best = None
        for i in range(n):
            pid, eid, off = struct.unpack(">HHI", data[base + 4 + 8 * i:base + 12 + 8 * i])
            fmt = struct.unpack(">H", data[base + off:base + off + 2])[0]
            if pid == 3 and fmt in (4, 12) and (best is None or fmt == 12):
                best = (fmt, base + off)
        fmt, t = best
        out = {}
        if fmt == 12:
            groups = struct.unpack(">I", data[t + 12:t + 16])[0]
            for g in range(groups):
                start, end, glyph = struct.unpack(">III", data[t + 16 + 12 * g:t + 28 + 12 * g])
                for c in range(start, min(end, 0x2FFFF) + 1):
                    out[c] = glyph + c - start
            return out
        segs = struct.unpack(">H", data[t + 6:t + 8])[0] // 2
        ends = struct.unpack(">%dH" % segs, data[t + 14:t + 14 + 2 * segs])
        p = t + 16 + 2 * segs
        starts = struct.unpack(">%dH" % segs, data[p:p + 2 * segs])
        deltas = struct.unpack(">%dh" % segs, data[p + 2 * segs:p + 4 * segs])
        ro_at = p + 4 * segs
        ros = struct.unpack(">%dH" % segs, data[ro_at:ro_at + 2 * segs])
        for s in range(segs):
            for c in range(starts[s], ends[s] + 1):
                if c == 0xFFFF:
                    continue
                if ros[s] == 0:
                    out[c] = (c + deltas[s]) & 0xFFFF
                else:
                    a = ro_at + 2 * s + ros[s] + 2 * (c - starts[s])
                    g = struct.unpack(">H", data[a:a + 2])[0]
                    out[c] = (g + deltas[s]) & 0xFFFF if g else 0
        return out

    def width(self, text, size):
        total = 0
        for ch in text:
            glyph = self.cmap.get(ord(ch))
            if glyph is None:
                raise ValueError("no glyph for %r" % ch)
            total += self.advances[min(glyph, len(self.advances) - 1)]
        return total * size / self.units


FACES = {w: Face(os.path.join(FONTS, "Inter-%s.ttf" % w)) for w in ("Regular", "Medium", "SemiBold")}

# Proposed geometry, from Sature's (plugins/sature/src/SatureEditor.cpp) and room.html:
# content x 13..646; Space card flex 2, Tone card flex 1, 8 px apart.
CONTENT = 646 - 13
SPACE_W = (CONTENT - 8) * 2 / 3
TONE_W = (CONTENT - 8) / 3
KNOB_W = 68          # Knob component width; below the 69.4 px spacing in both cards
CHECKBOX_TEXT = 14 + 6  # box and gap before a checkbox label
FOOTER_GAP = 12
DRAWER_LABEL_W = 170  # Sature's drawer: label 170, dropdown 170
DROPDOWN_TEXT_W = 170 - 8 - 18  # Controls.cpp trims 8 left, 18 right for the arrow

# (what, text, weight, size, box width)
CHECKS = []


def check(what, texts, weight, size, box):
    for t in texts:
        CHECKS.append((what, t, weight, size, box))


TIGHT = ("Regular", 12.0)   # Knob::tightStyle labels and values
SMALL = ("Regular", 14.0)   # Knob::smallStyle (drawer knobs)

check("Space/Tone knob label", ["Pre-delay", "Size", "Decay", "Diffusion", "Mod", "Width",
                                "Damping", "Low cut", "High cut"], *TIGHT, KNOB_W)
check("Knob value", ["250 ms", "100 %", "20.0 s", "0.10 s", "−1.00", "+2.00", "1000 Hz", "20.0 kHz",
                     "1.00 kHz"], *TIGHT, KNOB_W)
check("Pre-delay value, Sync on", ["1/64 triplet", "1/16 dotted", "1/32 triplet", "1/8 dotted"], *TIGHT, KNOB_W)
# Capped notes keep the note text and turn red (the user's choice, 13 November 2026): the
# "1/8 dotted (250 ms max)" of decision 5 is 138 px, so it's in the tooltip instead.
check("Drawer label", ["Ducking", "Quality", "Early reflections", "Stereo input"], "Regular", 14.0, DRAWER_LABEL_W)
check("Drawer dropdown", ["Eco", "Normal", "High", "Stereo", "Mono-sum"], "Regular", 14.0, DROPDOWN_TEXT_W)
check("Drawer knob value", ["−inf dB", "+6.0 dB", "100 %"], *SMALL, 90)
check("Display marker", ["decay 20.0 s (to −60 dB)", "decay 1.80 s (to −60 dB)"], "Regular", 10.0,
      SPACE_W + 8 + TONE_W - 32 - 14 - 6)  # from the marker to the plot's right edge, worst case: marker at the left
check("Display axis", ["−60", "3 s", "20 s", "2.5 s"], "Regular", 9.0, 26)


def footer():
    """The footer row: three checkboxes, then EDIT right-aligned at 646 - 64."""
    labels = ["Sync pre-delay", "Freeze", "Wet only (send)"]
    x = 13
    rows = []
    for t in labels:
        w = CHECKBOX_TEXT + FACES["Regular"].width(t, 12.0) + MARGIN
        rows.append((t, x, w))
        x += w + FOOTER_GAP
    return rows, x - FOOTER_GAP, 646 - 64


def legend():
    """The display's legend, laid out from measured widths and right-aligned at the plot's
    right edge. room.html places it at fixed offsets, where "early reflections" (76 px)
    runs into the decay swatch."""
    entries = ["early reflections", "decay", "highs"]
    swatch, gap, between = 14, 4, 12
    width = sum(swatch + gap + FACES["Regular"].width(t, 10.0) + MARGIN for t in entries) + between * (len(entries) - 1)
    plot = 610 - 32 - 14   # the display's plot width in room.html: L = 32, R = W - 14
    return width, plot - 2 * 120  # leave the left of the plot clear for the "decay" marker's label


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    fails = 0
    for what, text, weight, size, box in CHECKS:
        w = FACES[weight].width(text, size)
        ok = w + MARGIN <= box
        fails += not ok
        print("%-4s %-36s %-28s %6.1f of %6.1f px" % ("ok" if ok else "FAIL", what, text, w, box))
    rows, end, edit = footer()
    for t, x, w in rows:
        print("     footer %-20s x %5.1f, width %5.1f" % (t, x, w))
    ok = end <= edit - FOOTER_GAP
    fails += not ok
    print("%-4s footer ends at %.1f, EDIT starts at %d" % ("ok" if ok else "FAIL", end, edit))
    width, room = legend()
    ok = width <= room
    fails += not ok
    print("%-4s display legend, laid out from widths: %.1f of %.1f px" % ("ok" if ok else "FAIL", width, room))
    print("%d failures" % fails)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
