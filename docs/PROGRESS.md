# Progress

Short status for starting a session. The full history to 13 November 2026 (run numbers, failed attempts, how each problem was solved) is in `docs/progress-archive.md`; read it only when you need a finished item's details. Keep this file short: when an item is done, replace its working notes with one line and put the detail in its design note's "As built" section.

## Now

- **Current item: Verb (Q2.8)**: IR library, true stereo, modulation. Design (every recommended option) and mockup (`reference/phase3/24__verb_q28_mockup.png`) approved by the user on 14 November 2026; `docs/verb.md`, "Phase 3 Q2.8".
  - Push 1, true stereo: green, run 43648226467 (`91a7204`), 482 test cases. Bit-identical to the legacy convolver and builder for direct IRs. CPU, 3 s IR: Zero latency 1.26 x direct, Low CPU 1.09 x (target < 1.6 x). The IR display keeps one lane (the mockup drew two).
  - Push 2 (generator settings, library tool and 32 IRs as a Windows resource, Library menu): run 43650494608 (`b833304`) built, and 486 of 487 test cases passed; the one failure was an old generator test that set the raw value array, fixed in push 3.
  - Push 3 (Modulate, five presets, screenshots, docs): green. Follow-up `7a1b820` (silent stages skipped, Reverb card layout fixed): green, run 43655069240, 493 test cases. CPU in `docs/measurements/verb-q28.txt`: Modulate 1.85 x (Zero latency) and 2.04 x (Low CPU), inside 2.2 x; true stereo 1.15 x and 1.14 x. Screenshots `verb-library`, `verb-generators` and `verb-add` checked against the mockup.
  - Add module (user, 15 November 2026): the cards' + goes; a dashed "+ Add module" box after the last card opens the add panel there, with Cancel. Mockup `reference/phase3/25__verb_add_module_mockup.png` (source `design/phase3-mockup-src/verb-q28-add.html`) waits for the user's approval.
  - Next: the user's REAPER check (`docs/verb-checklist.md`, the Q2.8 section), then the release (v1.5.0).
