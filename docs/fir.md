# Fancy FIR: design note

A spectral processor with five modes: EQ, Gate, Compressor, Subtract and Convolve L/R. Bundle ID `com.fancyplugs.fir`, plugin code `FFir`.

## Engine

**Short-time Fourier transform**, weighted overlap-add, at 75 % overlap, so the hop is N/4.

- **Normal:** square-root Hann analysis and synthesis windows. At unity they reconstruct the input exactly, delayed by N.
- **Low latency:** the same N-point analysis, so the same frequency resolution, with an asymmetric window pair:
  - The analysis window rises over most of the frame and falls over the last N/4.
  - The synthesis window spans only the last N/2, and their product is an N/2-point Hann.
  - The pair reconstructs exactly at hop N/4, and the latency is N/2.
  - The cost is more time-domain smearing when the mask changes quickly.
- **Large sizes:** at N of 8192 and above, one frame's FFTs run in six steps spread over half a hop, so the audio thread never does them all in one block. This adds N/8 to the latency. Below 8192 a frame runs all at once.

| FFT size | Latency, normal | Latency, Low latency |
|---|---|---|
| 128-4096 | N | N/2 |
| 8192-32768 | N + N/8 | N/2 + N/8 |

At 48 kHz, 2048 is 2048 samples, 42.7 ms, as the mockup shows. The latency doesn't depend on the mode, and Convolve L/R ignores Low latency.

**Levels.** A bin's level is calibrated so that a sine at amplitude A reads `20 log10 A` dB, using the analysis window's sum, and is the same in every mode and at every size. The display uses Fancy EQ's calibrated analyzer, from `fancy-core`, so its readings match.

**Changing size, Low latency or Convolve L/R.**

1. A worker thread builds a new engine with its own buffers and FFTs.
2. Once it arrives, the new engine runs beside the old one until its own latency has passed and its output is real.
3. The output crossfades from old to new over 20 ms.
4. The old engine goes back to the worker to be freed, and the new latency is reported to the host.

An offline render builds inline instead, so the result doesn't depend on thread timing. Changing between EQ, Gate, Compressor and Subtract keeps the engine, and the overlapping frames blend the change over N samples.

**Reduce artifacts.** Smooths each frame's gain mask across bins with the kernel `[1/4, 1/2, 1/4]`. That's the same as multiplying the mask's impulse response by a Hann lag window, so less of it wraps round the frame. On by default.

## Modes

Every mode except Convolve L/R makes a real, zero-phase gain for each bin of each channel.

| Mode | Curve means | Gain |
|---|---|---|
| EQ | Gain, dB | The curve |
| Gate | Threshold per frequency | Open (unity) at or above the threshold, Floor below it |
| Compressor | Threshold per frequency | Fancy Comp's gain computer with no knee, at Ratio. Below 1:1 it's upward expansion, limited to +24 dB |
| Subtract | Not used. The noise profile is drawn instead | A Wiener mask (below) |

**Detector reach.** A sine covers about five bins of the analysis window. Gate and Compressor give each bin the loudest level within two bins either side, so a sine opens or ducks as one, rather than losing its outer bins below the threshold. A sidechain therefore reaches about ±25 Hz around its frequencies at 4096 points.

**Attack and Release** in Gate and Compressor are time constants in ms, the time to reach about 63 % of the target. They're converted to a coefficient for each hop from the hop's own length, `1 - exp(-hop / τ)`, so they don't depend on the FFT size. Gate's ballistics act on the gain, and Compressor's on the gain reduction in dB.

**Subtract** is a decision-directed Wiener filter:

1. The noise power for each bin is the profile scaled by Offset. With Sidechain detector on, it's the sidechain's power instead.
2. The signal power is smoothed across neighbouring bins, over ±(Smoothing × 100) Hz.
3. The a priori SNR blends the last frame's estimate with this one's, with a time constant of 10 + 190 × Smoothing ms. The usual weight is 0.98.
4. The gain is `ξ / (1 + ξ)`, never below Reduction.
5. Gains fall at the Release time and rise at once.

Smoothing in both time and frequency is what stops isolated bins flickering, the "musical noise". The Reduction floor means no bin is zeroed.

**Learn** captures the average power of each bin over the learn length, default 2 s, then stops by itself. It only counts while audio runs. The profile is held at a fixed grid of 2049 frequencies from 0 to 24 kHz, as power per bin divided by the analysis window's energy. That makes it the same at any FFT size, and it's saved as raw floats, so it reloads exactly. With no profile and no sidechain, Subtract passes the signal through.

**Sidechain detector**, for Gate, Compressor and Subtract: Gate and Compressor compare each bin of the sidechain against its threshold. Subtract treats the sidechain as the noise. The sidechain goes through its own analysis, with no synthesis.

