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

**Approved by the user on 23 November 2026: decisions 1 and 2 both A.** Item 45 of the Wave 2 order (`docs/enhancements.md`; backlog item 5 of section 1), for v1.7.1. Modelled first: `docs/research/exciter-kicker-w2-model.py` (part 45).

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

Approval, then one push: the parameter, the oversampler, the drawer row, tests, manual and checklist. The row sits on the drawer's second page, Duck and clip, under CLIPPER, as mockup 63 draws it, since the first page has no free row under Ceiling (user, 23 November 2026).

### 6. Pre-build check (24 November 2026, before any code)

`docs/research/kicker-clip-prepush.py` runs the kick through filters like `SwitchedOversampler`'s (half-band, transition 0.06 and 0.12, stopbands −100 and −90 dB) and Kicker's own soft clipper (knee 0.7, then tanh), where the design model used ideal resampling and a plain tanh.

- **Test 2 as written would fail.** On the design model's click, which has content up to 24 kHz, 4x is only 7 dB under Off measured to 24 kHz, and 18 dB under it below 20 kHz. The oversampler's passband ends near 21 kHz, so the clipper is fed a slightly different signal than the ideal reference's, and the clipper's nonlinearity spreads that difference everywhere. That's the filters, not aliasing, and no real oversampler avoids it.
- With the click low-passed at 18 kHz (real kicks have little above it) and the error measured below 20 kHz: Off −39.2 dB, 2x −60.0, 4x −75.0, so 4x is 35.8 dB under Off. At 20 kHz it's 34.6 dB, just under the note's 35.
- **Proposed test 2 (needs your OK):** the model's kick with its click low-passed at 18 kHz, the error against a 16x ideal reference measured below 20 kHz after removing the latency; 4x at least 30 dB under Off, and 2x at least 15 dB. The table in section 2 is the ideal case; the manual should quote the realistic figures (4x about 35 dB cleaner on a typical kick).
- **Decided by the user, 24 November 2026:** test 2 is the proposed one above (click low-passed at 18 kHz, error below 20 kHz against a 16x ideal reference, latency removed; 4x at least 30 dB under Off, 2x at least 15 dB). The manual quotes about 35 dB for 4x on a typical kick.
- **For the build:** the clipper runs per sample after the output gain, and the bypass mix uses the undelayed input. With oversampling on, the pre-clip signal has to be gathered per block, oversampled, clipped and brought back, and the bypass mix's input delayed by the oversampler's latency (69 samples at 4x in Exciter, 66 in this model), in both Boost and Sub mode.

### 7. As built (24 November 2026, built with the Duck in one push)

