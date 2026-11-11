# Fancy Sature: design note

Spec: `docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md` §4 (it calls the plugin Fancy Saturate; the user renamed it **Fancy Sature** on 10 November 2026), and §0.3 for Listen. Mockup: `reference/phase3/19__saturate_mockup.png`, source `design/phase3-mockup-src/sat.html`. Design rules: `docs/DESIGN_SYSTEM.md`. **Approved by the user on 11 November 2026: A for all seven decisions.** For Decision 7 the user weighed B (an optional 16×) and chose A: 4× stays the default, with no 16× setting.

Names: product **Fancy Sature**, title strip "FANCY SATURE", bundle ID `com.fancyplugs.sature`, folder `plugins/sature/`, docs `docs/sature*.md`. Accent orange `#E77E23`, not following a selection. Phase 2 size class: 760 × 445 (window 760 × 465 with the title strip), body 392 px as in the mockup.

## Signal path

```
x ─► input trim ─┬─► matched path (up ─► ½-sample average ─► down) ─────────────────────────────► m
                 └─► mid = band filter (Low/High) ─┬─► up ─► ×Drive ─► shaper (ADAA) ─► Tape HF loss ─► Tone ─► ×auto gain ─► down ─► s
                                                   └─► matched path ─────────────────────────────────────────────────────────────► m_mid
added = DC filter (s − m_mid)
out   = ((m + added) × Wet + m × Dry) × Output          Listen: out = added × Wet × Output
```

The whole design rests on one idea: **the plugin adds `added = saturated(mid) − mid` to the signal**, rather than replacing the signal with a saturated copy. Everything that isn't saturated (the dry signal, the bands outside Low and High, the clean part of the mid band) goes through a **matched path**: the same up- and down-sampling filters as the saturated band, with nothing between them but the ADAA's half-sample average. The matched path is linear, so the input and the mid band can share one (two oversamplers per channel in all). Concretely:

- **Wet and Dry sum flat.** Dry and the wet signal's clean part go through identical filters, so at Drive 0 there is nothing to comb against, with minimum-phase oversampling too. Comp delays its dry path by the rounded latency instead, which is fine for linear phase but leaves a small comb near the top for minimum phase. The brief's flatness test (Wet 0 dB, Dry 0 dB, Drive 0: flat within 0.1 dB, 20 Hz–20 kHz) needs the matched path.
- **The band split is complementary by construction.** The output is the whole input plus what saturating the mid band changes, so the bands outside it come out exactly as they went in, whatever the band filter's slope or phase. There is no crossover allpass and no time-alignment problem: the bands outside Low and High pass clean.
- **Listen to added harmonics is `added` itself**, already aligned and already level-matched by auto gain (below).
- **Oversampling Off:** the matched path is just the ADAA's half-sample average (below), so nothing else changes the dry signal.

### The band filter

`mid` is the input through a high-pass at Low and a low-pass at High, each skipped when its knob is at Off. Off to on and back crossfades over 20 ms, so the knobs never click at their ends.

**Decision 1: the band edges' slope (I recommend B).** The brief's test: with Low at 200 Hz, a 60 Hz sine at −6 dBFS gains no harmonics above −90 dBFS. 60 Hz is 1.7 octaves below 200 Hz, so how much of it leaks into the saturator depends on the slope, and how much distortion that leak makes depends on Drive.

- **A. 24 dB/oct** (4th-order Butterworth). 60 Hz leaks in at −42 dB. The test passes up to about Drive 18 dB, and fails at 24 and 36 dB: "below Low the signal passes clean" stops being true at high Drive.
- **B. 48 dB/oct** (8th order). 60 Hz leaks in at −84 dB, and the test passes at every Drive up to 36 dB. It costs 8 biquads per channel at the base rate, which is small. The steeper edge rings a little more, but only in the saturated band, and the subtraction above keeps the sum exact.

**Decision 2: High below Low (I recommend A).** Low runs to 2 kHz and High down to 1 kHz, so they can cross.

