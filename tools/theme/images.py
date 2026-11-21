"""Stage B images for the Fancy Plugs REAPER theme (docs/reaper-theme.md).

Drawn with Pillow (approved by the user for the theme script, 21 November 2026; the plugins don't
use it). Every image is drawn at 4x its final size and scaled down, for clean antialiased edges,
at three scales: 100 % (the image folder), 150 % and 200 % (its "150" and "200" subfolders).

Format, from REAPER's image reference (reaper.fm/sdk/walter/images.php):
- Buttons are three frames side by side: normal, mouseover, pressed.
- Backgrounds may carry 1 px pink (255, 0, 255) lines from the upper-left and lower-right corners,
  marking the parts that don't stretch. The lines sit in a 1 px border around the image.
"""

from PIL import Image, ImageDraw, ImageFont
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
FONTS = ROOT / "theme" / "fonts"
SS = 4  # supersampling

PINK = (255, 0, 255, 255)

# docs/DESIGN_SYSTEM.md
HDR, BODY, SEC, WELL = "#1E1E1E", "#303030", "#262626", "#141414"
G5, G6, HOVER, PRESSED = "#424242", "#4C4C4C", "#383838", "#1E1E1E"
TXT, TXT2 = "#FFFFFF", "#8D8D8D"
TEAL, RED, ORANGE, YELLOW, BLUE = "#00A985", "#E74D3D", "#E77E23", "#F1C315", "#3598DB"
GREEN, PURPLE = "#2DCC70", "#9A59B5"
KNOB_TOP, KNOB_BOT = "#4F4F4F", "#454545"
STROKE = 1.75  # toolbar icon line width at 100 %


def rgba(c, a=255):
    if isinstance(c, tuple):
        return c
    return (int(c[1:3], 16), int(c[3:5], 16), int(c[5:7], 16), a)


def font(size, weight="SemiBold"):
    return ImageFont.truetype(str(FONTS / f"Inter_18pt-{weight}.ttf"), size)


class Canvas:
    """A supersampled RGBA canvas in final-size units: draw at (x, y) in pixels of the final image."""

    def __init__(self, w, h, scale):
        self.w, self.h, self.k = w, h, scale * SS
        self.im = Image.new("RGBA", (round(w * self.k), round(h * self.k)), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.im)

    def _r(self, x, y, w, h):
        k = self.k
        return [round(x * k), round(y * k), round((x + w) * k) - 1, round((y + h) * k) - 1]

    def rect(self, x, y, w, h, fill, radius=0):
        self.d.rounded_rectangle(self._r(x, y, w, h), radius=round(radius * self.k), fill=rgba(fill))

    def outline(self, x, y, w, h, colour, radius=0, width=1.0):
        self.d.rounded_rectangle(self._r(x, y, w, h), radius=round(radius * self.k), outline=rgba(colour),
                                 width=max(1, round(width * self.k)))

    def circle(self, cx, cy, r, fill=None, outline=None, width=1.0):
        k = self.k
        box = [round((cx - r) * k), round((cy - r) * k), round((cx + r) * k), round((cy + r) * k)]
        self.d.ellipse(box, fill=rgba(fill) if fill else None, outline=rgba(outline) if outline else None,
                       width=max(1, round(width * k)))

    def line(self, pts, colour, width=1.0):
        k = self.k
        self.d.line([(round(x * k), round(y * k)) for x, y in pts], fill=rgba(colour), width=max(1, round(width * k)),
                    joint="curve")

    def arc(self, cx, cy, r, start, end, colour, width=1.0):
        """Angles in degrees, clockwise from 3 o'clock (Pillow's convention)."""
        k = self.k
        box = [round((cx - r) * k), round((cy - r) * k), round((cx + r) * k), round((cy + r) * k)]
        self.d.arc(box, start, end, fill=rgba(colour), width=max(1, round(width * k)))

    def polygon(self, pts, fill):
        k = self.k
        self.d.polygon([(round(x * k), round(y * k)) for x, y in pts], fill=rgba(fill))

    def text(self, cx, cy, s, size, colour, weight="SemiBold"):
        f = font(round(size * self.k), weight)
        self.d.text((round(cx * self.k), round(cy * self.k)), s, font=f, fill=rgba(colour), anchor="mm")

    def image(self):
        """The final image: scaled down from the supersampled canvas."""
        size = (round(self.w * self.k / SS), round(self.h * self.k / SS))
        return self.im.resize(size, Image.LANCZOS)


def frames(draw_one, fw, fh, scale):
    """A three-frame button image (normal, mouseover, pressed), each frame fw x fh."""
    states = ("normal", "hover", "pressed")
    parts = []
    for st in states:
        c = Canvas(fw, fh, scale)
        draw_one(c, st)
        parts.append(c.image())
    w, h = parts[0].size
    out = Image.new("RGBA", (w * 3, h), (0, 0, 0, 0))
    for i, p in enumerate(parts):
        out.paste(p, (i * w, 0))
    return out


