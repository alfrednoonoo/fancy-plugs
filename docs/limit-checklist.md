# Fancy Limit: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy Limit** to the master track of a finished mix.

## Look

- [ ] The title strip reads **FANCY LIMIT** with LIMIT in red, and the window matches `reference/phase3/20__limit_mockup.png`: Style and Target on top, Threshold and GR on the left, the history and read-outs, the Limiter card, the footer, and Out with its scale and the LUFS bar on the right.
- [ ] The I/O pill reads **2/2**.

## Limiting

- [ ] Play the loudest part and drag Threshold down to about -8 dB: it gets louder, the GR meter and the red area in the history move with the peaks, and nothing distorts audibly.
- [ ] REAPER's master meter never goes above the Ceiling (-1.0 dB). Set Ceiling to -3 dB: the output peaks drop to -3.
- [ ] Turn Lookahead to 0 ms with a lot of reduction: it gets grittier. Back at 2 ms it's clean again. REAPER's latency readout (the plugin's "PDC" in the FX chain) doesn't change while you turn it.
- [ ] Switch **Clean**, **Punchy** and **Dense** on drums: Punchy keeps more snap on the hits, Dense sounds denser and louder.
- [ ] Untick **Auto release** on a bass-heavy section with Release at 10 ms: the bass gets grainy. Tick it again: clean.
- [ ] Tick **Listen to removed**: you hear only the peaks being taken off. Untick it.
- [ ] Set Link to 0 % on a mix with a loud hit on one side only: the other side no longer dips with it.

## Loudness

- [ ] Click the read-out panel, play the whole song, and check **Integrated** settles to a steady number. Short-term moves with the music, and the LUFS bar follows it.
- [ ] Pick **Broadcast · -23 LUFS** in Target: the red line on the LUFS bar moves down. The sound doesn't change.
- [ ] Pick **Custom…**, type `-12`, and press Enter: the dropdown reads **Custom · -12.0 LUFS** and the red line moves.
- [ ] Render the song (File > Render) with the plugin window closed, then open it: Integrated shows the loudness of the whole render.

## EDIT drawer

- [ ] Click **⚙ EDIT**: the drawer covers the left part with Oversampling, Oversampling filter, Dither, Meter ballistics and Reset loudness. **Done** closes it.
- [ ] Change Oversampling while stopped: the plugin's latency in REAPER's FX chain changes, and playback is still in time with the other tracks. The tooltip mentions the latency.
- [ ] Set Dither to 16-bit shaped and render a 16-bit WAV: no errors, and it sounds the same.

## Presets, menus, undo, A/B and Compare

- [ ] The preset browser lists **Mastering**, **Mixing** and **Utility**, and the ‹ › arrows step through the presets.
- [ ] Hovering a row in any menu shows a red bar at its left, and the current choice has a red check mark.
- [ ] Click the Release value and type `0.2 s`: Release moves to 200 ms.
- [ ] Move a control, click **↶**: it goes back. Set something on **B**, switch A and B while playing: no clicks.
- [ ] With 6 dB of limiting, click **≈ Compare**: you hear the unlimited input at about the same loudness. Click again to go back.
- [ ] The ON pill switches cleanly, and with it off the sound is the untouched mix, in time.
- [ ] Drag the window's corner to make it bigger and smaller: everything scales together, nothing overlaps.

## Gain lock, Auto to target, DC filter, release curve and overs (Phase 4 Wave 1)

- [ ] Open a project saved with v1.6.0 that uses Fancy Limit: it loads and sounds as before, with every new setting off or at its default.
- [ ] Gain lock on, then lower Threshold: the limiting increases but the level doesn't.
- [ ] Auto to target with Target -14 LUFS: after 10 s Threshold moves and the integrated loudness lands near -14.
- [ ] 5 Hz high-pass on a track with DC offset: the offset goes, the bass stays.
- [ ] Release curve Linear sounds steadier on sustained material; Exponential is the old sound.
- [ ] Overs stays 0 in normal use; a click resets it.

## Mac (docs/mac.md, step 5)

On a Mac (Apple silicon, and Intel if one is available), after installing with `install.sh`:

- [ ] The VST3 and the Audio Unit load in Ableton Live: window, pop-ups and resizing work; Cmd-click (not Ctrl-click) on a control opens the typed-value box.
- [ ] The checks above, once, with the VST3 in Ableton Live.
- [ ] A project saved on Windows opens on the Mac with every setting, and a render matches the Windows render by ear (CI holds them to within -100 dB).
- [ ] The Audio Unit in Live: automation follows, an export matches playback, and the Set reopens as it was. (Logic too, if someone has it.)

## Clip before limiting (Phase 4 Wave 2)

- [ ] Open a project saved with v1.6.0 that uses Fancy Limit: it sounds as before, Clip off.
- [ ] A drum bus pushed 8 dB, Clip before limiting on, Clip 3 dB: louder at the same Ceiling, the limiter's red reduction shallower, the clipped peaks orange in the history and named in the legend.
- [ ] Clip 0 and 12: barely any orange, then a lot; the Clip knob dims while the checkbox is off.
- [ ] The output never passes Ceiling (dBTP with True peak) with the clipper on.
- [ ] Render offline twice: the renders match.

## Bands (Phase 4 Wave 3)

- [x] The window is taller (760 x 637): the BANDS card sits between the history and the LIMITER card, and nothing else is squeezed. A project saved with v1.7.1 opens with Bands at 1 and sounds as before.
- [x] A full mix pushed 6 to 8 dB, Bands 3: the vocal and cymbals pump less with the kick than at Bands 1; band 1's red drops most on the strip, and each GR read-out moves.
- [x] Drag a blue crossover line: it moves, stops a third of an octave from its neighbour, and the sound follows without clicks. Click a line without dragging: a box asks for the frequency; `2.5k` works. Ctrl+Z takes it back.
- [x] Change Bands from 1 to 3 while stopped: REAPER's PDC shows 5 ms more (240 samples at 48 kHz). Changing it while playing fades out and back in.
- [x] Each band's Release knob changes how fast that band's red recovers; double-click resets it; click its value to type one.
- [x] Listen to removed with 3 bands plays what's taken away; the output never passes Ceiling (dBTP with True peak) at 2, 3 or 4 bands.
- [x] Render offline twice at 3 bands: the renders match.