- **Parameter:** `clipOs` (Off, 2x, 4x; Off), appended, not automatable, since it changes the latency (as Gate's lookahead).
- **The clip stage** lives in `KickerProcessor` (the engine stays free of JUCE): `KickerDsp::process` takes an optional `Deferred`, with which it leaves each sample before its soft clip and bypass mix and records the sample's clip and bypass amounts. The processor then runs the block, in chunks of 512, through a `fancy::dsp::SwitchedOversampler` (linear phase, one stereo copy), clips each oversampled sample with its base sample's amount, brings it back, and mixes the bypass against the input and bypass amount delayed by the oversampler's latency. Switching uses the oversampler's own fade, hold and fade-in; once Off has faded fully back in, the processor returns to the plain path. Off and settled, `KickerDsp::process` runs as before, to the bit (test 1).
- **Latency:** the oversampler's plus the Ceiling's, reported per block, so it drops to 0 when Off has switched through: 55 samples at 2x and 68 at 4x, at 48 kHz (69 at 4x with the allpass, before the fix below).
- **No allpass (fixed after run 45467687811):** that run's test 2 measured Off -37.2 and 2x -57.7 dB, as the model, but 4x only -28.2. JUCE's `Oversampling` makes a fractional latency whole with a Thiran allpass delay at the base rate (`setUsingIntegerLatency`), and 4x's latency is fractional, so the allpass shifted the phase of the click's top octaves: not aliasing, but a real change to the sound against the ideal clip. `SwitchedOversampler::prepare` now takes `integerLatency`; Kicker passes false, and pads the latency to whole base samples with an exact delay of `getPadding()` samples at the oversampled rate, after the clip. Exciter, Sature and Comp keep the default, unchanged. Run 45473290714 (commit `4719d2f`): Off -37.2, 2x -57.7, 4x -71.9 dB, as the model; Duck test 2 -5.99 and -0.97 dB, as the model. CPU at 48 kHz, 128 samples: the processor at its defaults 0.18 %, with Duck, Soft clip and 4x 1.80 %.
- **Tests** (`tests/KickerW2Tests.cpp`, numbers from `docs/research/kicker-w2-prepush.py` on the same signal: the click's noise comes from a small LCG, which both copy): test 1 against `tests/LegacyKickerW1Dsp.h` (v1.7.0's engine) in Boost and Sub, one and two octaves, with and without Soft clip and a sidechain bus; test 2 as decided (model: Off -37.2, 2x -57.7, 4x -72.0 dB, 4x 34.7 dB under Off); test 3 checks that a tone under the knee and the bypassed signal both come out as the input delayed by the reported latency, at 2x and 4x, and that the latency is back to 0 after switching Off.

## Phase 4: sidechain Duck

**Approved by the user on 23 November 2026: decisions 1 to 3 all A.** Kicker 7 of `docs/enhancements.md`, from Phase 3 Q2.2 ("an optional sidechain input ducks the added sub when a kick hits on the sidechain"), deferred to Phase 4 and now planned for v1.7.1 (user, 22 November 2026). Modelled first: `docs/research/kicker-duck-model.py`.

### 1. What it does

On a bass, Kicker adds low end: Boost swells the bass's own note, and Sub adds an octave below. When the kick hits, kick and bass fill 40–80 Hz together and the low end clips or blurs. **Duck** turns the bass down while a kick on the sidechain hits, then lets it back over **Release**, so the two take turns.

- **Duck** (0 to 24 dB, default 0, when it isn't run) and **Duck release** (20 to 500 ms, default 120).
- **Duck from**: **Sidechain** (an optional stereo sidechain bus, as on Verb and Room) or **Host sync**, the shared tempo-synced trigger Comp and Gate use, with its Rate and Shape, for ducking on the beat without a kick track.
- The detector follows the key's peak with a 1 ms attack and ducks fully above −12 dBFS, not at all below −24 dBFS, so a kick's tail doesn't hold it down.

### 2. What the model found

A bass line (55 to 82 Hz in 8th notes) with Kicker's added sub, and a kick on every beat on the sidechain, Release 120 ms:

| What's ducked | Duck | The bass under each kick | Peak during kicks | The bass between kicks |
|---|---|---|---|---|
| — | 0 dB | −11.2 dB | +1.9 dBFS | 0 dB |
| The added sub only (Q2.2's wording) | 6 dB | −12.0 dB | +1.4 dBFS | −0.3 dB |
| | 12 dB | −12.2 dB | +1.2 dBFS | −0.5 dB |
| **The whole bass, sub and dry** | **6 dB** | **−17.2 dB** | **+0.1 dBFS** | **−1.7 dB** |
| | 12 dB | −23.2 dB | −1.0 dBFS | −3.2 dB |

Ducking only the added sub barely helps, because most of what meets the kick is the bass itself. Ducking the whole bass is what makes room. An earlier run, ducking fully down to −40 dBFS, never let the bass back between kicks, because the kick's tail kept it down. That's why the detector stops at −24.

### 3. Decisions

- **Decision 1: what's ducked.**
  - **A (R).** The whole output, the added part and the dry: the model shows that's what clears the low end.
  - **B.** The added part only, as Q2.2 wrote it.
- **Decision 2: Host sync.**
  - **A (R).** Include it, as Duck from's second choice, sharing Comp's and Gate's trigger (`TempoTrigger`, flat pulse as Gate's).
  - **B.** Sidechain only.
- **Decision 3: where the controls go.**
  - **A (R).** In the EDIT drawer, in both Boost and Sub mode: Duck and Duck release as small knobs, Duck from as a dropdown, and Rate and Shape while Host sync is chosen. The main window gets a small **DUCK** tag beside the EDIT link while Duck is above 0, so it isn't hidden. A mockup follows approval.
  - **B.** Duck on the main window as a fourth knob.

### 4. Tests

1. Legacy: Duck 0 renders exactly as v1.7.0 in Boost and Sub mode, with no sidechain bus connected.
2. The model's mix, Duck 6 dB on the whole output: the bass under each kick is at least 5 dB lower, and between kicks it's back within 2 dB.
3. A kick tail held at −30 dBFS doesn't duck.
4. Host sync at 1/4, 120 BPM: ducks on every beat to the sample, as Gate's test.
5. The host checks with the new bus: a mono main bus with no sidechain, sidechain inactive, the validators, and the "No sidechain signal" notice.

### 5. Build order

Approval, then the drawer mockup (`reference/phase4/63__kicker_duck_mockup.png`), then one push: the bus, the ducker, Host sync, the drawer and the tag, tests, manual and checklist. Mockup 63 approved by the user on 23 November 2026: the window grows 40 px wider, to 432 px, so the DUCK tag sits beside EDIT; the drawer has two pages, Main and Duck and clip.

### 6. Pre-build check (24 November 2026, before any code)

Run on `docs/research/kicker-duck-model.py`'s own signals, at Duck 6 dB on the whole output:

- **Under each kick:** −6.0 dB, whatever the detector's details, against test 2's "at least 5 dB": a 1 dB margin.
- **Between kicks, as the model measures it (0.3 to 0.48 s after each kick):** −1.7 dB against "back within 2 dB", a 0.3 dB margin. It depends on how fast the key's peak follower lets go, which the note doesn't give: −1.6 dB at 10 ms, −1.7 dB at 30 ms (the model's), −2.0 dB at 60 ms, which would fail. At 0.3 s the kick's tail is still at −24 dBFS, the detector's floor.
- **For the build:** the peak follower's release is 30 ms, as in the model, and the design should say so. **Proposed for test 2 (needs your OK):** measure "between kicks" over the last 100 ms before the next kick (0.4 to 0.5 s), where the model gives −1.0 dB (−0.9 to −1.2 dB across 10 to 60 ms), and keep "within 2 dB".
- **Decided by the user, 24 November 2026:** the key's peak follower releases over 30 ms, and test 2 measures "between kicks" over the last 100 ms before the next kick (0.4 to 0.5 s), within 2 dB.

### 7. As built (24 November 2026, built with item 45 in one push)

- **Parameters,** appended after Ceiling: `duck` (0 to 24 dB, 0), `duckRelease` (20 to 500 ms, log, 120), `duckFrom` (Sidechain, Host sync), `duckRate` (Gate's list, 1/4) and `duckShape` (5 to 100 %, 50). An optional stereo **Sidechain** input bus, off, mono or stereo, as Room's.
- **The detector,** `plugins/kicker/src/KickerDucker.h`: the key's peak, held instantly and released over 30 ms; the duck wanted is Duck times where that peak sits between -24 and -12 dBFS, in dB; the duck moves towards it over a 1 ms attack and Release, and lets go fully once it's under 0.0001 dB with nothing wanted, so an idle Duck gives a gain of exactly 1. The key is the sidechain's larger channel, or, with Host sync, `fancy::dsp::TempoTrigger`'s flat pulse (Gate's), which runs on at 120 BPM without a host tempo.
- **What's ducked:** the whole output, as a gain on each sample before the soft clip (Decision 1 A), so a ducked bass also drives the clipper less. With Duck 0 and fully let go, the processor takes the plain path, to the bit (test 1). `SidechainWatch` shows "No sidechain signal" while Duck is above 0 from a silent sidechain.
- **The editor (mockup 63):** the window is 432 px wide (its minimum too), the knob card 40 px wider with Freq, Wet, Dry, the mode switch and the octave row 20 px right; the drawer's contents keep their places. The drawer's title gives way to a **Main | Duck and clip** switch (`fancy::ui::ViewTabs`). Duck and clip has DUCK (Duck, Release, Duck from), HOST SYNC (Rate, Shape; shown only while Host sync is chosen) and CLIPPER (Clip oversampling, with the latency note), placed from mockup 63's source (window coordinates less 73). The **DUCK** tag (42 x 18, at 76, 382) shows beside EDIT while Duck is above 0 and opens the second page; the mode caption's right edge moves from 24 to 12 px inside the card (`CaptionPanels::setCaptionRightInset`, new in `fancy-core`), so "Sub: adds two octaves below" clears the tag. Screenshots: `kicker-duck` (the tag) and `kicker-duck-edit` (the page), at mockup 63's values.
- **Tests** (`tests/KickerW2Tests.cpp`, numbers from `docs/research/kicker-w2-prepush.py`): 1 as test 1 above; 2 on the model's bass and kick through the processor, Duck 6 dB against Duck 0 (model -5.99 dB under each kick, -0.97 dB over the last 100 ms before the next); 3 a key held at -30 dBFS with Duck 24 dB renders bit-identical to Duck 0; 4 Host sync at 1/4 with no play head (120 BPM) starts each duck on the beat's sample at blocks of 64, 333 and 4096; and the detector's -12 and -24 dBFS points alone. Test 5: `HostTests.cpp`'s sidechain case for Kicker (inactive, mono, stereo, the notice); the mono-bus case covers every plugin.
