# Fancy Delay: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy Delay** to a track with a drum loop or a vocal on it.

## Sound

- [ ] The default is a single 1/8 dotted echo, on the beat grid at the project tempo.
- [ ] Change the project tempo while playing: the echo follows, with no clicks.
- [ ] Set Feedback to 60 % and Lowpass to 2 kHz: repeats get darker as they fade. Untick Filters inside feedback loop: they keep the same tone.
- [ ] Load the **1/4 ping-pong** preset: repeats alternate left and right.
- [ ] Load **Haas widener**: the sound widens, and collapsing the track to mono doesn't make it disappear.
- [ ] Load **Lo-fi tape**: repeats are dark, gritty and saturated.
- [ ] Set Dry to -inf: you hear only the echoes, at full level.
- [ ] The ON pill switches cleanly, and the controls dim while it's off.

## Time changes

- [ ] Sweep Length quickly in ms mode with Smooth time changes on: no clicks and no pitch change.
- [ ] Do the same with it off: the pitch bends like tape, with no clicks.

## Feedback

- [ ] Set Feedback to 100 % on a loud source: it builds, and the Out meter stays at or below 0 dB with Volume and Wet at 0 dB.
- [ ] Tick Saturate feedback and set Feedback to 110 %: it builds and distorts softly, and still stays at or below 0 dB. Set it back afterwards.

## Tap map

- [ ] Drag a handle left and right in Note mode: it snaps to note lengths, and the tab title shows them.
- [ ] Shift-drag a handle: it moves freely, and the tap changes to ms.
- [ ] Drag a handle up and down: Volume follows.
- [ ] Feedback shows as faded bars.

## Taps

- [ ] `+` adds a tap in the next colour, and the map shows it.
- [ ] `S` solos a tap, the power icon mutes it, and `×` deletes it. The last tap can't be deleted.
- [ ] Add taps until `+` disappears at 16.

## Controls

- [ ] The ms and Note switch shows and hides the note controls.
- [ ] The straight, dotted and triplet switch changes the time, and the line under it shows the result.
- [ ] EDIT opens with the multiplier and Output, and the multiplier changes the time.
- [ ] Drag, Shift-drag, double-click, Ctrl-click typing and the mouse wheel all work on the knobs and faders. Resolution and the Note-mode Length knob step smoothly on a slow drag.

## Typing values

- [ ] Click Lowpass's value, type `2.5k` and press Enter. Repeat and type `2500`, then click on empty space instead.
- [ ] Click Pan's value and type `60 L`. Click Length's value in Note mode and type `1/16`.
- [ ] Click the Volume, Wet and Dry values and type `-6`. Press Esc on one: nothing changes.
- [ ] Type `abc` into any value: nothing changes.

## State

- [ ] Set up three taps, save the project, close REAPER and reopen it: every tap, setting and the selected tab are back, and it sounds the same.
- [ ] `+ New` saves a preset, and it appears in the list.
- [ ] Parameter > UI size scales the window, and the typing box scales with it.

## Menus and pop-ups (Phase 2.1)

- [ ] Click the preset dropdown: the menu opens just under it, in the plugin's dark style, with **FACTORY** (and **USER** once you've saved one) sections and a check mark on the loaded preset. Hovering a row shows a lighter row with a yellow bar at its left.
- [ ] Change any control: the dropdown shows the name with a **•** after it, and the menu offers **Reload "…"**. Reload puts the preset back and the dot goes.
- [ ] **Save as new preset…** opens a small dialog inside the plugin window. Save one, then **Rename…** and **Delete…** it from the menu. Delete asks first, with a red Delete button. **Open preset folder** opens Explorer.
- [ ] Arrow keys move through an open menu, Enter picks, Esc closes.
- [ ] Click a tap's note dropdown: the list scrolls if it's long, with the current note in view.
- [ ] Hover Feedback for about half a second: the tooltip appears under it.
- [ ] With the tap in Note mode, click the Length value and type `1/8.`: it becomes a dotted eighth. `1/4t` gives a quarter triplet.
- [ ] Change a tap's Feedback, then delete the tap with ×: it asks first.

## Character, motion, ducking and swing (Phase 3 Q2.3)

