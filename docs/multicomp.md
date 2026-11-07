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

§4.11 sets no target for this plugin. Target here: under 2 % for 4 bands at 48 kHz/128 samples in LR 24 mode, which is four of Fancy Comp's compressors at about 0.5 % each. Measured with a CI benchmark, in `docs/measurements/multicomp.txt`.

## How each P3.3 issue is handled

| Issue | Fix | Test |
|---|---|---|
| Weird phasing in the higher bands | Linkwitz-Riley with all-pass compensation, or linear phase | All bands at 1:1 and 0 dB are flat within ±0.1 dB from 20 Hz to 20 kHz in every mode, and Linear nulls against the delayed input below -90 dB |
| Adjusting several bands is tedious | Link bands, with Alt for one band | A gesture on one band moves the others by the same distance, and Alt, automation and presets don't |
| No easy sidechain per band | Detector plus the per-band checkbox, with the sidechain split at the same crossovers | A kick on the sidechain ducks only the bands with Sidechain on |
| Users want a multiband limiter | Limit output after the band sum | Output stays below 0 dBFS with ∞:1, 0 ms attack and Limit output on. The manual explains the setup |
| Unpredictable level with auto make-up | Estimated from Threshold, Ratio and Knee | The make-up doesn't change with the signal, and the read-out shows it |
| Moving crossovers clicks | Log-frequency smoothing with per-sample coefficients | A sweep from 100 Hz to 2 kHz in 200 ms adds no jump beyond the input's own, within a stated margin |
