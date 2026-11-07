# Fancy Gate: manual

A noise gate and ducker with hysteresis, hold, optional 50 ms lookahead, a sidechain input and MIDI triggering.

## How it opens and closes

The gate opens when the detector's level reaches **Threshold**. It closes only when the level falls below Threshold plus **Hysteresis**, the dashed line, so a signal hovering around the threshold doesn't make it chatter. After the level falls below that line, **Hold** keeps the gate open, then **Release** closes it.

Opening takes **Attack**. At 0 ms the gate still fades over 0.5 ms, or 32 samples above 64 kHz, so it never clicks. Release has the same minimum.

A gate that's closing re-opens on the next hit. A gate that's open doesn't re-trigger until the level has dropped below Threshold again.

## The display

| Part | What it shows |
|---|---|
| Threshold meter | The input level, with the Threshold handle over it. The dashed line is the close level. Below it are Threshold and the close level in dB |
| Gain | How far the gate turns the signal down right now, and the light: **open** (yellow), **hold** (orange) or **closed** (grey) |
| History | The last 3 s: the input level as a grey fill and the gate's gain as a yellow line, with the open and close levels. Click or drag on it to set Threshold |

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Threshold | -90 to 0 dB | -40 dB | Level where the gate opens |
| Hysteresis | -24 to +12 dB | -6 dB | Where it closes, relative to Threshold |
| Pre-open | 0-50 ms | 5 ms | How early the gate starts to open, before the hit. Needs Lookahead |
| Attack | 0-500 ms | 0.5 ms | How fast it opens |
| Hold | 0-2000 ms | 20 ms | How long it stays open after the level falls |
| Release | 1-5000 ms | 100 ms | How fast it closes |
| Range | -inf to 0 dB | -inf | How far the closed gate turns down. -inf is silence |
| RMS size | 0-250 ms | 0 ms | How long the detector averages. 0 is peak detection |
| Lowpass, Highpass | 20 Hz-22 kHz | Off | Filters on the detector only. A Highpass at 100 Hz stops bass spill opening a vocal gate |
| Detector input | Main or Sidechain, L+R, L or R | Main input L+R | What the gate listens to |
| Lookahead | On or off | Off | Delays the audio by 50 ms, so the gate can open before the hit. Adds 50 ms latency, which REAPER compensates |
| Mode | Gate or Duck | Gate | Duck turns the signal down by Range while the detector is over Threshold |
| Preview filter | On or off | Off | Plays what the detector hears, through Lowpass and Highpass |
| Link channels | On or off | On | Both channels open together, on the louder one |
| Wet, Dry | -inf to +12 dB | 0 dB, -inf | The gated signal and the untouched signal |
| Noise | -inf to 0 dB | -inf | White noise, only while the gate is open. In Duck mode, while it ducks |

Every knob and fader takes typed values: click the value, or Ctrl-click the control. Frequencies accept `2500` and `2.5k`, and the MIDI note accepts `A4`, `C#3` or a number.

## Pre-open

Pre-open only works with Lookahead on. Without it, the knob is dimmed, reads "off" and can't be changed. Clicking it explains why and makes the Lookahead checkbox flash. It never turns Lookahead on for you, because that changes the latency.

With Lookahead on, the latency is always 50 ms. Pre-open only sets how early within that the gate opens, so moving it never changes the latency.

## Sidechain in REAPER

1. On the track that should trigger the gate, open the routing window and add a send to the gated track.
2. Set the send to go to **channels 3/4** of the gated track.
3. On the gated track, set the track's channel count to 4.
4. In Fancy Gate, set **Detector input** to **Sidechain L+R**.

If a Sidechain source is chosen and nothing arrives for 2 s while the main input plays, the history shows "No sidechain signal: check track routing (REAPER: pins 3/4)".

## MIDI

Tick **MIDI on open/close** to send a note when the gate opens and its note-off when it closes. The dropdown beside it sets the note, the channel and the velocity window. To play an instrument from it in REAPER, add a send from the gate's track to the instrument's track with MIDI only, or put the instrument after Fancy Gate on the same track.

- **Velocity from level off:** every note has velocity 100, and lands exactly where the gate is heard opening.
- **Velocity from level on:** the velocity is the hit's peak over the velocity window after the gate opens, from -60 dBFS (1) to 0 dBFS (127). Without Lookahead, the note goes out one window late, 2 ms by default, because the peak isn't known until then. With Lookahead, it's on time unless Pre-open is within one window of 50 ms. The checkbox's tooltip gives the delay whenever there is one.

Turning MIDI off, or changing Lookahead, ends a sounding note.

## Factory presets

| Preset | For |
|---|---|
| Default | A clean start: Threshold -40, hard gating |
| Vocal clean-up | Gentle -18 dB gating of a vocal's pauses, with a 100 Hz detector high-pass |
| Kick tight | A short kick, listening below 150 Hz |
| Snare (with spill) · lookahead | Snare with hi-hat spill, opening 2 ms early |
| Tom (with spill) · lookahead | Toms with cymbal spill, listening below 400 Hz |
| Duck under kick | Ducks by 8 dB from a kick on the sidechain |
| Drum trigger to MIDI | Notes with velocity from each hit |
| Gentle expander | -12 dB of downward expansion |

## Latency

0 samples with Lookahead off. 50 ms with it on, at every sample rate.
