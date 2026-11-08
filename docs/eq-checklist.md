# Fancy EQ: 10-minute REAPER check

Use a full mix or a loop with plenty of top end.

## Install

1. Close any REAPER project using these plugins.
2. In PowerShell, run:
   ```powershell
   cd C:\dev\fancy-plugs
   .\scripts\get-latest.ps1
   ```
3. In REAPER, go to Options > Preferences > Plug-ins > VST and click **Re-scan**.
4. Add **Fancy EQ** to a track.

## Graph

- [ ] Dragging a handle moves the band, and the sound follows without clicks.
- [ ] Shift-drag is finer.
- [ ] The mouse wheel over a handle changes Q, and so does Alt-drag.
- [ ] Double-clicking empty space adds a band there.
- [ ] Double-clicking a handle resets that band.
- [ ] Right-clicking a handle shows the types, Slope, On and Delete band, and each works.
- [ ] Dragging a handle hard while the track plays gives no zipper noise.

## Band controls

- [ ] Clicking tabs 1 to 4 changes the band, and the handle and knobs take its colour.
- [ ] Freq, Gain and Q knobs follow the handle, and the note after Gain changes with Freq.
- [ ] The Type dropdown changes the filter without a click.
- [ ] The power icon switches a band in and out without a click.
- [ ] **+** adds a band and a tab, and **×** removes it.
- [ ] The Gain knob greys out on Notch, Low pass, High pass, Band pass and All pass.

## Filters

- [ ] A high pass at 80 Hz with Slope at 48 dB/oct in EDIT cuts clearly harder than at 12 dB/oct.
- [ ] Raising Q on that high pass adds a bump at the corner, without changing the slope.
- [ ] A boost around 18 kHz sounds like a smooth lift, and its curve doesn't bend down near the right edge.

## Linear phase

- [ ] In EDIT, Phase to Linear gives one short dropout, and REAPER shows the extra latency.
- [ ] Moving a band in Linear mode has no clicks.
- [ ] FIR length changes the latency.
- [ ] Back on Minimum, the latency is gone.

## Analyzer

- [ ] The orange line moves with the music.
- [ ] Analyzer on Pre and Post changes what it shows, and the corner label matches.
- [ ] Peak hold adds a fainter line, and turning it off clears it.
- [ ] Pink tilt lifts the right side of the line.
- [ ] A tone from a JS generator at -12 dBFS sits at -12 on the right-hand scale.

## Layout

- [ ] Show grid hides the lines and keeps the numbers.
- [ ] Show phase adds a yellow line, which goes flat in Linear mode.
- [ ] Compact mode hides the lower panel and floats the band card over the graph.
- [ ] Dragging the window's edge resizes it, and the graph takes the room. It stops at a minimum size.
- [ ] Parameter > UI size at 200 % resizes the window and everything stays sharp.
- [ ] Parameter lists only the bands in use.
- [ ] EDIT opens the drawer, and Done or EDIT closes it, in both layouts.

## Automation

- [ ] Write an envelope on a band's Freq. It plays back without clicks.
- [ ] Untick Log-scale automated freq. The band doesn't move, and the envelope's shape now means a different sweep.

## Controls

- [ ] Drag, Shift-drag, double-click, Ctrl-click typing and the mouse wheel all work on the knobs and the Gain fader.
- [ ] Click a value, type `2.5k` and press Enter: the control moves to it. Repeat and click on empty space instead: it applies too. Repeat and press Esc: nothing changes. Try it on Freq, and on the Gain fader with `-3`.
- [ ] Right-click on a knob shows REAPER's menu.
- [ ] The ON pill switches cleanly, and the controls dim while it's off.

## Presets

- [ ] Each factory preset loads and sounds as named.
- [ ] **+ New** saves a preset, and it appears in the dropdown, including in a new instance.
- [ ] Parameter > About shows the design attribution.

## State

- [ ] Change plenty of settings, including added bands, the EDIT drawer, Compact mode and the window size, then save the project.
- [ ] Close REAPER, reopen the project, and confirm the settings and sound are the same.
- [ ] Render a section with Linear mode on, twice. The two files match.

## Report back

Tell me "EQ OK", or list which items failed and what you saw. Once it's OK, I'll start the Verb.

## Menus and pop-ups (Phase 2.1)

- [ ] Click the preset dropdown: the menu opens just under it, in the plugin's dark style, with **FACTORY** (and **USER** once you've saved one) sections and a check mark on the loaded preset. Hovering a row shows a lighter row with a band-coloured bar at its left.
- [ ] Change any control: the dropdown shows the name with a **•** after it, and the menu offers **Reload "…"**. Reload puts the preset back and the dot goes.
- [ ] **Save as new preset…** opens a small dialog inside the plugin window. Save one, then **Rename…** and **Delete…** it from the menu. Delete asks first, with a red Delete button. **Open preset folder** opens Explorer.
- [ ] Arrow keys move through an open menu, Enter picks, Esc closes.
- [ ] Click the band type dropdown: the list shows each type's icon, then Low pass and High pass with a slope submenu each. Pick High pass, 24 dB/oct.
- [ ] Right-click a band handle: the menu's title reads the band, type and frequency. **Set frequency…** opens the band's Freq field to type in.
- [ ] Change a band's Q, then delete it with ×: it asks first. A band you've just added and not changed goes without asking.

## Undo, A/B and Compare (Phase 2.1 W)

- [ ] The header has **↶ ↷ | A B → | ≈ Compare** between the I/O pill and the output knob, with thin lines between the groups. Nothing overlaps at 100 % or 200 %.
- [ ] Move a control: **↶** lights up, and its tooltip says what it will undo. Click it: the control goes back. **↷** puts it forward again.
- [ ] Add a band, drag it on the graph, then delete it: Undo takes back each one in turn.
- [ ] Set something you'll recognise, click **B**, change it, then click **A** and **B** in turn while playing: each comes back exactly, with no click.
- [ ] **→** copies the active set into the other one.
- [ ] Save the project with B active and reopen it: B is still active, and A is as you left it.
- [ ] Play a loop and click **≈ Compare**: you hear the input at about the same loudness as the output, and the button shows the matching gain. Click again to go back. No clicks either way.
- [ ] Write some automation and play it back with the window open: Undo doesn't pick it up.
