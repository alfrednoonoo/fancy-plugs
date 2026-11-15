# Fancy MultiComp: design note

The brief and the mockup's file names call this plugin XComp. The user renamed it to Fancy MultiComp. Bundle ID `com.fancyplugs.multicomp`, plugin code `FMcp`.

## DSP

```
main ─┬─ crossover ─┬─ band 1 ─ × gain ─┐
      │             ├─ band 2 ─ × gain ─┤
      │             └─ ...              + ─ × Wet ─┐
      │                                            + ─ × Output ─ bypass ─ limiter ─ out
      │              (the same bands, summed) × Dry ┘
      └─ delay matching the crossover ──────────────── to bypass

sidechain ─ the same crossover, without compensation ─ per-band detectors
```

- **Bands:** 1-8, always contiguous from 20 Hz to 20 kHz. Each band's top frequency is a crossover, and the last band's top isn't used.
- **Crossover spacing:** at least 1/3 octave. Automation can ask for anything, so the DSP sorts crossovers and enforces the spacing before using them. Dragging on the graph stops at the neighbour.

### Crossover types

**LR 24 dB and LR 48 dB, minimum phase.** Each crossover is a Linkwitz-Riley pair built from TPT state-variable filters. A Linkwitz-Riley low-pass and high-pass sum to an all-pass with the same poles:

- LR 24: `(1 + s⁴) / D₂(s)² = D₂(-s) / D₂(s)`
- LR 48: `(1 + s⁸) / D₄(s)² = D₄(-s) / D₄(s)`

The splits form a cascade: the input splits at the lowest crossover, and each high-pass output splits at the next. A lower band has missed the later splits, so it goes through the all-pass of each crossover above it. An SVF gives that all-pass directly as `in - 2k × band`. Every band then has the same phase, and the sum is flat to rounding error. This is the fix for the "weird phasing" in P3.3.

The filters are in series, so this is the complete identity, not an approximation, as long as the coefficients are held. While a crossover moves it's close, and the test for that is that there are no clicks.

**Linear phase.** One Kaiser-windowed sinc low-pass per crossover, with the band filters made complementary: `band 1 = L₁`, `band k = Lₖ - Lₖ₋₁`, and the last band is `delta - Lₙ₋₁`. They sum to a pure delay exactly, whatever the crossovers are.

- Run by a uniformly partitioned overlap-save convolver, as in the Fancy EQ: 8 partitions of 512 samples at 48 kHz, scaled up by powers of two with the sample rate. The FIR is 4095 samples at 48 kHz.
- Small partitions spread the FFT work evenly, so the audio thread has no spike every 4096 samples.
- Latency is 2559 samples at 48 kHz, about 53 ms. It's reported to the host.
- The transition band is about 75 Hz wide at 48 kHz, whatever the crossover. Below about 150 Hz, the bands overlap more than in LR mode, though the sum stays perfect.
- Designs run on a worker thread, or inline in an offline render, into a spare slot handed over through an atomic index. A change crossfades for one partition.

### Band compressors

Everything here comes from Fancy Comp, moved into `fancy-core`:

- **Gain computer:** `fancy/dsp/Compression.h`, the same quadratic knee.
- **Detector:** one-pole RMS, with 0 ms as peak. L+R modes use the louder channel, so a band's gain is always stereo-linked. There's no Link control, because the mockup has none.
- **Ballistics:** the same follower. Attack and release are time constants to about 63 %, and the Attack = 90 % rise option in EDIT divides the attack by 2.303. Auto release is Fancy Comp's two-stage program-dependent release.
- **Feedback detect:** reads the band's previous output sample.
- **Limit output:** Fancy Comp's true-peak limiter, after the band sum.

**Ratio below 1:1.** The slope is `1 - 1/ratio`, which goes negative, so a level above the threshold gets gain. That's upward expansion above the threshold, and nothing below it. The envelope follows the size of the change, so attack still means the change starting. It's limited to +24 dB so a loud input can't run to +60 dB.

**Auto make-up.** `0.5 ×` the reduction the gain computer gives a 0 dBFS level, from Threshold, Ratio and Knee only, so it never pumps. It's 0 for expansion. The Gain knob's read-out shows the amount added, in brackets.

**Active off** bypasses the band's compressor, and the band stays in the mix. **Solo** leaves only the soloed bands in the wet signal.

### Detector and sidechain

The header's Detector and the band's Sidechain checkbox work together:

| Detector | Band's Sidechain off | Band's Sidechain on |
|---|---|---|
| Main input L+R, L or R | That main signal, filtered to the band | Sidechain, both channels |
| Sidechain L+R, L or R | That sidechain signal, filtered to the band | The same |

