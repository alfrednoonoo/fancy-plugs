# Fancy DS-Mini: design note

Spec: `docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md` §6b, and §0.3 for Listen. Mockup: `reference/phase3/22__ds_mini_mockup.png`, source `design/phase3-mockup-src/dsmini.html`. Design rules: `docs/DESIGN_SYSTEM.md`. **Approved 10 November 2026: A for all four decisions.**

## What it is

A one-knob de-esser: pick a profile, turn **Amount**. It runs Fancy DS's engine, so DS-Mini at a setting sounds exactly like Fancy DS at the equivalent settings, bit for bit.

## Shared code

### Decision 1: the engine moves to `fancy-core` (I recommend A)

- **A.** `DsDsp` moves from `plugins/ds/` to `fancy-core` as `fancy::dsp::DeEsser`, unchanged, as `LinearCrossover` moved for DS and Comp's parts moved for MultiComp. Both plugins use it. Fancy DS's tests, screenshots and stored renders must stay bit-identical. The history part of DS's display (detector level, reduction from the top, the threshold line) moves too, as `fancy::ui::DeEsserHistory`, with a style for DS-Mini's dashed line. DS keeps its frequency ruler.
- **B.** DS-Mini links against DS's sources. That isn't "moved to fancy-core", which the brief asks for.

### Decision 2: switching profile (I recommend A)

The brief asks for a 50 ms crossfade of the detector settings, so switching never clicks. Inside the engine, Freq and Width already glide over 20 ms, but Band and Shelf are different filters: switching between them (Female voice to Sibilance, say) is a step in the detector and in the split.

- **A.** Two engines. On a profile change, the running engine keeps going with the old profile while a second one starts with the new profile, and the output crossfades from one to the other over 50 ms (equal gain, since both carry the same input). Then the old one stops. The rest of the time only one engine runs, so it's bit-identical to Fancy DS and costs the same CPU. A new engine starts with no gain reduction, so the crossfade lands on a de-esser that's still catching up for a few ms. With 0.5 ms attack that's inaudible.
- **B.** One engine with its settings smoothed over 50 ms. Bit-identical between changes too, but the Band–Shelf switch still needs its own crossfade inside the engine, which would change Fancy DS.

**As built:** the incoming engine doesn't start from silence. `DeEsser::continueFrom` copies the playing engine's detector levels, filters, gain and delay lines, then the new profile's settings are snapped on, so the relative threshold's 50 ms average is already full and nothing over-cuts during the fade. Fancy DS never calls it, so it's unchanged.

## Profiles and Amount

**Amount** (0–10) sets Threshold and Range together, as the brief says:

```
threshold = −4 × Amount dB (relative)     range = −1.8 × Amount dB
```

Amount 0 does nothing: Range 0 dB is bit-identical to the input in Fancy DS. Amount 5 is −20 dB and at most −9 dB; Amount 10 is −40 dB and at most −18 dB.

Every profile uses Relative threshold, the minimum-phase split, Main input L+R as the detector, and Mode, Lookahead and Link channels from EDIT. The brief's starting values:

