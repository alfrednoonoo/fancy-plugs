# Fancy FIR: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy FIR** to a track.

## Header

- [ ] The I/O pill reads **4 in 2 out**.
- [ ] The header shows the latency, and REAPER's latency readout matches it. Change FFT size and Low latency: both follow.

## EQ

- [ ] At the defaults, it sounds the same as bypassed, only later by the latency.
- [ ] Click the curve to add a point, and drag it: the sound changes as you drag.
- [ ] Shift-drag moves a point only up and down. Alt-click deletes it. Double-click sets it to 0 dB.
- [ ] Right-click a point and type `2.5k` and `-6`: the point moves there.
- [ ] Ctrl-drag moves the whole curve.
- [ ] Tilt and De-mud load their curves, and Reset curve goes back to flat.
- [ ] Switch Edit to Freehand: the curve stays the same. Draw a new shape, and it plays.

## Gate and Compressor

- [ ] Gate on a drum loop: drag the threshold curve up, and quiet frequencies drop to Floor.
- [ ] Compressor: yellow shows the reduction at each frequency.
- [ ] Listen to gain-reduced part plays only what's being removed.
- [ ] Tick it, close the plugin window, and open it again: it's unticked, the sound is normal, and the title strip has no **LISTENING** tag. While it's ticked, the tag is there.
- [ ] Route a second track into channels 3 and 4 and tick Sidechain detector: the sidechain's frequencies duck the main track.

## Subtract

- [ ] On a noisy recording, play a noise-only section and click Learn: it counts to 2 s and stops by itself.
- [ ] The red profile line appears, and the noise drops without warbling at Reduction -18 dB.
- [ ] Listen to removed noise plays mostly noise.
- [ ] Save, close and reopen the project: the profile is still there and sounds the same.
- [ ] Clear removes it, and Re-learn captures a new one.

## Changing settings while playing

- [ ] Change FFT size while playing: a short crossfade, no click.
- [ ] Switch between EQ, Gate, Compressor and Subtract while playing: no clicks.
- [ ] Tick Low latency: the latency halves.

## Convolve L/R

- [ ] With a sound on the left and a short percussive sound on the right, the output is the left convolved with the right.
- [ ] On a mono track, it passes the sound through.

## Controls

- [ ] Drag, Shift-drag, double-click, Ctrl-click typing and the mouse wheel all work on the knobs and the Gain fader.
- [ ] Double-click the +24 or -90 on the graph's left and type a new value: the range changes.
- [ ] EDIT opens with Learn length and Output.

## Typing values

- [ ] Click the Reduction value, type `-24` and press Enter. Repeat and click on empty space instead.
- [ ] Click the Ratio value and type `3:1`, then `0.5:1`.
- [ ] Click the Gain value and type `-6`. Press Esc on another: nothing changes.
- [ ] Type `abc` into any value: nothing changes.

## State

- [ ] Draw curves in EQ, Gate and Compressor, save the project, close REAPER and reopen it: every curve is back.
- [ ] `+ New` saves a preset with the curves, and it appears in the list.
- [ ] Parameter > UI size scales the window, and the typing box scales with it.

## Menus and pop-ups (Phase 2.1)

- [ ] Click the preset dropdown: the menu opens just under it, in the plugin's dark style, with **FACTORY** (and **USER** once you've saved one) sections and a check mark on the loaded preset. Hovering a row shows a lighter row with a mode-coloured bar at its left.
- [ ] Change any control: the dropdown shows the name with a **•** after it, and the menu offers **Reload "…"**. Reload puts the preset back and the dot goes.
- [ ] **Save as new preset…** opens a small dialog inside the plugin window. Save one, then **Rename…** and **Delete…** it from the menu. Delete asks first, with a red Delete button. **Open preset folder** opens Explorer.
- [ ] Arrow keys move through an open menu, Enter picks, Esc closes.
- [ ] Click FFT size: the list opens with the check mark in the mode's colour.
- [ ] Hover Low latency for about half a second: the tooltip appears.
- [ ] In EQ mode, right-click a point: a small dialog asks for its frequency and gain. `2k` and `-3 dB` move it.
- [ ] In Subtract mode with a learned profile, click Clear: it asks first.

## Undo, A/B and Compare (Phase 2.1 W)

- [ ] The header has **↶ ↷ | A B → | ≈ Compare** between the I/O pill and the output knob, with thin lines between the groups. Nothing overlaps at 100 % or 200 %.
- [ ] Move a control: **↶** lights up, and its tooltip says what it will undo. Click it: the control goes back. **↷** puts it forward again.
- [ ] Draw a curve, then clear it: Undo takes back each one. Learn a noise profile: when it finishes, Undo takes it back. A and B each keep their own curves and profile.
- [ ] Set something you'll recognise, click **B**, change it, then click **A** and **B** in turn while playing: each comes back exactly, with no click.
- [ ] **→** copies the active set into the other one.
- [ ] Save the project with B active and reopen it: B is still active, and A is as you left it.
- [ ] Play a loop and click **≈ Compare**: you hear the input at about the same loudness as the output, and the button shows the matching gain. Click again to go back. No clicks either way.
- [ ] Write some automation and play it back with the window open: Undo doesn't pick it up.

## Adaptive noise tracking (Phase 2.1 A)

- [ ] In Subtract, the top of the left card has **Learned | Adaptive | Both** in red. Learned is selected, and the card works as before.
- [ ] Open an older project that uses Subtract: it's on Learned and sounds as before.
- [ ] Choose **Adaptive** on a recording with steady noise: the Learn button goes, Adapt speed and "Tracking" appear, with a noise floor in dB. The dashed line on the graph settles on the noise, and the noise is reduced without learning anything.
- [ ] On a recording whose noise changes (a fan, traffic), the dashed line follows it within about the Adapt speed, and reduction keeps up.
- [ ] Play speech or a held note over the noise: "Speech/music held, not learned" appears while it plays, and the voice or note isn't eaten away over time.
- [ ] Choose **Both** with a learned profile: the dashed line never sits below the solid one where it matters, and Learn is still there.
- [ ] Change FFT size while Adaptive: the dashed line stays where it was.