def with_markers(img, left=0, top=0, right=0, bottom=0):
    """Adds the 1 px marker border: pink from the upper-left corner along the top row (left
    unstretched width) and left column (top unstretched height), and from the lower-right corner
    along the bottom row (right width) and right column (bottom height). Sizes in final pixels."""
    w, h = img.size
    out = Image.new("RGBA", (w + 2, h + 2), (0, 0, 0, 0))
    out.paste(img, (1, 1))
    px = out.load()
    for x in range(0, left + 1):
        px[x, 0] = PINK
    for y in range(0, top + 1):
        px[0, y] = PINK
    for x in range(w + 1 - right, w + 2):
        px[x, h + 1] = PINK
    for y in range(h + 1 - bottom, h + 2):
        px[w + 1, y] = PINK
    return out


def button_face(c, st, on_fill=None, radius=3):
    """The suite's control: #303030, radius 3; lighter on hover, darker when pressed. An 'on'
    button takes its colour, slightly lifted on hover and lowered when pressed."""
    if on_fill:
        base = rgba(on_fill)
        shift = {"normal": 0, "hover": 18, "pressed": -22}[st]
        fill = tuple(max(0, min(255, v + shift)) for v in base[:3]) + (255,)
    else:
        # Grey-5 rather than the plugins' #303030: REAPER's panels are #2B2B2B, where #303030 vanishes.
        fill = rgba({"normal": G5, "hover": "#4A4A4A", "pressed": PRESSED}[st])
    c.rect(0.5, 0.5, c.w - 1, c.h - 1, fill, radius)


# ---- Icons, drawn in a frame of the canvas's size -------------------------------------------------

def icon_text(label, size=9.0, colour=TXT, weight="SemiBold"):
    def draw(c):
        c.text(c.w / 2, c.h / 2 + 0.3, label, size, colour, weight)
    return draw


def icon_channels(stereo, colour):
    """Mono: one ring. Stereo: two overlapping rings."""
    def draw(c):
        cx, cy = c.w / 2, c.h / 2
        if stereo:
            c.circle(cx - 2.6, cy, 4, outline=colour, width=1.3)
            c.circle(cx + 2.6, cy, 4, outline=colour, width=1.3)
        else:
            c.circle(cx, cy, 4, outline=colour, width=1.3)
    return draw


def icon_menu(colour):
    def draw(c):
        cx, cy = c.w / 2, c.h / 2
        for dy in (-3.5, 0, 3.5):
            c.line([(cx - 5, cy + dy), (cx + 5, cy + dy)], colour, 1.4)
    return draw


def icon_recarm(on):
    def draw(c):
        cx, cy = c.w / 2, c.h / 2
        if on:
            c.circle(cx, cy, 5.2, fill=rgba(RED, 90))
            c.circle(cx, cy, 4.2, fill=RED)
        else:
            c.circle(cx, cy, 4.2, outline=TXT2, width=1.4)
    return draw


def icon_phase(colour):
    def draw(c):
        cx, cy = c.w / 2, c.h / 2
        c.circle(cx, cy, 4.2, outline=colour, width=1.4)
        c.line([(cx - 4.6, cy + 4.6), (cx + 4.6, cy - 4.6)], colour, 1.4)
    return draw


def icon_monitor(colour, auto=False):
    """A small speaker; auto adds an A."""
    def draw(c):
        cx, cy = c.w / 2 - (2 if auto else 0), c.h / 2
        c.polygon([(cx - 5, cy - 2), (cx - 2.5, cy - 2), (cx + 1, cy - 5), (cx + 1, cy + 5),
                   (cx - 2.5, cy + 2), (cx - 5, cy + 2)], colour)
        if auto:
            c.text(cx + 6.5, cy + 0.3, "A", 7.5, colour)
        else:
            c.line([(cx + 3, cy - 2.5), (cx + 4.2, cy), (cx + 3, cy + 2.5)], colour, 1.2)
    return draw


def icon_env(colour):
    def draw(c):
        cx, cy = c.w / 2, c.h / 2
        c.line([(cx - 6, cy + 3), (cx - 2.5, cy - 3.5), (cx + 1.5, cy + 1.5), (cx + 6, cy - 3)], colour, 1.4)
        for x, y in [(cx - 2.5, cy - 3.5), (cx + 1.5, cy + 1.5)]:
            c.circle(x, y, 1.5, fill=colour)
    return draw


