# Phase 2.1, item D: dynamic EQ in Fancy EQ

Brief: §D of `docs/BUILD_PROMPT_PHASE2_1.md`. Mockup: panel 3 of `reference/phase2_1/14__phase2_1_controls.png`. Fancy EQ's own design is `docs/eq.md`.

## 1. What it does

Any band of type Band (bell), Low shelf, High shelf, Tilt or Notch can be made dynamic. Its gain then moves between its static Gain and Gain + Range, depending on how loud the signal is in that band's own frequency region. It adds no latency.

## 2. New parameters

Seven per band, appended after every existing parameter (the 16 bands' blocks, in band order), so no existing ID or index moves. `N` is 1 to 16.

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `bNdyn` | Band N Dynamic | off/on | off | | |
| `bNthresh` | Band N Threshold | -60 to 0 | -24 | dB | linear |
| `bNrange` | Band N Range | -30 to +30 | -6 | dB | linear |
| `bNattack` | Band N Attack | 0.1 to 200 | 5 | ms | log |
| `bNrelease` | Band N Release | 5 to 2000 | 80 | ms | log |
| `bNdynMode` | Band N Dynamic mode | Above, Below | Above | | |
| `bNdynSc` | Band N Sidechain | off/on | off | | |

All are automatable. Typed values work as on every other knob (`5`, `5 ms`, `0.2 s`).

**Bus:** Fancy EQ gains an optional stereo input called **Sidechain**, off until the host turns it on, mono or stereo, as Comp's. The I/O pill reads "4 in 2 out". `SidechainWatch` shows the usual warning when a band with Sidechain ticked hears nothing.

## 3. State and compatibility

- `stateVersion` 2 → 3. **Migration:** a state with no value for a parameter now gives it its default. Until now, a parameter missing from a loaded state kept whatever value the instance had; a fresh instance has its defaults anyway, so nothing that loads today changes, but a preset or project loaded into an edited instance now gets DYN off, as it was saved. This is in `FancyProcessor::applyState`, so it covers all nine plugins and the A/B slot.
- Every new parameter defaults to off or to a value that does nothing while DYN is off, so a v1.1.0 project loads and renders bit-identically (the §0.3 test, extended to EQ with states that have none of the new IDs).
- Adding the sidechain bus changes the plugin's bus list. It's off by default, and VST3 hosts treat an added optional input as unconnected.

## 4. DSP

### 4.1 Detector, one per dynamic band, at the host rate

- **Filter:** fed by the main input (pre-EQ) or, with Sidechain ticked and the bus active, the sidechain. Each channel goes through a matched 2nd-order band pass at the band's Freq and Q (bell and notch), a 12 dB/oct Butterworth low pass at Freq (low shelf), or a high pass at Freq (high shelf and tilt), from `eq::design`. Its coefficients are recomputed only when Freq, Q or type change.
- **Level:** the mean square of each channel over 5 ms (one-pole), the larger of the two (stereo-linked), in dB. A sine reads its RMS level, so a full-scale sine is -3 dB.
- **Gain computer:** how far the level is past Threshold, in dB, with a 6 dB soft knee (quadratic over ±3 dB). Above mode counts the level over Threshold; Below mode counts how far it's under. The change is Range × min(1, distance / 12 dB): full Range 12 dB past Threshold.
- **Attack and Release:** a one-pole on the gain change in dB. Attack applies while the change moves away from 0 dB, Release while it returns. Below Threshold minus 3 dB in Above mode, the change is exactly 0.0, so the band is exactly its static self (test 1).

### 4.2 Applying it

- The detector writes one gain change per host-rate sample into a buffer for the chunk. The band filter runs at the processing rate (host rate × oversampling), and every 16 samples it takes the gain change at the matching host-rate sample, adds it to the smoothed static Gain, redesigns the band's sections and interpolates the coefficients linearly over the next 16 samples. This is the existing parameter-smoothing path, so there's no zipper noise and nothing new to keep stable.
- A dynamic band whose change is exactly 0 and whose parameters have settled stops redesigning, as a static band does. While it moves, it redesigns every 16 samples.
- **Notch:** a notch has no gain, so a dynamic notch works as a dynamic cut (the user's choice, 8 November 2026): with DYN on it becomes a bell at the notch's Freq and Q whose gain is the change, held at 0 dB or below (a positive Range does nothing). It passes the signal while inactive and cuts as the detector rises. Switching DYN on a notch is a shape change, faded like changing type. Band pass, low pass, high pass and all pass have no DYN chip.
- **Linear phase:** a dynamic band can't be part of a linear-phase FIR. In Linear mode the DYN chip is disabled, with the tooltip "Dynamic bands need Minimum phase", detectors don't run, and dynamic bands use their static settings. The manual says so.
- **Display:** each band's current change is published in an atomic float (lock-free, one per band), read by the graph at 30 Hz.

### 4.3 Latency and CPU

- **Latency:** none added. The detector runs on the same samples, with no lookahead.
- **CPU:** a band with DYN off costs nothing new. A dynamic band adds its detector (one or two biquads per channel and a one-pole) and, while it moves, a redesign every 16 samples. The brief's target is 8 dynamic bands under 1.5 % in CI, measured by the §0.4 method. If the redesigns cost too much, a band skips one when its change has moved less than 0.01 dB since the last.

## 5. Controls and graph

**Band editor (panel 3):**

- A **DYN** chip under the type dropdown, in the band's colour when on. It's hidden for band pass, low pass, high pass and all pass, and disabled in Linear mode.
- With DYN on, the dynamic half appears to the right of Q, after a thin separator: **Threshold**, **Range** (lit from the centre), **Attack** and **Release** (small knobs), an **Above | Below** switch and a **Sidechain** checkbox, all in the band colour.
- **Compact card:** laid out as panel 3. The card widens from 437 to 600 px while the selected band is dynamic.
- **Normal layout:** not in the mockup; the user chose this on 8 November 2026. With DYN off, the Details panel is unchanged apart from the chip. With DYN on, Freq, Gain and Q shrink to the compact card's knob size so the dynamic half fits between Q and the band buttons, using the compact card's positions. Nothing else on the window moves.

**Graph:** the static curve and fill as now. A dynamic band adds its reach: the band's curve at Gain + Range, drawn as a lighter fill between the static and reach curves, with a dashed edge in the band colour. The white total curve includes each band's current change, so it moves inside the reach as the detector works. For the selected dynamic band the graph's top-left corner reads "reach -6 dB · now -3.6 dB".

## 6. Tests (Catch2)

1. **Below threshold:** DYN on, Above, a signal 20 dB under Threshold: the output is bit-identical to the same band with DYN off.
2. **Gain and timing:** a sine at the band centre, 12 dB over Threshold, Range -6 dB: the steady-state change is within 0.2 dB of -6 dB. With Attack at 50 ms, it reaches 63 % of that within 1.2 × Attack. At 5 ms, the 5 ms level window alone takes up about half of that time, so the timing check uses a longer Attack.
3. **Out of band:** a sine two octaves away, 12 dB over Threshold: the band moves less than 0.1 dB.
4. **Zipper noise:** a tone whose level sweeps up and down across Threshold. The output's THD+N, measured as everything outside ±1/3 octave of the tone, is within 3 dB of a reference render in which the same band is redesigned on every sample instead of every 16. The reference is a test-only setting of `EqDsp`.
5. **Below mode and Sidechain:** each repeats tests 1 and 2 in its own way. Below mode: a quiet sine moves the band and a loud one doesn't. Sidechain: the sidechain's sine moves the band while the main input is silent in that region, and the main input's sine doesn't move it.
6. **Shelves and notch:** a low shelf's detector reacts below Freq and not above it; a dynamic notch passes while inactive and cuts while active.
7. **Linear mode:** dynamic bands render exactly as their static settings.
8. **Compatibility:** a state with none of the new IDs (and `stateVersion` 2) loads with DYN off everywhere and renders bit-identically to a fresh load; a parameter missing from a state takes its default even in an edited instance.
9. **Host:** H's offline-render test, the sidechain test and the flood test include a dynamic EQ.
10. **CPU:** 8 dynamic bands under 1.5 % (§0.4 method), with the ratio to Comp recorded in `docs/measurements/`.