So a Sidechain detector makes every band listen to the sidechain, and the checkbox lets one band do that while the rest follow the main input. With no sidechain connected, a band set to listen to it sees silence.

The sidechain has its own crossover with the same frequencies and no compensation, because only its level matters. It runs only while a band needs it.

### Dry

Dry is the sum of the split bands before compression, not the raw input. So in LR modes it has the same phase as Wet and parallel compression doesn't comb. Bypass uses the true input, delayed to match the latency.

### Structure changes

Changing the band count, the crossover type or Limit output is debounced for 50 ms, then the output fades over 2 ms, the change is applied, and the output fades back in once the delay lines have refilled. The same scheme as Fancy Comp.

## Parameters

Per band, with IDs prefixed `b1` to `b8`:

| ID | Name | Range | Default | Unit | Skew | Source |
|---|---|---|---|---|---|---|
| `on` | Active | off/on | on | | | P3.2 |
| `solo` | Solo | off/on | off | | | P3.2 |
| `top` | Top frequency | 20-20000 | 120, 1200, 6000, then upwards | Hz | log, switchable | P3.2 |
| `thr` | Threshold | -60-0 | 0.0 | dB | linear | P3.2 |
| `ratio` | Ratio | 0.5:1-∞:1 | 2:1 | | log, top of range is ∞ | P3.2 |
| `knee` | Knee | 0-24 | 6.0 | dB | skewed | P3.2 |
| `att` | Attack | 0-500 | 10.0 | ms | skewed | P3.2 |
| `rel` | Release | 0-5000 | 120 | ms | skewed | P3.2 |
| `rms` | RMS size | 0-250 | 5.0 | ms | skewed | P3.2 |
| `gain` | Gain | -24-+24 | 0.0 | dB | linear | P3.2 |
| `mk` | Auto make-up | off/on | off | | | P3.2 |
| `ar` | Auto release | off/on | on | | | P3.2 |
| `fb` | Feedback detect | off/on | off | | | P3.2 |
| `sc` | Sidechain | off/on | off | | | P3.2 |

Phase 2.1 U appends `bNup`, `bNuthr`, `bNuratio`, `bNlift` and `bNfloor` per band, then `depth` and `time`, after everything below. Their design is in `docs/phase2_1-U.md`.

Global:

| ID | Name | Range | Default | Source |
|---|---|---|---|---|
| `bands` | Bands | 1-8 | 4 | P3.2, not automatable |
| `detector` | Detector | 6 choices | Main input L+R | P3.1 |
| `crossover` | Crossover | LR 24, LR 48, Linear | LR 24 | P3.2 |
| `link` | Link bands | off/on | off | P3.2 |
| `limit` | Limit output | off/on | off | P3.2 |
| `wet` | Wet | -inf-+12 dB | 0.0 | P3.2 |
| `dry` | Dry | -inf-+12 dB | -inf | P3.2 |
| `output` | Output | -24-+12 dB | 0.0 | Header knob |
| `logFreq` | Log-scale automated freq. | off/on | on | P3.2 |
| `attack90` | Attack = 90 % rise | off/on | off | P3.2 reuse list |
| `showSpectrum` | Show spectrum | off/on | on | P3.1, not automatable |
| `showGrid` | Show grid | off/on | on | P3.1, not automatable |
| `bypass` | Bypass | off/on | off | §4.6 |

## Link bands

P3.3 asks for relative moves of Threshold, Ratio, Attack, Release and Gain across all bands. It's a parameter listener in the processor, so it covers the knobs, the fader, typed values and the graph:

- It only acts during a user gesture on the message thread. Automation, presets and state loads set each band to its own value, so they don't get applied twice.
- Each change adds the same 0-1 distance to the other bands, so each stays offset from the rest until it reaches a limit.
- Holding Alt edits one band.

On the graph, Alt-drag already means Ratio, so a linked handle drag moves all the thresholds, and Alt-drag moves all the ratios.

## Controls the mockup doesn't show

- **EDIT link:** at the bottom of the right column. It opens a drawer with Attack = 90 % rise, Log-scale automated freq. and Output.
- **Threshold:** the brief says "input meter + Threshold vertical fader + GR meter", and the mockup draws three separate parts, so it's the shared `Fader` beside a meter. Comp's over-the-meter `ThresholdMeter` is too tall for this card.

## Typed values

Every knob and fader uses the shared `Knob` and `Fader`, so clicking a value or Ctrl-clicking a control opens the box from v1.0.1.

- Ratio here reads `a:b` as `a / b`, because below 1:1 means something different. `0.5:1`, `0.5` and `1:2` are the same, and `inf` is the top. Fancy Comp accepts either order, because it has no expansion.
- The crossovers are drawn on the graph, so there's no value to click there. Top frequency stays automatable, and appears in the Parameter menu.