def icon_io(receives, sends, colour):
    def draw(c):
        c.text(c.w / 2, c.h / 2 + 0.3, "IO", 8.5, colour)
        if receives:
            c.circle(3.6, c.h / 2, 1.6, fill=TEAL)
        if sends:
            c.circle(c.w - 3.6, c.h / 2, 1.6, fill=TEAL)
    return draw


def icon_fx(colour, struck=False):
    def draw(c):
        c.text(c.w / 2, c.h / 2 + 0.3, "FX", 8.5, colour)
        if struck:
            c.line([(c.w / 2 - 6, c.h / 2 + 4), (c.w / 2 + 6, c.h / 2 - 4)], RED, 1.4)
    return draw


def icon_folder(kind):
    """Folder state: off (normal track), on (folder parent), last (last in folder)."""
    def draw(c):
        cx, cy = c.w / 2, c.h / 2
        shape = [(cx - 5, cy - 3.5), (cx - 1.5, cy - 3.5), (cx, cy - 2), (cx + 5, cy - 2), (cx + 5, cy + 3.5),
                 (cx - 5, cy + 3.5)]
        if kind == "on":
            c.polygon(shape, TEAL)
        elif kind == "last":
            c.line([(cx - 4, cy - 4), (cx - 4, cy + 2), (cx + 4, cy + 2)], TXT2, 1.4)
        else:
            # An outline, so a normal track's button reads as a folder rather than a grey block.
            c.line(shape + [shape[0]], TXT2, 1.2)
    return draw


def icon_fxbypass(state):
    """The FX chain's on/off switch beside the FX button: teal dot on, grey ring off, faint dot
    with no FX."""
    def draw(c):
        cx, cy = c.w / 2, c.h / 2
        if state == "on":
            c.circle(cx, cy, 2.2, fill=TEAL)
        elif state == "off":
            c.circle(cx, cy, 2.2, outline=TXT2, width=1.1)
        else:
            c.circle(cx, cy, 1.2, fill=G6)
    return draw


