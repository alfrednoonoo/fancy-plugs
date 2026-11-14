# Fancy EQ: design note

## Signal path

```
                 ┌─ Minimum: [ up ─ band 1 ─ band 2 ─ … ─ band 16 ─ down ] ─┐
in ─ analyzer ───┤                                                          ├─ × Gain ─ × Output ─ bypass ─ analyzer ─ out
(pre)            └─ Linear:  partitioned FIR built from the same bands ─────┘  (post)
```

Everything is double precision except the oversamplers and the FIR's FFTs, which only exist in single precision in JUCE. Biquads are transposed direct form II (§6).

## Band filters (§5.1)

Each band is an analog prototype, split into first- and second-order sections and matched to the digital domain, so the shape doesn't depend on the sample rate and doesn't cramp near Nyquist. It adds no latency.

| Type | Prototype | Digital match |
|---|---|---|
| Band | `(s² + A/Q·w·s + w²) / (s² + w/(A·Q)·s + w²)`, `A = √gain` | Vicanek's matched peak. A cut is the inverse of the matching boost, so boosts and cuts mirror exactly |
| Low pass | Butterworth cascade | Matched poles; numerator matched at DC, at Freq (held between 0.6 and 0.7 × Nyquist) and at 0.9 × Nyquist. Vicanek's own low pass left 1.7 dB of error near 20 kHz at 44.1 kHz |
| High pass | Butterworth cascade | Vicanek's matched high pass |
| Band pass | `(w/Q·s) / (s² + w/Q·s + w²)` | Vicanek's matched band pass |
| Notch | `(s² + w²) / (s² + w/Q·s + w²)` | Matched poles with zeros exactly on the unit circle |
| All pass | `(s² - w/Q·s + w²) / (s² + w/Q·s + w²)` | Matched poles with the mirrored numerator |
| Low shelf, High shelf | Butterworth shelf (below) | Poles from the exponential map; numerator matched at DC, `w` and Nyquist |
| Tilt | High shelf scaled by `1/√gain` | As a shelf. Pivots at Freq, reaching -Gain/2 below and +Gain/2 above |

**Slopes:** 6, 12, 18, 24, 36 and 48 dB/oct are orders 1, 2, 3, 4, 6 and 8, which is up to four sections. Pass filters use one prototype per section with Butterworth Q values. Q is a separate control (§5.1): `Q / 0.7071` scales the section with the highest Q, so 0.707 is plain Butterworth and higher values add a resonant peak without changing the slope. At 6 dB/oct Q has no effect.

**Butterworth shelf of order N:** for gain `G` and `r = G^(1/2N)`, the poles are the Butterworth poles scaled by `r` and the zeros are scaled by `1/r`. The gain at Freq is exactly half the shelf gain in dB. Whichever of the two has the lower radius becomes the digital poles, and the result is inverted if needed, because mapping a pole above Nyquist isn't possible.

**Limits:** Freq is held to 0.48 × the processing rate. A 24 kHz band at 44.1 kHz therefore sits at 21.2 kHz.

Prototyped against the analog response before coding, ignoring anything more than 60 dB down. The largest errors below 20 kHz were 1.8 dB for a 20 kHz ±18 dB bell at 44.1 kHz and 1.2 dB for an 18 kHz ±18 dB shelf at 44.1 kHz, and under 0.1 dB in general at 96 kHz. An 18 kHz +6 dB bell at 44.1 kHz is the §5.1 test case. Cascades add their sections' errors, so a 48 dB/oct low pass reaches about 1 dB near 20 kHz at 44.1 and 48 kHz, where it's already far down the slope.

## Parameters

The 16 bands each have these, with `N` from 1 to 16:

