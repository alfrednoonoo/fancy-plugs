# Fancy Exciter: design note

## DSP

An original parallel high-frequency exciter:

```
in ──┬─ delay (matches oversampling latency) ───────────────────────────────┐
     │                                                                      + ─ × Output ─ bypass fade ─ out
     └─ high-pass ─ × Clip Boost ─ [ up ─ bias ─ ADAA shaper ─ down ] ─ high-pass ─ low-pass ─ × Harmonics × Mix Back ┘
```

- **High-pass filters:** 2nd-order Butterworth state-variable filters at Freq, before and after the shaper. The second removes the low intermodulation products the shaper creates, and it is also the DC blocker for the generated signal, since any bias from the even-harmonic setting is a DC offset. Because Freq is at least 500 Hz, a separate DC filter would do nothing extra.
- **Shaper:** a cubic soft clipper, `f(x) = x - 4x³/27` up to ±1.5 and ±1 beyond. It has unity gain for small signals, and its output never exceeds ±1, so high Clip Boost settings stay bounded.
- **ADAA:** first-order antiderivative anti-aliasing. The output is the difference of the antiderivative divided by the difference of its input, falling back to `f` at the midpoint when the two are almost equal. This adds half a sample of delay at the oversampled rate. At 4× that is 0.125 samples, and with oversampling off it is 0.5 samples, which isn't compensated.
- **Oversampling:** `juce::dsp::Oversampling` with equiripple FIR half-band stages and integer latency, so the wet path is linear-phase and the dry path can be delayed to match exactly. Stages use a -100 dB stopband first, then -90 dB and -80 dB. JUCE's preset stops at -75 dB, which doesn't leave enough margin for the §5.5 target.
- **Post low-pass:** a 2nd-order Butterworth at 0.45·fs, after the oversampling filters.
- **Even/odd character:** a bias added before the shaper makes it asymmetric, which creates even harmonics. The bias is `0.75 × (1 - Character)`, and `f(bias)` is subtracted so the shaper still outputs 0 for silence.

With Harmonics at 0 %, the wet path is multiplied by exactly 0, so the output equals the delayed input bit for bit.

## Parameters

| ID | Name | Range | Default | Unit | Skew | Source |
|---|---|---|---|---|---|---|
| `freq` | Freq | 500–16000 | 2000 | Hz | log | §3.2 |
| `clipBoost` | Clip Boost | 0–24 | 0.0 | dB | linear | §3.2 |
| `harmonics` | Harmonics | 0–100 | 0 | % | linear | §3.2 |
| `mixBack` | Mix Back | -60–+6, bottom = -inf | -6.0 | dB | linear | §3.2 |
| `oversampling` | Oversampling | Off, 2×, 4×, 8× | 4× | | | §3.2 |
| `character` | Character | 0–100 (even to odd) | 50 | % | linear | range chosen here |
| `output` | Output | -24–+12 | 0.0 | dB | linear | range chosen here |
| `bypass` | Bypass | off/on | off | | | §4.6 |

The header knob and the EDIT drawer's Output knob control the same `output` parameter, as on the Kicker.

## Smoothing

One-pole smoothers, per sample: Freq over 30 ms in the log domain, with filter coefficients recomputed every sample while it moves, and gains over 20 ms. Character is smoothed over 30 ms and ramped linearly across each internal block of up to 64 samples. Bypass fades over 10 ms.

## Latency

| Oversampling | Latency |
|---|---|
| Off | 0 samples |
| 2× | 55 samples |
| 4× | 69 samples |
| 8× | 74 samples |

Latency doesn't depend on the sample rate in samples. The dry path is delayed by the same amount, and bypass uses the delayed dry path.

Changing oversampling while playing fades the output out, swaps the filters, reports the new latency, waits for the delay line to refill and the filters to settle, then fades back in. The audio drops out for about 50 ms plus the new latency. All oversamplers are built in `prepareToPlay`, so nothing allocates on the audio thread.

## State

The same format as the Kicker: XML with `stateVersion` 1, `uiScale` and `presetName`, with the same validation.

## §5.5 fixes

