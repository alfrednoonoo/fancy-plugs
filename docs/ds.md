# Fancy DS: design note

Spec: `docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md` §2. Mockup: `reference/phase3/17__ds_mockup.png`, source `design/phase3-mockup-src/ds.html`. Design rules: `docs/DESIGN_SYSTEM.md`. **No code until this note is approved.** The points marked **Decision** need your answer.

## Signal path

```
main ─┬──────────────────────────────────────────────► (lookahead delay) ─► split ─► × Output ─► out
      │                                                                       ▲
      └─► detector source (L+R, Mid, Side, L, R) ─┐                          gain
sidechain ─► detector source (L+R, L, R) ─────────┴─► band-pass or high-pass at Freq
                                                      ─► peak level, 1 ms hold (dB)
                                                      ─► minus full-band RMS, 50 ms (Relative threshold)
                                                      ─► gain computer (8:1, 6 dB knee, capped by Range)
                                                      ─► Attack / Release in dB ─► gain
```

- **Split** (default): only the sibilance band is turned down.
- **Wide:** the whole signal is turned down by the same gain.
- **Listen:** plays what's being removed, `input − output`, before the Output gain. It's temporary (brief §0.3, added 10 November 2026): not a parameter, never saved in the state or presets, off after a project or preset loads, not automatable, no undo step, unchanged by A/B, and off when the window closes. While it's on, the checkbox is filled green and the title strip shows a LISTENING tag. It's `FancyProcessor::setListening`, shown by `ListenCheckbox`, both in `fancy-core`, so Limit, Comp, Gate and FIR use the same code.
- Zero latency by default. Lookahead and the linear-phase split add latency, reported to the host.

## Shared code it reuses

Since Fancy DS-Mini (10 November 2026), DS's own engine is shared too: `fancy::dsp::DeEsser` in `fancy-core` (DS still calls it `DsDsp`), its history display (`fancy::ui::DeEsserHistory`) and its parameter definitions (`fancy/plugin/DeEsserParameters.h`). All moved unchanged; see `docs/ds-mini.md`.

- **Detector:** the shared detector filters (`fancy::dsp::Svf`) for the 2nd-order band-pass (Band) or high-pass (Shelf) at Freq. Width sets the band-pass's Q: an octave width `w` gives `Q = √2^w / (2^w − 1)`.
- **Gain computer and ballistics:** `fancy::dsp::computeReduction` with a fixed slope for 8:1 (0.875) and a 6 dB knee, then `Ballistics` and `EnvelopeFollower` from `fancy/dsp/Compression.h` for Attack and Release in dB. They're the same code Comp and MultiComp use.
- **Meters and controls:**
  - `ThresholdMeter` in Gate's style
  - `GainReductionMeter`, single bar, green (the `setBarColour` Limit added)
  - `OutMeter`, `Fader`, `Knob::tightStyle`, `ParamSegments`, `ParamDropdown` with sections, `ParamCheckbox`, `EditLink` and `EditDrawer`
- **Header:** `ShellEditor` with short labels, as Limit's.

### Decision 1: the split filters (I recommend A)

The spec asks for two things at once: reuse MultiComp's crossovers, and a split that's **bit-identical** to the input when nothing is reduced. MultiComp's minimum-phase crossovers can't do the second: Linkwitz-Riley bands only add back up to an all-pass version of the input, so there's always a small phase change.

- **A.** **Minimum phase** (default) by subtraction: `out = x + (g − 1) · band`, where `band` is the detection filter applied to the audio. When `g` is exactly 1, `(g − 1)` is exactly 0, so the output is the input, bit for bit. **As built (10 November 2026):** in Band, the bell widens as it cuts deeper, with its Q multiplied by the square root of the gain, so Width marks where the cut is half its depth in dB, as an EQ's bell does. With Width's Q alone, a -12 dB cut reached only about -3 dB at the band's edges, and the default preset cut the test's "s" bursts by 4.1 dB instead of 6. Only the coefficients that depend on Q are recomputed per sample; there's no `tan` per sample. Shelf keeps a plain high-pass. The linear-phase split's own test asks for 4 dB, not the spec's 6, because its steeper edges leave the test bursts' skirts uncut; that test isn't in the spec. It's the same filter as the detector, so what you see in the display is what's turned down. **Linear phase** (in EDIT) reuses MultiComp's `LinearCrossover`, moved to `fancy-core` as Comp's parts were. It makes three complementary linear-phase bands with edges at `Freq · 2^(±Width/2)`, or two at Freq in Shelf, and the gain goes on the sibilance band. The bands sum to the delayed input within about -140 dB, which meets the -120 dB null test. Its edges are steeper than the minimum-phase band, which the manual will say.
- **B.** Both modes through MultiComp's crossovers (Linkwitz-Riley for minimum phase). That's more reuse, but it fails the spec's bit-identity test at Range 0 dB.

