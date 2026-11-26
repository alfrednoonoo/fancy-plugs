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

## Phase 4 Wave 1: band solo, auto gain, dynamic reach and analyzer freeze

Items from `docs/enhancements.md` (Wave 1 of the Phase 4 order). **Approved by the user on 21 November 2026 with every recommended option (R).** Every new parameter is appended, with a default that keeps saved projects bit-identical, checked against the v1.6.0 DSP in a legacy test as Comp's Wave 1 does. Items were checked in the code on 21 November 2026 before this note was written. Backlog items 1, 2, 3 and 5 of section 4.

### 1. Band solo

Hear only what one band works on, to find a resonance before cutting it. A temporary listen (§0.3): not a parameter, never saved, off when the window closes.

- **Decision 1: what a solo plays.**
  - **A (R).** The input through a band-pass at the band's Freq and Q (for shelves and cuts, a band-pass at the corner with Q 0.7), as Pro-Q does. Audible even for a 2 dB move.
  - **B.** What the band changes: the output with the band minus the output without it. Exact, but a 2 dB bell is nearly silent.
- **Decision 2: the gesture.**
  - **A (R).** Hold **Alt** while dragging a handle, or Alt-click and hold a band tab: the solo lasts while the mouse is down, so it can't be left on by accident.
  - **B.** Double-click a band tab to toggle the solo, and again to stop.