- [ ] Open a project saved with v1.4.0 that uses Fancy Delay: it loads and sounds as before. Render it and null it against a v1.4.0 render if you have one.
- [ ] Click **Motion** in the tab row: the knob card swaps to Character, Amount, Rate, Depth, Random and Diffusion. **Tone** brings the old knobs back.
- [ ] **Tape:** set a tap to Tape with Feedback around 50 % and Filters inside the loop on: the repeats are warmer and get duller each time. The tab shows **T**.
- [ ] **BBD:** set a tap to BBD and lengthen the time: the repeats get darker as the time grows. The tab shows **B**. With no input, any noise is very faint.
- [ ] Switch Character while a loop plays: no clicks.
- [ ] **Depth** around 15 cents with Rate 0.8 Hz: a gentle wobble; Random at 100 % makes it wander instead. Depth 0 is steady.
- [ ] **Diffusion** at 70 % on a snare: the repeats blur into a wash.
- [ ] **Ducking** (EDIT): Amount 12 dB on a vocal: the repeats dip while the singer sings and come back in the gaps.
- [ ] **Swing** (EDIT) at 60 % with a 1/8 tap: the repeats get a shuffle; a 1/4 tap doesn't move.
- [ ] The four new presets load: **Tape Echo**, **BBD Chorus Echo** (Character), **Diffused Wash** (Ambient), **Ducked Vocal Echo** (Tempo).

## Undo, A/B and Compare (Phase 2.1 W)

- [ ] The header has **↶ ↷ | A B → | ≈ Compare** between the I/O pill and the output knob, with thin lines between the groups. Nothing overlaps at 100 % or 200 %.
- [ ] Move a control: **↶** lights up, and its tooltip says what it will undo. Click it: the control goes back. **↷** puts it forward again.
- [ ] Add a tap, drag it on the map, then delete it: Undo takes back each one in turn.
- [ ] Set something you'll recognise, click **B**, change it, then click **A** and **B** in turn while playing: each comes back exactly, with no click.
- [ ] **→** copies the active set into the other one.
- [ ] Save the project with B active and reopen it: B is still active, and A is as you left it.
- [ ] Play a loop and click **≈ Compare**: you hear the input at about the same loudness as the output, and the button shows the matching gain. Click again to go back. No clicks either way.
- [ ] Write some automation and play it back with the window open: Undo doesn't pick it up.

## Freeze and ceiling (Phase 4 Wave 1)

- [ ] Open a project saved with v1.6.0 that uses Fancy Delay: it loads and sounds as before, with every new setting off or at its default.
- [ ] Freeze while repeats play: they hold without fading and new input isn't heard; off, they decay as normal. No clicks either way.
- [ ] Feedback 110 % with Ceiling -1 dBTP: the output stays under it.

## LFO shape and stereo phase (Phase 4 Wave 2, item 22)

- [ ] The window is 820 px wide at 100 % (and can't be made narrower); the tap graph and the knob card are wider, and nothing overlaps on the Tone or Motion page.
- [ ] Open a project saved with v1.7.0 that uses Fancy Delay with Depth above 0: it sounds as before, with Shape on the sine and Stereo at 90°.
- [ ] Motion page: the wave switch under Rate shows a sine and a triangle; hovering over each names it.
- [ ] Depth 12 cents at 0.8 Hz, Random 0, on a sustained note: Triangle holds a steady detune that flips up and down; Sine sweeps smoothly. Switching between them while playing doesn't click.
- [ ] Stereo at 0°: the repeats stay centred (mono-like); at 180° they're at their widest. Turning it while playing doesn't click.

## Mac (docs/mac.md, step 5)

On a Mac (Apple silicon, and Intel if one is available), after installing with `install.sh`:

- [ ] The VST3 and the Audio Unit load in Ableton Live: window, pop-ups and resizing work; Cmd-click (not Ctrl-click) on a control opens the typed-value box.
- [ ] The checks above, once, with the VST3 in Ableton Live.
- [ ] A project saved on Windows opens on the Mac with every setting, and a render matches the Windows render by ear (CI holds them to within -100 dB).
- [ ] The Audio Unit in Live: automation follows, an export matches playback, and the Set reopens as it was. (Logic too, if someone has it.)

## Freeze strip (v1.7.2)

- [ ] The window is a little taller, and Freeze sits above the tap handles. Set a tap near 375 ms to full volume: its handle stays clear of Freeze and can be dragged.
- [ ] A saved project opens with the same sound.
