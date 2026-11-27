# Fancy Comp: design note

## DSP

An original feed-forward compressor with optional feedback detection:

```
main ──┬─ delay (matches all wet latency) ───────────────────────────── × Dry ─┐
       │                                                                       + ─ × Output ─ bypass ─ limiter ─ out
       └─ [ up ─ pre-comp delay ─ × gain ─ × make-up ─ down ] ───────── × Wet ─┘
                                   ▲
main or sidechain ─ [ up ─ detector select ─ HP ─ LP ─ RMS ─ link ─ gain computer ─ ballistics ]
```

Everything inside the brackets runs at the oversampled rate.

- **Detector input:** Main L+R, Main L, Main R, Sidechain L+R, Sidechain L, Sidechain R, Mid, Side. L+R modes detect each channel separately, and Link channels decides how they combine. The others feed both channels the same detector signal.
- **Detector filters:** 2nd-order state-variable high-pass and low-pass. The filters switch out completely at their extremes: Highpass at 20 Hz, Lowpass at 24 kHz.
- **RMS size:** a one-pole average of the squared detector signal. 0 ms is peak detection. A one-pole average is used in place of a sliding window, so the size can move smoothly without recomputing a window on the audio thread. With oversampling on, the average is never shorter than 0.1 ms. Without that minimum, high frequencies swing the envelope through a sharp sawtooth on every half cycle, and its harmonics fold into the gain.
- **Link:** when Link channels is on, each channel's level moves towards the louder channel by Link amount, default 100 %.
- **Gain computer:** log domain, with a quadratic soft knee centred on Threshold over Knee dB. **Weird** moves the knee so it starts at Threshold and ends Knee dB above it, so compression arrives later.
- **Ballistics:** see below.
- **Make-up:** Auto make-up adds half of the gain reduction that a 0 dBFS signal would get: `0.5 × (1 - 1/ratio) × -threshold`.
- **Preview filter:** replaces the wet signal with the filtered detector signal. Since 10 November 2026 it's a temporary switch, not a parameter (`FancyProcessor::setListening`, Phase 3 new-plugins brief §0.3); a saved `preview` value is ignored.
- **Feedback detection:** the detector reads the previous output sample of the compressor in place of the main input. Sidechain inputs are unaffected.

## Ballistics (§5.2)

| Setting | Behaviour |
|---|---|
| **Classic off** (default) | Branching peak detector with smoothed gain. The attack and release filter the gain reduction, so maximum gain reduction doesn't depend on attack |
| **Classic on** | Smoothed detector. The attack and release filter the level before the gain computer, so a short peak with a long attack gets less gain reduction. This is by design |
| **Auto** | Program-dependent release in two stages. Release runs at a tenth of the set time down to a slow envelope, which follows the gain reduction at the set time. Transients recover fast, and sustained compression recovers slowly |
| **Release curve** | Exponential (default), or Linear, which falls at a constant rate in dB with the same initial slope |
| **Attack convention** | Attack and release are one time constant, the time to reach about 63 % of the target. The "Attack = 90 % rise" option makes the set time the 90 % point instead, by dividing the time constant by 2.303 |

## Parameters

| ID | Name | Range | Default | Unit | Skew | Source |
|---|---|---|---|---|---|---|
| `threshold` | Threshold | -60–0 | 0.0 | dB | linear | §3.3 |
| `ratio` | Ratio | 1:1–∞:1 | 4:1 | | log, top of range is ∞ | §3.3 |
| `knee` | Knee | 0–24 | 0.5 | dB | skewed | §3.3 |
| `weird` | Weird | off/on | off | | | §3.3 |
| `attack` | Attack | 0–500 | 3.0 | ms | skewed | §3.3 |
| `classic` | Classic | off/on | off | | | §3.3, §5.2 |
| `release` | Release | 0–5000 | 200 | ms | skewed | §3.3 |
| `auto` | Auto | off/on | off | | | §3.3 |
| `precomp` | Pre-comp | 0–250 | 0.0 | ms | skewed | §3.3 |
| `rms` | RMS size | 0–250 | 5.0 | ms | skewed | §3.3 |
| `lowpass` | Lowpass | 20–24000 | 24 kHz (off) | Hz | log | §3.3 |
| `highpass` | Highpass | 20–24000 | 20 Hz (off) | Hz | log | §3.3 |
| `wet` | Wet | -60–+12, bottom = -inf | 0.0 | dB | linear | range chosen here |
| `dry` | Dry | -60–+12, bottom = -inf | -inf | dB | linear | range chosen here |
| `detector` | Detector input | 8 choices | Main input L+R | | | §3.3 |
| `link` | Link channels | off/on | on | | | §3.3 |
| `makeup` | Auto make-up | off/on | off | | | §3.3 |
| `limit` | Limit output | off/on | off | | | §3.3 |
| `oversampling` | Oversampling | Off, 2x, 4x | Off | | | §3.3 |
| `osPhase` | Oversampling filter | Linear, Minimum | Linear | | | §5.2 |
| `feedback` | Feedback detection | off/on | off | | | §3.3 |
| `linkAmount` | Link amount | 0–100 | 100 | % | linear | §3.3 "Mix linking" |
| `releaseCurve` | Release curve | Exponential, Linear | Exponential | | | §5.2 |
| `attack90` | Attack = 90 % rise | off/on | off | | | §5.2 |
| `output` | Output | -24–+12 | 0.0 | dB | linear | range chosen here |
| `bypass` | Bypass | off/on | off | | | §4.6 |

The mockups show several checkboxes ticked. Those are demonstration states, so the defaults above are the neutral settings. Link channels defaults to on, because unlinked channels make a stereo image wander.

**Classic** is the §5.2 detector mode switch, so that switch needs no separate control.

## Smoothing

One-pole smoothers at the oversampled rate for Threshold, Ratio, Knee, make-up, RMS size and preview, over 20 ms. Filter coefficients are recomputed every sample while the frequencies move. Wet, Dry, Output and bypass are smoothed at the base rate.

## Latency

Total latency is the sum of oversampling latency, Pre-comp, and limiter lookahead when Limit output is on. Dry is delayed by all of it except the limiter, which is in series with both.

Pre-comp, oversampling and Limit output all change the latency, so a change is held until the value has been stable for 50 ms. The output then fades out, the delays are reconfigured, the new latency is reported, and the output stays silent until the delays have refilled. Dragging Pre-comp therefore causes one dropout at the end of the drag, not many.

## Output limiter (§5.2)

A separate stage after the make-up gain and the wet/dry sum, with a fixed 0 dBFS ceiling and about 2 ms of lookahead, so the ceiling doesn't depend on oversampling or input level.

- **True-peak detection:** a 4× windowed-sinc interpolator estimates inter-sample peaks.
- **Gain:** the required gain goes through a sliding minimum over the lookahead window, then a one-pole release, then a moving average over the same window. The gain therefore reaches its target before the peak leaves the delay.
- **Linked:** both channels share one gain, so limiting doesn't shift the image.
- **Guarantee:** a final clamp at ±1.0 catches anything left, so sample peaks can't exceed the ceiling even at extreme levels.

## State

The same format as the other plugins: XML with `stateVersion` 1, `uiScale` and `presetName`, with the same validation.

## §5.2 fixes

| Requirement | How it is met | Test |
|---|---|---|
| Proper oversampling | Polyphase 2×/4× around the gain computer and gain, linear- or minimum-phase | `aliasing is below -90 dBFS at 4x` |
| Limit output at extreme levels | Separate stage, gain in double precision, final clamp | `limiter holds the ceiling at extreme levels` |
| Ceiling independent of oversampling | Limiter runs at the base rate after everything else | same test, across oversampling modes |
| Attack time convention | One time constant, optional 90 % rise, shown in the tooltip and manual | `attack time follows the convention` |
| Maximum GR versus attack | Branching default, Classic switch | `maximum reduction ignores attack unless Classic is on` |
| Release curve | Exponential default, Linear option, Auto is dual time constant | `release curves` |
| Automation enabling changes audio | Regression test | `comp automation with an unchanged value is bit-identical` |
| Pre-comp latency | Reported, dry delayed | `pre-comp latency is reported and wet and dry stay aligned` |

## Layout

The Figma Reacomp frame is 834 × 527 without its host title bar, but it has no GR meter or Link channels checkbox. §3.3, §2.5 and `reference/08__ReaComp.png` all require both, and §2.0b says the control map applies even when Figma differs. So a 100 px GR panel sits between Threshold and the main grid, as in the reference, and everything to its right moves over by 100. The window is 934 × 527.

Figma's four footer checkboxes become Link channels, Auto make-up, Preview filter and Limit output.

The settings that aren't in the mockups sit in an EDIT drawer, which opens over the main grid. Its link is at the right of the Detector input row, so the main layout doesn't change.