def icon_toolbar(kind, colour):
    """The main toolbar's icons: thin line drawings about 14 px across, in a 30 px frame."""
    def draw(c):
        cx, cy = c.w / 2, c.h / 2
        w = STROKE
        if kind == "new":
            c.outline(cx - 5, cy - 7, 10, 14, colour, 1.5, w)
            c.line([(cx - 2.5, cy + 1), (cx + 2.5, cy + 1)], colour, w)
            c.line([(cx, cy - 1.5), (cx, cy + 3.5)], colour, w)
        elif kind == "load":
            c.line([(cx - 7, cy + 5), (cx - 7, cy - 5), (cx - 3, cy - 5), (cx - 1.5, cy - 3.5), (cx + 5, cy - 3.5),
                    (cx + 5, cy - 1)], colour, w)
            c.line([(cx - 7, cy + 5), (cx - 4.5, cy - 1), (cx + 7.5, cy - 1), (cx + 5, cy + 5), (cx - 7, cy + 5)],
                   colour, w)
        elif kind == "save":
            c.outline(cx - 6.5, cy - 6.5, 13, 13, colour, 1.5, w)
            c.rect(cx - 3.5, cy - 6.5, 7, 4.5, colour, 0.5)
            c.outline(cx - 3.5, cy + 1.5, 7, 5, colour, 0.5, w)
        elif kind == "projprop":
            for i, y in enumerate((cy - 4.5, cy, cy + 4.5)):
                c.line([(cx - 7, y), (cx + 7, y)], colour, w)
                c.circle(cx + (-3, 3, -1)[i], y, 1.9, fill=colour)
        elif kind in ("undo", "redo"):
            s = 1 if kind == "undo" else -1
            c.arc(cx + s * 1, cy + 2, 5.5, 180, 360, colour, w)
            c.line([(cx + s * 6.5, cy + 2), (cx + s * 6.5, cy + 5)], colour, w)
            tip = cx - s * 4.5
            c.polygon([(tip - 3, cy + 1), (tip + 3, cy + 1), (tip, cy + 5)], colour)
        elif kind == "revert":
            # Three quarters of a circle, open at the upper right, with an arrowhead at the top.
            c.arc(cx, cy, 6, 330, 360, colour, w)
            c.arc(cx, cy, 6, 0, 270, colour, w)
            c.polygon([(cx - 0.5, cy - 9), (cx + 3.5, cy - 6), (cx - 0.5, cy - 3)], colour)
        elif kind == "metro":
            c.line([(cx - 3, cy - 7), (cx + 3, cy - 7), (cx + 6, cy + 7), (cx - 6, cy + 7), (cx - 3, cy - 7)], colour, w)
            c.line([(cx, cy + 4), (cx + 4.5, cy - 6)], colour, w)
            c.circle(cx + 2.4, cy - 1.4, 1.5, fill=colour)
        elif kind == "grid":
            for d in (-3.5, 3.5):
                c.line([(cx + d, cy - 7), (cx + d, cy + 7)], colour, 1.2)
                c.line([(cx - 7, cy + d), (cx + 7, cy + d)], colour, 1.2)
        elif kind in ("snap", "relsnap"):
            dy = -1.5 if kind == "relsnap" else 0
            c.arc(cx, cy + 1 + dy, 5.5, 0, 180, colour, 3)
            c.line([(cx - 5.5, cy - 6 + dy), (cx - 5.5, cy + 1 + dy)], colour, 3)
            c.line([(cx + 5.5, cy - 6 + dy), (cx + 5.5, cy + 1 + dy)], colour, 3)
            c.rect(cx - 7, cy - 7.5 + dy, 3, 2.5, colour)
            c.rect(cx + 4, cy - 7.5 + dy, 3, 2.5, colour)
            if kind == "relsnap":
                c.line([(cx - 4, cy + 8.5), (cx + 4, cy + 8.5)], colour, 1.1)
                c.polygon([(cx + 3, cy + 6.8), (cx + 6, cy + 8.5), (cx + 3, cy + 10.2)], colour)
                c.polygon([(cx - 3, cy + 6.8), (cx - 6, cy + 8.5), (cx - 3, cy + 10.2)], colour)
        elif kind in ("lock_on", "lock_off"):
            c.rect(cx - 5.5, cy - 1, 11, 8, colour, 1.5)
            if kind == "lock_on":
                c.arc(cx, cy - 3.5, 3.5, 180, 360, colour, w)
                c.line([(cx - 3.5, cy - 3.5), (cx - 3.5, cy - 1)], colour, w)
                c.line([(cx + 3.5, cy - 3.5), (cx + 3.5, cy - 1)], colour, w)
            else:
                c.arc(cx + 5, cy - 4.5, 3.5, 180, 360, colour, w)
                c.line([(cx + 1.5, cy - 4.5), (cx + 1.5, cy - 1)], colour, w)
        elif kind.startswith("ripple"):
            lanes = (cy - 3.5, cy + 3.5) if kind == "ripple_all" else (cy,)
            for y in lanes:
                c.rect(cx - 7, y - 2.2, 5, 4.4, colour, 1)
                c.rect(cx + 2, y - 2.2, 5, 4.4, colour, 1)
            if kind != "ripple_off":
                y = cy - 7.5 if kind == "ripple_all" else cy - 5
                c.line([(cx + 4, y), (cx - 3, y)], colour, 1.2)
                c.polygon([(cx - 2, y - 2), (cx - 5, y), (cx - 2, y + 2)], colour)
        elif kind == "xfade":
            c.line([(cx - 7, cy + 5), (cx + 7, cy - 5)], colour, w)
            c.line([(cx - 7, cy - 5), (cx + 7, cy + 5)], colour, w)
        elif kind == "envitem":
            c.outline(cx - 7, cy - 5.5, 14, 11, colour, 1.5, 1.2)
            c.line([(cx - 5, cy + 2.5), (cx - 1.5, cy - 2.5), (cx + 1.5, cy + 1), (cx + 5, cy - 2.5)], colour, w)
        elif kind == "group":
            c.outline(cx - 7, cy - 6, 9, 9, colour, 1.5, w)
            c.outline(cx - 2, cy - 3, 9, 9, colour, 1.5, w)
        elif kind == "dock":
            c.outline(cx - 7, cy - 6, 14, 12, colour, 1.5, w)
            c.rect(cx - 7, cy + 1, 14, 5, colour, 1.5)
        elif kind == "marquee":
            x0, y0, x1, y1 = cx - 7, cy - 6, cx + 5, cy + 3
            for i in range(4):  # dashes along each side
                f0, f1 = i / 4, i / 4 + 0.14
                c.line([(x0 + (x1 - x0) * f0, y0), (x0 + (x1 - x0) * f1, y0)], colour, 1.3)
                c.line([(x0 + (x1 - x0) * f0, y1), (x0 + (x1 - x0) * f1, y1)], colour, 1.3)
                c.line([(x0, y0 + (y1 - y0) * f0), (x0, y0 + (y1 - y0) * f1)], colour, 1.3)
                c.line([(x1, y0 + (y1 - y0) * f0), (x1, y0 + (y1 - y0) * f1)], colour, 1.3)
            c.polygon([(cx + 1.5, cy), (cx + 1.5, cy + 8), (cx + 3.6, cy + 6), (cx + 5.2, cy + 8.8),
                       (cx + 6.6, cy + 8.1), (cx + 5, cy + 5.4), (cx + 7.6, cy + 5.2)], colour)
        elif kind == "razor":
            # A blade lying diagonally, with its slot.
            blade = [(cx - 8, cy + 3), (cx + 3, cy - 8), (cx + 8, cy - 3), (cx - 3, cy + 8)]
            c.line(blade + [blade[0]], colour, w)
            c.line([(cx - 2.5, cy + 2.5), (cx + 2.5, cy - 2.5)], colour, 2.4)
        elif kind == "filter":
            c.polygon([(cx - 7, cy - 6), (cx + 7, cy - 6), (cx + 1.5, cy + 0.5), (cx + 1.5, cy + 6.5),
                       (cx - 1.5, cy + 5), (cx - 1.5, cy + 0.5)], colour)
    return draw