| ID | Name | Range | Default | Unit | Skew | Source |
|---|---|---|---|---|---|---|
| `bNused` | Band N Used | off/on | on for 1–4 | | | §3.4 |
| `bNon` | Band N On | off/on | on | | | §3.4 |
| `bNtype` | Band N Type | Low shelf, High shelf, Band, Notch, Low pass, High pass, Band pass, All pass, Tilt | Low shelf, Band, Band, High shelf; Band from 5 up | | | §3.4 |
| `bNfreq` | Band N Freq | 20–24000 | 80, 500, 2000, 8000; 1000 from 5 up | Hz | log or linear, see below | chosen here |
| `bNgain` | Band N Gain | -24–+24 | 0.0 | dB | linear | matches the largest graph range |
| `bNq` | Band N Q | 0.1–30 | 0.707 on bands 1 and 4, 1.0 otherwise | | log | chosen here |
| `bNslope` | Band N Slope | 6, 12, 18, 24, 36, 48 dB/oct | 12 dB/oct | | | §3.4 |

Phase 2.1 D appends seven more per band, after every parameter below: `bNdyn`, `bNthresh`, `bNrange`, `bNattack`, `bNrelease`, `bNdynMode` and `bNdynSc`, with an optional stereo Sidechain input. Their design is in `docs/phase2_1-D.md`.

Global parameters:

| ID | Name | Range | Default | Unit | Automatable | Source |
|---|---|---|---|---|---|---|
| `gain` | Gain | -60–+12, bottom = -inf | 0.0 | dB | yes | range matches Comp's faders |
| `output` | Output | -24–+12 | 0.0 | dB | yes | as the other plugins |
| `bypass` | Bypass | off/on | off | | yes | §4.6 |
| `phase` | Phase | Minimum, Linear | Minimum | | yes | §5.1 |
| `firLength` | FIR length | 2048, 4096, 8192, 16384 | 8192 | samples at 48 kHz | yes | §5.1 |
| `oversampling` | Oversampling | Off, 2x, 4x | Off | | yes | §5.1 |
| `logFreq` | Log-scale automated freq. | off/on | on | | no | §3.4 |
| `showGrid` | Show grid | off/on | on | | no | §3.4 |
| `showPhase` | Show phase | off/on | off | | no | §3.4 |
| `compact` | Compact mode | off/on | off | | no | §3.4 |
| `range` | Graph range | Auto, ±6, ±12, ±18, ±24 dB | Auto | | no | §3.4 |
| `analyzer` | Analyzer | Off, Pre, Post | Post | | no | §3.4 |
| `anaFft` | Analyzer size | 4096, 8192, 16384 | 8192 | | no | §5.1 |
| `anaWindow` | Analyzer window | Hann, Blackman-Harris | Blackman-Harris | | no | §5.1 |
| `anaOverlap` | Analyzer overlap | 50 %, 75 % | 75 % | | no | §5.1 |
| `anaTilt` | Pink tilt | off/on | off | | no | §5.1 |
| `anaPeak` | Peak hold | off/on | off | | no | §5.1 |
| `anaSmooth` | Smoothing | 0–100 | 50 | % | no | §5.1 |

**Auto range** matches the two Figma frames: ±12 dB in the normal layout and ±18 dB in Compact mode.

**Log-scale automated freq.** changes how a Freq parameter's 0–1 host value maps to Hz, so it changes what an automation envelope's height means. Ticking it doesn't move any band. Freq's Hz value is kept, and the host is told the new 0–1 position.

## Smoothing and changes (§4.2)

Freq (in log Hz), Gain and Q are smoothed over 20 ms and sections are redesigned every 16 samples, with the coefficients interpolated linearly between. A settled band skips all of this.

Type, slope, band power and removing a band can't be smoothed, because the filter's shape jumps. Each band therefore has a dry/filtered mix. It fades to dry over 10 ms, switches, and fades back.

## Linear phase (§5.1)

A worker thread reads the parameters at up to 100 Hz and, when they've changed, multiplies every band's matched magnitude at each FFT bin, so both modes share one magnitude response. An inverse FFT gives the zero-phase impulse response, which is centred and Blackman-windowed.

The convolver is a uniformly partitioned overlap-save with 16 partitions, so the partition is `length / 16`. The FIR swaps at a partition boundary with a one-partition crossfade, and the audio thread only ever exchanges an atomic slot index. In an offline render the audio thread designs the FIR itself, so renders are repeatable.

The length is multiplied by 2, 4 or 8 at higher sample rates, so the frequency resolution doesn't change. Latency is `length / 2 + length / 16`:

