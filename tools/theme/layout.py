"""Stage C layouts for the Fancy Plugs REAPER theme (docs/reaper-theme.md, section 3).

Writes the WALTER part of rtconfig.txt, from REAPER's public WALTER reference
(reaper.fm/sdk/walter/walter.php) and the approved mockup (design/reaper-theme-mockup/index.html).
Nothing is taken from REAPER's own themes.

Coordinates are in 100 % units: REAPER scales the layout with the display (at the user's 125 %, a
20 px button is drawn 25 px), and rtconfig's misc_dpi_translate picks the 150 % images.

A coordinate list is [x y w h left-attach top-attach right-attach bottom-attach]. Positions are
written for the baseline panel, W x H below; the attach values move or stretch an element when the
user makes the panel wider or the track taller.

Done so far: the track panel, then the mixer strip and the master strip. The transport keeps
REAPER's built-in layout (the user's screenshot showed REAPER keeps its own for any part a theme
leaves out).
"""

W, H = 300, 54  # the mockup's track panel at 100 %

# Columns, from the mockup: a 5 px colour strip, the track number, then two rows of controls, and a
# thin stereo meter on the right edge.
STRIP = 5
IDX_X, IDX_W = 5, 22
ROW1, ROW2, ROW_H = 5, 29, 20
X0 = 29  # first control column

# Row 1, right to left from the meter: FX chain switch, FX, S, M; the name fills what's left.
METER = [284, 6, 12, 42, 1, 0, 1, 1]
FXBYP = [272, ROW1, 8, ROW_H, 1, 0, 1, 0]
FX = [246, ROW1, 24, ROW_H, 1, 0, 1, 0]
SOLO = [222, ROW1, 20, ROW_H, 1, 0, 1, 0]
MUTE = [198, ROW1, 20, ROW_H, 1, 0, 1, 0]
RECARM = [X0, ROW1, 20, ROW_H]
NAME = [53, ROW1, 141, ROW_H, 0, 0, 1, 0]

# Row 2: volume knob, its value, pan knob; then, when the track isn't armed, envelope and polarity,
# and when it is, the input box (stretching), monitoring, record mode and input FX. IO under FX.
VOL = [X0, ROW2, 20, ROW_H]
VOL_LABEL = [51, ROW2, 42, ROW_H]
PAN = [95, ROW2, 20, ROW_H]
ENV = [119, ROW2, 20, ROW_H]
PHASE = [143, ROW2, 20, ROW_H]
RECINPUT = [119, ROW2, 51, ROW_H, 0, 0, 1, 0]
RECMON = [174, ROW2, 20, ROW_H, 1, 0, 1, 0]
RECMODE = [198, ROW2, 20, ROW_H, 1, 0, 1, 0]
FXIN = [222, ROW2, 20, ROW_H, 1, 0, 1, 0]
IO = [246, ROW2, 24, ROW_H, 1, 0, 1, 0]

# Below the track number: the folder button.
FOLDER = [IDX_X + 2, 33, 18, 14]

ROW2_MIN_H = 50  # shorter tracks show row 1 only

GREY = "141 141 141 255"  # #8D8D8D
WHITE = "255 255 255 255"


def coords(c):
    return "[" + " ".join(str(v) for v in c) + "]"


