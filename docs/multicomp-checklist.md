# Fancy MultiComp: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy MultiComp** to a mix or a drum bus.

## Header

- [ ] The I/O pill reads **4 in 2 out**.

## Sound

- [ ] At the defaults, the plugin sounds the same as bypassed.
- [ ] Drag band 2's handle down: the yellow bar grows, and the band gets quieter.
- [ ] Load **Gentle Glue**: the mix tightens, without a big level change.
- [ ] Load **Multiband Limiter**: loud peaks are held, and the Out meter stays at or below 0 dB.
- [ ] The ON pill switches cleanly, and the controls dim while it's off.

## Crossovers

- [ ] Drag a blue line quickly while playing: no clicks.
- [ ] A line stops short of its neighbours.
- [ ] Switch Crossover between LR 24, LR 48 and Linear phase: a short fade, then the same level. REAPER's latency readout changes for Linear.
- [ ] Solo each band in turn: you hear just that range.

## Graph

- [ ] The wheel on a handle, and Alt-drag, change Ratio.
- [ ] Double-click a handle: the band resets, and its range stays.
- [ ] Double-click an empty spot: a new band appears there, and the sound doesn't change.
- [ ] Show spectrum and Show grid switch their parts on and off.

## Bands

- [ ] `+` splits the selected band, and `+` disappears at 8 bands.
- [ ] `×` deletes a band, and its range goes to the band above. The last band can't be deleted.
- [ ] The power icon bypasses the band's compression, and the band stays audible.
- [ ] Auto make-up shows its amount in brackets after Gain, and that amount doesn't move with the music.

## Link bands

- [ ] With Link bands on, moving one band's Threshold moves the others by the same amount.
- [ ] The same works for Ratio, Attack, Release and Gain.
- [ ] Holding Alt moves just the one band.
- [ ] Loading a preset with Link bands on gives the preset's exact values.

## Sidechain

- [ ] Route a kick into channels 3 and 4 of the plugin, and tick Sidechain on band 1 only. The kick ducks the low end, and the other bands don't move.
- [ ] Set Detector to Sidechain L+R: every band listens to the kick.

## Controls

- [ ] Drag, Shift-drag, double-click, Ctrl-click typing and the mouse wheel all work on the knobs and faders.
- [ ] EDIT opens with Attack = 90 % rise, Log-scale automated freq. and Output.

## Typing values

- [ ] Click the Threshold value, type `-18` and press Enter. Repeat and click on empty space instead.
- [ ] Click the Ratio value and type `3:1`, then `0.5:1`, then `inf`.
- [ ] Click the Wet and Dry values and type `-6`. Press Esc on one: nothing changes.
- [ ] Type `abc` into any value: nothing changes.

## State

- [ ] Set up five bands with different settings, save the project, close REAPER and reopen it: everything is back, and it sounds the same.
- [ ] `+ New` saves a preset, and it appears in the list.
- [ ] Parameter > UI size scales the window, and the typing box scales with it.

## Menus and pop-ups (Phase 2.1)

- [ ] Click the preset dropdown: the menu opens just under it, in the plugin's dark style, with **FACTORY** (and **USER** once you've saved one) sections and a check mark on the loaded preset. Hovering a row shows a lighter row with a band-coloured bar at its left.
- [ ] Change any control: the dropdown shows the name with a **•** after it, and the menu offers **Reload "…"**. Reload puts the preset back and the dot goes.
- [ ] **Save as new preset…** opens a small dialog inside the plugin window. Save one, then **Rename…** and **Delete…** it from the menu. Delete asks first, with a red Delete button. **Open preset folder** opens Explorer.
- [ ] Arrow keys move through an open menu, Enter picks, Esc closes.
- [ ] Click Crossover: the list opens in the selected band's colour.
- [ ] Hover Ratio for about half a second: the tooltip appears under it.
- [ ] Change a band's Ratio, then delete the band with ×: it asks first.
- [ ] Click the Threshold value and type `-24`: the fader moves to -24.0.

## Undo, A/B and Compare (Phase 2.1 W)

- [ ] The header has **↶ ↷ | A B → | ≈ Compare** between the I/O pill and the output knob, with thin lines between the groups. Nothing overlaps at 100 % or 200 %.
- [ ] Move a control: **↶** lights up, and its tooltip says what it will undo. Click it: the control goes back. **↷** puts it forward again.
- [ ] Split a band, then merge it back: Undo takes back each one. With Link bands on, one drag that moves every band undoes in one step.
- [ ] Set something you'll recognise, click **B**, change it, then click **A** and **B** in turn while playing: each comes back exactly, with no click.
- [ ] **→** copies the active set into the other one.
- [ ] Save the project with B active and reopen it: B is still active, and A is as you left it.
- [ ] Play a loop and click **≈ Compare**: you hear the input at about the same loudness as the output, and the button shows the matching gain. Click again to go back. No clicks either way.
- [ ] Write some automation and play it back with the window open: Undo doesn't pick it up.