Moving `LinearCrossover` out of MultiComp is a pure move. MultiComp's tests and stored renders must stay bit-identical, as when Comp's code moved.

### Decision 2: Gain, Output and Output trim (I recommend A)

The spec has three level controls: the right column's **Gain** fader, EDIT's **Output trim**, and the header's **Output** knob that every Fancy plugin has.

- **A.** One parameter, `output`, shown by the header knob and the Gain fader, as two views of the same control. EDIT has no Output trim. A de-esser only turns things down, so it doesn't need make-up and trim as separate stages.
- **B.** Three separate gains, as written. They multiply together, which makes the display confusing.

## Detector

- **Source:** Main L+R, Mid, Side, L or R; or Sidechain L+R, L or R. L+R and Link channels take the louder channel's level. With Link channels off, each channel has its own detector and its own gain (Main L+R and Sidechain L+R only; the single-channel and Mid/Side sources drive both channels).
- **Filter:** Band is a 2nd-order band-pass at Freq with Width (0.3–3 octaves). Shelf is a 2nd-order high-pass at Freq, and Width is locked with a hint.
- **Level:** the peak of the filtered signal in dB, held for 1 ms, then released at 20 dB/s, so short "s" transients are caught whole.
- **Relative threshold** (default on): the level is measured against the **full-band** level of the same source, as a 50 ms RMS in dB. The threshold then means "this many dB below the voice". The meter's read-out says "relative" under the value. Off, it's against 0 dBFS. The full-band level has a floor of -60 dBFS, so silence between phrases can't make breath noise look loud.

## Gain computer

- **The curve:** `reduction = min (computeReduction (level, Threshold, 0.875, 6 dB knee), −Range)`. That's 8:1 above Threshold, softened over 6 dB around it, and never more than Range.
- **Timing:** Attack (0.1–20 ms) and Release (5–500 ms) smooth the reduction in dB, with Comp's ballistics.
- **Lookahead** (EDIT: Off, 1, 2 or 5 ms) delays the audio but not the detector, so the gain is already moving when an "s" arrives. It adds that latency, with the shared latency note.

## Parameters

A new plugin, so `stateVersion` 1.

| ID | Name | Range | Default | Unit | Skew | Notes |
|---|---|---|---|---|---|---|
| `threshold` | Threshold | −60–0 | −28 | dB | linear | Relative or absolute |
| `freq` | Freq | 2000–16000 | 6200 | Hz | log | Blue knob; accepts `6.2k` |
| `width` | Width | 0.3–3 | 1.0 | oct | linear | Locked in Shelf |
| `shape` | Detection | Band, Shelf | Band | | | Grey segments |
| `range` | Range | −24–0 | −12 | dB | linear | Most reduction |
| `attack` | Attack | 0.1–20 | 0.5 | ms | log | |
| `release` | Release | 5–500 | 60 | ms | log | |
| `detector` | Detector | 8 choices | Main input L+R | | | Sections "Main input", "Sidechain" |
| `mode` | Mode | Split, Wide | Split | | | Accent segments |
| `relative` | Relative threshold | off/on | on | | | |
| `link` | Link channels | off/on | on | | | |
| `lookahead` | Lookahead | Off, 1, 2, 5 ms | Off | | | Not automatable; latency |
| `splitPhase` | Split filter | Minimum phase, Linear phase | Minimum phase | | | Not automatable; latency |
| `output` | Output | −24–+12 | 0 | dB | linear | Header knob and Gain fader (decision 2) |
| `bypass` | Bypass | off/on | off | | | |

Listen isn't a parameter (§0.3): see the signal path above.

Every knob and fader uses the shared components, so they all take typed values.

## Display

The display card is the plugin's own component: 510 × 160, at the mockup's place.

