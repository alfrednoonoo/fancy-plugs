# Fancy Room: manual

An algorithmic reverb, from a small room to a long hall. Decay sets the length in seconds whatever the Size, the tail is kept smooth so it doesn't ring, and it can freeze, sync its pre-delay to the tempo, work as a send and duck under the dry signal. Design: `docs/room.md`.

## Controls

The knobs sit in two cards: **SPACE** (Pre-delay, Size, Decay, Diffusion, Mod and Width) and **TONE** (Damping, Low cut and High cut). The rest are in the footer, the right column and the EDIT drawer.

| Control | Range | Default | What it does |
|---|---|---|---|
| Pre-delay | 0 to 250 ms | 12 ms | A gap before the reverb starts. With Sync pre-delay on, a note length instead |
| Size | 0 to 100 % | 55 % | How big the space is: the spacing of the early reflections and of the delays in the tail |
| Decay | 0.10 to 20.0 s | 1.80 s | How long the tail takes to fall by 60 dB (RT60), at any Size |
| Diffusion | 0 to 100 % | 70 % | How quickly the echoes blur into a smooth tail |
| Mod | 0 to 100 % | 25 % | Slow movement of the delays in the tail, which keeps it from ringing |
| Width | −1.00 to +2.00 | 1.00 | 0 is mono, 1 as it is, 2 wider; below 0 swaps the sides. The mono sum never changes |
| Damping | 0 to 100 % | 45 % | How much faster the highs die away: their decay is Decay × (1 − 0.7 × Damping) |
| Low cut | 20 Hz to 1 kHz | 150 Hz | Takes the lows out of the reverb |
| High cut | 1 to 20 kHz | 9.0 kHz | Takes the highs out of the reverb |
| Sync pre-delay | on/off | off | Pre-delay as a note at the host's tempo |
| Freeze | on/off | off | Holds the tail |
| Wet only (send) | on/off | off | Only the reverb comes out |
| Out | | | The output's peak level |
| Wet | −inf to +12 dB | −12 dB | The reverb |
| Dry | −inf to +12 dB | 0 dB | The untouched signal |

**Sync pre-delay** uses Fancy Delay's notes: 1/64, 1/32, 1/16 and 1/8, each straight, dotted or triplet, at the host's tempo, or 120 BPM if the host gives none. The default is 1/32. Pre-delay stops at 250 ms: a longer note, such as 1/8 dotted at 120 BPM (375 ms), shows in red, and its tooltip says so. The ms value is kept, so turning Sync off brings it back.

**Freeze** fades the input to the tail out over 20 ms and holds what's there, with nothing lost, for as long as it's on. Size, Decay and Damping wait while it's frozen and glide to where they are when it's off. New playing still sounds through the early reflections, over the frozen pad, and everything after the tail (Pre-delay, Width, Low cut, High cut, Ducking, Wet and Dry) still works. While frozen, the plugin tells the host its tail is endless.

**Wet only (send)** treats Dry as off and greys its fader, without changing it, so turning it off brings Dry back. The ON pill still passes the dry signal.

**Rendering.** The plugin tells the host how long its tail is: Pre-delay, plus the longest reflection, plus 1.5 × Decay, where the tail is 90 dB down. Hosts that use it render the whole tail on export.

## Display

The tail's level over 0 to 3 s, longer when Decay needs it, from 0 to −60 dB: the early reflections as thin white lines after Pre-delay, the overall decay filled in purple, and the highs' decay dashed, steeper with more Damping. The marker shows where it reaches −60 dB. It's worked out from the settings, so it shows even when nothing is playing.

## EDIT