**Listen** to the removed part: the output becomes `X × (1 - G)`. It's offered in Gate, Compressor and Subtract.

**Convolve L/R:**

- Each N-sample block of the left channel is convolved with the matching block of the right one, using a 2N-point FFT and overlap-add. The right channel acts as an impulse response, renewed every N samples, and the result goes to both outputs.
- There's no normalisation, so a dense right channel is loud, and the Gain fader is there for that.
- On a mono track, the input passes through.

## Curve

Three curves are kept, one each for EQ, Gate and Compressor, so changing mode doesn't lose them. Their defaults are 0 dB, -60 dB and -30 dB.

- **Points (smooth):** monotone cubic interpolation in log frequency, so it never overshoots its points.
- **Points (flat):** straight lines between points, in log frequency.
- **Freehand:** a table of 512 values from 20 Hz to 24 kHz, drawn with the mouse. Switching to Freehand starts the table from the current points. Switching back shows the points again.

Outside the first and last points, the curve holds their values. A curve is stored as up to 64 points plus its table, in the plugin state.

The message thread renders the active curve of each mode to 512 log-spaced values in atomics and bumps a version number. The engine re-reads them into its bins when the version changes. A frame that reads part of an update just blends the two, and the overlap smooths that.

## Parameters

| ID | Name | Range | Default | Unit | Source |
|---|---|---|---|---|---|
| `mode` | Mode | EQ, Gate, Compressor, Subtract, Convolve L/R | EQ | | P4.2 |
| `edit` | Edit | Points (smooth), Points (flat), Freehand | Points (smooth) | | P4.2, not automatable |
| `fft` | FFT size | 128-32768, in octaves | 4096 | | P4.2 |
| `lowLatency` | Low latency | off/on | off | | P4.2 |
| `reduce` | Reduce artifacts | off/on | on | | P4.2 |
| `gain` | Gain | -24-+24 | 0.0 | dB | P4.2 |
| `gateFloor` | Floor | -inf-0 | -40 | dB | P4.2, -inf at -90 |
| `gateAttack` | Attack | 0-200 | 1.0 | ms | P4.2 |
| `gateRelease` | Release | 0-2000 | 80 | ms | P4.2 |
| `compRatio` | Ratio | 0.5:1-100:1 | 2:1 | | P4.2, log |
| `compAttack` | Attack | 0-200 | 5.0 | ms | P4.2 |
| `compRelease` | Release | 0-2000 | 80 | ms | P4.2 |
| `reduction` | Reduction | -60-0 | -18 | dB | P4.2 |
| `offset` | Offset | -20-+20 | 0.0 | dB | P4.2 |
| `smoothing` | Smoothing | 0-100 | 35 | % | P4.2 |
| `subRelease` | Release | 10-500 | 80 | ms | P4.2 |
| `learnLength` | Learn length | 0.5-10 | 2.0 | s | P4.2, not automatable |
| `sidechain` | Sidechain detector | off/on | off | | P4.2 |
| `showAnalysis` | Show analysis | off/on | on | | P4.2, not automatable |
| `graphTop` | Graph top | 0-+48 | +24 | dB | P4.1, not automatable |
| `graphFloor` | Analysis floor | -150 to -30 | -90 | dB | P4.2, not automatable |
| `output` | Output | -24-+12 | 0.0 | dB | Header knob, as on every plugin |
| `bypass` | Bypass | off/on | off | | §4.6 |

"ReaFir 0.x compatibility mode" is dropped, as P4.3 says.

`listen` (Listen to removed part), which came after `sidechain`, was removed on 10 November 2026: it's now a temporary switch (`FancyProcessor::setListening`, Phase 3 new-plugins brief §0.3), shown as "Listen to removed noise" in Subtract and "Listen to gain-reduced part" in Gate and Compressor. A saved `listen` value is ignored.

Phase 2.1 A appends `source` (Learned, Adaptive, Both) and `adaptSpeed` (0.5-10 s, default 2 s) after everything above. Their design is in `docs/phase2_1-A.md`.

## Controls the mockup doesn't show

- **Learn length:** P4.2 asks for it and the mockup has no control. It's a small knob in an EDIT drawer, opened from an `EDIT` link at the right of the footer, as on the Delay.
- **Output:** a small knob in the same drawer, because the header knob shows no value.
- **Graph range:** the axis labels at the top and bottom of the graph can be double-clicked to type a new top or floor, as P4.1 says.

## Typed values

Every knob and fader uses the shared `Knob` and `Fader`, so clicking a value or Ctrl-clicking a control opens the box from v1.0.1.

- **Ratio:** reads `a:b` as `a / b`, as in MultiComp.
- **Curve points:** right-click a point to type its frequency and gain. Frequency accepts `2500` or `2.5k`.
- **Graph axis:** double-click the top or bottom label to type it.

## State format

