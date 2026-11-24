# Fancy Sature: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy Sature** to a bass, drum or vocal track.

## Look

- [ ] The title strip reads **FANCY SATURE** with SATURE in orange, and the window matches `reference/phase3/19__saturate_mockup.png` below the header: the Type row, the curve, the harmonics bars, the DRIVE and BAND cards, Auto gain, Listen to added harmonics and ⚙ EDIT, and Out, Wet and Dry on the right.

## Saturation

- [ ] Play the track and raise **Drive**: it gets thicker and grittier, and with **Auto gain** on the level stays about the same. Untick Auto gain: it gets louder as you drive.
- [ ] Click through **Tape, Tube, Transistor, Clip** while playing: each sounds different, with no click. The curve and the bars change.
- [ ] Turn **Character** left: the bars at H2, H4 and so on grow. Turn it right: the odd ones (H3, H5…) grow and the even ones shrink.
- [ ] **Tone** up brightens the saturated sound, down darkens it.
- [ ] On a bass, set **Low** to about 120 Hz and push Drive: the low end stays clean and tight while the upper part growls. Turn Low fully left: it reads **Off**.
- [ ] Set **High** to 4 kHz on a bright source: the top stays clean.
- [ ] Wet at 0 dB, Dry at 0 dB, Drive at 0: the sound is just louder, with no phasing or hollowness.
- [ ] Tick **Listen to added harmonics**: you hear only what the saturation adds, the checkbox is filled orange and the title strip shows an orange **LISTENING** tag. Close the window: the sound is normal. Open it: Listen is off.

## EDIT drawer

- [ ] Click **⚙ EDIT**: Oversampling (4x), its latency note, Oversampling filter, Input trim and DC filter. **Done** closes it.
- [ ] Switch Oversampling while playing: a short dip, no click, and the track stays in time with the others. With Off, the note says it aliases.
- [ ] Linear phase shows more latency than Minimum phase, and the track stays in time either way.

## Presets, menus, undo, A/B and Compare

- [ ] The preset browser lists **Mix**, **Vocals**, **Drums** and **Bass**.
- [ ] Click Drive's value and type `18`: Drive moves to +18.0 dB. Type `off` into Low's value: it reads Off.
- [ ] Move Drive, press Ctrl+Z: it goes back. Switch A and B while playing: no clicks.
- [ ] The ON pill switches cleanly. Render the track: it sounds the same as playback.
- [ ] Drag the window's corner: everything scales together.

## Mac (docs/mac.md, step 5)

On a Mac (Apple silicon, and Intel if one is available), after installing with `install.sh`:

- [ ] The VST3 and the Audio Unit load in Ableton Live: window, pop-ups and resizing work; Cmd-click (not Ctrl-click) on a control opens the typed-value box.
- [ ] The checks above, once, with the VST3 in Ableton Live.
- [ ] A project saved on Windows opens on the Mac with every setting, and a render matches the Windows render by ear (CI holds them to within -100 dB).
- [ ] The Audio Unit in Live: automation follows, an export matches playback, and the Set reopens as it was. (Logic too, if someone has it.)

## Outer bands (Phase 4 Wave 2)

- [ ] Open a project saved with v1.6.0 that uses Fancy Sature: it sounds as before.
- [ ] Low 200 Hz on a full mix, EDIT > Outer bands, Low drive 18 dB: the low end thickens, the mids don't change; the BAND card tints and names the low band. Low mix 0 %: back to the clean low end.
- [ ] High 6 kHz, High drive 12 dB: the top gets grit without touching the mids.
- [ ] Low at Off: the low pair dims and does nothing.
- [ ] Auto gain on: turning Low drive up and down keeps the level steady within about 1 dB.
- [ ] Listen to added harmonics plays all three bands' harmonics.

## Diode, Fold, Offset, cuts, Spread and Channels (Phase 4 Wave 2, v1.7.1)

- [ ] Open a project saved with v1.7.0 that uses Fancy Sature: it loads and sounds as before.
- [ ] The Type row reads Tape, Tube, Transistor, Clip, **Diode**, **Fold**, all on one row. Diode buzzes; Fold turns metallic as Drive rises; Auto gain keeps both near the same loudness at 4x.
- [ ] The drawer shows **Shape | Tone and stereo** beside EDIT. Shape has **Offset** under Input trim: on a bass at Drive 12, Offset 0.25 thickens it (more 2nd harmonic) with no thump of DC.
- [ ] Tone and stereo: on a full mix, Low cut 200 Hz **Pre** cleans up the mud a loud bass adds at high Drive, and the bass is still there. **Post** keeps the drive and only thins the added part. High cut 8 kHz Post takes the fizz off.
- [ ] **Spread** 50 % on a mono source widens the saturation while the note stays in the centre; in mono it's only a little thinner.
- [ ] **Channels** Mid only on a mix with a centre bass and wide pads: the bass is driven, the pads stay clean. Switching while playing fades, no click.
