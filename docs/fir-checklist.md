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