The standard `FancyProcessor` XML, plus two children:

- **`Curves`:** one `Curve` per mode, with its points as text and its freehand table as base64.
- **`Profile`:** the learned power as base64 floats, its captured length in seconds, and whether there is one.

A state with no profile, or one of the wrong size, loads as having none.

## CPU

§4.11 sets no target. Target here: under 3 % at 48 kHz/128 samples for 4096 points in each mode, which is two forward and two inverse FFTs per 1024 samples plus the mask. Measured with a CI benchmark, in `docs/measurements/fir.txt`: 0.5-1.4 % across the modes in one run and 0.9-3.8 % in the next, on the same code. The busiest block was 29 % of its time in the first run, with 32768 points included, so the staging works. The second run had stalls of about 50 ms that the first didn't.

## How each P4.3 issue is handled

| Issue | Fix | Test |
|---|---|---|
| Latency too high for live use | Exact `setLatencySamples`, a header read-out, and Low latency at N/2 | The reported latency equals the measured impulse delay in every mode at every size, both ways |
| Musical noise when subtracting hard | Decision-directed Wiener mask, smoothed in time and frequency, with a reduction floor | White noise plus a sine at -18 dB: the residual's frame-to-frame variance is well below plain subtraction's, and the sine loses under 0.5 dB |
| Noise learning is easy to get wrong | Learn captures a fixed length and stops, and the profile is saved | Learning stops by itself, and a saved profile reloads bit-identical |
| Attack and release depend on FFT size | Times in ms, converted for each hop | The 63 % release time is within 10 % of the set value at 1024 and 8192 |
| Sidechain wanted in Compressor and Subtract | Sidechain detector, compared bin by bin | A 1 kHz sidechain reduces only the bins near 1 kHz |
| The analysis is only approximate | Fancy EQ's calibrated analyzer | A -12 dBFS sine reads -12 dB ± 0.5 |
| 0.x compatibility mode | Dropped | |
| Changing size or mode while playing glitches | Engines built off the audio thread, warmed up and crossfaded | A size change mid-stream ends on the new latency with no discontinuity, and the new latency is reported |

## Phase 3 Q2.7: Resonance mode and CPU stability

Brief: Q2.7 of `docs/BUILD_PROMPT_PHASE3.md`. **Approved by the user on 15 November 2026, with every recommended option (R)**, mockup included. Mockup: `design/phase3-mockup-src/fir-q27.html`, rendered as `reference/phase3/28__fir_q27_mockup.png`. Model: `docs/research/fir-resonance-model.py`, results in `fir-resonance-model.txt`. Every new setting is heard only in the new mode, so old projects render bit-identically.

### 1. What Resonance does

A sixth mode that finds narrow peaks standing above the spectrum around them and turns them down, by up to Depth, only while they're there. A whistling snare, a boxy vocal or a ringing room mode comes down; the rest of the spectrum doesn't move.

Per frame, on the existing engine (same FFT size, Low latency, Reduce artifacts and latency as the other modes):