| Requirement | How it is met | Test |
|---|---|---|
| Oversampling, default 4×, plus ADAA | Both, around the shaper | `aliasing is below -80 dBFS at 4x` |
| Post-shaper low-pass and DC blocker | 0.45·fs low-pass; the post high-pass blocks DC | `wet path blocks DC` |
| Even/odd character | Bias before the shaper | `character moves between even and odd harmonics` |
| Unity at Harmonics 0 % | Wet multiplied by 0, dry delay-matched | `harmonics at 0 is the delayed input exactly` |

## Layout

Base window is 392 × 432, the Figma Exciter frame without its 22 px host title bar. The meter uses that frame's own scale, which is shorter than the Kicker's.

The EDIT drawer holds Character, Output and an Oversampling dropdown.

## Phase 4 Wave 1: listen to added harmonics, THD read-out, ceiling and auto gain

Items from `docs/enhancements.md` (Wave 1 of the Phase 4 order). **Approved by the user on 21 November 2026 with every recommended option (R).** Every new parameter is appended, with a default that keeps saved projects bit-identical, checked against the v1.6.0 DSP in a legacy test as Comp's Wave 1 does. Items were checked in the code on 21 November 2026 before this note was written. Backlog items 1, 2 and 7 of section 2.

### 1. Listen to added harmonics

A temporary listen (§0.3), a checkbox in the accent above the footer (EDIT and the name): plays only what Exciter adds (the output minus the latency-aligned input, before Output), so Harmonics and Clip Boost can be set by ear.

### 2. THD read-out

Like Sature's harmonics card, but as one line under the meters: a background thread runs a private copy of Exciter's path (Freq, Clip Boost, Harmonics, Mix Back, Character, oversampling) on a test tone whenever one of those changes, and the read-out under the Out meter says "THD 2.4 %".

- **Decision 1: the test tone.**
  - **A (R).** One octave above Freq, at −12 dBFS: always inside the band Exciter works on, so the figure moves with every knob.
  - **B.** A fixed 1 kHz, as Sature: comparable between settings, but it reads 0 % whenever Freq is above 1 kHz.

### 3. Ceiling and auto gain

- `ceiling` Off or −12 to 0 dBTP (default Off), in the drawer: the shared `TruePeakLimiter`, so Clip Boost 24 dB can't pass 0 dBFS.
- `autoGain` (off), in the drawer: the same background thread measures what the path adds to pink noise at −18 dBFS RMS and trims the output by that much, smoothed over 200 ms. Deterministic.

### 4. Parameters

| ID | Name | Range | Default |
|---|---|---|---|
| `ceiling` | Ceiling | Off, −12 to 0 dBTP | Off |
| `autoGain` | Auto gain | off/on | off |

### 5. Tests

1. Legacy: v1.6.0's Exciter against the new one at every oversampling: identical.
2. Listen: the listen output plus the latency-aligned input equals the normal output within −100 dBFS.
3. THD: the read-out matches an offline measurement of the same settings within 10 % of its value.
4. Ceiling: Clip Boost 24 dB on a full-scale signal stays under the ceiling in true peak.
5. Auto gain: on pink noise, output RMS within 0.5 dB of input RMS at Harmonics 100 %.

Mockup: `reference/phase4/39__exciter_w1_mockup.png` (source `design/phase4-mockup-src/w1_mockups.py`): the footer's listen, the THD line, the drawer.

### 6. As built

One push (`ff59906`), with every recommended option; `tests/ExciterW1Tests.cpp`.

- **Listen to added harmonics:** a temporary listen (`ListenCheckbox`), never saved; it plays the output minus the dry signal.
- **THD read-out:** at the bottom of the body, for a tone one octave above Freq at -12 dBFS through the current settings, worked out on a background thread (synchronously in an offline render).
- **Ceiling:** `ceiling`, Off or -12 to 0 dBTP, in the drawer; **Auto gain:** `autoGain`, turns the output down by the level the harmonics add, from the same background analysis, smoothed.

## Phase 4 Wave 2: two bands, Punch, a high cut, harmonics width and minimum-phase oversampling

**Approved by the user on 23 November 2026: decisions 1 to 4 all A.** Items 40 to 44 of the Wave 2 order (`docs/enhancements.md`; backlog items 3, 4, 5, 6 and 8 of section 2), for v1.7.1. Kicker's item 45 shares the model and is in `docs/kicker.md`. Modelled first: `docs/research/exciter-kicker-w2-model.py`. Every new setting defaults to today's sound, bit for bit.

