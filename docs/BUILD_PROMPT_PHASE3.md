# Phase 3.0: close the gaps to the market leaders

> **Trimmed 9 November 2026.** Fancy Gate (Phase 2.0.1) and Phase 2.1 came before this phase and are released as v1.2.0. Phase 2.1 built four items this draft once had, so they're removed below: undo and redo, A/B and level-matched Compare (Q2.1), dynamic EQ (Q2.4), MultiComp's upward compression (Q2.6) and FIR's adaptive profile (Q2.7). The suite has nine plugins, Fancy Gate included. This is still a draft until the user approves it.

This extends `docs/BUILD_PROMPT.md` and `docs/BUILD_PROMPT_PHASE2.md`. **Every rule in them still applies**, including:

- cloud builds only, with nothing claimed until the Actions run is green;
- the design system and the shared `fancy-core` components;
- the real-time rules (§4) and the acceptance checks (§7 and P5);
- typed values on every knob and fader;
- "ask before changing the parameter list or layout";
- Phase 2.1's rules: appended parameters with defaults that keep the old sound, `stateVersion` migrations, and the §0.4 CPU method (median of five runs, ratio to Fancy Comp at its defaults).

The evidence behind this phase is in `docs/research/2026-11-market-report-summary.md` and `2026-11-market-report-detailed.md`. Read the summary first, including its "Phase 2.1 results" section. Phase 3 starts when the user approves this brief.

## Q0. Decisions for the user before starting

Ask these together, once, before writing any code. Record the answers in `docs/PROGRESS.md`.

**Answers (7 November 2026):**

- **Platform:** Windows first, until it's a concrete, finished version. Mac is wanted after that, as a later phase.
- **New plugins:** a limiter, a de-esser and a saturator. The user's main concern is that their design matches the suite, so every new plugin goes through Q5 first. The others (transient shaper, chorus or modulation, algorithmic reverb) are for Phase 4.
- **Order:** the user approved the order in Q2's preamble. They use the EQ, Comp and Verb most, so those are improved first.

Still open, to ask together before any code:

1. **EQ Match** (Q2.4): keep it in Phase 3 or drop it.
2. **The later plugin order** (Q2, step 3.4): confirm once EQ, Comp and Verb are done.

## Q1. The benchmark host (first, before any DSP work)

The research couldn't compare our plugins with the leaders head to head, because they can't run in CI. Build the tool that lets the user do it on their own PC.

**`tools/bench`**, built in CI and shipped as `FancyBench.exe` in the artifact, outside the plugin bundles.

- **What it is:** a console or small-window JUCE app that loads any VST3 by path, using `juce::VST3PluginFormat`. It must never ship or copy third-party binaries.
- **What it measures, at chosen rates and block sizes:**
  - CPU: mean, 99th percentile and the worst block, timed at real time and at high thread priority, as the Verb benchmark does.
  - The latency the plugin reports, against its measured impulse delay.
  - Frequency response, from a sweep or noise transfer function.
  - THD and THD+N at chosen levels.
  - Aliasing, from a high-frequency sine at 0 dBFS through a nonlinear plugin.
  - A null test against a delayed dry signal, for plugins set to neutral.
  - Click detection under fast automation of a chosen parameter.
- **How it's driven:** the user picks parameters by name or index, and sets their values in a small JSON test file. That way "Pro-C 3 at 4:1, -20 dB" and "Fancy Comp at 4:1, -20 dB" run the same test.
- **What it writes:** Markdown and CSV into `bench-results/`, with plots as PNGs.
- **Tests:** CI runs it against our own nine plugins as a smoke test, and checks the numbers match `docs/measurements` within the runners' variation. The user runs it on their PC against plugins they own or trial.
- **Docs:** `docs/bench.md`, a how-to with three ready-made comparison files: compressor, EQ and delay.

## Q2. Improvements, in order

Order, approved by the user:

1. The benchmark host (Q1), which gives a baseline before anything changes.
2. The shared features (Q2.1) and the design rules (Q5), so every later change is built on them once.
3. Improvements to the existing plugins, one at a time, each signed off in REAPER, in this order:
   1. EQ (Q2.4)
   2. Comp (Q2.5)
   3. Verb (Q2.8)
   4. then MultiComp (Q2.6), Delay (Q2.3), FIR (Q2.7) and Kicker (Q2.2). Confirm this part with the user when the first three are done.