1. **Detector:** each bin's power, averaged over ±1 bin and smoothed over a fixed 80 ms, separately from Attack and Release. Without that smoothing, the random bin-to-bin peaks of noise are found as resonances.
2. **Envelope:** for each bin, the mean level in dB on each side of it, over a window set by Sharpness, leaving out ±2 bins round it (a sine's main lobe). The envelope is the **louder** side. A peak is below its envelope on both sides; a step, a shelf or a band edge has one side as loud as itself, so it's never taken for a peak. Two more passes clip each level at envelope + 4 dB and average again, so a peak doesn't lift its own envelope.
3. **Reduction:** `min(Depth, level − envelope − 4 dB)`, so a bin is turned down only once it stands 4 dB above its envelope, and never below that line. Weighted by Range (Decision 2).
4. **Ballistics:** Attack and Release act on the reduction in dB, per hop, as in Compressor mode.
5. **Gain:** `10^(−reduction / 20)`, then Reduce artifacts' smoothing across bins, as in every mode.

The 4 dB tolerance and the 80 ms detector are fixed. The model chose them: at 3 dB or 40 ms, noise lost up to 0.5 dB in some octaves; at 4 dB and 80 ms, 0.03 dB at most.

**Sharpness** sets the envelope window: 2 octaves wide at 0 % (broad bumps count as peaks), 1/3 octave at 100 % (only very narrow ones), on a log scale, and never narrower than ±6 bins. At 4096 points a bin is 11.7 Hz, so below about 100 Hz the window is always more than an octave. Larger FFT sizes resolve low resonances better, and the manual will say so.

### 2. What the model shows

`docs/research/fir-resonance-model.py` runs the algorithm above on the engine's STFT (4096 points, 48 kHz). The gain is measured at the frequency, from the averaged cross-spectrum.

| Test | Result |
|---|---|
| 3 kHz resonance, 12 dB above white or pink noise (peaking +12 dB, Q 8), Sharpness 50 %, Depth 3 dB | −3.0 dB at 3 kHz; 0.0 dB at 1.5 kHz |
| The same, Depth 6 dB | −5.2 to −5.4 dB at 3 kHz; 0.0 dB at 1.5 kHz |
| The same, Depth 12 dB | −5.5 to −5.9 dB: a peak 12 dB up is only brought down to 4 dB above its envelope, by design |
| White and pink noise, 20 Hz to 20 kHz, no resonance, every Sharpness, Depth 6 and 24 dB | 0.03 dB or less in every octave from 31.5 Hz to 16 kHz |
| Pink noise band-limited to 300 Hz to 8 kHz, by a brick wall or 24 dB/oct slopes, Depth 24 dB | 0.16 dB or less in every octave inside the band |
| The 3 kHz resonance with High at 2 kHz; with Range 1 to 10 kHz | 0.0 dB; −5.2 dB |
| Pink noise with Range's Low at 20 Hz instead of 100 Hz | −0.86 dB at 31.5 Hz and −0.54 dB at 63 Hz: pink's slope across a window wider than an octave looks like a peak. Hence Low's default of 100 Hz |

### 3. Decisions

- **Decision 1: how Resonance joins the Mode parameter.**
  - **A (R).** Append "Resonance" to the end of `mode`'s choices, as Comp's 8x was. Saved states store the index, so projects load unchanged. Only a host automation lane drawn on Mode would shift, since its normalised values move; nobody automates the mode, because each change is a jump. The switch shows it last: EQ, Gate, Compressor, Subtract, Convolve L/R, Resonance.
  - **B.** A new parameter `mode2` with six choices, the old one hidden. Safe for automation lanes, but the host lists two Mode parameters for good.
- **Decision 2: Range, where Resonance works.**
  - **A (R).** Two knobs, **Low** (20 Hz to 2 kHz, default 100 Hz) and **High** (1 to 20 kHz, default 20 kHz), in the left card, which Resonance doesn't otherwise use. Full depth between them, fading to nothing over half an octave outside. The graph dims the area outside the range. It keeps a vocal's low end out of it, and Low's default keeps pink noise's lowest octaves still (section 2).
  - **B.** No range: it works everywhere above 100 Hz. Fewer controls, but you can't keep it to the harsh band.
  - **C.** Use the curve, as Gate and Compressor do, for Depth per frequency. The most flexible, but a curve in % on a graph scaled in dB is confusing, and it's more to build.
- **Decision 3: stereo.**
  - **A (R).** Linked: the detector reads the two channels' power together, and both get the same gain, so the stereo image never moves. Resonances are almost always in both channels.
  - **B.** Each channel on its own, as Gate and Compressor do. Catches a resonance in one channel only, but can shift the image while it acts.
- **Decision 4: Sidechain detector in Resonance.** **(R)** Greyed, with the tooltip "Resonance listens to its own input". The other modes keep it. Say if you'd rather it found the peaks in the sidechain instead.
- **Decision 5: Listen.** **(R)** "Listen to removed resonances", the same temporary switch as Gate's and Compressor's (§0.3), in the footer, so you hear only what's taken out.
- **Decision 6 (R): two factory presets.** "Resonance Tamer" (Restoration: Depth 6 dB, Sharpness 50 %, Attack 5 ms, Release 80 ms, default range) and "Harsh Vocal" (Restoration: Depth 8 dB, Sharpness 60 %, Attack 2 ms, Release 60 ms, Range 1.5 to 10 kHz).

### 4. Controls and display

- **Mode switch:** "Resonance" as the sixth segment, in green `#2DCC70`, the one accent FIR doesn't use yet. The switch grows from 308 to about 390 px and still ends before Edit.
- **Left card:** RANGE, with Low and High as compact knobs (Decision 2 A). The Edit dropdown stays as it does in Subtract.
- **Controls card:** Depth, Sharpness, Attack and Release, in the four knob places Subtract uses, then FFT size and Reduce artifacts as today.
- **Graph:** input and output as in every mode, the reduction hanging from 0 dB in green, as Compressor's does in yellow, and the area outside Range dimmed. Legend: input, output, reduction.
- **Footer:** Show analysis, Low latency, Sidechain detector (greyed), Listen to removed resonances, EDIT.

### 5. CPU stability

The brief asks for the cause of the run-to-run variation, a high-priority benchmark like the Verb's, and the 99th percentile.

- **What varies today:** `docs/measurements/fir.txt` has the same code at 0.5 to 1.4 % in one run and 0.9 to 3.8 % in the next, with blocks of up to 1878 % of their time.
- **Likely cause:** FIR's benchmark runs flat out at normal priority and times each block by the clock. The FFT work lands in one block of every eight (one hop at 4096), so a preemption or page fault during that block counts in full, and the runner's other processes decide how often that happens. The Verb's benchmark avoids this: it runs at time-critical priority, paced at real time.
- **The check:** time each block both by the clock and by the thread's own CPU cycles (`QueryThreadCycleTime`). If they differ only in a few blocks, preemption is the cause; if the cycles vary too, it's the CPU (clock speed, cache), and the ratio to Fancy Comp in the same run (§0.4) is the figure to trust.
- **The benchmark after:** time-critical priority, paced at real time, five runs in one build, reporting the median of the mean, the 99th-percentile block and the worst block, for every mode including Resonance, with the ratio to Fancy Comp.
- **Targets:** Resonance under 3 % at 4096 points, like the other modes, and under 1.3 x Compressor mode in the same run. Stability: two CI runs agree within 20 % on each mode's median.

### 6. Parameters and state

Appended after `adaptSpeed`, so nothing moves. `mode` gains "Resonance" at the end (Decision 1 A).

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `resDepth` | Depth | 0 to 24 | 6 | dB | linear |
| `resSharpness` | Sharpness | 0 to 100 | 50 | % | linear |
| `resAttack` | Attack | 0 to 200 | 5 | ms | as `compAttack` |
| `resRelease` | Release | 0 to 2000 | 80 | ms | as `compRelease` |
| `resLow` | Low | 20 to 2000 | 100 | Hz | log |
| `resHigh` | High | 1000 to 20000 | 20000 | Hz | log |

`stateVersion` doesn't change: the new parameters act only in the new mode, and an old state lacks them and gets their defaults.

### 7. Tests

1. **Compatibility:** v1.4.0 states in each of the five old modes render bit-identically.
2. **The brief's resonance test:** a 3 kHz resonance 12 dB above pink noise, Depth 3 and 6 dB: the gain at 3 kHz is Depth ± 1 dB, and at 1.5 kHz within 0.2 dB of 0.
3. **The brief's broadband test:** white and pink noise, every Sharpness, Depth 24 dB: under 0.5 dB in every octave from 31.5 Hz to 16 kHz.
4. **Edges:** noise high-passed at 300 Hz and low-passed at 8 kHz: under 0.5 dB in every octave inside the band.
5. **Range:** the resonance at 3 kHz with High at 2 kHz: under 0.5 dB at 3 kHz.
6. **Linked:** the resonance in the left channel only: both channels get the same gain.
7. **Ballistics:** the reduction reaches 63 % in Attack and recovers 63 % in Release, within 10 %, at 1024 and 8192 points.
8. **Listen:** the output plus the Listen output equals the input, within 0.01 dB.
9. **The usual:** latency equals the measured delay, every rate and block size, random settings stay finite, automation with an unchanged value is bit-identical, presets load.
10. **CPU:** as in section 5, in `docs/measurements/fir-q27.txt`.

### 8. Build order

Two pushes. First the DSP, the parameters, the tests and the CPU work. Then the UI (switch, Range card, knobs, graph), presets, screenshots, manual and checklist. Before the first, a pre-push check that the C++ test signals give the model's figures, as Room's did.

### 9. As built

Released in v1.6.0, run 43742763641 (commit `ff534e3`), 516 test cases.

- **As designed,** with the model's constants (`SpectralEngine::computeResonance`). The C++ gives the model's figures: Depth 3 dB cuts −2.99 dB at 3 kHz and Depth 6 dB −5.32 dB (model −2.98 and −5.2 to −5.4), 0.00 dB at 1.5 kHz; broadband 0.04 dB at worst in any octave; Listen plus output equals the input to −139 dB; release 63 % after 1963 ms of 2000 at 1024 and 8192 points.
- **Attack** is tested at 1024 points only, from 10 % to 63 % (0.889 of the time constant: 853 ms against 889 expected). At 8192 a frame is 170 ms long and the target itself ramps in over it, so attack reads short there, as the model showed.
- **Linked test:** the first version compared the cross-spectrum gain of two different signals and failed by 0.16 dB (run 43741706340), because a gain that moves over time is weighted differently on each. It now checks exactly: a half-level copy in the right channel comes out at exactly half, and swapping the channels swaps the outputs bit for bit.
- **Compatibility:** there's no legacy copy of the engine, since the five old modes' code didn't change. The test removes the six new parameters from a saved state (a v1.5.0 state), checks the mode loads unchanged and the new parameters take their defaults, and checks an old mode renders bit-identically whatever the new parameters are set to.
- **Range readouts** use whole Hz and one decimal of kHz ("100 Hz", "1.5 kHz", "20 kHz"), Fancy EQ's knob format; the mockup drew "20.0 kHz".
- **CPU** (`docs/measurements/fir-q27.txt`): Resonance 1.84 %, 1.04 x Compressor mode (targets 3 % and 1.3 x). Paced at time-critical priority, no block in any mode was preempted, the 99th-percentile block was 11 to 19 % of its time and the worst 26 %. The clock and the thread's own cycles agree within 0.02 % on the mean. The old flat-out benchmark, in the same run, had its worst block at 57 % of its time, against 1878 % in the run recorded in `fir.txt`, on code that is the same for those modes. That fits section 5's explanation (the runner preempting the thread), but one quiet run doesn't prove it; the paced benchmark counts preempted blocks from now on, so a noisy run will show it. The stability target (two runs within 20 % on each mode's median) was checked on run 43773992670: every mode's ratio to Fancy Comp came out 13 to 18 % lower (EQ 1.99 to 1.68, Gate 2.43 to 2.00, Compressor 2.32 to 1.93, Subtract 1.84 to 1.55, Convolve 0.91 to 0.79, Resonance 2.42 to 2.09), within 20 %, again with no preempted block. Absolute figures moved more between runners (Gate 23 %, Resonance 20 %), which is why §0.4 compares ratios. Resonance was 1.08 x Compressor mode.

