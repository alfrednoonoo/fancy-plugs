# Fancy Plugs theme for REAPER (design note)

Approved by the user on 21 November 2026. Mockup: `design/reaper-theme-mockup/index.html` (palette, and the window with each tint).

**Decisions (user, 21 November 2026):**

- **Light tint over grey** (mockup A) is the default: a strip of track colour on the panel, a faint tint behind it, coloured items, the colour on the mixer strip's name.
- **The palette is approved** as drawn: 12 hues in rainbow order, dark shades at 65 % brightness.
- **Filled** (mockup B) stays reachable through the tint-strength settings. For it: dark text on light colours (yellow, lime, and any colour whose luminance is above about 150 of 255), and a light outline on teal controls over teal and green tracks.
- **Menus follow the design rules where REAPER allows it** (section 8).
- **Knobs and faders in the track's colour** (mockup A2; user, 21 November 2026): the volume and pan knobs' arc and pointer and the fader fill take the track's colour. A track in a dark shade uses that hue's bright colour, a track with no colour uses white, and the master uses teal. If REAPER can't recolour these per track (to be tested early in Stage B with a screenshot from the user), they're white everywhere (mockup A3).
- **Title bar** (section 9).

## 1. What it is

A REAPER theme in the suite's look, so REAPER and the plugins read as one product: the same greys, teal, knobs, meters and type as `docs/DESIGN_SYSTEM.md`. For the user's REAPER 7.86 on Windows, at 125 % display scaling (1920 x 1080 shown as 1536 x 864; a second screen at 1280 x 720).

## 2. Rules it follows

