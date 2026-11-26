# Progress

Short status for starting a session. The full history to 13 November 2026 (run numbers, failed attempts, how each problem was solved), and the long "Now" section as it stood on 23 November 2026, are in `docs/progress-archive.md`; read it only when you need a finished item's details. Keep this file short: when an item is done, replace its working notes with one line and put the detail in its design note's "As built" section.

## Now

- **Next: v1.7.2** (Wave 3, `docs/enhancements.md`): Limit multiband (▲), Gate multiband (▲), EQ spectral dynamics, Verb freeze and shimmer, Room shimmer, Delay reverse and pitch taps. Design note approved first, one plugin per push, each with its Python model and the pre-push checklist in `CLAUDE.md`; the two ▲ items need layout approval before their note. **Approved 26 November 2026, A in every decision** (EQ keeps decision 1's B, a drawer FFT size, as its only fallback): the four non-▲ notes, each a "Phase 4 Wave 3" section with its model: EQ spectral dynamics (`docs/eq.md`, `eq-spectral-model.py`), Verb Freeze and shimmer (`docs/verb.md`, `verb-freeze-model.py`, `shimmer-model.py`), Room shimmer (`docs/room.md`, `shimmer-model.py room`), Delay reverse and pitch taps (`docs/delay.md`, `delay-reverse-pitch-model.py`). They share a new `fancy::dsp::PitchShifter` (built with whichever of Delay or Verb comes first) and EQ moves FIR's STFT framing into `fancy-core`. Mockups follow approval: 69 Delay, 71 EQ, 72 Verb, 73 Room. **Mockup 69 approved** (user, 26 November 2026: the window grows 64 px to 884 x 472 rather than tightening the knobs). **Delay's Wave 3 is built and pushed** (`docs/delay.md`, Wave 3 section 8 "As built"; `fancy::dsp::PitchShifter` in `fancy-core`, tests `tests/DelayW3Tests.cpp` and `tests/PitchShifterTests.cpp` from `docs/research/delay-w3-prepush.py`; manual and checklist sections added): **green** in run 45799546744 (commit `db30e13`; the first run's click came from Pitch's warm-up, fixed); `delay-reverse` and `delay-motion` match mockup 69. **CPU open:** four +12 st taps add 0.60 % on the Mac runner but 1.85 % on Windows, against the note's < 1 %: the user decides whether to restate the target or have the shifter made cheaper. Then the user's REAPER check (`docs/delay-checklist.md`, last section). At the release: Delay's manual reference tables (`tools/manual/content.py`) for Pitch and Reverse. Start a fresh session every one or two items (user, 23 November 2026). The user moves installed builds to the default VST3 folder; REAPER does not scan `VST3-Dev`.
- **Latest release:** [v1.7.1](https://github.com/alfrednoonoo/fancy-plugs/releases/tag/v1.7.1), 25 November 2026 (Wave 2 items 11 to 46 except the folded and dropped ones, Kicker's Duck; every REAPER check passed, user, 25 November 2026; manual 1.7.1, 164 pages). Open: the Mac tester's listening check in Ableton Live (of v1.7.0).
- **Release plan** (user, 22 November 2026): v1.7.1 is released; v1.7.2 is Wave 3 (`docs/enhancements.md`). Phase 5: Fancy Harmony (user).
- **Side tracks:** REAPER theme is the main theme (`python tools/theme/build.py`, `docs/reaper-theme.md`); the mixer's FX list slots are still to check in REAPER. Website (`tools/site/build.py`, `docs/website-plan.md` §12): every plugin has its demo clip since 23 November 2026; refresh a clip by copying the `.webm` and `.mp4` from `demos-<sha>`. Content (`docs/content-plan.md`): film and fifteen carousels approved. Manual: rebuild with `tools/manual/` for each release.
- **Open:** Verb's FancyBench head-to-head is the user's (presets to export from REAPER); FIR's one 98 % bench block wants a re-run (`docs/research/bench-2026-11-17.md`); Q3's saved-state fixtures for v1.1.0 and v1.2.0 (`tests/data/`). Verb's IR search fix is green (run 45624311597, commit `a5b9101`); its shots are back in the manual and on the site. Delay's label strip (mockup 67 A, 820 x 472) is green (run 45632577034, commit `bf09b80`, Mac on its re-run) and matches. EQ's band list in a large window (mockup 68 A, `docs/eq.md`, Decisions 1 to 3 A) is green on Windows and Mac (run 45649213148, commit `a74c0a1`, started by hand because the push event was lost) and matches; `eq-list-small` shows the delete cross. All three passed the user's REAPER check (26 November 2026). At the v1.7.2 release: Delay's and EQ's manual text and screenshots, the site's views, markers and Delay's window size (they stay at v1.7.1 until then).
- **Working notes:** Python edits on Windows write CRLF: run `python tools/fix_eol.py` before committing (it skips binary files). Room keeps its own ducker and gate; Verb's are `fancy::dsp::WetDucker` and `WetGate`. Limit's tests 3 and 4 are looser than its note, with the reason in its As built.

## Releases

| Version | What it added | Run |
|---|---|---|
| v1.0.0 | Kicker, Exciter, Comp, EQ, Verb | |
| v1.0.1 | Typed values on every knob and fader | |
| v1.1.0 | Delay, MultiComp, FIR | |
| v1.2.0 | Gate; Phase 2.1 (S, H, W, D, U, A) | 42576245019, commit `a613f77` |
| v1.3.0 | Limit, DS, DS-Mini, Sature, Room, Kicker Sub mode, preset browser, free resizing, title strip, temporary Listen switches | 43436706689, commit `e2ed62c` |
| v1.4.0 | EQ: channel per band, 24 bands, Match. Comp: Character, Upward, Auto threshold, detector EQ, 8x | 43644278032, commit `82a4bbd` |
| v1.5.0 | Verb: IR library, true stereo, Modulate, Add module. MultiComp: lookahead, channel per band. Delay: Character, Motion, Ducking, Swing | 43737725969, commit `947168b` |
| v1.6.0 | FIR: Resonance mode (Depth, Sharpness, Attack, Release, Range, linked stereo, Listen) | 43742763641, commit `ff534e3` |
| v1.7.0 | Mac (VST3 and AU); Fancy TS, Fancy Chorus; Phase 4 Wave 1 and Wave 2 items 1 to 10; user zips | 45071833119, commit `c83b24a` |
| v1.7.1 | Wave 2 items 11 to 46 and Kicker's Duck (thirteen plugins) | 45602564567, commit `9196802` |

Release recipe: bump `project(... VERSION)` in `CMakeLists.txt` (it has CRLF line endings, so edit it byte-exact), update `README.md` (plugin table, "What's new", screenshots from the latest `screens-<sha>` @2x into `docs/images/`), push, wait for green, then `gh release create v<x.y.z>` with that run's user zips (from v1.7.0): `FancyPlugs-win64.zip` from `release-zip-win64-<sha>` renamed `FancyPlugs-<x.y.z>-win64.zip`, and `FancyPlugs-macos.zip` from `release-zip-macos-<sha>` renamed `FancyPlugs-<x.y.z>-macos.zip`, both uploaded as they are (never re-zipped: the Mac one keeps permissions only as `ditto` made it). Notes follow v1.2.0's: what's new, install, build (run, commit, test count). Then the website: update `site-src/data/release.json` (version, files, sizes, dates, test count); What's new comes from the README. Then update `site-src/data/plugins.json` for every changed plugin (views, markers checked on the new screenshots, "What you can do", details, window size; each new screenshot into `docs/images/`): v1.7.1 missed this at first (user, 25 November 2026).

## Done

Every item below is built, green in CI and passed by the user in REAPER. Each has a design note, a manual (`docs/<plugin>-manual.md`), a REAPER checklist (`docs/<plugin>-checklist.md`) and, from Phase 2 on, measurements in `docs/measurements/`.

- **Phase 1:** Kicker, Exciter, Comp, EQ, Verb (`docs/<plugin>.md`). Released.
- **Phase 2:** Delay, MultiComp (renamed from XComp; the mockup keeps `xcomp` file names), FIR. Released as v1.1.0.
- **Phase 2.0.1:** Gate (`docs/gate.md`).
- **Phase 2.1:** S styled pop-ups, H host compatibility (`docs/host-compat.md`), W undo/A-B/Compare, D dynamic EQ, U upward compression, A adaptive noise tracking. Design notes `docs/phase2_1-<item>.md`. Released as v1.2.0.
- **Phase 3 Q1:** FancyBench benchmark host (`docs/phase3-Q1.md`, how-to `docs/bench.md`, results `docs/research/bench-2026-11-09.md`).
- **Phase 3 Q2.1:** preset browser, free resizing, title strip (`docs/phase3-Q2.1.md`, `docs/presets-checklist.md`).
- **Phase 3 Q2.4:** Fancy EQ, channel per band, 24 bands, Match (`docs/eq.md`, section 7). **Q2.5:** Fancy Comp, Character, Upward, Auto threshold, detector EQ, 8x (`docs/comp.md`, section 10). REAPER checks passed on 14 November 2026.
- **Phase 3 Q2.8, Q2.6, Q2.3:** Verb (`docs/verb.md`), MultiComp (`docs/multicomp.md`), Delay (`docs/delay.md`). REAPER checks passed on 15 November 2026. Released as v1.5.0.
- **Phase 3 Q5 step 1:** design rules, `docs/DESIGN_SYSTEM.md` (accents: Limit red, DS green, DS-Mini blue, Sature orange, Room purple).
- **Phase 3 new plugins:** Fancy Limit (`docs/limit.md`), Fancy DS (`docs/ds.md`), Fancy DS-Mini (`docs/ds-mini.md`), Fancy Sature (`docs/sature.md`), Kicker's Sub mode (`docs/kicker-sub.md`), Fancy Room (`docs/room.md`). REAPER checks passed on 10 November (Limit) and 13 November 2026 (the rest).
- **Mac** (`docs/mac.md`): VST3 and AU, CI job, user zips; released with v1.7.0.
- **Phase 4 Wave 1** (thirteen plugins) and **Wave 2 items 1 to 10**; **Fancy TS** (`docs/ts.md`) and **Fancy Chorus** (`docs/chorus.md`). REAPER checks passed on 22 November 2026; released as v1.7.0.
- **Phase 4 Wave 2 items 11 to 46** and **Kicker's Duck** (each note's "As built"; mockups 53 to 66). REAPER checks passed on 25 November 2026; released as v1.7.1.
- **§0.3 Listen rule:** Listen and preview switches are temporary, never parameters (`FancyProcessor::setListening`, `ListenCheckbox`), retrofitted to Comp's and Gate's Preview filter and FIR's Listen. Limit's "Listen to removed" stays a parameter, by the user's choice.

Latest green release run: 45602564567 (commit `9196802`, release 1.7.1), 763 test cases on Windows and Mac, pluginval, Steinberg's validator, `auval`, the host harness and FancyBench.

## Still to do in Phase 3

In Q2's order (`docs/BUILD_PROMPT_PHASE3.md`), each with an approved design note first and signed off in REAPER before the next:

1. FIR (Q2.7): released as v1.6.0; REAPER check passed on 16 November 2026.
2. Kicker (Q2.2) is reduced to Sub mode, which is done.

Mac (`docs/mac.md`) is approved and in progress. Then Phase 4 (`docs/enhancements.md`).

## Standing decisions

- JUCE pinned to `8.0.17-security`; static MSVC runtime; sccache; CI builds with `-k 0`.
- Geometry in native Figma units at 100 %; Kicker is 392 × 472. Knob pointers follow the parameter value, not Figma's drawn positions.
- Header knob and the EDIT drawer's Output are the same parameter.
- Kicker's Sub mode: Divider engine only; a Spectral engine only if Divider disappoints. Q2.2's Harmonics mode is dropped and its Duck moves to Phase 4 (user, 11 November 2026).
- Limit's CPU target is under 5 % at 4x with True peak (user, 10 November 2026).
- MultiComp's Wave 2 CPU target is the cost lookahead and detector EQ on every band add beyond today's LR 48, about 20 %; everything on with 48 dB/oct measures 1.37x the defaults (user, 23 November 2026).
- EQ's Brickwall (96 dB/oct) CPU target is under 0.25 % per cut, not 1.5 % for 24 cuts: each cut is eight sections (user, 23 November 2026, option A).
- Comp's Q2.5 CPU targets are the cost added over the same oversampling: under 1.5 % at 4x, under 3 % at 8x (user, 14 November 2026).
- Verb's Modulate CPU target is under 2.2 times the same IR without it (user, 14 November 2026).
- The EQ's minimum width is 724 px, to fit `MATCH` in the header (user, 14 November 2026).
- The preset browser: one click loads and keeps it open; double-click or Enter loads and closes (user, 13 November 2026).
- **The manual has no "More views" section** (user, for the v1.6.0 manual; restated 23 November 2026): every screenshot sits under the heading that explains it, every button, checkbox, knob, fader and menu in it is explained, and every change to every plugin is described with what it does when switched on. `tools/manual/build.py` stops if a screenshot has no heading; before each release, check new screens and labels against the manuals.
- Phase 1 reference images live outside the public repo, in `C:/dev/fancy-plugs-private/reference/`. Never commit them.
- Never store a Figma token in the repo. The Figma data is committed, so a token is only needed to export again.
- Before pushing a new plugin, model its tests in Python and run a pre-push check (as `docs/research/room-model.py` and `room-prepush.py` did), to aim for one green run.

## Known warnings

- Run 45632577034's Mac job failed FancyBench's CPU ratios for Comp, EQ, MultiComp and Delay at once: Catch2's figures had Comp at 0.40 x itself, so the runner stalled while timing the reference. Its re-run passed; no DSP had changed.

- Run 45509105696's Windows job failed only at the demo clips: the new `windows-2025-vs2026` runner image has no ffmpeg, and the step's `choco install` hid its output. The step now shows Chocolatey's output, checks it, and puts its shims on the PATH; green from run 45514445844.

- Run 45485209546's Mac job failed one test under Rosetta (the Intel half), seed 3589446228: DS-Mini's "six earlier profiles render exactly as on the v1.7.0 engine", Bowed strings at Amount 2 only. The native Apple silicon pass and Windows passed it; DS and DS-Mini are unchanged since run 45451969543, where it passed; its input is fixed (seed 17) and the profile has no linear-phase worker, so the likely cause is state left by an earlier test (a thread or freed memory), not the engine. If it fails again, rerun that seed under Rosetta to find the test before it.

- Run 45200635372's Mac job was cancelled twice by GitHub before any step, then failed once on FancyBench's Delay CPU ratio check (bench 0.40x against Catch2 0.25x; Delay unchanged) and passed on its next re-run. If that check fails again, ask the user before widening its 50 % allowance.

- Run 45175938464's first Windows attempt crashed the test program with heap corruption (`0xc0000374`, no test named); its re-run passed with the same commit, so its cause is unknown. The later crashes (runs 45301827449 to 45368115988) were FIR W2b's Mid/Side switch test writing past its buffers, fixed in `2e9d505`. `tests/TestStartListener.cpp` now prints `START: <name>` before each test, so a crash names its test.

- `hendrikmuhs/ccache-action` and `ilammy/msvc-dev-cmd` target Node.js 20 and run on Node.js 24. Both work today.
- The runner image changes about weekly, which adds about 4 minutes of configure time to the next run.
