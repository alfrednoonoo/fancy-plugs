# Progress

Short status for starting a session. The full history to 13 November 2026 (run numbers, failed attempts, how each problem was solved) is in `docs/progress-archive.md`; read it only when you need a finished item's details. Keep this file short: when an item is done, replace its working notes with one line and put the detail in its design note's "As built" section.

## Now

- **FIR (Q2.7)**: Resonance mode and CPU stability, green in run 43742763641 (commit `ff534e3`, 516 test cases); as built in `docs/fir.md`. Released as v1.6.0 on 15 November 2026. The user checked Resonance in REAPER on 16 November 2026: fine. The paced FIR benchmark's second run (43773992670) agrees with 43742763641 within 20 % on every ratio to Fancy Comp (13 to 18 % lower); absolute CPU moved up to 23 % (Gate) between runners.
- **Current item: Mac** (`docs/mac.md`), **approved by the user on 15 November 2026, every recommended option.** Step 1 (portable code) green on Windows in run 43765631996. Step 2 (the `macos` CI job, VST3) green in run 43773992670: universal, all tests natively and under Rosetta, validators, host harness, FancyBench. Step 3 (Audio Units, `auval`, Gate as a music effect) green in run 43784934209. Step 4 (packaging: `install.sh`, `get-latest.sh`, signing, README, checklists) green in run 43788151856, installed on the user's Windows PC. **Step 5 in progress:** `strip -x` and `-dead_strip` done (artifact 331 to 197 MB, run 43807988300); Verb's library stays embedded (user, 16 November 2026). Pre-release **v1.7.0-beta.1** (Mac zip only, made with `ditto` so permissions survive; run 43812185484, commit `a027396`) published on 16 November 2026 for the Mac tester. Then the listening check on a Mac in Ableton Live, the tester's DAW (the checklists' Mac sections) and the first release with both zips. As built in `docs/mac.md`, section 9.
- **Manual PDF** (user, 16 November 2026): `docs/manual/Fancy-Plugs-Manual-1.6.pdf`, 123 pages: title page, linked contents and bookmarks, two shared chapters, a chapter per plugin (quick start, the plugin's manual, more views, a control reference of every parameter with range, default and description), glossary and credits. Built by `tools/manual/` (README there); rebuild it for each release.
- **Website** (user, 16 November 2026): design plan in `docs/website-plan.md`, **a draft waiting for the user's approval**: a free download showcase on GitHub Pages from this repo, 23 pages (home, three group hubs, a page per plugin, download, guide, what's new, about), screenshots with hotspots, audio slots filled later, animation guidelines. Logo and favicon are in `design/website/` (drawn by `make-brand.py`). Next after approval: HTML mockups of the home page, one hub and Comp's page, then the build. No site code yet.
- **Then:** Phase 3's Q2 list is done with FIR. Phase 4 holds Kicker's Duck, a transient shaper and a chorus. Q3's saved-state fixtures for v1.1.0/v1.2.0 (`tests/data/`) are still open, and FancyBench head-to-heads for Verb, MultiComp and FIR are the user's.
- **Latest release:** [v1.6.0](https://github.com/alfrednoonoo/fancy-plugs/releases/tag/v1.6.0), 15 November 2026: FIR Q2.7 (Resonance mode), released before the REAPER check at the user's request.

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
- **Phase 3 Q2.8, Q2.6, Q2.3:** Verb (`docs/verb.md`), MultiComp (`docs/multicomp.md`), Delay (`docs/delay.md`). REAPER checks passed on 15 November 2026. Released as v1.5.0.
- **Phase 3 Q5 step 1:** design rules, `docs/DESIGN_SYSTEM.md` (accents: Limit red, DS green, DS-Mini blue, Sature orange, Room purple).
- **Phase 3 new plugins:** Fancy Limit (`docs/limit.md`), Fancy DS (`docs/ds.md`), Fancy DS-Mini (`docs/ds-mini.md`), Fancy Sature (`docs/sature.md`), Kicker's Sub mode (`docs/kicker-sub.md`), Fancy Room (`docs/room.md`). REAPER checks passed on 10 November (Limit) and 13 November 2026 (the rest).
- **§0.3 Listen rule:** Listen and preview switches are temporary, never parameters (`FancyProcessor::setListening`, `ListenCheckbox`), retrofitted to Comp's and Gate's Preview filter and FIR's Listen. Limit's "Listen to removed" stays a parameter, by the user's choice.

Latest green release run: 43742763641 (commit `ff534e3`, release 1.6.0), 516 test cases, pluginval, Steinberg's validator, the host harness and FancyBench.

## Still to do in Phase 3

In Q2's order (`docs/BUILD_PROMPT_PHASE3.md`), each with an approved design note first and signed off in REAPER before the next:

1. FIR (Q2.7): released as v1.6.0; REAPER check passed on 16 November 2026.
2. Kicker (Q2.2) is reduced to Sub mode, which is done.

Mac (`docs/mac.md`) is approved and in progress. Phase 4 holds Kicker's sidechain Duck, a transient shaper and a chorus.

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
