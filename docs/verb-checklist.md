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
- [ ] The cards have no **+**. The dashed **+ Add module** box after the last card opens the add panel there; a module from it lands at the end. **Cancel**, Esc and a click on the rack's empty space close it without adding. With a full rack, scroll right to reach the box.
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

## Library, true stereo and Modulate (Phase 3 Q2.8)

- [ ] Open a project saved with v1.4.0 that uses Fancy Verb: it loads, looks and sounds as before. Render it and null it against a v1.4.0 render if you have one.
- [ ] Add a File module and open its dropdown: a LIBRARY section with Rooms, Halls, Plates, Chambers and Special, each with its IRs, and the TS mark on the true-stereo ones. Pick Halls > Concert Hall: the card reads "/ Library / Halls", shows TS, and the display says "4ch true stereo".
- [ ] Step with `<` and `>`: it moves through Halls only. Try a few from each category: rooms sound like rooms, plates like plates, nothing clicks or jumps in level.
- [ ] Pan a mono source hard left into a true-stereo IR: the reverb comes from both sides, and the right side has its own reflections. Switch to a normal library IR: the reverb stays on the left.
- [ ] **Pair right input…** with two stereo IR files: TS appears, both names show on the dropdown; **Remove pair** goes back to one.
- [ ] Load a four-channel WAV, if you have one: TS appears and Mix to mono is unticked.
- [ ] **Reverb generator:** Low RT 2 x and High RT 0.5 x make a darker, longer low end; Early Room and Hall with Size give audible early reflections; Plate is dense at once; Build-up fades the tail in; True stereo shows 4ch on the display. Size dims unless Early is Room or Hall.
- [ ] **Modulate:** add it to a long hall. The tail moves gently, with no pitch wobble on a sustained note; Depth 0 sounds like no Modulate; Rate and Depth change smoothly while playing. The add panel greys Modulate once the chain has one.
- [ ] The five new presets (Library Hall, Library Plate, True Stereo Room, Modulated Hall, Endless) load from the preset browser.
- [ ] Save a project with a library IR, a pair and Modulate, close and reopen: everything comes back, and the project file is small with the library IR.
- [ ] Render offline twice with Modulate on: the renders are identical.

## Equal loudness, EQ module, Export IR, drop and favourites (Phase 4 Wave 1)

- [ ] Open a project saved with v1.6.0 that uses Fancy Verb: it loads, looks and sounds as before, and SETUP's Equal loudness is off.
- [ ] SETUP > Equal loudness on: the gain shows beside it. Switch between Rooms > Booth and Halls > Concert Hall while playing: the level stays about the same, and the change fades without a click. Off: back to the old level.
- [ ] Add module > EQ: a blue card with a flat curve. Raise the bell at 2 kHz: the curve and the reverb brighten there; the shelves cut the lows and highs. All bands at 0 dB sound like no EQ. Moving a knob costs no CPU while playing.
- [ ] Right-click the waveform > Export IR...: the WAV plays back as the reverb's impulse, at the session rate. With a true-stereo IR, it's four channels and loads back as TS. Reset trim puts the handles back.
- [ ] Drag a WAV from Explorer onto the rack: it tints and says "Drop to add a File module"; the drop adds the card at the end. Drop on a File card: its file is replaced. Drop a text file: refused with a message.
- [ ] File dropdown: type "hall" in the search field: only halls are left. Star one: it's listed under FAVOURITES; still starred after closing and reopening REAPER, and not saved in the project.

## Mac (docs/mac.md, step 5)

On a Mac (Apple silicon, and Intel if one is available), after installing with `install.sh`:

- [ ] The VST3 and the Audio Unit load in Ableton Live: window, pop-ups and resizing work; Cmd-click (not Ctrl-click) on a control opens the typed-value box.
- [ ] The checks above, once, with the VST3 in Ableton Live.
- [ ] A project saved on Windows opens on the Mac with every setting, and a render matches the Windows render by ear (CI holds them to within -100 dB).
- [ ] The Audio Unit in Live: automation follows, an export matches playback, and the Set reopens as it was. (Logic too, if someone has it.)