- **A.** The DSP holds High at least one octave above Low. Nothing on screen changes; the High knob's tooltip says "kept at least an octave above Low".
- **B.** Let them cross: the band is empty and nothing is saturated. Simple, but it looks broken.

### Oversampling

Off, 2×, 4× (default) or 8×, with a Minimum phase (default) or Linear phase filter, in the EDIT drawer with the latency beside it (`(+N samples latency)`). JUCE's polyphase IIR half-band for minimum phase and its FIR equiripple half-band for linear phase, as in Comp and Limit, with Exciter's stage settings. Changing either one is Exciter's switch: a 2 ms fade out, reconfigure, report the new latency (H.6), stay silent while the delays refill, fade in.

**Decision 3: where the oversampler lives (I recommend A).** The brief says Sature reuses Exciter's oversampling.

- **A.** Move Exciter's oversampler and its switch logic into `fancy-core` as `fancy::dsp::SwitchedOversampler`, with the minimum-phase option added. Exciter uses it with linear phase and stays bit-identical: its tests, screenshots and stored renders must not change, as with every earlier move.
- **B.** Copy it into Sature. Less risk to Exciter, but a second copy to maintain.

### The shaper

Every Type uses first-order ADAA (anti-derivative anti-aliasing) as Exciter does, inside the oversampler. ADAA needs each curve's antiderivative, every sample at 4× or 8×. `tanh`'s is `log(cosh(x))`, about four times the cost of the curve itself, so the curves are chosen to have cheap exact antiderivatives (a square root or a polynomial). Drive multiplies the input, and every curve has a slope of 1 at zero and saturates at ±1 (0 dBFS).

| Type | Curve `f(u)` | Antiderivative | Sound |
|---|---|---|---|
| **Tape** | `u / √(1 + u²)` | `√(1 + u²)` | soft, tanh-like, plus a one-pole low-pass that falls from off (Drive ≤ 12 dB) to 12 kHz (Drive 36 dB): the "gentle high-frequency loss at high drive" |
| **Tube** | positive half as Tape; negative half `u / √(1 + (u/a)²)` with `a = 1.4`, so it saturates later and softer | `a √(a² + u²)` on that half | asymmetric soft curve: even harmonics even at Character 0 |
| **Transistor** | linear to 0.5, quadratic knee to 1.5, then flat | piecewise polynomial | harder knee |
| **Clip** | linear to 0.9, quadratic knee to 1.1, then flat | piecewise polynomial | hard clip with a small rounded knee |

The ADAA averages each sample with the one before, so it's a half-sample delay and a gentle low-pass on the saturated band (at 4×, about −0.5 dB at 20 kHz). The matched path applies the same two-sample average, so at Drive 0 `added` is exactly zero and the Wet/Dry test holds.

### Character

**Decision 4: what the odd half of Character does (I recommend A).** The brief: bipolar, "even 100 % … odd 100 %, default even 30 %: asymmetry, more even or more odd harmonics". The even half is a bias: the input is shifted by up to 0.5 before the curve and `f(bias)` subtracted after, which makes the curve asymmetric and adds even harmonics. A symmetric curve already makes only odd harmonics, so the odd half needs a meaning.