def tcp():
    out = [
        "; ---- Track panel (tcp) ----",
        "clear tcp.*",
        f"set tcp.size [{W} {H} 160 24]",
        "",
        "; The track's colour as a strip on the left edge (a text element with no text, drawn in its",
        "; background colour); nothing when the track has no colour.",
        'custom tcp.custom.strip ""',
        f"set tcp.custom.strip [0 0 {STRIP} {H} 0 0 0 1]",
        "set tcp.custom.strip.color trackcolor_valid==1 [0 0 0 0 trackcolor_r trackcolor_g trackcolor_b 255] "
        "[0 0 0 0 0 0 0 0]",
        "",
        f"set tcp.trackidx [{IDX_X} 0 {IDX_W} 27]",
        f"set tcp.trackidx.color [{GREY} 0 0 0 0]",
        "set tcp.trackidx.margin [0 0 0 0 0.5]",
        f"set tcp.folder h<{ROW2_MIN_H} [0] {coords(FOLDER)}",
        "",
        "; Row 1",
        f"set tcp.recarm {coords(RECARM)}",
        f"set tcp.label {coords(NAME)}",
        f"set tcp.label.color [{WHITE} 0 0 0 0]",
        "set tcp.label.margin [7 0 4 0 0]",
        f"set tcp.mute {coords(MUTE)}",
        f"set tcp.solo {coords(SOLO)}",
        f"set tcp.fx {coords(FX)}",
        f"set tcp.fxbyp {coords(FXBYP)}",
        f"set tcp.meter {coords(METER)}",
        "set tcp.meter.vu.div [2]",
        "",
        "; Row 2: knobs. Their pointer takes the track's colour, white with no colour (mockup A2; the",
        "; arc isn't drawn by REAPER's knobs).",
        "set tcp.volume.fadermode [1]",
        "set tcp.pan.fadermode [1]",
        f"set tcp.volume h<{ROW2_MIN_H} [0] {coords(VOL)}",
        f"set tcp.pan h<{ROW2_MIN_H} [0] {coords(PAN)}",
        "set tcp.volume.color trackcolor_valid==1 [trackcolor_r trackcolor_g trackcolor_b 255 "
        "trackcolor_r trackcolor_g trackcolor_b 255] "
        f"[{WHITE} {WHITE}]",
        "set tcp.pan.color tcp.volume.color",
        f"set tcp.volume.label h<{ROW2_MIN_H} [0] {coords(VOL_LABEL)}",
        f"set tcp.volume.label.color [{GREY} 0 0 0 0]",
        "set tcp.volume.label.margin [0 0 0 0 0.5]",
        "",
        "; Row 2, not armed: envelope and polarity",
        f"set tcp.env recarm!=0 [0] h<{ROW2_MIN_H} [0] {coords(ENV)}",
        f"set tcp.phase recarm!=0 [0] h<{ROW2_MIN_H} [0] {coords(PHASE)}",
        "",
        "; Row 2, armed: input, monitoring, record mode, input FX",
        f"set tcp.recinput recarm==0 [0] h<{ROW2_MIN_H} [0] {coords(RECINPUT)}",
        "set tcp.recinput.color [207 207 207 255 48 48 48 255]",
        "set tcp.recinput.margin [6 0 4 0 0]",
        f"set tcp.recmon recarm==0 [0] h<{ROW2_MIN_H} [0] {coords(RECMON)}",
        f"set tcp.recmode recarm==0 [0] h<{ROW2_MIN_H} [0] {coords(RECMODE)}",
        f"set tcp.fxin recarm==0 [0] h<{ROW2_MIN_H} [0] {coords(FXIN)}",
        "",
        f"set tcp.io h<{ROW2_MIN_H} [0] {coords(IO)}",
    ]
    return out


# ---- Mixer strip (mcp), from the mockup: 78 px wide. Top to bottom: the FX list (when the user
# shows it), the pan knob, the fader and meter with a column of small buttons beside them, the
# volume value (or the input box when armed), M S and record arm, and the name on a bar in the
# track's colour. Positions are for a strip MH tall, the user's mixer at 100 % (about 240 px);
# everything from the volume value down is attached to the bottom edge, and the fader and meter
# stretch with the strip's height.
MW, MH = 78, 240
FXLIST_H = 50  # the FX list's room at the top, when shown
TOP = 4  # the pan knob's top without the FX list
FADER_TOP = 32  # the fader's top without the FX list
BOTTOM_ROWS = 176  # where the bottom rows start, on the MH baseline


