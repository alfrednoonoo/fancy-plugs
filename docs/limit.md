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

**Draft, for approval.** Item 9 of the Wave 2 order (`docs/enhancements.md`; backlog item 3 of section 10). A soft clipper before the limiter, the common clip-then-limit mastering chain: it shaves the fastest peaks (kick and snare clicks, a few ms long), so the limiter, which can only turn the whole signal down, has less to do. Modelled first: `docs/research/limit-clip-model.py`.

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