| FIR length | Samples at 44.1–48 kHz | At 48 kHz |
|---|---|---|
| 2048 | 1152 | 24.0 ms |
| 4096 | 2304 | 48.0 ms |
| 8192 | 4608 | 96.0 ms |
| 16384 | 9216 | 192.0 ms |

Oversampling does nothing in Linear mode, because the FIR is designed directly from the matched magnitude.

## Latency (§4.4)

Minimum phase has no latency without oversampling, and the oversampler's reported latency otherwise. Phase, FIR length and Oversampling change the latency, so they use the Comp's approach: wait for 50 ms of stability, fade out, reconfigure, report the new latency, stay silent until the delays have refilled, fade in. Bypass crossfades against a dry path delayed by the same amount.

## Analyzer (§5.1)

The audio thread pushes the mono sum into a lock-free FIFO, only while an editor is open. A background thread owned by the editor runs the FFT. With zero padding to twice the window length, scalloping stays small, and amplitude is scaled by the window's sum, so a -12 dBFS sine reads -12 dB. The sum is `(L + R) / 2`, so a signal in one channel only reads 6 dB low.

The graph's corner shows "pre" or "post". Pink tilt adds 4.5 dB per octave about 1 kHz, and is off by default.

## State

The same format as the other plugins: XML with `stateVersion` 1, `uiScale` and `presetName`. The EQ adds `uiWidth`, `uiHeight` and `selectedBand`. A corrupted value falls back to its default. Since Phase 3 Q2.4 a project's state also has a `MATCH` child: Match's learned spectra, its card's settings and the bands it last wrote.

## §5.1 fixes

| Requirement | How it is met | Test |
|---|---|---|
| Cramping | Matched design, optional 2×/4× | `18 kHz bell matches the analog prototype` |
| Response changes with sample rate | Same | `responses agree across sample rates` |
| Linear phase | Worker-designed FIR, reported latency, length setting | `linear phase is symmetric and matches minimum phase` |
| Slope only by stacking | Slope selector, separate Q | `slopes are as labelled`, `Q adds resonance without changing the slope` |
| Analyzer disagrees | Calibrated amplitude, window and size options, pre/post, peak hold, smoothing, optional tilt | `analyzer reads a -12 dBFS sine correctly` |
| Notch replacing a bell | Separate types | `a deep bell stays a bell` |
| Automation clicks | Interpolated coefficients and FIR crossfades | `automation sweeps are click-free` |

## Layout

The Figma frames are 802 × 625 without the host title bar. The graph is 734 wide; the Details panel is 178 high and the Options row 50, with the Gain fader in a 68 px column. Compact mode hides Details and floats a 437 × 122 card over the graph.

**Resizing:** §2.4 asks for a resizable window with a 480 × 320 minimum on a 560 × 380 base. Figma is about 1.45× that scale, so the minimum here is 696 × 464, which also keeps the four footer checkboxes on one row. The host's window border does the resizing, so nothing new appears in the design. The graph takes all spare space.

**Axes:** Figma's frequency axis isn't a pure log scale. Its gridlines fit `log2(f + 40)` to within a pixel from 20 Hz to 24 kHz, so the build uses that formula and its gridlines match. The right axis is the analyzer's scale, as Figma draws it, with 0 dBFS on the +12 line and 5 analyzer dB to each EQ dB. §3.4 calls it the gain fader's scale, but Figma takes priority.

**Musical note:** §3.4's text says it goes on the frequency display, but its own example and both mockups put it after the gain. The build follows the mockups: it's the note nearest Freq, in white, after the Gain value.

**EDIT link:** sits in the header's empty space, left of the Output knob, so both layouts get it without moving anything. The drawer holds Phase, FIR length, Oversampling, the selected band's Slope, Graph range and the analyzer settings. Slope is also on the band's right-click menu.

**Knob arcs:** Gain lights from the centre as §2.3 says. Figma draws it from the minimum.

**Compact card:** Figma's stored positions for the knobs disagree with its own render, which spaces them evenly. The build follows the render.

**Q:** Figma's bell at "2.0" is wider than a true Q of 2, because the original control was bandwidth in octaves. The build keeps Q, as §2.5 labels it, so the curve in the comparison render is narrower.