def bottom(x, y, w, h, stretch_w=False):
    """Attached to the bottom edge (y measured on the MH-tall baseline)."""
    return [x, y, w, h, 0, 1, 1 if stretch_w else 0, 1]


def knob_colour(prefix, element):
    return (f"set {prefix}.{element}.color trackcolor_valid==1 [trackcolor_r trackcolor_g trackcolor_b 255 "
            f"trackcolor_r trackcolor_g trackcolor_b 255] [{WHITE} {WHITE}]")


def below_fxlist(element, c, grow=False):
    """An element under the FX list: moved down by the list's height when the user shows it (and
    shortened by it, for the fader and meter, so their bottom stays put)."""
    shift = "fxgrow" if grow else "fxshift"
    return f"set {element} + {coords(c)} {shift}"


def strip(prefix, width, col_x, armed_rows=True):
    """The parts the track and master strips share: FX list, pan knob, fader, meter, the top of the
    button column."""
    col_w = 24
    fader_h = BOTTOM_ROWS - 4 - FADER_TOP
    return [
        f"set {prefix}.extmixer.mode [1]",
        f"set {prefix}.extmixer.position [4 4 {width - 8} {FXLIST_H - 4} 0 0 1 0]",
        # "clear" also empties the lists' fonts, which leaves them with no rows: the label font at
        # 13 px rows, small side margins.
        f"set {prefix}.fxlist.font [0 13]",
        f"set {prefix}.fxlist.margin [2 0 2 0 0]",
        f"set {prefix}.sendlist.font [0 13]",
        f"set {prefix}.sendlist.margin [2 0 2 0 0]",
        f"set {prefix}.fxparm.font [0 13]",
        f"set {prefix}.fxparm.margin [2 0 2 0 0]",
        f"set {prefix}.pan.fadermode [1]",
        below_fxlist(f"{prefix}.pan", [width // 2 - 12 if not armed_rows else 27, TOP, 24, 24]),
        f"set {prefix}.volume.fadermode [-1]",
        below_fxlist(f"{prefix}.volume", [6, FADER_TOP, 14, fader_h, 0, 0, 0, 1], grow=True),
        f"set {prefix}.meter.vu.div [2]",
        below_fxlist(f"{prefix}.fx", [col_x, FADER_TOP, col_w, 18]),
        below_fxlist(f"{prefix}.fxbyp", [col_x, FADER_TOP + 20, col_w, 8]),
        below_fxlist(f"{prefix}.io", [col_x, FADER_TOP + 30, col_w, 18]),
    ]


def mcp():
    col_x = 48
    rows = BOTTOM_ROWS
    return [
        "; ---- Mixer strip (mcp) ----",
        "; fxshift/fxgrow move what's under the FX list down by its height when the user shows it.",
        f"set fxshift mcp_wantextmix&1 [0 {FXLIST_H}] [0 0]",
        f"set fxgrow mcp_wantextmix&1 [0 {FXLIST_H} 0 -{FXLIST_H}] [0 0]",
        "clear mcp.*",
        f"set mcp.size [{MW} {MH} 60 200]",
        *strip("mcp", MW, col_x),
        knob_colour("mcp", "pan"),
        below_fxlist("mcp.folder", [58, TOP + 5, 16, 14]),
        below_fxlist("mcp.meter", [24, FADER_TOP, 18, rows - 4 - FADER_TOP, 0, 0, 0, 1], grow=True),
        "",
        "; The rest of the button column: envelope and polarity, or monitoring and input FX when armed",
        "set mcp.env recarm!=0 [0] + " + coords([col_x, FADER_TOP + 50, 24, 18]) + " fxshift",
        "set mcp.phase recarm!=0 [0] + " + coords([col_x, FADER_TOP + 70, 24, 18]) + " fxshift",
        "set mcp.recmon recarm==0 [0] + " + coords([col_x, FADER_TOP + 50, 24, 18]) + " fxshift",
        "set mcp.fxin recarm==0 [0] + " + coords([col_x, FADER_TOP + 70, 24, 18]) + " fxshift",
        "",
        "; The volume value, or the input box when armed",
        f"set mcp.volume.label recarm!=0 [0] {coords(bottom(4, rows + 1, MW - 8, 14, True))}",
        f"set mcp.volume.label.color [{GREY} 0 0 0 0]",
        "set mcp.volume.label.margin [0 0 0 0 0.5]",
        f"set mcp.recinput recarm==0 [0] {coords(bottom(4, rows, MW - 8, 17, True))}",
        "set mcp.recinput.color [207 207 207 255 48 48 48 255]",
        "set mcp.recinput.margin [4 0 2 0 0]",
        "",
        "; M, S, record arm",
        f"set mcp.mute {coords(bottom(6, rows + 20, 21, 18))}",
        f"set mcp.solo {coords(bottom(30, rows + 20, 21, 18))}",
        f"set mcp.recarm {coords(bottom(54, rows + 20, 18, 18))}",
        "",
        "; The name on a bar in the track's colour (dark text; white on near-black with no colour),",
        "; with the track number at its left. The bar is a custom element, as the track panel's strip:",
        "; a label's own background comes out blended at about a third of the colour.",
        'custom mcp.custom.namebar ""',
        f"set mcp.custom.namebar {coords(bottom(0, MH - 22, MW, 22, True))}",
        "set mcp.custom.namebar.color trackcolor_valid==1 [0 0 0 0 trackcolor_r trackcolor_g trackcolor_b 255] "
        "[0 0 0 0 20 20 20 255]",
        f"set mcp.label {coords(bottom(0, MH - 22, MW, 22, True))}",
        "set mcp.label.color trackcolor_valid==1 [20 20 20 255 0 0 0 0] [255 255 255 255 0 0 0 0]",
        "front mcp.label",
        "front mcp.trackidx",
        "set mcp.label.margin [16 0 2 0 0.5]",
        f"set mcp.trackidx {coords(bottom(0, MH - 22, 16, 22))}",
        "set mcp.trackidx.color trackcolor_valid==1 [20 20 20 170 0 0 0 0] [141 141 141 255 0 0 0 0]",
        "set mcp.trackidx.margin [3 0 0 0 0.5]",
    ]


# ---- Master strip: 104 px wide, the same order with a wider meter, the mixer's menu button in the
# column, and M, S and mono at the bottom. The master's knob pointer and name bar are teal
# (decision A2).
MMW = 104


def master_mcp():
    col_x = 74
    rows = BOTTOM_ROWS
    return [
        "; ---- Master mixer strip (master.mcp) ----",
        "clear master.mcp.*",
        f"set master.mcp.size [{MMW} {MH} 80 200]",
        *strip("master.mcp", MMW, col_x, armed_rows=False),
        "set master.mcp.pan.color [0 169 133 255 0 169 133 255]",
        below_fxlist("master.mcp.meter", [26, FADER_TOP, 44, rows - 4 - FADER_TOP, 0, 0, 0, 1], grow=True),
        below_fxlist("master.mcp.env", [col_x, FADER_TOP + 50, 24, 18]),
        below_fxlist("master.mcp.menubutton", [col_x, FADER_TOP + 70, 24, 18]),
        f"set master.mcp.mute {coords(bottom(MMW // 2 - 34, rows + 20, 21, 18))}",
        f"set master.mcp.solo {coords(bottom(MMW // 2 - 10, rows + 20, 21, 18))}",
        f"set master.mcp.mono {coords(bottom(MMW // 2 + 14, rows + 20, 21, 18))}",
        f"set master.mcp.label {coords(bottom(0, MH - 22, MMW, 22, True))}",
        "set master.mcp.label.color [255 255 255 255 0 169 133 255]",
        "set master.mcp.label.margin [0 0 0 0 0.5]",
    ]


def rtconfig_walter():
    return "\n".join(tcp() + [""] + mcp() + [""] + master_mcp()) + "\n"
