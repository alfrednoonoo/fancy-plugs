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

## Phase 4 Wave 1: learn crossovers and band mute

Items from `docs/enhancements.md` (Wave 1 of the Phase 4 order). **Approved by the user on 21 November 2026 with every recommended option (R).** Every new parameter is appended, with a default that keeps saved projects bit-identical, checked against the v1.6.0 DSP in a legacy test as Comp's Wave 1 does. Items were checked in the code on 21 November 2026 before this note was written. Backlog items 4 and 7 of section 7. MultiComp wasn't in the Wave 1 plugin order (`docs/enhancements.md`); these are its two Wave 1 items, placed after DS-Mini unless the user moves them.

### 1. Learn crossovers

A **Learn** button on the graph: over 5 s of playback, the spectrum is averaged and the crossovers are placed where the material's energy splits evenly between the bands (in pink-weighted energy, so the lows don't take every band). An action, one undo step; it moves only the crossover parameters.

- **Decision 1: the split.**
  - **A (R).** Equal energy per band, pink-weighted: each band works on a similar share of the music.
  - **B.** Fixed musical defaults scaled to the material's own range (where its energy starts and ends). Predictable, less adaptive.

### 2. Band mute

`bNmute` per band (8 parameters), off by default: a muted band is taken out of the sum entirely (Active off keeps it in the mix, unprocessed). An M button in the band row, before the channel menu, lit in the band colour when on.

### 3. Tests

1. Legacy: identical with no band muted.
2. Mute: a muted band's centre drops by over 30 dB (the neighbours' LR24 skirts still leak about −37 dB there, so the 60 dB first written here can't hold); the rest stays within 0.1 dB.
3. Learn: pink noise gives crossovers near today's defaults; a bass-heavy test moves the lowest crossover down.

Mockup: `reference/phase4/35__multicomp_w1_mockup.png` (source `design/phase4-mockup-src/w1_mockups.py`): the Learn button and the M buttons.

### 4. As built

One push (`98b143e`), with every recommended option; `tests/MultiCompW1Tests.cpp`.

- **Learn crossovers:** a button at the graph's top right; over 5 s of playback it averages the spectrum in third octaves and places the crossovers where the pink-weighted energy splits equally between the bands. One undo step; only the crossover parameters move.
- **Band mute:** `bNmute` per band, an M button in the band row lit in the band colour; a muted band is out of the sum entirely.

## Phase 4 Wave 2: dynamic-phase crossover