| Setting | Range or choices | Default | Notes |
|---|---|---|---|
| Quality | Eco, Normal, High | Normal | Eco runs 8 delay lines for the least CPU. Normal runs 16; High adds smoother (cubic) modulation. On a change, the old tail rings out for up to 2 s while the new one starts, so nothing cuts off |
| Stereo input | Stereo, Mono-sum | Stereo | Mono-sum feeds the reverb the sum of both channels |
| Ducking | 0 to 100 % | 0 % | Turns the reverb down while the dry signal plays: at 100 %, by 24 dB once the input is above −20 dBFS, and not at all below −50 dBFS. It comes back over about 300 ms |
| Early reflections | −inf to +6 dB | 0 dB | The level of the first echoes |
| Reverb EQ: Low | 20 Hz to 1 kHz, ±12 dB | 200 Hz, 0 dB | A low shelf on the reverb, after Low cut and High cut. A band at 0 dB is out of the path |
| Reverb EQ: Mid | 200 Hz to 8 kHz, ±12 dB, Q 0.3 to 8 | 1 kHz, 0 dB, 1.00 | A bell |
| Reverb EQ: High | 1 to 20 kHz, ±12 dB | 6 kHz, 0 dB | A high shelf |
| Mod rate | 0.25× to 4× | 1.00× | Scales the speed of every line's modulation: slow for a lush chorus, fast for shimmer. Mod still sets the depth |
| Gate the tail | on, off | off | When the input stays under Threshold (−60 to 0 dB, −30) for Hold (10 to 500 ms, 80), the reverb fades out over Decay (5 to 500 ms, 40): the gated drum sound |
| Auto gain | on, off | off | Trims the wet level as Size and Decay change, so a longer decay doesn't make the reverb louder. 0 dB at Size 55 % and Decay 1.8 s |
| Duck from | Input, Sidechain | Input | What Ducking listens to. Sidechain reads Room's optional stereo sidechain bus (pins 3/4 in REAPER), so a reverb on the drums can make room for the vocal: down while the vocal sings, back in its gaps. With nothing on the sidechain, Ducking has nothing to follow and leaves the reverb alone, and while Ducking is above 0 the main window shows "No sidechain signal". (A host that switches the sidechain bus off altogether gives Ducking the input instead) |
| Listen to ducking | on, off | off | Plays only what Ducking takes away from the reverb, without the dry signal, to hear what it does. Temporary: it's never saved, and it switches off when the window closes. Dimmed while Ducking is 0 |
| Decay shape: Low decay | ×0.25 to ×4 | ×1.00 | How long the lows (below about 250 Hz) ring compared with Decay: ×2 doubles their decay time, ×0.5 halves it. The change fades out by 1 kHz; Damping still sets the highs. At ×1 it's out of the path |
| Decay shape: Attack | 0 to 500 ms | 0 ms | Makes the late tail swell in over this time instead of starting at full level, keeping its overall level. The early reflections still start at once. On sharp hits at long settings the swell can sound like a quick flam |
| Shimmer | 0 to 100 % | 0 % | Feeds the tail back into the room an interval higher on each pass (see Shimmer). At 0 % it's off |
| Shimmer: Interval | Octave up, Fifth up, Fourth up, Octave and fifth up, Two octaves up, Octave down | Octave up | How far each pass is shifted |

The window is 64 px taller than before (529 px at 100 %) to fit the Decay shape group.

CPU at 48 kHz is in `docs/measurements/room.txt`.

## Shimmer (Phase 4)

**Shimmer** and **Interval**, in the EDIT drawer's bottom-left corner under the gate, feed the room's tail back into it through a pitch shifter, so each pass comes back an interval higher: octaves rising out of the tail, the classic shimmer sound on pads and slow melodies.

- **Shimmer** (0 to 100 %): how much goes round. At 0 % it's off and Room sounds as it did before. Higher values make the tail longer and brighter, but it always dies away, even at 100 % with a long Decay; Freeze is the way to hold it.
- **Interval**: **Octave up** (the default), **Fifth up**, **Fourth up**, **Octave and fifth up**, **Two octaves up** or **Octave down**, the same list as Fancy Verb's. These stay consonant as they stack; Octave down thickens the tail instead of brightening it.
- What goes round passes through Low cut and High cut first, and only what's above 200 Hz, so the low end doesn't pile up and the highest octaves thin out the way the room's highs do. A guard stops a held note from building up much louder than your playing: on a sustained pure tone the tail can bloom a few dB, never more.
- Freeze with Shimmer: freezing stops the loop along with the input, so the held sound keeps the octaves that were sounding and stays at one level.
- Shimmer roughly doubles the tail's length at 100 %, so Room tells the host its tail is three times as long while Shimmer is up.

## Presets

Default; Small room (Rooms); Drum room (Drums); Vocal plate and Ducked vocal (Vocals); Big hall (Halls); Ambient wash (Ambient); Send, all wet (Sends).