### 1. A second band (item 40)

Today the harmonics come from one band, everything above Freq. **Band 2** adds a second, lower one: its own **Freq 2** (100 Hz up to Freq) and **Harmonics 2** (0 to 100 %, default 0, when it isn't run), through the same shaper and Character, with the band running from Freq 2 to Freq. On a 150 Hz voice-like tone stack, today's band at 3 kHz adds almost nothing between 400 Hz and 3 kHz. A second band from 400 Hz fills that region with harmonics, the "warmth" or "body" two-band exciters are for, while the top band keeps the air.

### 2. Punch (item 41)

**Punch** (0 to 100 %, default 0): the harmonics' level follows a transient detector on the excited band, so attacks get more and sustains less. At 100 % the added part is raised up to 6 dB on attacks and lowered up to 6 dB in sustains. On a plucked note, the attack gains 6 dB of harmonics against its sustain. Pick and stick sounds come forward without the tail getting fizzy.

### 3. A high cut on the harmonics (item 42)

The excited path is already high-passed at Freq, before and after the shaper, which is its low cut. What's missing is control of the top: **High cut** (Off, or 4 to 20 kHz, 12 dB/oct, default Off) on the added harmonics only, to tame fizz on bright sources. The dry path is never filtered.

### 4. Harmonics width (item 43)

**Width** (0 to 150 %, default 100) scales the side of the added part only. Below 100 % the harmonics narrow towards the centre; above it they spread wider than the source. The model gives the same trade-off as Verb's Extra width: a mono fold-down is unchanged, and each side rises by up to 1.6 dB at 150 %, where the added part's correlation reaches −0.09. Above 150 % it would be more opposed than alike.

### 5. Minimum-phase oversampling (item 44)

**Oversampling filter**: Linear phase (today, the default) or **Minimum phase**, as Comp and Limit have. Minimum phase cuts the latency (today 69 samples at 4x) to a few samples, but Exciter is parallel. Its excited path carries the input above Freq as well as the new harmonics, and is added to a delayed dry path, so a minimum-phase path's phase shift combs the sum slightly. The model, at Harmonics 100 % and Mix Back −6 dB, shows today's sum flat at +3.53 dB above Freq. With minimum phase and the dry delayed by its best match, the sum ranges from +2.82 to +3.51 dB, with up to 0.7 dB less near the top. The manual will say to use it where latency matters more than the last fraction of a dB.

### 6. Decisions

- **Decision 1: the second band's controls.**
  - **A (R).** In the EDIT drawer (**Band 2**: Freq 2 and Harmonics 2), keeping the main window's four knobs as they are. A mockup follows approval.
  - **B.** On the main window, which grows by a row.
- **Decision 2: the cuts.**
  - **A (R).** A High cut on the added harmonics only, as above. The low cut is Freq.
  - **B.** Full Low cut and High cut, pre or post, as Sature gets: more controls for what Freq already does at the bottom.
- **Decision 3: Width's range.**
  - **A (R).** 0 to 150 %.
  - **B.** 0 to 200 %, with the manual's mono warning.
- **Decision 4: minimum-phase oversampling.**
  - **A (R).** Offer it as an option, with Linear phase the default, and state the 0.7 dB.
  - **B.** Drop it: Exciter's whole job is the top end, where minimum phase costs a little.

### 7. Tests

1. Legacy: Harmonics 2 at 0, Punch 0, High cut Off, Width 100 %, Linear phase render exactly as v1.7.0.
2. Band 2 at 400 Hz: on the model's tone stack, the added energy from 400 Hz to 3 kHz rises by over 30 dB, and nothing changes above Freq.
3. Punch 100 %: on a plucked note, the added part's attack-to-sustain difference grows by 10 to 14 dB.
4. Width 0 %: the added part is mono, with left equal to right.
5. Minimum phase: the latency is under 8 samples at 4x and reported; the small-signal sum stays within 1 dB of linear phase from Freq to 20 kHz.

### 8. Build order

Approval, then the drawer mockup (`reference/phase4/62__exciter_w2_mockup.png`), then one push with all five items, tests, manual and checklist.
