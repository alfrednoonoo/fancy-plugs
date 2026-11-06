# Fancy Comp: design note

## DSP

An original feed-forward compressor with optional feedback detection:

```
main ──┬─ delay (matches all wet latency) ───────────────────────────── × Dry ─┐
       │                                                                       + ─ × Output ─ bypass ─ limiter ─ out
       └─ [ up ─ pre-comp delay ─ × gain ─ × make-up ─ down ] ───────── × Wet ─┘
                                   ▲
main or sidechain ─ [ up ─ detector select ─ HP ─ LP ─ RMS ─ link ─ gain computer ─ ballistics ]
```

Everything inside the brackets runs at the oversampled rate.

- **Detector input:** Main L+R, Main L, Main R, Sidechain L+R, Sidechain L, Sidechain R, Mid, Side. L+R modes detect each channel separately, and Link channels decides how they combine. The others feed both channels the same detector signal.
- **Detector filters:** 2nd-order state-variable high-pass and low-pass. The filters switch out completely at their extremes: Highpass at 20 Hz, Lowpass at 24 kHz.
- **RMS size:** a one-pole average of the squared detector signal. 0 ms is peak detection. A one-pole average is used in place of a sliding window, so the size can move smoothly without recomputing a window on the audio thread. With oversampling on, the average is never shorter than 20 µs. Without that minimum, the log of an instantaneous square has corners at each signal peak, and their harmonics fold into the gain.
- **Link:** when Link channels is on, each channel's level moves towards the louder channel by Link amount, default 100 %.
- **Gain computer:** log domain, with a quadratic soft knee centred on Threshold over Knee dB. **Weird** moves the knee so it starts at Threshold and ends Knee dB above it, so compression arrives later.
- **Ballistics:** see below.
- **Make-up:** Auto make-up adds half of the gain reduction that a 0 dBFS signal would get: `0.5 × (1 - 1/ratio) × -threshold`.
- **Preview filter:** replaces the wet signal with the filtered detector signal.
- **Feedback detection:** the detector reads the previous output sample of the compressor in place of the main input. Sidechain inputs are unaffected.

## Ballistics (§5.2)

| Setting | Behaviour |
|---|---|
| **Classic off** (default) | Branching peak detector with smoothed gain. The attack and release filter the gain reduction, so maximum gain reduction doesn't depend on attack |
| **Classic on** | Smoothed detector. The attack and release filter the level before the gain computer, so a short peak with a long attack gets less gain reduction. This is by design |
| **Auto** | Program-dependent release in two stages. Release runs at a tenth of the set time down to a slow envelope, which follows the gain reduction at the set time. Transients recover fast, and sustained compression recovers slowly |
| **Release curve** | Exponential (default), or Linear, which falls at a constant rate in dB with the same initial slope |
| **Attack convention** | Attack and release are one time constant, the time to reach about 63 % of the target. The "Attack = 90 % rise" option makes the set time the 90 % point instead, by dividing the time constant by 2.303 |

## Parameters

| ID | Name | Range | Default | Unit | Skew | Source |
|---|---|---|---|---|---|---|
| `threshold` | Threshold | -60–0 | 0.0 | dB | linear | §3.3 |
| `ratio` | Ratio | 1:1–∞:1 | 4:1 | | log, top of range is ∞ | §3.3 |
| `knee` | Knee | 0–24 | 0.5 | dB | skewed | §3.3 |
| `weird` | Weird | off/on | off | | | §3.3 |
| `attack` | Attack | 0–500 | 3.0 | ms | skewed | §3.3 |
| `classic` | Classic | off/on | off | | | §3.3, §5.2 |
| `release` | Release | 0–5000 | 200 | ms | skewed | §3.3 |
| `auto` | Auto | off/on | off | | | §3.3 |
| `precomp` | Pre-comp | 0–250 | 0.0 | ms | skewed | §3.3 |
| `rms` | RMS size | 0–250 | 5.0 | ms | skewed | §3.3 |
| `lowpass` | Lowpass | 20–24000 | 24 kHz (off) | Hz | log | §3.3 |
| `highpass` | Highpass | 20–24000 | 20 Hz (off) | Hz | log | §3.3 |
| `wet` | Wet | -60–+12, bottom = -inf | 0.0 | dB | linear | range chosen here |
| `dry` | Dry | -60–+12, bottom = -inf | -inf | dB | linear | range chosen here |
| `detector` | Detector input | 8 choices | Main input L+R | | | §3.3 |
| `link` | Link channels | off/on | on | | | §3.3 |
| `makeup` | Auto make-up | off/on | off | | | §3.3 |
| `preview` | Preview filter | off/on | off | | | §3.3 |
| `limit` | Limit output | off/on | off | | | §3.3 |
| `oversampling` | Oversampling | Off, 2x, 4x | Off | | | §3.3 |
| `osPhase` | Oversampling filter | Linear, Minimum | Linear | | | §5.2 |
| `feedback` | Feedback detection | off/on | off | | | §3.3 |
| `linkAmount` | Link amount | 0–100 | 100 | % | linear | §3.3 "Mix linking" |
| `releaseCurve` | Release curve | Exponential, Linear | Exponential | | | §5.2 |
| `attack90` | Attack = 90 % rise | off/on | off | | | §5.2 |
| `output` | Output | -24–+12 | 0.0 | dB | linear | range chosen here |
| `bypass` | Bypass | off/on | off | | | §4.6 |

