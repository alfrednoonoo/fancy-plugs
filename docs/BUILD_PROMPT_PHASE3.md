# Phase 3.0: close the gaps to the market leaders

This extends `docs/BUILD_PROMPT.md` and `docs/BUILD_PROMPT_PHASE2.md`. **Every rule in them still applies**, including:

- cloud builds only, with nothing claimed until the Actions run is green;
- the design system and the shared `fancy-core` components;
- the real-time rules (§4) and the acceptance checks (§7 and P5);
- typed values on every knob and fader;
- "ask before changing the parameter list or layout".

The evidence behind this phase is in `docs/research/2026-11-market-report-summary.md` and `2026-11-market-report-detailed.md`. Read the summary first. Phase 3 starts only after Fancy FIR passes its REAPER check, unless the user says otherwise.

## Q0. Decisions for the user before starting

Ask these together, once, before writing any code. Record the answers in `docs/PROGRESS.md`.

**Answers so far (7 November 2026):**

- **Platform:** Windows first, until it's a concrete, finished version. Mac is wanted after that, as a later phase.
- **New plugins:** wanted. Which ones isn't decided yet. The user's main concern is that their design matches the suite, so every new plugin goes through Q5 first.
- **Release:** v1.1.0 comes first, once Fancy FIR has passed its REAPER check.
- **Order:** not decided yet. The options are explained to the user, and the recommendation is in Q2's preamble.

1. **Platform.** Stay Windows VST3 only, as the original brief says, or add CLAP, macOS or AU? Anything beyond Windows VST3 matters only if other people will use the plugins. It changes the CI matrix, costs macOS runner minutes, which are about 10 times Windows ones, and brings signing and notarisation questions.
2. **New plugins.** The suite has no limiter or maximiser, de-esser, saturator, transient shaper, chorus or other modulation effect, or algorithmic reverb. These are common in the "essential plugins" lists. Phase 3.0 doesn't add any unless the user picks some. If they do, each gets a P2-style section written and approved first.
3. **Order.** The default order is in Q2. The user may reorder or drop items.
4. **Phase 1 versus Phase 2.** Whether to cut a v1.1.0 release with Phase 2 before Phase 3 starts. The recommendation is yes, once FIR is signed off.

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
- **Tests:** CI runs it against our own eight plugins as a smoke test, and checks the numbers match `docs/measurements` within the runners' variation. The user runs it on their PC against plugins they own or trial.
- **Docs:** `docs/bench.md`, a how-to with three ready-made comparison files: compressor, EQ and delay.

## Q2. Improvements, in order

Recommended order, pending the user's choice:

1. The benchmark host (Q1), which gives a baseline before anything changes.
2. The shared features (Q2.1) and the design rules (Q5), so every later change is built on them once.
3. Improvements to the plugins the user reaches for most, one plugin at a time, each signed off in REAPER.
4. New plugin types, each designed through Q5 before any code.
5. Mac, as its own phase.

Each item needs its design-note section, in `docs/<plugin>.md`, written and approved first, because each changes the parameter list or layout. New controls go in EDIT drawers or popovers unless the user approves a layout change. Every new knob and fader uses the shared components, so typed values come with them.

### Q2.1 Shared, in `fancy-core`, for all eight plugins

| Feature | Detail | Test |
|---|---|---|
| A/B compare | Two parameter snapshots with a header toggle, and copy A to B. Non-parameter state, such as curves, profiles and chains, goes with the snapshot | Switching is bit-exact. Automation still writes to the active slot |
| Undo and redo | Inside the plugin, for parameter gestures and state edits, 50 steps, with Ctrl+Z and Ctrl+Shift+Z | Undo restores the exact value. Host automation isn't recorded as edits |
| Level-matched bypass | Optional. Bypass is matched to the processed signal's loudness over a sliding 3 s window, so a comparison isn't fooled by level | The level difference is within 0.5 dB on pink noise |
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

