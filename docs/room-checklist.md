# Fancy Room: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy Room** to a vocal or drum track.

## Look

- [ ] The title strip reads **FANCY ROOM** with ROOM in purple, and the window matches `reference/phase3/18__room_mockup.png` below the header: the decay display, the SPACE and TONE cards, Sync pre-delay, Freeze, Wet only (send) and ⚙ EDIT, and Out, Wet and Dry on the right.

## Reverb

- [ ] Play the track: a smooth room, with no metallic ringing on snare hits or vocal consonants.
- [ ] Turn **Decay** from 0.5 s to 8 s: the tail gets longer, the display's slope and marker follow, and the room's size doesn't change. Turn **Size**: the room sounds bigger or smaller, and the decay time stays.
- [ ] **Damping** up darkens the tail, and the dashed line gets steeper. **Low cut** and **High cut** thin and darken the reverb only.
- [ ] **Width** to 0: the reverb is mono. To 2: wider. Below 0: the sides swap. Check in mono (REAPER's master mono button): the reverb's level hardly changes at any Width.
- [ ] **Pre-delay** to 80 ms: a clear gap before the reverb. Tick **Sync pre-delay**: the knob shows a note (1/32). Pick 1/8 dotted at 120 BPM: it turns red, and its tooltip says Pre-delay stops at 250 ms. Untick Sync: the ms value is back.
- [ ] Tick **Freeze** on a held chord: the tail holds, with no growth or fading, for a minute. Play on: you hear the new notes' reflections over it. Untick: the tail decays away.
- [ ] Tick **Wet only (send)**: the dry signal goes, and the Dry fader greys with a tooltip saying why. Untick: Dry is as it was.
- [ ] Render 2 bars that end in silence: the render holds the whole tail, not cut off.

## EDIT drawer

- [ ] Click **⚙ EDIT**: Quality (Normal), Stereo input (Stereo), Ducking and Early reflections. **Done** closes it.
- [ ] Switch **Quality** while a long tail plays: the tail doesn't cut off or click.
- [ ] **Ducking** at 60 % on a vocal: the reverb drops while the singer sings and blooms in the gaps.
- [ ] **Early reflections** to −inf: the start of the reverb softens, and the display's white lines go.

## Presets, menus, undo, A/B and Compare

- [ ] The preset browser lists **Rooms**, **Drums**, **Vocals**, **Halls**, **Ambient** and **Sends**.
- [ ] Click Decay's value and type `2.5`: Decay moves to 2.50 s. Type `500 ms`: 0.50 s.
- [ ] Move Decay, press Ctrl+Z: it goes back. Switch A and B while playing: no clicks.
- [ ] The ON pill switches cleanly. Render the track: it sounds the same as playback.
- [ ] Drag the window's corner: everything scales together.