- **History** (top, about 3 s): the detector level as a grey fill, Threshold as a green line, and the reduction as a green fill from the top, with the mockup's caption. Click or drag on it to set Threshold, as on Gate.
- **Frequency ruler** (bottom strip, `#141414`, 1–20 kHz on a log scale): the detection band as a translucent green bar, with a blue handle at Freq. Drag the handle to set Freq. Drag the bar's edges to set Width. In Shelf, the bar runs from Freq to 20 kHz and only the handle drags. Each drag is one undo step.

## Layout

From the mockup at 100 %: **760 × 445** (53 px header + 392 px body), and 465 with the title strip.

- **Top row:** "Detector" and its dropdown on the left, "Mode" and the green Split | Wide segments on the right.
- **Left column:**
  - Threshold over the detector meter, with the scale at 0 to -60 in 12 dB steps and the value in green
  - the GR bar, green, beside it
- **Display card**, then two cards side by side:
  - **DETECTION**: a Band | Shelf switch in the title row, with Freq (blue) and Width
  - **REDUCTION**: Range, Attack and Release
- **Footer:** Relative threshold, Listen, Link channels, and ⚙ EDIT at the right.
- **Right column,** 100 px as in the mockup: Out (no scale, as on Delay and FIR) and the teal Gain fader.
- **EDIT drawer:** Lookahead and Split filter.

One small addition to `fancy-core`: `ThresholdMeter::setReadoutNote`, a second read-out line, so the meter says "-28.0 dB" with "relative" under it. "-28.0 dB rel" on one line didn't fit the 66 px column. Gate's and Limit's read-outs don't change.

## Latency

0 by default. Lookahead adds 1, 2 or 5 ms. The linear-phase split adds the crossover's latency (partition + half its length: 2559 samples, 53.3 ms, at 48 kHz). The dry path for bypass is delayed to match. Changes fade out and in, as on Limit.

## State format

The standard `FancyProcessor` XML.

## Factory presets

| Category | Preset | Settings |
|---|---|---|
| Vocals | Gentle vocal | Threshold −24, Range −6 |
| Vocals | Strong vocal | Threshold −32, Range −15, Release 40 ms |
| Vocals | Bright female vocal | Freq 7.5 kHz, Width 1.2 |
| Vocals | Deep male vocal | Freq 5 kHz, Width 1.0 |
| Vocals | Podcast voice | Threshold −26, Range −9, Lookahead 2 ms |
| Mix | Harsh cymbals | Split, Shelf at 8 kHz, Range −6, Release 120 ms |
| Mix | Bus taming | Wide, Shelf at 6 kHz, Threshold −30, Range −4 |

## CPU

Target under 1 % at 48 kHz/128, by the §0.4 ratio method. Minimum phase is two filters for the detector and two for the audio per channel, two levels and a gain curve: well under the target. The linear-phase split runs the shared partitioned convolver: MultiComp's figures suggest 1.5–2.5 %, which is measured separately and isn't the default.

## How each issue in §2.4 is handled

| Issue | Handled by | Test (Catch2) |
|---|---|---|
| A de-esser that "does nothing" | Default preset acts on a typical voice; Listen and the GR meter show it | A synthetic voice is built in the test: a 150 Hz sawtooth through three vowel formants, with "s" bursts of noise band-passed to 5–9 kHz at +12 dB. The default preset cuts the bursts by at least 6 dB, and the vowels by less than 0.5 dB |
| Dull voice or lisp from chesty low end | Split by default; Range caps the cut; Band or Shelf with Width | A 200 Hz + 3 kHz vowel, 6 dB over Threshold (absolute mode), triggers less than 1 dB |
| Fixed threshold misses quiet "s" sounds | Relative threshold | The burst signal at −30, −18 and −6 dBFS gets the same reduction within 1 dB |
| Split colours the sound when idle | Subtraction split; linear phase from complementary bands | Range 0 dB: output bit-identical to input in minimum phase; in linear phase, it nulls below −120 dB against the delayed input |
| Fast "s" onsets slip through | Lookahead | With 2 ms, the first 1 ms of a burst is already cut by at least 3 dB |

Also, as for every plugin:

- offline equals real time
- blocks from 1 to 8192 samples, at 44.1 to 384 kHz
- the automation bit-identity test
- the S, H and W checks
- pluginval, Steinberg's validator, the host harness and FancyBench
- every pop-up in `getPopupOpeners()`
- the CI screenshot compared with the mockup and with Delay

## Registering it