- **Nothing from Cockos.** The project never copies or references Cockos binaries or source, and REAPER's own themes (Default 6.0, Default 7.0) count: no images, `rtconfig.txt` or `.ReaperTheme` copied or edited from them. Layouts are written from scratch from the public WALTER documentation; images are drawn by our own script.
- **Themes from elsewhere are references only.** The theme the user sends is read in full and its licence checked; it gives ideas (what's shown where, sizes), never files, unless its licence allows reuse and the user agrees.
- **Design values from the suite.** The tokens in `docs/DESIGN_SYSTEM.md`: header `#1E1E1E`, body `#303030`, sections `#262626`, wells `#141414`, controls `#303030` radius 3, knob face `#4F4F4F` to `#454545` with 300 degrees of travel, text `#FFFFFF` and `#8D8D8D`, teal `#00A985` for the UI accent, red `#E74D3D` for record and clipping, meters `#FFCF07` to `#00FF96` with the red clip cap, Inter as the font.

## 3. Stages (user: staged, 21 November 2026)

Each stage is usable on its own, and the user can stop after any of them.

- **Stage A, colours.** A `.ReaperTheme` over REAPER's built-in layouts: backgrounds, arrange and grid, items, envelopes, MIDI, text, selection, meters, the font. Buttons and faders keep REAPER's stock shapes in our colours.
- **Stage B, images.** Our own PNGs at 100 %, 150 % and 200 %: buttons (record arm, mute, solo, FX, phase, I/O, automation), fader and pan knobs, meters, transport buttons, panel backgrounds, toolbar button frames. Starts after the HTML mockup is approved.
- **Stage C, layouts.** Our own `rtconfig.txt`: track panel, mixer strip and transport, each with a normal and a compact layout, and the master track.

Priority within each stage (user): track panel and arrange, then the mixer, then the transport and toolbars. The MIDI editor gets Stage A's colours only.

## 4. Track colours (user: the plugins' accents, more colours, in rainbow order, 21 November 2026)

Twelve hues in rainbow order: the seven suite accents (marked *) and five between them, chosen in the same flat, mid-bright style so none stands out. Each has a darker shade, for 24 colours in two rows. Exact values are checked side by side in the HTML mockup before they're final.

| # | Hue | Colour | Dark shade |
|---|---|---|---|
| 1 | Red * | `#E74D3D` | about 65 % brightness, set in the mockup |
| 2 | Orange * | `#E77E23` | |
| 3 | Yellow * | `#F1C315` | |
| 4 | Lime | `#A4C639` | |
| 5 | Green * | `#2DCC70` | |
| 6 | Teal * | `#00A985` | |
| 7 | Cyan | `#1BB5D4` | |
| 8 | Blue * | `#3598DB` | |
| 9 | Indigo | `#5864D6` | |
| 10 | Purple * | `#9A59B5` | |
| 11 | Magenta | `#C94FA6` | |
| 12 | Pink | `#E8608A` | |

REAPER has no palette of its own, and SWS isn't installed, so they're offered by a small Lua action of ours, "Fancy: colour tracks": a menu of the 24 colours in this order (plus "default colour"), for the selected tracks, on a toolbar button. It ships in the theme zip with how to add it.

## 4.1 What we took from the reference theme

The user's reference is the Reapertips Theme v1.95a (Alejandro Hernandez and FeedTheCat), read in full on 21 November 2026. Licence CC BY-NC-SA 4.0: a derivative must use the same licence and keep "Reapertips" in its name, and its `rtconfig.txt` is itself "based on Default_v6.0" (Cockos). So nothing is copied from it: no images, layouts, colour files or icons. Ideas only, which are free to use:

- Track colour carries the arrangement: it tints the track panel, its items and the mixer strip, with settings for how strong the tint is, normal and selected.
- A theme settings panel (WALTER's `define_parameter`, shown by REAPER's theme adjuster): layout widths, which buttons show, tint strengths.
- A selected-track bar; separator tracks; folder indents that stay compact; the FX list beside the controls in the track panel.
- A rainbow colour palette offered with the theme.
- One centred, docked transport.

## 5. Scale

Images at 100 %, 150 % and 200 %. REAPER picks the nearest set for the user's 125 %, so 150 % is the set checked most closely, and every stage is checked at 125 %.

## 6. Build and test

- `theme/` holds the source: the colour list, the layouts, and the image specs. `tools/theme/` holds the script that draws every PNG at three scales and writes `Fancy Plugs.ReaperThemeZip`.
- CI builds it and uploads `reaper-theme-<sha>`. The theme isn't part of the plugin release zips.
- CI can't run REAPER. The test is the user opening the theme in REAPER (Options > Themes > Load theme) and sending screenshots; a checklist, `docs/reaper-theme-checklist.md`, lists what to look at.
- Before Stage B, an HTML mockup of REAPER's main window (track panels, arrange, transport) and mixer in the theme, for approval, as each new plugin has had.

## 7. Open points

- Image drawing: Pillow, approved by the user on 21 November 2026, for the theme script only (`tools/theme/requirements.txt`).

## 8. Menus (user, 21 November 2026: follow the design rules, unless that can't be verified)

The suite's menu style (`docs/DESIGN_SYSTEM.md`: panel `#262626`, 1 px `#434343` border, radius 6, 26 px rows in 11.5 px `#EDEDED`, hover `#383838` with a 2 px accent bar, accent check marks) can only be met where we draw the menu ourselves.

- **Our own menus: fully styled.** The "Fancy: colour tracks" palette menu is drawn by our Lua action in its own small window (REAPER's `gfx`), not with REAPER's `gfx.showmenu`, which opens a standard Windows menu. It uses the suite's menu values exactly, with each row's swatch in its track colour. Checked from the user's screenshots.
- **REAPER's own menus: dark, not restyled.** The menu bar, right-click and dropdown menus are standard Windows menus. A theme can't set their font, row height, radius, border or hover colour, so they can't match the design rules, and we won't claim they do. The closest is REAPER's Windows dark mode (Preferences > General > Advanced, marked experimental in REAPER 7.x), with Windows' apps set to dark: it should draw them dark grey, which sits well with the theme. The checklist has the user turn it on and send a screenshot of a menu, and the result is written here.
- **REAPER's in-panel dropdowns** (the record input on the track panel, for example) are drawn from the theme's images and colours, so they get the suite's control look in Stage B: `#303030`, radius 3, with the `▾` in `#8D8D8D`. The list they open is again a Windows menu.

## 9. Title bar (user, 21 November 2026)

The window's title bar is drawn by Windows, not by REAPER's theme, so the theme can't colour it. REAPER's Windows dark mode does turn it dark: Preferences > General > Advanced UI/system tweaks > "Dark mode (experimental)" set to "Forced on", then restart REAPER (REAPER 7.81 or later; the user has 7.86). That also darkens REAPER's menus and dialogs (section 8). The bar is then Windows' own dark grey, close to the theme's `#1E1E1E` but not exactly it. Windows' "Show accent colour on title bars" could match it exactly, but it changes every app's title bar, so we don't recommend it. The theme's install notes and checklist include the dark mode step.

## 10. As built

- **Stage A (21 November 2026):** `tools/theme/build.py` writes `theme/Fancy Plugs.ReaperTheme`: 246 colours from the suite's values and the palette (lists, tracks, arrange, items, cursors, timeline, meters, envelopes in rainbow order, MIDI editor), and five fonts in Inter (labels, items, ruler, transport clock in SemiBold). Colours are stored as Windows COLORREF, fonts as a Windows LOGFONT plus REAPER's checksum byte; both encoders checked against known values. Blend modes, group and score colours are left at REAPER's defaults. Key names follow REAPER's format; no values from REAPER's themes or the reference. `theme/Fancy Plugs - Colour tracks.lua` is the palette action of sections 4 and 8 (gfx window at the mouse, scaled for HiDPI, one undo step). No Lua interpreter here, so its first run is in REAPER. Install notes in `theme/README.md`, checks in `docs/reaper-theme-checklist.md`. Copied into the user's `ColorThemes` and `Scripts` folders. CI ignores `theme/` and `tools/theme/` until Stage B adds a theme job.
- **Stage A fixes after the user's first screenshots (21 November 2026):** the theme loaded, the title bar went dark, and the palette coloured tracks with one undo step each. Two faults: text in Inter sometimes came out with wrong characters ("0.00dB" as "2..22dB", "Default colour" as "Defa]lt colo]r"), in REAPER's labels and in the palette alike, which points to Windows' GDI and the variable font that owns the family name "Inter" on the user's PC; the theme and the palette now use the static family "Inter 18pt" (files in `theme/fonts`, OFL). And each swatch had a light seam from rounded corners drawn at fractional positions; corners are now drawn row by row on whole pixels.
- **Menus checked (user's screenshot, 21 November 2026):** with REAPER's dark mode "Forced on", the menu bar's menus are dark grey with white text and white check marks, and the title bar is dark. They aren't the suite's menu style (section 8), as expected.
- **Fonts checked (user's screenshot, 21 November 2026):** a fresh project reads correctly ("0.00dB", "120", "4/4", the ruler), and the palette's text and swatches are clean. Its text was about a fifth too small, because `gfx.setfont` takes the line height; sizes are now multiplied by 1.21.
- **Stage B, first round (21 November 2026, built while the user was away):** `tools/theme/images.py` draws 54 images with Pillow (4x supersampled, then scaled down), at 100, 150 and 200 %: the shared track buttons the track panel and mixer use (record arm, mute in orange, solo in yellow, phase, monitor, the envelope button in each automation mode's colour, FX, IO with send and receive dots, record mode, folder), the fader and pan tracks and caps for the track panel and mixer, the name fields, and the transport buttons (play, pause and repeat teal when on, record red). Buttons are three frames (normal, mouseover, pressed: `#303030`, `#383838`, `#1E1E1E`); tracks and name fields carry pink markers so their rounded ends don't stretch. Names, frame order and markers follow REAPER's image reference (reaper.fm/sdk/walter/images.php); sizes were sanity-checked against the reference theme's, no art taken. Not done in this round, and why: the meters (the strip images pack several bands whose meaning REAPER doesn't document, so Stage A's meter colours stay), knobs (REAPER's built-in layout uses sliders; knobs come with Stage C's layouts), and per-track colours on fader caps (they're white, the agreed fallback, until the test in Stage C, since tinting needs a layout). `python tools/theme/build.py --stage-b` writes `theme/build/Fancy Plugs (Stage B preview).ReaperThemeZip` (git-ignored): the Stage A colours with `ui_img`, the images, and an `rtconfig.txt` that only switches PNGs on and uses the 150 % images from 120 % up. Copied into the user's `ColorThemes` beside Stage A, which is unchanged. Not yet seen in REAPER.
- **Stage B, first round seen in REAPER (user's screenshots, 21 November 2026):** loads with Stage A's colours; the track and mixer buttons, faders, pan, name fields and transport are the suite's; hover lightens, M orange, S yellow, FX teal with FX and struck red when bypassed, record arm a red dot, repeat teal; sharp at 125 %. Still REAPER's own: the FX chain switch beside FX, the input FX button, the record input box, the main toolbar icons, the folder button (a grey block) and the Rate slider. The master strip already uses the mixer's fader images; its meter waits like the others.
- **Stage B, second round (21 November 2026, user):** 41 more images (95 in all): the main toolbar (new, open, save, project settings, undo, redo, revert, and metronome, grid, snap, relative snap, crossfades, envelopes with items, grouping, dock, filter, lock and ripple off/one/all as toggles, teal when on, grey when off), the FX chain switch (`track_fxon/off/empty`, `_v` and `_h`: teal dot, grey ring, faint dot), the input FX button (`track_fx_in_norm/empty`), the record input box (`tcp_recinput`, `#303030` radius 3; REAPER draws its text and arrow), the Rate slider (`transport_playspeedbg/thumb`) and an outlined folder for a normal track. Names and frame sizes from REAPER's image reference and the default themes' file names and sizes; no art taken. MIDI toolbar icons stay REAPER's (the MIDI editor gets Stage A's colours only).
- **Toolbar colours (user, 21 November 2026):** the first toolbar icons were "too bland". Chosen from a comparison: each icon in its own suite accent (file actions blue, undo and redo teal, project settings, grid and filter purple, metronome and lock orange, ripple yellow, grouping green, snap red, crossfades and dock blue, envelopes teal); a toggle that's off is grey, and one that's on fills its button with its accent (dark icon on yellow, orange and green, as M and S, white on the rest). Lines are bolder (1.75 px at 100 %). Every button face is now grey-5 `#424242` rather than `#303030`, which vanished on REAPER's `#2B2B2B` panels. The last two main toolbar buttons are marquee selection and razor editing (user's tooltips); their images, `toolbar_marquee_cursor_selection_on/off` and `toolbar_razor_on/off`, live in REAPER's `Data/toolbar_icons`, not in the theme reference. The theme now has its own (a dashed box with a cursor in yellow, a razor blade in red); the theme's copy wins over `Data/toolbar_icons` (user's screenshot, 21 November 2026). The whole main toolbar is now the theme's.
- **Second round seen in REAPER (user's screenshot, 21 November 2026, playing):** grey-5 faces read clearly on the track panel, mixer and transport; play teal while playing; the outlined folder, the FX chain switch (a teal dot on the track panel; in the mixer REAPER's layout squashes it into a short teal bar, which still reads as on) and the Rate slider are the theme's. Armed track (user's screenshot, 21 November 2026): record arm a red dot on the track panel and in the mixer, the input FX button the theme's. The record input box is still REAPER's (white outline over the meter): the built-in layout draws its own and ignores `tcp_recinput`, so it comes with Stage C's layouts. Stage B is done apart from that, the meters, knobs and per-track colours, all in Stage C.
- **Stage C, first round (21 November 2026, user: start Stage C):** the track panel from the mockup, in `tools/theme/layout.py` (WALTER, written from REAPER's public reference; nothing from its themes), built by `python tools/theme/build.py --stage-c` as a third preview, "Fancy Plugs (Stage C preview)", so Stage B stays one click away. 54 px at 100 % (REAPER scales layouts with the display): a 5 px strip in the track's colour (a WALTER custom text element with a coloured background; to be confirmed), the track number with the folder button under it; row 1 record arm, name well, M, S, FX, FX chain switch; row 2 volume knob, its value, pan knob, then envelope and polarity, or, when armed, the input box (stretching), monitoring, record mode and input FX; IO under FX; a thin stereo meter on the right edge. Tracks under 50 px show row 1 only. Knobs are the suite's knob face (`gen_knob_bg_small/large`, `#4F4F4F` to `#454545`) with REAPER's pointer in the track's colour (`tcp.volume.color` from `trackcolor_r/g/b`), white for a track with no colour: decision A2 without the arc, which REAPER's knobs don't draw. The mixer, master and transport keep REAPER's built-in layouts in this round, which also tests whether REAPER keeps them when a theme lays out only the track panel.
- **Track panel seen in REAPER (user's screenshot, 21 November 2026; tracks coloured red, yellow and green by the user with the palette action):** both rows in place, the colour strip shows (the custom-element trick works), the knobs are the grey face with the pointer in the track's colour, buttons grouped on the right, the meter on the edge. The name well takes a dark tint of the track colour from REAPER's tint setting, which suits decision A. REAPER kept its built-in mixer and transport, so a theme can lay out one part at a time. Armed row 2 not yet seen.
- **Stage C, mixer (21 November 2026):** `mcp` and `master.mcp` in `tools/theme/layout.py`, from the mockup: a 78 px strip (master 104 px) with the FX list at the top, the pan knob (pointer in the track's colour, teal on the master), fader and meter stretching with the mixer's height and a column of small buttons beside them (FX, FX switch, IO, envelope, polarity, monitoring, and input FX and record mode when armed), the volume value or, when armed, the input box, M, S and record arm (M, S and mono on the master), and the name on a bar in the track's colour with dark text (white on grey for a track with no colour, teal on the master). Not yet seen in REAPER.
- **Mixer seen in REAPER, and fixes (user's screenshots, 21 November 2026):** the armed track panel row works (input box, monitoring, record mode, input FX). In the mixer the strips' order, the pan pointer in the track's colour and the master's teal name bar work. Fixed: the track strips' name bar was dark, because REAPER drew the name-well image over the colour (`mcp_namebg` is now transparent); the user's mixer is about 240 px tall at 100 %, so the strip is laid out for 240 (bottom rows from 176), what's under the FX list moves up by its 50 px when the list is hidden (`mcp_wantextmix&1`, added with WALTER's `+`), and the button column is FX, FX switch, IO, then envelope and polarity, or monitoring and input FX when armed (the input FX button had landed on the input box); the master's mono button (`track_mono`, `track_stereo`) and the mixer menu button (`mixer_menu`) are drawn. The track number sits at the left of the name bar in grey.
