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