**Band colours:** Figma's EQ Graph uses #E74D3D, #9A59B5, #3598DB and #1BBC9C, and these replace the §2.1 band colours. Bands 5 and up continue with green, yellow, orange, and then repeat.

## Phase 3 Q2.4: channel per band, 24 bands and Match

Brief: Q2.4 of `docs/BUILD_PROMPT_PHASE3.md`. **Approved by the user on 13 November 2026**, with every recommended option **(R)**: the Channel chip by DYN, the `MATCH` header link, and full Linear-phase support. The alternatives are kept below for the record.

### 1. Parameters and state

Appended after every existing parameter, so no ID or index moves:

1. Bands 17-24, each with the seven base parameters and then the seven dynamic ones, exactly as bands 1-16 (`b17used` … `b24dynSc`). `bNused` defaults to off.
2. `bNchan` for N = 1 to 24: **Band N Channel**: Stereo, Left, Right, Mid, Side. Default Stereo. Automatable.
3. No Match parameters. Match is an editor action whose result is ordinary band parameters (§4).

`FancyProcessor::stateVersion` goes from 3 to 4. It's shared, so every plugin writes 4; nothing else changes for the others. Migration for EQ: an older state lacks every new ID, and since Phase 2.1 D a missing parameter takes its default (bands 17-24 unused, every band Stereo), so v1.1.0, v1.2.0 and v1.3.0 projects load and render bit-identically. New fixtures `tests/data/eq-v1.3.0-*.xml` join the older ones.

`eq::maxBands` goes from 16 to 24. The band tabs already show only the bands in use and shrink to fit: 24 tabs are 29 px each at the default size and 25 px at the minimum, which still fits "24". Band colours keep repeating after band 4. Double-click on the graph and `+` add bands until 24 are in use.

### 2. Channel per band: DSP

- **Domains (R).** Stereo, Left and Right bands run on L/R; Mid and Side bands run on M = (L+R)/2 and S = (L-R)/2. Bands run in band order, as now. Before a Mid or Side band the signal converts to M/S if it isn't already there, and before a Stereo, Left or Right band it converts back; the output is always L/R. In double precision from float input both conversions are exact, and with no Mid or Side band nothing converts, so every existing state stays bit-identical.
- **One channel.** A Left band filters L only and passes R untouched. Right, Mid and Side likewise.
- **Changing Channel** is a shape change: the band fades to dry over 10 ms, switches, clears its filter state and fades back, as changing type does. It's click-free and can be automated.
- **Mono bus (R):** every band acts on the single channel, whatever its setting, and the Channel chip is greyed with the tooltip "Channel needs a stereo bus". The alternative, Right and Side doing nothing on mono, would silently drop bands.
- **Dynamic bands:** the detector reads the band's own channel, from the main input or the sidechain: Left reads L, Mid reads (L+R)/2, and so on. Stereo keeps the larger of L and R. A mono sidechain feeds every channel.
- **Oversampling:** the conversions sit inside the oversampled section with the bands; nothing else changes.
- **Linear phase (R):** each band's matched magnitude is real, so the whole EQ at each FFT bin is a real 2 × 2 matrix from (L, R) in to (L, R) out: Stereo is `H·I`, Left `diag(H, 1)`, Mid `½[[H+1, H-1], [H-1, H+1]]`, and so on, multiplied in band order. The worker designs one FIR per distinct non-zero entry. With only Stereo bands that's today's single FIR. With L/R-only or M/S-only bands it's two (L and R, or M and S with the conversions around them). Only when L/R-specific and M/S bands are mixed does it need four, which roughly doubles the convolution cost (about 1.1 % at 8192, against 0.53 % today). **Alternative:** Linear mode ignores Channel, as it ignores DYN. Simpler, but it makes M/S unusable for mastering, where linear phase is common.
- **CPU:** Stereo bands cost what they do today; a one-channel band costs about half. A domain change is two adds and two multiplies per sample pair.

### 3. Channel per band: controls and graph

This adds a visible control, so it needs the user's approval.

