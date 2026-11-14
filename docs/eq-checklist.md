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

## Dynamic bands (Phase 2.1 D)

- [ ] Select band 2 (a bell): a **DYN** chip sits under the type. Pick Low pass: the chip goes. Back to Band: it's back.
- [ ] Turn DYN on: Threshold, Range, Attack, Release, Above | Below and Sidechain appear right of Q, in the band's colour, after a thin line. Freq, Gain and Q get smaller to make room. Nothing overlaps at 100 % or 200 %.
- [ ] On the graph, band 2 shows a lighter fill with a dashed edge down to Gain + Range, and the corner reads "reach −6 dB · now 0 dB".
- [ ] Play a vocal or a mix and lower Threshold until the band acts: the white curve dips inside the dashed area, and "now" follows the music. It sounds smooth, with no crackle or zipper noise, even with Attack at 0.1 ms.
- [ ] With the music quieter than Threshold, switch DYN on and off: nothing changes in the sound.
- [ ] Switch to **Below** and a positive Range: the band lifts the region when it's quiet.
- [ ] Turn on Compact mode: the floating card is wider and shows the same controls as panel 3 of the mockup.
- [ ] Set Phase to Linear (EDIT): the DYN chip and the dynamic controls grey out, and hovering the chip says "Dynamic bands need Minimum phase". The band stays at its static Gain.
- [ ] Make a Notch dynamic: quiet, it lets everything through; loud, it cuts at its Freq.
- [ ] Sidechain: route a kick to the EQ's sidechain (track to 4 channels, send to 3/4, pins 3/4 to Sidechain). The I/O pill reads 4/2. Tick Sidechain on a dynamic low shelf on a bass track: the bass dips on each kick. Unroute it: the "No sidechain signal" warning appears after 2 s.
- [ ] Open a project saved before this version: every band is static and it sounds as before.

## 24 bands and Channel (Phase 3 Q2.4)

- [ ] Open a project saved with v1.3.0: it loads, looks and sounds as before. Render it and null it against a v1.3.0 render if you have one.
- [ ] Double-click the graph until no more bands appear: there are 24 tabs, all readable, and a 25th double-click does nothing.
- [ ] Under the type dropdown there's a small **ST** chip left of DYN, in the Details panel and in the Compact card. Nothing else has moved.
- [ ] Set band 3 to **M** and add a band set to **S**: both handles carry their letter, the corner reads "total: Mid" with band 3 selected and "total: Side" with the Side band selected, and the white line changes with it.
- [ ] Right-click a handle: Channel is in the menu, and it follows the chip.
- [ ] On a stereo mix, a Side high-pass at 150 Hz: the low end narrows to the centre, and the mono sum (REAPER's mono button) sounds the same with the band on and off.
- [ ] A Left bell at 1 kHz, +12 dB: only the left channel changes.
- [ ] Switch a band between ST, L, R, M and S while playing: no clicks.
- [ ] EDIT > Phase > Linear with M and S bands in use: it sounds like the same EQ, with the latency reported. Add a Left band too: still right, no dropouts beyond the one on switching.
- [ ] EDIT > Analyzer > Channel > Side: the orange line drops on mono material and shows the sides on a wide mix.
- [ ] Put the EQ on a mono track: the chip is greyed, its tooltip says "Channel needs a stereo bus", and a band set to S still works.
- [ ] A dynamic band on S reacts to wide material and not to a centred vocal.

## Match (Phase 3 Q2.4)

- [ ] **MATCH** sits left of EDIT in the header. Make the window as narrow as it goes: it becomes just its icon, and nothing in the header overlaps.
- [ ] Click it: a card opens over the graph, MATCH turns white, and "ref 0.0 s" and "current 0.0 s" show. The X closes it, and so does opening EDIT.
- [ ] **Sidechain:** send a reference track to the EQ's sidechain (as for a dynamic band; the I/O pill reads 4/2). Click **Learn** and play: both times count up, a purple and a grey spectrum grow on the graph, and a dashed white line appears. With nothing on the sidechain, "No sidechain signal" appears after 2 s.
- [ ] After 3 s of each, **Match** lights. Click it: up to 8 bands appear in the free slots, the white total follows the dashed line, and the track sounds closer to the reference.
- [ ] Ctrl+Z once: every Match band is gone. Redo: they're back.
- [ ] Set Amount to 50 % and click Match again: the same slots are reused, with about half the gains. No bands pile up.
- [ ] Move one Match band, then Match again: your band stays where you put it, and the others are replaced.
- [ ] **Captured:** switch Source, click Learn reference while the reference plays, stop, then Learn current on your material, then Match.
- [ ] Stop playback while learning: the times stop counting. Clear: both go to 0.0 s and the curves vanish.
- [ ] Type a value into Low (`80`) and High (`8k`): the dashed line fades to 0 outside them.
- [ ] Add a low shelf of your own, +4 dB at 100 Hz, then Match: your shelf stays, and the dashed line asks for less down there.
- [ ] Save the project with spectra learned, close and reopen: the card shows the same seconds, and Match works without learning again.
- [ ] Match doesn't change the sound while it learns: toggle Learn during playback and listen.
- [ ] Compact mode with the card open: the card takes the band card's place, and the band card returns when it closes.