## Phase 4 Wave 1: resonance tilt and learned curves

Items from `docs/enhancements.md` (Wave 1 of the Phase 4 order). **Approved by the user on 21 November 2026 with every recommended option (R).** Every new parameter is appended, with a default that keeps saved projects bit-identical, checked against the v1.6.0 DSP in a legacy test as Comp's Wave 1 does. Items were checked in the code on 21 November 2026 before this note was written. Backlog items 1 and 3 of section 8.

### 1. Resonance tilt

`resTilt` −100 to +100 %, default 0 (today): Resonance mode's Depth leans towards the highs (positive) or the lows (negative). At +100 % the depth doubles at 16 kHz and halves at 62 Hz, log-linear about 1 kHz; between Low and High only, as Depth is. A small bipolar knob in the Resonance card beside FFT size.

### 2. Learn for the Gate and Compressor curves

A **Learn** button in the top row, between the Mode switch and the Edit menu, shown in Gate and Compressor modes only: over 3 s of playback, every bin's level is measured, and the curve's points are placed on the result.

- **Gate:** the threshold curve at each bin's 20th percentile + 6 dB, so steady noise is under it and the music over it.
- **Compressor:** the threshold curve at each bin's 80th percentile, so only the loud parts of each band are compressed.
- **Decision 1: smoothing.**
  - **A (R).** The learned curve is smoothed across a third of an octave and thinned to the curve editor's points, so it's editable afterwards.
  - **B.** Every bin kept as measured: exact, but a curve no one can edit by hand.
