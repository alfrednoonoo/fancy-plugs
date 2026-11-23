# Fancy Kicker: design note

## DSP

An original parallel low-frequency enhancer:

```
in ──┬───────────────────────────────────────── × Dry ──┐
     └─ mono blend ─ SVF band-pass ─ 15 Hz HPF ─ × Wet ──┴─ + ─ × Output ─ soft clip ─ bypass fade ─ out
```

- **Band-pass:** a topology-preserving state-variable filter with constant peak gain, so Wet sets the level at Freq directly. It stays stable under fast modulation, which is why it is used instead of a biquad. Both integrator states are double precision.
- **Subsonic filter:** a 2nd-order Butterworth high-pass at 15 Hz on the wet path, as a transposed direct form II biquad. The dry path is untouched, so Wet at -inf and Dry at 0 dB reproduces the input exactly.
- **Mono below Freq:** feeds the band-pass with L+R, making the added sub identical in both channels. It acts on the wet path only and fades over 20 ms.
- **Soft clip:** linear below 0.7, then a tanh knee that approaches ±1.0. It is off by default and fades over 20 ms.

## Parameters

| ID | Name | Range | Default | Unit | Skew | Source |
|---|---|---|---|---|---|---|
| `freq` | Freq | 20–240 | 50.0 | Hz | log | §3.1 |
| `wet` | Wet | -60–+12, bottom = -inf | -12.0 | dB | linear | §3.1 |
| `dry` | Dry | -60–+12, bottom = -inf | 0.0 | dB | linear | §3.1 |
| `q` | Filter Q | 0.5–10 | 2.0 | | log | range chosen here |
| `output` | Output | -24–+12 | 0.0 | dB | linear | range chosen here |
| `monoBelow` | Mono below Freq | off/on | off | | | §5.4 |
| `softClip` | Soft clip | off/on | off | | | §5.4 |
| `bypass` | Bypass | off/on | off | | | §4.6 |

The header knob and the EDIT drawer's Output knob control the same `output` parameter.

## Smoothing

All continuous parameters use one-pole smoothers, per sample: Freq and Q over 30 ms in the log domain, gains over 20 ms. Filter coefficients are recomputed every sample while Freq or Q is moving, and not at all once settled. Bypass fades over 10 ms.

## Latency and phase

Latency is 0 samples at every setting. The wet path is minimum-phase and is not delay-compensated. The band-pass has zero phase shift at Freq, so wet and dry add constructively there. The 15 Hz high-pass adds some phase lead on top: about 25° at 50 Hz, where wet and dry at equal level sum to about +5.8 dB instead of +6 dB. The lead grows towards the bottom of the range, so the sum at 20 Hz is noticeably less than +6 dB. The phase offset is part of the sound.

## State

XML with a `stateVersion` attribute (currently 1) plus `uiScale` and `presetName`. Unknown parameters are dropped, out-of-range values are clamped, non-finite values fall back to defaults, and unreadable data is ignored.

## §5.4 fixes

| Requirement | How it is met | Test |
|---|---|---|
| Click-free 20→240 Hz sweep in 200 ms | Per-sample log smoothing into a modulation-stable SVF | `frequency sweep is click-free` |
| DC blocker / subsonic HPF | 15 Hz 2nd-order HPF on the wet path | `wet path blocks DC` |
| Mono below Freq | Wet input summed to mono | `mono wet path` |
| Soft-clip safety | Optional output stage, off by default | `soft clip bounds the output` |
| Phase-coherent wet/dry | Documented as minimum-phase, see above | `wet and dry add in phase at Freq` |

## Layout

Base window is 392 × 472, the Figma Kicker frame without its 22 px host title bar, and all positions come from that frame. In and Out are each a stereo pair of bars with one readout per pair.

The EDIT drawer opens over the left panel, so the window and main layout don't change.

## Phase 4 Wave 1: sub length, make-room, polarity, sub meter, auto gain and ceiling

Items from `docs/enhancements.md` (Wave 1 of the Phase 4 order). **Approved by the user on 21 November 2026 with every recommended option (R).** Every new parameter is appended, with a default that keeps saved projects bit-identical, checked against the v1.6.0 DSP in a legacy test as Comp's Wave 1 does. Items were checked in the code on 21 November 2026 before this note was written. Backlog items 1, 2, 3, 4 and 6 of section 1. Kicker's sidechain Duck (item 7) is its own Phase 4 item, after the tempo-synced trigger.

### 1. Sub length

Sub mode's generated sub lasts as long as the detector hears the kick. **Sub length** (`subLen`, 20 to 2000 ms, with **Off** at the top, the default) ends it earlier with a smooth fade, so a long 808 tail doesn't run into the next kick, without touching Sensitivity. Off is today's.

### 2. Make-room

`roomCut` (off) and `roomFreq` (20 to 200 Hz, default 60): a 12 dB/oct high-pass with a small resonance on the **source** kick, so the generated sub has the low end to itself (the Dropkick idea). Sub mode only; the generated sub isn't filtered.

### 3. Sub polarity

