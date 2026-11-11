# Fancy Sature: manual

A saturator with four characters. It adds harmonics, the warmth or grit of driving tape, valves or transistors, without making the track louder, and can keep the lows or the highs clean. Design: `docs/sature.md`.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Type | Tape, Tube, Transistor, Clip | Tape | The curve. See below |
| Drive | 0 to 36 dB | +6 dB | How hard the signal is pushed into the curve |
| Character | even 100 % to odd 100 % | even 30 % | Left: more even harmonics (2nd, 4th…), warm and full. Right: more odd ones (3rd, 5th…), edgier |
| Tone | −6 to +6 dB | 0 dB | Tilts the saturated sound around 1 kHz: +6 is 3 dB brighter at the top and 3 dB less at the bottom |
| Low | Off, 20 Hz to 2 kHz | Off | Below this, the signal passes clean |
| High | 1 kHz to 20 kHz, Off | Off | Above this, the signal passes clean. Kept at least an octave above Low |
| Auto gain | on/off | on | Keeps the loudness about the same at any setting |
| Listen to added harmonics | on/off | off | Plays only what the saturation adds. Temporary: see below |
| Out | | | The output's peak level |
| Wet | −inf to +12 dB | 0 dB | The saturated signal |
| Dry | −inf to +12 dB | −inf | The clean signal, for parallel saturation |

**Types.** Tape is soft and warm, and loses a little top end above Drive 12 dB, as tape does. Tube is soft and uneven, so it makes even harmonics even with Character neutral. Transistor has a harder knee. Clip is hard clipping with a small rounded corner.

**Auto gain** doesn't follow the music, so it can't pump: it's a fixed correction for each Type, Drive, Character and Tone, measured on pink noise when the plugin is built. With Low or High set, it still assumes the whole signal is saturated, so it may turn down a little too much. Raising Input trim (EDIT) drives harder without its correction.

**Low and High** split with steep (48 dB/oct) filters, and only the band between them is saturated. Everything else comes out exactly as it went in, so mixing the bands back never thins or phases the sound.

**Wet and Dry** stay lined up at every setting: Drive 0 with both at 0 dB is just 6 dB louder, flat.

**Listen to added harmonics is temporary,** so it can't end up in an export by mistake. While it's on, the checkbox is filled orange and the title strip says **LISTENING**. It turns off when you close the plugin's window, and it's never saved. It isn't automatable, and undo and A/B leave it alone.

## Displays

- **in → out:** the current curve. The dashed line is "no change".
- **Harmonics:** what the current settings add to a 1 kHz tone at −12 dBFS: the tone dimmed, even harmonics in full orange, odd ones lighter, and the THD. It's worked out from the settings, so it shows even when nothing is playing.

## EDIT

| Setting | Choices | Default | Notes |
|---|---|---|---|
| Oversampling | Off, 2x, 4x, 8x | 4x | Higher is cleaner on bright, heavily driven sounds. Off has no latency but aliases more |
| Oversampling filter | Minimum phase, Linear phase | Minimum phase | Linear phase has more latency. The clean and saturated signals stay lined up either way |
| Input trim | −24 to +24 dB | 0 dB | The level going in |
| DC filter | on/off | on | Removes the offset an uneven curve can add |

Latency at 48 kHz is in `docs/measurements/sature.txt`.

## Presets

Default; Tape warmth and Top sparkle (Mix); Tube glow (Vocals); Transistor edge, Clip it and Parallel crush (Drums); Bass drive (Bass).
