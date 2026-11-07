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
| `listen` | Listen to removed part | off/on | off | | P4.2 |
| `showAnalysis` | Show analysis | off/on | on | | P4.2, not automatable |
| `graphTop` | Graph top | 0-+48 | +24 | dB | P4.1, not automatable |
| `graphFloor` | Analysis floor | -150 to -30 | -90 | dB | P4.2, not automatable |
| `output` | Output | -24-+12 | 0.0 | dB | Header knob, as on every plugin |
| `bypass` | Bypass | off/on | off | | §4.6 |

"ReaFir 0.x compatibility mode" is dropped, as P4.3 says.

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

§4.11 sets no target. Target here: under 3 % at 48 kHz/128 samples for 4096 points in each mode, which is two forward and two inverse FFTs per 1024 samples plus the mask. Measured with a CI benchmark, in `docs/measurements/fir.txt`.

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
