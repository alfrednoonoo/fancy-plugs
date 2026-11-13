# Fancy Room: design note

Spec: `docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md` §3 (and §0.2 for the accent). Mockup: `reference/phase3/18__room_mockup.png`, source `design/phase3-mockup-src/room.html`. Design rules: `docs/DESIGN_SYSTEM.md`. **Not started: the build plan below (11 November 2026) and the Phase 1 decisions, approved 13 November 2026 (A for all), the Python model (13 November 2026), which passes all five tests, the CPU budget (13 November 2026): about 1.3 % in Normal, the layout check, and the code (Phases 5 and 6, 13 November 2026; "As built" below). CI run 43415017352 is green (the first push failed on one missing include). Next: the user's REAPER checks.**

## Build plan (agreed with the user, 11 November 2026)

The goal is one green push. The plan answers what went wrong while building Fancy Sature and Kicker's Sub mode.

### What failed before, and what prevents it

| Failure | Cause | Prevention |
|---|---|---|
| Sature: three test failures | Numbers predicted without a model (ADAA's roll-off in the gain table, half-band fold-back above 20 kHz) | Model every test before code |
| Sature: CPU over target | Estimated, not measured until CI | A CPU budget, and cheap choices from the start |
| Kicker: sweep test failure | The subsonic filter's start-up, left out of the model | Model every filter in the path |
| Four clipped labels or captions, one overlapping dot | Text width never checked against its box | Measure text with the real font before pushing |
| Partial commit, broken escapes, line-ending churn | Files edited through inline Python scripts | Edit and Write tools only, plus a pre-push check |

No compile errors came up in about six builds of new code: copying existing patterns and checking every API signature before use stays.

### Phase 1: the design note, with every ambiguity settled before code

Decisions for the user, written 13 November 2026. Each has a recommendation (A) and an alternative (B). **Approved by the user on 13 November 2026: A for all seven.**

1. **Wet only (send).**
   - A: while it's on, the DSP treats Dry as −inf and the Dry fader is greyed with the tooltip "Off while Wet only (send) is on". Dry's own value is kept, so turning the switch off brings it back. Host bypass still passes the dry signal, as in every other plugin.
   - B: it only hides the Dry fader, and Dry has to be pulled down by hand.
2. **Freeze.**
   - A: the input to the late network fades out over 20 ms. Feedback goes to exactly 1 with damping and modulation off, so the loop is lossless: interpolated reads lose highs, and that would fail the 60 s test. Size, Decay and Damping hold while frozen and glide to their current values when Freeze goes off. Early reflections still follow the input, so new playing is heard as reflections over the frozen pad. Pre-delay, Width, Low cut, High cut, Ducking, Wet and Dry still work, because they come after the network.
   - B: Freeze also mutes the early reflections, so only the frozen pad sounds.
3. **The tail reported to the host.**
   - A: normally Pre-delay + the longest reflection + 1.5 × Decay (−90 dB at RT60 × 1.5), told to the host whenever it changes. While frozen it reports "infinite" if JUCE's VST3 wrapper passes that through (to be checked in JUCE's source before any code), and otherwise 60 s.
   - B: always report the tail for the longest Decay (30 s), which is simple but makes every export render 30 s of extra silence.
4. **Quality while playing.** The brief gives Eco 8 lines and Normal and High 16. High uses cubic interpolation and modulates every line, where Normal modulates 4 lines with linear interpolation.
   - A: on a change, the old network stops taking input and is left to ring out; it's switched off once its output stays under −90 dB, or after 2 s with a fade. The new network takes the input from that moment. The tail never drops out, at up to double the CPU for 2 s.
   - B: a 50 ms crossfade to the new network, which starts empty, so the existing tail is cut short.