### Q2.4 Fancy EQ: dynamic EQ and mid/side

- **Dynamic bands:** each band gains Threshold, Range, Attack, Release and an optional external sidechain.
  - Use a TPT filter with the gain changed per sample, as the EQ already smooths.
  - Test: the band's gain follows the level with the set times. With Range at 0 it's bit-exact with today.
- **Channel per band:** Stereo, Left, Right, Mid or Side.
- **Up to 24 bands.** This changes the state format, so migration from 16 is needed, with `stateVersion` 2.
- **EQ Match:** learn a reference spectrum, from the sidechain or a captured section, and fit the bands to it. Optional, and only if Q0 doesn't drop it.

### Q2.5 Fancy Comp: character and upward compression

- **Character:** a saturation stage, pre or post, with Warm and Bright original curves and a drive control. It runs at the oversampled rate.
- **Upward compression:** for levels below the threshold, a Ratio below 1:1, with a Range control. This shares MultiComp's expansion code.
- **Auto threshold:** keeps the gain reduction steady across input level changes.
- **Sidechain EQ:** a 3-band EQ on the detector, replacing the high-pass and low-pass. It reuses Fancy EQ's matched filters.
- **Oversampling to 8x.**

### Q2.6 Fancy MultiComp: upward compression, lookahead and CPU

- **Upward and downward per band**, with a separate threshold for each, as in the OTT style. A preset reproduces an aggressive three-band upward and downward sound, as an original setting.
- **Lookahead:** 0-10 ms, reported as latency.
- **Mid/side per band.**
- **CPU:** 4 bands in LR 24 below 1.5 %. The main cost is `log10` and `pow` on every sample of every band. Compute the detector and gain every 4 samples and interpolate, or use fast approximations.
  - Test: the gain differs by under 0.05 dB from today's per-sample path on the existing tests.

### Q2.7 Fancy FIR: adaptive noise and resonance

- **Adaptive profile:** an optional minimum-statistics noise tracker that follows noise as it changes, for real-time use.
  - Test: noise that steps up by 6 dB is followed within 2 s, and speech isn't learned as noise.
- **Resonance mode:** a sixth mode that finds narrow peaks against a smoothed spectral envelope and turns them down by Depth, with Sharpness, Attack and Release.
  - Test: a 3 kHz resonance 12 dB above a noise bed is reduced by the set depth, and broadband material changes by under 0.5 dB.
- **CPU stability:** find the cause of the large differences between CI runs. Run the benchmark at high priority, as the Verb's does, and report the 99th percentile.

### Q2.8 Fancy Verb: content, true stereo and modulation

- **IR library:** 30 or more original impulse responses, built with the Verb's own Reverb and Echo generators and saved as WAV in the plugin's resources. No recorded or third-party IRs.
  - Categories: rooms, halls, plates, chambers and special.
  - Shown in the FILE module's dropdown.
- **True stereo:** load 4-channel or paired IRs, so left and right each go to both outputs.
- **Modulation:** a slow, pitch-free modulation of the IR's tail, as an optional module.

## Q5. Designing a new plugin so it matches the suite

There's no designer mockup for a new plugin type. It's designed in three steps, and the user approves each before the next.

1. **Design rules.** Before the first new plugin, write `docs/DESIGN_SYSTEM.md` from the eight finished plugins:
   - the header;
   - the page and card colours;
   - card radius and spacing;
   - the 12 px label size;
   - the shared knob styles and the right-hand column of meter and faders;
   - the tabs and segment switches;
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
- **State compatibility:** every plugin's saved state from v1.1.0 loads in Phase 3 with the same sound, tested with saved state files kept in `tests/data/`.

## Q4. Out of scope unless the user asks

- Installers, licensing, copy protection, auto-update and telemetry, as before.
- AAX.
- Recreating any named product's sound or interface. Original models only.
- Machine-learning noise reduction. It would need a trained model and an inference dependency the brief hasn't approved.
