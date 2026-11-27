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

## Reverb EQ, Mod rate, gate and Auto gain (Phase 4 Wave 1)

- [ ] Open a project saved with v1.6.0 that uses Fancy Room: it sounds as before, and the new EDIT controls are flat, 1.00×, gate and Auto gain off.
- [ ] Reverb EQ: Mid +6 dB at 2 kHz brightens the reverb only (Dry unchanged); the shelves cut the tail's lows and highs. Moving the knobs while playing: no zipper or clicks.
- [ ] Mod rate: with Mod at 50 %, 0.25× is a slow chorus, 4× a fast shimmer. Turning it while a long tail rings: no clicks or pitch jumps.
- [ ] Gate the tail on a snare, Decay 3 s: the reverb stops dead when the snare does; Threshold, Hold and Decay act as labelled.
- [ ] Auto gain on: turning Decay from 1 to 6 s and Size up and down keeps the reverb at about the same level. Off: back to the old behaviour.

## Low decay, Attack and Duck from (Phase 4 Wave 2)

- [ ] Open a project saved with v1.7.0 that uses Fancy Room: it sounds as before, the window is 529 px tall, and the EDIT drawer shows Low decay ×1.00, Attack 0 ms and Duck from Input.
- [ ] The main page: the display and both cards are a little taller, the meter and faders reach the footer, and nothing overlaps.
- [ ] **Low decay** on a long hall (Decay 3 s): ×2 makes the low end boom on after the rest has gone, ×0.5 tightens it. The highs don't change. Turning it while a tail rings: no clicks.
- [ ] Low decay ×4 with **Freeze** on: the frozen tail holds steady and doesn't grow.
- [ ] **Attack** 150 ms on a vocal: the reverb swells in behind the voice. At 500 ms on a snare, listen for a flam; the early reflections still start at once. Sweeping Attack while playing: no clicks.
- [ ] **Duck from Sidechain**: put Room on a drum bus, route a vocal into pins 3/4, Ducking 100 %: the drum reverb ducks while the vocal sings and blooms in the gaps. The I/O pill shows the sidechain's pins.
- [ ] Duck from Sidechain with nothing routed and Ducking above 0: "No sidechain signal" shows after about 2 s, and the reverb isn't ducked until something is routed.
- [ ] **Listen to ducking**: plays only what's taken away; it's dim at Ducking 0, off after closing and reopening the window, and not in the saved project.

## Shimmer (Phase 4 Wave 3, v1.7.2)

- [x] Open a project saved with v1.7.1 that uses Fancy Room: it sounds as before; the EDIT drawer shows Shimmer 0 % and Interval Octave up, in the bottom-left corner, level with Decay shape, and the window is still 529 px tall.
- [x] Shimmer 50 %, Octave up, on a slow melody or a pad: octaves rise out of the tail and it fades out. 100 %: longer and brighter, and it still fades out, also at Decay 10 s and Size 100 %.
- [x] Try each Interval: Fifth up, Fourth up, Octave and fifth up, Two octaves up, Octave down. None runs away or gets harsh in the highs; High cut darkens the octaves.
- [x] Shimmer 100 %, Fifth up, on a held sine or a single sustained synth note: the tail never gets much louder than the note itself.
- [x] Shimmer 75 %, then Freeze: the held sound keeps its octaves and stays at one level.
- [x] Turn Shimmer up and down while a tail rings, and switch Interval: no clicks.
- [x] Automate Shimmer, render offline twice: the renders match.

## Mac (docs/mac.md, step 5)

On a Mac (Apple silicon, and Intel if one is available), after installing with `install.sh`:

- [ ] The VST3 and the Audio Unit load in Ableton Live: window, pop-ups and resizing work; Cmd-click (not Ctrl-click) on a control opens the typed-value box.
- [ ] The checks above, once, with the VST3 in Ableton Live.
- [ ] A project saved on Windows opens on the Mac with every setting, and a render matches the Windows render by ear (CI holds them to within -100 dB).
- [ ] The Audio Unit in Live: automation follows, an export matches playback, and the Set reopens as it was. (Logic too, if someone has it.)