| Profile | Detection | Attack / Release | Caption (from the settings) |
|---|---|---|---|
| Female voice (default) | Band, 7.0 kHz, **1.4 oct** (tuned; the brief's 1.0) | 0.5 / 60 ms | Sibilance · 4.3–11.4 kHz |
| Male voice | Band, 5.0 kHz, 1.0 oct | 0.5 / 60 ms | Sibilance · 3.5–7.1 kHz |
| Sibilance (any voice) | Shelf from 5.0 kHz | 0.5 / 60 ms | Sibilance · above 5 kHz |
| Harshness | Band, 3.5 kHz, 1.2 oct | 2 / 120 ms | Harshness · 2.3–5.3 kHz |
| Acoustic guitar | Band, 3.0 kHz, 1.0 oct | 1 / 80 ms | String squeak · 2.1–4.2 kHz |
| Bowed strings | Band, 4.0 kHz, 1.5 oct | 2 / 120 ms | Rosin · 2.4–6.7 kHz |

### Decision 3: the captions (I recommend A)

The brief's captions ("≈5.5–9 kHz" for Female voice) don't match its own widths. Width marks where the cut is half its depth, so 1.0 octave around 7 kHz runs from 4.9 to 9.9 kHz.

- **A.** Keep the brief's widths and show the true band, as above. The test vocal's "s" sounds sit at 5–9 kHz, so the wider band also helps the default reach the 6 dB the test asks for.
- **B.** Narrow the widths to match the brief's captions: Female voice 0.7 oct at 7.0 kHz (5.5–9 kHz), Male voice 0.7 oct at 5.1 kHz (4–6.5 kHz), Harshness 1.0 oct at 3.5 kHz (2.5–5 kHz).

**Tuning.** The brief says to tune these by ear and on the test signals. I can only tune on the test signals: if the default (Female voice, Amount 5) misses the 6 dB on the §2.4 test vocal, I'll change its Attack and Release first, then the Amount mapping's slopes, and record the final numbers here. Your listening in REAPER is the by-ear part.

**As tuned (10 November 2026).** With the brief's values, the default cut the test vocal's bursts by 5.58 dB (run 42913375230). Attack is already 0.5 ms, so Attack and Release couldn't add the missing 0.4 dB; the cut is limited by Range and by how much of the bursts' 5–9 kHz energy sits at the bell's edges. A plain-Python model of the engine reproduced CI's figures (6.67 against 6.64 dB for Fancy DS's default, 5.58 against 5.58 for DS-Mini's) and compared the options:

| Change | Bursts | Vowels |
|---|---|---|
| None (7.0 kHz, 1.0 oct, Range −1.8 × Amount) | −5.58 dB | −0.11 dB |
| Female voice at 6.5 kHz | −5.41 dB | −0.14 dB |
| Female voice 1.2 oct | −6.14 dB | −0.16 dB |
| Female voice 1.3 oct | −6.39 dB | −0.18 dB |
| **Female voice 1.4 oct** | **−6.61 dB** | **−0.21 dB** |
| Range −2.0 × Amount | −6.12 dB | −0.13 dB |
| Range −2.2 × Amount | −6.65 dB | −0.15 dB |

Female voice is now 1.4 octaves wide (4.3–11.4 kHz): it meets the test with a 0.6 dB margin, its vowels change as little as Fancy DS's default does, and the brief's Amount mapping, which sets every profile's strength, stays as written. The other profiles keep the brief's values.

## Rounding like Fancy DS

Fancy DS reads its settings through JUCE parameters, which snap Threshold and Range to 0.1 dB and pass every value through its normalised form in float. DS-Mini rounds its mapped settings the same way, through private copies of Fancy DS's parameters: their definitions moved to `fancy-core` (`fancy/plugin/DeEsserParameters.h`), and Fancy DS builds its own from the same functions. `fancy::deesser::roundAsParameter` reproduces what the value tree reads back after a host sets a value, with const calls only. That's what makes the equivalence and preset tests bit-identical rather than merely close.

## Parameters

A new plugin, so `stateVersion` as every Fancy plugin, from the start.

| ID | Name | Range | Default | Unit | Notes |
|---|---|---|---|---|---|
| `profile` | Profile | 6 choices | Female voice | | The pill with ‹ › |
| `amount` | Amount | 0.0–10.0 | 5.0 | | Large knob, step 0.1 |
| `mode` | Mode | Split, Wide | Split | | EDIT |
| `lookahead` | Lookahead | Off, 2 ms | Off | | EDIT, not automatable, latency note |
| `link` | Link channels | off/on | on | | EDIT |
| `output` | Output | −24 to +12 | 0 | dB | Header knob |
| `bypass` | Bypass | off/on | off | | |

**Listen** isn't a parameter. It's the temporary switch from §0.3 (`FancyProcessor::setListening` and `ListenCheckbox`), as on Fancy DS.

There's no sidechain bus, because DS-Mini has no detector choice. It works on mono and stereo buses.

## Layout

From the mockup: **392 × 472** at 100 % (53 px header and 419 px body), plus the 20 px title strip, the same size as Kicker. The header is the shared compact header (preset pill, +, Prm, A | B, output knob, ON); the mockup draws its own, so only the body is the target.

- **Left (276 px):**
  - the profile control
  - its caption in `textSecondary`
  - the history display, 238 × 92
  - Amount, a large blue knob
  - the footer: Listen and ⚙ EDIT
- **Right column (116 px):** GR (single bar, blue) and Out, as drawn.
- **EDIT drawer:** Mode, Lookahead, Link channels, and **Save as Fancy DS preset…**.

### Decision 4: the profile control (I recommend A)

The brief asks for a dropdown with ‹ › arrows, "styled like the preset pill". The shared preset pill (Phase 3 Q2.1) has its arrows inside the pill. The mockup draws them as separate square buttons outside it.

- **A.** A `ParamDropdown` variant with the arrows inside, like `PresetPill`, added to `fancy-core` and the gallery first, as the brief says. It matches the header's pill, and `fancy-core` sets styling. **As built:** `ParamDropdown::setStepArrows`, with the arrow shared with the preset pill as `PillArrow`. On the body it's filled with `bgSection` (`#262626`, as the mockup draws it), since the header pill's `#303030` would vanish on the body's own `#303030`.
- **B.** As drawn: a dropdown between two separate 26 px arrow buttons.

## Display

The `DeEsserHistory` from decision 1, at 238 × 92: the "s" level as a grey fill, the reduction in blue from the top, and a dashed blue line at the threshold. The caption reads "“s” level · reduction · where it starts". Unlike DS's, it doesn't set anything when clicked, because Amount is the only control.

## Save as Fancy DS preset

The EDIT drawer's action asks for a name in the shared dialog. It then writes a Fancy DS user preset to `%APPDATA%\Fancy Plugs\Fancy DS\Presets\From DS-Mini\<name>.xml`. The preset holds the exact settings of the current profile and Amount: Threshold, Freq, Width, Detection, Range, Attack, Release, Mode, Relative threshold on, Link channels, Lookahead (Off or 2 ms), Split filter minimum phase, Detector Main input L+R and Output. Fancy DS fills any parameter it adds later with its default, as every state does. The dialog's note says where the preset goes, and that it shows up in Fancy DS's browser under "From DS-Mini".

## Latency

0 by default; 2 ms with Lookahead, reported to the host, with the dry path delayed to match. Changes fade out and in, as on Fancy DS.

## Factory presets

| Category | Preset | Settings |
|---|---|---|
| | Default | Female voice, 5 |
| Vocals | Gentle female | Female voice, 3 |
| Vocals | Strong female | Female voice, 7 |
| Vocals | Gentle male | Male voice, 3 |
| Vocals | Strong male | Male voice, 7 |
| Vocals | Podcast | Sibilance, 5, Lookahead 2 ms |
| Vocals | Harsh vocal | Harshness, 4 |
| Instruments | Acoustic guitar | Acoustic guitar, 5 |
| Instruments | Strings | Bowed strings, 4 |

## Look

- **Accent:** blue `#3598DB` (`uiBlue`), the approved exception: DS-Mini has no filter knobs, so blue can't be mistaken for one. Recorded in `DESIGN_SYSTEM.md` §2.
- **Names:** product name "Fancy DS-Mini", title strip "FANCY DS-MINI", bundle ID `com.fancyplugs.dsmini`, plugin code `FDsm`, folder `plugins/dsmini/`.

## Tests (§6b.4, plus the suite's)

| Test | How |
|---|---|
| 1. Amount 0 | Output bit-identical to the input, in each profile |
| 2. Equivalence | Each profile at Amount 2, 5 and 8: DS-Mini's output equals Fancy DS's at the mapped settings, bit for bit, through both processors |
| 3. Default on the test vocal | Female voice, Amount 5, on DS's §2.4 synthetic voice: "s" bursts cut by at least 6 dB, vowels by less than 0.5 dB |
| 4. Profile switching | A full-level signal with the profile changed every 100 ms: no sample-to-sample step larger than 1.5 × the input's own largest step |
| 5. Save as Fancy DS preset | The preset loaded into a `DsProcessor` gives bit-identical output |
| 6. CPU | Within 10 % of Fancy DS, by the ratio method; the measured ratio goes in `docs/measurements/ds-mini.txt` |

Also, as for every plugin:

- offline equals real time, at any block size
- the automation bit-identity test
- the S, H and W checks
- §0.3's Listen tests in `tests/ListenTests.cpp`
- pluginval, Steinberg's validator, the host harness and FancyBench
- every pop-up in `getPopupOpeners()`
- the CI screenshot compared with the mockup and with Kicker, the other compact plugin

Manual `docs/ds-mini-manual.md`, checklist `docs/ds-mini-checklist.md`.