- Through `setListening` with a listen target per band (Comp's Wave 1 mechanism). In linear phase the solo is delayed to match the latency. The title strip shows LISTENING while it plays.

### 2. Auto gain

Keeps the output as loud as the input whatever the curve does, so A/B comparisons aren't fooled by level.

- **Decision 3: how the trim is worked out.**
  - **A (R).** From the static curve: the power average of the total response over 20 Hz to 20 kHz, weighted like pink noise, applied as an output trim smoothed over 200 ms. Deterministic: renders always match. Dynamic bands are counted at their static gain.
  - **B.** Measured: input and output loudness compared live, as Compare does. Follows dynamic bands, but pumps on transients and makes renders depend on where playback started.
- `autoGain`, off by default, a checkbox in the EDIT drawer's third column under Peak hold. The applied trim shows under the Gain fader's value ("auto −1.8").

### 3. Dynamic reach

Today a dynamic band moves its full Range over the 12 dB past Threshold: the slope depends on Range, so a classic ratio can't reproduce today's sound at any default.

- **Decision 4: the control.**
  - **A (R).** **Reach** per band, 1 to 48 dB, default 12: how far past Threshold the full Range is reached. 12 is today's curve, bit for bit; 3 is a hard, compressor-like move; 48 a gentle one.
  - **B.** A ratio (0.5:1 to ∞) with an extra "Auto" position at the bottom that keeps today's Range/12 slope. Familiar, but two models in one knob.
- 24 parameters `bNreach`, on the DYN half of the band editor: a small value field under Sidechain, in the band colour (the row has no room for a seventh knob). It drags and types like a knob's value.

### 4. Analyzer freeze and pre and post together

- **Pre + Post** appended to the Analyzer choice: post as now, pre as a dimmer line under it. Saved values keep their meaning.
- **Freeze:** a button at the graph's top right while the analyzer is on, and the F key. It holds the current spectrum, drawn dimmer, under the live one, so a change can be compared with the moment before. A view action, not saved.

### 5. Parameters

| ID | Name | Range | Default |
|---|---|---|---|
| `autoGain` | Auto gain | off/on | off |
| `bNreach` (24) | Band N Reach | 1 to 48 dB | 12 |

Plus "Pre + Post" at the end of `analyzer`.

### 6. Tests

1. Legacy: v1.6.0's EQ against the new one, linear and minimum phase, with 8 dynamic bands: identical.
2. Solo: a band at 1 kHz, Q 4: a 1 kHz tone passes within 0.5 dB, 4 kHz is 20 dB down; the solo ends when the editor closes.
3. Auto gain: a +6 dB high shelf at 2 kHz on pink noise: the output's RMS within 0.5 dB of the input's.
4. Reach: at Reach 12 the change matches today's formula exactly; at Reach 3 the full Range is reached 3 dB past Threshold (±0.2 dB).
5. Freeze and Pre + Post draw (screenshot `eq-freeze`).

Mockup: `reference/phase4/30__eq_w1_mockup.png` (source `design/phase4-mockup-src/w1_mockups.py`): the solo, Reach, the auto-trim read-out, Freeze, the drawer's Auto gain and Pre + Post.

### 7. As built

One push (`d7fd3e8`), with every recommended option. `tests/LegacyEqDsp.*` is EqDsp at v1.6.0; `tests/EqW1Tests.cpp` holds the tests.

- **Band solo:** a band-pass at the band's Freq and Q (Q 0.7 at a shelf's or cut's corner), as Decision 1 A. **Changed from Decision 2:** the gesture is **Ctrl-drag** (Cmd on a Mac) on a handle, not Alt, because Alt-drag already sets Q. It lasts while the mouse is down, as decided, and it's a temporary listen (listen target band + 1, only while the editor is open), never saved.
- **Auto gain:** `autoGain`, a checkbox in the settings row; the trim is the pink-weighted power average of the static curve (dynamic bands at their static gain), smoothed over 200 ms, and shows as "auto -2.3" at the graph's bottom right while on.
- **Reach:** `bNreach` per band, 1 to 48 dB (12 is the old curve, to the bit), typed in a value field in the dynamic section of the band editor. `fancy::dsp::matched::DynamicSpec` carries it.
- **Analyzer:** "Pre + Post" appended to `analyzer`, the pre spectrum dimmer under the post one; **Freeze** is a button at the graph's top right while the analyzer is on. **Changed from section 4:** no F key, as a key in a plugin window competes with the host's shortcuts.
- **Test fix (run 44942993322):** EqTests' pre-Q2.4 state test now counts the eight new Reach parameters of bands 17 to 24.

## Phase 4 Wave 2: brickwall cut slope

**Approved by the user on 22 November 2026 with every recommended option (A).** Item 15 of the Wave 2 order (`docs/enhancements.md`; backlog item 4 of section 4), for v1.7.1. Modelled first: `docs/research/eq-brickwall-model.py`.

### 1. What it adds

Low cut and High cut bands go to 48 dB/oct today: order 8, four sections. A **Brickwall** slope is for jobs where everything past the corner must go and nothing before it may be touched, such as rumble under a vocal, or the top octave before a lossy encoder.

- **96 dB/oct**, order 16, as eight matched Butterworth sections, appended to `bNslope`'s choices after 48 and named "Brickwall (96)" in the menus. Q works on it as on the other slopes, scaling the highest-Q section.
- **Linear phase:** the same magnitude. EQ already builds its FIR from the magnitude response, so there's no phase shift, and the ringing is symmetric and within the FIR's length.
- **Only cuts.** Bells and shelves don't have slopes.

### 2. What the model found

A high-pass at 100 Hz, minimum phase:

| Slope | ⅓ octave below | An octave below | ⅓ octave above | Rings for (to −60 dB) |
|---|---|---|---|---|
| 24 dB/oct | −8.7 dB | −24 dB | −0.64 dB | 9.7 ms |
| 48 dB/oct (today's steepest) | −16.2 dB | −48 dB | −0.11 dB | 15.0 ms |
| 72 dB/oct | −24.1 dB | −72 dB | −0.02 dB | 24.0 ms |
| **96 dB/oct** | **−32.1 dB** | **−96 dB** | **0.00 dB** | **29.4 ms** |

Brickwall cuts twice as hard just past the corner and leaves what's above it untouched. In minimum phase it rings about twice as long, so a kick's low end smears a little more. The ringing scales with the corner's period: at 30 Hz it lasts about 100 ms. The manual will say to use Linear phase when ringing matters.

### 3. Decisions

- **Decision 1: which slopes.**
  - **A (R).** One new choice, Brickwall (96 dB/oct).
  - **B.** 72 and 96 dB/oct: one more step, which the model shows is halfway in every respect.
- **Decision 2: automation of Slope.** Appending a choice changes the 0–1 position of every existing slope for a host. A project's saved Slope values load correctly, because they're stored as choices. But a host automation lane already drawn on a band's Slope would land one step off.
  - **A (R).** Accept it, as Gate's appended Detector inputs did. Slope is rarely automated, and the manual and release notes say so.
  - **B.** A separate `bNbrickwall` toggle per band (24 more parameters) that overrides Slope, so no existing value moves.

### 4. Tests

1. Legacy: every existing slope, in both phase modes, renders exactly as v1.7.0, and so does a project saved with v1.7.0.
2. Brickwall, minimum phase: the 100 Hz high-pass is −32 ± 1 dB a third of an octave below the corner, −3 ± 0.2 dB at the corner, and flat within 0.05 dB from a third of an octave above.
3. Linear phase: the same magnitude within 0.5 dB down to −60 dB, and the impulse response is symmetric.
4. Changing a band from 48 to Brickwall while it plays fades as any slope change does, with no click.
5. CPU: 24 Brickwall cuts at 48 kHz stay within §4.11's target for 24 bands. **Changed by the user on 23 November 2026 (option A):** each Brickwall cut is eight sections, so 24 can't meet the 1.5 % set for 24 one-section bands; Brickwall's target is under 0.25 % per cut, and the 24-cut figure is recorded but not held to 1.5 %. Every other slope keeps the 1.5 % target for 24 bands.

### 5. Build order

Approval, then one push, with no mockup: the slope shows up in the existing Slope menus and the drawer. That covers the sections, the appended choice, tests, the manual (with a note on automation) and the checklist.

### 6. As built

One push with item 16 (v1.7.1). Tests in `tests/EqW2Tests.cpp`, their numbers from `docs/research/eq-w2-prepush.py`.

- `fancy::dsp::matched`: `maxSections` 8, `slopeOrders` gains 16, and "Brickwall (96)" is appended to `bNslope`.
- **Shelves and Tilt:** they have slopes in this EQ (section 1 said they don't). A shelf or Tilt set to Brickwall stays at 48 dB/oct (`shelfMaxOrder`, in `design()`), and the band's right-click Slope menu leaves Brickwall out for them. The drawer's Slope dropdown, shared by every type, still lists it; the manual says what it does there. The drawer's Slope dropdown is 124 px wide (was 110) to fit the name.
- **Linear-phase test:** at a 1 kHz corner with the 8192 FIR. The model shows the Blackman-windowed FIR can't hold a 100 Hz Brickwall to 0.5 dB (6 dB of error at 8192, 1.7 dB at 16384), because the slope is narrower than the window's resolution there; at 1 kHz the error is 0.07 dB. The manual's advice for low cuts in Linear phase is the longest FIR.
- **CPU:** recorded by the `eq w2 CPU measurement` line (24 Brickwall cuts against 24 bells in the same run); figures in `docs/measurements/eq-w2.txt` (run 45167508920): 24 Brickwall cuts 5.03 %, 5.3 times 24 bells (0.94 %) in the same run, over test 5's 1.5 % target; each cut is eight sections. That is 0.21 % per cut, within the per-cut target of 0.25 % the user chose on 23 November 2026 (option A; a low and a high Brickwall cut cost about 0.4 %). Character Warm adds 0.20 % at Off and 0.79 % at 4x.

## Phase 4 Wave 2: Character output stage

**Approved by the user on 22 November 2026 with every recommended option (A).** Item 16 of the Wave 2 order (`docs/enhancements.md`; backlog item 7 of section 4), for v1.7.1. Modelled first: `docs/research/eq-character-model.py`.

### 1. What it adds

`character`: **Clean** (the default, today's EQ, bit for bit), **Subtle** or **Warm**, a saturation stage after the bands and before Output. It uses Sature's soft curve and ADAA (`fancy/dsp/Saturation.h`) as Comp's Character does, so no new shaper code. The stage computes `f(d·x) / d`, which has slope 1 at zero, so quiet material passes at unity and only peaks pick up harmonics.

| Character | Drive | Curve | THD at −18 / −12 / −6 dBFS | Level change at −6 dBFS |
|---|---|---|---|---|
| Clean | none (not run) | | 0 | 0 |
| Subtle | −9 dB | symmetric, so odd harmonics | 0.025 / 0.098 / 0.385 % | −0.12 dB (−0.10 at 4x) |
| Warm | −3 dB | asymmetric (negative limit 4), so the 2nd harmonic leads the 3rd by about 5 dB | 0.108 / 0.425 / 1.60 % | −0.22 dB (−0.21 at 4x) |

A 5 Hz DC blocker follows Warm, because asymmetry makes DC on loud signals. The stage runs at the oversampled rate when EQ's Oversampling is on, and at the base rate with ADAA alone when it's off.

### 2. What the model found

The drives were set from the model. At Comp's Warm settings the stage already gave 1.5 % THD at −12 dBFS, too much for an EQ's colour. At −9 and −3 dB of drive, Subtle stays at 0.1 % THD at −12 dBFS and Warm reaches about 1.6 % at −6, and both change the level by under 0.25 dB below −6 dBFS. Comp's asymmetry of 1.4 didn't make the 2nd harmonic lead. A negative limit of 4 does: H2 at −48.6 dB against H3 at −53.7 at −12 dBFS.

### 3. Decisions

- **Decision 1: fixed or variable.**
  - **A (R).** Three fixed choices, as the backlog has them: one menu, nothing to set.
  - **B.** The choice plus a Drive knob (−24 to +6 dB) for how hard it works.
- **Decision 2: where it goes.**
  - **A (R).** A **Character** dropdown in the EDIT drawer's Output settings, next to Auto gain. The header gets no badge. Only the drawer changes, so a mockup isn't needed beyond a drawer screenshot.
  - **B.** A small Clean | Subtle | Warm switch in the header, beside the EDIT link: visible at a glance, but it changes the header.
- **Decision 3: Auto gain with Character.**
  - **A (R).** Auto gain keeps its formula, from the bands' response, and ignores Character. The stage changes the level by under 0.25 dB below −6 dBFS.
  - **B.** Auto gain also measures the stage's level change on the signal.

### 4. Tests

1. Legacy: Clean renders exactly as v1.7.0, with the stage not run.
2. THD and level for a 1 kHz sine at −18, −12 and −6 dBFS within 10 % of the table above, at Oversampling Off and 4x.
3. Warm: H2 is above H3 at −12 dBFS, and the output's DC stays under −80 dBFS with a full-scale asymmetric input.
4. Aliasing: a 10 kHz sine at −6 dBFS through Warm with Oversampling 4x has no alias above −90 dBFS under 20 kHz.
5. Switching Character while it plays crossfades over 10 ms, with no click.

### 5. Build order

Approval, then one push: the stage in `EqDsp`, the parameter, the drawer row, tests, manual and checklist.

### 6. As built

One push with item 15 (v1.7.1). The THD and level figures in section 1's table are the pre-push model's (`docs/research/eq-w2-prepush.py`), with the first-order ADAA and the DC blocker the build runs; the first model's rounded figures were 0.03 / 0.10 / 0.39 % and 0.11 / 0.43 / 1.6 %.

- `character` (Clean, Subtle, Warm), appended. `EqDsp::runCharacter` runs after the bands: on the oversampled block when Oversampling is on, after the FIR in Linear phase, at the base rate otherwise. Clean and settled returns at once, so the output is v1.7.0's to the bit, including after a return to Clean.
- ADAA adds half a sample of delay while Subtle or Warm is on, which is not reported as latency (as Comp's Character).
- **Switching:** a 10 ms linear crossfade from the old character to the new; a further change waits for it to finish. The newly faded-in stage starts from rest.
- **Drawer:** a Character dropdown under Auto gain (third column, fifth row), with a tooltip. Screenshot `eq-brickwall` shows it set to Warm with a Brickwall high-pass selected.

## Phase 4 Wave 2: band list and groups

**Layout A approved by the user on 24 November 2026** (mockup 66, `reference/phase4/66__eq_band_list_mockup.png`, panels 1 and 2; B and C, a wider or taller window, drawn and not chosen). **Decisions 1 to 4 approved by the user, all A, 24 November 2026.** Item 46 of the Wave 2 order (`docs/enhancements.md`; backlog item 6 of section 4, ▲), the last item of v1.7.1. Checked in the code on 24 November 2026 before this note was written.

### 1. What it adds

- **A band list.** A **Bands | List** switch at the left end of the band-tab row (x 14, y 419, 112 x 20), empty today. **Bands** is today's band editor, unchanged. **List** puts a table in the same place (x 0 to 734, y 417 to 595): a header and one 22 px row per band in use, six rows showing, scrolling past six, up to 24. The window keeps its size; nothing on it moves or shrinks.
- **Groups.** Each band can join group **A**, **B**, **C** or **D**, or none (the default). Moving one band of a group moves the others the same way: Freq by the same ratio, Gain by the same number of dB, Q by the same ratio. A grouped band's handle and tab carry its letter in a small grey tag (15 x 14, `grey6`, white 9.5 px semi-bold).

The list's columns, from mockup 66's source (`design/phase4-mockup-src/w2b_mockups.py`, `EQ_COLS`):

| Column | x, width | What it shows | What clicking does |
|---|---|---|---|
| Band | 16, 30 | The band's colour dot and number | Selects the band (its handle and tab), as clicking a handle does |
| On | 50, 26 | The band's power, in its colour | Toggles `bNon` |
| Type | 84, 104 | The type, as a menu box | Opens the band's Type menu, with its Slope submenu, as the editor's dropdown does |
| Freq, Gain, Q | 196, 84; 288, 76; 372, 46 | The values, in the band's colour; Gain shows — for types without gain | Drag to change, click to type, double-click to reset: the shared value field (as Reach's) |
| Ch | 426, 46 | ST, L, R, M or S | Opens the Channel menu, as the editor's chip does |
| Dyn | 480, 64 | Range in dB while the band is dynamic, — otherwise | Opens the band in Bands with its DYN page showing |
| Group | 552, 62 | —, A, B, C or D, as a menu box | Opens the group menu |
| Solo | 622, 40 | An S button | Solos the band while the mouse is held, as Ctrl-drag on a handle does (temporary, §0.3) |
| Delete | 670, 30 | × | Deletes the band after the same confirm as the editor's × |

Double-clicking a row (outside its controls) opens that band in Bands. **+ Add band** under the last row adds a band as + does. The selected band's row is lighter (`#3A3A3A`) with a 3 px bar in its colour on the left.

### 2. How a group moves

Groups change only what the user does in the plugin's window: dragging a handle, turning a knob, the list's value fields, typed values, the mouse wheel on a handle, and the band menu's Set frequency and Set gain. Each move becomes the same move on every band of the group, as one undo step, with every changed parameter sent to the host inside its own gesture, so automation records each band.

- **Freq:** every member's frequency times the same ratio. **Gain:** the same dB added to every member that has gain; cuts, notches, band-passes and all-passes have none, so they follow Freq and Q only. **Q:** every member's Q times the same ratio.
- **At a limit:** the move stops where the first member reaches the end of its range, so the group keeps its shape.
- **Not linked:** Type, Slope, Channel, power, the dynamic settings, Reset band and Delete band stay per band.

### 3. Decisions

- **Decision 1: host automation of a grouped band.**
  - **A (R).** Automation and host edits move only the band they're written to; the group follows only edits made in the window. A host writing one band's Freq can't make others jump, there's no loop between linked parameters, and renders match playback.
  - **B.** Automation moves the whole group too, by watching every grouped parameter. Simpler to describe, but automation already written on two members would fight.
- **Decision 2: moving one grouped band on its own.** Every modifier on a handle is taken: Alt sets Q, Ctrl (Cmd) solos, Shift is fine control.
  - **A (R).** No override: to move one band alone, set its Group to — in the list or its right-click menu, then back. One click each way.
  - **B.** Ctrl+Shift-drag moves one band alone. Quicker, but a third modifier on the handle is easy to hit by mistake while fine-tuning a solo.
- **Decision 3: the list's order.**
  - **A (R).** By band number, as the tabs are, so a row doesn't jump while its band is dragged.
  - **B.** By frequency, low to high, re-sorted when a drag ends.
- **Decision 4: the switch's state.**
  - **A (R).** Saved with the project as a view setting, `bandView` (Bands or List; not automatable, as Show grid is), so a project reopens on the view it was left on.
  - **B.** Not saved: every window opens on Bands.
- **Compact mode** (no decision): Compact mode replaces the band strip with the floating card and has no tab row, so the switch hides with it and the card shows the selected band as today. Groups still work from the handles and the card. The right-click menu on a handle gets a **Group** submenu (—, A, B, C, D) after Channel, in every mode.

### 4. Parameters

| ID | Name | Range | Default |
|---|---|---|---|
| `bNgroup` (24) | Band N Group | —, A, B, C, D | — |
| `bandView` | Band view | Bands, List | Bands |

All appended, after `character`; none automatable. A group doesn't touch the audio path, so every saved project renders as before, to the bit, and no migration is needed.

### 5. Tests

1. Legacy: a v1.7.0 state loads with every group — and Bands showing, and renders as v1.7.0's EQ, to the bit (the audio path is unchanged; this checks state loading).
2. Group moves, on the link logic alone (a JUCE-free `EqGroups` with the rules of section 2): bands at 200 Hz and 2 kHz in group A, the first dragged to 400 Hz: the second goes to 4 kHz. Gain +3 dB on one: both gain 3 dB, and a high-pass in the group keeps its gain at none. Q by the same ratio. A move that would take a member past 20 kHz or past +24 dB stops where that member reaches its limit, with the shape kept.
3. Through the editor: dragging a grouped handle changes every member's parameters as one undo step; Undo restores all of them.
4. Automation: setting one grouped band's Freq from the host (parameter set without a gesture from the editor) moves only that band (Decision 1 A).
5. The list: rows for the bands in use only, in band order; selecting a row selects the band; the switch's state survives a save and reload (Decision 4 A). Screenshots `eq-list` (List, mockup 66 panel 1's values) and `eq` with the switch and the group tags (panel 2).
6. Host checks: the parameter count tests (`EqTests`' pre-Q2.4 state test, the host harness) count the 25 new parameters.

### 6. Build order

Approval of this note, then a short Python model of section 2's group arithmetic and limits (test 2's numbers), then one push: the parameters, `EqGroups`, the switch and the list in `EqEditor`, the group tags on `EqGraph`'s handles and the tabs, the menu's Group submenu, tests, manual (a "Band list and groups" heading with both screenshots) and checklist.

### 7. As built (24 November 2026, one push)
- **Green:** run 45514445844 (commit `81e8f1e`, Windows and Mac). The first run failed only on the list's viewport taking keyboard focus (`setWantsKeyboardFocus (false)`); the rows' delete cross was made 10 px after `eq-list` was compared with mockup 66. `eq-list` and `eq-groups` match mockup 66.

- **Parameters:** `bNgroup` (24; none, A to D) and `bandView` (Bands, List), appended after `character`, not automatable. `addBand` puts a new band in no group; Reset band keeps the group, as section 2 says.
- **Group logic:** `plugins/eq/src/EqGroups.h`, JUCE-free: `limitMove` (a ratio or an offset, limited by the first member to reach its range), `apply` and `moveFor`. **The link:** `EqEditor::GroupLink` listens to every band's Freq, Gain and Q. A gesture that starts on the message thread on a grouped band takes the group's values and opens a gesture on each member (for Gain, only members with gain); each change moves them from those values with each parameter's own range, and re-sets the edited band if a member reached its limit; the gesture's end closes them. Hosts never start gestures, so automation moves its own band only (Decision 1 A); undo makes the drag one step, as before. **Changed:** the graph's mouse wheel on Q now wraps its change in a gesture rather than a UserEdit scope, so a group follows it; undo still makes it one step.
- **The list:** `EqEditor::BandList`, a header and a `juce::Viewport` of rows, one made for each of the 24 bands at the start and shown while the band is in use, in band order (Decision 3 A), with **+ Add band** after the last. Columns and positions from mockup 66's source. The Type cell is the band editor's `TypeDropdown` in a new list style (the name in 12 px, no icon); Freq, Gain and Q are `ValueField`s with no label (a dash for Gain on types without it); Ch the shared `ChannelChip`; Group a `ParamDropdown`; S holds the band solo (`setListenTarget`, `setListening`, as Ctrl-drag); × the editor's delete with its confirm. The list replaces the band editor and the tabs while List is chosen and Compact mode is off; the switch (`ParamSegments` on `bandView`) sits at (14, 2) of the band strip and hides in Compact mode.
- **Tags:** a grouped band's handle carries its letter in a 15 x 14 grey tag 7 px right of and 17 px above its centre; its tab carries a 14 x 14 tag at the tab's right, with the number moved left. The handle's right-click menu has a **Group** submenu after Channel.
- **Screenshots:** `eq-list` (List, mockup 66 panel 1: bands 2 and 3 in group A, band 3 dynamic, band 1 the Brickwall high-pass from the shot before) and `eq-groups` (Bands with the tags, panel 2), rather than changing the main `eq` shot, which now just shows the switch.
- **Tests** (`tests/EqGroupsTests.cpp`, numbers from `docs/research/eq-groups-prepush.py`): a v1.7.0 state loads with no groups and on Bands; groups and List leave the output bit-identical; the arithmetic alone; through the editor, Freq, Gain and Q moves, a move stopped at 24 kHz, one undo step, and a high-pass that follows Freq but not Gain; automation without a gesture moves one band; the view and groups survive a reload, and an added band starts in no group. `EqTests`' pre-Q2.4 state test counts the eight new Group parameters of bands 17 to 24. **Fixed after run 45480208304:** the list's `juce::Viewport` wanted keyboard focus by default, which the host check "Editors leave the keyboard to the host" caught (its only failure of 763); it no longer does.

## v1.7.2: the band list in a large window

Decisions 1 to 3 A (user, 25 November 2026). Mockup 68 (`reference/phase4/68__eq_list_large_mockup.png`, source `design/phase4-mockup-src/w3_mockups.py`), layout A approved by the user, 25 November 2026. Today the list is always 178 px tall with its columns at the left, so a larger window shows no more rows.

### 1. Layout

All in 1x units, from `EqEditor::layoutEditors`. Only List in the normal view changes; Bands and Compact mode keep their layout at every size.

- **List height:** `max (178, min (44 + 22 x rows, height - 50 - 344))`, where `rows` is the bands in use, plus one for **+ Add band** while there are fewer than 24, and `height` is the body's. 344 px is the graph's height in the default window, so the list only takes height the window has beyond that. In the default window and smaller ones the list is 178 px, as today.
- **All 24 bands** (572 px) show from a window 1039 px tall, title strip included. With more rows than fit, the list scrolls, as today.
- **Columns:** `span = clamp (contentWidth, 734, 1100)`. Each column's x is today's x times `span / 734`, rounded, plus `(contentWidth - span) / 2` when the content is wider than `span`. Below 734 px see Decision 3. Every control keeps its size. The header text, the selected row's bar and **+ Add band** move with the columns.
- **The graph** ends at the list's top. MATCH's card floats over the graph's bottom, as today. The EDIT drawer starts at the list's top, so no part of the list shows above it; its contents stay where they are in it.
- The height follows the bands in use: adding, deleting, undo, a preset and automation of a band's use all lay the window out again, on the editor's existing timer.

### 2. Parameters, state, latency, CPU

None changes: no parameter, no state, no DSP. The renders are bit-identical by construction.

### 3. Tests (`tests/EqGroupsTests.cpp`)

1. Default window: list 178 px and graph 344 px with 5, 6, 12 and 24 bands.
2. Largest window with 12 bands: list 330 px, so the graph is 152 px shorter than in Bands.
3. A window 1019 px tall without its title strip and 24 bands: list 572 px, graph 344 px, nothing to scroll.
4. Columns: at the default width the Freq cell's x is 196; at 1732 px of content it is `round (196 x 1100 / 734) + 316` = 610.
5. Bands, then Compact mode, at the largest size: the graph's height is as it is today.

A new screenshot `eq-list-large` (the largest window, twelve bands, List) is compared with mockup 68.

### 4. Decisions

1. **While a drag or a typed value is in progress** the list doesn't change height (none of them adds or deletes a band), so nothing moves under the mouse. A: as described (recommended).
2. **EDIT drawer over a tall list.** A: it covers the whole list (recommended). B: it keeps its 228 px height and the list's top rows stay visible above it.
3. **Narrow windows** (found while writing this, from the code; v1.7.1 has it). The delete cross is at x 677 to 687, but the minimum window (724 px) has 656 px of content, so under about 755 px wide the cross is cut off; a band can still be deleted from its handle's menu or in Bands. A (recommended): below 734 px of content only the gaps after Q close up, in proportion, until at 656 px Dyn is at 470, Group 538, Solo 604 and the cross 634, clear of the 6 px scroll bar; no control shrinks. B: leave it. Test 6 (A): at the minimum width every row's controls are inside the list and none overlap.

### 5. As built

`EqEditor::listHeight`, `listSpan`, `listColumnX` and `wantedStripHeight`; the strip's height as laid out is `stripShown`, checked on the 30 Hz tick, never while a mouse button is down. Tests 1 to 6 in two cases of `tests/EqGroupsTests.cpp`. Screens `eq-list-large` and `eq-list-small`. The column arithmetic was checked for every content width from 656 to 1732 px before pushing: no control overlaps another, and the cross is clear of the scroll bar.

## Phase 4 Wave 3: spectral dynamics

**Approved by the user on 26 November 2026: A in every decision; decision 1's B is kept as the fallback (section 4).** Item 3 of Wave 3 (`docs/enhancements.md`; backlog item 8 of section 4), for v1.7.2. Modelled first: `docs/research/eq-spectral-model.py`.

### 1. What it adds

A dynamic band today moves its whole bell or shelf when the level in its region passes Threshold (`docs/phase2_1-D.md`). **Spectral** (`bNspectral`, off), a per-band switch beside DYN, does the same arithmetic per frequency inside the band's shape. Each FFT bin has its own level, the band's gain computer (Threshold, Range, Reach, the 6 dB knee, Above or Below), and its own Attack and Release. Its change is weighted by the band's shape: 1 at the centre, falling to 0 where the band's curve does. A resonance inside a wide band is then cut on its own, and the rest of the band is left alone. This is Pro-Q 4's spectral dynamics, built on FIR's STFT framing.

The band's static Gain stays in the IIR chain as today; only the dynamic part moves into the spectral stage. A band with Spectral off, or DYN off, is today's band. With no spectral band the stage doesn't exist, so the EQ has no latency and renders as v1.7.1 to the bit.

### 2. What the model found

Pink noise at −24 dBFS with a 2.5 kHz tone at −14 dBFS that sounds for 0.5 s in each second, as a ringing resonance would. The band: a bell at 2.5 kHz, Q 1 (about 1.2 to 5 kHz), DYN Above, Threshold −30 dB, Range −9 dB, Reach 12, Attack 5 ms, Release 80 ms.

| | Tone | Noise at 1.5 kHz while the tone sounds | Noise at 4 kHz | Noise alone | 90 % of the cut after | Latency |
|---|---|---|---|---|---|---|
| Today's dynamic band | −9.0 dB | −4.1 dB | −4.4 dB | −0.1 dB | 13 ms | 0 |
| **Spectral, 1024 points** | **−9.0 dB** | **0.0 dB** | **0.0 dB** | **0.0 dB** | **19 ms** | **1024 samples, 21.3 ms** |
| Spectral, 2048 points | −9.0 dB | 0.0 dB | 0.0 dB | 0.0 dB | 24 ms | 2048 samples, 42.7 ms |

- **Selective:** today's band takes 4 dB out of everything between 1.2 and 5 kHz whenever the resonance rings. The spectral band cuts the resonance by the full Range and leaves the noise round it untouched.
- **Exact at rest:** with Range 0 the output is the input delayed by the latency, to float rounding (−319 dBFS in double precision).
- **Pre-echo:** clicks over the held, cut tone spread backwards by at most one frame. The click's energy in the 10 ms before it is 29 dB (1024) and 32 dB (2048) under its energy after it: a short, quiet smear, as in FIR.
- **Threshold means the level at one frequency.** Each bin's level is measured over the Hann window's main lobe and calibrated so a sine reads its RMS, as the band's detector does. A tone at Threshold triggers both kinds of band alike. Broadband material spreads its level over many bins, so each bin sits far under Threshold, and the noise alone wasn't cut at all. That's the point of a spectral band: it reacts to peaks and resonances, not to a loud band in general (decision 3).

### 3. How it's built

- **The stage** sits before the IIR chain, at the host rate, ahead of any oversampling: one STFT of the input, 1024 points at 44.1 and 48 kHz (scaled with the rate, so 2048 at 88.2 and 96 kHz, and the bins stay about 47 Hz wide), Hann, 75 % overlap. Detection reads the same spectrum, or the sidechain's, analysed only while a spectral band uses it. Per bin, every spectral band's weighted change (in dB) is summed and applied once, then the frame is overlap-added.
- **Channels:** each band keeps its channel setting. Stereo, Left and Right bands act on the L and R spectra; Mid and Side bands on M and S, formed from them per bin.
- **The framing moves to `fancy-core`** (`fancy::dsp::StftFrames`): FIR's input and output rings, windows, overlap-add and staged FFTs, with FIR switched to it and held bit-identical by its tests and a legacy copy of `SpectralEngine` (decision 5).
- **Latency:** 1024 samples at 48 kHz, reported only while at least one band is spectral (decision 2). Turning the first spectral band on or off changes the reported latency and crossfades, as switching Minimum to Linear phase does today.
- **Linear phase:** as DYN, Spectral needs Minimum phase. In Linear mode spectral bands play their static settings.
- **Graph:** the white total curve includes each bin's current change, published per frame as 512 log-spaced values (FIR's curve mechanism), so a spectral band shows a narrow dip at the resonance where today's band shows the whole bell moving. The reach fill and the "reach −9 dB · now −3.6 dB" read-out stay; "now" is the deepest bin.
- **CPU:** one stereo STFT, two forward and two inverse 1024-point FFTs per 256-sample hop, plus each spectral band's gain computer over its bins. Target: four spectral bands under 1 % at 48 kHz and 128 samples (§0.4), against the 1.5 % for eight dynamic bands today.

### 4. Decisions

Chosen: A in all five (user, 26 November 2026). Only decision 1's B is a fallback: if 1024 points can't single out low resonances in REAPER, the drawer's FFT size setting is added. The other B options are not taken up.

- **Decision 1: the FFT size.**
  - **A (R).** 1024 points at 48 kHz (21 ms), scaled with the rate: about 47 Hz bins, fine enough to single out a resonance above 200 Hz, and the shorter wait.
  - **B.** A drawer setting, 1024, 2048 or 4096: finer bins for low resonances, at 43 or 85 ms.
- **Decision 2: when the latency is reported.**
  - **A (R).** Only while a band is spectral, so the EQ at its defaults and every saved project keep 0 latency. Switching costs a host latency change, as Linear phase does.
  - **B.** A drawer option, **Reserve spectral latency**, which keeps it reported all the time so switching a band never changes the host's compensation (useful when automating Spectral). Off by default, with A's behaviour otherwise.
- **Decision 3: what Threshold means for a spectral band.**
  - **A (R).** The level at one frequency, calibrated on a sine as today's detector is: the same number triggers a tone the same way, and broadband material rarely triggers. The manual says to set Threshold by the resonance's level.
  - **B.** Offset by the band's width, so broadband noise triggers a spectral band about where it triggers today's. Switching Spectral on then changes less, but resonances need a higher Threshold to stand out.
- **Decision 4: where the switch goes.**
  - **A (R).** A **SPECTRAL** chip beside the DYN chip in the band editor (both layouts), in the band colour when on, shown only while DYN is on and disabled in Linear mode with the tooltip "Spectral bands need Minimum phase". The band list's DYN column shows "S" for a spectral band. A mockup follows approval, measured on EQ's latest screenshots.
  - **B.** An **Above | Below | Spectral** row instead of the Above | Below switch. One control fewer, but Spectral is independent of the direction, so Below would lose it.
- **Decision 5: FIR's STFT code.**
  - **A (R).** Move the framing into `fancy-core` and use it in both, as the backlog planned: one tested piece, FIR held bit-identical.
  - **B.** A separate small STFT inside the EQ, leaving FIR untouched: less risk to FIR, two copies of the same thing.

### 5. Parameters

| ID | Name | Range | Default |
|---|---|---|---|
| `bNspectral` | Band N Spectral | off/on | off |

24 parameters, appended after `bNgroup` for bands 1 to 24. No `stateVersion` bump: a state without them loads Off.

### 6. Tests

1. Legacy: no spectral band renders exactly as v1.7.1, minimum and linear phase, with eight dynamic bands, with 0 latency reported.
2. Selective: the model's case. The spectral band cuts the tone by 9 ± 0.5 dB and moves the noise at 1.5 and 4 kHz by under 0.5 dB, while today's band on the same input moves it by over 3 dB.
3. Calibration: a sine at the band's centre, 12 dB over Threshold, Range −6: the change at the sine is −6 ± 0.5 dB, as today's band test 2.
4. Reconstruction: Range 0 gives the input delayed by the latency, within 1e-6.
5. Latency: the reported latency equals the measured impulse delay, at 44.1, 48 and 96 kHz; switching the first spectral band on and off changes it and crossfades with no step beyond the input's own.
6. Timing: 90 % of the cut within 30 ms of the tone's start at Attack 5 ms.
7. Channels and sidechain: a Mid spectral band cuts a centred tone and not the same tone in the side; with Sidechain, the sidechain's tone triggers the cut and the main input's doesn't.
8. FIR: every FIR test passes unchanged, and FIR renders bit-identically to the legacy `SpectralEngine` in every mode and size.
9. CPU (§0.4): four spectral bands, in `docs/measurements/eq-w3.txt`.

### 7. Build order

Approval, then the band editor mockup (`reference/phase4/71__eq_w3_mockup.png`), then two pushes: first the framing moved into `fancy-core` with FIR on it (test 8 green before anything uses it), then the spectral stage, the parameters, the chip, the graph, tests, manual and checklist.

### 8. As built

Mockup 71 approved by the user on 26 November 2026. Two commits, pushed together because Verb's CI run was still going (a push cancels the run in progress): `deaa67f` moves the framing, then the spectral stage. Tests in `tests/StftFramesTests.cpp` (test 8) and `tests/EqW3Tests.cpp`, their numbers run first through `docs/research/eq-spectral-prepush.py`, a port of the stage.

- **`fancy::dsp::StftFrames`** (`fancy-core/include/fancy/dsp/StftFrames.h`, header only): FIR's input and output rings, windows (square-root Hann both ways; the low-latency pair; none for Convolve), FFT, frame timing and overlap-add, moved as they were. The owner's frame work runs as stages through `advance()`, spread over `extra` samples as FIR's staged sizes need. FIR's `SpectralEngine` keeps everything else. A copy of v1.7.1's engine (`tests/LegacyFirW2Engine`) holds FIR bit-identical in nine configurations (every mode, 1024 to 16384 points, staged, low latency, Convolve, Mid/Side, mono, sidechain) at block sizes 1, 64 and 1000.
- **`eq::SpectralStage`** (`plugins/eq/src/EqSpectral.h`): 1024 points at 44.1 and 48 kHz, doubling with the rate, on `StftFrames` with square-root Hann both ways, not the model's plain Hann analysis window: that's the shared framing, and the level reads the same. Each bin's level is the three bins round it, calibrated in `prepare()` on a sine on a bin and halfway between, so a sine reads its RMS within 0.02 dB wherever it falls. The band's weight is its shape at -1 dB (a notch as a bell), normalised to 1 at its deepest and zero under 1e-4; only bins with weight run. Per bin: the gain computer of today's band (Threshold, Range, Reach, the 6 dB knee, Above or Below; a notch only cuts) times the weight, then Attack and Release per hop. Changes sum per bin in dB in the band's channel (L, R, both; Mid and Side formed per bin, then back) and apply once. A band switched off lets go at its Release before it stops running.
- **In `EqDsp`:** a new `Config::spectral` is on while any band is dynamic, spectral, of a type that allows it, in Minimum phase. The switch goes through the existing config change (fade out, the new latency, silence until the delays refill, fade in), as Linear phase does. The stage runs before oversampling and the bands. The other dynamic bands' detectors hear the input and sidechain delayed by the stage's latency, so they stay in time with the audio. A spectral band's filter keeps its static Gain; its detector doesn't run; the graph's "now" reads its deepest bin.
- **Parameters,** appended after `bandView`: `b1spectral` to `b24spectral`, off. Reset band clears it. No `stateVersion` bump.
- **Graph:** the stage publishes its summed change at 512 log-spaced points each frame (Left and Right averaged, plus Mid); while a band is spectral the graph adds it to the white total, redrawing in 0.05 dB steps, and the spectral band's own curve stays static.
- **Layout** (mockup 71): SPECTRAL is a `ParamTextToggle` in the band colour, 94 x 24, under ST and DYN (at (12, 94) from the dynamic layout's origin; (12, 92) in Compact mode, inside the card), shown while DYN is; in Linear phase it dims with the tooltip "Spectral bands need Minimum phase". The list's DYN cell draws a ringed S after the reach. Screenshots: `eq-spectral` and `eq-spectral-list`.
- **Model figures:** tone -8.98 dB, noise at 1.5 and 4 kHz -0.00 and +0.00 dB (today's band -4.1); a sine 12 dB over Threshold at Range -6: -5.97 dB; Range 0: 1.7e-16 in double precision; 90 % of the cut 17.3 ms after the tone. Tests 5 and 7 (latency, channels and sidechain) are structural and weren't modelled. The switching test allows 1.25 times the input's largest step: the 2 ms fade adds up to 1.18 times on a 440 Hz sine.
- **CPU** (`eq w3 cpu`, median of five, 48 kHz, 128 samples, stereo). Run 45819322589 (Mac only; Windows stopped at the tests): four dynamic bands 0.652 %, four spectral 0.791 %. Run 45826938789 (commit `3848b23`, green): Windows four dynamic 0.417 %, four spectral **1.257 % (target < 1, over)**; Mac 0.623 % and 0.732 % (met). The two real forward and two real inverse FFTs per hop come from `StftFrames`, which FIR also uses bit-identically, so packing both channels into one complex FFT (as Verb's Freeze does) needs an opt-in mode there. The user chose to optimise (26 November 2026): `StftFrames::analysePair` and `synthesisePair` put two channels through one complex FFT each way, with the scaling against the real transforms measured in `prepare()`; EQ uses them for the main and key pairs, and `exp` in place of `pow` for the per-bin gains. FIR still calls `analyse` and `synthesise`, so it stays bit-identical. A test holds the pair calls to the single ones (within 1e-5). Run 45844235109 (commit `d4b8aef`, Windows; GitHub dropped the Mac job before it started): four dynamic bands 0.379 %, four spectral **0.849 % (met)**. **Ratios to Comp** (Run 45863329197 (commit `1639b92`, green), the first with Fancy Comp at its defaults timed in the same test: Comp 0.449 % Mac, 0.500 % Windows): Mac four dynamic bands 0.648 % = 1.44 x Comp, four spectral 0.837 % = 1.86 x (met); Windows four dynamic 0.563 % = 1.13 x, four spectral **1.256 % = 2.51 x (over)**. Comp costs the same on both runners, so the spectral bands' larger share on Windows is JUCE's FFT against the Mac's vDSP, not the runner's speed; the 0.849 % of run 45844235109 was a fast Windows runner. As a ratio the target of < 1 % is about 2 x Comp. **Optimised again** (user, 26 November 2026): `fancy::dsp::ComplexFft`, a Stockham radix-4 FFT in SSE on Windows (JUCE's vDSP engine on the Mac, so the Mac's output is unchanged), for `StftFrames`' paired transforms, and the per-bin dB and gain by bit-level log and exp (to 3e-5 dB and 2e-6) in place of `log10` and `exp`. Run 45885824732 (commit `9ec6078`, green): a 1024-point inverse 2.64 us against JUCE's 12.80 (4096: 13.35 against 59.10); Windows four dynamic bands 0.685 % = 1.04 x Comp, four spectral **1.233 % = 1.88 x Comp** (Comp 0.656 %, a slow runner: under 2 x Comp, over the < 1 % as written); Mac 0.521 % and 0.620 % = 1.69 x (met). The spectral bands' own share on Windows fell from 1.39 to 0.84 x Comp.
- **Screenshots:** the first green run took `eq-spectral` in the List view and `eq-spectral-list` with `eq-list-large`'s twelve bands and band 2 still spectral. Each is now taken straight after the shot it builds on (`eq-dynamic`, `eq-list`), and those previews clear the spectral switches. Both then matched panels 2 and 5, except that panel 2's total dips round band 2 and the shot's was flat: the spectral total only moves with audio, so `EqGraph::showPreviewSpectral` draws the dip for the shot (-2 dB at 3.2 kHz, 0.2 octave standard deviation), as `showPreviewChange` does for dynamic bands.