**Approved by the user on 21 November 2026 with every recommended option (A).** Backlog item 1 of `docs/enhancements.md` section 7, the first item of Wave 2. A fourth Crossover choice, **Dynamic phase**: with no latency, the signal is untouched while the bands aren't changing it, and phase shift appears only where neighbouring bands' gains differ and only while they do (the behaviour of Pro-MB's mode of the same name). There's no published method, so it was modelled first: `docs/research/multicomp-dynphase-model.py` (three bands, crossovers at 200 Hz and 2 kHz, analogue responses, against today's compensated LR 24).

### 1. What the model found

| Candidate | At unity | Every band −6 dB | Band 2 −6 dB, error against LR 24's shape | Verdict |
|---|---|---|---|---|
| Today, LR 24 compensated | flat, but 180° of all-pass phase always | exact | (the reference) | |
| Residual: in + Σ (gₖ − 1) bandₖ on LR bands | exact, no phase | 9.5 dB ripple | 5.7 dB | the bands sum to the all-pass, not to 1 |
| Residual with the common gain taken out first | exact | exact | 2.4 to 6.1 dB, depending on the gains | still several dB off at crossovers |
| Complementary split (1 − L) | exact | exact | a −6 dB cut reaches only −1.9 dB; a +6 dB boost leaks 6.9 dB outside the band | the bands aren't in phase with the input |
| Crossfade dry ↔ LR by activity | exact | exact | | halfway through a fade the two cancel: −66 dB notches at the crossovers |
| **Shelves in series, order 2** | **exact, no phase** | **exact** | **0.5 dB (1.9 dB at −12/0/−12)** | **recommended** |
| Shelves in series, order 4 | exact | exact | 1.3 dB (3.4 dB) | sharper than LR 24's summed shape, so further from it |

**How the recommended method works.** The audio isn't split at all. The band gains are applied as a minimum-phase filter: band 1's gain, then at each crossover a high shelf from the band below's gain to the band above's. When neighbouring gains are equal that shelf is exactly 1, so it adds no phase. Phase appears only around crossovers where the bands differ: at most about 25° for a 6 dB difference, against today's full all-pass rotation all the time. The crossover is where it differs from LR 24. LR sums the bands' amplitudes there (the mean of the gains), and a shelf passes their geometric mean: −3.0 dB instead of −2.5 dB for a band at −6. In the band, the gain is exact (−6.00 against −5.83).

### 2. How the rest of MultiComp works in this mode

- **Detection:** the detectors still need bands, so the input (or sidechain) is split with today's LR 24 crossover for the detectors only, as the sidechain is now. A band set to Feedback reads an LR 24 split of the output, run only when a band uses Feedback.
- **Gain:** each band's smoothed gain (compression, make-up, Active, Wet) goes to the shelves. The shelves are TPT state-variable filters (the Cytomic form, which stays clean under fast modulation). Each crossover's tan term is fixed, so a sample needs one exp per shelf for √A: 7 shelves × 2 channels at most.
- **Channel per band** (Phase 3 Q2.6): two cascades, one on L/R for Stereo, Left and Right bands, then one on M/S for Mid and Side bands. A band's gain is 1 in the domain it doesn't use.
- **Dry:** the input itself (delayed by Lookahead), with no phase at all, where today Dry is the bands summed.
- **Lookahead:** as now, the audio path is delayed and the detectors aren't.
- **Latency:** 0 plus Lookahead. CPU should be close to LR 24's (one LR split for detection, plus at most 14 second-order shelves). Measured with §0.4 before release; the target is under 1.3 × LR 24's.

### 3. Decisions

- **Decision 1: the shelf order.**
  - **A (R).** Order 2: closest to the LR 24 band shapes users know (0.5 dB), with the least phase.
  - **B.** Order 4: steeper transitions between bands, 1.3 dB from LR 24's shapes and more phase.
- **Decision 2: Solo and band Mute in this mode.** A shelf can't silence a band: order-2 shelves spread an 80 dB step over about 7 octaves.
  - **A (R).** While any band is soloed or muted, the audio runs through today's LR 24 bands, switched with a 20 ms crossfade. The model shows a crossfade only cancels while the two paths differ in phase, so the 20 ms is kept short and happens only when you press Solo or Mute.
  - **B.** Solo and Mute are greyed in Dynamic phase.
- **Decision 3: the menu.** "Dynamic phase" is appended as the fourth Crossover choice (LR 24, LR 48, Linear, Dynamic phase), so stored indices keep their meaning, as Fancy FIR's Resonance mode was added. The default stays LR 24, so saved projects are bit-identical. Host automation of Crossover in an old project would map to the new choices differently; Crossover changes the plugin's structure, so it's rarely automated. No layout change and no mockup: it's one menu entry.

### 4. Tests

1. Legacy: LR 24, LR 48 and Linear render as v1.6.0 (and as Wave 1).
2. Unity: with every band idle (no gain reduction, make-up 0), the output equals the input bit for bit, at 0 latency.
3. Equal gains: every band at −6 dB is a −6 dB gain within 0.01 dB, with no phase shift.
4. Shape: band 2 at −6 dB is within 0.6 dB of LR 24's magnitude at every frequency (the model's 0.5 dB at 48 kHz, in the digital domain).
5. Modulation: a 1 ms attack on a sine sweep adds nothing above −90 dB that LR 24 at the same gains doesn't (no zipper from the shelves).
6. Solo and Mute (Decision 2 A): switching in and out makes no step larger than the signal's own.
7. Channel per band: a Mid band compresses only the mid; L/R balance holds.

### 5. Build order

Approval of this note, then one push: the shelf cascade (`fancy-core`, so a multiband Gate or Limit in Wave 3 can share it), MultiComp's Dynamic phase path, the tests, the manual and the checklist.

### 6. As built

One push, with Decisions 1 to 3 all A.

- **The cascade:** `fancy::dsp::ShelfCascade` (`fancy-core/include/fancy/dsp/ShelfCascade.h`, header-only, so a multiband Gate or Limit can use it): band 1's gain, then a second-order TPT high shelf (Simper's form, Q 0.707) at each crossover from the band below's gain to the band above's, with A = sqrt (step) and g = tan (pi f / fs) x sqrt (A). At a step of exactly 1 the shelf's output is its input, to the bit, and its state keeps running, so it starts shaping without a click.
- **MultiComp:** `CrossoverType::dynamic`, appended. The main crossover runs as LR 24 for the detectors (and for Solo and Mute). Each band's gain, with Active's mix, is recorded per sample on L/R (Stereo, Left, Right bands) and M/S (Mid, Side bands); `processDynamic` runs the input, delayed by Lookahead, through an L/R cascade and, only while a band is Mid or Side, an M/S one. Dry is the delayed input. While any band is soloed or muted the output blends over 20 ms to the LR 24 bands (Decision 2 A).
- **Changed from section 2:** Feedback needs no extra split. A Feedback band's detector already reads its LR 24 band times its own gain, which is that band of the output.
- **Tests:** `tests/MultiCompW2Tests.cpp`. The legacy check for LR 24, LR 48 and Linear is MultiCompQ26Tests' test against the old DSP, which runs every crossover type.

