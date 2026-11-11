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
- [ ] Click a value, type `60` and press Enter: the control moves to it. Repeat and click on empty space instead: it applies too. Repeat and press Esc: nothing changes.
- [ ] Right-click on a knob shows REAPER's menu.
- [ ] The small header knob and Output in the EDIT drawer move together.
- [ ] EDIT opens the drawer, and Done closes it.
- [ ] The In and Out meters move, with readouts below.

## Presets and size

- [ ] Choosing a preset from the dropdown changes the settings.
- [ ] **+** saves a preset, and it appears in the dropdown, including in a new instance.
- [ ] Prm > UI size at 200 % resizes the window and everything stays sharp.
- [ ] Prm > About shows the design attribution.

## State

- [ ] Change every setting, including the UI size, then save the project.
- [ ] Close REAPER, reopen the project, and confirm the settings and sound are the same.

## Report back

Tell me "Kicker OK", or list which boxes failed. Once it's OK, I'll start the Exciter. Also tell me if the sound isn't what you wanted: the filter's character and the default Q are the settings most worth tuning by ear.

## Menus and pop-ups (Phase 2.1)

- [ ] Click the preset dropdown: the menu opens just under it, in the plugin's dark style, with **FACTORY** (and **USER** once you've saved one) sections and a check mark on the loaded preset. Hovering a row shows a lighter row with a red bar at its left.
- [ ] Change any control: the dropdown shows the name with a **•** after it, and the menu offers **Reload "…"**. Reload puts the preset back and the dot goes.
- [ ] **Save as new preset…** opens a small dialog inside the plugin window. Save one, then **Rename…** and **Delete…** it from the menu. Delete asks first, with a red Delete button. **Open preset folder** opens Explorer.
- [ ] Arrow keys move through an open menu, Enter picks, Esc closes.
- [ ] Open **Prm** and hover **UI size**: the sizes open to the right after a moment. Pick 200 %, and open the preset menu again: it's twice the size too.
- [ ] Click the Freq value and type `80`, then Enter: the field has a red border and caret, and Freq moves to 80 Hz.

## Undo, A/B and Compare (Phase 2.1 W)

- [ ] The header reads: preset, **+**, **Prm**, **A | B**, output knob, **ON**. The I/O pill has gone into the Prm menu.
- [ ] Move a knob, then open **Prm**: **Undo** (Ctrl+Z) is enabled. Click it: the knob goes back. **Redo** puts it forward again.
- [ ] Set something you'll recognise, click **B**, change it, then click **A** and **B** in turn while playing: each comes back exactly, with no click.
- [ ] **Copy A → B** in Prm, then switch: both sound the same.
- [ ] Save the project with B active and reopen it: B is still active, and A is as you left it.
- [ ] Play a loop and pick **≈ Compare (level-matched)**: you hear the input at about the same loudness as the plugin's output. Pick it again to go back. No clicks either way.
- [ ] Write some automation and play it back with the window open: Undo doesn't pick it up.

## Sub mode (docs/kicker-sub.md)

- [ ] An old project with Kicker on it opens in **Boost** and sounds exactly as before. The knobs sit a little lower than they used to, under the **Boost | Sub** switch, and the caption reads "Boost: lifts its own low end".
- [ ] Click **Sub** while a kick plays: no click, and a sub an octave below comes in. The controls swap in place to Cutoff, the −1 oct / −2 oct row, Sub and Dry, and the caption reads "Sub: adds an octave below".
- [ ] Turn **Sub** up and down: the added low end follows. Turn **Dry** to −inf: you hear the sub alone, a clean sine that follows the kick, starting and stopping without clicks.
- [ ] On a kick that drops in pitch, the sub follows the drop without warbling.
- [ ] Set **Cutoff** just above the kick's body: the sub is steadiest there. Too low, and it comes in late or thin.
- [ ] Pick **−2 oct** on a tom: a sub two octaves down. On a low kick it nearly vanishes (below 15 Hz), as the tooltip says.
- [ ] Open **⚙ EDIT** in Sub: Sensitivity, Sub low-pass, Output, Mono sub and Soft clip. Raise **Sensitivity** on a track with bleed: the bleed stops making sub. Untick **Mono sub**: each side makes its own.
- [ ] The preset browser's **Kick** category has **Sub: Deep 808**, **Sub: Tight Punch** and **Sub: Tom Drop**.
- [ ] Automate Mode between Boost and Sub and render: no clicks, and the render matches playback. The plugin reports no latency in either mode.