def icon_transport(kind, colour):
    def draw(c):
        cx, cy = c.w / 2, c.h / 2
        if kind == "play":
            c.polygon([(cx - 3.5, cy - 5), (cx + 5, cy), (cx - 3.5, cy + 5)], colour)
        elif kind == "stop":
            c.rect(cx - 4.2, cy - 4.2, 8.4, 8.4, colour, 1)
        elif kind == "pause":
            c.rect(cx - 4.5, cy - 5, 3, 10, colour, 0.8)
            c.rect(cx + 1.5, cy - 5, 3, 10, colour, 0.8)
        elif kind == "record":
            c.circle(cx, cy, 5, fill=colour)
        elif kind == "repeat":
            c.line([(cx - 5, cy + 1), (cx - 5, cy - 3), (cx + 4, cy - 3)], colour, 1.5)
            c.polygon([(cx + 3, cy - 6), (cx + 6.5, cy - 3), (cx + 3, cy)], colour)
            c.line([(cx + 5, cy - 1), (cx + 5, cy + 3), (cx - 4, cy + 3)], colour, 1.5)
            c.polygon([(cx - 3, cy + 6), (cx - 6.5, cy + 3), (cx - 3, cy)], colour)
        elif kind == "home":
            c.rect(cx - 5.5, cy - 5, 1.8, 10, colour)
            c.polygon([(cx + 5, cy - 5), (cx - 3, cy), (cx + 5, cy + 5)], colour)
        elif kind == "end":
            c.rect(cx + 3.7, cy - 5, 1.8, 10, colour)
            c.polygon([(cx - 5, cy - 5), (cx + 3, cy), (cx - 5, cy + 5)], colour)
    return draw


# ---- Images ---------------------------------------------------------------------------------------

def button(icon, fw, fh, on_fill=None, radius=3):
    """A sliced button: the suite's control face with an icon, in three frames."""
    def make(scale):
        def one(c, st):
            button_face(c, st, on_fill, radius)
            icon(c)
        return frames(one, fw, fh, scale)
    return make


def slider_track(horizontal, length, thickness, line=3.0):
    """A fader or pan track: a thin #141414 well with rounded ends, the ends kept unstretched."""
    def make(scale):
        w, h = (length, thickness) if horizontal else (thickness, length)
        c = Canvas(w, h, scale)
        if horizontal:
            c.rect(0.5, (h - line) / 2, w - 1, line, WELL, line / 2)
        else:
            c.rect((w - line) / 2, 0.5, line, h - 1, WELL, line / 2)
        img = c.image()
        end = round(3 * scale)
        return with_markers(img, left=end, right=end) if horizontal else with_markers(img, top=end, bottom=end)
    return make


def thumb(horizontal_travel, w, h, accent=TXT):
    """The suite's fader cap: near-black with a line across it. White until per-track colours are
    tested (docs/reaper-theme.md, decisions: knobs and faders)."""
    def make(scale):
        c = Canvas(w, h, scale)
        c.rect(0.5, 0.5, w - 1, h - 1, "#111111", 2)
        c.outline(0.5, 0.5, w - 1, h - 1, "#000000", 2, 1)
        if horizontal_travel:
            c.rect(w / 2 - 0.75, 3, 1.5, h - 6, accent)
        else:
            c.rect(3, h / 2 - 0.75, w - 6, 1.5, accent)
        return c.image()
    return make


def rounded_bg(fill):
    """A stretchable rounded box, radius 3, corners kept unstretched."""
    def make(scale):
        c = Canvas(9, 9, scale)
        c.rect(0, 0, 9, 9, fill, 3)
        r = round(3 * scale)
        return with_markers(c.image(), left=r, top=r, right=r, bottom=r)
    return make


name_field = rounded_bg(WELL)  # the track name's well: #141414