## Latency

| Crossover | Limit output off | Limit output on |
|---|---|---|
| LR 24, LR 48 | 0 | Limiter's lookahead, 104 samples at 48 kHz |
| Linear | 2559 samples at 48 kHz | 2663 samples at 48 kHz |

## State format

The standard `FancyProcessor` XML, plus the editor's selected band as a `selectedBand` property.

## CPU

§4.11 sets no target for this plugin. Target here: under 2 % for 4 bands at 48 kHz/128 samples in LR 24 mode, which is four of Fancy Comp's compressors at about 0.5 % each. Measured with a CI benchmark, in `docs/measurements/multicomp.txt`: 2.17-2.42 % for 4 bands in LR 24, so it's a little over. The cost is mostly `log10` and `pow` on every sample of every band.

## How each P3.3 issue is handled

| Issue | Fix | Test |
|---|---|---|
| Weird phasing in the higher bands | Linkwitz-Riley with all-pass compensation, or linear phase | All bands at 1:1 and 0 dB are flat within ±0.1 dB from 20 Hz to 20 kHz in every mode, and Linear nulls against the delayed input below -90 dB |
| Adjusting several bands is tedious | Link bands, with Alt for one band | A gesture on one band moves the others by the same distance, and Alt, automation and presets don't |
| No easy sidechain per band | Detector plus the per-band checkbox, with the sidechain split at the same crossovers | A kick on the sidechain ducks only the bands with Sidechain on |
| Users want a multiband limiter | Limit output after the band sum | Output stays below 0 dBFS with ∞:1, 0 ms attack and Limit output on. The manual explains the setup |
| Unpredictable level with auto make-up | Estimated from Threshold, Ratio and Knee | The make-up doesn't change with the signal, and the read-out shows it |
| Moving crossovers clicks | Log-frequency smoothing with per-sample coefficients | A sweep from 100 Hz to 2 kHz in 200 ms adds no jump beyond the input's own, within a stated margin |

## Phase 3 Q2.6: lookahead, mid/side and CPU

Brief: Q2.6 of `docs/BUILD_PROMPT_PHASE3.md`. Upward compression, Depth, Time and "Big squash" are done (Phase 2.1 U). **Approved by the user on 15 November 2026, with every recommended option (R)**, mockup included. Mockup: `design/phase3-mockup-src/multicomp-q26.html`, rendered as `reference/phase3/26__multicomp_q26_mockup.png`.

### 1. Lookahead

- **What:** the audio is delayed by Lookahead and the detectors aren't, so each band's gain starts moving before the transient it reacts to. 0 to 10 ms, default 0, in 0.1 ms steps.
- **Latency:** reported to the host, added to the crossover's and the limiter's. Dry and the bypass path are delayed to match, so wet and dry stay aligned. A change goes through the existing debounced switch (50 ms settle, 2 ms fade), because the latency changes.
- **Decision 1: one Lookahead for all bands, or one per band.**
  - **A (R).** One, global. Every band then shares the same delay, so the bands stay aligned and sum flat; per-band lookahead would need the longest delay everywhere anyway.
  - **B.** Per band. It sounds flexible, but every band must still be delayed by the largest value, so it only changes how early each detector looks, at the cost of 8 more controls.
- **Where:** the EDIT drawer, as a small knob.

### 2. Channel per band (mid/side)

- **What:** each band compresses Stereo (today), Left, Right, Mid or Side, from a chip on the band row, as Fancy EQ's bands do. A Mid band converts the band's signal to M = (L+R)/2 and S = (L−R)/2, compresses M with its detector reading M, leaves S as it is, and converts back. Left and Right work the same way on one channel. With every band on Stereo the path is today's, to the bit.
- **Upward, sidechain, Solo and Active** work as before on the chosen channel. The band's Sidechain detector reads the same channel of the sidechain.
- **Decision 2: what a non-stereo band does with the other channel.**
  - **A (R).** Passes it untouched, so a Mid band leaves that band's sides alone. One channel per frequency range: Mid and Side of the same frequencies can't be compressed differently.
  - **B.** Two compressors per band, one for Mid and one for Side, each with its own settings. Everything per band doubles (about 200 more parameters), and the band row needs a Mid/Side switch to choose which settings it shows.
- **UI:** a chip reading ST, L, R, M or S between the band name and the S button, with the shared menu; the graph's handle shows the letter for a non-stereo band, as in EQ. On a mono bus the chip is greyed ("Channel needs a stereo bus") and the band works on the mono signal.

### 3. CPU

