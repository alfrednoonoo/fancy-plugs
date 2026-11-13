# Fancy Room: manual

An algorithmic reverb, from a small room to a long hall. Decay sets the length in seconds whatever the Size, the tail is kept smooth so it doesn't ring, and it can freeze, sync its pre-delay to the tempo, work as a send and duck under the dry signal. Design: `docs/room.md`.

## Controls

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

CPU at 48 kHz is in `docs/measurements/room.txt`.

## Presets

Default; Small room (Rooms); Drum room (Drums); Vocal plate and Ducked vocal (Vocals); Big hall (Halls); Ambient wash (Ambient); Send, all wet (Sends).
