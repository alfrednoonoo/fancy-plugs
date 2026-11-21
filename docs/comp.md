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