- **Channel chip (R):** a small dropdown chip under the type dropdown, left of the DYN chip, reading `ST`, `L`, `R`, `M` or `S`, using the shared dropdown and menus from Phase 2.1 S. It goes in both the Details panel and the compact card; nothing else moves. It's also on the band handle's right-click menu. **Alternative:** the right-click menu and the EDIT drawer only, as Slope is, with no visible layout change.
- **Band handles:** a band that isn't Stereo shows its letter (`L`, `R`, `M`, `S`) in 9 px text inside its handle.
- **Total curve (R):** with mixed channels there's no single total. The white curve is the response for the selected band's channel: Mid is what a signal with L = R hears, Side what L = -R hears, Left and Right what a signal in that channel alone hears. A selected Stereo band shows Mid, which is today's curve whenever every band is Stereo. The graph's top-right corner reads "total: Mid" (or Side, Left, Right) while any band isn't Stereo, and nothing otherwise.
- **Analyzer channel (R):** a new non-automatable setting in the EDIT drawer: Mid (today's `(L+R)/2`, default), Side, Left, Right. Without it, a Side band's effect can't be seen in the analyzer.

### 4. EQ Match

**What it does:** learns two long-term average spectra, the reference and the current material, and writes the difference into free bands as ordinary bells and shelves. The result is just bands: editable, automatable, saved in the project and undone in one step. Match adds nothing to the audio path.

- **Sources (R):** the reference comes from the **Sidechain** (both spectra learn at once while the song plays) or is **Captured**: play the reference section and press Learn reference, then play the material and press Learn current. A reference from an audio file would need file decoding the brief doesn't name, so it's left out unless the user asks.
- **Capture:** while learning, the audio thread pushes the main input (pre-EQ) and, for Sidechain, the sidechain into lock-free FIFOs, as the analyzer does, only while the Match panel is open. A worker thread runs 8192-point Blackman-Harris FFTs with 75 % overlap (scaled with the sample rate, like the FIR length) and averages the power. Frames under -60 dBFS are skipped, so silences don't pull the average down. The panel shows each spectrum growing, with the seconds learned; Match is enabled after 3 s of each.
- **Target:** reference minus current, in dB on 1/48-octave bins from 20 Hz to 20 kHz, smoothed by **Smoothing** (1/12 to 1 octave, default 1/3), offset to a 0 dB mean between 200 Hz and 5 kHz so a loudness difference doesn't become a broadband gain, scaled by **Amount** (0-100 %, default 100 %), held to ±12 dB, and faded to 0 outside **Range** (default 30 Hz to 18 kHz). The used bands' response is subtracted, so they stay, and the total EQ makes the current match the reference.
- **Fit (R):** greedy, then refined. A low and a high shelf are placed first if the target's ends need them. Then, until **Bands** (1-24, default 8, limited to the free ones) is reached or the error stops falling by 0.1 dB, a bell goes at the largest remaining error, with its Q from that error's width. Every few additions, Levenberg-Marquardt refines all fitted bands' Freq, Gain and Q together on the log-frequency grid, weighting 100 Hz to 10 kHz double. Gains are held to ±12 dB and Q to 0.3-8. It runs on a worker thread and is deterministic.
- **Writing the bands:** one undo transaction sets the free bands it uses, with Channel Stereo and DYN off. The bands from the last Match are remembered in the state (a property, not a parameter) and replaced by the next Match, so changing Amount or Smoothing and pressing Match again doesn't pile up bands. A Match band the user has edited since is kept as theirs.
- **Stored spectra:** the learned reference and current spectra are saved in the state (about 480 values each), so a reopened project can match again without relearning.
- **Panel (R):** a `MATCH` link in the header, left of `⚙ EDIT`, opens a card over the graph: Source (Sidechain | Captured), Learn reference, Learn current, Clear, small knobs for Smoothing, Amount, Bands, Range low and Range high, and the Match button. While it's open the graph draws the reference spectrum in purple, the current in grey and the target as a dashed white line. **Alternative:** a section of the EDIT drawer, with no new header control but a much taller drawer.

### 5. Tests (Catch2)

1. **Compatibility:** v1.1.0, v1.2.0 and v1.3.0 EQ states load with bands 17-24 unused and every band Stereo, and render bit-identically to their stored renders.
2. **24 bands:** all 24 in use match the analog product within the existing tolerances; a 25th add is refused.
3. **One channel:** a Left band leaves R bit-identical to the input, and Right leaves L.
4. **Mid/side:** with L = R input a Side band changes nothing (bit-identical) and a Mid band equals the same Stereo band; with L = -R, the reverse. A chain alternating Stereo, Mid, Left and Side bands matches an explicit-matrix reference to -120 dB.
5. **Channel switch:** changing Channel under signal is click-free (the existing click detector).
6. **Dynamic bands:** a dynamic Side band reacts to a Side-only sine and not to a Mid-only one at the same level.
7. **Linear phase:** with mixed channels, Linear mode's magnitude per path matches Minimum mode's within 0.1 dB from 50 Hz to 16 kHz; with only Stereo bands the render is bit-identical to today's.
8. **Mono:** on a mono bus every Channel setting gives the Stereo result.
9. **Match fit:** pink noise as current and pink noise through a known 6-band EQ as reference: 8 fitted bands come within 1 dB RMS from 50 Hz to 16 kHz at 1/3-octave smoothing, and two runs give identical bands.
10. **Match capture:** a reference with silent gaps learns the same spectrum as one without, within 0.2 dB; a pure level difference gives no bands.
11. **Match writing:** one undo step; existing bands untouched; a second Match replaces the first's bands; an edited Match band survives.
12. **Host:** H's offline-render and flood tests include 24 bands with mixed channels; the sidechain test includes Match's Sidechain source.
13. **CPU (§0.4):** 24 Stereo bells, 24 mixed-channel bells, and Linear 8192 with mixed channels, with ratios to Comp, in `docs/measurements/eq-q24.txt`. Targets: 24 bells under 1.5 %; Linear with mixed channels under 1.5 %.

### 6. Build order

One item in two pushes: first 24 bands and Channel, with their tests and screenshots, then Match. Both go to REAPER together with the updated manual and checklist, then the release. If the user approves a visible change (the Channel chip or the `MATCH` link), an HTML mockup of it in `design/phase3-mockup-src/eq-q24.html` comes first.

### 7. As built

What differs from sections 1 to 6, or was settled while building.

**Parameters.** As section 1, plus `anaChan` (Analyzer channel: Mid, Side, Left, Right; default Mid; not automatable) after the `bNchan` parameters. It's the setting section 3 describes, in the EDIT drawer's analyzer column.

**Linear phase and Channel.** `LinearPhase` picks one of four routings from the bands in use: one FIR (every band Stereo, which is the old code path, so those renders are unchanged), two on L and R, two on M and S, or four as a matrix. The sum and difference for M and S are taken on the spectra, so the input history stays in L and R and a design of any routing can crossfade into any other. The matrix costs two extra multiply-accumulates per bin and no extra FFTs. Measured with 24 bells at 8192: one FIR 0.72 %, M/S 0.67 %, four FIRs 0.89 %, against section 2's estimate of 1.1 %.

Linear and Minimum agree on the magnitude of each independent path: L and R with Left and Right bands, M and S with Mid and Side bands. With both kinds mixed they differ on the cross paths, and that's inherent: a Mid band alone takes a left-only signal to `(H + 1) / 2` in the left channel, where Minimum's `H` carries its phase and Linear's is the bare magnitude. Each is exactly a Mid band in its own phase mode. Test 7 is therefore three tests: L/R bands and M/S bands against the band responses, and a mixed chain against the matrix of the bands' magnitudes.

A mono bus designs every band as Stereo. `EqDsp::prepare` takes the bus's channel count, and `process` asks for a new design if the host sends a different count.

**Total curve.** With any band off Stereo, the graph multiplies a 2 x 2 complex matrix per pixel in band order, and the white curve is one path through it: Mid is `(T00 + T01 + T10 + T11) / 2`, Side `(T00 - T01 - T10 + T11) / 2`, Left `T00`, Right `T11`. A Left band therefore shows in the Mid total at about half its gain, which is what a centred signal hears. "total: Mid" sits left of the right axis's top label. Channel is also on the handle's right-click menu.

**Match: learning.** A frame counts only if each quarter of it (each hop) is above -60 dBFS, not just the whole frame. A frame that reaches into a gap would otherwise count at part level: with 2 s pieces that pulled the average down 0.36 dB, over the test's 0.2 dB. Each grid point is the mean power of the FFT bins within a 48th of an octave, or an interpolation between two bins below about 400 Hz, where a point is narrower than a bin. The two FIFOs (512 kB each) are made when the card first opens.

**Match: target.** As section 4, in this order: smooth the difference, take out its 200 Hz to 5 kHz mean, scale by Amount, hold to +/-12 dB, subtract what the bands that stay do, fade to 0 over a third of an octave outside Range. "The bands that stay" are the used, switched-on bands that the last Match didn't write. Since the current is learned as `(L + R) / 2`, a Stereo or Mid band counts in full, a Left or Right band as `(H + 1) / 2`, and a Side band not at all. Dynamic bands count at their static gain.

**Match: fit.** `docs/research/eq-match-model.py` is the same fit in Python; it was tried there first. Four things were added to section 4 after the model's first runs, which fitted the curve but with bands nobody would dial in (a -10 dB high shelf against a +7.6 dB bell at 20 kHz):

- A bell's Freq stays inside Range; a low shelf stays between Range's low end and 1 kHz, and a high shelf between 1 kHz and its high end.
- Every dB of band gain costs as much as 0.03 dB of RMS error, so two bands never grow against each other. It shrinks an honest gain by about 1 %.
- A shelf is placed only if the range's outer sixth of an octave holds at least 70 % of that end's level. A bell near the edge has fallen away by there.
- Errors outside Range count a quarter, between 100 Hz and 10 kHz double, and elsewhere once.

It refines after every added band, not every few, which costs little (a fit takes well under 100 ms) and makes the stopping rule exact. A band is kept if it lowers the weighted RMS error by 0.1 dB, or by a twentieth of what's left when that's already under 2 dB; with section 4's plain 0.1 dB, a subtle difference of half a dB got no bands at all. Nothing is added for an error under 0.25 dB, and a fitted band under 0.1 dB is dropped. With fewer than 3 bands allowed there are no shelves. On the model's six-band test case it uses 6 of the 8 bands and comes within 0.13 dB RMS.

**Match: writing and state.** `EqProcessor::applyMatch` is one `UserEdit`, named "Match". The last Match's bands are remembered as their parameters read back, so "edited since" is an exact comparison; a band also stops being Match's if its Channel or DYN changes or it's switched off. Undoing a Match leaves that memory alone: the bands it names are unused again, so nothing follows from it.

The spectra, the card's settings and that memory are saved through a new `FancyProcessor::addProjectData`, which only `getStateInformation` calls: they go with the project, and stay out of presets, undo steps and the A/B slots, where `addToState` would have put them. Loading a preset or undoing therefore keeps what's been learned. The card's six controls are the shared `Knob` and `ParamSegments` on parameters of their own that the host never sees, as Verb's module knobs are, so they have typed values and no automation.

**Match: card and link.** The card is 548 x 152 with the shared 50 px knobs, where the mockup drew 34 px ones in 500 px. In Compact mode it takes the band card's place. Opening EDIT closes it, and the reverse. The learning button reads "Learning..." with the reference's purple round it, since Inter has no filled-circle glyph. The fit runs on the Match worker; the card applies it on the next tick.

**The header had no room.** The mockup drew the header without undo, redo, A/B and Compare, so its "empty space" left of EDIT is 45 px at the default width and nothing at the old 696 px minimum. As built, the preset pill gives up to 60 px as before, `MATCH` is its icon alone below 781 px, and **the minimum width went from 696 to 724 px** so that icon always fits. This is the one change to the approved layout, which the user accepted on 14 November 2026; `minWidth` in `EqEditor.cpp` is the only line.

**Compatibility.** There are no `tests/data` fixtures: section 1 and test 1 assumed some. A test covers it instead: a state stripped of every Q2.4 ID and marked version 3, loaded into an instance whose new parameters have been moved, comes up with every band Stereo, bands 17 to 24 unused and the analyzer on Mid, and renders bit-identically to the instance that saved it. The v1.3.0 project check in REAPER is on the checklist.

**CPU (section 0.4)** is in `docs/measurements/eq-q24.txt`.
