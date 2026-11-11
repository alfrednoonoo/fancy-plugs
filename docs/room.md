# Fancy Room: design note

Spec: `docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md` §3 (and §0.2 for the accent). Mockup: `reference/phase3/18__room_mockup.png`, source `design/phase3-mockup-src/room.html`. Design rules: `docs/DESIGN_SYSTEM.md`. **Not started: only the build plan below exists (11 November 2026). No code until the design note is written and approved.**

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

Decisions for the user:
- **Wet only (send):** forces Dry off, or only hides the Dry fader.
- **Freeze:** how it interacts with Ducking and Pre-delay.
- **The tail reported while frozen:** endless, but the host needs a length.
- **Quality switching:** Eco has 8 delay lines and Normal 16, so a change while playing needs a crossfade.
- **Sync pre-delay:** reuses Fancy Delay's note values.
- **Width −1 to +2:** as mid/side scaling.

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