- An action, one undo step. FIR's existing noise learning (Adaptive, "Learned") is separate and unchanged.

### 3. Tests

1. Legacy: v1.6.0's FIR against the new one in every mode: identical.
2. Tilt +100 %: depth at 16 kHz is twice that at 1 kHz (± 10 %).
3. Learn, Gate: white noise at −50 dBFS plus a tone: the learned curve sits 6 ± 1 dB over the noise and under the tone.

Mockup: `reference/phase4/41__fir_w1_mockup.png` (source `design/phase4-mockup-src/w1_mockups.py`).

### 4. As built

One push holds the whole item. `tests/LegacyFirEngine.*` is `SpectralEngine` at v1.6.0, in `fir::legacy`.

- **Tilt:** `resTilt`, appended, -100 to +100 % (0). `SpectralEngine::tiltFactor` is 2^(tilt x log2 (f / 1 kHz) / 4): 2 at 16 kHz and 0.5 at 62.5 Hz for +100 %. Each bin's Depth is scaled by it, inside the Range weighting as Depth is; the factors are worked out once per tilt. At 0 the engine takes the old path, to the bit. The knob is a small bipolar one beside FFT size, as the mockup places it.
- **Learn:** `fir::CurveLearner` (`plugins/fir/src/FirLearn.h`) runs on the graph's analyzer thread, from the same input feed the graph draws, so it never touches the audio thread. Each analysis reads 31 third-octave bands (20 Hz to 20 kHz), each the average of 32 readings across the band (8 at first; run 44946550458 showed the percentile wandering 1.5 dB with so few) (Decision 1 A's smoothing), and after 3 s of input the curve gets a point per band: the Gate's at the 20th percentile + 6 dB, the Compressor's at the 80th percentile. The graph sets it with `setPoints` (or the Freehand table in Freehand mode), one undo step. The button shows progress and stops a Learn when clicked again; it's in the top row in Gate and Compressor modes, and Edit's label moves right to make room.
- **Tests:** `tests/FirW1Tests.cpp`. Test 3 plays the tone 1 s in every 1.5 s: a steady tone is, to a percentile, steady noise, so the Gate would learn over it.

## Phase 4 Wave 2: Resonance depth nodes