def knob_face(size):
    """The suite's knob face: a disc shaded #4F4F4F to #454545 with a 1 px black 20 % border.
    REAPER draws the pointer on it, in the colour the layout gives (the track's colour)."""
    def make(scale):
        k = scale * SS
        px = round(size * k)
        grad = Image.new("RGBA", (px, px))
        top, bot = rgba(KNOB_TOP), rgba(KNOB_BOT)
        gd = ImageDraw.Draw(grad)
        for y in range(px):
            t = y / max(1, px - 1)
            gd.line([(0, y), (px, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(top, bot)))
        inset = round(1.5 * k)
        mask = Image.new("L", (px, px), 0)
        ImageDraw.Draw(mask).ellipse([inset, inset, px - 1 - inset, px - 1 - inset], fill=255)
        out = Image.new("RGBA", (px, px), (0, 0, 0, 0))
        out.paste(grad, (0, 0), mask)
        ImageDraw.Draw(out).ellipse([inset, inset, px - 1 - inset, px - 1 - inset], outline=(0, 0, 0, 51),
                                    width=max(1, round(k)))
        return out.resize((round(size * scale), round(size * scale)), Image.LANCZOS)
    return make


# Each toolbar icon's accent (user, 21 November 2026: "A + B, but instead of only teal, use other
# colours"): actions draw their icon in it; a toggle that is on fills its button with it.
TOOLBAR_ACCENT = {
    "new": BLUE, "load": BLUE, "save": BLUE, "revert": BLUE, "projprop": PURPLE, "undo": TEAL, "redo": TEAL,
    "metro": ORANGE, "lock": ORANGE, "ripple": YELLOW, "xfade": BLUE, "group": GREEN, "envitem": TEAL,
    "grid": PURPLE, "snap": RED, "relsnap": RED, "dock": BLUE, "filter": PURPLE, "marquee": YELLOW, "razor": RED,
}


def on_ink(fill):
    """The icon colour on a filled button: dark on the light accents (yellow, orange, green), as
    M and S are, white on the others."""
    r, g, b, _ = rgba(fill)
    return HDR if 0.299 * r + 0.587 * g + 0.114 * b > 140 else TXT


def toolbar(kind, toggle=None):
    """A main toolbar button, 30 x 30 frames: an action's icon in its accent; a toggle grey when off
    and filled with its accent when on."""
    accent = TOOLBAR_ACCENT[kind.split("_")[0]]
    if toggle == "on":
        return button(icon_toolbar(kind, on_ink(accent)), 30, 30, on_fill=accent)
    return button(icon_toolbar(kind, TXT2 if toggle == "off" else accent), 30, 30)


B = 20  # the track buttons' frame size at 100 %

IMAGES = {
    # Track panel and mixer buttons (the mixer falls back to these)
    "track_recarm_off": button(icon_recarm(False), B, B),
    "track_recarm_on": button(icon_recarm(True), B, B),
    "track_mute_off": button(icon_text("M", 9, "#D6D6D6"), B, B),
    "track_mute_on": button(icon_text("M", 9, HDR), B, B, on_fill=ORANGE),
    "track_solo_off": button(icon_text("S", 9, "#D6D6D6"), B, B),
    "track_solo_on": button(icon_text("S", 9, HDR), B, B, on_fill=YELLOW),
    "track_phase_norm": button(icon_phase(TXT2), B, B),
    "track_phase_inv": button(icon_phase(TEAL), B, B),
    "track_monitor_off": button(icon_monitor(TXT2), B, B),
    "track_monitor_on": button(icon_monitor(TEAL), B, B),
    "track_monitor_auto": button(icon_monitor(TEAL, auto=True), B, B),
    "track_env": button(icon_env(TXT2), B, B),
    "track_env_read": button(icon_env("#2DCC70"), B, B),
    "track_env_touch": button(icon_env(YELLOW), B, B),
    "track_env_latch": button(icon_env("#9A59B5"), B, B),
    "track_env_write": button(icon_env(RED), B, B),
    "track_fx_norm": button(icon_fx(TEAL), 24, B),
    "track_fx_empty": button(icon_fx(TXT2), 24, B),
    "track_fx_dis": button(icon_fx(TXT2, struck=True), 24, B),
    "track_io": button(icon_io(False, False, "#D6D6D6"), 24, B),
    "track_io_r": button(icon_io(True, False, "#D6D6D6"), 24, B),
    "track_io_s": button(icon_io(False, True, "#D6D6D6"), 24, B),
    "track_io_s_r": button(icon_io(True, True, "#D6D6D6"), 24, B),
    "track_io_dis": button(icon_io(False, False, G6), 24, B),
    "track_io_r_dis": button(icon_io(True, False, G6), 24, B),
    "track_io_s_dis": button(icon_io(False, True, G6), 24, B),
    "track_io_s_r_dis": button(icon_io(True, True, G6), 24, B),
    "track_recmode_in": button(icon_text("IN", 7.5, "#D6D6D6"), 24, B),
    "track_recmode_out": button(icon_text("OUT", 7.5, "#D6D6D6"), 24, B),
    "track_recmode_off": button(icon_text("OFF", 7.5, TXT2), 24, B),
    "track_folder_off": button(icon_folder("off"), 18, 14),
    "track_folder_on": button(icon_folder("on"), 18, 14),
    "track_folder_last": button(icon_folder("last"), 18, 14),
    "track_mono": button(icon_channels(False, TEAL), B, B),
    "track_stereo": button(icon_channels(True, TXT2), B, B),
    "mixer_menu": button(icon_menu("#D6D6D6"), B, B),
    "track_fx_in_norm": button(icon_fx(TEAL), 20, B),
    "track_fx_in_empty": button(icon_fx(TXT2), 20, B),
    "track_fxon_v": button(icon_fxbypass("on"), 8, B, radius=2),
    "track_fxoff_v": button(icon_fxbypass("off"), 8, B, radius=2),
    "track_fxempty_v": button(icon_fxbypass("empty"), 8, B, radius=2),
    "track_fxon_h": button(icon_fxbypass("on"), 16, B),
    "track_fxoff_h": button(icon_fxbypass("off"), 16, B),
    "track_fxempty_h": button(icon_fxbypass("empty"), 16, B),

    # The record input box (the mixer falls back to it). REAPER draws its text and arrow on top.
    "tcp_recinput": rounded_bg(BODY),

    # Faders and pan
    "tcp_volbg": slider_track(True, 40, 14),
    "tcp_volthumb": thumb(True, 8, 14),
    "tcp_panbg": slider_track(True, 30, 14),
    "tcp_panthumb": thumb(True, 8, 12),
    "mcp_volbg": slider_track(False, 40, 14),
    "mcp_volthumb": thumb(False, 22, 10),
    "mcp_panbg": slider_track(True, 30, 12),
    "mcp_panthumb": thumb(True, 8, 12),

    # Knob faces (Stage C's knobs; every knob falls back to these)
    "gen_knob_bg_small": knob_face(20),
    "gen_knob_bg_large": knob_face(32),

    # Name fields
    "tcp_namebg": name_field,
    # Transparent, so the layout's background colour (the track's colour) fills the mixer's name bar
    "mcp_namebg": rounded_bg((0, 0, 0, 0)),

    # Transport
    "transport_play": button(icon_transport("play", "#E0E0E0"), 34, 26),
    "transport_play_on": button(icon_transport("play", TXT), 34, 26, on_fill=TEAL),
    "transport_stop": button(icon_transport("stop", "#E0E0E0"), 34, 26),
    "transport_pause": button(icon_transport("pause", "#E0E0E0"), 34, 26),
    "transport_pause_on": button(icon_transport("pause", TXT), 34, 26, on_fill=TEAL),
    "transport_record": button(icon_transport("record", RED), 34, 26),
    "transport_record_on": button(icon_transport("record", TXT), 34, 26, on_fill=RED),
    "transport_repeat_off": button(icon_transport("repeat", "#E0E0E0"), 34, 26),
    "transport_repeat_on": button(icon_transport("repeat", TXT), 34, 26, on_fill=TEAL),
    "transport_home": button(icon_transport("home", "#E0E0E0"), 34, 26),
    "transport_end": button(icon_transport("end", "#E0E0E0"), 34, 26),
    "transport_playspeedbg": slider_track(True, 40, 12),
    "transport_playspeedthumb": thumb(True, 8, 14),

    # Main toolbar
    "toolbar_blank": button(lambda c: None, 30, 30),
    "toolbar_new": toolbar("new"),
    "toolbar_load": toolbar("load"),
    "toolbar_save": toolbar("save"),
    "toolbar_projprop": toolbar("projprop"),
    "toolbar_undo": toolbar("undo"),
    "toolbar_redo": toolbar("redo"),
    "toolbar_revert": toolbar("revert"),
    "toolbar_ripple_off": toolbar("ripple_off", "off"),
    "toolbar_ripple_one": toolbar("ripple_one", "on"),
    "toolbar_ripple_all": toolbar("ripple_all", "on"),
    **{f"toolbar_{k}_{s}": toolbar(k, s) for k in ("metro", "grid", "snap", "relsnap", "xfade", "envitem",
                                                     "group", "dock", "filter") for s in ("on", "off")},
    "toolbar_lock_on": toolbar("lock_on", "on"),
    "toolbar_lock_off": toolbar("lock_off", "off"),
    # These two live in REAPER's Data/toolbar_icons rather than its theme images; the theme's copy
    # is meant to take precedence (to be confirmed in REAPER).
    "toolbar_marquee_cursor_selection_on": toolbar("marquee", "on"),
    "toolbar_marquee_cursor_selection_off": toolbar("marquee", "off"),
    "toolbar_razor_on": toolbar("razor", "on"),
    "toolbar_razor_off": toolbar("razor", "off"),
}

SCALES = {"": 1.0, "150": 1.5, "200": 2.0}


def render(folder):
    """Writes every image at every scale into folder (100 %), folder/150 and folder/200."""
    folder = pathlib.Path(folder)
    count = 0
    for sub, scale in SCALES.items():
        out = folder / sub if sub else folder
        out.mkdir(parents=True, exist_ok=True)
        for name, make in IMAGES.items():
            if sub and name in ONLY_100:
                continue
            make(scale).save(out / f"{name}.png", optimize=True)
            count += 1
    return count


# ---- Meters (Stage C) ---------------------------------------------------------------------------
# REAPER's meter images (reaper.fm/sdk/walter/images.php, "Meter images"): slices of a dim (unlit)
# and a lit meter per channel, with a fixed 32 px clip-indicator end. The user's gradient (21
# November 2026): red at the very top, through yellow, to green at the bottom; the clip end red.
# Solid, without REAPER's stripes. The unlit part is the well with a faint trace of the gradient.
# They are drawn at 100 % only: the clip end is a fixed 32 px and REAPER stretches the rest.

METER_STOPS = [(0.0, "#00FF96"), (0.55, "#FFCF07"), (1.0, RED)]  # bottom (0) to top (1)
CLIP_LIT, CLIP_DIM = RED, "#3A1E1B"
METER_DIM_MIX = 0.13  # how much of the gradient shows in the unlit part


def meter_colour(t, lit=True, strength=1.0):
    """The gradient at t (0 bottom, 1 top), lit or dim, over the well."""
    for (t0, c0), (t1, c1) in zip(METER_STOPS, METER_STOPS[1:]):
        if t <= t1:
            f = (t - t0) / (t1 - t0) if t1 > t0 else 0
            a, b = rgba(c0), rgba(c1)
            col = [a[i] + (b[i] - a[i]) * f for i in range(3)]
            break
    well = rgba(WELL)
    mix = strength if lit else METER_DIM_MIX
    return tuple(round(well[i] + (col[i] - well[i]) * mix) for i in range(3)) + (255,)


def meter_slice_vertical(w, h, lit, strength=1.0):
    """One vertical slice: the 32 px clip end at the top, the gradient below it."""
    img = Image.new("RGBA", (w, h))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w - 1, 31], fill=rgba(CLIP_LIT if lit else CLIP_DIM))
    body = h - 32
    for y in range(body):
        d.line([(0, 32 + y), (w - 1, 32 + y)], fill=meter_colour(1 - y / max(1, body - 1), lit, strength))
    return img


