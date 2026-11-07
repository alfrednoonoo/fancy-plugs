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
