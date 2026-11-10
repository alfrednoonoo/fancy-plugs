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