- Registered like Limit: `ds` in `FANCY_ALL_PLUGINS`, the workflow's choice, the screenshots, the shared tests and the host harness's latency list (Lookahead).
- Bundle ID `com.fancyplugs.ds`, product name "Fancy DS", plugin code `FDes`.
- Manual `docs/ds-manual.md`, checklist `docs/ds-checklist.md`, measurements `docs/measurements/ds.txt`.

## Phase 4 Wave 1: auto threshold, filter slope and output ceiling

Items from `docs/enhancements.md` (Wave 1 of the Phase 4 order). **Approved by the user on 21 November 2026 with every recommended option (R).** Every new parameter is appended, with a default that keeps saved projects bit-identical, checked against the v1.6.0 DSP in a legacy test as Comp's Wave 1 does. Items were checked in the code on 21 November 2026 before this note was written. Backlog items 1, 3 and 5 of section 11.

### 1. Auto threshold

A **Learn** button in the footer: over 2 s of playback, measures the sibilance band's level (Relative or absolute, as set) and sets Threshold so the loudest 10 % of it gets reduced.

- **Decision 1: the target.**
  - **A (R).** Threshold at the 90th percentile of the band level: only the peaks, the "s" sounds, are touched.
  - **B.** At the 75th: more de-essing, risks dulling the voice.
- An action, one undo step, with a progress ring; the history shows the learned threshold line.

### 2. Detection filter slope

