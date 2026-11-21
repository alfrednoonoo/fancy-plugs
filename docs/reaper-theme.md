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

- Whether the image script needs a library not named in the brief (drawing antialiased PNGs at three scales): Pillow, or rendering SVG through the headless Edge the website checks already use. To be asked before Stage B.

## 8. Menus (user, 21 November 2026: follow the design rules, unless that can't be verified)

The suite's menu style (`docs/DESIGN_SYSTEM.md`: panel `#262626`, 1 px `#434343` border, radius 6, 26 px rows in 11.5 px `#EDEDED`, hover `#383838` with a 2 px accent bar, accent check marks) can only be met where we draw the menu ourselves.

- **Our own menus: fully styled.** The "Fancy: colour tracks" palette menu is drawn by our Lua action in its own small window (REAPER's `gfx`), not with REAPER's `gfx.showmenu`, which opens a standard Windows menu. It uses the suite's menu values exactly, with each row's swatch in its track colour. Checked from the user's screenshots.
- **REAPER's own menus: dark, not restyled.** The menu bar, right-click and dropdown menus are standard Windows menus. A theme can't set their font, row height, radius, border or hover colour, so they can't match the design rules, and we won't claim they do. The closest is REAPER's Windows dark mode (Preferences > General > Advanced, marked experimental in REAPER 7.x), with Windows' apps set to dark: it should draw them dark grey, which sits well with the theme. The checklist has the user turn it on and send a screenshot of a menu, and the result is written here.
- **REAPER's in-panel dropdowns** (the record input on the track panel, for example) are drawn from the theme's images and colours, so they get the suite's control look in Stage B: `#303030`, radius 3, with the `▾` in `#8D8D8D`. The list they open is again a Windows menu.

## 9. Title bar (user, 21 November 2026)

The window's title bar is drawn by Windows, not by REAPER's theme, so the theme can't colour it. REAPER's Windows dark mode does turn it dark: Preferences > General > Advanced UI/system tweaks > "Dark mode (experimental)" set to "Forced on", then restart REAPER (REAPER 7.81 or later; the user has 7.86). That also darkens REAPER's menus and dialogs (section 8). The bar is then Windows' own dark grey, close to the theme's `#1E1E1E` but not exactly it. Windows' "Show accent colour on title bars" could match it exactly, but it changes every app's title bar, so we don't recommend it. The theme's install notes and checklist include the dark mode step.

## 10. As built

- **Stage A (21 November 2026):** `tools/theme/build.py` writes `theme/Fancy Plugs.ReaperTheme`: 246 colours from the suite's values and the palette (lists, tracks, arrange, items, cursors, timeline, meters, envelopes in rainbow order, MIDI editor), and five fonts in Inter (labels, items, ruler, transport clock in SemiBold). Colours are stored as Windows COLORREF, fonts as a Windows LOGFONT plus REAPER's checksum byte; both encoders checked against known values. Blend modes, group and score colours are left at REAPER's defaults. Key names follow REAPER's format; no values from REAPER's themes or the reference. `theme/Fancy Plugs - Colour tracks.lua` is the palette action of sections 4 and 8 (gfx window at the mouse, scaled for HiDPI, one undo step). No Lua interpreter here, so its first run is in REAPER. Install notes in `theme/README.md`, checks in `docs/reaper-theme-checklist.md`. Copied into the user's `ColorThemes` and `Scripts` folders. CI ignores `theme/` and `tools/theme/` until Stage B adds a theme job.
