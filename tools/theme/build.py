"""Builds the Fancy Plugs theme for REAPER, Stage A: colours and fonts (docs/reaper-theme.md).

    python tools/theme/build.py

Writes theme/Fancy Plugs.ReaperTheme. Every value comes from docs/DESIGN_SYSTEM.md and the palette
in docs/reaper-theme.md section 4. Key names are REAPER's .ReaperTheme format; no values are taken
from REAPER's own themes or from the Reapertips reference.

Stage A sets colours and fonts only, over REAPER's built-in images and layouts (no ui_img). Keys
left out keep REAPER's defaults: blend modes (*_drawmode, *_mode), group colours, score colours.
"""

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "theme" / "Fancy Plugs.ReaperTheme"

# docs/DESIGN_SYSTEM.md
HDR, BODY, SEC, WELL = "#1E1E1E", "#303030", "#262626", "#141414"
G5, G6 = "#424242", "#4C4C4C"
TXT, TXT2, MENU_TXT = "#FFFFFF", "#8D8D8D", "#EDEDED"
TEAL, TEAL_DARK, RED = "#00A985", "#00755C", "#E74D3D"
METER_TOP, METER_BOT = "#FFCF07", "#00FF96"
HOVER, BORDER = "#383838", "#434343"

# docs/reaper-theme.md section 4: twelve hues in rainbow order
RED_, ORANGE, YELLOW, LIME, GREEN, TEAL_, CYAN, BLUE, INDIGO, PURPLE, MAGENTA, PINK = (
    "#E74D3D", "#E77E23", "#F1C315", "#A4C639", "#2DCC70", "#00A985",
    "#1BB5D4", "#3598DB", "#5864D6", "#9A59B5", "#C94FA6", "#E8608A")
RAINBOW = [RED_, ORANGE, YELLOW, LIME, GREEN, TEAL_, CYAN, BLUE, INDIGO, PURPLE, MAGENTA, PINK]

# The arrange view, as in the mockup (design/reaper-theme-mockup): lanes a step darker than the
# track panels, grid lines in the suite's greys.
ARR, LANE1, LANE2 = "#1B1B1B", "#1D1D1D", "#1B1B1B"

