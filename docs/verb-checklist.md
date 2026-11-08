# Fancy Verb: REAPER check

## Install

1. Close any REAPER project that uses these plugins, then run:
   ```powershell
   cd C:\dev\fancy-plugs
   .\scripts\get-latest.ps1
   ```
2. In REAPER, go to Options > Preferences > Plug-ins > VST and click **Re-scan**.
3. Add **Fancy Verb** to a track with some audio on it, and have a WAV impulse response ready.

## Starting out

- [ ] A new instance shows the empty state, and the track sounds unchanged.
- [ ] Each factory preset sounds like a reverb or echo, and changes without a click or burst.
- [ ] Empty brings the empty state back.

## Rack

- [ ] Every button on the empty state adds its module.
- [ ] **+** on a card opens the add panel after it, and clicking **+** again closes it.
- [ ] A new module lands in the right place, and the sound and waveform follow.
- [ ] **×** removes a card, and removing the last one brings the empty state back.
- [ ] The power icon on a card and on its chain-strip chip each bypass that module, and they stay in step.
- [ ] Dragging a card by its left-edge grip reorders the chain.
- [ ] The mouse wheel scrolls the rack, and so does dragging in the chain strip.
- [ ] The strip's outline follows what's visible.
- [ ] Knob gestures work on module knobs: drag, shift-drag, double-click, ctrl-click to type.
- [ ] Click a value, type `2.5k` and press Enter: the control moves to it. Repeat and click on empty space instead: it applies too. Repeat and press Esc: nothing changes. Try it on a Lowpass module knob, on Pre-delay, and on the Wet fader with `-6`.
- [ ] Right-clicking a module knob doesn't crash, and shows no automation menu.

## While audio plays

- [ ] Moving any module knob crossfades without a click, burst or dropout.
- [ ] Dragging a knob fast keeps up, and the CPU meter stays calm.
- [ ] Sweeping Pre-delay, Center, Width, Wet and Dry makes no zipper noise.
- [ ] The ON pill switches cleanly.
- [ ] Late blocks in SETUP stays at 0.

## File module

- [ ] Browse... loads your WAV, and the waveform and info string are right.
- [ ] `<`, `>` and the dropdown step through the folder.
- [ ] The breadcrumb shows the folder.
- [ ] Mix to mono changes the info string from 2ch to 1ch on a stereo file.
- [ ] The gain dropdown's settings change the level.
- [ ] A file at another sample rate loads at the right pitch and length.

## Trim handles

- [ ] Both handles drag, dim what they cut, and the sound follows.
- [ ] The info string's length follows the handles.

## SETUP

- [ ] Latency to Low CPU makes a short gap, and REAPER shows 4096 samples of plugin latency.
- [ ] Back on Zero latency, REAPER shows none.
- [ ] With Dry up and Wet at -inf, in either mode, the track stays in phase with a copy of itself.
- [ ] Max FFT changes without a problem on a long IR.

## Rendering

- [ ] Render a section twice with the plugin on and confirm the files match, for instance by polarity-flipping one against the other.

## Save and reload (§7 check 3)

- [ ] Set up a chain with a File module, change Pre-delay and the trim handles, save the project, close REAPER and reopen it. The sound, chain and settings are the same.
- [ ] Close REAPER, rename the IR file, and reopen. It still plays, from the embedded copy.
- [ ] Set IR storage to Path only and load the file again. Save, close, rename the file, and reopen. The card shows `IR missing – Relink...` in red, and the wet signal for that file is gone.
- [ ] Click the red text and pick the renamed file. The sound comes back.
- [ ] Choose an IR folder in SETUP, repeat the rename with the file moved into that folder under its old name, and reopen. It's found.
- [ ] `+ New` saves a preset with the chain, and loading it brings the chain back.

## Layout

- [ ] The Parameter menu lists Pre-delay, Center, Width, Wet, Dry, Output and Bypass.
- [ ] Automating Wet from a REAPER envelope works.
- [ ] The UI size menu scales the window, and the waveform and rack stay sharp.

## Menus and pop-ups (Phase 2.1)

- [ ] Click the preset dropdown: the menu opens just under it, in the plugin's dark style, with **FACTORY** (and **USER** once you've saved one) sections and a check mark on the loaded preset. Hovering a row shows a lighter row with a purple bar at its left.
- [ ] Change any control: the dropdown shows the name with a **•** after it, and the menu offers **Reload "…"**. Reload puts the preset back and the dot goes.
- [ ] **Save as new preset…** opens a small dialog inside the plugin window. Save one, then **Rename…** and **Delete…** it from the menu. Delete asks first, with a red Delete button. **Open preset folder** opens Explorer.
- [ ] Arrow keys move through an open menu, Enter picks, Esc closes.
- [ ] On a FILE module, click the SRC-gain dropdown: the list opens under it with a purple check mark.
- [ ] Change a module's setting, then click its ×: it asks before deleting. A new, unchanged module goes without asking.
- [ ] Click the Pre-delay value and type `0.02 s`: Pre-delay moves to 20.0 ms.

## Undo, A/B and Compare (Phase 2.1 W)

- [ ] The header has **↶ ↷ | A B → | ≈ Compare** between the I/O pill and the output knob, with thin lines between the groups. Nothing overlaps at 100 % or 200 %.
- [ ] Move a control: **↶** lights up, and its tooltip says what it will undo. Click it: the control goes back. **↷** puts it forward again.
- [ ] Add a module, change one of its knobs, then remove it: Undo takes back each one in turn. A and B each keep their own chain.
- [ ] Set something you'll recognise, click **B**, change it, then click **A** and **B** in turn while playing: each comes back exactly, with no click.
- [ ] **→** copies the active set into the other one.
- [ ] Save the project with B active and reopen it: B is still active, and A is as you left it.
- [ ] Play a loop and click **≈ Compare**: you hear the input at about the same loudness as the output, and the button shows the matching gain. Click again to go back. No clicks either way.
- [ ] Write some automation and play it back with the window open: Undo doesn't pick it up.