## Decay module (Phase 4 Wave 2)

- [ ] Open a project saved with v1.6.0 that uses Fancy Verb: it sounds as before.
- [ ] Add > Decay after a hall IR: the card shows three RTs. Low 0.5x: the boom shortens, the read-out halves. High 2x: the air lasts longer.
- [ ] High 4x on a short room IR: the read-out stops short of 4x and the card says High can't grow past the IR's tail; no hiss at the end.
- [ ] Move the crossovers: the read-outs update after the rebuild.
- [ ] A Decay module after an Echo generator on its own: "can't measure" under each knob, and the sound doesn't change.
- [ ] Save, reopen: the module and its settings come back.

## Ducking and the Gate (Phase 4 Wave 2)

- [ ] Open a project saved with v1.6.0 that uses Fancy Verb: it sounds as before; REAPER shows the plugin with 4 input pins and nothing on 3/4.
- [ ] SETUP, Ducking 60 % on a vocal: the reverb dips while the vocal sings and swells in the gaps.
- [ ] Gate on, Threshold -30, Hold 80, Decay 40 on a snare in Low CPU mode: a tight gated reverb that ends with the snare's tail, not before it. Same in Zero latency mode.
- [ ] Verb on a send, Source Sidechain, the vocal routed to pins 3/4: the vocal ducks the reverb. Unroute it: Ducking follows the send's own input.
- [ ] The Gate's knobs dim while it's off.
- [ ] Render offline twice: the renders match.

## Attack and Extra width (Phase 4 Wave 2, items 17 and 18)

- [ ] Open a project saved with v1.7.0 that uses Fancy Verb with a Reverb generator: it sounds as before; the card's Attack reads 0 ms and SETUP's Extra width 100 %.
- [ ] Reverb generator, Attack 150 ms on a snare: the reverb swells in after the hit instead of starting with it, and it's a little quieter. Equal loudness on: the level comes back.
- [ ] Turn Attack while the loop plays: the IR rebuilds and crossfades with no click. The waveform shows the fade-in at the start.
- [ ] Attack with Early on Room or Hall: the early reflections fade in too.
- [ ] SETUP, Extra width 150 % on a stereo loop: the reverb is wider. Switch the master to mono: the reverb's level and tone don't change from Extra width 100 %.
- [ ] Width 0W with Extra width 200 %: still mono (they multiply).
- [ ] Automate Extra width from 100 to 200 %: it moves smoothly, no zipper.
- [ ] Render offline twice: the renders match.

## IR search (v1.7.2)

- [x] Load an IR far down the library, then open the File dropdown: the list opens showing that IR.
- [x] Type "hall": the halls show from the top.

## Freeze and Shimmer (Phase 4 Wave 3, v1.7.2)

- [x] Open a project saved with v1.7.1 that uses Fancy Verb: it sounds as before; Freeze is off and SETUP's Shimmer reads 0 %.
- [x] A chord through a hall, Freeze on as it rings: the sound holds, steady, with no looping or pumping, as wide as it was. The button lights purple and the display tints.
- [x] Play while frozen: the new notes are heard dry only; nothing new enters the held sound.
- [x] Freeze off: the held sound dies away like the hall's own tail, not cut short; new playing reverberates again.
- [x] Toggle Freeze quickly on and off several times: no clicks.
- [x] SETUP, Shimmer 50 %, Octave up, on a slow melody: octaves rise out of the tail and it fades out. 100 %: longer and brighter, and it still fades out.
- [x] Try each Interval: Fifth up, Fourth up, Octave and fifth up, Two octaves up, Octave down. None runs away or gets harsh in the highs.
- [x] Shimmer 100 %, Fifth up, with the library's Metal Ping: it never gets much louder than the notes themselves.
- [x] Shimmer 75 %, then Freeze: the held sound keeps its octaves and stays at one level.
- [x] SETUP open: its panel covers the module strip; close it and the strip is back.
- [x] Automate Freeze and Shimmer, render offline twice: the renders match.
