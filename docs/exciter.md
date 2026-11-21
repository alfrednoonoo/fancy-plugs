# Fancy Exciter: design note

## DSP

An original parallel high-frequency exciter:

```
in ──┬─ delay (matches oversampling latency) ───────────────────────────────┐
     │                                                                      + ─ × Output ─ bypass fade ─ out
     └─ high-pass ─ × Clip Boost ─ [ up ─ bias ─ ADAA shaper ─ down ] ─ high-pass ─ low-pass ─ × Harmonics × Mix Back ┘
```

- **High-pass filters:** 2nd-order Butterworth state-variable filters at Freq, before and after the shaper. The second removes the low intermodulation products the shaper creates, and it is also the DC blocker for the generated signal, since any bias from the even-harmonic setting is a DC offset. Because Freq is at least 500 Hz, a separate DC filter would do nothing extra.
- **Shaper:** a cubic soft clipper, `f(x) = x - 4x³/27` up to ±1.5 and ±1 beyond. It has unity gain for small signals, and its output never exceeds ±1, so high Clip Boost settings stay bounded.
- **ADAA:** first-order antiderivative anti-aliasing. The output is the difference of the antiderivative divided by the difference of its input, falling back to `f` at the midpoint when the two are almost equal. This adds half a sample of delay at the oversampled rate. At 4× that is 0.125 samples, and with oversampling off it is 0.5 samples, which isn't compensated.
- **Oversampling:** `juce::dsp::Oversampling` with equiripple FIR half-band stages and integer latency, so the wet path is linear-phase and the dry path can be delayed to match exactly. Stages use a -100 dB stopband first, then -90 dB and -80 dB. JUCE's preset stops at -75 dB, which doesn't leave enough margin for the §5.5 target.
- **Post low-pass:** a 2nd-order Butterworth at 0.45·fs, after the oversampling filters.
- **Even/odd character:** a bias added before the shaper makes it asymmetric, which creates even harmonics. The bias is `0.75 × (1 - Character)`, and `f(bias)` is subtracted so the shaper still outputs 0 for silence.

With Harmonics at 0 %, the wet path is multiplied by exactly 0, so the output equals the delayed input bit for bit.

## Parameters

| ID | Name | Range | Default | Unit | Skew | Source |
|---|---|---|---|---|---|---|
| `freq` | Freq | 500–16000 | 2000 | Hz | log | §3.2 |
| `clipBoost` | Clip Boost | 0–24 | 0.0 | dB | linear | §3.2 |
| `harmonics` | Harmonics | 0–100 | 0 | % | linear | §3.2 |
| `mixBack` | Mix Back | -60–+6, bottom = -inf | -6.0 | dB | linear | §3.2 |
| `oversampling` | Oversampling | Off, 2×, 4×, 8× | 4× | | | §3.2 |
| `character` | Character | 0–100 (even to odd) | 50 | % | linear | range chosen here |
| `output` | Output | -24–+12 | 0.0 | dB | linear | range chosen here |
| `bypass` | Bypass | off/on | off | | | §4.6 |

The header knob and the EDIT drawer's Output knob control the same `output` parameter, as on the Kicker.

## Smoothing

One-pole smoothers, per sample: Freq over 30 ms in the log domain, with filter coefficients recomputed every sample while it moves, and gains over 20 ms. Character is smoothed over 30 ms and ramped linearly across each internal block of up to 64 samples. Bypass fades over 10 ms.

## Latency

| Oversampling | Latency |
|---|---|
| Off | 0 samples |
| 2× | 55 samples |
| 4× | 69 samples |
| 8× | 74 samples |

Latency doesn't depend on the sample rate in samples. The dry path is delayed by the same amount, and bypass uses the delayed dry path.

Changing oversampling while playing fades the output out, swaps the filters, reports the new latency, waits for the delay line to refill and the filters to settle, then fades back in. The audio drops out for about 50 ms plus the new latency. All oversamplers are built in `prepareToPlay`, so nothing allocates on the audio thread.

## State

The same format as the Kicker: XML with `stateVersion` 1, `uiScale` and `presetName`, with the same validation.