`subPolarity` (off): inverts the generated sub, for kicks whose own low end cancels it. The detector's delay compensation is unchanged.

### 4. Sub meter

A thin meter beside the Out meter showing only the generated sub's level, in Kicker's red, so you can see how much the sub adds. Sub mode only; no parameter.

### 5. Auto gain and ceiling

- `autoGain` (off): trims the output by what Boost or Sub adds to the kick, measured as Exciter's is (a background copy of the path on a reference kick), smoothed over 200 ms.
- `ceiling` Off or −12 to 0 dBTP (default Off): the shared `TruePeakLimiter` after Soft clip.

### 6. Where the controls go

All five settings in the EDIT drawer (Sub length, Make-room and Polarity in a Sub group, dimmed in Boost mode); the sub meter on the main window beside the Out meter. **Decision 1:** the sub meter's place.
- **A (R).** A 6 px bar between the In and Out meters. Nothing else moves.
- **B.** No meter; the drawer shows the sub's level as a number.

### 7. Parameters

| ID | Name | Range | Default |
|---|---|---|---|
| `subLen` | Sub length | 20 to 2000 ms, Off | Off |
| `roomCut`, `roomFreq` | Make-room | off/on; 20 to 200 Hz | off; 60 Hz |
| `subPolarity` | Sub polarity | off/on | off |
| `autoGain` | Auto gain | off/on | off |
| `ceiling` | Ceiling | Off, −12 to 0 dBTP | Off |

### 8. Tests

1. Legacy: v1.6.0's Kicker against the new one, Boost and Sub, one and two octaves: identical.
2. Sub length 150 ms on a kick with a 1 s tail: the sub is 60 dB down by 150 ms + 20 ms.
3. Make-room at 60 Hz: the source's 30 Hz is 12 dB down; the generated sub's isn't.
4. Polarity: the sub with Polarity on sums with it off to under −90 dBFS.
5. Ceiling and auto gain, as Exciter's tests.

Mockup: `reference/phase4/40__kicker_w1_mockup.png` (source `design/phase4-mockup-src/w1_mockups.py`).

### 9. As built

One push (`22b0e05`), with every recommended option; `tests/KickerW1Tests.cpp`.

- **Sub length:** `subLen`, 20 to 2000 ms, Off at the top; a new hit is the generator starting or its level jumping over its recent average, and the sub then fades over 20 ms (60 dB). **Test corrected after runs 44942993322 to 44949179446:** the sub measured about 56 dB down 30 ms after its fade, which turned out to be the sub output's 15 Hz DC filter ringing after the fade, not a restart; two changes to the onset logic made on that wrong guess were reverted, and the test now measures from 300 ms, after the filter has rung out.
- **Make-room:** `roomCut` and `roomFreq` (20 to 200 Hz, 60), a high-pass on the source kick in Sub mode. **Invert sub:** `subPolarity`.
- **Sub meter:** a 6 px bar between the In and Out meters.
- **Auto gain** (`autoGain`) and **Ceiling** (`ceiling`, Off or -12 to 0 dBTP) in the drawer. `KickerDsp`'s sub peak is a copyable atomic, so the tests can build an engine by value (fixed after run 44935591606).

## Phase 4 Wave 2: an oversampled clipper

**Draft for approval (23 November 2026).** Item 45 of the Wave 2 order (`docs/enhancements.md`; backlog item 5 of section 1), for v1.7.1. Modelled first: `docs/research/exciter-kicker-w2-model.py` (part 45).

### 1. What it adds

Kicker's output soft clipper runs at the base rate today, so a loud kick's click aliases. **Clip oversampling** (`clipOs`): **Off** (today, the default, bit for bit), **2x** or **4x**, through the shared `SwitchedOversampler` that Sature and Comp use, with its latency reported only while it's on.

### 2. What the model found

On a 0 dBFS kick with a 3 ms click pushed into the clipper, the aliasing and error under the signal measured:

| Clip oversampling | Alias and error |
|---|---|
| Off (today) | −32.9 dB |
| 2x | −49.5 dB |
| 4x | −75.4 dB |

At 1x the click's aliasing sits only 33 dB under the kick, which is audible as grit on bright kicks. 4x puts it 75 dB down.

### 3. Decisions

- **Decision 1: the default.**
  - **A (R).** Off, so saved projects are bit-identical. The manual recommends 4x when the clipper is working hard.
  - **B.** 4x for new instances only, with saved projects loading Off.
- **Decision 2: where it goes.**
  - **A (R).** A **Clip oversampling** dropdown in the EDIT drawer, under Ceiling, in both Boost and Sub mode. No mockup is needed beyond a drawer screenshot.
  - **B.** Tied to the existing **Soft clip** checkbox, so ticking it also oversamples at 4x.

### 4. Tests

1. Legacy: Off renders exactly as v1.7.0 in Boost and Sub mode.
2. 4x on the model's kick: alias and error at least 35 dB lower than Off.
3. The latency is reported and the dry path matched while it's on, and it drops back to 0 when Off.

### 5. Build order

Approval, then one push: the parameter, the oversampler, the drawer row, tests, manual and checklist.