**Approved by the user on 21 November 2026 with every recommended option (A), without a Python model (section 3).** Backlog item 2 of `docs/enhancements.md` section 8, the second item of Wave 2. A node curve on the graph in Resonance mode sets how deep the cut may go at each frequency, the way soothe2's node graph does. It generalises Wave 1's Tilt: Tilt is one slope, and the nodes are any shape.

### 1. What it does

- In Resonance mode the graph draws the **depth ceiling**, the most each frequency may be cut (Depth × the curve × Tilt, inside Low to High), as a line hanging from the 0 dB line. The green reduction moves inside it. The curve has nodes, edited as the Gate and Compressor curves are: double-click to add, drag, right-click to delete, and the Edit menu's Points (smooth), Points (flat) and Freehand. **Reset curve** in the left card goes back to flat.
- With no nodes the curve is flat at 100 %, which is today. The engine then skips it, so the output is bit-identical.
- **Decision 1: what a node holds.**
  - **A (R).** A multiplier on Depth, 0 to 200 %, drawn where it lands (a node at 50 % with Depth 6 dB sits at −3 dB). The Depth knob stays the master, and Tilt stacks on top, so nothing about the existing controls changes meaning.
  - **B.** The depth itself, in dB. Easier to read at a glance, but the Depth knob would then have to become an offset or hide in Resonance mode.