4. New plugin types, each designed through Q5 before any code: Fancy Limiter, Fancy De-esser and Fancy Saturator. Before the first, write a P2-style section for each, with its DSP, parameters, known problems with existing tools and their tests, and get it approved.
5. Mac, as its own phase.

The sections below keep their numbers. Follow the order above, not the numbering.

Each item needs its design-note section, in `docs/<plugin>.md`, written and approved first, because each changes the parameter list or layout. New controls go in EDIT drawers or popovers unless the user approves a layout change. Every new knob and fader uses the shared components, so typed values come with them.

### Q2.1 Shared, in `fancy-core`, for all nine plugins

Undo and redo, A/B and level-matched Compare are done (Phase 2.1 W). The preset browser's preset loads must stay single undo steps, as W made them.

| Feature | Detail | Test |
|---|---|---|
| Preset browser | Categories, search, favourites, and next and previous arrows. The factory presets get categories | Loads and saves round-trip. A damaged preset file is ignored |
| Free resizing | Every editor resizes between its minimum and 200 %, as Fancy EQ does now | The layouts render at the minimum and maximum without clipping, checked from the CI screenshots |

### Q2.2 Fancy Kicker: create bass, not only boost it

The research found the category is bought for two things the Kicker can't do.

- **Sub synth mode:** a subharmonic generator an octave below the detected fundamental within 30-120 Hz.
  - Use a pitch tracker on the band-limited signal, and a sine oscillator whose amplitude follows the source's envelope.
  - Glide and an amplitude threshold avoid warbling on noise.
  - Test: a 100 Hz kick produces a 50 Hz sub within ±1 Hz. A source below the threshold produces nothing.
- **Harmonics mode:** psychoacoustic bass. Generate 2nd to 5th harmonics of the band below Freq, through a waveshaper with anti-aliasing, so the bass carries on small speakers.
  - Test: harmonics at the expected ratios. Aliasing is below -90 dBFS at 4x oversampling.
- **Duck:** an optional sidechain input ducks the added sub when a kick hits on the sidechain.
- The existing boost stays as the first mode, with the same sound.

### Q2.3 Fancy Delay: character

- **Modulation:** wow and flutter on each tap: rate, depth and a random amount. It goes through the existing Hermite read.
  - Test: the measured pitch deviation matches the depth, and depth 0 is bit-exact.
- **Character:** Clean, which is today's sound, plus Tape and BBD.
  - Tape: soft saturation, a head bump and high-frequency loss with each repeat.
  - BBD: band-limiting with clock noise and companding.
  - Each is an original model, not a recreation of a named product.
- **Diffusion:** an all-pass diffuser inside the loop, from 0 to 100 %.
- **Ducking:** the wet signal ducks while the input is loud, with threshold, amount and release.
- **Groove:** swing for Note-mode taps.
- **CPU:** the "everything on" figure for 4 taps must come below 2 %. Saturate's `tanh` is the main cost, and an approximation is allowed if it holds the 0 dBFS guarantee.

### Q2.4 Fancy EQ: mid/side and more bands

Dynamic bands are done (Phase 2.1 D).

- **Channel per band:** Stereo, Left, Right, Mid or Side, dynamic bands included (their detector reads the same channel).
- **Up to 24 bands.** Bands 17-24 are appended with their dynamic parameters, and `stateVersion` is bumped from 3 with a migration, so 16-band states load unchanged.
- **EQ Match:** learn a reference spectrum, from the sidechain or a captured section, and fit the bands to it. Optional, and only if Q0 doesn't drop it.

### Q2.5 Fancy Comp: character and upward compression

- **Character:** a saturation stage, pre or post, with Warm and Bright original curves and a drive control. It runs at the oversampled rate.
- **Upward compression:** reuse MultiComp's upward stage from Phase 2.1 U, with the same controls (Up threshold, Up ratio, Max lift, Floor) and the same static curve, moved into `fancy-core` so both plugins share it.
- **Auto threshold:** keeps the gain reduction steady across input level changes.
- **Sidechain EQ:** a 3-band EQ on the detector, replacing the high-pass and low-pass. It reuses Fancy EQ's matched filters.
- **Oversampling to 8x.**

