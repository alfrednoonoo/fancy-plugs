# Fancy Exciter: 5-minute REAPER check

## Install

1. Close any REAPER project using these plugins.
2. In PowerShell, run:
   ```powershell
   cd C:\dev\fancy-plugs
   .\scripts\get-latest.ps1
   ```
3. In REAPER, go to Options > Preferences > Plug-ins > VST and click **Re-scan**.
4. Add **Fancy Exciter** to a track with vocals, drums or a full mix.

## Sound

- [ ] With Harmonics at 0 %, switching the ON pill makes no audible change.
- [ ] Raising Harmonics adds brightness or presence.
- [ ] Clip Boost makes the added top end denser and more driven.
- [ ] Freq moves the effect: low settings affect more of the sound, high settings only the air.
- [ ] Sweeping Freq quickly makes no clicks.
- [ ] In EDIT, Character changes the tone: warmer at 0 %, edgier at 100 %.
- [ ] The ON pill switches cleanly, and the controls dim while it's off.

## Oversampling and latency

- [ ] With a bright source and high Clip Boost, Off sounds harsher than 4x.
- [ ] Changing Oversampling while playing gives a short dropout but no click.
- [ ] In REAPER's FX window, the performance readout or the track's PDC display shows latency at 4x and none at Off.
- [ ] Duplicate a track, exciting one copy at 4x. With Harmonics at 0 % and the copy polarity-flipped, the two cancel to silence. This confirms latency compensation.

## Controls

- [ ] Drag, Shift-drag, double-click, Ctrl-click typing and the mouse wheel all work on the knobs.
- [ ] Click a value, type `2.5k` and press Enter: the control moves to it. Repeat and click on empty space instead: it applies too. Repeat and press Esc: nothing changes.
- [ ] Right-click on a knob shows REAPER's menu.
- [ ] The small header knob and Output in the EDIT drawer move together.
- [ ] EDIT opens the drawer, and Done closes it.
- [ ] The Oversampling dropdown opens and changes setting.
- [ ] The In and Out meters move, with readouts below.

## Presets and size

- [ ] Choosing a preset from the dropdown changes the settings.
- [ ] **+** saves a preset, and it appears in the dropdown, including in a new instance.
- [ ] Prm > UI size at 200 % resizes the window and everything stays sharp.
- [ ] Prm > About shows the design attribution.

## State

- [ ] Change every setting, including Oversampling and the UI size, then save the project.
- [ ] Close REAPER, reopen the project, and confirm the settings and sound are the same.

## Report back

Tell me "Exciter OK", or list which items failed and what you saw. Once it's OK, I'll start the Comp. Also tell me if the sound isn't what you want: the balance between Clip Boost and Harmonics is the part most worth tuning by ear.

## Menus and pop-ups (Phase 2.1)

- [ ] Click the preset dropdown: the menu opens just under it, in the plugin's dark style, with **FACTORY** (and **USER** once you've saved one) sections and a check mark on the loaded preset. Hovering a row shows a lighter row with a red bar at its left.
- [ ] Change any control: the dropdown shows the name with a **•** after it, and the menu offers **Reload "…"**. Reload puts the preset back and the dot goes.
- [ ] **Save as new preset…** opens a small dialog inside the plugin window. Save one, then **Rename…** and **Delete…** it from the menu. Delete asks first, with a red Delete button. **Open preset folder** opens Explorer.
- [ ] Arrow keys move through an open menu, Enter picks, Esc closes.
- [ ] Open **EDIT** and click Oversampling: the list opens in the same style, with a red check mark on the current value.
- [ ] Click the Freq value and type `2.5k`: Freq moves to 2.50 kHz.
