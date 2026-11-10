# Fancy Gate: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy Gate** to a drum or vocal track.

## Header

- [ ] The I/O pill reads **4 in 2 out**.

## Gating

- [ ] Play a drum track and drag Threshold up: the quiet parts go silent, and the hits come through without clicks.
- [ ] The light reads **open**, **hold** and **closed** in turn, and the Gain meter moves with it.
- [ ] The history scrolls, and clicking or dragging on it moves Threshold.
- [ ] Set Hysteresis to -12 dB: the dashed line moves down, and the gate chatters less on decaying notes.
- [ ] Set Range to -20 dB: the gaps are quieter, not silent.
- [ ] Load **Duck under kick** without a sidechain: the warning appears across the history within a few seconds.

## Pre-open and Lookahead

- [ ] With Lookahead off, Pre-open is dimmed and reads "off". Hovering shows the hint.
- [ ] Clicking or dragging Pre-open shows the hint at once, and the Lookahead checkbox flashes yellow. The knob doesn't move.
- [ ] Tick Lookahead: REAPER's latency readout shows 50 ms (2400 samples at 48 kHz), and Pre-open works.
- [ ] Move Pre-open from 0 to 50 ms: the latency doesn't change, and the hits' attacks get softer as Pre-open grows.

## Preview filter

- [ ] Tick **Preview filter**: you hear what the detector hears, through Lowpass and Highpass, the checkbox is filled yellow, and the title strip shows a yellow **LISTENING** tag.
- [ ] Close the plugin window and open it again: Preview filter is unticked, the sound is normal, and the tag is gone.
- [ ] Open a project saved with Preview filter on in v1.2.0: it opens with Preview filter off.

## Sidechain

- [ ] Route a kick to channels 3/4 of a bass track, and set Detector input to Sidechain L+R with Mode on Duck: the bass ducks on each kick.
- [ ] Set Detector input back to Main input L+R: the ducking stops.

## MIDI trigger

- [ ] Load **Drum trigger to MIDI** on a drum track, and send the track's MIDI to a track with a sampler or ReaSynth.
- [ ] Each hit plays one note. Softer hits play quieter notes.
- [ ] The tooltip on Velocity from level says the notes are 2 ms late. With Lookahead on, it doesn't.
- [ ] Untick MIDI on open/close while a note sounds: it stops.

## Mix

- [ ] Raise Dry: the untouched signal comes back under the gate.
- [ ] Raise Noise: hiss plays only while the gate is open.
- [ ] The ON pill switches cleanly, with and without Lookahead.

## Menus and pop-ups (Phase 2.1)

- [ ] Click the preset dropdown: the menu opens just under it, in the plugin's dark style, with **FACTORY** (and **USER** once you've saved one) sections and a check mark on the loaded preset. Hovering a row shows a lighter row with a yellow bar at its left.
- [ ] Change any control: the dropdown shows the name with a **•** after it, and the menu offers **Reload "…"**. Reload puts the preset back and the dot goes.
- [ ] **Save as new preset…** opens a small dialog inside the plugin window. Save one, then **Rename…** and **Delete…** it from the menu. Delete asks first, with a red Delete button. **Open preset folder** opens Explorer.
- [ ] Arrow keys move through an open menu, Enter picks, Esc closes.
- [ ] Click Detector input: the list has **MAIN INPUT** and **SIDECHAIN** sections.
- [ ] Click the MIDI dropdown and open **Note**: one scrolling list of all 128 notes, with the current one ticked and in view.
- [ ] With Lookahead off, hover the dimmed Pre-open: its hint appears after about a third of a second, quicker than other tooltips.
- [ ] Click the Release value and type `0.2 s`: Release moves to 200 ms.

## Undo, A/B and Compare (Phase 2.1 W)

- [ ] The header has **↶ ↷ | A B → | ≈ Compare** between the I/O pill and the output knob, with thin lines between the groups. Nothing overlaps at 100 % or 200 %.
- [ ] Move a control: **↶** lights up, and its tooltip says what it will undo. Click it: the control goes back. **↷** puts it forward again.
- [ ] Set something you'll recognise, click **B**, change it, then click **A** and **B** in turn while playing: each comes back exactly, with no click.
- [ ] **→** copies the active set into the other one.
- [ ] Save the project with B active and reopen it: B is still active, and A is as you left it.
- [ ] Play a loop and click **≈ Compare**: you hear the input at about the same loudness as the output, and the button shows the matching gain. Click again to go back. No clicks either way.
- [ ] Write some automation and play it back with the window open: Undo doesn't pick it up.