The mockups show several checkboxes ticked. Those are demonstration states, so the defaults above are the neutral settings. Link channels defaults to on, because unlinked channels make a stereo image wander.

**Classic** is the §5.2 detector mode switch, so that switch needs no separate control.

## Smoothing

One-pole smoothers at the oversampled rate for Threshold, Ratio, Knee, make-up, RMS size and preview, over 20 ms. Filter coefficients are recomputed every sample while the frequencies move. Wet, Dry, Output and bypass are smoothed at the base rate.

## Latency

Total latency is the sum of oversampling latency, Pre-comp, and limiter lookahead when Limit output is on. Dry is delayed by all of it except the limiter, which is in series with both.

Pre-comp, oversampling and Limit output all change the latency, so a change is held until the value has been stable for 50 ms. The output then fades out, the delays are reconfigured, the new latency is reported, and the output stays silent until the delays have refilled. Dragging Pre-comp therefore causes one dropout at the end of the drag, not many.

## Output limiter (§5.2)

A separate stage after the make-up gain and the wet/dry sum, with a fixed 0 dBFS ceiling and about 2 ms of lookahead, so the ceiling doesn't depend on oversampling or input level.

- **True-peak detection:** a 4× windowed-sinc interpolator estimates inter-sample peaks.
- **Gain:** the required gain goes through a sliding minimum over the lookahead window, then a one-pole release, then a moving average over the same window. The gain therefore reaches its target before the peak leaves the delay.
- **Linked:** both channels share one gain, so limiting doesn't shift the image.
- **Guarantee:** a final clamp at ±1.0 catches anything left, so sample peaks can't exceed the ceiling even at extreme levels.

## State

The same format as the other plugins: XML with `stateVersion` 1, `uiScale` and `presetName`, with the same validation.

## §5.2 fixes

| Requirement | How it is met | Test |
|---|---|---|
| Proper oversampling | Polyphase 2×/4× around the gain computer and gain, linear- or minimum-phase | `aliasing is below -90 dBFS at 4x` |
| Limit output at extreme levels | Separate stage, gain in double precision, final clamp | `limiter holds the ceiling at extreme levels` |
| Ceiling independent of oversampling | Limiter runs at the base rate after everything else | same test, across oversampling modes |
| Attack time convention | One time constant, optional 90 % rise, shown in the tooltip and manual | `attack time follows the convention` |
| Maximum GR versus attack | Branching default, Classic switch | `maximum reduction ignores attack unless Classic is on` |
| Release curve | Exponential default, Linear option, Auto is dual time constant | `release curves` |
| Automation enabling changes audio | Regression test | `comp automation with an unchanged value is bit-identical` |
| Pre-comp latency | Reported, dry delayed | `pre-comp latency is reported and wet and dry stay aligned` |

## Layout

The Figma Reacomp frame is 834 × 527 without its host title bar, but it has no GR meter or Link channels checkbox. §3.3, §2.5 and `reference/08__ReaComp.png` all require both, and §2.0b says the control map applies even when Figma differs. So a 100 px GR panel sits between Threshold and the main grid, as in the reference, and everything to its right moves over by 100. The window is 934 × 527.

Figma's four footer checkboxes become Link channels, Auto make-up, Preview filter and Limit output.

The settings that aren't in the mockups sit in an EDIT drawer, which opens over the main grid. Its link is at the right of the Detector input row, so the main layout doesn't change.