### Q2.6 Fancy MultiComp: lookahead, mid/side and CPU

Upward compression, Depth, Time and the "Big squash" preset are done (Phase 2.1 U).

- **Lookahead:** 0-10 ms, reported as latency.
- **Mid/side per band.**
- **CPU:** 4 bands in LR 24, about 2.3 % today, below 1.5 %. The main cost is `log10` and `pow` on every sample of every band. Compute the detector and gain every 4 samples and interpolate, or use fast approximations.
  - Test: the gain differs by under 0.05 dB from today's per-sample path on the existing tests, upward compression included.

### Q2.7 Fancy FIR: resonance

The adaptive noise profile is done (Phase 2.1 A).

- **Resonance mode:** a sixth mode that finds narrow peaks against a smoothed spectral envelope and turns them down by Depth, with Sharpness, Attack and Release.
  - Test: a 3 kHz resonance 12 dB above a noise bed is reduced by the set depth, and broadband material changes by under 0.5 dB.
- **CPU stability:** Phase 2.1's §0.4 method makes comparisons within a run reliable, but absolute figures still vary between CI runs. Find the cause, run the benchmark at high priority, as the Verb's does, and report the 99th percentile.

### Q2.8 Fancy Verb: content, true stereo and modulation

- **IR library:** 30 or more original impulse responses, built with the Verb's own Reverb and Echo generators and saved as WAV in the plugin's resources. No recorded or third-party IRs.
  - Categories: rooms, halls, plates, chambers and special.
  - Shown in the FILE module's dropdown.
- **True stereo:** load 4-channel or paired IRs, so left and right each go to both outputs.
- **Modulation:** a slow, pitch-free modulation of the IR's tail, as an optional module.

## Q5. Designing a new plugin so it matches the suite

There's no designer mockup for a new plugin type. It's designed in three steps, and the user approves each before the next.

1. **Design rules.** Before the first new plugin, write `docs/DESIGN_SYSTEM.md` from the nine finished plugins:
   - the header, with its undo, A/B and Compare controls;
   - the page and card colours;
   - card radius and spacing;
   - the 12 px label size;
   - the shared knob styles and the right-hand column of meter and faders;
   - the tabs and segment switches;
   - the styled menus, dropdowns, tooltips and dialogs from Phase 2.1 S;
   - the footer row of checkboxes and the EDIT drawer for extras;
   - the accent colour of each plugin.

   It records which shared component to use for each kind of control, and what each window size is.
2. **Mockup.** Draw the new plugin as HTML with the Phase 2 mockup kit in `design/phase2-mockup-src/` (`common.css`, `lib.js`), so it uses the same knobs, faders, meters and cards as the existing mockups. Save it as `design/phase3-mockup-src/<plugin>.html`. The user opens it in a browser to review it, and approves or changes the control list, types and layout. Nothing is coded until this is approved.
3. **Build and compare.** Build it from the shared `fancy-core` components only. Compare the CI screenshot with the approved mockup, side by side, exactly as for Phase 2. Also compare it with one existing plugin of similar size, to check it reads as part of the same suite.

A control that needs a component `fancy-core` doesn't have is added to `fancy-core` and the gallery, never drawn inside one plugin.

## Q3. Acceptance

Each item passes §7 and P5. In addition:

- **Benchmark host:** the user has run `FancyBench.exe` on their PC against at least one competitor they own or trial in each improved category. The results go into `docs/research/bench-<date>.md`, and the research summary is updated with real head-to-head numbers.
- **Each improved plugin:** its manual, checklist and `docs/measurements` file are updated, and the CI screenshots are compared with the previous version as well as the mockup.
- **State compatibility:** every plugin's saved state from v1.1.0 and v1.2.0 loads in Phase 3 with the same sound, tested with saved state files kept in `tests/data/`.

## Q4. Out of scope unless the user asks

- Installers, licensing, copy protection, auto-update and telemetry, as before.
- AAX.
- Recreating any named product's sound or interface. Original models only.
- Machine-learning noise reduction. It would need a trained model and an inference dependency the brief hasn't approved.
