# Progress

Short status for starting a session. The full history to 13 November 2026 (run numbers, failed attempts, how each problem was solved), and the long "Now" section as it stood on 23 November 2026, are in `docs/progress-archive.md`; read it only when you need a finished item's details. Keep this file short: when an item is done, replace its working notes with one line and put the detail in its design note's "As built" section.

## Now

- **Next: v1.7.1.** Design notes for Wave 2 items 11 to 45 and Kicker's sidechain Duck are approved, all A (22 and 23 November 2026): item 29 is folded into 27, items 30 and 31 and Sature's Knee are dropped. Item 46 (EQ band list, ▲) needs the user's layout approval before its note. Mockups 53 to 64 (`design/phase4-mockup-src/w2b_mockups.py`, screenshots from run 45087209567) are **approved by the user, 23 November 2026**, each recorded in its note's build order. Windows grow rather than squeeze (user): Gate 790 x 465, Delay 820, FIR 830 and Kicker 432 wide, Room 529 tall. Items 15 and 16 (EQ) need no mockup. **Gate 11 and 12 are built and green** (run 45125270876, commit `82a0bd6`; Gate 790 x 465), waiting for the user's REAPER check (`docs/gate-checklist.md`, last section). **Comp's level history in dB** (user, 23 November 2026; `docs/comp.md`, "v1.7.1: level history in dB", Decisions 1 to 3 A, mockup 65) is built and green (run 45131497194, commit `750f699`; `comp-history` matches mockup 65), waiting for the user's REAPER check (`docs/comp-checklist.md`, section "Level history in dB"). **Comp items 13 and 14** (six-band detector EQ with 12/24/48 dB/oct cuts, and link modes; mockup 55) are pushed in one commit: check the CI run, compare `comp-detector` and `comp-options` with mockup 55, and read test SC EQ 4 and link 3 printouts against the models; then the REAPER check (`docs/comp-checklist.md`, last section). Then the rest in the order of `docs/enhancements.md`, **one plugin per push** (user, 23 November 2026), each with its Python model and the pre-push checklist in `CLAUDE.md`. Start a fresh session every one or two items (user, 23 November 2026).
- **Latest release:** [v1.7.0](https://github.com/alfrednoonoo/fancy-plugs/releases/tag/v1.7.0), 22 November 2026 (Windows and Mac zips, Fancy TS, Fancy Chorus, Phase 4 Wave 1, Wave 2 items 1 to 10; manual 1.7, 137 pages). Open: the Mac tester's listening check in Ableton Live; findings go into v1.7.1.
- **Release plan** (user, 22 November 2026): v1.7.1 is Wave 2 items 11 to 46 and Kicker's Duck; v1.7.2 is Wave 3 (`docs/enhancements.md`). Phase 5: Fancy Harmony (user).
- **Side tracks:** REAPER theme is the main theme (`python tools/theme/build.py`, `docs/reaper-theme.md`); the mixer's FX list slots are still to check in REAPER. Website (`tools/site/build.py`, `docs/website-plan.md` §12): every plugin has its demo clip since 23 November 2026; refresh a clip by copying the `.webm` and `.mp4` from `demos-<sha>`. Content (`docs/content-plan.md`): film and fifteen carousels approved. Manual: rebuild with `tools/manual/` for each release.
- **Open:** Verb's FancyBench head-to-head is the user's (presets to export from REAPER); FIR's one 98 % bench block wants a re-run (`docs/research/bench-2026-11-17.md`); Q3's saved-state fixtures for v1.1.0 and v1.2.0 (`tests/data/`).
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

Release recipe: bump `project(... VERSION)` in `CMakeLists.txt` (it has CRLF line endings, so edit it byte-exact), update `README.md` (plugin table, "What's new", screenshots from the latest `screens-<sha>` @2x into `docs/images/`), push, wait for green, then `gh release create v<x.y.z>` with that run's user zips (from v1.7.0): `FancyPlugs-win64.zip` from `release-zip-win64-<sha>` renamed `FancyPlugs-<x.y.z>-win64.zip`, and `FancyPlugs-macos.zip` from `release-zip-macos-<sha>` renamed `FancyPlugs-<x.y.z>-macos.zip`, both uploaded as they are (never re-zipped: the Mac one keeps permissions only as `ditto` made it). Notes follow v1.2.0's: what's new, install, build (run, commit, test count). Then the website: update `site-src/data/release.json` (version, files, sizes, dates, test count); What's new comes from the README.

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
- **§0.3 Listen rule:** Listen and preview switches are temporary, never parameters (`FancyProcessor::setListening`, `ListenCheckbox`), retrofitted to Comp's and Gate's Preview filter and FIR's Listen. Limit's "Listen to removed" stays a parameter, by the user's choice.

Latest green release run: 43742763641 (commit `ff534e3`, release 1.6.0), 516 test cases, pluginval, Steinberg's validator, the host harness and FancyBench.

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
- Comp's Q2.5 CPU targets are the cost added over the same oversampling: under 1.5 % at 4x, under 3 % at 8x (user, 14 November 2026).
- Verb's Modulate CPU target is under 2.2 times the same IR without it (user, 14 November 2026).
- The EQ's minimum width is 724 px, to fit `MATCH` in the header (user, 14 November 2026).
- The preset browser: one click loads and keeps it open; double-click or Enter loads and closes (user, 13 November 2026).
- **The manual has no "More views" section** (user, for the v1.6.0 manual; restated 23 November 2026): every screenshot sits under the heading that explains it, every button, checkbox, knob, fader and menu in it is explained, and every change to every plugin is described with what it does when switched on. `tools/manual/build.py` stops if a screenshot has no heading; before each release, check new screens and labels against the manuals.
- Phase 1 reference images live outside the public repo, in `C:/dev/fancy-plugs-private/reference/`. Never commit them.
- Never store a Figma token in the repo. The Figma data is committed, so a token is only needed to export again.
- Before pushing a new plugin, model its tests in Python and run a pre-push check (as `docs/research/room-model.py` and `room-prepush.py` did), to aim for one green run.

## Known warnings

- `hendrikmuhs/ccache-action` and `ilammy/msvc-dev-cmd` target Node.js 20 and run on Node.js 24. Both work today.
- The runner image changes about weekly, which adds about 4 minutes of configure time to the next run.
