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

## MIDI trigger, Ratio, Learn, Mid/Side and De-bleed (Phase 4 Wave 1)

- [ ] Open a project saved with v1.6.0 that uses Fancy Gate: it loads and sounds as before, with every new setting off or at its default.
- [ ] EDIT opens the drawer over the knobs; the meter and faders stay visible.
- [ ] MIDI trigger: a MIDI track routed to Gate opens it on each note and closes it at note-off.
- [ ] Learn on a tom track: Threshold lands between the bleed and the hits.
- [ ] Ratio 2:1 gives a gentle expander; inf:1 sounds as before.
- [ ] De-bleed with Highpass and Lowpass around the tom: cymbal hits no longer open it.
- [ ] Detector input Mid and Side work.

## Mac (docs/mac.md, step 5)

On a Mac (Apple silicon, and Intel if one is available), after installing with `install.sh`:

- [ ] The VST3 and the Audio Unit load in Ableton Live: window, pop-ups and resizing work; Cmd-click (not Ctrl-click) on a control opens the typed-value box.
- [ ] The checks above, once, with the VST3 in Ableton Live.
- [ ] A project saved on Windows opens on the Mac with every setting, and a render matches the Windows render by ear (CI holds them to within -100 dB).
- [ ] The Audio Unit in Live: automation follows, an export matches playback, and the Set reopens as it was. (Logic too, if someone has it.)

## Host sync (Phase 4 Wave 2)

- [ ] Open a project saved with v1.6.0 that uses Fancy Gate: it sounds as before, with its Detector input unchanged.
- [ ] The EDIT link reads "EDIT" in full.
- [ ] Detector input Host sync on a pad at 120 BPM, Range -inf: the pad chops on every eighth, exactly on the grid. Shape 25 % and 75 %: shorter and longer chops. Threshold -50 and -12: the chops don't change length.
- [ ] Rate 1/4, 1/8 dotted, 1/16 triplet: the chops follow. Mode Duck, Range -12: it pumps instead.
- [ ] Stop the song: it keeps chopping at the tempo; play from a bar: the chops land on the beat again. Loop a bar: no doubled or missing chop at the loop point.
- [ ] Tick MIDI trigger: the Host sync controls dim and the pulse is ignored.
- [ ] Render offline twice: the renders match.

## Transient detector and Upward mode (v1.7.1)

- [ ] Open a project saved with v1.7.0 that uses Fancy Gate: it loads and sounds as before (Detector Level, Mode Gate or Duck, Ceiling Off). The window is 790 x 465 and every control is where it was, with the Range card's new title row.
- [ ] Tom track with bass bleed, Lowpass about 1 kHz, Detector **Transient**, Rise 8 dB: every hit opens it, soft and loud, and the bass doesn't, in a quiet and a loud section. RMS size becomes Rise; Hysteresis and Ratio dim.
- [ ] Transient on a held synth note: it opens once at the start and closes after Hold and Release while the note goes on.
- [ ] Transient with MIDI out on: one note per hit. With Lookahead on, it opens Pre-open ahead of each hit. Duck mode ducks on each hit.
- [ ] Tick MIDI trigger, or choose Host sync: the Detector switch dims.
- [ ] Mode **Upward** on a mix with a quiet verse and a loud chorus, Threshold between them, RMS size 50 ms: the chorus comes up by about Boost or less, the verse doesn't change. Range becomes Boost, Ratio in the drawer becomes Up ratio, Hysteresis dims, the light reads "boost".
- [ ] Switch Gate, Duck and Upward while playing: no clicks.
- [ ] Ceiling in the drawer: pick -1.0 dBTP from the list, then Type a value... and type -3; Ctrl-click opens the box too. Upward with Boost 12 on a loud track stays under it; the host's latency compensation keeps the track in time when you turn it on and off. Off adds no latency.

## Bands (Phase 4 Wave 3)

- [x] The window is taller (790 x 637): the BANDS card sits between the history and the envelope card, and nothing else is squeezed. A project saved with v1.7.1 opens with Bands at 1 and sounds as before.
- [x] A tom mic with cymbal bleed, Bands 2 (crossover about 250 Hz), Threshold set on the toms: the toms ring as before, and the cymbals while a tom rings are much quieter than at Bands 1; band 1's "open" lights with each hit, band 2's stays "shut" on the cymbals.
- [x] Raise band 2's Threshold: it needs more high end to open. Lower band 2's Range to 50 %: the cymbals come back, about half as far down; the knob reads the band's Range in dB.
- [x] Drag a blue crossover line: it moves, stops a third of an octave from its neighbour, and the sound follows without clicks. Click a line without dragging: a box asks for the frequency; `2.5k` works. Ctrl+Z takes it back.
- [x] Bands 2 or 3: Lowpass, Highpass and De-bleed (EDIT) dim. Bands 1: they work again.
- [x] Change Bands while playing: it fades out and back in, with no click; REAPER's PDC doesn't change.
- [x] MIDI out with 2 bands: one note per hit even when both bands open; the light and the Gain meter follow whichever band is open.
- [x] Wet and Dry both up with 3 bands: no thin or hollow sound (no comb).
- [x] Render offline twice at 3 bands: the renders match.
