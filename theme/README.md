# Fancy Plugs theme for REAPER

Stage A: colours and fonts in the Fancy Plugs look, over REAPER's built-in images and layouts. Our own images (Stage B) and layouts (Stage C) come next; see `docs/reaper-theme.md`.

## Install

1. **Fonts.** The theme uses Inter's static "18pt" fonts. If `Inter 18pt` isn't installed, double-click the three files in `theme/fonts` and click Install (licence: `theme/fonts/Inter-LICENSE.txt`, SIL Open Font License). The plain family "Inter" isn't used: on Windows it can be the variable font, which REAPER drew with wrong characters.
2. **Theme.** Copy `Fancy Plugs.ReaperTheme` into REAPER's `ColorThemes` folder (Options > Show REAPER resource path in explorer/finder, then `ColorThemes`), then pick **Fancy Plugs** under Options > Themes. Dragging the file onto REAPER's window also loads it.
3. **Dark title bar and menus.** Preferences > General > Advanced UI/system tweaks…, set **Dark mode (experimental)** to **Forced on**, and restart REAPER. A theme can't do this itself (`docs/reaper-theme.md`, sections 8 and 9).

## The colour palette action

`Fancy Plugs - Colour tracks.lua` opens the suite's 24 colours, in rainbow order, at the mouse. Click one to colour the selected tracks, or **Default colour** to clear theirs. Esc closes it.

1. Copy it into REAPER's `Scripts` folder (same resource path).
2. Actions > Show action list > **New action… > Load ReaScript…**, and pick it. It's listed as "Script: Fancy Plugs - Colour tracks.lua".
3. To put it on a toolbar: right-click the toolbar > **Customize toolbar…** > **Add…**, find the action, and OK.

## Light tint

The design (`docs/reaper-theme.md`) tints each track's panel lightly in its colour and colours its items. In Stage A, REAPER's own settings decide how strongly: Preferences > Appearance, the track colour options (tint the track panels, colour the items). Stage C moves these into the theme's own settings.

## Stage B preview

`python tools/theme/build.py --stage-b` also writes `theme/build/Fancy Plugs (Stage B preview).ReaperThemeZip`: Stage A's colours plus our own buttons, faders and transport, at 100, 150 and 200 %. Copy it into `ColorThemes` and pick it under Options > Themes; Stage A stays available beside it. It needs Pillow (`pip install -r tools/theme/requirements.txt`).

## Rebuild

`python tools/theme/build.py` writes `Fancy Plugs.ReaperTheme` from the values in that script. Edit the script, not the generated file.