def meter_slice_horizontal(w, h, lit):
    """One horizontal slice: the gradient from green at the left, the 32 px clip end at the right."""
    img = Image.new("RGBA", (w, h))
    d = ImageDraw.Draw(img)
    body = w - 32
    for x in range(body):
        d.line([(x, 0), (x, h - 1)], fill=meter_colour(x / max(1, body - 1), lit))
    d.rectangle([body, 0, w - 1, h - 1], fill=rgba(CLIP_LIT if lit else CLIP_DIM))
    return img


def mcp_vu(scale):
    """Mixer meter: 4 slices side by side (left dim, right dim, left lit, right lit)."""
    sw, h = 8, 288
    img = Image.new("RGBA", (sw * 4, h))
    for i, lit in enumerate((False, False, True, True)):
        img.paste(meter_slice_vertical(sw, h, lit), (i * sw, 0))
    return img


def mcp_master_vu(scale):
    """Master meter: 8 slices (left RMS, left peak, right RMS, right peak; dim, then lit). RMS is a
    little darker than peak, so the two read apart."""
    sw, h = 8, 288
    img = Image.new("RGBA", (sw * 8, h))
    order = [(False, 0.75), (False, 1), (False, 0.75), (False, 1), (True, 0.75), (True, 1), (True, 0.75), (True, 1)]
    for i, (lit, strength) in enumerate(order):
        img.paste(meter_slice_vertical(sw, h, lit, strength), (i * sw, 0))
    return img


def tcp_vu(scale):
    """Track panel meter: 8 horizontal slices stacked (left dim, right dim, left lit, right lit;
    then the same while record armed)."""
    w, sh = 288, 8
    img = Image.new("RGBA", (w, sh * 8))
    for i, lit in enumerate((False, False, True, True) * 2):
        img.paste(meter_slice_horizontal(w, sh, lit), (0, i * sh))
    return img


METERS = {"mcp_vu": mcp_vu, "mcp_master_vu": mcp_master_vu, "tcp_vu": tcp_vu}
IMAGES.update(METERS)
ONLY_100 = set(METERS)
