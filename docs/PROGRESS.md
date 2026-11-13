# Progress

Short status for starting a session. The full history to 13 November 2026 (run numbers, failed attempts, how each problem was solved) is in `docs/progress-archive.md`; read it only when you need a finished item's details. Keep this file short: when an item is done, replace its working notes with one line and put the detail in its design note's "As built" section.

## Now

- **Next item: Q2.4, Fancy EQ** (mid/side, 24 bands, Match), from `docs/BUILD_PROMPT_PHASE3.md`. Write its design-note section in `docs/eq.md` first (the brief's rule for Q2 items), with each decision given a recommended option, and get the user's approval before any code. New controls go in EDIT drawers or popovers unless the user approves a layout change.
- **Latest release:** [v1.3.0](https://github.com/alfrednoonoo/fancy-plugs/releases/tag/v1.3.0), 13 November 2026.

## Releases

| Version | What it added | Run |
|---|---|---|
| v1.0.0 | Kicker, Exciter, Comp, EQ, Verb | |
| v1.0.1 | Typed values on every knob and fader | |
| v1.1.0 | Delay, MultiComp, FIR | |
| v1.2.0 | Gate; Phase 2.1 (S, H, W, D, U, A) | 42576245019, commit `a613f77` |
| v1.3.0 | Limit, DS, DS-Mini, Sature, Room, Kicker Sub mode, preset browser, free resizing, title strip, temporary Listen switches | 43436706689, commit `e2ed62c` |

Release recipe: bump `project(... VERSION)` in `CMakeLists.txt` (it has CRLF line endings, so edit it byte-exact), update `README.md` (plugin table, "What's new", screenshots from the latest `screens-<sha>` @2x into `docs/images/`), push, wait for green, then `gh release create v<x.y.z>` with that run's `FancyPlugs-VST3-<sha>` artifact zipped as it is, named `FancyPlugs-<x.y.z>-win64.zip`. Notes follow v1.2.0's: what's new, install, build (run, commit, test count).

## Done

Every item below is built, green in CI and passed by the user in REAPER. Each has a design note, a manual (`docs/<plugin>-manual.md`), a REAPER checklist (`docs/<plugin>-checklist.md`) and, from Phase 2 on, measurements in `docs/measurements/`.

- **Phase 1:** Kicker, Exciter, Comp, EQ, Verb (`docs/<plugin>.md`). Released.
- **Phase 2:** Delay, MultiComp (renamed from XComp; the mockup keeps `xcomp` file names), FIR. Released as v1.1.0.
- **Phase 2.0.1:** Gate (`docs/gate.md`).
- **Phase 2.1:** S styled pop-ups, H host compatibility (`docs/host-compat.md`), W undo/A-B/Compare, D dynamic EQ, U upward compression, A adaptive noise tracking. Design notes `docs/phase2_1-<item>.md`. Released as v1.2.0.
- **Phase 3 Q1:** FancyBench benchmark host (`docs/phase3-Q1.md`, how-to `docs/bench.md`, results `docs/research/bench-2026-11-09.md`).
- **Phase 3 Q2.1:** preset browser, free resizing, title strip (`docs/phase3-Q2.1.md`, `docs/presets-checklist.md`).
- **Phase 3 Q5 step 1:** design rules, `docs/DESIGN_SYSTEM.md` (accents: Limit red, DS green, DS-Mini blue, Sature orange, Room purple).
- **Phase 3 new plugins:** Fancy Limit (`docs/limit.md`), Fancy DS (`docs/ds.md`), Fancy DS-Mini (`docs/ds-mini.md`), Fancy Sature (`docs/sature.md`), Kicker's Sub mode (`docs/kicker-sub.md`), Fancy Room (`docs/room.md`). REAPER checks passed on 10 November (Limit) and 13 November 2026 (the rest).
- **§0.3 Listen rule:** Listen and preview switches are temporary, never parameters (`FancyProcessor::setListening`, `ListenCheckbox`), retrofitted to Comp's and Gate's Preview filter and FIR's Listen. Limit's "Listen to removed" stays a parameter, by the user's choice.

Latest green run: 43436706689 (commit `e2ed62c`, the v1.3.0 release), 439 test cases, pluginval, Steinberg's validator, the host harness and FancyBench.

## Still to do in Phase 3

In Q2's order (`docs/BUILD_PROMPT_PHASE3.md`), each with an approved design note first and signed off in REAPER before the next:

1. EQ (Q2.4): mid/side, 24 bands, Match. Next.
2. Comp (Q2.5): character and upward compression.
3. Verb (Q2.8): content, true stereo and modulation.
4. Then MultiComp (Q2.6), Delay (Q2.3), FIR (Q2.7) and Kicker (Q2.2, now reduced to Sub mode, which is done). Confirm this part of the order with the user when the first three are done.

Mac comes after Windows is finished. Phase 4 holds Kicker's sidechain Duck, a transient shaper and a chorus.

## Standing decisions

- JUCE pinned to `8.0.17-security`; static MSVC runtime; sccache; CI builds with `-k 0`.
- Geometry in native Figma units at 100 %; Kicker is 392 × 472. Knob pointers follow the parameter value, not Figma's drawn positions.
- Header knob and the EDIT drawer's Output are the same parameter.
- Kicker's Sub mode: Divider engine only; a Spectral engine only if Divider disappoints. Q2.2's Harmonics mode is dropped and its Duck moves to Phase 4 (user, 11 November 2026).
- Limit's CPU target is under 5 % at 4x with True peak (user, 10 November 2026).
- The preset browser: one click loads and keeps it open; double-click or Enter loads and closes (user, 13 November 2026).
- Phase 1 reference images live outside the public repo, in `C:/dev/fancy-plugs-private/reference/`. Never commit them.
- Never store a Figma token in the repo. The Figma data is committed, so a token is only needed to export again.
- Before pushing a new plugin, model its tests in Python and run a pre-push check (as `docs/research/room-model.py` and `room-prepush.py` did), to aim for one green run.

## Known warnings

- `hendrikmuhs/ccache-action` and `ilammy/msvc-dev-cmd` target Node.js 20 and run on Node.js 24. Both work today.
- The runner image changes about weekly, which adds about 4 minutes of configure time to the next run.
