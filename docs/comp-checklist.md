# Fancy Comp: 10-minute REAPER check

This one takes a little longer than the others, because there's more to try. Use a loop with clear dynamics, such as drums or a vocal.

## Install

1. Close any REAPER project using these plugins.
2. In PowerShell, run:
   ```powershell
   cd C:\dev\fancy-plugs
   .\scripts\get-latest.ps1
   ```
3. In REAPER, go to Options > Preferences > Plug-ins > VST and click **Re-scan**.
4. Add **Fancy Comp** to a track.

## Compression

- [ ] Lowering Threshold compresses more, the GR meter moves, and the red blocks at the top of the input meter light up.
- [ ] Ratio, Knee, Attack and Release each change the sound as you'd expect.
- [ ] Auto make-up brings the level back up.
- [ ] Attack feels reasonable at familiar settings. Ticking **Attack = 90 % rise** in EDIT makes it faster.
- [ ] Classic, Auto and Weird each change the response, without clicks when toggled.
- [ ] RMS size at 0 ms grabs peaks more tightly than at 50 ms.

## Detector

- [ ] Highpass at about 150 Hz stops a kick from pumping the mix.
- [ ] Preview filter lets you hear what the detector hears.
- [ ] Mid and Side settings respond differently on a stereo source.

## Sidechain

- [ ] Route a kick to channels 3 and 4 of a bass track, then pick **Sidechain L+R**. The bass ducks with the kick.

## Wet and dry

- [ ] The Wet and Dry faders work. With Dry raised, compression sounds parallel, with no comb filtering. Try this with Pre-comp at 10 ms too.

## Limiter

- [ ] Tick Limit output, then raise the header knob far enough to push the output over 0 dB. The Out meter stays at or below 0.
- [ ] REAPER shows extra latency once Limit output is on.

## Latency settings

- [ ] Changing Pre-comp while playing gives one short dropout after you stop turning, and no click.
- [ ] Oversampling at 2x and 4x in EDIT does the same, and the compressor keeps working.

## Controls

- [ ] Drag, Shift-drag, double-click, Ctrl-click typing and the mouse wheel all work on knobs and faders.
- [ ] Click a value, type `-12` and press Enter: the control moves to it. Repeat and click on empty space instead: it applies too. Repeat and press Esc: nothing changes. Try it on a knob, on Threshold and on Wet.
- [ ] Right-click on a knob shows REAPER's menu.
- [ ] Hovering over Attack shows its tooltip.
- [ ] The Detector input dropdown opens and changes setting.
- [ ] EDIT opens the drawer, and Done or EDIT closes it.
- [ ] The ON pill switches cleanly, and the controls dim while it's off.

## Presets and size

- [ ] Choosing a preset from the dropdown changes the settings.
- [ ] **+ New** saves a preset, and it appears in the dropdown, including in a new instance.
- [ ] Parameter > UI size at 200 % resizes the window and everything stays sharp.
- [ ] Parameter > About shows the design attribution.

## State

- [ ] Change plenty of settings, including the detector, the EDIT drawer settings and the UI size, then save the project.
- [ ] Close REAPER, reopen the project, and confirm the settings and sound are the same.

## Report back

Tell me "Comp OK", or list which items failed and what you saw. Once it's OK, I'll start the EQ.