COLOURS = {
    # Main window, toolbars, dockers
    "col_main_bg": HDR,
    "col_main_bg2": HDR,
    "col_main_text": TXT,
    "col_main_text2": TXT2,
    "col_main_textshadow": WELL,
    "col_main_3dhl": G5,
    "col_main_3dsh": WELL,
    "col_main_resize": WELL,
    "col_main_resize2": WELL,
    "col_main_editbk": WELL,
    "col_transport_editbk": WELL,
    "col_buttonbg": BODY,
    "col_toolbar_text": "#CFCFCF",
    "col_toolbar_text_on": TEAL,
    "col_toolbar_frame": WELL,
    "toolbararmed_color": RED,
    "io_text": MENU_TXT,
    "io_3dhl": G5,
    "io_3dsh": WELL,
    "docker_bg": HDR,
    "docker_shadow": WELL,
    "docker_selface": BODY,
    "docker_unselface": SEC,
    "docker_text": TXT2,
    "docker_text_sel": TXT,
    "windowtab_bg": SEC,

    # Lists (FX browser, media explorer, project bay, ...)
    "genlist_bg": SEC,
    "genlist_fg": MENU_TXT,
    "genlist_grid": BODY,
    "genlist_selbg": TEAL_DARK,
    "genlist_selfg": TXT,
    "genlist_seliabg": HOVER,
    "genlist_seliafg": MENU_TXT,
    "genlist_hilite": TEAL,
    "genlist_hilite_sel": "#9FE6D6",
    "col_explorer_sel": TEAL_DARK,
    "col_explorer_seledge": TEAL,
    "explorer_grid": G5,
    "explorer_pitchtext": TXT,

    # Track panels and mixer
    "col_tracklistbg": SEC,
    "col_mixerbg": BODY,
    "col_tcp_text": TXT,
    "col_tcp_textsel": TXT,
    "col_seltrack": HOVER,
    "col_seltrack2": "#2B2B2B",
    "tcplocked_color": WELL,
    "tcp_list_scrollbar": G6,
    "tcp_list_scrollbar_mouseover": TEAL,
    "mcp_list_scrollbar": G6,
    "mcp_list_scrollbar_mouseover": TEAL,
    "mcp_fx_normal": MENU_TXT,
    "mcp_fx_bypassed": "#6A6A6A",
    "mcp_fx_offlined": RED,
    "mcp_fxparm_normal": MENU_TXT,
    "mcp_fxparm_bypassed": "#6A6A6A",
    "mcp_fxparm_offlined": RED,
    "mcp_sends_normal": BLUE,
    "mcp_sends_muted": RED,
    "mcp_sends_levels": "#1B3A50",
    "mcp_send_midihw": PURPLE,
    "tcp_pinned_track_gap": WELL,
    "pinned_track_gap": WELL,
    "tcp_pinned_track_gap_unreachable": RED,

    # Arrange: background, lanes, grid
    "col_arrangebg": ARR,
    "col_tr1_bg": LANE1,
    "col_tr2_bg": LANE2,
    "selcol_tr1_bg": "#262626",
    "selcol_tr2_bg": "#242424",
    "col_tr1_divline": WELL,
    "col_tr2_divline": WELL,
    "col_envlane1_divline": WELL,
    "col_envlane2_divline": WELL,
    "col_gridlines": "#353535",
    "col_gridlines2": "#2A2A2A",
    "col_gridlines3": "#232323",
    "arrange_vgrid": "#2A2A2A",
    "track_lane_tabcol": G6,
    "track_lanesolo_tabcol": YELLOW,
    "track_lanesolo_text": TXT,
    "track_lane_gutter": G6,

    # Items (uncoloured tracks; coloured ones take the track colour)
    "col_mi_bg": G6,
    "col_mi_bg2": G5,
    "col_tr1_itembgsel": "#6A6A6A",
    "col_tr2_itembgsel": "#6A6A6A",
    "col_tr1_peaks": "#CFCFCF",
    "col_tr2_peaks": "#CFCFCF",
    "col_tr1_ps2": TXT,
    "col_tr2_ps2": TXT,
    "col_peaksedge": WELL,
    "col_peaksedge2": WELL,
    "col_peaksedgesel": WELL,
    "col_peaksedgesel2": WELL,
    "col_mi_label": TXT,
    "col_mi_label_sel": TXT,
    "col_mi_label_float": TXT2,
    "col_mi_label_float_sel": MENU_TXT,
    "col_mi_fades": YELLOW,
    "col_mi_fade2": TXT,
    "selitem_tag": TXT,
    "selitem_dot": TXT,
    "activetake_tag": TXT,
    "item_grouphl": TEAL,
    "take_marker": YELLOW,
    "take_marker_sel": TXT,
    "playrate_edited": RED,
    "col_offlinetext": RED,
    "mute_overlay_col": WELL,
    "inactive_take_overlay_col": HDR,
    "auto_item_unsel": TXT2,
    "autogroup": TXT,
    "col_stretchmarker": BODY,
    "col_stretchmarker_h0": MENU_TXT,
    "col_stretchmarker_h1": TEAL,
    "col_stretchmarker_h2": RED,
    "col_stretchmarker_text": TXT,
    "col_stretchmarker_tm": GREEN,

    # Cursors, selections, fades
    "col_cursor": TXT,
    "col_cursor2": TXT,
    "playcursor_color": TEAL,
    "col_repos": TEAL,
    "guideline_color": TEAL,
    "mouseitem_color": TEAL,
    "marquee_fill": TEAL,
    "marquee_outline": TEAL,
    "areasel_fill": TEAL,
    "areasel_outline": TEAL,
    "col_fadearm": RED,
    "col_fadearm2": TEAL,
    "col_fadearm3": YELLOW,
    "fadearea_color": YELLOW,
    "fadezone_color": RED,

    # Timeline, markers, regions, tempo
    "col_tl_bg": HDR,
    "col_tl_fg": TXT2,
    "col_tl_fg2": G6,
    "col_tl_bgsel": TEAL,
    "col_tl_bgsel2": TEAL_DARK,
    "ruler_header_text": TXT2,
    "marker": BLUE,
    "marker_edge": TXT2,
    "marker_edge_sel": TXT,
    "marker_lane_bg": HDR,
    "marker_lane_text": MENU_TXT,
    "region": PURPLE,
    "region_edge": TXT2,
    "region_edge_sel": TXT,
    "region_lane_bg": HDR,
    "region_lane_text": MENU_TXT,
    "ts_lane_bg": HDR,
    "ts_lane_text": MENU_TXT,
    "timesig_sel_bg": TEAL_DARK,
    "col_tsigmark": TXT2,

    # Meters: the suite's gradient, top to bottom, with the red clip cap
    "col_vutop": METER_TOP,
    "col_vumid": "#80E74E",
    "col_vubot": METER_BOT,
    "col_vuclip": RED,
    "col_vuintcol": WELL,
    "col_vudoint": WELL,
    "col_vumidi": "#CFCFCF",
    "col_vudbscale": TXT2,
    "col_vudbscale2": TXT2,
    "vu_gr_bgcol": WELL,
    "vu_gr_fgcol": METER_TOP,

    # Transport
    "col_trans_bg": HDR,
    "col_trans_fg": TXT,

    # Envelopes: the rainbow, with the item envelopes on their usual hues
    "env_item_vol": GREEN,
    "env_item_pan": ORANGE,
    "env_item_mute": RED,
    "env_item_pitch": CYAN,
    "env_item_sampleedit": PINK,
    "env_trim_vol": TEAL,
    "env_track_mute": RED,
    "env_sends_mute": RED,

    # Routing and wiring
    "col_routingact": TEAL,
    "col_routinghl1": TXT,
    "col_routinghl2": ORANGE,
    "wiring_grid": SEC,
    "wiring_grid2": "#222222",
    "wiring_tbg": SEC,
    "wiring_border": G5,
    "wiring_activity": TEAL,

    # MIDI editor
    "midi_trackbg1": SEC,
    "midi_trackbg2": "#222222",
    "midi_trackbg_outer1": HDR,
    "midi_trackbg_outer2": ARR,
    "midi_inline_trackbg1": SEC,
    "midi_inline_trackbg2": "#222222",
    "midi_rulerbg": HDR,
    "midi_rulerfg": TXT2,
    "midi_leftbg": SEC,
    "midi_pkey1": "#D9D9D9",
    "midi_pkey2": HDR,
    "midi_pkey3": TEAL,
    "midi_editcurs": TXT,
    "midi_notebg": WELL,
    "midi_notefg": TXT,
    "midi_notemute": G6,
    "midi_notemute_sel": "#6A6A6A",
    "midi_noteon_flash": TEAL,
    "midi_selpitch1": "#22332F",
    "midi_selpitch2": "#22332F",
    "midi_grid1": G5,
    "midi_grid2": BODY,
    "midi_grid3": "#2A2A2A",
    "midi_gridh": BODY,
    "midi_gridhc": G5,
    "midi_ofsn": BODY,
    "midi_ofsnsel": BODY,
    "midi_endpt": WELL,
    "midi_itemctl": "#CFCFCF",
    "midi_ccbut": "#CFCFCF",
    "midi_ccbut_text": "#CFCFCF",
    "midi_ccbut_arrow": "#CFCFCF",
    "midi_selbg": TEAL,
    "midioct": TXT2,
    "midioct_inline": TXT2,
    "midieditorlist_bg": SEC,
    "midieditorlist_bg2": "#222222",
    "midieditorlist_fg": MENU_TXT,
    "midieditorlist_fg2": MENU_TXT,
    "midieditorlist_grid": BODY,
    "midieditorlist_selbg": TEAL_DARK,
    "midieditorlist_selbg2": TEAL_DARK,
    "midieditorlist_selfg": TXT,
    "midieditorlist_selfg2": TXT,
    "midieditorlist_seliabg": HOVER,
    "midieditorlist_seliafg": MENU_TXT,
    "midifont_col_light": TXT,
    "midifont_col_light_unsel": TXT,
    "midifont_col_dark": WELL,
    "midifont_col_dark_unsel": WELL,
}

