# Fancy Limit: design note

Spec: `docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md` §5. Mockup: `reference/phase3/20__limit_mockup.png`, source `design/phase3-mockup-src/limit.html`. Design rules: `docs/DESIGN_SYSTEM.md`. **No code until this note is approved.** The points marked **Decision** are the ones that need your answer.

## Signal path

```
in ─► input gain (−Threshold) ─► oversample up (Off/2×/4×/8×) ─► limiter core ─► down ─► safety clip at Ceiling ─► dither ─► Output knob ─► out
                                                    ▲
                                 true-peak detector (4× interpolator)
Listen to removed: out = (delayed input × input gain) − limited signal, before dither
Loudness meter (BS.1770): taps the output after the safety clip, before dither and the Output knob
```

- Input gain is `−Threshold` dB, smoothed over 20 ms. At Threshold −7 dB the signal is pushed up 7 dB, then held at Ceiling.
- **Ceiling** is applied by scaling: the core limits to 1.0, with the signal divided by the ceiling's gain before it and multiplied by it after. Both gains are smoothed over 20 ms, so the core stays the one tested against a fixed 1.0.
- **Safety clip:** a final clamp at Ceiling, on sample values. That makes "sample peaks never exceed Ceiling" true by construction, even at +200 dB.
- **Output knob** (header, as on every plugin) comes after the clip. At its default of 0 dB the ceiling holds. A raised Output knob can push the output above Ceiling, so its tooltip says so.
- **Bypass:** the shared VST3 soft bypass, crossfading to the input delayed by the full latency.

## Shared code it reuses, and what changes there

### The limiter core: `fancy::dsp::TruePeakLimiter`

The spec says to reuse Fancy Comp's limiter rather than write a second one. Today that class is fixed:

- 2 ms window
- 50 ms one-pole release
- ceiling 1.0
- fully linked channels
- a moving-average (box) attack

It gets a `Settings` struct, set from the message thread through an atomic snapshot and read once per block:

| Setting | Comp and MultiComp (unchanged) | Fancy Limit |
|---|---|---|
| `maxLookaheadMs` (fixes the latency) | 2 | 5 |
| `lookaheadMs` (the attack window) | 2 | 0–5, the Lookahead knob |
| `releaseMs` | 50 | 1–1000, the Release knob |
| `autoRelease` | off | the Auto release checkbox |
| `link` | 1.0 | 0–1, the Link knob |
| `attackShape` | box | per Style, below |
| `truePeak` | on | the True peak checkbox |

- **Constant latency:** the audio delay line is always `maxLookahead` long. A shorter Lookahead feeds the gain computer through an extra delay of `maxLookahead − lookahead`, so the gain still lands exactly on the peak. Changing Lookahead never changes the reported latency, as the spec requires.
- **Bit-identity for Comp and MultiComp:** their default `Settings` reproduce today's arithmetic in the same order. Test: Comp's and MultiComp's limiter tests and the stored v1.2.0 state renders stay bit-identical, and a new test compares the old and new class sample by sample at 44.1, 48 and 96 kHz.
- **Link:** each channel's required gain is `g_c`, and the linked gain is `min(g_L, g_R)`. The gain used is `g_c + link × (min − g_c)`, interpolated in dB. At 100 % this equals today's linked path.
- **Lookahead 0 ms:** the window is one sample. That's an instant gain change, so it can distort; the knob's tooltip says so. The safety clip still holds the ceiling.

### Attack shape: "Hann-shaped"

The spec asks for a smooth, Hann-shaped gain envelope over the lookahead window. A true Hann-weighted average over 5 ms costs one multiply per window sample per output sample: 240 at 48 kHz, 960 at 4× oversampling. That doesn't fit the 2 % CPU target.

**Decision 1: A, approved by the user on 10 November 2026.**

- **A. Three cascaded moving averages, each a third of the window.** The result is a quadratic B-spline: a bell with smooth ends like a Hann window, though not identical (the largest difference is about 12 % of the peak, measured), at a fixed cost of three adds per sample, whatever the window length. It keeps the core's guarantee: the sliding minimum covers the whole window, so the gain is at its target when the peak arrives.
- **B. Exact Hann** with a recursive (sliding-DFT) form. That's O(1) too, but it drifts numerically and needs a periodic exact recompute. It's more code for an audibly identical result.

### Other shared code

- **Oversampling:** `juce::dsp::Oversampling` with the minimum-phase IIR or linear-phase FIR half-band stages, set up exactly as in `CompDsp` (`oversamplerFor`, integer latency). The filter phase is chosen in EDIT.
- **Header, meters and controls:**
  - `ShellEditor`, with short header labels as in the mockup
  - `ThresholdMeter` in Gate's style
  - `GainReductionMeter`, single bar
  - `OutMeter`
  - `ParamSegments`, `ParamDropdown`, `Knob::tightStyle`, `ParamCheckbox`, `EditLink` and `EditDrawer`

## New in `fancy-core`

