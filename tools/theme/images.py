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
KNOB_TOP, KNOB_BOT = "#4F4F4F", "#454545"


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
        fill = rgba({"normal": BODY, "hover": HOVER, "pressed": PRESSED}[st])
    c.rect(0.5, 0.5, c.w - 1, c.h - 1, fill, radius)


# ---- Icons, drawn in a frame of the canvas's size -------------------------------------------------

def icon_text(label, size=9.0, colour=TXT, weight="SemiBold"):
    def draw(c):
        c.text(c.w / 2, c.h / 2 + 0.3, label, size, colour, weight)
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
            c.polygon(shape, G6)
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


def name_field(scale):
    """The track name's well: #141414, radius 3, corners kept unstretched."""
    c = Canvas(9, 9, scale)
    c.rect(0, 0, 9, 9, WELL, 3)
    r = round(3 * scale)
    return with_markers(c.image(), left=r, top=r, right=r, bottom=r)


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

    # Faders and pan
    "tcp_volbg": slider_track(True, 40, 14),
    "tcp_volthumb": thumb(True, 8, 14),
    "tcp_panbg": slider_track(True, 30, 14),
    "tcp_panthumb": thumb(True, 8, 12),
    "mcp_volbg": slider_track(False, 40, 14),
    "mcp_volthumb": thumb(False, 22, 10),
    "mcp_panbg": slider_track(True, 30, 12),
    "mcp_panthumb": thumb(True, 8, 12),

    # Name fields
    "tcp_namebg": name_field,
    "mcp_namebg": name_field,

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
            make(scale).save(out / f"{name}.png", optimize=True)
            count += 1
    return count
