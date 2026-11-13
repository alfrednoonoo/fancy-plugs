# Fancy Room: design note

Spec: `docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md` §3 (and §0.2 for the accent). Mockup: `reference/phase3/18__room_mockup.png`, source `design/phase3-mockup-src/room.html`. Design rules: `docs/DESIGN_SYSTEM.md`. **Not started: the build plan below (11 November 2026) and the Phase 1 decisions, approved 13 November 2026 (A for all), and the Python model (13 November 2026), which passes all five tests. Next: Phase 3, the CPU budget.**

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
   - A: a separate stepped parameter holds the note, so the ms value is kept when Sync goes off. It uses Fancy Delay's note lengths and feels (`plugins/delay/src/DelayTime.h`, moved to `fancy-core`), from 1/64 to 1/8 with straight, dotted and triplet, and defaults to 120 BPM when the host gives no tempo, as in Delay. The result is capped at 250 ms, with "(250 ms max)" in the read-out when the cap applies.
   - B: allow notes up to 1/4 and raise the pre-delay buffer to 1 s when Sync is on.
6. **Width −1 to +2.**
   - A: mid/side scaling on the wet signal only: M = (L + R) / 2, S = (L − R) / 2, L = M + W·S, R = M − W·S. So 0 is mono, −1 swaps the sides and 2 doubles the side signal. The mono sum never changes with Width. Stereo input "Mono-sum" sums the input before the reverb.
   - B: crossfeed-style width, which changes the mono sum.
7. **Ducking** (the brief only gives 0–100 %).
   - A: a follower on the dry input, with 10 ms attack and 300 ms release. At 100 % the wet is turned down by up to 24 dB, fully when the input is above −20 dBFS and not at all below −50 dBFS; Ducking scales that depth.
   - B: a threshold knob in the drawer, which adds a parameter the brief doesn't list.

Tooling the suite doesn't have yet: an RT60 measurement (T30 from the energy-decay curve), and the host harness's "render 1 s, then silence, and get the full tail to −90 dB".

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

### Phase 4: the layout checked before it renders

- Positions from `design/phase3-mockup-src/room.html`.
- A script reads character widths from `assets/fonts/Inter-*.ttf` (its `hmtx` and `cmap` tables, no libraries needed) and checks every label, read-out and caption against its box: six knobs in the Space card at about 70 px each, "Wet only (send)", "decay 1.80 s (to −60 dB)", the drawer's labels.

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