- **Decision 2: Learn.** Wave 1's Learn is for the Gate and Compressor curves only.
  - **A (R).** Not in Resonance mode: what to cut is the detector's job, and the nodes are a taste control.
  - **B.** A Learn that lowers the ceiling where the material is already dull (from the analyzer's tilt). It's guesswork, so it isn't recommended.

### 2. Engine and state

- A fourth curve, index 3, in `fir::Shared::curves` and the saved curves, in percent, read per bin as the engine reads the other curves (per frame, at the curve's version). An older FIR skips curve indices it doesn't know, so it loads these projects with a flat curve.
- Each bin's ceiling is `Depth × curve × tiltFactor`, applied where Depth is today (the clamp on each bin's reduction), so the detector, Sharpness and the 4 dB floor over the surrounding spectrum are unchanged.
- No new host parameters: like the other curves, the nodes are state, not automation (`resNodes` in the backlog's wording becomes curve 3).

### 3. Model

There's no new signal processing: a node curve changes one clamp per bin. Rather than a Python model, the curve maths is shared with the Gate and Compressor curves (`fir::evaluateCurve`, already tested), and the engine tests below check the clamp directly. If you'd still like a model first, as the Wave 2 rule says, it would be a copy of `docs/research/fir-resonance-model.py` with the ceiling made per bin.

### 4. Tests

1. Legacy: with no nodes, Resonance renders as v1.6.0 and as Wave 1 (Tilt 0 and Tilt ±100 %).
2. A node at 3 kHz at 200 %: a resonance there that today is cut by the full Depth is cut by up to twice Depth (still never below 4 dB over its surroundings); one at 700 Hz with flat nodes is unchanged.
3. A node at 0 % on a resonance: no cut there.
4. Nodes × Tilt multiply.
5. State: nodes survive save and load, undo and A/B; a project with nodes loads in v1.6.0's FIR (curve 3 ignored).

### 5. Mockup and build order

The graph's look changes in Resonance mode (a ceiling line with nodes), so a mockup comes first after this note: `reference/phase4/42__fir_w2_mockup.png`, from the Resonance screenshot, **approved by the user on 21 November 2026**. Then one push: engine, graph editing, state, tests, manual and checklist.

### 6. As built

One push.

- **Curve 3:** `fir::numCurves` is 4, with `depthCurve` = 3 in percent (flat 100, `clampCurveValue` holds it to 0 to 200; the dB curves keep -150 to +48). `fir::Shared` starts it at 100, so an engine built without the processor takes the old path. The engine marks the curve flat when every bin reads exactly 100 and then skips it; otherwise each bin's ceiling is Depth x Tilt's factor x the curve.
- **Graph:** Resonance edits curve 3 (`FirGraph::curve`), drawn where the ceiling lands: `yForValue` and `valueAt` convert between percent and the graph's dB with Depth and Tilt at that frequency. The ceiling is dashed in the mode colour between Low and High, with a light fill from 0 dB and the nodes on it. Editing is the curves' own: a click on the line adds a node, drag moves it, Alt-click deletes it and right-click types it (Depth in %), Ctrl-drag moves the whole curve; double-click puts a node back to 100 %. **Changed from the mockup's notes:** they said double-click adds and right-click deletes; the Gate and Compressor curves add on click, delete with Alt-click and type with right-click, and the depth curve works the same way.
- **Reset curve** sits in the Range card's top row, beside RANGE, in Resonance mode only.
- **Tests:** `tests/FirW2Tests.cpp`; FirW1Tests' legacy test covers the flat curve.

## Phase 4 Wave 2: ratio curve, Mid/Side, and EQ-mode dynamics

**Approved by the user on 23 November 2026: decisions 1 to 3 all A.** Items 27, 28 and 29 of the Wave 2 order (`docs/enhancements.md`; backlog items 4, 5 and 6 of section 8), for v1.7.1, in one note because 27 and 29 turn out to be the same feature. Modelled first: `docs/research/fir-w2-model.py`.

### 1. A ratio curve for Compressor mode (item 27)

Compressor mode has a threshold curve and one **Ratio** for every frequency. A second curve, **Ratio**, drawn like the threshold (from 1:1 to 20:1, and 0.5:1 for upward), sets the ratio at each frequency. A new project's ratio curve is flat at the Ratio knob's value, and moving the knob moves the whole curve. A saved project loads with its curve flat at its Ratio, so it renders as before, bit for bit.

What the model found: a mix whose 2.5–3.6 kHz gets 15 dB louder in the chorus and whose bass gets 6 dB louder, with the threshold curve 3 dB over the verse (as Learn places it):

| | 3 kHz, verse | 3 kHz, chorus | 100 Hz, verse | 100 Hz, chorus |
|---|---|---|---|---|
| One Ratio, 4:1 | 0 dB | 9.2 dB | 0 dB | 2.3 dB |
| Ratio curve: 4:1 from 2 to 5 kHz, 1:1 elsewhere | 0 dB | 9.2 dB | 0 dB | 0 dB |

The curve tames the harsh band only when it's loud and leaves the bass alone, which one ratio can't do.

### 2. EQ-mode dynamics (item 29)

The backlog asked for Attack and Release in EQ mode "when the curve is a threshold": a cut per frequency that appears only while that frequency is loud. With item 27 that is Compressor mode: the threshold curve says where, the ratio curve says how hard, and Attack and Release are already there. The model's run above is exactly that, a dynamic cut at 3 kHz in the chorus only. Building it again inside EQ mode would give two ways to do one thing (decision 1).

### 3. Mid/Side (item 28)

`msMode` (off by default): the spectral process runs on mid and side instead of left and right. Every mode keeps its curves, with a **Mid | Side** switch over the graph choosing which domain's curve you draw. Each domain has its own curve, Learn profile and gains, and the output is turned back into left and right. Off, FIR is as it was, bit for bit.

What the model found: Subtract on a voice in the centre with hiss mostly in the side (a wide room mic). In L/R, the side's hiss came down 4.0 dB, and the voice in the centre was changed by artefacts about 30 dB under itself. In Mid/Side with only the side processed, the side's hiss came down 4.6 dB and the centre wasn't touched at all. Processing only the side keeps the centre clean.

### 4. Decisions

- **Decision 1: item 29.**
  - **A (R).** Fold it into item 27: Compressor mode with the ratio curve is the per-frequency dynamic EQ. The manual will show it as "a dynamic EQ", with the model's example as a preset (**Dynamic de-harsh**).
  - **B.** Build Attack and Release into EQ mode too, with a threshold curve of its own: a second route to the same result.
- **Decision 2: the ratio curve's editing.**
  - **A (R).** A **Threshold | Ratio** switch over the graph in Compressor mode, choosing which curve the points edit. The other curve stays drawn, dimmed. A mockup follows approval.
  - **B.** Both curves always editable at once, the ratio's points on a second scale at the right.
- **Decision 3: Mid/Side's scope.**
  - **A (R).** Every mode (EQ, Gate, Compressor, Subtract, Resonance; Convolve L/R stays left and right by its nature), each with a curve per domain. A **Mid | Side** switch beside the mode row picks the domain shown.
  - **B.** EQ and Subtract only, the two where mid/side is most often wanted.

### 5. Tests

1. Legacy: projects saved with v1.7.0 render exactly as before in every mode: a flat ratio curve at Ratio's value, Mid/Side off.
2. The model's mix: with the ratio curve, the chorus's 3 kHz is reduced by 9 ± 1.5 dB and the 100 Hz by under 0.2 dB. With one Ratio of 4:1, the 100 Hz is reduced by over 1.5 dB.
3. Mid/Side: with only the side's curve changed, the mid passes bit for bit (within the FFT's own rounding, −120 dB) and the side is changed.
4. A mono input in Mid/Side gives the same output as L/R within −120 dB, with the side silent.
5. CPU: Mid/Side costs no more than L/R (the same two analyses), and the ratio curve adds under 5 %.

### 6. Build order

Approval, then the mockup (`reference/phase4/59__fir_w2_mockup.png`): the Threshold | Ratio switch and the Mid | Side switch. Then one push with the ratio curve, Mid/Side, the preset, tests, manual and checklist.
