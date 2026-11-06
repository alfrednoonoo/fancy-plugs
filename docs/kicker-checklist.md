# Fancy Kicker: 5-minute REAPER check

## Install

1. In the project folder, run `.\scripts\get-latest.ps1`.
2. First time only: in REAPER, go to Options > Preferences > Plug-ins > VST and add `%USERPROFILE%\VST3-Dev` to the VST plug-in paths. Use the full path, for example `C:\Users\<you>\VST3-Dev`.
3. Click **Re-scan**.
4. Add **Fancy Kicker** to a track with a kick or bass on it.

## Sound

- [ ] Default settings add weight around 50 Hz.
- [ ] Wet at -inf and Dry at 0 dB sounds identical to the track without the plugin.
- [ ] Sweeping Freq quickly, by hand and with automation, makes no clicks.
- [ ] The ON pill switches cleanly, and the controls dim while it's off.
- [ ] With Soft clip on and Wet at +12 dB, the Out meter stays at or below 0 dB.
- [ ] Mono below Freq on: with a stereo source, the low end feels centred.

## Controls

- [ ] Drag, Shift-drag, double-click, Ctrl-click typing and the mouse wheel all work on Freq.
- [ ] Right-click on a knob shows REAPER's menu.
- [ ] The small header knob and Output in the EDIT drawer move together.
- [ ] EDIT opens the drawer, and Done closes it.
- [ ] The In and Out meters move, with readouts below.

## Presets and size

- [ ] Choosing a preset from the dropdown changes the settings.
- [ ] **+** saves a preset, and it appears in the dropdown, including in a new instance.
- [ ] Prm > UI size at 200 % resizes the window and everything stays sharp.
- [ ] At 100 % the window is a comfortable size. It is 392 × 472, larger than the first build. Tell me if it feels too big.
- [ ] Prm > About shows the design attribution.

## State

- [ ] Change every setting, including the UI size, then save the project.
- [ ] Close REAPER, reopen the project, and confirm the settings and sound are the same.

## Report back

Tell me "Kicker OK", or list which boxes failed. Once it's OK, I'll start the Exciter. Also tell me if the sound isn't what you wanted: the filter's character and the default Q are the settings most worth tuning by ear.
