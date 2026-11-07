# Fancy MultiComp: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy MultiComp** to a mix or a drum bus.

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