`slope` 12, 24 or 48 dB/oct, default 12 (today's one SVF). 24 and 48 cascade two or four SVFs at the same frequency with Butterworth Qs, so the split sharpens without moving. In the drawer (Detection settings).

### 3. Output ceiling

`ceiling` Off or −12 to 0 dBTP, default Off: the shared `TruePeakLimiter` after the output gain, its latency reported. In the drawer.

### 4. Parameters

| ID | Name | Range | Default |
|---|---|---|---|
| `slope` | Filter slope | 12, 24, 48 dB/oct | 12 |
| `ceiling` | Ceiling | Off, −12 to 0 dBTP | Off |

### 5. Tests

1. Legacy: v1.6.0's DS against the new one in Band and Split, every detector mode: identical.
2. Learn: a vocal-like test (pink noise with 6 kHz bursts 12 dB up): Threshold lands within 2 dB of the bursts' 90th percentile.
3. Slope: at 24 dB/oct the detector filter is 12 dB further down one octave out than at 12 (± 1 dB); the frequency doesn't move.
4. Ceiling: a full-scale 0 dBFS square with Output +6 dB stays under the ceiling in true peak; latency reported matches.

Mockup: `reference/phase4/33__ds_w1_mockup.png` (source `design/phase4-mockup-src/w1_mockups.py`): the footer's Learn and the drawer's two settings. DS-Mini's item is in `docs/ds-mini.md`.

### 6. As built

One push (`365a133`), with every recommended option; `tests/DsW1Tests.cpp`.

- **Learn:** a button in the footer after Link channels; it listens for 2 s (the note beside it says so) and sets Threshold at the 90th percentile of the sibilance band's level. (This line said 3 s until 23 November 2026; the code has always used 2 s.)
- **Filter slope:** `slope`, 12, 24 or 48 dB/oct, in the drawer; the shared de-esser engine runs 1, 2 or 4 stages, 1 by default, which is the old detector.
- **Ceiling:** `ceiling`, Off (at the bottom) or -12 to 0 dBTP, in the drawer; DS was moved onto the shared `fancy::dsp::OutputCeiling` (Delay's commit, `f276ab4`), out of the path while Off.

## Phase 4 Wave 2: dual-band detection

**Approved by the user on 22 November 2026 with every recommended option (A).** Item 10 of the Wave 2 order (`docs/enhancements.md`; backlog item 2 of section 11). Then a mockup, approved before any code. Modelled first: `docs/research/ds-dual-model.py`.

### 1. Why

A voice has two kinds of sibilance: "s" (and "z", "t") high up, about 5 to 10 kHz, and "sh" (and "ch", "j") lower, about 2.5 to 5 kHz. Today's one detection band treats one of them well. Tuned to "s", it barely touches "sh". Widened to catch both, it also turns down the vowels' upper harmonics around 3 kHz, which dulls the voice between the sibilants. A second, independent band lets each kind of sibilance have its own frequency, threshold and depth.

### 2. What the model found

A test voice (syllables of a 150 Hz tone whose harmonics fall like a vowel's), with "s" bursts 10 dB over the vowels in 5.5 to 10 kHz and "sh" bursts 8 dB over them in 2.3 to 4.8 kHz. Each figure is how far that sound is cut in its own band. "Vowels" is the 2.5 to 4.5 kHz region over 1.7 s with no burst within 200 ms, which is the collateral dulling.

| Setting | "s" | "sh" | Vowels |
|---|---|---|---|
| A. One band at today's defaults (6.2 kHz, 1 octave, −28 dB, Range −12) | −5.0 dB | −2.9 dB | −0.4 dB |
| B. One band widened to catch both (4.6 kHz, 2.5 octaves) | −7.2 dB | −8.0 dB | −2.9 dB |
| C. Two bands: High as A, Low at 3.5 kHz, 1 octave, −20 dB, Range −9 | −5.6 dB | −6.6 dB | −1.0 dB |
| C, Low's Threshold −24 | −6.0 dB | −6.8 dB | −1.2 dB |
| C, Low's Threshold −16 | −5.2 dB | −5.9 dB | −0.7 dB |
| C, both bands detected from the input (decision 2) | −6.0 dB | −6.8 dB | −1.3 dB |

So two bands catch "sh" more than twice as well as today's band, for about a third of the dulling that the widened band causes. Low's defaults below are row C. Detecting Low from the High band's output, or from the input, makes no measurable difference.

### 3. What it adds

- **Dual band** (`dual`, off by default). Off, the second band isn't run, and saved projects render as before, bit for bit.
- **High** is today's band, unchanged: Freq, Width, Band or Shelf, Threshold and Range.
- **Low** is a second band-pass, with its own Freq (1 to 8 kHz, default 3.5 kHz), Width (0.3 to 3 octaves, default 1), Threshold (−60 to 0 dB, default −20) and Range (−24 to 0 dB, default −9). It's always a band, never a shelf.
- **Shared by both:** Attack, Release, Detector, Mode, Relative threshold, Link channels, Lookahead, Filter slope and Listen. Listen plays what both bands remove.
- **Split mode:** each band cuts only its own band, one after the other: `y = x + (gHigh − 1) · high(x)`, then `out = y + (gLow − 1) · low(y)`. With both gains exactly 1 the output is the input, bit for bit, as today.
- **Wide mode:** the whole signal is turned down by the deeper of the two reductions.
- **Fancy DS-Mini** uses the same engine with Dual band always off, so it doesn't change.

### 4. Decisions

- **Decision 1: where Low's controls go.**
  - **A (R).** Band tabs **High | Low** in the top row, between Detector and Mode, with a power switch on Low that turns Dual band on. The Threshold fader and meter, the GR bar, Freq, Width and Range show the selected band. Band | Shelf dims while Low is selected. The frequency ruler shows both bars, Low's in a second shade of DS's green, and each can be dragged. The history shows the selected band's level and threshold, with both reductions. A mockup follows approval.
  - **B.** Low in the EDIT drawer: an on switch and four small knobs (Freq, Width, Threshold, Range). The main window doesn't change, apart from Low's bar on the ruler, but Low's Threshold is then away from the meter.
- **Decision 2: what each band listens to.**
  - **A (R).** Both detect from the input, each independently; the cuts are applied one after the other. Each threshold then means the same as it would alone.
  - **B.** Low detects from High's output. The model shows no measurable difference, and the thresholds would interact.
- **Decision 3: Linear phase with two bands.**
  - **A (R).** The shared linear crossover makes five bands instead of three (it allows up to eight). Low's upper edge is held at or below High's lower edge, so the bands never overlap. The latency doesn't change.
  - **B.** Dual band works in Minimum phase only, and Linear phase is dimmed while it's on.
- **Decision 4: Learn with two bands.**
  - **A (R).** Learn sets both thresholds, each at the 90th percentile of its own band's level, in the same 3 s.
  - **B.** Learn sets only the selected band's threshold.

### 5. Parameters

Appended, with defaults that keep saved projects bit-identical:

| ID | Name | Range | Default | Unit, skew |
|---|---|---|---|---|
| `dual` | Dual band | off, on | off | toggle |
| `lowFreq` | Low freq | 1000 to 8000 | 3500 | Hz, log |
| `lowWidth` | Low width | 0.3 to 3 | 1.0 | oct, linear |
| `lowThreshold` | Low threshold | −60 to 0 | −20 | dB, linear |
| `lowRange` | Low range | −24 to 0 | −9 | dB, linear |

The selected tab isn't a parameter: it's the window's state, like EQ's selected band. One new factory preset, **Vocal, s and sh** (Vocals): Dual band on, at the defaults above.

### 6. Tests

1. Legacy: with Dual band off, DS renders exactly as the current release in every Mode, Detector, Filter slope and Split filter. DS-Mini's tests are unchanged.
2. Bit identity: Dual band on, both Ranges at 0 dB: the output is the input, bit for bit (Minimum phase), or the delayed input within −120 dB (Linear phase).
3. The model's signal: two bands cut "sh" by at least 5 dB and "s" by at least 4 dB, with the vowels cut by under 2 dB. One band at the defaults cuts "sh" by under 4 dB.
4. Independence: a 3.5 kHz burst alone moves only Low's reduction (High's stays under 1 dB), and a 7 kHz burst only High's.
5. Wide mode: the gain is the deeper of the two bands' reductions, within 0.1 dB.
6. Linear phase, two bands: five bands, Low's edges never above High's, and the same latency as with one band.
7. Learn: both thresholds land within 2 dB of each band's 90th percentile on Wave 1's Learn test signal, with "sh" bursts added.
8. CPU: Dual band on adds under 60 % to DS's CPU at its defaults (§0.4, median of five, ratio to Fancy Comp).

### 7. Build order

Approval of this note, then the mockup (`reference/phase4/52__ds_w2_mockup.png`), then one push: the engine's second band in `fancy::dsp::DeEsser`, the parameters, the tabs and the ruler's second bar, Learn for both bands, the preset, the tests, the manual and the checklist. Then the CPU measurement and the REAPER check.

### 8. As built

Mockup 52 approved by the user on 22 November 2026, with yellow (`accentYellow`, #F1C315) for the Low band instead of the mockup's light green (user). One push; `tests/DsW2Tests.cpp`.

- The engine (`fancy::dsp::DeEsser`, shared with DS-Mini) gains the Low band as a second detector on the same sources, with its own filters (Filter slope's stages too), peak hold, follower and bell split, cut after the High band. While Dual band is off it isn't run; after it's switched off it runs until its reduction has fully let go (under 1e-6 dB), then stops, so switching off doesn't click. The High path's arithmetic is untouched: test 1 compares against the v1.6.0 engine, including Linear phase and Shelf.
- Linear phase with two bands designs five bands (four with a High shelf); `dualEdgesFor` holds Low's upper edge 1.26 x under High's lower edge. Switching Dual band while in Linear phase fades out and in, as a change of shape does, because the crossover's band count changes; the latency doesn't.
- `HistoryColumn` gains Low's level and reduction (DS-Mini leaves them at their defaults); the history draws Low's reduction in yellow and, with Low selected, Low's level and threshold. The engine reports Low's level on its own meter tap and its reduction separately for the GR bar.
- The editor: `BandTabs` (High | Low, the power switch at Low's right end) between Detector and Mode; Low's Threshold meter, Freq, Width and Range in High's places while Low is selected (dimmed while Dual band is off); the ruler's Low bar in the lower half, its handle and edges draggable, a click on either bar selecting its tab. The screenshot preview shows Dual band on, as panel 1 of the mockup.
- One new factory preset, **Vocal, s and sh** (Vocals).
- Test 4 uses tones and absolute thresholds (-16 dB) so each band's filter skirt stays under the other's threshold; test 7 (Learn) is the editor's and is in the checklist.
- CPU (§0.4, median of five): run 45064371266 measured Dual band 61 % over DS at its defaults (target under 60 %); Low's displayed reduction now comes from its followers instead of two `log10` a sample, and run 45068210426 (commit `0dae4f9`) measured **58 %** (DS 0.480 %, Dual band 0.760 %, 1.67 x Comp). Green on Windows and Mac. The history's top edge keeps High's green where Low isn't reducing (fixed with the v1.7.0 push).
- REAPER check passed by the user on 22 November 2026.

## Phase 4 Wave 2: Mid/Side reduction and Hiss mode (and DS-Mini's Hiss profile)

**Approved by the user on 23 November 2026: decisions 1 to 3 all A.** Items 32, 33 and 34 of the Wave 2 order (`docs/enhancements.md`; backlog items 4 and 6 of section 11, and item 2 of section 12 for DS-Mini), for v1.7.1. Modelled first: `docs/research/ds-w2b-model.py`.

### 1. Mid/Side reduction (item 32)

The Detector can already listen to Main Mid or Main Side, but the cut always lands on both channels. **Reduce** (`reduceChan`): **Both** (today, the default), **Mid** or **Side**. With Mid, the input is split into mid and side, only the mid is cut, and the two are joined again, so wide material keeps its highs while a centre vocal is de-essed. Both is today's path, bit for bit.

What the model found: a centre vocal's "s" sounds with wide cymbals (Split, Threshold −30 dB, Range −12). Cutting both channels, the "s" came down 2.8 dB and the cymbals dipped 0.7 dB on every "s". Cutting the mid only, the "s" came down the same 2.8 dB and the cymbals didn't move. The effect is modest on this mix and larger with brighter, louder cymbals, where it's audible as the cymbals ducking with the singer.

### 2. Hiss mode (item 33)

A de-esser cuts when the highs are loud. Hiss is the opposite problem: you hear it when the music is quiet. **Hiss** is a third choice beside Band and Shelf in the Detection switch. It uses the shelf (everything above Freq) and turns it down when its level falls *below* Threshold, by up to Range at 1 dB per dB. Attack is how fast it opens and Release how fast it closes, so the controls keep their meaning. Pauses lose their hiss, and loud passages keep their highs. Band and Shelf are unchanged.

What the model found: a vocal with tape hiss at −54 dBFS, the shelf at 4 kHz, Threshold −38 dB (above the hiss's peaks, under the voice's highs), Range −12 dB, measured above 8 kHz:

| | Hiss in the pauses | The voice's highs in loud parts |
|---|---|---|
| **Hiss mode** (Release 60 ms, DS's default) | **−6.8 dB** | **−0.2 dB** |
| Hiss mode, Release 150 ms | −3.7 dB | −0.9 dB |
| A static −12 dB high shelf | −8.6 dB | −8.6 dB |

Hiss mode takes most of the hiss out of the gaps and leaves the singing bright, where a static shelf dulls everything. It's a quick fix for a hissy take. For steady noise under the music too, Fancy FIR's Subtract mode does more, which the manual will say.

### 3. DS-Mini's Hiss profile (item 34)

A seventh profile, **Hiss**, after DS-Mini's voice and instrument profiles: Hiss mode with the shelf at 5 kHz and Threshold placed by Amount (higher Amount, higher threshold and deeper Range), Release 60 ms. **Save as Fancy DS preset** carries it over as Hiss mode with the same settings, as the other profiles do.

### 4. Decisions

- **Decision 1: Mid/Side's control.**
  - **A (R).** A **Reduce: Both | Mid | Side** dropdown in the EDIT drawer, under Filter slope. The main window doesn't change.
  - **B.** Two small segments in the top row by Mode, which changes the main layout.
- **Decision 2: Hiss mode.**
  - **A (R).** A third segment, **Band | Shelf | Hiss**, in the DETECTION card. In Hiss, Width dims (as in Shelf) and the history shows the reduction growing in the gaps. A mockup follows approval.
  - **B.** Leave hiss to FIR's Subtract and drop items 33 and 34.
- **Decision 3: DS-Mini.**
  - **A (R).** Add the Hiss profile, as above.
  - **B.** No Hiss profile in DS-Mini, keeping its profiles to sibilance.

### 5. Tests

1. Legacy: Reduce Both, Band and Shelf render exactly as v1.7.0, and DS-Mini's existing profiles likewise.
2. Reduce Mid: with a side-only input, the output equals the input bit for bit while the mid is cut; Side likewise the other way round.
3. Hiss mode on the model's signal: the hiss in the pauses comes down by at least 5 dB, and the highs in loud parts by under 0.5 dB.
4. Hiss mode with Range 0: the output equals the input bit for bit.
5. DS-Mini Hiss: its saved DS preset loads in DS as Hiss mode and renders the same within −100 dB.

### 6. Build order

Approval, then the mockup (`reference/phase4/60__ds_w2b_mockup.png`): the three-way Detection switch and the drawer's Reduce row, and DS-Mini's profile list with Hiss. Then one push, with tests, manuals and checklists for both plugins. Mockup 60 approved by the user on 23 November 2026: Reduce under Filter slope, Ceiling a row lower.
