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