## Phase 4 Wave 2: per-band lookahead, detector EQ, crossover slope and GR history

**Draft for approval (23 November 2026).** Items 23 to 26 of the Wave 2 order (`docs/enhancements.md`; backlog items 2, 6, 3 and 5 of section 7), for v1.7.1, in one note because they share the band row and the EDIT drawer. Modelled first: `docs/research/multicomp-w2-model.py` (items 23 to 25; item 26 is a display and doesn't change the sound).

### 1. Per-band lookahead (item 23)

Today one **Lookahead** (0 to 10 ms) delays every band's audio against its detector. Per band, each band gets its own **Lookahead** (`bNlook`, 0 to 10 ms). All the audio is delayed by the largest setting, so the bands stay lined up and the reported latency is that largest value, and each band's detector reads its own amount ahead. The global Lookahead becomes the default for new bands and sets all bands at once. A saved project loads with every band at its old global value, so it renders as before, bit for bit.

What the model found (4:1, threshold −24 dB, attack 0.5 ms; "gets past" is how far the hit exceeds the target gain in its first 3 ms, "turned down early" is how much the sound just before it is reduced):

| Band | Lookahead | Hit gets past by | Sound before it turned down early |
|---|---|---|---|
| High, a snare crack over hi-hats | 0 ms | 13.8 dB | 2.6 dB |
| | 2 ms | 1.1 dB | 7.2 dB |
| | 5 ms | 1.1 dB | 16.3 dB |
| Low, a bass note swelling in | 0 ms | 0.1 dB | 0 dB |
| | 5 ms | 0 dB | 0.5 dB |

Lookahead helps a band with sharp transients, and 2 ms is enough there; longer only ducks what comes before. On a low band whose notes swell in it buys nothing. One value for every band is a compromise that per-band settings avoid.

### 2. Per-band detector EQ (item 24)

Each band's detector already hears only its band, but sometimes only part of the band should drive it. Per band: **Detector HP** and **Detector LP** (Off, or 20 Hz to 20 kHz, 24 dB/oct) and **Tilt** (−6 to +6 dB/oct around the band's centre), on the detector only, never on the audio. Off and 0 by default, and not run then.

What the model found: a 20–150 Hz band with a 50 Hz kick and a 100 Hz bass. With the detector hearing the whole band, the kicks get 13.3 dB of reduction and the bass notes 9.0 dB. With the detector high-passed at 80 Hz, the kicks get 5.4 dB and the bass 8.4 dB, so the bass sets the gain and the kick passes.