- **Today:** 4 bands in LR 24 cost about 2.3 % at 48 kHz/128 samples (`docs/measurements/multicomp.txt`). Most of it is `log10` (the detector level) and `pow` (the gain) on every sample, for every band.
- **Change:** a fast base-2 log and exponent (a range reduction and a 5th-order polynomial, accurate to better than 0.005 dB), and the detector's level and the gain computed once per sample per band, already stereo-linked, instead of per channel. Target: 4 bands in LR 24 under 1.5 %.
- **The catch:** the faster maths can't be bit-identical to today's, which the rule for saved projects requires. The brief's own test allows 0.05 dB.
- **Decision 3: how old projects stay identical.**
  - **A (R).** A new **Precision** setting, Exact or Fast, in the EDIT drawer. New instances and presets start on Fast. A project saved before Q2.6 has no Precision, so it loads as Exact and renders as before; the user can switch it.
  - **B.** Fast everywhere. Old projects then change by up to 0.05 dB, which breaks the bit-identical rule, so it would need the user's explicit waiver.
- **Test:** on the existing MultiComp tests, upward compression included, Fast's gain stays within 0.05 dB of Exact's, and Exact renders bit-identically to v1.4.0 (a legacy copy of today's DSP in `tests/`, as for Comp and Verb).

### 4. Controls and layout

- **Main layout:** the only addition is the channel chip on the band row (and the letter on non-stereo handles).
- **EDIT drawer:** today Attack = 90 % rise, Log-scale automated freq. and Output. It gains **Lookahead** (small knob, with the latency next to it) and **Precision** (a dropdown: Exact, Fast).
- **Decision 4 (R):** two factory presets: "Lookahead Glue" (Bus: 4 bands, 2 ms lookahead, gentle ratios) and "Mid/Side Master" (Mastering: low band Mid only, high band Side only, light ratios).

### 5. Parameters and state

Appended after Phase 2.1 U's, so nothing moves:

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `b1chan` … `b8chan` | Band N channel | Stereo, Left, Right, Mid, Side | Stereo | | |
| `lookahead` | Lookahead | 0 to 10 | 0 | ms | linear |
| `precision` | Precision | Exact, Fast | Fast (new instance); Exact for a state without it | | |

`stateVersion` stays at 4. The Precision migration is "absent means Exact", done where the state is applied, and tested.

### 6. Tests

1. **Compatibility:** a v1.4.0 state renders bit-identically (Exact), against a legacy copy of the DSP, in all three crossover modes.
2. **Fast:** within 0.05 dB of Exact on the existing tests, upward compression and expansion included.
3. **Lookahead:** reported latency matches the measured delay; a step at 0 dBFS through ∞:1, 0 ms attack, 5 ms lookahead overshoots the threshold by under 1 dB, against over 6 dB at 0 ms; wet and dry stay aligned.
4. **Channel:** a Mid band at ∞:1 on a signal whose sides are louder than its middle leaves the sides within 0.01 dB; a Side band does the reverse; Stereo is bit-identical to today.
5. **CPU (§0.4):** 4 bands LR 24, Exact and Fast, and Fast with everything on, in `docs/measurements/multicomp-q26.txt`. Target: Fast under 1.5 %.

### 7. Build order

One push: lookahead, channel, Fast/Exact with the legacy copy, the chip, the drawer, presets, tests; then the screenshots against the mockup, the manual, the checklist and CPU.

### 8. As built

- **Precision removed (the user's decision, 15 November 2026).** Measured with the same input for both, Fast cost 2.11 % against Exact's 1.86 % (run 43680967298): MSVC's `log10` and `pow` are cheaper than section 3 assumed, so the fast maths saved nothing. The Fast maths and the `precision` parameter, never released, are gone; MultiComp always uses the exact maths, so old projects stay bit-identical with no migration. The CPU target is restated as no more than before Q2.6. Sections 3, 6 (the Precision row), 7 (test 2) and the drawer's Precision describe the plan, not the build.
- **Shared chip:** Fancy EQ's channel chip moved into `fancy-core` as `fancy::ui::ChannelChip`; EQ uses it unchanged. On the band row it sits at x 494, left of S, and the band title stops at x 490.
- **Lookahead:** a ring per band of up to 10 ms at the session rate, used only when Lookahead is above 0; the bypass path waits the crossover's latency plus the lookahead.
- **Legacy copy:** `tests/LegacyMultiCompDsp.*` is the DSP before Q2.6.
- **Lookahead tests:** a ±0.9 square-wave step (1 kHz) through ∞:1 with a 2 ms attack: the first 10 ms are within 1 dB of the peaks 20 to 40 ms later with 10 ms of lookahead, and over 6 dB louder without. (A sine with RMS 0 makes the detector climb over tens of ms, as it reaches each peak only briefly.) Alignment compares the render with 5 ms of lookahead against the one without, 240 samples later, since an LR crossover's bands sum to an all-pass rather than the input.