# Sixteen automation envelope colours: the rainbow, then four of it again a step lighter.
for i, hex_ in enumerate(RAINBOW + [YELLOW, GREEN, CYAN, MAGENTA]):
    COLOURS[f"col_env{i + 1}"] = hex_


def colorref(hex_):
    """'#RRGGBB' to the integer REAPER stores: Windows COLORREF, 0x00BBGGRR."""
    r, g, b = (int(hex_[i:i + 2], 16) for i in (1, 3, 5))
    return r | (g << 8) | (b << 16)


def font(face, height, weight=400):
    """A Windows LOGFONTA, hex-encoded, with the checksum byte REAPER's .ini struct format adds.

    height: negative = character height in pixels at 100 %; REAPER scales it.
    """
    raw = (height & 0xFFFFFFFF).to_bytes(4, "little")       # lfHeight
    raw += bytes(4 * 3)                                     # lfWidth, lfEscapement, lfOrientation
    raw += weight.to_bytes(4, "little")                     # lfWeight
    raw += bytes([0, 0, 0, 0, 3, 2, 1, 0x22])               # italic, underline, strikeout, charset,
                                                            # out/clip precision, quality, pitch
    name = face.encode("ascii")
    assert len(name) < 32
    raw += name + bytes(32 - len(name))                     # lfFaceName[32]
    assert len(raw) == 60
    raw += bytes([sum(raw) & 0xFF])
    return raw.hex().upper()


