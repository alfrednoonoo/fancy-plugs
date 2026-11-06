# Fancy Kicker: design note

## DSP

An original parallel low-frequency enhancer:

```
in ──┬───────────────────────────────────────── × Dry ──┐
     └─ mono blend ─ SVF band-pass ─ 15 Hz HPF ─ × Wet ──┴─ + ─ × Output ─ soft clip ─ bypass fade ─ out
```

- **Band-pass:** a topology-preserving state-variable filter with constant peak gain, so Wet sets the level at Freq directly. It stays stable under fast modulation, which is why it is used instead of a biquad. Both integrator states are double precision.
- **Subsonic filter:** a 2nd-order Butterworth high-pass at 15 Hz on the wet path, as a transposed direct form II biquad. The dry path is untouched, so Wet at -inf and Dry at 0 dB reproduces the input exactly.
- **Mono below Freq:** feeds the band-pass with L+R, making the added sub identical in both channels. It acts on the wet path only and fades over 20 ms.
- **Soft clip:** linear below 0.7, then a tanh knee that approaches ±1.0. It is off by default and fades over 20 ms.

## Parameters

| ID | Name | Range | Default | Unit | Skew | Source |
|---|---|---|---|---|---|---|
| `freq` | Freq | 20–240 | 50.0 | Hz | log | §3.1 |
| `wet` | Wet | -60–+12, bottom = -inf | -12.0 | dB | linear | §3.1 |
| `dry` | Dry | -60–+12, bottom = -inf | 0.0 | dB | linear | §3.1 |
| `q` | Filter Q | 0.5–10 | 2.0 | | log | range chosen here |
| `output` | Output | -24–+12 | 0.0 | dB | linear | range chosen here |
| `monoBelow` | Mono below Freq | off/on | off | | | §5.4 |
| `softClip` | Soft clip | off/on | off | | | §5.4 |
| `bypass` | Bypass | off/on | off | | | §4.6 |

The header knob and the EDIT drawer's Output knob control the same `output` parameter.

## Smoothing

All continuous parameters use one-pole smoothers, per sample: Freq and Q over 30 ms in the log domain, gains over 20 ms. Filter coefficients are recomputed every sample while Freq or Q is moving, and not at all once settled. Bypass fades over 10 ms.

## Latency and phase

Latency is 0 samples at every setting. The wet path is minimum-phase and is not delay-compensated. The band-pass has zero phase shift at Freq, so wet and dry add constructively there. The 15 Hz high-pass adds some phase lead on top: about 25° at 50 Hz, where wet and dry at equal level sum to about +5.8 dB instead of +6 dB. The lead grows towards the bottom of the range, so the sum at 20 Hz is noticeably less than +6 dB. The phase offset is part of the sound.

## State

XML with a `stateVersion` attribute (currently 1) plus `uiScale` and `presetName`. Unknown parameters are dropped, out-of-range values are clamped, non-finite values fall back to defaults, and unreadable data is ignored.

## §5.4 fixes

| Requirement | How it is met | Test |
|---|---|---|
| Click-free 20→240 Hz sweep in 200 ms | Per-sample log smoothing into a modulation-stable SVF | `frequency sweep is click-free` |
| DC blocker / subsonic HPF | 15 Hz 2nd-order HPF on the wet path | `wet path blocks DC` |
| Mono below Freq | Wet input summed to mono | `mono wet path` |
| Soft-clip safety | Optional output stage, off by default | `soft clip bounds the output` |
| Phase-coherent wet/dry | Documented as minimum-phase, see above | `wet and dry add in phase at Freq` |

## Layout

Base window is 392 × 472, the Figma Kicker frame without its 22 px host title bar, and all positions come from that frame. In and Out are each a stereo pair of bars with one readout per pair.

The EDIT drawer opens over the left panel, so the window and main layout don't change.
