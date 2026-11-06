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
| 2×, 4×, 8× | Whole samples, reported to the host. Measured values are in `docs/measurements/exciter.txt` |

Latency doesn't depend on the sample rate in samples. The dry path is delayed by the same amount, and bypass uses the delayed dry path.

Changing oversampling while playing fades the output out, swaps the filters, reports the new latency and fades back in. The audio drops out for about 40 ms in total. All oversamplers are built in `prepareToPlay`, so nothing allocates on the audio thread.

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