# Inter, as the plugins use it, from its static "18pt" files (theme/fonts). Not the family "Inter":
# on the user's PC that name is the variable font, which REAPER's GDI text drew with the wrong
# glyphs some of the time ("0.00dB" as "2..22dB"; user's screenshots, 21 November 2026).
FONTS = {
    "lb_font": font("Inter 18pt", -11),             # track and mixer labels
    "lb_font2": font("Inter 18pt Medium", -11),
    "mi_font": font("Inter 18pt Medium", -11),      # item labels
    "tl_font": font("Inter 18pt", -11),             # timeline / ruler
    "trans_font": font("Inter 18pt SemiBold", -16), # transport clock
}


def theme_text(stage, ui_img=None):
    lines = [
        f"; Fancy Plugs theme for REAPER, {stage}.",
        "; Generated by tools/theme/build.py (docs/reaper-theme.md). Edit that, not this file.",
        "[color theme]",
    ]
    lines += [f"{k}={colorref(v)}" for k, v in sorted(COLOURS.items())]
    lines += ["[REAPER]"]
    lines += [f"{k}={v}" for k, v in sorted(FONTS.items())]
    if ui_img:
        lines += [f"ui_img={ui_img}"]
    return "\r\n".join(lines) + "\r\n"


# Stage B: the colours above plus our own images, as a .ReaperThemeZip: the .ReaperTheme and, beside
# it, the image folder it names in ui_img. The rtconfig.txt only switches PNGs on and picks the
# 150 % images from 120 % up (the user runs 125 %: sharper scaled down from 150 % than up from 100 %).
# It defines no layouts, so REAPER's built-in ones stay (Stage C replaces them).
STAGE_B_NAME = "Fancy Plugs (Stage B preview)"
STAGE_B_IMG = "FancyPlugs_B"
RTCONFIG_B = """; Fancy Plugs theme for REAPER, Stage B (docs/reaper-theme.md). No layouts yet: REAPER's own.
use_pngs 1
misc_dpi_translate 120 150
misc_dpi_translate 174 200
"""


def build_preview(name, img_dir, rtconfig, stage_text):
    """A preview .ReaperThemeZip: the colours with ui_img, our images and an rtconfig.txt."""
    import shutil
    import zipfile
    sys.path.insert(0, str(pathlib.Path(__file__).parent))
    import images

    build = ROOT / "theme" / "build"
    folder = build / img_dir
    if folder.exists():
        shutil.rmtree(folder)
    count = images.render(folder)
    (folder / "rtconfig.txt").write_text(rtconfig.replace("\n", "\r\n"), encoding="ascii", newline="")
    zpath = build / f"{name}.ReaperThemeZip"
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(f"{name}.ReaperTheme", theme_text(stage_text, img_dir))
        for f in sorted(folder.rglob("*")):
            if f.is_file():
                z.write(f, f.relative_to(build).as_posix())
    print(f"{zpath.relative_to(ROOT)}: {count} images at 100, 150 and 200 %")


def build_stage_b():
    build_preview(STAGE_B_NAME, STAGE_B_IMG, RTCONFIG_B, "Stage B preview: colours, fonts and images")


# Stage C: Stage B plus our own layouts (tools/theme/layout.py), as a third preview so Stage B stays
# one click away.
STAGE_C_NAME = "Fancy Plugs (Stage C preview)"
STAGE_C_IMG = "FancyPlugs_C"


def build_stage_c():
    sys.path.insert(0, str(pathlib.Path(__file__).parent))
    import layout
    rtconfig = RTCONFIG_B.replace("Stage B (docs/reaper-theme.md). No layouts yet: REAPER's own.",
                                  "Stage C preview (docs/reaper-theme.md): our layouts.")
    rtconfig += "\n" + layout.rtconfig_walter()
    build_preview(STAGE_C_NAME, STAGE_C_IMG, rtconfig, "Stage C preview: colours, fonts, images and layouts")


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(theme_text("Stage A: colours and fonts"), encoding="ascii", newline="")
    print(f"{OUT.relative_to(ROOT)}: {len(COLOURS)} colours, {len(FONTS)} fonts")
    if "--stage-b" in sys.argv:
        build_stage_b()
    if "--stage-c" in sys.argv:
        build_stage_c()


if __name__ == "__main__":
    main()