- **A.** The odd half removes all asymmetry (no bias, and Tube's negative half moves to `a = 1`) and blends the curve towards a symmetric hard knee (Transistor's), so odd 100 % has the strongest odd series. Blending two curves keeps the antiderivative exact (it's the same blend of the two). Character then does something on every Type.
- **B.** The odd half only removes asymmetry. On Tape, Transistor and Clip, which are already symmetric, the whole odd half does nothing.

Read-out: "even 30 %", "neutral" at the centre, "odd 50 %". Stored as −100 (even 100 %) to +100 (odd 100 %), default −30, so the knob sits left of centre as the mockup draws it.

### Tone

A tilt on the saturated band, after the shaper, pivoting at 1 kHz: two first-order shelves.

**Decision 5: what ±6 dB means (I recommend A).**

- **A.** +6 dB lifts the top by 3 dB and lowers the bottom by 3 dB: the brief's range is the total tilt, 6 dB from end to end. Gentle, as "Tone" suggests.
- **B.** +6 dB lifts the top by 6 and lowers the bottom by 6, 12 dB from end to end.

### Auto gain

The brief: a fixed compensation per Type, Drive and Character, measured offline at build time on pink noise at −18 dBFS RMS, never a signal follower, so it can't pump. It scales the saturated band before `added` is formed.

**Decision 6: how the table is made (I recommend A).**

- **A. Generated during the build.** A small console tool (`tools/sature-gaintable`) is built first from the same shaper header, and CMake runs it to write `SatureGainTable.h` into the build folder before the plugin compiles. It renders 3 s of fixed-seed pink noise at 48 kHz through the shaper for every Type, Drive 0–36 dB in 1 dB steps and Character −100 to +100 in steps of 10 (3,108 points, a few seconds in CI), and measures loudness with `fancy-core`'s BS.1770 `LoudnessMeter`. The plugin interpolates bilinearly and smooths the result over 20 ms. The table can never go stale, because it's remade from the curves every build.
- **B.** The same tool, run once, with its output committed. Then a test has to catch it going stale, and every curve change needs two CI runs.
- **C.** Compute it when the plugin loads, on a background thread. Not "at build time", and the plugin would need a fallback until it finishes.

Tone gets its own small table (13 points, −6 to +6 dB) from the same tool, so a tilt doesn't change the level either. Wet, Dry, Output and the band limits aren't compensated: with Low or High set, auto gain still assumes the full band is saturated, so it compensates a little too much. The manual says so.

### Listen to added harmonics

Temporary, per §0.3: `ListenCheckbox`, `FancyProcessor::setListening`, the LISTENING tag, never saved, automated or undone. While it's on, the output is `added × Wet × Output`: what the saturator adds, already level-matched by auto gain, so the fundamental mostly cancels. With Tone away from 0, the tilt of the band is in it too: it's everything the saturator path changes.

### DC filter

A 5 Hz one-pole high-pass on `added`, not on the whole signal, so with the DC filter on the dry signal and the clean bands keep their own low end, and at Drive 0 it touches nothing (on the full signal, a 5 Hz filter would put the Wet/Dry sum 0.2 dB out at 20 Hz). The bias is already compensated in the curve; the filter catches what's left. Test: Character at both ends, Drive 36 dB, output DC below −100 dBFS.

## Displays

- **Transfer curve** (own component, 170 × 160): input → output of the current Type, Drive and Character, from −1 to +1 at the input, the unity diagonal dashed, the axes in grey-5. The output axis is normalised to the curve's peak, as the mockup draws it.
- **Harmonics** (own component, 432 × 160): a background thread runs a private copy of the saturated path (Type, Drive, Character, Tone, auto gain and the band limits; not Wet, Dry or Output) on a 1 kHz tone at −12 dBFS, 48 kHz, 4× oversampling, whenever one of those changes. A Blackman-Harris FFT reads H1 to H10. Bars in dB relative to the fundamental, from 0 to −72: the fundamental greyed (35 %), even harmonics at full accent, odd at 70 %. The caption is "Harmonics added to a 1 kHz tone at these settings · THD 6.1 %", THD from H2 to H10. With Low above 1 kHz, the tone isn't saturated, and the caption says "1 kHz is outside the band".

## Parameters

A new plugin, with `stateVersion` from the start.

| ID | Name | Range | Default | Unit | Notes |
|---|---|---|---|---|---|
| `type` | Type | Tape, Tube, Transistor, Clip | Tape | | `ParamSegments`, accent-filled |
| `drive` | Drive | 0–36 | 6 | dB | step 0.1 |
| `character` | Character | −100 to +100 | −30 | % | bipolar, read-out "even 30 %" |
| `tone` | Tone | −6 to +6 | 0 | dB | bipolar, step 0.1 |
| `low` | Low | Off, 20 Hz–2 kHz | Off | Hz | log skew, Off at the left end, blue |
| `high` | High | 1 kHz–20 kHz, Off | Off | Hz | log skew, Off at the right end, blue |
| `autogain` | Auto gain | off/on | on | | footer |
| `wet` | Wet | −inf (−60) to +12 | 0 | dB | right column, as Comp |
| `dry` | Dry | −inf (−60) to +12 | −inf | dB | right column, as Comp |
| `output` | Output | −24 to +12 | 0 | dB | header knob |
| `oversampling` | Oversampling | Off, 2×, 4×, 8× | 4× | | EDIT |
| `osfilter` | Oversampling filter | Minimum phase, Linear phase | Minimum phase | | EDIT |
| `trim` | Input trim | −24 to +24 | 0 | dB | EDIT, small knob |
| `dcfilter` | DC filter | off/on | on | | EDIT |

Listen to added harmonics is not a parameter (§0.3). Frequencies accept `2500` and `2.5k`, and Low and High accept "off".

**Smoothing:** Drive, Character, Tone, Low, High, auto gain, Trim, Wet, Dry and Output over 20 ms (Low and High in log frequency, filters recomputed every sample while they move). Type changes crossfade between the old and the new curve over 20 ms.

## Latency

The oversamplers' latency, rounded to whole samples, plus 0 for Off. Reported with `setLatencySamples` on every change. Bypass crossfades to the input delayed by it. The ADAA's half sample is inside the matched path, so it doesn't add to the report. CI measures each setting and the numbers go in `docs/sature-manual.md`.

## Presets

Default, plus Tape warmth, Tube glow, Transistor edge, Clip it, Bass drive (Tube, High 1 kHz), Top sparkle (Low 2 kHz), Parallel crush (Wet −6, Dry 0, Drive 30), in the factory categories of Q2.1. Final values are tuned on the test signals and listed in the manual.

## Footer, drawer, right column

- **Footer:** Auto gain, Listen to added harmonics, then `⚙ EDIT`, as the mockup draws it.
- **EDIT drawer:** Oversampling with its latency note and "aliasing is higher with Off" under it, Oversampling filter, Input trim, DC filter.
- **Right column, 104 px:** Out meter, Wet and Dry faders, as Delay's.

## Tests (§4.4, §0.3 and §7)

| Test | Pass |
|---|---|
| Aliasing: 10 kHz sine, −6 dBFS, Drive 24, 44.1 kHz, Clip | see Decision 7 |
| Auto gain: pink noise at −18 dBFS RMS, every Type, Drive 0, 12, 24, 36 | output within ±1 LU of input |
| Band: Low 200 Hz, 60 Hz sine at −6 dBFS, every Type | no harmonics above −90 dBFS (at every Drive with Decision 1 B) |
| DC: Character ±100 %, Drive 36, every Type | output DC below −100 dBFS |
| Wet 0, Dry 0, Drive 0, both filters, every factor | sum flat within 0.1 dB, 20 Hz–20 kHz, at 48 kHz (at 44.1 kHz, 20 kHz is in the half-band filters' transition; the figure is recorded) |
| Latency | reported equals measured, every factor and filter |
| §0.3 Listen | the shared `checkListenSwitch` |
| Bit-identical automation, random block sizes, offline equals real time, 44.1–384 kHz, mono and stereo, corrupted state | shared tests |
| Exciter unchanged after the move (Decision 3 A) | stored renders and screenshots bit-identical |
| CPU at 4× (median of 5, ratio to Comp) | under 2 % (§7) |

**CPU risk.** Two up/down pairs per channel at 4× plus the shaper is close to the 2 % target. If it's over, the first saving is the matched path for linear phase, where a plain delay gives the same result to within the filter's passband ripple.

### Decision 7: the aliasing test

The brief's test: a 10 kHz sine at −6 dBFS, Drive 24 dB, 44.1 kHz, Clip: aliased components below −90 dBFS at 4×, below −60 dBFS with Oversampling Off. **A model says the design can't meet it, and nor could any 4× design with first-order ADAA.** Drive 24 turns that sine into a near-square wave whose harmonics fall off only as 1/n, so harmonics far above the 4× rate fold back loudly. The model (`docs/research/sature-alias-model.py`, pure Python, ideal up and down filters, so a real build is no better) gives the worst alias below 22.05 kHz:

| 10 kHz, −6 dBFS, Drive 24 | Off | 2× | 4× | 8× | 16× | 32× |
|---|---|---|---|---|---|---|
| Clip, no ADAA | −7.7 | −16.0 | −30.8 | −42.4 | | |
| **Clip, ADAA** | **−17.9** | −29.4 | **−55.9** | −69.3 | −92.7 | −107.9 |
| Transistor, ADAA | −18.0 | −29.7 | −57.9 | −79.0 | | |
| Tape, ADAA | −19.3 | −32.2 | −59.1 | −86.3 | | |

| Clip, ADAA, 4× | Drive 6 | Drive 12 | Drive 24 |
|---|---|---|---|
| 10 kHz | −103.1 | −78.5 | −55.9 |
| 5 kHz | | | −60.3 (8×: −81.8, Off: −18.0) |
| 1 kHz | | | −94.4 (8×: −119.3, Off: −51.7) |

So ADAA is worth about 25 dB at 4×, and −90 dBFS for this signal needs 16×.

- **A. Keep 4× as the default and test what it does** (I recommend this). The brief's signal stays, with limits a few dB inside the model: Clip below −50 dBFS at 4×, −65 at 8×, −15 at Off; plus a 1 kHz tone at the same level and Drive, below −90 dBFS at 4× and −45 at Off. The drawer's note under Oversampling says that Off aliases on bright, heavily driven material and 8× is cleaner. The CPU target stays reachable.
- **B. Add a 16× setting** for the brief's −90 dBFS on the 10 kHz test, with 4× still the default. At 16× the shaper and both oversamplers run four times as often as at 4×: I estimate 6 to 8 % CPU, which is outside every other target in the suite but is opt-in.
- **C. Second-order ADAA.** It gains roughly another 15 to 20 dB, which still falls short of −90 at 4×, costs more per sample, and its second antiderivatives are messier on Tube's and Tape's curves. Not recommended.

## As built (11 November 2026)

Two details the note didn't foresee, both forced by first-order ADAA:

- **ADAA lags by half a sample.** Its output is the curve at the midpoint of each step, so its linear part is the two-sample average of the input. For `added = saturated(mid) − mid` to cancel exactly at low Drive, the reference subtracted inside the saturated path is that same two-sample average, at the oversampled rate. The matched path (dry, and the clean part of the wet signal) gets a **half-sample delay** instead of the average, so it stays flat: a 4-tap Lagrange filter at 4× and 8×, 8 taps at 2× (within 0.1 dB to 20 kHz at 44.1 kHz and above), and the saturated path is delayed by the filter's whole samples to line up. The two differ only in the average's gentle roll-off, so at high Drive a trace of the clean input remains in the top octave: about −26 dB at 20 kHz and −38 dB at 10 kHz at 4×, −14 dB at 20 kHz at 2×. The reported latency adds the filter's delay, rounded: +2 samples at 2×, +0 at 4× and 8× (the true extra is 0.375 and 0.19 samples).
- **Oversampling Off runs the curve without ADAA.** At the base rate the half-sample lag can't be matched without either dulling the dry signal (the average is −12 dB at 20 kHz at 48 kHz) or leaving the clean input's top end almost untouched at high Drive. Without ADAA, Off stays flat, exact and at zero latency, at the cost of about 10 dB more aliasing there: the model gives −42 dBFS for the 1 kHz test (−52 with ADAA) and −8 dBFS for the 10 kHz one. The test limit for Off is −38 dBFS on the 1 kHz tone; the drawer says Off aliases on bright, heavily driven sounds.

Other choices made while building:

- Character's odd half blends towards a symmetric knee at 0.97 to 1.03 for every Type, so it hardens Clip too, rather than towards Transistor's knee as the note said, which would have softened Clip.
- The aliasing tests run with Character neutral and Auto gain off, matching the model's bare curve.
- Low's knob is Off below 20 Hz (the first 5 % of its travel) and High's above 20 kHz (the last 7 %). Both accept "off" when typed.
