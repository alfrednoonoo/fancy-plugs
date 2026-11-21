# Fancy Plugs theme for REAPER

The suite's look for REAPER 7: its greys, teal and Inter, our own buttons, knobs, faders, meters and toolbar icons at 100, 150 and 200 %, and our own layouts for the track panel, mixer, master and transport, each track panel and mixer strip with a **Compact** layout too. The design and every decision are in `docs/reaper-theme.md`.

## Get it

Each push that changes the theme builds it on GitHub (workflow `reaper-theme`): download the `reaper-theme-<sha>` artifact. It holds `Fancy Plugs.ReaperThemeZip`, the palette action, the fonts and this file. To build it yourself: `pip install -r tools/theme/requirements.txt`, then `python tools/theme/build.py`, which writes `theme/build/Fancy Plugs.ReaperThemeZip`.

## Install

1. **Fonts.** The theme uses Inter's static "18pt" fonts. If `Inter 18pt` isn't installed, double-click the three files in `fonts` and click Install (licence: `Inter-LICENSE.txt`, SIL Open Font License). The plain family "Inter" isn't used: on Windows it can be the variable font, which REAPER drew with wrong characters.
2. **Theme.** Copy `Fancy Plugs.ReaperThemeZip` into REAPER's `ColorThemes` folder (Options > Show REAPER resource path in explorer/finder, then `ColorThemes`), then pick **Fancy Plugs** under Options > Themes. Dragging the file onto REAPER's window also loads it.
3. **Dark title bar and menus.** Preferences > General > Advanced UI/system tweaks…, set **Dark mode (experimental)** to **Forced on**, and restart REAPER. A theme can't do this itself (`docs/reaper-theme.md`, sections 8 and 9).

## Using it

- **Compact layouts:** right-click a track's panel or mixer strip, then Layout > Compact: the track panel in one row, the mixer strip 52 px wide.
- **The mixer's FX list** shows when "Mixer: Toggle show FX inserts when size permits" is on and the mixer is tall enough.
- **Light tint:** each track's panel takes a light tint of its colour and its items are coloured through REAPER's own settings, Preferences > Appearance, the track colour options.

## The colour palette action

`Fancy Plugs - Colour tracks.lua` opens the suite's 24 colours, in rainbow order, at the mouse. Click one to colour the selected tracks, or **Default colour** to clear theirs. Esc closes it.

1. Copy it into REAPER's `Scripts` folder (same resource path).
2. Actions > Show action list > **New action… > Load ReaScript…**, and pick it. It's listed as "Script: Fancy Plugs - Colour tracks.lua".
3. To put it on a toolbar: right-click the toolbar > **Customize toolbar…** > **Add…**, find the action, and OK.

## Changing it

Edit `tools/theme/build.py` (colours and fonts), `tools/theme/images.py` (images) or `tools/theme/layout.py` (layouts), never the generated files, and rebuild.
