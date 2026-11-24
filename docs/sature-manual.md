# Fancy Sature: manual

A saturator with four characters. It adds harmonics, the warmth or grit of driving tape, valves or transistors, without making the track louder, and can keep the lows or the highs clean. Design: `docs/sature.md`.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Type | Tape, Tube, Transistor, Clip, Diode, Fold | Tape | The curve. See below |
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

## Outer bands (Phase 4)

Low and High set the band Sature saturates; below Low and above High pass clean. **EDIT > Outer bands** drives those two as well:

- **Drive** (below Low, and above High): 0 to 36 dB. 0 leaves the band clean, as before.
- **Mix**: how much of that band's added harmonics you hear, 0 to 100 %.

The outer bands use the same Type, Character, Tone and Tape HF loss as the main band, and Auto gain keeps each at its level. Their edges are the same steep 48 dB/oct filters, so no frequency is driven twice. An outer band exists only while its edge is set: with Low at Off, the low pair dims. While either is driven, the BAND card takes a light tint and names it. Each driven outer band costs about as much CPU again as the main band.

## Diode and Fold (Phase 4)

Two more Types, after Clip:

- **Diode** passes one polarity and holds the other down to a tenth, like a rectifier: even and odd harmonics together, a rectified, buzzy colour. It distorts even at low Drive, since it bends right from zero.
- **Fold** folds the wave back past its ceiling instead of flattening it. Past a little Drive its 3rd harmonic is louder than the note itself: a metallic, synth-like sound for effects more than mixing.

Both alias no more than Clip at the same settings. Auto gain for these two is measured at 4x, so at Off and 2x Fold can be louder than the other Types at very high Drive.

## Shape and Tone and stereo (Phase 4)

The EDIT drawer has two pages, switched beside its title.

**Shape** has the oversampling settings, Input trim, DC filter and the outer bands, as before, and **Offset** (-0.5 to +0.5, 0 by default) under Input trim. Offset shifts the signal before the curve and back after it, so even a symmetric curve adds even harmonics, the 2nd first: on Tape at Drive 12 dB, 0.25 brings the 2nd harmonic up to about -21 dB while the 3rd stays where it was. The DC it adds is taken off after the curve. Character's even half does something similar but changes the curve too; Offset leaves it.

**Tone and stereo:**

| Setting | Range | Default | What it does |
|---|---|---|---|
| Low cut, Freq | Off, 20 Hz to 20 kHz | Off | A high-pass on the saturation |
| Low cut, Slope | 12, 24 dB/oct | 12 dB/oct | |
| Low cut, Pre / Post | | Pre | Pre cuts what's driven: the lows below the cut still pass, clean, but no longer drive the curve. On a bass-heavy mix a Low cut at about 200 Hz Pre stops the bass from muddying everything above it with intermodulation. Post cuts only the added harmonics |
| High cut, Freq | 20 Hz to 20 kHz, Off | Off | A low-pass on the saturation |
| High cut, Slope | 12, 24 dB/oct | 12 dB/oct | |
| High cut, Pre / Post | | Post | Post takes the fizz off the added harmonics without dulling the source; Pre keeps the highs from being driven |
| Spread | 0 to 50 % | 0 % | Widens the added harmonics on each side while the note itself stays centred. At 50 % the harmonics are fully decorrelated between left and right, and in mono they lose at most 3 dB |
| Channels | Stereo, Mid only, Side only | Stereo | Mid only saturates the centre (a bass, a vocal) and leaves wide parts clean; Side only does the reverse. Changing it fades the saturation out and back in |

Every new setting's default is the old sound, exactly.

## Presets

Default; Tape warmth and Top sparkle (Mix); Tube glow (Vocals); Transistor edge, Clip it and Parallel crush (Drums); Bass drive (Bass).