### 3. Per-band crossover slope (item 25)

Each crossover gets its own **Slope**: LR 24 or LR 48 dB/oct, in minimum-phase mode. Today's single crossover type stays as the default for all of them. Linear phase and Dynamic phase keep their own shapes, so per-crossover slopes apply to the LR types only.

What the model found, with crossovers at 150 Hz, 1.5 kHz and 6 kHz:

| Slopes | Sum, each lower band through each crossover's own all-pass | Sum, if every lower band used a 24 dB all-pass |
|---|---|---|
| 24, 24, 24 | flat within 0.0000 dB | flat within 0.00 dB |
| 48, 48, 48 | 0.0000 dB | 0.42 dB |
| 48, 24, 48 | 0.0000 dB | 0.51 dB |
| 24, 48, 24 | 0.0000 dB | 0.08 dB |

Mixed slopes sum flat if each band below a crossover goes through that crossover's own all-pass: the all-pass of its own order. That's how the cascade is built already, so mixing needs no extra correction, only the right all-pass per crossover. Using one all-pass shape everywhere would leave up to 0.5 dB of ripple.

### 4. Per-band GR history (item 26)

The graph gets a **GR history** view: each band's gain reduction over the last 3 s as a trace in the band's colour, stacked under the spectrum, with each band's peak reduction beside its trace. It uses the shared `fancy::ui::LevelHistory` widget Comp's History page uses. It's a view switch (Spectrum | GR history), not a parameter, and draws only while the window is open.

### 5. Decisions

- **Decision 1: per-band lookahead's controls.**
  - **A (R).** A small **Look** knob in each band row (0 to 10 ms), and the EDIT drawer's Lookahead becomes **Set all bands**, which moves every band's knob at once. A mockup follows approval.
  - **B.** Per-band lookahead only in the band's drawer section, with the global value kept as an offset.
- **Decision 2: the detector EQ's controls.**
  - **A (R).** In the band's drawer section, under its Sidechain checkbox: HP and LP as small knobs, and Tilt. The band row doesn't change.
  - **B.** A shared "detector EQ" page in the EDIT drawer showing all bands at once.
- **Decision 3: crossover slopes.**
  - **A (R).** Per crossover, from a right-click menu on the crossover line on the graph and in the EDIT drawer as a row per crossover. The Crossover type menu keeps its four choices, and picking LR 24 or LR 48 sets all of them.
  - **B.** Per band (the slope of its upper crossover), in the band row.
- **Decision 4: GR history.**
  - **A (R).** A Spectrum | GR history switch at the graph's top left; the history replaces the spectrum while chosen.
  - **B.** The traces drawn over the spectrum, always, in faded colours.

### 6. Tests

1. Legacy: a project saved with v1.7.0 (one Lookahead, one crossover type, no detector EQ) renders exactly as before.
2. Per-band lookahead: with band 1 at 0 ms and band 3 at 5 ms, the reported latency is 5 ms, the bands sum flat at unity gain, and band 3's gain moves 5 ms ahead of its audio while band 1's moves with it.
3. Detector EQ: the model's band, HP 80 Hz: the reduction during kicks is under half of what it is without, within 1 dB of the model's.
4. Mixed slopes: crossovers 48, 24, 48 dB/oct sum flat within 0.01 dB from 20 Hz to 20 kHz at 0 dB gains, and a sweep across a slope change doesn't click.
5. GR history: draws each active band's trace and peak; nothing runs while the window is closed (the editor-off CPU test).
6. CPU: every band with lookahead, detector EQ and 48 dB/oct slopes adds under 20 % to MultiComp at its defaults.

### 7. Build order

Approval, then the mockup (`reference/phase4/58__multicomp_w2_mockup.png`): the band row's Look knob, the drawer's detector EQ and slope rows, and the GR history view. Then one push with the four items, tests, manual and checklist.