## §5.5 fixes

| Requirement | How it is met | Test |
|---|---|---|
| Oversampling, default 4×, plus ADAA | Both, around the shaper | `aliasing is below -80 dBFS at 4x` |
| Post-shaper low-pass and DC blocker | 0.45·fs low-pass; the post high-pass blocks DC | `wet path blocks DC` |
| Even/odd character | Bias before the shaper | `character moves between even and odd harmonics` |
| Unity at Harmonics 0 % | Wet multiplied by 0, dry delay-matched | `harmonics at 0 is the delayed input exactly` |

## Layout

Base window is 392 × 432, the Figma Exciter frame without its 22 px host title bar. The meter uses that frame's own scale, which is shorter than the Kicker's.

The EDIT drawer holds Character, Output and an Oversampling dropdown.

## Phase 4 Wave 1: listen to added harmonics, THD read-out, ceiling and auto gain

Items from `docs/enhancements.md` (Wave 1 of the Phase 4 order). **Approved by the user on 21 November 2026 with every recommended option (R).** Every new parameter is appended, with a default that keeps saved projects bit-identical, checked against the v1.6.0 DSP in a legacy test as Comp's Wave 1 does. Items were checked in the code on 21 November 2026 before this note was written. Backlog items 1, 2 and 7 of section 2.

### 1. Listen to added harmonics

A temporary listen (§0.3), a checkbox in the accent above the footer (EDIT and the name): plays only what Exciter adds (the output minus the latency-aligned input, before Output), so Harmonics and Clip Boost can be set by ear.

### 2. THD read-out

Like Sature's harmonics card, but as one line under the meters: a background thread runs a private copy of Exciter's path (Freq, Clip Boost, Harmonics, Mix Back, Character, oversampling) on a test tone whenever one of those changes, and the read-out under the Out meter says "THD 2.4 %".

- **Decision 1: the test tone.**
  - **A (R).** One octave above Freq, at −12 dBFS: always inside the band Exciter works on, so the figure moves with every knob.
  - **B.** A fixed 1 kHz, as Sature: comparable between settings, but it reads 0 % whenever Freq is above 1 kHz.

### 3. Ceiling and auto gain

- `ceiling` Off or −12 to 0 dBTP (default Off), in the drawer: the shared `TruePeakLimiter`, so Clip Boost 24 dB can't pass 0 dBFS.
- `autoGain` (off), in the drawer: the same background thread measures what the path adds to pink noise at −18 dBFS RMS and trims the output by that much, smoothed over 200 ms. Deterministic.

### 4. Parameters

| ID | Name | Range | Default |
|---|---|---|---|
| `ceiling` | Ceiling | Off, −12 to 0 dBTP | Off |
| `autoGain` | Auto gain | off/on | off |

### 5. Tests

1. Legacy: v1.6.0's Exciter against the new one at every oversampling: identical.
2. Listen: the listen output plus the latency-aligned input equals the normal output within −100 dBFS.
3. THD: the read-out matches an offline measurement of the same settings within 10 % of its value.
4. Ceiling: Clip Boost 24 dB on a full-scale signal stays under the ceiling in true peak.
5. Auto gain: on pink noise, output RMS within 0.5 dB of input RMS at Harmonics 100 %.

Mockup: `reference/phase4/39__exciter_w1_mockup.png` (source `design/phase4-mockup-src/w1_mockups.py`): the footer's listen, the THD line, the drawer.

### 6. As built

One push (`ff59906`), with every recommended option; `tests/ExciterW1Tests.cpp`.

- **Listen to added harmonics:** a temporary listen (`ListenCheckbox`), never saved; it plays the output minus the dry signal.
- **THD read-out:** at the bottom of the body, for a tone one octave above Freq at -12 dBFS through the current settings, worked out on a background thread (synchronously in an offline render).
- **Ceiling:** `ceiling`, Off or -12 to 0 dBTP, in the drawer; **Auto gain:** `autoGain`, turns the output down by the level the harmonics add, from the same background analysis, smoothed.