These are added to `fancy-core` and the screenshot gallery rather than drawn inside Limit, because DS and Saturate will want them too (Q5's rule).

1. **`fancy::dsp::LoudnessMeter`:** ITU-R BS.1770-4 / EBU R128.
   - Measures:
     - K-weighting, two biquads with coefficients recomputed for any sample rate
     - momentary (400 ms) and short-term (3 s) loudness
     - integrated loudness, with the −70 LUFS absolute gate and the −10 LU relative gate
     - loudness range, per EBU Tech 3342
   - **All on the audio thread, in fixed storage** (as built): K-weighted squares summed into 100 ms blocks; momentary and short-term from a ring of the last 30 blocks; integrated and range from two histograms of 0.1 LU bins (-70 to +10 LUFS) that keep each bin's summed energy and count, so gating is exact apart from the one bin the relative gate falls in. It keeps measuring in an offline render with no editor open, and has no time limit. Results are atomics.
2. **`fancy::dsp::TruePeakMeter`:** the BS.1770-4 Annex 2 method, a 4× polyphase interpolator with 24 taps per phase (its own coefficients, not the standard's table). It's longer than the limiter's 16-tap one, because it's measuring, not steering. It skips the interpolation when the window's loudest sample can't beat the peak so far. It's used for the dBTP read-out and in the tests.
3. **Two small additions to existing meters:** `GainReductionMeter::setBarColour` (yellow by default; Limit's is red), and the gate-style `ThresholdMeter` skips its "close" read-out when no close level is set. Gate's screenshots are unchanged.
4. **`fancy::ui::LoudnessBar`:** a single bar on the meter gradient, with a red marker line at the Target and a readout. It's a sibling of `OutMeter`, with the same well and geometry.

## Parameters

Appended in this order (a new plugin, so `stateVersion` 1).

| ID | Name | Range | Default | Unit | Skew | Notes |
|---|---|---|---|---|---|---|
| `threshold` | Threshold | −30–0 | 0 | dB | linear | Input gain = −Threshold |
| `ceiling` | Ceiling | −12–0 | −1.0 | dB | linear | dBTP when True peak is on |
| `release` | Release | 1–1000 | 80 | ms | log | Scaled by Auto release |
| `lookahead` | Lookahead | 0–5 | 2 | ms | linear | No change to latency |
| `link` | Link | 0–100 | 100 | % | linear | |
| `style` | Style | Clean, Punchy, Dense | Clean | | | |
| `target` | Target | Streaming −14, Podcast −16, Broadcast −23, Custom | Streaming | | | Display only |
| `customTarget` | Custom target | −36 to −5 | −14 | LUFS | linear | Typed from "Custom…" |
| `truePeak` | True peak | off/on | on | | | |
| `autoRelease` | Auto release | off/on | on | | | |
| `listen` | Listen to removed | off/on | off | | | |
| `oversampling` | Oversampling | Off, 2×, 4×, 8× | 4× | | | Changes latency; latency note |
| `osPhase` | Oversampling filter | Minimum, Linear | Minimum | | | Changes latency |
| `dither` | Dither | Off, 16-bit, 16-bit shaped, 24-bit, 24-bit shaped | Off | | | |
| `ballistics` | Meter ballistics | EBU, RMS | EBU | | | Right-column LUFS bar only |
| `output` | Output | −24–+12 | 0 | dB | linear | Header knob |
| `bypass` | Bypass | off/on | off | | | |

- **Target is display-only.** It moves the red marker on the LUFS bar and colours the Short-term read-out. It never changes the gain. The spec doesn't ask for automatic loudness matching, and it would be a second gain stage a mastering user doesn't expect.
- **"Custom…"** in the Target dropdown opens the shared dialog with one field, accepting `-14` or `-14 LUFS`.
- **Loudness meter reset** is an EDIT button, and a click on the read-out panel. It's an action, not a parameter, so it isn't automatable and isn't saved.
- **Typed values:** every knob uses the shared `Knob` and `ThresholdMeter`, so all accept typed values.

## Styles

Styles only change the core's internal shapes. All four knobs keep working in every Style.

| Style | Attack shape | Release | Character |
|---|---|---|---|
| Clean | three cascaded averages over the full Lookahead | as set | smoothest; least distortion |
| Punchy | two cascaded averages over the last 60 % of Lookahead | as set | the gain falls later and faster, so a transient's front keeps more of its shape; the ceiling still holds |
| Dense | as Clean | × 0.5, both the fast and the slow paths | more consistent loudness, more density |

**Decision 2, approved by the user on 10 November 2026:** Punchy as above, where transients come through by starting the gain reduction later in the lookahead window, not by letting anything pass the ceiling. "Lets very short transients through more" can't mean above the ceiling, because the True peak test would fail. I recommend this reading.

## Auto release

Two release paths run in parallel:

- **fast:** the Release setting
- **slow:** 8 × Release, at most 2 s, after a 30 ms hold (a second sliding minimum), so the gain doesn't follow each cycle of a bass note

The weight between them follows how sustained the reduction is: a 300 ms average of the gain reduction compared with its 20 ms peak. Short peaks recover on the fast path; steady reduction, such as on a dense chorus or bass, moves to the slow path, which removes pumping and bass distortion (the 40 Hz test). With Auto release off, only the fast path runs.

## True peak

**The true-peak guard (added while testing, 10 November 2026).** The first CI runs showed the downsampling filters add up to 0.03 dB to a limited sine, and much more to noise near Nyquist, after the core has done its work. So a second instance of the shared limiter runs at the base rate after the filters: 1 ms of lookahead, the smooth shape, and a longer interpolator (32 taps, 8x; 16 taps let the dense test mix through by up to 0.2 dB), set through two new `Settings` fields (`interpolatorTaps`, `interpolatorPhases`; Comp's stay 16 and 4x). When oversampling is on, the core interpolates only the midpoint between its samples, since they're already close together and the guard measures the output exactly. Both interpolators read their window in one straight run and skip the work when the loudest sample in it can't reach the ceiling, which changes no output. Its latency is fixed and counted in the total, so Lookahead still doesn't change it. The core and the guard also check the sample their interpolator centres on (`alignedSamplePeak`), which Comp's v1.2.0 limiter leaves to its final clamp; Comp and MultiComp keep that behaviour, bit for bit.

The test's "full mix" is three tones plus noise rolled off above about 10 kHz, as a real mix's top end is. White noise flat to Nyquist at +30 dB isn't a realistic programme.

- **On:** the core's existing 4× interpolator (3 phases + the sample) estimates inter-sample peaks, with the input divided by the ceiling's gain, so the reconstructed peak stays at or below Ceiling. With oversampling on, the detector runs on the oversampled signal. That's denser than 4× of the base rate, so the estimate only gets better.
- **Off:** sample peaks only.
- **The guarantee:** the 16-tap interpolator isn't perfect, so the core aims 0.05 dB under Ceiling when True peak is on. The test measures with an independent 8× reference (a 64-tap-per-phase windowed sinc, in the test code only) and allows Ceiling + 0.1 dB, as the spec does.

## Dither

- **Algorithm:** TPDF, ±1 LSB at the chosen word length. The "shaped" variants add second-order error-feedback noise shaping, which pushes the noise above about 15 kHz.
- **Determinism:** the noise comes from a seeded generator, reset on `prepareToPlay` and on reset, so offline and real-time renders match (H.1).
- **Ceiling with dither:** dither comes after the safety clip, so with dither on, the output can sit up to 1 LSB above Ceiling. The ceiling tests run with Dither off. The drawer's tooltip says to keep Ceiling at −0.1 or lower when dithering.

## Latency

`(5 ms + 8 samples) × oversampling factor + the oversampler's latency`, rounded to whole samples at the base rate, and constant for any Lookahead.

- At 48 kHz and 4× minimum phase, that's about 248 samples (5.2 ms) plus the IIR stages' few samples.
- Changing Oversampling or its filter changes the latency. This uses Comp's existing scheme: the change is held until the value has been stable for 50 ms, then a fade, then the new latency is reported. The control gets the shared latency note.
- The dry path for bypass and Listen is delayed to match.

## Layout

From the mockup, at 100 %: **760 × 445** (53 px header + 392 px body), 465 with the title strip.

- **Header:** full style with short labels (`+`, `Prm`, `2/2`), and no read-out in the free area.
- **Top row:**
  - left: "Style" and the accent-filled `Clean | Punchy | Dense` segments
  - right: "Target" and its dropdown
- **Left column:**
  - Threshold over the input meter, Gate's style, with the scale at 0 to −30 in 6 dB steps (the parameter's range, which wins over the mockup's 0 to −48, as on Gate) and the value in red below it
  - next to it, the GR single bar and its read-out
- **Display card** (510 × 160):
  - history on the left: input in grey, output as a white line, reduction in red from the top, the ceiling dashed red
  - loudness panel on the right, in `#141414` and 150 px wide: Short-term, Integrated, True peak and Range
- **Limiter card:** Ceiling, Release, Lookahead and Link, all `Knob::tightStyle` in red.
- **Footer:** True peak, Auto release, Listen to removed, and ⚙ EDIT at the right.
- **EDIT drawer:** Oversampling, Oversampling filter, Dither, Meter ballistics, and a Reset loudness button.
- **Right column,** about 120 px (decision 3): Out with the shared `OutMeter` scale (Comp's `withScale`, 70 px), then the LUFS bar with no scale. The display card narrows by the 20 px the column gains.

**Decision 3, chosen by the user on 10 November 2026:** Out has a scale, as the spec says (option B). The LUFS bar has none, because its units differ.

## State format

The standard `FancyProcessor` XML. The loudness history isn't saved: integration restarts when a project loads, as on most loudness meters.

## Factory presets

Categories as in Q2.1:

| Category | Name | Settings |
|---|---|---|
| Mastering | Streaming master | Clean, Threshold −6, Ceiling −1.0, True peak, Auto release |
| Mastering | Loud and dense | Dense, Threshold −10, Ceiling −1.0 |
| Mastering | Broadcast safe | Clean, Threshold −3, Ceiling −2.0, Target Broadcast −23 |
| Mixing | Drum bus catch | Punchy, Threshold −4, Ceiling −0.3, Lookahead 1 ms |
| Mixing | Transparent peak guard | Clean, Threshold 0, Ceiling −0.3 |
| Utility | 16-bit bounce | Clean, Ceiling −0.3, Dither 16-bit shaped |

## CPU

**Target changed by the user on 10 November 2026: under 5 % at 48 kHz/128 with 4x and True peak on** (the spec's 2 % was set before anything was measured; measured 4.5 %, 9.6 x Comp, in run 42840531658). With True peak off, the core and the guard both skip their between-sample interpolation, and the guard keeps its fixed delay so the latency doesn't change; that path is measured separately. The rest of this section is the original estimate.

- **Target:** under 2 % at 48 kHz/128 with 4× and True peak, by the §0.4 ratio method against Fancy Comp's defaults. The core is already O(1) per sample apart from the 16-tap interpolator, which runs 3 phases × 2 channels at the oversampled rate.
- **If it's over target:** run the true-peak detector at the base rate on the upsampled signal's even phases.
- **Loudness meter:** a few multiply-adds per sample.

## How each issue in §5.4 is handled

| Issue | Handled by | Test (Catch2) |
|---|---|---|
| Inter-sample peaks | True peak on by default; core aims 0.05 dB under Ceiling; safety clip | 997 Hz and 11.025 kHz sines at fs/4 phase offsets, a square wave and a synthetic dense mix, all +30 dB over: an independent 8× reference meter never reads above Ceiling + 0.1 dB, at 44.1 and 48 kHz and each oversampling setting |
| Bass distortion and pumping | Auto release, Styles | 40 Hz sine with 10 dB of reduction, Auto release on: THD < 1 % |
| Ceiling exceeded at extreme drive | Shared core and safety clip | +200 dB over, every Style and Lookahead: no sample above Ceiling |
| Loudness targets need a reliable meter | `LoudnessMeter`, `TruePeakMeter` | EBU Tech 3341 cases 1–10 and 15–23, synthesised in the test from their published definitions (sines, levels, durations; no files downloaded), within ±0.1 LU and ±0.2 dB; Tech 3342 LRA cases 1–6 within ±1 LU |
| Lookahead moving the host's compensation | Constant latency | Reported latency identical for Lookahead 0, 2 and 5 ms, and an impulse comes out at the same sample in all three |

Also, from §7 of the spec and Phase 2.1:

- offline equals real time, Dither included
- variable block sizes from 1 to 8192, at 44.1 to 384 kHz
- the automation-without-change bit-identity test
- the S, H and W checks
- pluginval, Steinberg's validator, the host harness and FancyBench
- every pop-up listed in `getPopupOpeners()`
- the CI screenshot compared with the mockup and with Delay

## Registering a tenth plugin

- `limit` is added to `FANCY_ALL_PLUGINS`, the workflow's plugin choice, the screenshot tool, the host harness, and FancyBench's smoke list. `scripts\get-latest.ps1` copies every bundle in the artifact, so it needs no change.
- Bundle ID `com.fancyplugs.limit`, product name "Fancy Limit".
- Manual `docs/limit-manual.md`, REAPER checklist `docs/limit-checklist.md` and measurements `docs/measurements/limit.txt`.

## Phase 4 Wave 1: Gain Lock, Auto to Target, DC filter, release shape and peak statistics

Items from `docs/enhancements.md` (Wave 1 of the Phase 4 order). **Approved by the user on 21 November 2026 with every recommended option (R).** Every new parameter is appended, with a default that keeps saved projects bit-identical, checked against the v1.6.0 DSP in a legacy test as Comp's Wave 1 does. Items were checked in the code on 21 November 2026 before this note was written. Backlog items 1, 2, 4 and 5 of section 10.

### 1. Gain Lock

Lower the Threshold and Limit gets louder; with Gain Lock on, the output is turned down by the same amount, so you hear what the limiting does rather than "louder is better".

- **Decision 1: the compensation.**
  - **A (R).** Static: when Gain Lock is switched on, the current Threshold is remembered; the output then gets `Threshold − remembered` dB (always ≤ 0 when you push harder). Deterministic, and it's what Pro-L 2's Unity Gain does.
  - **B.** Measured: hold the output's short-term LUFS where it was. Closer to "same loudness", but it drifts with the music and renders differ.
- `gainLock` (off) and `gainLockRef` (the remembered Threshold, hidden from the UI, saved), a checkbox in the footer after Listen to removed.

### 2. Auto to Target

A button in the top row, left of the Target menu: measures the integrated loudness for 10 s of playback (a progress ring in the button), then moves Threshold by `target − measured`, once. An action, not a parameter: one undo step. With Gain Lock on, it's greyed out (the two contradict).

### 3. DC filter and release shape

- `dcFilter` (off): a 5 Hz first-order high-pass before the limiter, in the drawer.
- `releaseCurve` (Exponential, Linear), Comp's code and wording, in the drawer. Exponential is today's.

### 4. Peak statistics

The loudness panel's true-peak read-out gets a held maximum since the last reset, and a count of overs (samples above the ceiling before the final clamp, which should stay 0). A click on either resets both. No parameter; it resets when playback starts from stopped (as the LUFS integration does today).

### 5. Parameters

| ID | Name | Range | Default |
|---|---|---|---|
| `gainLock` | Gain lock | off/on | off |
| `gainLockRef` | Gain lock reference | −30 to 0 dB | 0 |
| `dcFilter` | DC filter | off/on | off |
| `releaseCurve` | Release curve | Exponential, Linear | Exponential |

### 6. Tests

1. Legacy: v1.6.0's Limit against the new one in every style and oversampling: identical.
2. Gain Lock: at Threshold −6 dB with the reference at 0, the output of a 1 kHz sine at −20 dBFS is −6 dB from Gain Lock off, within 0.01 dB.
3. Auto to Target: pink noise at −24 LUFS, target −14: after the action, integrated loudness −14 ± 0.5 LU.
4. DC filter: a 0.2 DC offset is gone (under −80 dBFS) after 1 s; 20 Hz loses under 0.1 dB.
5. Release curve: linear falls at a constant dB rate (Comp's test).
6. Peak statistics: the held maximum equals the largest true peak measured offline within 0.1 dB.

Mockup: `reference/phase4/31__limit_w1_mockup.png`: Gain lock in the footer, **Auto to target** in the top row left of the Target menu (not in the loudness panel, which has no room), the peak statistics beside True peak, and the drawer's two rows.

### 7. As built

One push (`3816ef6`), with every recommended option; `tests/LimitW1Tests.cpp`.

- **Gain lock:** `gainLock` and the hidden `gainLockRef`, a checkbox in the footer; switching it on stores the current Threshold, and the output then gets Threshold minus that.
- **Auto to target:** a button in the top row, left of the Target menu, reading "Listening..." with a progress ring for 10 s, then one move of Threshold; greyed while Gain lock is on. Clicking while it listens stops it.
- **DC filter** (`dcFilter`, "5 Hz high-pass") and **Release curve** (`releaseCurve`, Exponential or Linear) are in the drawer; the linear release is the shared limiter's, off by default.
- **Peak statistics:** the loudness panel shows the overs count (red when above 0) under the held true peak; a click resets both.

## Phase 4 Wave 2: clipper

**Approved by the user on 21 November 2026 with every recommended option (A).** Item 9 of the Wave 2 order (`docs/enhancements.md`; backlog item 3 of section 10). A soft clipper before the limiter, the common clip-then-limit mastering chain: it shaves the fastest peaks (kick and snare clicks, a few ms long), so the limiter, which can only turn the whole signal down, has less to do. Modelled first: `docs/research/limit-clip-model.py`.

### 1. What it does

- `clip` (off) and `clipAmount` (0 to 12 dB, default 3): after Threshold's push and before the limiter, a soft clipper whose ceiling sits 12 − Clip amount dB above Ceiling. At 12 it clips at Ceiling and the limiter only catches what's left; at 0 it's 12 dB above and almost never touches anything.
- The clip curve is linear up to 6 dB under its ceiling, then a cubic knee that meets the ceiling flat (no corner, so fewer high harmonics than a hard clip).
- It runs inside Limit's oversampling, at the same rate as the limiter, so at 2× and up its harmonics fold back far less. True peak and the final safety clip are unchanged.
- Off, it isn't run: a saved project renders as before to the bit.

### 2. What the model found

A drum-like test signal (kicks with a 3 ms click, noise snares, a quiet pad) pushed 8 dB into a 0 dBFS ceiling:

| Clip shaves | Loudness | Limiter GR, mean | GR, deepest | GR on the pad between hits | Clipper's added signal |
|---|---|---|---|---|---|
| nothing (limiter alone) | −15.3 dB | 4.3 dB | 17.3 dB | 1.7 dB | none |
| 3 dB of the peaks | −14.9 dB | 4.1 dB | 14.3 dB | 1.6 dB | −33 dB under the signal |
| 6 dB of the peaks | −14.1 dB | 3.7 dB | 11.3 dB | 1.5 dB | −25 dB |

So clipping trades a little distortion, all inside the transients, for up to about 1 dB more loudness and gentler limiting at the same push. The manual will say it's for dense, drum-heavy material and that 2 to 4 dB is the usual range.

### 3. Decisions

- **Decision 1: the control.**
  - **A (R).** One **Clip** amount knob (0 to 12 dB, as above) with an on switch: one number for how hard the clipper works.
  - **B.** Ozone's pair, **Clip drive** and **Clip ceiling**: more control, but two coupled knobs for one job.
- **Decision 2: the curve.**
  - **A (R).** Soft only, the cubic knee: the gentler sound, and the one the model measured.
  - **B.** A Soft / Hard choice: hard clipping is louder still, with harsher edges.
- **Decision 3: where it goes.**
  - **A (R).** In the drawer, with the DC filter and Release curve: a **Clip before limiting** checkbox and the Clip knob. The loudness panel's GR history shows clipped peaks in a second colour, so you can see what the clipper takes. A mockup follows approval.
  - **B.** On the main window beside Threshold: quicker, but it changes the main layout.

### 4. Tests

1. Legacy: Clip off renders as v1.6.0 (and as Wave 1) at every oversampling and True peak setting.
2. Peaks: with Clip 12, nothing over Ceiling reaches the limiter; with Clip 6, a test tone 10 dB over Ceiling reaches it at most 6 dB over (soft knee included).
3. Loudness: the model's signal, Clip shaving 3 dB, comes out louder than the limiter alone by 0.2 to 0.8 dB at the same output ceiling.
4. Aliasing: a 7 kHz sine clipped by 6 dB at 4× oversampling: no alias above −90 dBFS under 20 kHz.
5. Output: never over Ceiling (dBTP with True peak), as now.

### 5. Build order

Approval, then the drawer mockup (`reference/phase4/49__limit_w2_mockup.png`), then one push: the clipper in the limiter path, parameters, drawer, the GR history's second colour, tests, manual and checklist.

### 6. As built

Mockup 49 approved by the user on 22 November 2026; `tests/LimitW2Tests.cpp`.

- `clip` and `clipAmount` appended. `LimitDsp::softClip` is the model's curve; it runs inside the limiter lambda at the oversampled rate, on the signal in the limiter's scale, with the clipper's ceiling (12 - Clip dB) smoothed at the base rate. Off, nothing of it runs.
- `HistoryColumn` gains `clipDb` (the most the clipper took in a column); the display fills it in orange over the red reduction and adds "clipped (orange)" to the legend while the clipper is on or has clipped.
- The drawer's Clip before limiting checkbox and Clip knob sit beside the DC filter and Release curve rows.
- Tests 3 and 4 are looser than section 4: the loudness gain is checked between 0.1 and 1.5 dB (the model's 0.2 to 0.8 came from a simplified limiter, not Limit's), and aliasing as 4x at least 20 dB cleaner than Off and under -60 dBFS, because the true-peak guard and final clip run at the base rate after the clipper.

## Phase 4 Wave 2: Mid/Side link and an external sidechain

**Approved by the user on 23 November 2026: decisions 1 and 2 both A, both items dropped.** Items 30 and 31 of the Wave 2 order (`docs/enhancements.md`; backlog items 7 and 8 of section 10), for v1.7.1. Modelled first: `docs/research/limit-w2-model.py`. The model argues against building either; the decisions below let you choose.

### 1. Mid/Side and per-channel link (item 30)

The backlog asked for M/S and per-channel link modes beside Link %. **Per-channel already exists:** Link at 0 % limits each channel on its own, and anything between blends the two.

**Mid/Side** would limit mid and side separately, so a peak at the sides (a hard-panned tom fill, a crash) turns down only the side and not the centre's vocal. But Limit's ceiling is in left and right, and `L = M + S` can peak higher than either. The model, a centre vocal with a hard-left tom fill pushed 3 dB into a −1 dBFS ceiling, shows:

| | Vocal dips on each fill | Left/right peak |
|---|---|---|
| L/R linked (today) | 1.6 dB | at the ceiling |
| Mid/Side limited | — | +3.7 dB over the ceiling |
| Mid/Side, then left/right held at the ceiling | 1.6 dB | at the ceiling |

To keep its promise, the ceiling, Limit has to finish in left and right, and that final stage takes back exactly what Mid/Side saved. A mid/side limiter only helps where the ceiling can be broken, which a brickwall limiter mustn't allow.

### 2. External sidechain (item 31)

A limiter keeps its own output under the ceiling, so it must hear itself. Keyed by another signal alone, Limit let its own peaks through at **+3.7 dB over the ceiling** in the model. Taking the deeper of its own gain and the key's holds the ceiling, but then the key only adds ducking, which is Fancy Comp's job (its sidechain, with Range and Host sync).

### 3. Decisions

- **Decision 1: item 30.**
  - **A (R).** Drop it. Per-channel is Link 0 %, which the manual will spell out, and Mid/Side can't help under a true ceiling.
  - **B.** Build Mid/Side with a final left/right ceiling stage, for the rare case where the sides peak much more than the centre and you accept the same final reduction.
- **Decision 2: item 31.**
  - **A (R).** Drop it. Keyed limiting either breaks the ceiling or becomes ducking, which Comp does.
  - **B.** Build it as "the deeper of own and key", with an optional sidechain bus and the host checks run again.

### 4. Both dropped

`docs/enhancements.md` marks items 30 and 31 dropped with the reason, and Limit's manual says that Link 0 % is per-channel limiting. No code changes.

## Phase 4 Wave 3: multiband limiting

**Layout A of mockup 74 approved by the user on 26 November 2026; the note approved the same day with every recommended option (A).** Item 1 of Wave 3 (`docs/enhancements.md`; backlog item 6 of section 10, ▲), for v1.7.2. Modelled first: `docs/research/limit-multiband-model.py`.

### 1. What it adds

**Bands** (1 to 4, default 1): the signal is split by a crossover, each band is limited on its own with its own **Release**, the bands are summed, and today's limiter (the final stage: Ceiling, Release, Lookahead, Link, True peak, the clipper, oversampling and the guard) then holds the ceiling across the whole signal. One band skips the split and the band limiters, so a saved project renders as v1.7.1, to the bit, with the same latency.

What it's for: a wideband limiter turns everything down when the kick hits, so the vocal and the hats pump with the kick. With the bass limited in its own band, the bands above keep their level.

### 2. What the model found

A mix of 60 Hz kicks every 0.5 s, a steady 1 kHz tone and hats above 6 kHz, pushed 8 dB into a 0 dBFS ceiling. Crossovers 150 Hz (2 bands); 150 Hz and 2.5 kHz (3); 150 Hz, 1 kHz and 6 kHz (4). Band releases low to high 120, 80, 50 (and 40) ms; the final stage 2 ms lookahead and 80 ms release, as Limit's defaults. "Dip" is how far the 1 kHz tone (and the hats) fall in the 150 ms after each kick against just before it: the deepest point and the mean.

| Kick | Bands | Band ceiling | 1 kHz dip, deepest / mean | Hats, deepest / mean | Final stage GR, mean / max | Loudness |
|---|---|---|---|---|---|---|
| body only | 1 (today) | | −8.7 / −5.1 dB | −8.7 / −5.1 dB | (it is the limiter) | −9.9 dB |
| body only | 3 | Ceiling | −4.2 / −2.4 | −4.2 / −2.4 | 0.9 / 5.0 dB | −10.0 |
| body only | 3 | Ceiling − 3 dB | **−2.6 / −1.4** | **−2.6 / −1.4** | 0.5 / 3.5 | **−10.3** |
| body only | 3 | Ceiling − 6 dB | −1.1 / −0.6 | −1.6 / −0.6 | 0.2 / 2.1 | −10.6 |
| with a 3 ms click | 1 (today) | | −9.3 / −5.3 | −9.3 / −5.3 | | −10.1 |
| with a 3 ms click | 3 | Ceiling − 3 dB | −2.3 / −1.1 | −11.6 / −3.0 | 0.3 / 3.0 | −10.0 |

- **It works for what it's for:** the kick's body no longer pumps the mids and highs. With the bands limited at Ceiling, the mean dip halves (5.1 to 2.4 dB) at the same loudness; the rest comes from the final stage, because the limited bands add up past the ceiling. Limiting the bands 3 dB under Ceiling leaves the final stage less to do and halves the dip again, for 0.4 dB of loudness. 6 dB under gives up 0.7 dB.
- **2, 3 and 4 bands do the same for a kick body**, which is all under 150 Hz. More bands help when the material has peaks in the middle too (with the click, the 1 kHz tone's mean dip is −2.1 dB at 3 bands against −3.7 at 2, where it shares a band with the click).
- **A click still ducks the top band:** it's broadband, so it reaches every band, and the top band, limited alone, dips deeper at its lowest point than today's limiter (−11.6 against −9.3 dB), though for less time (mean −3.0 against −5.3). That's the clipper's job (Wave 2): the manual says to use Clip for clicky drums with Bands.
- **The crossover sums flat:** MultiComp's minimum-phase Linkwitz-Riley cascade, each lower band through the all-passes of the crossovers above it, sums to an all-pass (within 0.0001 dB at 2, 3 and 4 bands), so with nothing limited the output's magnitude is the input's.

### 3. How it's built

- **Where:** after the DC filter and Threshold's push, at the base rate, before the clipper and the oversampler. The band limiters measure sample peaks; inter-sample peaks stay the final stage's (True peak), which still sees everything. Running the bands inside the oversampling would cost the band count times the factor more, for peaks the final stage already handles.
- **The crossover:** MultiComp's `MinPhaseCrossover` moves into `fancy-core` as `fancy::dsp::MinPhaseCrossover` (Gate's multiband, the next item, uses it too), with MultiComp's legacy tests holding it to the bit. LR 24 and LR 48 only (decision 3). Crossover frequencies are kept in order and at least 1/3 octave apart by MultiComp's `sanitiseCrossovers`, smoothed in log frequency over 20 ms.
- **Each band's limiter** is a `fancy::dsp::TruePeakLimiter` (True peak off), as the final stage's: the same Style, Link, Lookahead window and Release curve; its own Release from the band's knob. Its ceiling is the Ceiling minus the band headroom (decision 1). Its lookahead delay is a fixed 5 ms, so Lookahead still doesn't change latency.
- **Latency:** with Bands 2 to 4 the band stage adds 5 ms (240 samples at 48 kHz), reported to the host; Bands 1 adds nothing (decision 2). The dry paths (Listen to removed, bypass) are delayed to match.
- **Displays:** the BANDS card's strip shows each band's gain reduction, falling from the top in its colour, and each band's GR read-out under it (atomics, read by the editor's timer). The GR meter and the history's red show the final stage, as now, so they read what they read today.
- **Unchanged:** Listen to removed (input minus output, so it includes the bands), Gain lock, Auto to target, dither, the meters and the loudness panel.
- **CPU:** the crossover (two or four biquads per crossover per channel, plus the all-passes) and one base-rate limiter per band. Target: four bands add under 2.5 times Fancy Comp at its defaults (§0.4, the ratio printed in the same test).

### 4. Decisions

- **Decision 1: the bands' ceiling.**
  - **A (R).** Each band limits 3 dB under Ceiling, fixed: the model's balance, half the pumping of bands at Ceiling for 0.4 dB of loudness, and no new control.
  - **B.** A **Band headroom** knob in the EDIT drawer, 0 to 6 dB, 3 by default, for the trade between loudness and pumping (a drawer mockup follows).
  - **C.** Each band at Ceiling: the loudest, but twice the pumping of A.
- **Decision 2: latency.**
  - **A (R).** The band stage's 5 ms only while Bands is 2 to 4: the default keeps v1.7.1's latency, and the host is told when the count changes, as EQ's spectral bands do.
  - **B.** Always report it, so changing Bands never changes latency; every saved project's latency grows by 5 ms.
- **Decision 3: crossover types.**
  - **A (R).** LR 24 and LR 48, minimum phase, LR 24 by default: no added latency, and they sum flat.
  - **B.** Also MultiComp's Linear phase: no phase shift at the crossovers, but its FIR adds about 21 ms of latency and the most CPU.
- **Decision 4: Auto release in the bands.**
  - **A (R).** The footer's Auto release applies to every band, each from its own Release (the fast path, with the slow path 8 times it), as the LIMITER card's Release works now; the band knobs keep showing their values. Mockup 74's caption had them reading "Auto"; they show their value instead, since it still sets the fast path.
  - **B.** Bands always use their plain Release, and Auto release is the final stage's only.

### 5. Parameters

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `bands` | Bands | 1, 2, 3, 4 | 1 | | |
| `crossoverType` | Crossover | LR 24 dB · Min phase, LR 48 dB · Min phase | LR 24 | | |
| `crossover1` | Crossover 1 | 20 to 20000 | 150 | Hz | log |
| `crossover2` | Crossover 2 | 20 to 20000 | 2500 | Hz | log |
| `crossover3` | Crossover 3 | 20 to 20000 | 8000 | Hz | log |
| `bandRelease1` to `bandRelease4` | Band 1 to 4 release | 1 to 1000 | 120, 80, 50, 40 | ms | log, as Release |

Appended after `clipAmount`. No state migration: a state without them loads Bands 1. Two bands use Crossover 1, three use 1 and 2, four use all three. The crossover handles are dragged on the strip or typed (`2500`, `2.5k`).

### 6. Tests

1. Legacy: Bands 1 renders exactly as v1.7.1 at every Style, oversampling and filter, with True peak, Auto release, the clipper, DC filter and Gain lock on and off; latency unchanged.
2. Crossover: Bands 2, 3 and 4 at LR 24 and LR 48 on noise 30 dB under the ceiling (nothing limits): the output's magnitude against the input's within 0.05 dB from 20 Hz to 20 kHz.
3. Pumping: the model's body-only mix at 3 bands: the 1 kHz tone's mean dip at most half of Bands 1's (model: 1.4 against 5.1 dB).
4. Band independence: a 60 Hz tone pushed 9 dB over Ceiling with a 5 kHz tone 20 dB under it, 3 bands: the 60 Hz band shows at least 6 dB of reduction, and the 5 kHz tone's level moves under 0.5 dB.
5. Per-band release: a 60 Hz burst at 3 bands, Auto release off: band 1 takes 2 to 6 times longer to recover 6 dB at a 500 ms release than at 120 ms.
6. Ceiling: never over Ceiling (dBTP with True peak) at 2, 3 and 4 bands, on the model's mixes and the existing dense test signal, at every oversampling.
7. Latency: Bands 2 to 4 report 5 ms more (240 samples at 48 kHz); the bypass and Listen to removed paths stay aligned (the existing bypass null test at 3 bands).
8. Sweeps: Bands, the crossovers and the band releases automated through their ranges: no NaN or inf, and no step over 0.1 between samples on a 100 Hz sine (§4 rule 2).
9. Offline renders are bit-identical to real time and to random block sizes at 3 bands; enabling automation without changing a value leaves the output bit-identical (§4 rule 8).
10. MultiComp renders exactly as before the crossover moved (its legacy tests).
11. CPU (§0.4): 2, 3 and 4 bands at the defaults, with the ratio to Fancy Comp, in `docs/measurements/limit-w3.txt`.

### 7. Build order

Approval, then one push: the crossover into `fancy-core` (MultiComp's tests unchanged), the band stage in `LimitDsp`, the parameters, the BANDS card (mockup 74 A, window 760 x 637, its minimum too), tests from a pre-push port of the model, manual and checklist. Then Gate's multiband (▲) needs its layout approved.

### 8. As built

- **Band limiters:** `LimitDsp::bandSettings` is `limiterSettings` at the base rate with the band's Release and True peak off, and a window of at most 5 ms less the core's 8-sample interpolator, so each band delays by exactly 5 ms (240 samples at 48 kHz). The bands run in the limiter's scale (Ceiling = 1), each pushed up by 3 dB into its limiter and brought back down.
- **Bands and Crossover type aren't automatable,** as Oversampling: one changes the latency and the other the phase in a step. The note's test 8 sweeps them anyway by setting the targets directly (the count steps 4, 3, 2, 1 while the crossovers and band releases sweep). A change of count fades out and back in, as a change of Oversampling does; at 2 to 4 the latency doesn't change, but the bands start from rest.
- **Listen to removed with bands:** the final stage's removed signal, as before, plus what the bands took away (each band's delayed input less its output), delayed to meet it. The bands add up to an all-pass of the input, so what's kept and what's removed add up to that all-pass, not to the input sample for sample (test 7 compares them with the chain's output at -60 dB, where nothing limits). With oversampling on, the bands' part skips the oversampling filters, so the two parts line up to the filters' group delay, not to the sample; test 7 holds the sum of the kept and removed signals to the input within 1e-3 with oversampling off.
- **Crossovers on the strip** are dragged between their neighbours, a third of an octave from each, or clicked to type a value (a dialog, as Custom target's, taking `2500` or `2.5k`). The strip shows the crossovers as the stage uses them (`LimitDsp::sanitisedCrossovers`). With one band it shows the final stage's reduction in band 1's colour, and the band row is empty.
- **Layout:** `bandsGrow` (172) in `LimitEditor.cpp` moves the LIMITER card and the footer down and lengthens the Threshold, GR, Out and LUFS meters; the window and its minimum are 760 x 637. The band knobs are the shared `Knob` (27 px face, as Room's and Verb's drawer knobs), with typed values.
- **Pumping test (3)** runs as the model does, sample peaks at the base rate (True peak and oversampling off, Auto release off): with True peak and 4x on, the hats' inter-sample peaks kept the final stage busier, and run 45891942396 measured -4.63 dB at one band against -2.35 at three, a ratio of 0.507. It measures the 1 kHz tone through a band-pass (Q 4) in 5 ms RMS windows; the model, measured the same way, gives a mean dip of -5.3 dB at one band and -1.4 dB at three (ratio 0.27, the test's limit 0.5).
- **Screenshot:** `limit-bands` (mockup 74's panel 1, three bands limiting); `limit` shows the default, one band.
