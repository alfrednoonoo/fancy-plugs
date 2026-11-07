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