## Phase 3 Q2.5: character, upward compression, auto threshold, sidechain EQ and 8x

Brief: Q2.5 of `docs/BUILD_PROMPT_PHASE3.md`. **Approved by the user on 14 November 2026, with every recommended option (R):** 1 A, 2 A, 3 A, 4 A, 5 A, 6 A, and the three factory presets. The `CHAR`/`UP` badge had no recommendation and is left out. Mockup: `reference/phase3/23__comp_q25_mockup.png`, source `design/phase3-mockup-src/comp-q25.html`, waiting for the user's approval before any code.

### 1. Signal path

```
main ──┬─ delay (matches all wet latency) ─────────────────────────────────────────────── × Dry ─┐
       │                                                                                         + ─ × Output ─ bypass ─ limiter ─ out
       └─ [ up ─ PRE char ─ pre-comp delay ─ × gain ─ × make-up ─ POST char ─ down ] ─── × Wet ─┘
                   │                           ▲
                   │      gain = make-up × 10^((lift − reduction) / 20)
                   ▼                           │
main or sidechain ─ [ up ─ detector select ─ HP ─ LP ─ 3-band EQ ─ RMS ─ link ─ auto threshold ─ gain computer (down + up) ─ ballistics ]
```

Everything new sits inside the oversampled brackets. With every new setting at its default the path is today's, to the bit (section 7).

### 2. Character

A saturation stage in the wet path, at the oversampled rate, using Sature's curve and ADAA code from `fancy/dsp/Saturation.h` (no new shaper code).

| Type | Curve | Emphasis | Sound |
|---|---|---|---|
| **Off** (default) | none: the stage is skipped | | today's sound |
| **Warm** | Sature's soft curve, asymmetric (negative limit 1.4), so even harmonics from the first dB of drive | a first-order tilt of −4 dB (pivot 1 kHz) before the curve and its inverse after, so the lows saturate more than the highs | thick, rounded |
| **Bright** | Sature's transistor knee (linear to 0.5, knee to 1.5), symmetric, so odd harmonics | the opposite tilt, +4 dB, and its inverse after, so the highs saturate more | edge, presence |

- **Drive** 0 to 24 dB. The stage computes `f(d·x) / d`: every curve has slope 1 at zero, so quiet material passes at unity and only peaks are shaped. Turning Drive up therefore adds harmonics and rounds peaks without a jump in level, and Auto make-up needs no change.
- **DC:** Warm's asymmetry makes DC on loud signals, so a 5 Hz high-pass follows the stage (Warm only).
- **Position:** Pre (into the compressor) or Post (after make-up, before Wet). Default Post.
- **Decision 1: does the detector hear Pre's saturation?**
  - **A (R).** Yes, when the detector reads the main input: Pre behaves like a saturator inserted before the compressor, which is what "pre" means. Raising Drive rounds peaks, so the compressor reacts a little less to them.
  - **B.** No: the detector always hears the clean input, so Drive never changes the gain reduction. Easier to reason about, but Pre and Post then differ only in where the harmonics are compressed.
- **Oversampling Off:** the stage runs at the base rate with ADAA alone, like Sature at Off.
- **Changing Type or Position** is click-free: the stage crossfades over 10 ms. Drive is smoothed over 20 ms.

### 3. Upward compression

- **Shared code:** MultiComp's `liftFor` and its lift follower move into `fancy-core` as `fancy/dsp/Upward.h`. MultiComp uses it unchanged and must stay bit-identical (its existing tests and Upward tests).
- **Same controls and static curve as Phase 2.1 U:** Up threshold, Up ratio, Max lift, Floor, Upward on, with the Up threshold clamped to at most Threshold and the 8 dB fade above Floor.
- **In Comp:** the lift comes from the same detector level as the reduction, after Link and Auto threshold, and has its own follower on Comp's attack and release, Auto release included. With Classic on it reads the smoothed level as the reduction does. The gain is `make-up + lift − reduction` in dB. Auto make-up keeps its formula, since lift doesn't change what a 0 dBFS signal gets.
- **Feedback detection:** the lift follows the feedback level like the reduction does.
- **Decision 2: what the GR meter shows.**
  - **A (R).** The reduction, as now, like MultiComp. The lift shows on a transfer curve in the drawer's Upward tab (section 6), with a moving dot at the current level and its gain.
  - **B.** The net gain: reduction down from the top as now, and lift as a green bar growing up from a zero line moved 12 dB down the meter. It shows both at a glance, but changes how the meter reads in every existing project.

### 4. Auto threshold

Keeps the gain reduction steady when the input gets louder or quieter, for example between a quiet verse and a loud chorus, or across a whole album of different levels.

- **Long-term level:** the mean square of the detector signal (after the sidechain EQ, before RMS size) through a one-pole average with a 3 s time constant. Below −60 dBFS the average holds, so silence and fades don't drag it down.
- **Offset:** `long-term level − reference`, held to ±24 dB and smoothed over 50 ms. It's added to Threshold and to Up threshold, so both the downward and upward curves follow the material.
- **Decision 3: the reference level.**
  - **A (R).** Fixed at −18 dBFS RMS: Threshold is set as if the input sat at −18 dBFS. It's deterministic, so offline renders, saved projects and presets always behave the same.
  - **B.** Latched when Auto threshold is switched on. It feels natural in the moment, but the same project behaves differently depending on where playback was when it was turned on, and offline renders can't reproduce it.
- **Feedback detection:** the tracker reads the detector's input (the main input or sidechain), not the output, so the loop can't run away.
- **Display:** while Auto threshold is on, `ThresholdMeter` draws a second, thinner marker at the effective threshold. That's an option added to the shared component, so Gate is unchanged.
- **Reset:** the average starts at the reference, so the offset begins at 0 dB after loading or `reset()`.

### 5. Sidechain EQ and 8x oversampling

**Fancy EQ's matched filters** (`plugins/eq/src/EqFilters.cpp`, after Vicanek) move into `fancy-core` as `fancy/dsp/MatchedFilters.h`. EQ uses them unchanged and must stay bit-identical.