- **Plan agreed with the user (15 November 2026), to run while they're away:** build Verb's Add module (mockup `25`, approved), then MultiComp Q2.6, then Delay Q2.3, each in as few pushes as it takes, with tests, screenshots checked against its mockup, manual, checklist and CPU. If one needs a decision that isn't in its approved note, record it here, leave that item, and go on to the next. The user then checks Verb, MultiComp and Delay in REAPER together, and v1.5.0 follows.
- **MultiComp (Q2.6) and Delay (Q2.3):** design notes (`docs/multicomp.md`, "Phase 3 Q2.6"; `docs/delay.md`, "Phase 3 Q2.3") and mockups (`reference/phase3/26__multicomp_q26_mockup.png`, `27__delay_q23_mockup.png`) wait for the user's approval. This also confirms MultiComp and Delay as the next items; FIR (Q2.7) and Kicker (Q2.2) follow.
- **Latest release:** [v1.4.0](https://github.com/alfrednoonoo/fancy-plugs/releases/tag/v1.4.0), 14 November 2026.

## Releases

| Version | What it added | Run |
|---|---|---|
| v1.0.0 | Kicker, Exciter, Comp, EQ, Verb | |
| v1.0.1 | Typed values on every knob and fader | |
| v1.1.0 | Delay, MultiComp, FIR | |
| v1.2.0 | Gate; Phase 2.1 (S, H, W, D, U, A) | 42576245019, commit `a613f77` |
| v1.3.0 | Limit, DS, DS-Mini, Sature, Room, Kicker Sub mode, preset browser, free resizing, title strip, temporary Listen switches | 43436706689, commit `e2ed62c` |
| v1.4.0 | EQ: channel per band, 24 bands, Match. Comp: Character, Upward, Auto threshold, detector EQ, 8x | 43644278032, commit `82a4bbd` |

Release recipe: bump `project(... VERSION)` in `CMakeLists.txt` (it has CRLF line endings, so edit it byte-exact), update `README.md` (plugin table, "What's new", screenshots from the latest `screens-<sha>` @2x into `docs/images/`), push, wait for green, then `gh release create v<x.y.z>` with that run's `FancyPlugs-VST3-<sha>` artifact zipped as it is, named `FancyPlugs-<x.y.z>-win64.zip`. Notes follow v1.2.0's: what's new, install, build (run, commit, test count).

## Done

Every item below is built, green in CI and passed by the user in REAPER. Each has a design note, a manual (`docs/<plugin>-manual.md`), a REAPER checklist (`docs/<plugin>-checklist.md`) and, from Phase 2 on, measurements in `docs/measurements/`.

- **Phase 1:** Kicker, Exciter, Comp, EQ, Verb (`docs/<plugin>.md`). Released.
- **Phase 2:** Delay, MultiComp (renamed from XComp; the mockup keeps `xcomp` file names), FIR. Released as v1.1.0.
- **Phase 2.0.1:** Gate (`docs/gate.md`).
- **Phase 2.1:** S styled pop-ups, H host compatibility (`docs/host-compat.md`), W undo/A-B/Compare, D dynamic EQ, U upward compression, A adaptive noise tracking. Design notes `docs/phase2_1-<item>.md`. Released as v1.2.0.
- **Phase 3 Q1:** FancyBench benchmark host (`docs/phase3-Q1.md`, how-to `docs/bench.md`, results `docs/research/bench-2026-11-09.md`).
- **Phase 3 Q2.1:** preset browser, free resizing, title strip (`docs/phase3-Q2.1.md`, `docs/presets-checklist.md`).
- **Phase 3 Q2.4:** Fancy EQ, channel per band, 24 bands, Match (`docs/eq.md`, section 7). **Q2.5:** Fancy Comp, Character, Upward, Auto threshold, detector EQ, 8x (`docs/comp.md`, section 10). REAPER checks passed on 14 November 2026.
- **Phase 3 Q5 step 1:** design rules, `docs/DESIGN_SYSTEM.md` (accents: Limit red, DS green, DS-Mini blue, Sature orange, Room purple).
- **Phase 3 new plugins:** Fancy Limit (`docs/limit.md`), Fancy DS (`docs/ds.md`), Fancy DS-Mini (`docs/ds-mini.md`), Fancy Sature (`docs/sature.md`), Kicker's Sub mode (`docs/kicker-sub.md`), Fancy Room (`docs/room.md`). REAPER checks passed on 10 November (Limit) and 13 November 2026 (the rest).
- **§0.3 Listen rule:** Listen and preview switches are temporary, never parameters (`FancyProcessor::setListening`, `ListenCheckbox`), retrofitted to Comp's and Gate's Preview filter and FIR's Listen. Limit's "Listen to removed" stays a parameter, by the user's choice.

Latest green run: 43655069240 (commit `7a1b820`, Verb Q2.8), 493 test cases, pluginval, Steinberg's validator, the host harness and FancyBench.

## Still to do in Phase 3

In Q2's order (`docs/BUILD_PROMPT_PHASE3.md`), each with an approved design note first and signed off in REAPER before the next:

1. Verb (Q2.8): content, true stereo and modulation. Next.
2. Then MultiComp (Q2.6), Delay (Q2.3), FIR (Q2.7) and Kicker (Q2.2, now reduced to Sub mode, which is done). Confirm this part of the order with the user when Verb is done.

Mac comes after Windows is finished. Phase 4 holds Kicker's sidechain Duck, a transient shaper and a chorus.

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
- Phase 1 reference images live outside the public repo, in `C:/dev/fancy-plugs-private/reference/`. Never commit them.
- Never store a Figma token in the repo. The Figma data is committed, so a token is only needed to export again.
- Before pushing a new plugin, model its tests in Python and run a pre-push check (as `docs/research/room-model.py` and `room-prepush.py` did), to aim for one green run.

## Known warnings

- `hendrikmuhs/ccache-action` and `ilammy/msvc-dev-cmd` target Node.js 20 and run on Node.js 24. Both work today.
- The runner image changes about weekly, which adds about 4 minutes of configure time to the next run.