5. **Sync pre-delay.**
   - A: a separate stepped parameter holds the note, so the ms value is kept when Sync goes off. It uses Fancy Delay's note lengths and feels (`plugins/delay/src/DelayTime.h`, moved to `fancy-core`), from 1/64 to 1/8 with straight, dotted and triplet, and defaults to 120 BPM when the host gives no tempo, as in Delay. The result is capped at 250 ms. **Changed by the user on 13 November 2026 (Phase 4):** "(250 ms max)" doesn't fit the read-out (138 px in a 68 px knob), so a capped note keeps its text, such as "1/8 dotted", in red (#E74D3D), and the tooltip says "1/8 dotted is 375 ms at this tempo; Pre-delay stops at 250 ms".
   - B: allow notes up to 1/4 and raise the pre-delay buffer to 1 s when Sync is on.
6. **Width −1 to +2.**
   - A: mid/side scaling on the wet signal only: M = (L + R) / 2, S = (L − R) / 2, L = M + W·S, R = M − W·S. So 0 is mono, −1 swaps the sides and 2 doubles the side signal. The mono sum never changes with Width. Stereo input "Mono-sum" sums the input before the reverb.
   - B: crossfeed-style width, which changes the mono sum.
7. **Ducking** (the brief only gives 0–100 %).
   - A: a follower on the dry input, with 10 ms attack and 300 ms release. At 100 % the wet is turned down by up to 24 dB, fully when the input is above −20 dBFS and not at all below −50 dBFS; Ducking scales that depth.
   - B: a threshold knob in the drawer, which adds a parameter the brief doesn't list.

Tooling the suite doesn't have yet: an RT60 measurement (T30 from the energy-decay curve), and the host harness's "render 1 s, then silence, and get the full tail to −90 dB".

### Phase 2: a Python model of the whole engine, used as the spec

- The full signal path, every filter included: early reflections, input diffusers, the delay network with its mixing matrix and per-line damping, seeded modulation, Width, Low cut and High cut.
- The five §3.4 tests run on the model exactly as the C++ tests will run them, with the same signals and the same measurement code:
  - RT60 within ±10 % across Size × Decay;
  - no metallic peak in the tail;
  - Freeze holding level for 60 s;
  - the mono sum within 3 dB;
  - the tail render.
- Test limits come from the model's figures, with a stated margin. Where the brief's number can't be met, the user hears it before any code, as with Sature's aliasing.
- The riskiest test is RT60 within ±10 % at extreme Size and Decay, since early reflections and diffusion bend the start of the decay curve. The model settles it, or shows the measurement needs a later window.
- The C++ is a transcription of the model: the same constants and the same order of operations.

**Model results, 13 November 2026** (`docs/research/room-model.py`, pure Python, 48 kHz, all defaults unless named; run time about 1 minute):

| Test | Model | Limit |
|---|---|---|
| No metallic ringing: Decay 2 s, Mod 25 %, Welch 4096-point Hann, 50 % overlap, 0.3–1.5 s, L and R averaged, each bin against its ±1/6-octave mean, 100 Hz–16 kHz | +4.07 dB at 2918 Hz (the measurement's own noise is about +4 dB) | +6 dB |
| RT60, T30 from the energy-decay curve, Size 20/55/90 % × Decay 0.5/2/8 s | −5.6 % to 0.0 % | ±10 % |
| Freeze, −12 dBFS noise, frozen at 1.5 s, 1 s levels over 60 s | within 0.14 dB, no NaN | 1 dB |
| Mono sum of the tail, Σ(L+R)² against Σ(L²+R²) | −0.01 dB, L/R correlation −0.002 | 3 dB |
| Tail: 1 s of −6 dBFS noise, then silence; loudest 50 ms after the reported tail, against the loudest 50 ms | −107.7, −101.5, −99.9 dB at Decay 0.5, 1.8, 8 s | −90 dB |

Settled by the model:
- **Every line is modulated, in every Quality.** With only 4 of 16 lines modulated, the linear reads took highs from those lines alone, their modes decayed at different rates, and peaks reached +6 to +10 dB. With all lines modulated the worst is +4.8 dB at any Size or Mod. High differs from Normal by cubic reads (+4.0 dB against +4.1 at the defaults) and Eco by its 8 lines.
- **RT60 is measured as T_mid**, the mean of T30 in the 500 Hz and 1 kHz octave bands (ISO 3382), because Damping shortens the highs by design: broadband T30 reads 10–14 % short at the default 45 % Damping. Results sit 3–5 % short, from the linear reads' slight loss; well inside ±10 %, so there's no correction.
- **The reported tail** (Pre-delay + longest reflection + 1.5 × Decay) leaves 10 dB of margin at −90 dB.
- **Freeze** reads every line at a whole-sample length once frozen (the modulation glides to the line's unmodulated length over the 20 ms ramp), so the loop is exactly lossless.

### Phase 3: a CPU budget before writing the engine

Count the operations per sample in Normal (16 lines, Householder mixing, a few modulated reads, eight all-passes, about 20 reflection taps per channel) against measured references: Sature 1.72 %, Comp about 0.45 %. Target under 3 %, with the cheap choices made in the design.

**Budget, 13 November 2026.** There's no compiler here, so the figures are counts calibrated against CI measurements, not timings.

Calibration:
- At 48 kHz, 1 % of a core is 208 ns per sample.
- One Fancy Delay tap costs 0.19 % (16 taps plain 3.01 %, 1 tap 0.23 %; `docs/measurements/delay.txt`), about 39 ns per sample. That covers about 100 scalar double operations (14 smoothers, two reads, two DC blockers, the mid/side output) plus two to four `tanh`. So about 0.3 ns per operation is a fair rate for this kind of code on CI's machines.
- A plugin that does almost nothing costs 0.15 to 0.23 % (Kicker 0.16 %, Verb 0.21 %, Sature with oversampling off 0.23 %): smoothers, meters, bypass and the workflow layer.

Operations per sample, Normal, stereo, with the cheap choices below:

| Part | Count | Operations |
|---|---|---|
| Early reflections | 32 taps, two loads and two multiply-adds each | 130 |
| Input diffusers | 8 all-passes, about 8 each | 65 |
| Line reads | 16 linear reads: modulation step, floor, two loads, lerp, index | 240 |
| Damping and Decay gains | 16 one-poles, about 4 each (freeze-off path) | 65 |
| Late sums | 16 | 30 |
| Mixing | 16-point Walsh-Hadamard (the model's matrix), 64 add/sub, then 16 writes with the input | 130 |
| Modulation | one `sin` per sample (16 lines every 16 samples) | about 30 |
| Output | Low cut and High cut biquads on two channels, Width, Wet, Dry, Ducking | 80 |
| **Total** | | **about 770** |

About 770 operations × 0.3 ns is about 230 ns, or 1.1 %. With the base cost that's **about 1.3 % for Normal**, with a range of 1.0 to 2.0 % allowing for CI's drift (Comp measured 0.22 to 0.46 % on 11 November 2026). High adds a 4-point cubic per line (about +130 operations): **about 1.6 %**. Eco halves the network: **about 0.9 %**. The 3 % target leaves room for an estimate that's 2× out.

Outside the steady figure:
- For up to 2 s after a Quality change, two networks run (decision 4): up to about 2.5 % in Normal → High.
- Moving Decay, Damping or Size recomputes 16 line gains (two `pow` each) every 16 samples: about +0.2 % while the knob moves.

Cheap choices, part of the design from the start:
1. **Modulation is computed every 16 samples, not every sample.** The exact sine is taken at each 16th sample of the absolute sample count and interpolated linearly in between, so the output doesn't depend on block size. The model now does the same (`MOD_STEP`), and all five tests give the same figures as before to the precision reported. One `sin` per sample instead of 16.
2. **Line gains and damping coefficients are recomputed only while Decay, Damping or Size move**, every 16 samples on the same absolute grid. Size needs nothing per sample, because every read is already fractional: a Size change glides the read positions.
3. **Delay lines and the input history are stored as `float`**, with the arithmetic in `double`, as in Fancy Delay. At 48 kHz the 16 lines take about 330 kB and stay in L2 cache. Float rounding sits near −140 dB, far under the tail test's −90 dB; Freeze still loops losslessly in level, since the matrix is orthonormal and its scale for 16 lines (1/4) is exact.
4. **The input history is stereo-interleaved**, so each reflection tap reads one cache line, and each tap's two gains (0.75 and 0.25 of its own side) are precomputed.
5. **Parts that are off are skipped**: early reflections at −inf, Ducking at 0 %, and the freeze terms when Freeze is off and its ramp has finished.
6. **No `tanh`, `exp` or `log` per sample in the network.** Ducking's follower maps to gain with one `log` per sample only when Ducking is above 0 %.

The CPU test reports Eco, Normal and High at the defaults, and Normal with Freeze and Ducking on, as the median of five with Comp's figure from the same run, the way Sature's test does.

### Phase 4: the layout checked before it renders

- Positions from `design/phase3-mockup-src/room.html`.
- A script reads character widths from `assets/fonts/Inter-*.ttf` (its `hmtx` and `cmap` tables, no libraries needed) and checks every label, read-out and caption against its box: six knobs in the Space card at about 70 px each, "Wet only (send)", "decay 1.80 s (to −60 dB)", the drawer's labels.

**Done, 13 November 2026:** `docs/research/room-textfit.py` (pure Python, `python -I docs/research/room-textfit.py`, 0 failures). Widths are glyph advances × point size ÷ units per em, which is how `fancy::ui::font` sizes text (`withPointHeight`); kerning is left out, so every box keeps a 2 px margin. Geometry, taken from Sature's editor and `room.html`:
- Content from x 13 to 646. Space card (6 knobs) 417 px and Tone card (3 knobs) 208 px, 8 px apart, so knobs sit 69.4 px apart in both; each `Knob` is 68 px wide, `tightStyle` (12 pt).
- Widest knob label: "Pre-delay", 55.4 px. Widest value: "1/16 dotted", 61.6 px, with Sync on.
- Footer: Sync pre-delay at x 13, Freeze at 134, Wet only (send) at 207, ending at 319; EDIT at 582.
- Drawer, as Sature's: 170 px labels ("Early reflections" 106.9 px at 14 pt) and 170 px dropdowns ("Mono-sum" 72.1 of 144 px of text room).
- Display: marker label "decay 20.0 s (to −60 dB)" 118.2 px at 10 pt; axis labels at 9 pt.

What it found:
- **The display legend** in `room.html` sits at fixed offsets, where "early reflections" (76.2 px at 10 pt) runs into the decay swatch, 66 px away. The legend is laid out from measured widths instead, right-aligned: 214.8 px in all.
- **The capped Sync read-out** of decision 5 doesn't fit: "1/8 dotted (250 ms max)" is 138.3 px, and even "250 ms max" is 69.4 px. The user chose on 13 November 2026 to keep the note's text in red while it's capped, with the cap explained in the tooltip (decision 5 above).

### Phase 5: code from patterns that have already passed

- Sature and Delay are the templates. Sature's commit (`e3db803`) lists the 12 places a plugin is registered.
- Every JUCE and `fancy-core` call is grepped before use. `getTailLengthSeconds` is still at its default in `FancyProcessor` and needs a real override.
- Built in from the start, because the shared suites check them:
  - settings applied per sample, and modulation seeded from the sample count, so block size never changes the output;
  - delay buffers sized in `prepare` for 384 kHz, the longest Size and the modulation depth, so nothing allocates on the audio thread;
  - no denormals and no NaN in the feedback paths;
  - an energy-preserving mixing matrix, so Freeze can't grow.

### Phase 6: a pre-push checker, then one push

- Room registered in all 12 places.
- Every parameter ID used by the editor, presets and tests exists.
- No raw non-ASCII bytes in C++ string literals.
- No line-ending changes against HEAD (no whole-file diffs).
- Unique test names, includes that resolve, balanced braces in every file.
- Text widths fit their boxes.
- A full review of the diff by hand, then one push.

### Risks that remain

One push is the goal, not a guarantee. In order of risk:
1. The new harness tail test, which changes host-harness code nothing else exercises yet.
2. The no-metallic-ringing criterion: model and C++ tails can differ through floating-point detail, hence the margins.
3. CPU on CI's machines, which drifts between runs (Comp measured 0.22 % to 0.46 % on 11 November 2026).

If CI fails, the model should make the fix one understood change.

## As built (13 November 2026)

Code: `plugins/room/` (`RoomDsp`, `RoomProcessor`, `RoomEditor`), tests `tests/RoomTests.cpp`, the harness's tail render in `tools/hosttest/Main.cpp`, manual `docs/room-manual.md`, REAPER checklist `docs/room-checklist.md`. Bundle ID `com.fancyplugs.room`, plugin code `FRoo`. `plugins/delay/src/DelayTime.h` moved to `fancy-core/include/fancy/dsp/DelayTime.h` unchanged (decision 5), so Delay's sound can't change.

### Signal path

`RoomDsp` is `render()` in `docs/research/room-model.py`, line for line: input history (stereo-interleaved `float`), 16 reflection taps per channel from it after Pre-delay, four all-passes per channel on the pre-delayed input, then the network (8 or 16 lines, `float` storage, `double` arithmetic): read each line (linear, or 4-point Lagrange in High), damp it (one-pole per line), sum the late outputs, mix with an orthonormal Walsh-Hadamard, write back with the diffused input. Then Low cut and High cut (Butterworth biquads, `fancy::dsp::Biquad`), Width as mid/side, Ducking, Wet, Dry, Output and the host's bypass. Latency is 0.

What the model leaves out, and how the plugin adds it without changing the model's steady state:
- **Every setting is smoothed** (30 ms; Pre-delay 50 ms, bypass 10 ms). Size and Pre-delay change the read positions per sample while they move, with interpolated reads; once settled, every length is a whole sample, as in the model. Line gains, damping and the cut filters are recomputed every 16 samples on the absolute sample grid, only while something moves.
- **Quality changes** (decision 4) use two preallocated networks. The new one is cleared on the audio thread (about 0.5 MB at 48 kHz). A third change while one is still ringing out drops the ringing one at once; Quality isn't automatable, so that only happens with quick clicks in the drawer.
- **Ducking** (decision 7): a peak detector on the dry input (instant attack, 50 ms release, to ride over the waveform) gives the wanted reduction, from 0 dB below −50 dBFS to 24 dB × Ducking above −20 dBFS, and the reduction follows it in dB with 10 ms attack and 300 ms release. A follower on the level itself would take about 1.5 s to let a −6 dBFS signal's reverb back, since 300 ms in linear amplitude is only 29 dB a second.

### Parameters

In this order (the host harness's offline test moves the first one, Pre-delay):

| ID | Name | Range | Default | Unit, skew, text |
|---|---|---|---|---|
| predelay | Pre-delay | 0–250, step 0.1 | 12 | ms, skewed so 40 ms is mid-travel; "12 ms", "4.5 ms" |
| prenote | Pre-delay note | 12 notes | 1/32 | 1/64 to 1/8, each straight, dotted, triplet ("1/8 dotted") |
| sync | Sync pre-delay | off/on | off | |
| size | Size | 0–100 | 55 | % |
| decay | Decay | 0.1–20, step 0.01 | 1.8 | s, log; "1.80 s", "12.5 s"; typing "500 ms" works |
| diffusion | Diffusion | 0–100 | 70 | % |
| mod | Mod | 0–100 | 25 | % |
| width | Width | −1 to 2, step 0.01 | 1.00 | −1 to 1 on the lower half of the knob, 1 to 2 on the upper, so 1.00 is at the default mark |
| damping | Damping | 0–100 | 45 | % |
| lowcut | Low cut | 20–1000 | 150 | Hz, log |
| highcut | High cut | 1000–20000 | 9000 | Hz, log; "9.0 kHz" |
| freeze | Freeze | off/on | off | |
| wetonly | Wet only (send) | off/on | off | |
| ducking | Ducking | 0–100 | 0 | % |
| quality | Quality | Eco, Normal, High | Normal | not automatable |
| erlevel | Early reflections | −60 (−inf) to +6 | 0 | dB |
| stereoin | Stereo input | Stereo, Mono-sum | Stereo | |
| wet | Wet | −60 (−inf) to +12 | −12 | dB, as Fancy Verb's |
| dry | Dry | −60 (−inf) to +12 | 0 | dB, as Fancy Verb's |
| output | Output | −24 to +12 | 0 | dB |
| bypass | Bypass | off/on | off | |

The brief gives no Wet or Dry default for Room, so they're Fancy Verb's. The brief gives no default note either: 1/32 (62.5 ms at 120 BPM) is the nearest musical value to the 12 ms default that still reads as a pre-delay.

### State

`FancyProcessor`'s versioned state (`stateVersion` 3) with these parameters, A/B slots included. It's a new plugin, so there's nothing to migrate.

### The §3.4 tests

All five run on `RoomDsp` as the model runs them. Metallic ringing and Freeze also run in Eco and High; the model gives +3.7 and +4.0 dB for metallic ringing and 0.09 and 0.13 dB for Freeze there (13 November 2026). The tail render runs in Catch2 on the engine and in the host harness on the built VST3, at Decay 0.5, 1.8 and 8 s, Wet only, from 1 s of −6 dBFS noise.

### Results, run 43415017352 (13 November 2026)

Everything passed: 439 test cases, pluginval, Steinberg's validator, the host harness and FancyBench. All figures are in `docs/measurements/room.txt`.

- **The C++ matches the model** to the precision reported: metallic ringing +4.07 dB at 2918 Hz (Eco +3.67, High +3.99), every RT60 the same as the model's (−5.6 to 0.0 %), Freeze within 0.13 dB, mono sum −0.01 dB, the tail −107.3, −101.4 and −100.4 dB after the reported tail. In the host harness, on the built VST3: −107.0, −101.7 and −99.9 dB.
- **Offline renders are bit-identical** to real time and to random blocks, through Size, Decay, Pre-delay, Freeze, Quality and Low cut changes.
- **CPU:** Normal 1.95 %, Eco 1.55 %, High 2.59 %, Normal with Freeze and Ducking 2.20 %, against Comp's 0.40 % in the same run. All are under the 3 % target, but Normal is 50 % over the budget's 1.3 % estimate, and Eco saves only a fifth: the parts that don't scale with the line count (32 interpolated reflection reads, eight interpolated all-passes, per-sample smoothers) cost more than counted. For the next budget: count an interpolated read from a large buffer as about 3 operations' worth more than a plain one.
- **The first push failed to compile** on one missing include (`fancy::levelToGain` without `ParamHelpers.h` in the editor); everything else compiled. The pre-push checker now checks that every `fancy::` function a changed file calls comes from a header it includes.