- **Decision 4: "replacing the high-pass and low-pass".**
  - **A (R).** Keep the Highpass and Lowpass knobs and their filters exactly, and add three bands after them: a low shelf, a bell and a high shelf. The bands live in the drawer's Detector tab with a small response curve. Old projects with a filtered detector stay bit-identical, and the main layout doesn't change.
  - **B.** Replace them literally: three bands, each with a type (High-pass, Low shelf, Bell, High shelf, Low-pass), with the old Highpass and Lowpass migrated into bands 1 and 3, and the two knobs taken off the main layout. Old projects would then compress very slightly differently (matched filters aren't the SVFs) unless their SVFs were kept as a special case, and it changes the main layout.
- **Bands:** gain 0 dB skips a band entirely, so the default is bit-identical. Coefficients are recomputed every 16 samples from interpolated parameters while they move (§4.2). They run on the detector at the oversampled rate.
- **Preview filter** listens through the whole detector chain, EQ bands included.

**Oversampling to 8x.**

- Comp keeps its own oversamplers (not `SwitchedOversampler`), so Off, 2x and 4x keep their filters and stay bit-identical. 8x adds a third half-band stage with the same settings, in linear or minimum phase, for both the main and sidechain paths.
- **Decision 5: the parameter.**
  - **A (R).** Add "8x" to the end of the existing Oversampling choice. Saved states store the choice index, so projects load unchanged. Only a host automation lane drawn on Oversampling would shift, since its normalised values move. Nobody automates it, because every change is a dropout.
  - **B.** A new parameter `osRate` (Off, 2x, 4x, 8x), with the old `oversampling` still registered but hidden from the UI and read only when a state lacks `osRate`. It's safe for automation lanes, but the host then lists two Oversampling parameters for good.
- **Latency:** 8x adds the third stage's latency, reported with the existing hold-and-refill switch.

### 6. Controls

The new controls don't fit in today's drawer (524 × 374, half full).

- **Decision 6: where the new controls go.**
  - **A (R).** The EDIT drawer gets a segment switch at its top: **Detector | Character | Upward | Options**. It uses the shared `ParamSegments` styling, but it's a view switch, not a parameter.
    - **Detector:** the three sidechain EQ bands (Freq, Gain, and Q on the bell) as small knobs, a response curve including Highpass and Lowpass, and Auto threshold.
    - **Character:** a Type segment (Off, Warm, Bright), a Position segment (Pre, Post) and a Drive knob.
    - **Upward:** Upward on, Up threshold, Up ratio, Max lift, Floor and the transfer curve.
    - **Options:** today's drawer contents (Oversampling, filter, Release curve, Feedback detection, Attack = 90 % rise, Link amount, Output).
  - **B.** Grow the window by a row, as MultiComp's Upward did (934 × 527 to about 934 × 640), with Character and Upward always visible. It's quicker to use, but it changes the main layout and window size.
- **Main layout:** unchanged under A. The only visible additions are the second threshold marker (section 4) and, optionally, a small `CHAR` or `UP` badge next to the EDIT link when Character or Upward is on, so a project doesn't hide an active stage. The badge isn't in either decision and is left out for now.
- **Mockup:** `design/phase3-mockup-src/comp-q25.html`, rendered at 2x as `reference/phase3/23__comp_q25_mockup.png`: the four drawer tabs at 524 × 374, the open Oversampling menu with 8x, and the threshold meter's second marker. The view switch sits in the drawer's title row after "EDIT", so the drawer keeps today's size.

### 7. Parameters and state

Appended after `bypass`, so no ID or index moves:

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `charType` | Character | Off, Warm, Bright | Off | | |
| `charPos` | Character position | Pre, Post | Post | | |
| `charDrive` | Drive | 0 to 24 | 6 | dB | linear |
| `up` | Upward | off/on | off | | |
| `upThr` | Up threshold | −80 to 0 | −40 | dB | linear |
| `upRatio` | Up ratio | 1:1 to 1:10 | 1:2 | | log |
| `upLift` | Max lift | 0 to 30 | 12 | dB | linear |
| `upFloor` | Floor | −100 to −40 | −70 | dB | linear |
| `autoThr` | Auto threshold | off/on | off | | |
| `scLowFreq` | SC low freq | 20 to 1000 | 100 | Hz | log |
| `scLowGain` | SC low gain | −18 to +18 | 0 | dB | linear |
| `scMidFreq` | SC mid freq | 100 to 10000 | 1000 | Hz | log |
| `scMidGain` | SC mid gain | −18 to +18 | 0 | dB | linear |
| `scMidQ` | SC mid Q | 0.3 to 8 | 1 | | log |
| `scHighFreq` | SC high freq | 1000 to 20000 | 8000 | Hz | log |
| `scHighGain` | SC high gain | −18 to +18 | 0 | dB | linear |

Plus "8x" in `oversampling` under Decision 5 A. Up ratio is shown and typed as `1:r`, as in MultiComp. Every knob is the shared `Knob`, so typed values come with it.

**State:** `stateVersion` stays at 4. Q2.4 bumped it and v1.4.0 isn't released yet. A state missing the new IDs gives their defaults (Character Off, Upward off, Auto threshold off, EQ bands flat), which are today's path, so v1.3.0 and older projects load and render bit-identically.

**Factory presets (R):** three new ones, with categories: "Warm Bus" (Bus: 2:1, −18 dB, Character Warm Post 6 dB), "Upward Vocal" (Vocals: 3:1 down and 1:3 up, so quiet words come up), and "Level Rider" (Bus: 2:1, Auto threshold on, 1:2 upward). The existing six don't change.

### 8. Tests (Catch2)

1. **Compatibility:** a state stripped of every Q2.5 ID loads with the defaults and renders bit-identically to the instance that saved it, at Off, 2x and 4x, with Highpass and Lowpass set (as EQ's Q2.4 test does).
2. **Shared code:** MultiComp's existing and Upward tests pass unchanged, and EQ's renders are bit-identical after the moves.
3. **Character, small signal:** at −60 dBFS, Warm and Bright at any Drive are within 0.05 dB of Off from 20 Hz to 20 kHz.
4. **Character, harmonics:** a 0 dBFS 1 kHz sine through Warm at 12 dB has a 2nd harmonic above −40 dB; Bright has a 3rd above −40 dB and a 2nd below −80 dB.
5. **Character, aliasing:** a 10 kHz sine at 0 dBFS with Drive 24 dB: aliasing below −90 dBFS at 8x and below −70 dBFS at 4x (the existing method).
6. **Character switch:** changing Type and Position under signal is click-free (the existing click detector).
7. **Upward, static curve:** as U's test 2, through Comp with slow ballistics: within 0.3 dB of the formula, including Max lift, Floor and the clamp to Threshold.
8. **Upward, floor:** silence and noise at −90 dBFS aren't raised.
9. **Auto threshold:** pink noise at −30, −18 and −6 dBFS RMS: once settled, the average gain reduction differs by under 1 dB across the three, against about 12 dB apart with Auto threshold off. A −18 dBFS input gives the same reduction as Auto threshold off, within 0.1 dB.
10. **Sidechain EQ:** with a +12 dB bell at 3 kHz, a 3 kHz tone gets 12 dB more detector level than a 300 Hz tone at the same level (within 0.5 dB, at ∞:1); Preview filter's output shows the bell.
11. **8x:** latency reported matches the measured delay in both filter modes; wet and dry stay aligned; the limiter still holds the ceiling.
12. **Automation:** the existing bit-identical automation test covers every new parameter.
13. **Host:** H's offline-render and flood tests cover the new parameters.
14. **CPU (§0.4):** defaults (today's ratio, 1.00 by definition), Character on at 4x, everything on at 4x, and everything on at 8x, in `docs/measurements/comp-q25.txt`. Targets, restated by the user on 14 November 2026 as the cost the stages add over the same oversampling: everything on adds under 1.5 % at 4x and under 3 % at 8x. The measurement also times 4x and 8x with nothing new on.

### 9. Build order

Two pushes, both checked in REAPER together:

1. The moves into `fancy-core` (Upward, matched filters, the threshold marker), 8x, Character and Upward, with their tests.
2. Auto threshold, the sidechain EQ and the drawer tabs, then screenshots, the manual, the checklist and CPU.

Before the first push, a Python model of the Character curves' harmonics and aliasing and of Auto threshold's settling, as `docs/research/room-model.py` did for Room.

### 10. As built

Push 1 holds the whole item: the moves into `fancy-core`, 8x, Character, Upward, Auto threshold, the sidechain EQ, the drawer's tabs and graphs, the presets and the tests (`tests/CompQ25Tests.cpp`). The model is `docs/research/comp-q25-model.py`.

- **Compatibility:** `tests/LegacyCompDsp.*` is `CompDsp` at commit `07cebe8`. A test renders both at Off, 2x and 4x in six setting variants (filters, Classic, feedback, pre-comp, sidechain, limiter, minimum phase, Auto make-up, linear release, Mid) and requires identical samples and latency. A second test strips the 16 new IDs from a saved state and requires identical renders.
- **Shared code:** EQ's filters are `fancy/dsp/MatchedFilters.h` and `fancy-core/src/MatchedFilters.cpp`, in `fancy::dsp::matched`. `plugins/eq/src/EqFilters.h` is now a shim that brings the names into `eq`, so EQ's code is unchanged. MultiComp's `liftFor` calls `fancy::dsp::upward::liftFor` (`fancy/dsp/Upward.h`).
- **Character, as built:** the stage adds what the curve changes rather than replacing the signal, as Fancy Sature does: `x + (shaped − linear) / Drive` through the inverse tilt, where `linear` is the ADAA's own two-sample average. Quiet material is untouched to the bit (the model: under 0.001 dB at −60 dBFS, 20 Hz to 20 kHz, even at the base rate), and only the added part carries the ADAA's half-sample lag. Warm's DC blocker (5 Hz) works on the added part. A change of Type or Position fades the stage out over 10 ms, switches, and fades in over 10 ms.
- **Character's level:** `f(d·x)/d` holds peaks near −Drive dBFS (at Drive 24 dB, a 0 dBFS sine comes out near −24 dBFS). That's the approved curve; the manual says so.
- **Aliasing targets changed.** Section 8, test 5 asked for −90 dBFS at 8x and −70 at 4x (10 kHz, 0 dBFS, Drive 24, 44.1 kHz). The model, with ideal filters, reaches −88.5 (Bright, 8x) and −73.3 (Bright, 4x); Warm reaches −97.6 and −75.6. The test requires under −82 at 8x, under −66 at 4x, and 8x at least 8 dB under 4x.
- **Upward with Classic:** Classic's smoothed level starts at the bottom of the knee, so it can't see quiet levels. The lift always reads the level itself, through its own follower on Comp's ballistics, in Classic too.
- **Auto threshold's gate:** the long-term level holds below −60 dBFS **or 40 dB under the long-term level**, measured through a 5 ms average. With the designed 50 ms gate alone, the first 0.6 s of a pause pulled the level down by about 1 dB. With feedback detection on a main-input detector, the tracker reads the input before the sidechain EQ.
- **Threshold meter:** the second marker is a 2 px white line at 60 %. The meter has no room under its readout for the mockup's "auto −18.0" caption, so the offset shows only on the Detector page.
- **Drawer:** the view switch remembers the last page per editor, not in the state, and starts on Detector.
- **Sidechain EQ:** the shelves are second order with a Butterworth Q. A band at exactly 0 dB is skipped, and its state is cleared, so it starts from rest when it comes back.
- **Screenshots:** `comp-detector`, `comp-character`, `comp-upward` and `comp-options`, with the mockup's values.
- **Green:** run 43633937359 (`d21fb1b`), 476 test cases, pluginval, Steinberg's validator, the host harness and FancyBench. The first push (`9b77d50`) failed to compile: a tooltip on a plain component and Catch's `range` without its header.
- **CPU targets restated** (`docs/measurements/comp-q25.txt`): as first written, everything on at 4x (4.37 %) and 8x (8.3 %) missed 1.5 % and 3 %, because Comp at 4x with nothing new on already costs 3.38 %. On 14 November 2026 the user restated the targets as the cost added over the same oversampling. Run 43639193810 (`5b71497`): everything on adds 0.86 % at 4x (3.13 % against 2.27 %) and 1.76 % at 8x (5.90 % against 4.14 %), inside both.

## Phase 4 Wave 1: Hold, Range, Listen to removed and GR history

Items 1 to 6 of `docs/enhancements.md` section 3, the first plugin of Wave 1 in the Phase 4 order (`docs/enhancements.md`, "Phase 4 order"). On 21 November 2026 the user said to go on with Phase 4, which this note takes as the proposed order, Comp first. **Approved by the user on 21 November 2026 with every recommended option (A) on Decisions 1 to 4**, plus, after comparing with Woodstock Audio's Waveform Compressor, the History page's waveforms, zoom, pause and beat lines (section 5); that comparison also put an Optical mode in the Wave 2 backlog (`docs/enhancements.md`, Comp 10). Mockup: `design/phase4-mockup-src/comp-w1.html`, rendered at 2x as `reference/phase4/29__comp_w1_mockup.png`.

### 1. Two items are already there

Checked in the code before writing this note:

- **Item 5, lookahead, isn't needed.** Pre-comp (0 to 250 ms) already is lookahead: it delays the audio and not the detector (`preCompDelay` in `CompDsp::compress`), reports the delay as latency and delays the dry path to match (section "Latency"). The backlog's "Comp has none" was wrong; Pre-comp covers Pro-C 3's 1 to 20 ms and more.
- **Item 6, per-channel GR, isn't needed.** The GR meter already draws two bars, left and right, each with its own read-out (`gainReductionMeter.update (left, right, …)`; `docs/images/comp.png` shows −2.8 dB and −2.6 dB). Unlinked, they differ.

Both get crossed off in `docs/enhancements.md` with that reason. That leaves four items.

### 2. Hold

The release waits Hold after the last moment the reduction was rising or level, then releases as now.

- **Where:** in the shared `fancy::dsp::EnvelopeFollower` (`fancy/dsp/Compression.h`). `Ballistics` gets `holdSamples` and the follower a countdown. Each sample where the target is at or above the envelope reloads the countdown; while the target is below and the countdown runs, the envelope stays put. With `holdSamples` at 0 the new branch is never entered, so MultiComp, Gate and FIR, which share the follower and never set it, stay bit-identical (their legacy tests prove it).
- **In Comp:** it holds the gain reduction, in both the default (branching) and Classic modes, and before both stages of Auto release. The upward lift has its own follower and doesn't hold: Hold is about not letting the reduction ripple between peaks.
- **Why it matters:** with a fast release and peak detection, the reduction follows each cycle of a low note, which is distortion. A Hold a little longer than the period (20 ms for 50 Hz) keeps the gain flat between peaks.

### 3. Range

The most reduction Comp will apply. With Auto threshold, it turns Comp into a level rider that never pulls a passage down by more than Range.

- The cap applies to the static curve's output: before the follower in the default mode, after the smoothed level in Classic. So the ballistics stay smooth up to the cap. The GR meter and the history show the capped reduction. Upward lift is unaffected.
- **Off** (default) skips the cap, so the path is today's to the bit.
- **Decision 1: Auto make-up with Range.**
  - **A (R).** Auto make-up uses the capped figure: `0.5 × min (Range, (1 − 1/ratio) × −threshold)`. With a deep threshold and a small Range, make-up then matches what Comp can actually take off. At Off it is today's formula.
  - **B.** Keep today's formula whatever Range is. Simpler, but at Threshold −40 dB, 4:1 and Range 6 dB, make-up adds 15 dB to a signal that lost at most 6.

### 4. Listen to removed

A temporary listen switch (§0.3 of `docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md`): not a parameter, never saved, automated or undone, and off when the last editor closes.

- **Decision 2: what it plays.**
  - **A (R).** What the downward compression took off: the delayed input times `1 − 10^(−reduction/20)`, before make-up, lift and Character. It is taken where the gain is applied, so at the oversampled rate and through the same down-sampler. Wet, Dry and Output don't apply; the output limiter does, if it's on. With make-up, Upward and Character off, the normal output plus this signal is the input (a null test, section 7).
  - **B.** The input minus the final output. That includes make-up, so with Auto make-up on it is mostly the input turned up, which says little about the compression.
- **Two listen switches.** `FancyProcessor` has one listen flag. It gets a listen *target*, an int that defaults to 0, and `ListenCheckbox` an optional target: a checkbox is on when listening is on and the target is its own, and clicking it sets both. Preview filter is target 0 and Listen to removed target 1, so the two are exclusive. Every other plugin uses target 0 and is unchanged.
- **Decision 3: where the switch goes.**
  - **A (R).** A fifth footer checkbox, between Preview filter and Limit output, so the two listens sit together. The five are spaced evenly across the 750 px left of the Out meter (mockup, section 1: x 24, 167, 314, 453 and 624, with 29 px gaps). This is the one change to the main layout; the window size doesn't change.
  - **B.** On the drawer's Options page. The footer stays as it is, but a listen switch is then hidden behind EDIT, where nobody looks for one.

### 5. GR history

A fifth page of the EDIT drawer (Decision 4 A, approved), **History**, oldest on the left, one column per pixel (mockup, section 3):

- **Waveforms:** the input in grey and the output in teal over it, mirrored about a centre line on a linear scale. The output is drawn before make-up, so the two line up when nothing is compressed and the gap is what Comp took off.
- **Gain reduction:** a yellow line from the top (0 to 24 dB, right scale), the GR meter's colour. Unlinked, the right channel is a lighter second line.
- **Threshold:** the effective threshold as a dashed pair of lines at ± its level; it moves with Auto threshold.
- **Zoom:** 1.5, 3, 6 or 12 s, a segment switch above the graph.
- **Beat and bar lines** from the host's tempo and time signature while the transport plays (bars brighter), switched by a "Grid" checkbox. Stopped or without tempo, none.
- **Pause:** a click on the graph freezes it, and the caption says "paused · click to go on"; another click goes on. The audio and the meters carry on.
- **Peak read-out:** the largest reduction in the shown span; a click clears it.
- None of these are parameters. Zoom and Grid are remembered per editor, as the drawer's page is.

**Shared widget:** `DeEsserHistory` becomes a general `fancy::ui::LevelHistory` in `fancy-core`, with its own column type (input and output peaks, reduction left and right, threshold, beat and bar marks). DS and DS-Mini switch to it with their look unchanged, and their screenshots must match. MultiComp, Gate and Limit can use it in later items.

**Data:** the audio thread makes one column per 1.5 s / 476 (about 3.2 ms; peaks and the largest reduction over that time) into a lock-free FIFO, as the de-esser engine does, with a beat or bar mark when the host position crosses one in that column. The editor keeps 12 s (3 808 columns) and merges 2, 4 or 8 into one pixel for the longer zooms. Nothing is allocated on the audio thread.

### 6. Parameters and state

Appended after `scHighGain`, so no ID or index moves:

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `hold` | Hold | 0 to 500 | 0 | ms | skewed, as Attack |
| `range` | Range | 1 to 40, top = Off | Off | dB | linear |

Range shows and types "Off" at the top of its travel, as Dry shows −inf at the bottom. Both are shared `Knob`s on the Options page (mockup, section 2), so typed values come with them. **State:** `stateVersion` doesn't change. A state without the two IDs loads their defaults, which are today's path, so v1.6.0 projects load and render bit-identically. No new factory presets.

### 7. Tests (Catch2)

1. **Compatibility:** `tests/LegacyCompQ25Dsp.*`, `CompDsp` as released in v1.6.0, renders identically to the new `CompDsp` at Off, 2x, 4x and 8x, in the six existing setting variants plus Character, Upward and Auto threshold on. A state stripped of `hold` and `range` renders identically to the instance that saved it.
2. **Shared follower:** MultiComp's, Gate's and FIR's existing legacy and render tests pass unchanged.
3. **Hold:** a 0 dBFS burst into ∞:1, then −40 dBFS. With Hold 100 ms the reduction stays within 0.1 dB of its peak for 100 ms (±1 ms) and then releases; with Hold 0 it releases at once.
4. **Hold and low-note distortion:** a 50 Hz sine 12 dB over threshold, RMS size 0, Attack 1 ms, Release 20 ms. The THD with Hold 30 ms is at least 10 dB under Hold 0.
5. **Range:** ∞:1, Threshold −40 dB, a 0 dBFS sine. The reduction equals Range within 0.1 dB at 6, 12 and 24 dB, and is 40 dB at Off. Auto make-up follows Decision 1.
6. **Listen to removed, null test:** at Oversampling Off, the normal output plus the removed signal equals the delayed input within −120 dBFS. At 4x, it is within −100 dBFS of the input rendered through the same oversampler at 1:1.
7. **Two listens:** switching one on switches the other off; closing the editor turns both off; neither changes the saved state.
8. **History:** columns arrive at 476 per 1.5 s at 44.1, 48 and 96 kHz and every block size; merged zooms keep each column's peaks; the peak read-out equals the largest reduction in the span; beat marks land within one column of the host's beat at 120 BPM; DS's and DS-Mini's screenshots are unchanged.
9. **Automation and host:** the existing bit-identical automation test and H's offline-render and flood tests cover the two new parameters.
10. **CPU (§0.4):** Comp at its defaults stays at 1.00 × itself within noise, and Hold, Range and the history together add under 0.1 % at 48 kHz/128 samples (`docs/measurements/comp-w1.txt`).

### 8. Build order

One push with everything: the follower's Hold, Range, the listen target, `LevelHistory`, the drawer page, the footer and the tests. Then the screenshots (`comp`, `comp-options`, `comp-history`, `comp-listening`), the manual, the checklist and the CPU figures. Before the push, a Python model of test 4 (Hold's distortion) to set its margin. Then the REAPER check, then the next plugin of Wave 1, EQ.

### 9. As built

Push 1 (`2991726`) holds the whole item. The model is `docs/research/comp-w1-model.py`: at 50 Hz, 12 dB over a 4:1 threshold with Attack 1 ms and Release 20 ms, Hold 0 gives −26.6 dB THD and Hold 10 or 30 ms −134 dB. Once Hold is longer than the period the gain is flat between peaks, so test 4 asks for 30 dB less and under −60 dB.

- **Hold:** `Ballistics::setHold` and the follower's countdown in `fancy/dsp/Compression.h`. Comp's lift uses its own copy of the ballistics with Hold at 0 (`upBallistics`).
- **Range:** the parameter runs 1 to 41 dB, and 41 shows and types as "Off"; the DSP gets 0 for Off. Moves are smoothed over 20 ms, and switching it on starts at its value.
- **Listen to removed:** `FancyProcessor::setListenTarget` and `ListenCheckbox`'s optional target; Comp's Preview filter is target 0. The removed signal fades in and out over 10 ms, at the oversampled rate and at the base rate where Wet, Dry and Output are bypassed.
- **History:** `fancy/dsp/LevelHistory.h` (the recorder, at the oversampled rate, so it keeps time at every oversampling) and `fancy::ui::LevelHistory`, plus `fancy::ui::ToggleCheckbox` for Grid. **Changed from section 5:** DS and DS-Mini keep `DeEsserHistory`. Its column type is the de-esser engine's, and moving two released plugins over would only add risk to this item; the move can wait for the first item that needs one widget for both. The caption reads "paused - click to go on" while paused.
- **Footer:** x 24, 167, 314, 453 and 624, as measured in the mockup.
- **Tests:** `tests/CompW1Tests.cpp`; `tests/LegacyCompQ25Dsp.*` is `CompDsp` at v1.6.0.

## Phase 4 Wave 2: tempo-synced trigger

**Approved by the user on 21 November 2026 with every recommended option (A).** Backlog item 9 of `docs/enhancements.md` section 3, next in the Wave 2 order (Gate's follows it and Kicker's Duck will use the same trigger). Comp ducks on the host's beat grid with no sidechain kick, as Pro-C 3's Host Sync does. Modelled first: `docs/research/comp-sync-model.py`.

### 1. What it does

A new Detector input choice, **Host sync**, appended after Side. The detector then hears a **virtual kick** instead of audio: a 0 dBFS hit at every step of the host's grid, decaying to −60 dB over **Shape** of the step. Threshold, Ratio, Knee, Attack, Release, Hold and Range act on it exactly as on a real sidechain kick, so the ducking is shaped with the controls Comp already has.

- `syncRate`: 1/1, 1/2, 1/4, 1/8, 1/16, each also dotted and triplet; default 1/4.
- `syncShape`: 5 to 100 % of the step, default 25 %: how long the virtual kick lasts.
- The trigger is `fancy::dsp::TempoTrigger` in `fancy-core`, so Gate and Kicker can share it.

### 2. What the model found

- **Hits must be found by step index, not by comparing positions.** Comparing each sample's grid position with the one before can fire twice at one step from floating-point rounding (samples 87120 and 87121 in the model, for some block sizes). Keeping the last step index fired, and firing only when it goes up, lands every hit on the grid sample at every block size (1, 64, 441, 512, 4096).
- **Release shapes the duck more than Shape does.** At 120 BPM, quarter notes, Ratio 8 and Release 120 ms, Shape 10 % and 50 % differ by 3 dB a quarter of the way through the step (5.6 against 8.6 dB). Shape matters most with short Releases. The manual will say so.

### 3. Decisions

- **Decision 1: what drives the ducking.**
  - **A (R).** The virtual kick into the detector, as above: every Comp control keeps its meaning, and Hold, Range and the GR history all work with it.
  - **B.** A gain curve drawn directly, like a volume-shaping LFO: exact shapes, but Threshold, Ratio, Attack and Release stop meaning anything, and depth becomes a new knob.
- **Decision 2: when the transport is stopped.**
  - **A (R).** The trigger keeps running at the last tempo from where it stopped, so you can set the duck up while the song is stopped. Playback re-aligns it to the host's position at once. Offline renders always play, so they stay repeatable.
  - **B.** No hits while stopped: nothing ducks until you press play.
- **Decision 3: where Rate and Shape go.**
  - **A (R).** On the Options page, beside Hold and Range: a Rate dropdown and a small Shape knob, shown while the Detector input is Host sync. The main layout doesn't change. A mockup follows approval.
  - **B.** On the main window, in the RMS size knob's place while Host sync is chosen: faster to reach, but it changes a control's place on the main layout.

### 4. Details

- The virtual kick goes into the detector after the Detector EQ and the Highpass and Lowpass, which are for audio. It's the same for both channels, so Link doesn't matter for it.
- Pre-comp delays the audio and not the virtual kick, so the duck starts Pre-comp ahead of the beat, as it would with a real kick.
- The "No sidechain signal" notice doesn't show for Host sync. Without host tempo (a host that sends none), the trigger runs at 120 BPM and the Options page says so.
- No latency. `stateVersion` doesn't change: the new choice is appended, and the two parameters have defaults.

### 5. Tests

1. Legacy: every other Detector input renders as v1.6.0 (and as Wave 1).
2. Hits land on the grid to the sample at 44.1, 48 and 96 kHz, for block sizes 1, 64, 441, 4096 and random sizes, with no double hits; a tempo change mid-render moves the grid from that point.
3. With Ratio 8 and Threshold −20, gain reduction at each hit is within 0.5 dB of a real sidechain kick of the same shape fed through the Sidechain input.
4. Rate 1/8 dotted at 120 BPM hits every 375 ms.
5. Stopped transport (Decision 2 A): hits continue at the last tempo; play re-aligns them to the host's bar.
6. Offline renders of the same project match bit for bit.

### 6. Build order

Approval of this note, then the Options page mockup (`reference/phase4/43__comp_w2_mockup.png`, **approved by the user on 21 November 2026**), then one push: `TempoTrigger`, Comp's Host sync, the tests, the manual and the checklist. Then Gate's, with the same trigger, as its own note.

### 7. As built

One push, with Decisions 1 to 3 all A.

- **The trigger:** `fancy::dsp::TempoTrigger` (`fancy-core/include/fancy/dsp/TempoTrigger.h`, header-only). Comp gives it the host's position at each block's start (the transport the GR history already reads). Hits fire when the step index goes up; a jump of more than two samples from where it expected to be (play, a loop, a seek) re-aligns it, firing at once if the position is on a step. While stopped it runs on from its own position at the last tempo, whatever the stopped host reports; without host tempo it runs at 120 BPM.
- **Comp:** `hostSync` is appended to the Detector enum and choice; the virtual kick fills the key buffer at the base rate (both channels) and goes through the key's oversampler, then skips the detector filters and EQ. `syncRate` is 15 choices (1/1 to 1/16, each plain, dotted and triplet), 1/4 by default; `syncShape` 5 to 100 %, 25 %.
- **Options page:** the HOST SYNC group as mockup 43 places it (drawer coordinates: caption 28, 237; Rate 28 and 72, 257; Shape 196, 237; note 28, 289), shown while Host sync is chosen and the Options tab is open. The note reads "No host tempo: 120 BPM" without host tempo. Screenshot `comp-sync`.
- **Tests:** `tests/CompW2Tests.cpp`.

## Phase 4 Wave 2: Optical mode

**Approved by the user on 21 November 2026 with every recommended option (A).** Item 6 of the Wave 2 order (`docs/enhancements.md`; backlog item 10 of section 3, added by the user after the Waveform Compressor comparison). A third ballistics mode beside the default follower and Classic, behaving like an optical cell. Ballistics only: no tube colour or other "style model". Modelled first: `docs/research/comp-optical-model.py`.

### 1. What it does

A new follower in `fancy::dsp` (so MultiComp could offer it later), used instead of the default one, in dB of gain reduction:

1. **Attack follows the signal:** the further over the threshold, the faster it closes, at Attack's rate × (over ÷ 6 dB), held between 0.5 × and 4 ×.
2. **Two-stage release:** half the reduction lets go at Release ÷ 10, the rest at Release.
3. **Memory:** a memory value charges while more than 3 dB is taken off (time constant 2 s) and drains back (6 s). The slow stage's time is Release × (1 + 3 × memory), so after long heavy compression it recovers up to 4 × more slowly, as a cell does.

### 2. What the model found

With Attack 10 ms and Release 500 ms at 48 kHz:

| | Optical | Default |
|---|---|---|
| Attack to 63 %, 3 dB over | 20.0 ms | 10 ms |
| 6 dB over | 16.1 ms | 10 ms |
| 12 dB over | 8.6 ms | 10 ms |
| Back to 1 dB after a 0.3 s burst of 12 dB | 1.7 s | 1.2 s |
| after a 6 s burst of 12 dB | 3.3 s | 1.2 s |

So peaks are caught faster and gentle overs more gently, and a long squash recovers slowly, without the knobs changing. At 6 dB over the attack is 16 ms rather than the knob's 10 ms, because it slows as it closes in. The manual will say Attack is the time "at about 12 dB over".

### 3. Decisions

- **Decision 1: how it's switched on.**
  - **A (R).** An **Optical** checkbox, `optical`, appended (off). While it's on, Classic and Auto release dim and don't apply: Optical has its own release. Weird knee, Hold and Range still work.
  - **B.** A "Ballistics" choice (Default, Classic, Optical) replacing the Classic and Auto checkboxes. Clearer, but it changes the main layout and the meaning of saved Classic values, so it would need a migration.
- **Decision 2: where the checkbox goes.**
  - **A (R).** On the Options page, under "Attack = 90 % rise", where it sits with the other ballistics options. The main layout doesn't change. A mockup follows approval.
  - **B.** In the main window's row of checkboxes, beside Classic: easier to find, but the row has no room without moving the others.
- **Decision 3: the memory's strength.**
  - **A (R).** Fixed at up to 4 × as modelled: one switch, nothing more to learn.
  - **B.** A **Memory** knob (0 to 100 %, default 50) on the Options page, for those who want a stickier or freer cell.

### 4. Tests

1. Legacy: Optical off renders as v1.6.0 (and as Wave 1) for every other setting.
2. Attack: a step 12 dB over reaches 63 % in 8.6 ± 1 ms; 3 dB over in 20 ± 2 ms (Attack 10 ms).
3. Release: after a 0.3 s and a 6 s burst of 12 dB reduction, back to 1 dB in 1.7 ± 0.2 s and 3.3 ± 0.3 s (Release 500 ms).
4. Classic and Auto release have no effect while Optical is on; Hold and Range still do.
5. Block-size independence and bit-identical offline renders, as for every follower.

### 5. Build order

Approval, then the Options page mockup (`reference/phase4/46__comp_w2_optical_mockup.png`), then one push: the follower in `fancy-core`, Comp's `optical`, tests, manual and checklist.

### 6. As built

Mockup 46 approved by the user on 22 November 2026; `tests/CompOpticalTests.cpp`.

- `fancy::dsp::OpticalFollower` (`Compression.h`) is the model's follower, with Attack = 90 % rise and Hold as the default follower has them. Comp keeps one per channel; with `optical` (appended, off) the non-Classic branch uses it and Classic is skipped. Each follower's state tracks the other's reduction, so switching Optical on or off doesn't jump.
- The Options page's Optical checkbox sits under Attack = 90 % rise, with its note below while Host sync isn't the Detector input. Host sync's group (mockup 43) moved down 37 px to make room. Classic and Auto dim on the main window while Optical is on.

## Phase 4 Wave 2: six-band sidechain EQ and steeper cuts

**Approved by the user on 22 November 2026 with every recommended option (A).** Item 13 of the Wave 2 order (`docs/enhancements.md`; backlog item 7 of section 3), for v1.7.1. Modelled first: `docs/research/comp-sceq-model.py`.

### 1. What it adds

The detector EQ grows from three fixed bands to six, each with a type, and Highpass and Lowpass get steeper slopes. All of it acts only on what the detector hears, never on the audio.

- **Six bands.** Each has a **Type**: Off, Low shelf, Bell or High shelf, and Freq, Gain and Q (Q for the bell). Bands 1 to 3 default to today's low shelf, bell and high shelf, with their own parameters unchanged. Bands 4 to 6 are new and default to Off. A band set to Off, or at 0 dB, isn't run, so saved projects stay bit-identical.
- **Cut slopes.** `hpSlope` and `lpSlope`: 12 dB/oct (today, the default), 24 or 48, as cascades of Butterworth sections at the same frequency, as DS's Filter slope does. The Highpass and Lowpass knobs stay on the main window.

### 2. What the model found

The model is a mix bed at −18 dBFS RMS with a 55 Hz kick; Comp runs at 4:1 with Threshold −20 dB. Each figure is how far the gain reduction jumps on every kick:

| Detector high-pass | Kick −6 dBFS, cut at 150 Hz | Kick −2 dBFS, bass at 98–131 Hz kept, cut at 90 Hz |
|---|---|---|
| none | +4.1 dB | +4.8 dB |
| 12 dB/oct (today) | +0.1 dB | +1.2 dB |
| 24 dB/oct | −0.1 dB | +0.2 dB |
| 48 dB/oct | −0.1 dB | −0.1 dB |
| 96 dB/oct ("brickwall") | −0.1 dB | −0.1 dB |

Today's slope is enough when the cut can sit well above the kick. When it has to sit close, to keep a bass line driving the compressor, 24 or 48 dB/oct stops the pumping. A brickwall slope adds nothing beyond 48, so it's left out (decision 2).

### 3. Decisions

- **Decision 1: the bands.**
  - **A (R).** Six bands with types Off, Low shelf, Bell and High shelf. Bands 1 to 3 default to today's three, and the Highpass and Lowpass cuts stay separate, on their knobs.
  - **B.** Six fully general bands that include the cuts, with the main window's Highpass and Lowpass moved into them. Q2.5 turned this down: old projects would change slightly, and the main layout would too.
- **Decision 2: cut slopes.**
  - **A (R).** 12, 24 and 48 dB/oct, as the model justifies.
  - **B.** Add a 96 dB/oct "Brickwall" as the backlog named it. It measures the same as 48.
- **Decision 3: the Detector page.**
  - **A (R).** Band tabs 1 to 6 over the response curve, which has a draggable handle per band. Under the curve sit the selected band's Type dropdown and its Freq, Gain and Q knobs, with the two slope dropdowns beside them. Auto threshold stays where it is. Only the drawer changes; a mockup follows approval.
  - **B.** All six bands' knobs at once: 18 knobs, too small to use on one page.

### 4. Tests

1. Legacy: bands 4 to 6 Off and 12 dB/oct slopes render exactly as v1.7.0, with every detector input and every page's settings.
2. Types: a band set to Bell at +12 dB and 3 kHz raises a 3 kHz tone's detector level by 12 ± 0.5 dB over a 300 Hz tone; Low shelf and High shelf behave likewise below and above their frequencies.
3. Slopes: one octave below Highpass, 24 dB/oct is 12 ± 1 dB further down than 12 dB/oct, and 48 dB/oct is 36 ± 1.5 dB further down; the −3 dB point doesn't move.
4. The model's case 2: at 24 dB/oct the reduction jumps by under 0.5 dB on each kick, against over 1 dB at 12.
5. Preview filter plays the whole detector chain, the new bands and slopes included.
6. CPU: a typical use of what this item adds, one more band and a 24 dB/oct Highpass, over the most v1.7.0 could run (bands 1 to 3 on), costs under 15 % of Comp at its defaults. All six bands with 48 dB/oct on both cuts is recorded, not held to a target. **Changed by the user on 23 November 2026**, after the first measurement: the full load added 73 % (0.494 % to 0.856 %), about 3 ns per filter section per sample, which no design of this kind brings near 15 %.

### 5. Build order

Approval, then the Detector page mockup (`reference/phase4/55__comp_w2_sceq_mockup.png`), then one push: the bands and slopes in `CompDsp`, the parameters, the Detector page, tests, manual and checklist. Mockup 55 (with item 14's Options page) approved by the user on 23 November 2026.

### 6. As built

One push with item 14 (one plugin per push).

- **Parameters,** appended after `optical`: `scLowType`, `scMidType`, `scHighType` (Off, Low shelf, Bell, High shelf; by default today's shapes), `scLowQ` and `scHighQ` (0.3 to 8, 1.0; used only as a Bell), then for bands 4 to 6 `sc4Type` to `sc6Type` (Off), `sc4Freq` to `sc6Freq` (20 Hz to 20 kHz; 200 Hz, 2 kHz, 5 kHz), `sc4Gain` to `sc6Gain` (±18 dB, 0) and `sc4Q` to `sc6Q`, and `hpSlope` and `lpSlope` (12, 24, 48 dB; 12). **Changed (user, 23 November 2026):** bands 1 to 3's Freq ranges widen to 20 Hz to 20 kHz, so any band can take any Type. States store hertz (snapped to whole hertz), so saved projects load and render as before; host automation of those three Freqs, which is stored normalised, now maps to different frequencies. A shelf's Q is Butterworth's, as before.
- **DSP:** `CompDsp::numDetectorBands` bands. A band keeps its shape while Off fades its gain to 0 over 20 ms, so switching Off doesn't click. The cuts are cascades of SVF sections at Butterworth Qs (`filterCuts`); at 12 dB/oct both, the old `DetectorFilter::filter` runs, to the bit. A new slope starts its added sections from rest.
- **Detector page** as mockup 55 places it: tabs inside the curve, the selected band's Type, Freq, Gain and Q under it (one set of controls per band, the selected one shown), Cut slopes to the right. The curve draws each band that isn't Off with its number. The HP label adds the slope when it's steeper than 12.
- **Tests:** `tests/CompSceqTests.cpp`; `tests/LegacyCompW2Dsp.*` is `CompDsp` at v1.7.0. Test 4 ports the model's case 2 (the model gives +1.23 to +1.25 dB at 12 dB/oct and +0.15 to +0.17 at 24 over five noise beds; CI measured +1.23 and +0.14).
- **CPU** (`docs/measurements/comp-w2-sceq.txt`, run 45159840182): one more band and a 24 dB/oct Highpass add 12.7 % of Comp at its defaults (target under 15 %); all six bands with 48 dB/oct cuts add 73 %, recorded. Mid/Side adds 8.9 % (item 14's target, under 10 %).

## Phase 4 Wave 2: link modes

**Approved by the user on 22 November 2026 with every recommended option (A).** Item 14 of the Wave 2 order (`docs/enhancements.md`; backlog item 8 of section 3), for v1.7.1. Modelled first: `docs/research/comp-linkmodes-model.py`.

### 1. What it adds

`linkMode`: how the two channels are detected and compressed.

- **Stereo** (today, the default): left and right, combined by Link channels and Link amount. Saved projects are unchanged, bit for bit.
- **Mid/Side:** the input is split into mid and side. Each is detected and compressed on its own, with Link channels and Link amount now acting between mid and side, and the result is turned back into left and right.
- **M>S:** the mid's level compresses only the side, and the mid passes untouched. The width narrows while the centre is loud, so a vocal steps forward.
- **S>M:** the side's level compresses only the mid.

The detector inputs keep their meaning. Main L+R and Sidechain L+R are split into mid and side in these modes. A single-channel input (L, R, Mid, Side, Host sync) drives the gain of whichever channels the mode compresses. Upward, Character, Auto threshold and the GR meter work per channel as now, with the meter's two bars reading M and S in these modes and labelled so.

### 2. What the model found

The model has a vocal in the mid, in phrases at −12 dBFS RMS, and guitars in the side at a steady −20. Comp runs at 4:1 with Threshold −24 dB. Gains are shown as singing / gaps:

| Mode | Mid | Side | Width moves with the vocal |
|---|---|---|---|
| Stereo (today, linked) | −9.4 / −5.5 dB | −9.4 / −5.5 dB | 3.9 dB |
| Mid/Side | −8.9 / −3.3 dB | −3.1 / −3.1 dB | 0.0 dB |
| M>S | 0 | −8.9 / −3.3 dB | 5.5 dB, on purpose |
| S>M | −3.1 / −3.1 dB | 0 | 0.0 dB |

In Stereo, a loud vocal turns the guitars down with it, so the width breathes with the singing. Mid/Side keeps the width steady while it controls the vocal. M>S makes the breathing deliberate.

### 3. Decisions

- **Decision 1: the modes.**
  - **A (R).** Stereo, Mid/Side, M>S and S>M, as above.
  - **B.** Stereo and Mid/Side only. M>S and S>M are specialised, though the model shows what they're for.
- **Decision 2: where it goes.**
  - **A (R).** A **Link mode** dropdown on the Options page, beside Link amount. The GR meter's labels read M and S when the mode isn't Stereo, and the main layout doesn't otherwise change.
  - **B.** A small Stereo | M/S switch on the main window by Link channels, with M>S and S>M only in the drawer.

### 4. Tests

1. Legacy: Stereo renders exactly as v1.7.0 with every detector input, linked and unlinked.
2. Mid/Side on a mono signal (side silent): the output equals Stereo's to −120 dB, and the side stays silent.
3. The model's mix: in Mid/Side the side's gain moves by under 0.5 dB between singing and gaps, while in Stereo it moves by over 3 dB.
4. M>S: the mid passes bit for bit, and the side is reduced by the mid's level. S>M likewise the other way round.
5. Sidechain L+R in Mid/Side: the sidechain's mid drives the mid, and its side drives the side.
6. CPU: Mid/Side adds under 10 % to Comp at its defaults.

### 5. Build order

Approval, then the Options page mockup (with item 13's), then one push: the modes in `CompDsp`, the parameter, the Options page and the GR labels, tests, manual and checklist.

### 6. As built

In item 13's push.

- **`linkMode`** (Stereo, Mid/Side, M>S, S>M; Stereo), appended after item 13's parameters. It is part of `CompDsp::Config`, so a change fades out and in, as an oversampling change does.
- **DSP:** in the M/S modes `compress` turns each sample into mid and side at the start, at the oversampled rate, and back at the end; Character, Upward and Range run per channel in that domain. The detector inputs are formed from left and right first; Main and Sidechain L+R are then split. The channel a mode leaves alone gets no reduction and no lift, but make-up and Character as the other does (**changed by the user, 23 November 2026**, from passing it untouched); at the defaults it passes as it came in. Auto threshold's power is the same in either domain (m^2 + s^2 = (l^2 + r^2) / 2). The history is drawn in left and right.
- **UI:** Link mode on the Options page at drawer (308, 293), as mockup 55 places it; `GainReductionMeter::setChannelNames` labels the read-outs M and S.
- **Tests:** in `tests/CompSceqTests.cpp` (link 2 to 5). Test 4's "bit for bit" is within -120 dB: the mid is rebuilt from left and right in float.

## v1.7.1: level history in dB

**Approved by the user on 23 November 2026 with every recommended option (A).** Asked for by the user on 23 November 2026, after a comparison with Woodstock Audio's Waveform Compressor. The History page (Wave 1, section 5) draws the waveforms on a linear scale, so anything under about −30 dBFS is a hairline at the centre, and a threshold of −24 dB sits 8 px from it. This note draws them in dB. Mockup: `reference/phase4/65__comp_db_history_mockup.png` (`design/phase4-mockup-src/w2b_mockups.py comp_db`): before, after, a quiet passage both ways, and Decision 1 B.

### 1. What changes

- **Waveforms:** each column's input and output peak is drawn at its level in dB, linear in dB from the floor at the centre line to 0 dB at the edges, mirrored as today. Silence and anything under the floor draw nothing.
- **Threshold:** the dashed pair sits at the threshold's level on the same scale (−24 dB with a −60 floor: 60 % of the way out, against 6 % today).
- **Level lines:** faint lines every 12 dB (white at 7 %), with their values in grey at the left edge, drawn over the waveforms with a dark outline so they stay legible.
- **Unchanged:** the GR line from the top (0 to 24 dB, yellow scale on the right), zoom, Grid, pause, peak read-out, the legend, the colours, the page layout and the window size.
- On this scale the gap between the grey and teal edges *is* the gain reduction, at 3.5 px per dB (with a 60 dB floor), so the waveforms and the GR line now tell the same story.

### 2. Decisions

- **Decision 1: the shape.**
  - **A (R).** Mirrored about the centre line, as today: still reads as a waveform, the picture the user approved in Wave 1; only the scale changes (mockup, sections 2 and 3).
  - **B.** One-sided, level up from the bottom (as Pro-C draws it): twice the height per dB and one threshold line, but it stops looking like a waveform, loud peaks run into the GR line from the top, and the legend sits on the waveform (mockup, section 4).
- **Decision 2: the floor.**
  - **A (R).** Fixed at −60 dB: room tone and breaths show, and 12 dB steps give five clean lines.
  - **B.** A small `30 | 60 | 90 dB` switch on the zoom row, remembered per editor like Zoom. More control, one more thing on the page.
- **Decision 3: keep a linear view?**
  - **A (R).** No: dB only. The linear view hides exactly what a compressor works on, and a switch would only add a control.
  - **B.** A `dB` checkbox beside Grid, on by default, so linear stays one click away.

### 3. Code and state

A paint change in `fancy::ui::LevelHistory` only (`fancy-core/src/LevelHistory.cpp`): one mapping function from peak to distance, used for the waveforms and the threshold, plus the level lines. The recorder, `LevelColumn`, the FIFO and the audio thread don't change. No parameter, nothing saved, no `stateVersion` change, so projects load and render bit-identically. DS and DS-Mini keep `DeEsserHistory` and are untouched; MultiComp's GR history (Wave 2) has no waveforms. The tooltip becomes "Input in grey and output in teal, in dB (−60 at the centre), before make-up; gain reduction in yellow. Click to pause."

### 4. Tests

1. The mapping: 0 dBFS reaches the edge, the floor and silence reach the centre, −30 dB is half-way (with a −60 floor), within 0.5 px.
2. The existing History tests (column rate, merged zooms, peak read-out, beat marks) pass unchanged.
3. The `comp-history` screenshot is re-rendered and compared with mockup 65; DS's and DS-Mini's screenshots are unchanged.
4. Comp's legacy and automation tests pass unchanged (nothing in the DSP moves).

### 5. Build order

Approval of this note and mockup 65, then one push: the paint change, the tooltip, test 1, the `comp-history` screenshot, the manual's History section (the scale, with the new screenshot) and a line in `docs/comp-checklist.md`. Then the REAPER check, then the next item of `docs/enhancements.md`.

### 6. As built

One push. `fancy::ui::LevelHistory::levelDistance` (with `floorDb` = −60) maps a peak to its distance from the centre line and is used for the waveforms and the threshold; the level lines go under the waveforms and their labels over them, outlined in the well colour. The threshold pair is hidden when the threshold is at or under the floor. Test: "the history draws levels in dB…" in `tests/CompW1Tests.cpp`.

## Phase 4 Wave 3: History on the main page

**Layout C1 of mockup 77 approved by the user on 27 November 2026; this note waits for approval.** Asked for by the user on 27 November 2026 as a late addition to Wave 3 (v1.7.2): the waveform from EDIT > History on the main page, "so people can see how turning the knobs directly affects the audio waveform". Mockup 76 placed it (A, above the knobs, chosen; B, below); the user then asked for smaller knobs, with Woodstock Audio's Waveform Compressor (a big waveform over a compact knob row) as the idea, and chose C1 of mockup 77. Mockups: `reference/phase4/76__comp_history_main_mockup.png` and `77__comp_history_small_knobs_mockup.png` (`design/phase4-mockup-src/w3_mockups.py`, `comp-w3` and `comp-w3-small`).

**A standing rule set aside, by the user's choice:** "never shrink existing controls" (23 November 2026). Here the user asked for the knobs to shrink, on Comp's main page only; no other control shrinks, and the window still grows.

### 1. What changes

- **The HISTORY card** goes at the top of the centre column, where the knob cards began: today's History page whole (the waveforms in dB, the GR line, the dashed threshold, the legend, the 1.5 / 3 / 6 / 12 s zoom, Grid, the peak read-out that clears on a click, and click to pause), with its graph 500 x 278 px against the page's 476 x 260.
- **Every main-page knob gets smaller:** Ratio, Knee, Attack, Release, Pre-comp, RMS size, Lowpass and Highpass go from `Knob::mediumStyle` (72 px dial) to the shared `Knob::smallStyle` (50 px, as the EDIT drawer's knobs). Each keeps its label, its value (click it, or Ctrl-click the knob, to type one), its arc, its colour and its default mark. The cards keep their grouping: Ratio to Release with Weird, Classic and Auto under Knee, Attack and Release; Pre-comp and RMS size; Lowpass and Highpass in their blue card.
- **The window grows 200 px taller**, to 934 x 747 (its minimum too). The Detector row, EDIT, the Threshold and GR meters, Out, Wet and Dry stay in their columns; the meters and faders lengthen with the window, and the footer moves down 200 px.
- **Unchanged:** every parameter, the DSP, the presets and the state; the history's recorder, its columns and its drawing.

### 2. Layout (body coordinates at 100 %, under the 73 px header)

| Part | Position and size |
|---|---|
| HISTORY card | x 212, y 50, 524 x 324: HISTORY at (224, 61); zoom switch (300, 57, 176 x 22); Grid (492, 60); peak right-aligned in (600, 61, 124 x 14); graph (224, 86, 500 x 278) |
| Ratio to Release card | x 212, y 380, 524 x 120: knobs centred at x 277.5, 408.5, 539.5 and 670.5, labels at y 386; Weird, Classic and Auto at y 477 under Knee, Attack and Release |
| Pre-comp and RMS size card | x 212, y 506, 260 x 118: knobs at x 277 and 407, labels at y 520 |
| Lowpass and Highpass card | x 478, y 506, 258 x 118: knobs at x 542 and 672 |
| Threshold, GR, Out, Wet, Dry | as now, 200 px longer; their scales respaced evenly over the longer wells |
| Footer | 200 px lower |

### 3. Decisions

- **Decision 1: EDIT > History.**
  - **A (R).** The page goes: the drawer's switch keeps Detector, Character, Upward and Options. The waveform is always in view (decision 3), so a second copy would only be a fifth page to find.
  - **B.** Keep it as well, showing the same history at the page's size.
- **Decision 2: hiding the card.**
  - **A (R).** Always shown: it's what the main page is for now, and a show and hide switch would change the window's height or leave a hole.
  - **B.** A History checkbox in the footer that folds the card away and gives the knob cards back their medium knobs: two layouts to keep right, and a window that changes height.
- **Decision 3: the EDIT drawer.** Its pages are laid out for 374 px, the knob cards are now 244 px tall, and nothing in the drawer may be squeezed.
  - **A (R).** The drawer keeps its size and opens over the lower 374 px of the centre column (y 250 to 624), and the HISTORY card shortens to the 194 px above it: the graph becomes 500 x 152, mockup 76 A's size, and comes back to 278 when the drawer closes. The waveform stays whole and in view while you set Character, Upward or the detector's EQ, which change it too.
  - **B.** The drawer covers the whole centre column (574 px), as it covers the knobs today: the history is hidden while it's open, and its pages have 200 px of empty space under them.

### 4. Code and state

Editor only (`CompEditor`): the layout constants, `Knob::smallStyle` for the eight knobs, the history widgets moved from the drawer's History page to the body, and (decision 3 A) the card's height following the drawer. `DrawerTabs` loses `history` (decision 1 A); the drawer's page isn't saved, so nothing stored points at it. The editor already reads the history every tick, open or not; the card now repaints at the editor's timer rate whenever the window is open, on the message thread only. Zoom, Grid and pause stay per editor, not saved. No parameter and no `stateVersion` change: projects load and render bit-identically. `fancy::ui::LevelHistory` scales its columns to its width, so 500 px needs no change to it or to the recorder.

### 5. Tests

1. The editor opens at 934 x 727 plus the title strip, and its minimum height is the same.
2. The eight main-page knobs use `Knob::smallStyle`; every control on the page sits inside the window and no two overlap, with the drawer closed and open.
3. The history graph is 500 x 278 with the drawer closed and 500 x 152 with it open; the drawer is 524 x 374 at y 250; the drawer's switch has four pages.
4. The existing History tests (column rate, merged zooms, the peak read-out, beat marks, the dB mapping) pass unchanged.
5. Comp's legacy, automation and render tests pass unchanged (nothing in the DSP moves).
6. Screenshots: `comp` against mockup 77 C1; `comp-edit` and the drawer pages with the history above the drawer; `comp-history` goes, and `comp` takes its place in the manual.

### 6. Build order

Approval of this note, then one push: the layout, the moved history, the drawer, tests 1 to 3, the screenshots, the manual (History moves from "EDIT drawer" to its own section beside the main controls; the drawer has four pages), `tools/manual/content.py` (the `comp-history` view goes; the History section uses the `comp` shot) and `docs/comp-checklist.md`. At the release, the site's Comp views and window size.
