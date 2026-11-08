# Progress

## Status

- Skeleton workflow: done.
- `fancy-core`: done. The Verb module card lives in `plugins/verb/src/ModuleRack.cpp`, because only the Verb uses it.
- Fancy Kicker: done, signed off in REAPER by the user.
- Fancy Exciter: done, signed off in REAPER by the user.
- Fancy Comp: done, signed off in REAPER by the user.
- Fancy EQ: done, signed off in REAPER by the user.
- Fancy Verb: done, signed off in REAPER by the user. All five plugins are finished.

- Phase 1 is finished. Phase 2 (below) is the work that comes after it.
- Release: [v1.1.0](https://github.com/alfrednoonoo/fancy-plugs/releases/tag/v1.1.0), with all eight plugins: the five from v1.0, with the slow-drag fix, and Fancy Delay, Fancy MultiComp and Fancy FIR. v1.0.1 added typed values. The repo is private, so the releases are too.

Latest release run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42420703067 (commit `9b1ea24`, version 1.1.0): 230 test cases passed, and all eight plugins passed pluginval at strictness 10.

Latest run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42508653864, Phase 2.1 H: every test, Steinberg's validator, pluginval with its Steinberg step, and 101 host-harness checks passed for all nine plugins.

## Phase 2

Brief: `docs/BUILD_PROMPT_PHASE2.md`. Mockups: `reference/phase2/`. Each plugin passes §7 and P5 before the next starts, which includes the user's check in REAPER. P5 adds a comparison of each CI screenshot with its mockup, and every P2.3, P3.3 and P4.3 test in the Catch2 suite.

- Fancy Delay: done. The user reported it works well in REAPER.
- Fancy MultiComp: the user renamed Fancy XComp to this, and the mockup and its source keep their `xcomp` file names. Done. The user reported it works well in REAPER.
- Fancy FIR: done. The user reported it works in REAPER.

All three use the shared controls, so every knob and fader gets the typed values added in v1.0.1.

Phase 2 is finished and released as v1.1.0.

## What comes next

The user set this order on 7 November 2026, and updated Phase 2.1 on 8 November 2026:

1. **Phase 2.0.1, Fancy Gate:** done. The updated Phase 2.1 brief treats it as finished, as one of the nine plugins that Phase 2.1 changes. Its REAPER checklist, `docs/gate-checklist.md`, can still be run with the next build.
2. **Phase 2.1:** from `docs/BUILD_PROMPT_PHASE2_1.md`, in the order S, H, W, D, U, A. It builds no new plugin. Each item has its own design note, `docs/phase2_1-<item>.md`, written before any code.
   - S: styled menus, dropdown lists, tooltips, typed values and dialogs, built once in `fancy-core`. Mockup `reference/phase2_1/16__menus_mockup.png`, which also sets their exact styling. **Done.** The user reported the REAPER checks passed on 8 November 2026. Audit and design: `docs/phase2_1-S.md`.
   - H: **built, and green in CI**; waiting for the user's host checks (`docs/host-compat.md`) and one decision about FIR (below). Design note `docs/phase2_1-H.md`. Host compatibility: offline rendering done in-line, keys passed to the host, host scaling, an optional sidechain bus, latency-change notices, Steinberg's validator and a host-behaviour harness in CI, and `docs/host-compat.md` with a checklist per host.
   - W: undo and redo, A/B, and level-matched Compare, for all nine plugins.
   - D: dynamic EQ.
   - U: upward compression in MultiComp.
   - A: adaptive noise tracking in FIR.

   The controls mockup is `reference/phase2_1/14__phase2_1_controls.png`, and both mockups' source is in `design/phase2_1-mockup-src/`. The brief cites `docs/2026-11-market-report-detailed.md`, where the report is actually `docs/research/2026-11-market-report-detailed.md`.
3. **Phase 3:** from `docs/BUILD_PROMPT_PHASE3.md`, a draft. Phase 2.1 now covers four of its items, so it keeps what's left:
   - the benchmark host;
   - the preset browser and free resizing;
   - EQ mid/side, 24 bands and Match;
   - Comp, Verb, Kicker and Delay;
   - MultiComp's lookahead, mid/side and CPU;
   - FIR's resonance mode;
   - Fancy Limiter, Fancy De-esser and Fancy Saturator.

   Mac comes after Windows is finished. Trim the draft with the user before Phase 3 starts.

Changes to shared code since v1.0.1, which the next release should include:

- **Drag fix:** dragging accumulates, so controls with coarse steps move on slow drags. This reaches every plugin. Before it, each small move was rounded away against the snapped value.
- **Moved to `fancy-core`:** `OutMeter`, from Comp, and `ParamPowerIcon` and `IconButton`, from the EQ. All nine Phase 1 screenshots compared after the move were byte-identical.
- **New in `fancy-core`:** `ParamSegments`, `ColourTabs`, `formatFilterHz`, `ParamCheckbox::setLabelHeight`, and a colour for `Knob::setValueSuffix`.
- **Moved to `fancy-core` for MultiComp:** the analyzer, `TruePeakLimiter`, the gain computer and ballistics (`fancy/dsp/Compression.h`), `GainReductionMeter` and `SwitchableFrequencyParameter`. All nine Phase 1 screenshots compared after the move were byte-identical, and every Comp and EQ test passes.
- **New for MultiComp:** `ParamTextToggle`, `parseExpansionRatio`, `ShellEditor::setPresetDropdownWidth` and `getShownInputs`, `ParamDropdown::setTextHeight`, and `MeterTap::report`.
- **New for FIR:** `FancyProcessor::addToState`, a hook for state the audio thread writes, used by user presets too. `ParamSegments` gains a text size, widths that follow the text, and a selected colour that can change.

The mockups' HTML source is in `design/phase2-mockup-src/`, with exact layout formulas. Read it before building FIR. The page is #303030 and the cards are #262626.

## Delay acceptance (§7, P5)

Run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42405723460 (commit `7c4586d`).

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass for all six plugins, GUI tests on |
| 3 | State survives a REAPER project reload | Pass. A save and load test passes in CI, and the user reported the plugin works well in REAPER |
| 4 | P2.3 tests | Pass, 166 test cases across the suite. Numbers in `docs/measurements/delay.txt` |
| 5 | Screenshots compared with the mockup | Done; `docs/screens/delay-vs-reference.png` shows the mockup on the left and the build on the right |
| 6 | CPU | No §4.11 target. 0.37-1.08 % for 4 plain taps across runs, against the design note's < 1 %. 4 taps with every option on, including Saturate, took 1.67-3.11 %, which is over it. 16 taps took 1.5-3.0 % plain and 6.4-10.7 % with every option. CI runners moved this code by nearly a factor of three between runs |
| 7 | Installed and confirmed in REAPER | Pass, the user reported it works well |

P2.3 results:

- **Note lengths:** 1/8 dotted, 1/8 triplet and 1/4 at 120 BPM land within a sample of 375.0, 166.67 and 500.0 ms.
- **Filtered repeats:** with LP 2 kHz and 60 % feedback, the energy above 2 kHz is 21.4, 6.9, 2.7, 1.1 and 0.5 % over five repeats. With the checkbox off it's constant.
- **Time step:** the 375 to 500 ms test allows the output's largest step to be 5 % above the input's. The brief says no larger than the input's own. The crossfade's slope adds a small amount to a sine, so the margin is there for that.
- **Tempo ramp:** 100 % feedback through 90 to 180 BPM over 30 s stays below 0 dBFS, with Saturate on and off.

Delay deviations from the mockup:

- **Window:** 760 x 428, not about 410, because the shared header is 53 px and the mockup draws 37.
- **Controls:** knobs, faders, meters, tabs and the header are the shared components, so the arcs, fader handles and "0.0" rather than "+0.0" follow `fancy-core`, as the brief says.
- **Right column:** Out, Wet and Dry are 33 px apart, and the Out meter has no scale, as the mockup draws it.
- **Label size:** 12 px, because the shared 14 px doesn't fit four footer labels or six knob labels in this width.
- **Length knob:** 60 px, between the shared 50 and 72 px sizes, so that it fits the Time card.
- **Note dropdown:** reads "1/8" where the mockup draws "1/8 note", because it shares its text with the Length knob.
- **EDIT link:** added at the right of the footer, for the Multiplier that P2.2 asks for and the mockup doesn't show. It also holds Output, as on Kicker, Exciter and Comp.
- **Preset name and values:** the render shows "No preset" and default Ping-pong off is overridden to the mockup's state. The repeats are drawn from the real settings, so the faded bars sit in different places.

Lessons from the Delay build:

- **Look at the render, not the layout maths.** All 166 tests passed with four labels clipped, and my width estimates were wrong twice.
- **Keep test levels below any safety curve,** unless the test is about the curve. A full-scale impulse was the cause of two false failures.
- **PIL isn't installed here.** `System.Drawing` in PowerShell makes the comparison images with nothing to install.

## Fancy Gate (Phase 2.0.1)

Built, and green in CI. Waiting for the user's check in REAPER with `docs/gate-checklist.md`. The manual is `docs/gate-manual.md`, and the design note `docs/gate.md`.

The user's decisions, 8 November 2026:

- **Velocity timing with Lookahead on:** the physical rule. A note with velocity from level is late by the velocity window minus the lookahead still ahead of it, `max(0, W - (50 - Pre-open))` ms, and by `W` with Lookahead off.
- **Velocity mapping:** kept as the brief sets it, -60 to 0 dBFS onto 1 to 127. Hits at -12 and -3 dBFS come out 18.9 apart, not the brief's 20, and the test accepts 17 or more for that pair.

Shared code: Fancy Comp's detector, detector filters, Threshold meter and reduction meter moved into `fancy-core`. Comp's tests still pass, and the screenshots of all eight earlier plugins are byte-identical to the committed ones.

## Gate acceptance (§7, Phase 2.0.1)

Run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42451899318 (commit `72c5de2`). A later run only changes the I/O pill in the screenshot and line endings.

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass for all nine plugins |
| 3 | State survives a REAPER project reload | Save and load, corrupted state and preset tests pass in CI. REAPER check pending |
| 4 | §5 tests | Pass, 272 test cases across the suite |
| 5 | Screenshots compared with the mockup | `docs/screens/gate-vs-reference.png`, mockup on the left |
| 6 | CPU < 0.5 % with filters and RMS on, Lookahead off | Pass: 0.385 %, 0.99 × Fancy Comp in the same run. Numbers in `docs/measurements/gate.txt` |
| 7 | REAPER checklist, with a sidechain and a MIDI-trigger test | Pending, `docs/gate-checklist.md` |

Gate deviations from the mockup:

- **Window:** 740 × 421, not about 405, because the shared header is 53 px and the mockup draws 37.
- **Text:** the gate's own captions, checkboxes and Threshold scale use the mockup's sizes (10.5 and 8). Knobs, faders, meters and the header use `fancy-core`'s, which are larger.
- **Threshold scale:** 0 to -90 in 18 dB steps, because the brief's Threshold goes to -90 and the mockup's scale stops at -60.

Fixed during the build:

- **Range from -inf to 0 dB** was 0.8 dB short after 100 ms, because it was smoothed in dB from -120. It's now smoothed as a linear gain.
- **Clipped labels** in the first render: "Threshol", "close -4", "MIDI on open/clos" and "A4 · c...". The I/O pill read "2 in 2 out" in renders, because the shell's first header refresh runs before the plugin's override exists. In a host the 30 Hz refresh corrects it at once. The fix also corrects the FIR and MultiComp renders, which now read "4 in 2 out" like their mockups. Their committed screenshots changed only there.
- **Line endings:** three commits turned seven files into CRLF, and the next commit puts them back to LF. Write files with `newline=''` or bytes in Python here.

## Phase 2.1 S: styled pop-ups

Design note and audit: `docs/phase2_1-S.md`. Every pop-up in the audit is converted. Run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42497501203 (commit `814f947`): 289 test cases passed, and all nine plugins passed pluginval. A selection of the gallery is in `docs/screens/popups/`.

What changed, for all nine plugins:

- **One overlay layer.** Menus, tooltips, dialogs and typed values are components on the editor's layout root, so they're drawn inside the plugin window with Inter and follow the UI size. No JUCE `PopupMenu`, `AlertWindow` or `TooltipWindow` is left, and a test scans the sources for them.
- **Preset menu:** Factory and User sections, a check on the loaded preset, Save as new preset… (Ctrl+S), Reload "…" while changed, Rename…, Delete… (asks first) and Open preset folder. The dropdown reads "Default" at the defaults and adds " •" once anything changes. A project saved with a changed preset reopens with the mark.
- **Confirms** before deleting a user preset, a changed EQ or MultiComp band, Delay tap or Verb module, and the FIR noise profile. A fresh band, tap or module goes without asking.
- **Prompts in the window:** Save, Rename, About, and FIR's point and axis prompts, which were JUCE alert windows.
- **Typed values:** the §S.2 field, in each control's accent. Times take seconds ("0.12 s"), and the Delay's note takes "1/8." for dotted and "1/8t" for triplet.
- **Tooltips:** the mockup's look, after 600 ms, or 300 ms on a dimmed control such as Gate's Pre-open.
- **Shared code:** `fancy-core/ui/Popups.h` (Menu, MenuLayer, Tooltips, DialogLayer); `ShellEditor` is the pop-up host and lists its pop-ups for the gallery; `PresetManager` tracks changes, renames, deletes and reloads; `FancyProcessor::noteEdited()` counts changes outside the parameters (Verb's chain, FIR's curves and profile); `ParamControl` gains `openValueEditor`, `onTypedText` and `getAccentColour`; `ParamDropdown::addSection`; `parseMs`.

Checks:

| Check | Result |
|---|---|
| Actions run green, pluginval | Pass, run above |
| Gallery (§S.5.1) | `screens-popups-<sha>`: 81 pop-ups across the nine plugins, at 100 % and 200 % |
| Attached and styled (§S.5.2) | Every listed pop-up on every editor opens on the layout root, inside the window, in the plugin's accent, and no JUCE menu is open |
| Keyboard (§S.5.3) | ↑ ↓ skip disabled rows, → and ← open and close a submenu, Enter chooses, Esc closes with no choice |
| Typed values (§S.5.4) | "120", "120ms", "120 ms", "0.12 s", "2k", "2500", "-6", "-6 dB", "1/8." and a clamped "9000 ms" give the expected values; "abc" and "" change nothing |
| Saved projects | No parameter or DSP change. Every existing test passes, and the main screenshots differ from the committed ones only in the preset name (x 26-88, y 19-32), which now reads "Default" |
| REAPER (§S.5.5) | Pass: the user reported the checks passed, 8 November 2026 |

Deviations from the menus mockup:

- **Typed values** still open with a single click on the value, as since v1.0.1, as well as the brief's double-click.
- **Long lists:** the 12-row limit applies to value lists. The preset menu and right-click menus show every row, as the mockup draws the preset menu with 15, and scroll only when the window is too short. Gate's window, at 421 px, is.
- **EQ band types** keep the parameter's names, so "Band" where the mockup says "Bell". Slope submenus are on Low pass and High pass only, as drawn; the shelves' and Tilt's slope stays in the band's right-click menu and EDIT.
- **EQ right-click menu:** no "Del" hint, because the graph doesn't take keyboard focus (H.2). Dynamic comes with item D. A Slope submenu is added for the types that have one.
- **Gate MIDI dropdown:** Note, Channel and Velocity window as three submenus, with Note one scrolling list of 128. At the Gate's window edge the Note list opens to the left.
- **Comp's Detector input** has a third section, Mid/side, for its two extra choices.
- **Tooltips:** Kicker, Exciter, EQ and Verb have none of their own, so their checklists don't ask for one.

## Phase 2.1 H: host compatibility

Design note: `docs/phase2_1-H.md`. Host table, scenario sources and host checklists: `docs/host-compat.md`. Run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42506516139.

What changed:

- **Offline (H.1):** worker threads always run, so a host can switch between offline and real time without preparing again. Verb waits as long as an IR build takes when offline, instead of giving up after 60 s. EQ, MultiComp, FIR and Verb have `waitForBackgroundWork()`, which tests use to render real time as a machine that keeps up would.
- **Exciter fix:** with automation, its output depended on the block size by up to -60 dB, because the post-shaper filter and the character bias used one value per 64-sample chunk. Each sample now has its own, and offline renders are bit-identical across block sizes. Static settings sound exactly as before.
- **Keyboard (H.2):** nothing but text fields, menus and dialogs takes focus. Verb's rack viewport did, and no longer does.
- **Sidechain (H.4):** `fancy::dsp::SidechainWatch` gives Comp, MultiComp and FIR Gate's warning, in a strip at the top of the body (FIR: the top of the graph) only while it's on. The text is Gate's, which its brief set word for word. The bus was already optional, mono or stereo, and inactive by default.
- **Latency (H.6):** the settings that change latency have the tooltip line "Latency changes take effect in some hosts after you stop and start playback."
- **Program name:** the one host-facing program is now called "Default", the only error Steinberg's validator found.
- **CI (H.7):** Steinberg's validator (`vst3sdk` `v3.8.0_build_66`), built once and cached, runs on every bundle and inside pluginval. `tools/hosttest` loads every bundle through JUCE's VST3 hosting. JUCE's AudioPluginHost is built once per JUCE version and ships in the artifact as `tools/AudioPluginHost.exe`; `install.ps1` copies it to `%USERPROFILE%\VST3-Dev-Tools`.
- **Scaling (H.3):** `screens-hostscale-<sha>` renders every editor and its preset menu at host scales 1.25, 1.5 and 2.0. Checked by eye: no clipped text, and the menus scale with the window.
- **Docs:** host sections in every manual (sidechain in each host, Gate's MIDI-out table, latency, rendering), and `docs/host-compat.md`.

Results:

| Check | Result |
|---|---|
| Offline, random blocks of 0-8192 against 128, with automation | Bit-identical for all nine plugins in every mode, in-process and through the hosted `.vst3` |
| Real time against offline | Bit-identical for the seven plugins and modes with no rebuild. EQ linear, MultiComp linear and Verb bit-identical outside the window after each rebuild (1 s; 2.5 s for Verb's impulse swap, paced at real time). FIR EQ and Subtract within -132 dB |
| FIR Gate, Compressor and Convolve after an FFT-size change | Differ by -25.9, -26.7 and -12.9 dB from then on: see the open decision below |
| Keyboard | No control wants focus in any editor; space, Enter, Esc, letters, digits, arrows and Ctrl+Z all go back to the host |
| Mono main bus, no sidechain | All nine process |
| Sidechain inactive, mono, stereo, back to inactive | Comp, MultiComp, FIR and Gate process, and the warning comes and goes as it should |
| MIDI out nobody reads | Gate's audio is identical with MIDI on and off |
| Latency notices | Exactly one per change, both ways, for every setting in H.6, in-process and hosted |
| Automation flood, 10 s | No NaN, and no allocation on the audio thread in any plugin |
| Host harness | 101 of 101 checks: loads, offline blocks, 44.1/48/96 kHz, reactivation after 60 s of silence, state before and while playing, buses, host bypass time-aligned, latency notices, flood, 50 editor opens |
| Steinberg validator | 47 of 47 tests for each plugin |
| Host checks by the user | Pending, `docs/host-compat.md` |

Notes:

- **Steinberg's validator prints every bus as "Default Active".** Its bus scan tests the constant `busInfo.kDefaultActive` instead of the bus's flags. JUCE reports the sidechain as inactive by default.
- **Hosts restore state as JUCE's hosting does:** component state, then the controller's. The harness compares the restored parameters as the plugin displays them, because a stepped parameter snaps a host's raw value.
- **Allocation counting is in-process:** the plugins link the C runtime statically, so a hook in a host can't see their allocations.

### Open decision: FIR's frame grid after a rebuild

In real time, a rebuilt FIR engine (new FFT size, mode or Low latency) arrives from the worker a block or more after the change, while offline builds it in the same block. EQ and Subtract are linear, so that doesn't change their output. In Gate, Compressor and Convolve L/R, the new engine's analysis frames then sit a block later than in an offline render, and the two keep differing, at -26 to -13 dB, until the next rebuild. Both sound right; they just aren't the same render. Options:

1. **Leave it, and document it** in the FIR manual: after changing FFT size, mode or Low latency during playback, a render can differ slightly from what played.
2. **Align engine switches to a fixed sample grid:** the new engine starts at the next multiple of its hop, counted from the start of playback, in both real time and offline. Then the two match whenever the worker keeps up. That changes FIR's core switching code, which passed its REAPER check, so it needs FIR's size-change tests rerun and a REAPER check.

Verb's impulse swap has the same timing difference, but its convolution is linear, so it only lasts for the old impulse's tail.

## Market research and Phase 3 draft

- **Reports:** `docs/research/2026-11-market-report-summary.md` and `2026-11-market-report-detailed.md` compare the suite with the 2026 leaders in each category. They use our CI measurements and the leaders' published documentation. No competitor was run.
- **User's answers, 7 November 2026:** Windows first, and Mac later. New plugin types are wanted, but which ones isn't decided. Release v1.1.0 once FIR passes in REAPER. Order approved: benchmark host, shared features and design rules, then EQ, Comp and Verb first, as the plugins the user uses most, then new types, then Mac. New plugin types for Phase 3: limiter, de-esser and saturator. The others (transient shaper, chorus or modulation, algorithmic reverb) move to Phase 4.
- **Phase 3.0:** `docs/BUILD_PROMPT_PHASE3.md` is a draft. Its section Q0 lists the decisions the user makes before any work starts, and it isn't in `AGENTS.md` or `CLAUDE.md` until they approve it.

## FIR acceptance (§7, P5)

Run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42417161407 (commit `45b616b`).

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass for all eight plugins, GUI tests on |
| 3 | State survives a REAPER project reload | Pass. A save and load test passes in CI, with curves and the noise profile bit-identical, and the user reported it works in REAPER |
| 4 | P4.3 tests | Pass, 230 test cases across the suite. Numbers in `docs/measurements/fir.txt` |
| 5 | Screenshots compared with the mockup | Done; `docs/screens/fir-vs-reference.png` shows the mockup on the left and the build on the right |
| 6 | CPU | No §4.11 target. At 4096 points, 0.5-1.4 % across the modes in one run and 0.9-3.8 % in the next, against the design note's < 3 %. The second run also had one block per test take 17-19 times its time, where the first run's worst was 29 %. That looks like the runner being preempted, but CI can't prove it |
| 7 | Installed and confirmed in REAPER | Pass, the user reported it works |

P4.3 results:

- **Latency:** the reported latency equals the measured impulse delay at all nine sizes, both ways, in all five modes. That's 90 cases.
- **Reconstruction:** a flat EQ nulls against the delayed input below -100 dB, with and without Low latency.
- **Subtract:** with white noise plus a sine at -18 dB, the residual's frame-to-frame variance is 37.4 dB², against 98.4 dB² for plain subtraction, and the sine changes by -0.03 dB.
- **Learning:** stops by itself at the set length, doesn't go on learning the music, and reloads bit-identical.
- **Release:** the 63 % point is 1963 ms against 2000 ms set, at both 1024 and 8192 points.
- **Sidechain:** a 1 kHz sidechain reduces 980-1020 Hz by 28.5 dB, and moves 200-400 Hz and 4-6 kHz by under 0.01 dB.
- **Analysis:** a -12 dBFS sine reads -12 dB ± 0.5.
- **Size change mid-stream:** ends on the new latency, the output then equals the delayed input, and there's no step beyond the input's own slope with a 20 % margin.

FIR deviations from the mockup:

- **Window:** 760 x 445, not about 430, because the shared header is 53 px and the mockup draws 37.
- **I/O pill:** should read "4 in 2 out" live, as on MultiComp. The render can't show it, and it's on the checklist.
- **Edit dropdown:** 16 px wider, so that "Points (smooth)" fits at 12 px.
- **Learn hint:** wraps to three lines where the mockup has two, because 12 px is wider than the mockup's text.
- **Release:** reads "80.0 ms" where the mockup draws "80 ms", from the shared time format below 100 ms.
- **EDIT link:** added at the right of the footer, for Learn length, which P4.2 asks for and the mockup doesn't show, plus Output.
- **Cards for other modes:** the mockup only shows Subtract. EQ has Reset curve, Flat, Tilt and De-mud, Gate and Compressor have Reset curve, and Convolve L/R has its explanation, as P4.1 describes.

Decisions the brief left open, in `docs/fir.md`:

- **Staged frames:** at 8192 points and above, each frame's work spreads over half a hop, which adds N/8 to the latency.
- **Detector reach:** Gate and Compressor give each bin the loudest level within two bins either side, so a sine opens or ducks as one.
- **Convolve L/R:** the right channel is the impulse response, renewed every N samples, with no normalisation.
- **Points (flat):** straight lines between points, in log frequency.
- **One curve per mode:** EQ, Gate and Compressor keep their own.

Lessons from the FIR build:

- **A spectral detector sees a sine as five bins.** The first gate shut a sine's outer bins and lost 2.3 dB of a sine above its threshold.
- **Print the numbers a test checks.** INFO only shows on failure, so `docs/measurements` had nothing to record until the tests printed them.
- **CI timing varies widely between runs.** The same code measured a factor of two apart, and one run showed 50 ms stalls the other didn't.

## MultiComp acceptance (§7, P5)

Run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42410913133 (commit `e5a582d`).

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass for all seven plugins, GUI tests on |
| 3 | State survives a REAPER project reload | Pass. A save and load test passes in CI, and the user reported the plugin works well in REAPER |
| 4 | P3.3 tests | Pass, 203 test cases across the suite. Numbers in `docs/measurements/multicomp.txt` |
| 5 | Screenshots compared with the mockup | Done; `docs/screens/multicomp-vs-reference.png` shows the mockup on the left and the build on the right |
| 6 | CPU | No §4.11 target. 2.17-2.42 % for 4 bands in LR 24, 3.5-3.7 % in Linear, 4.9-7.3 % for 8 bands. That's over the design note's < 2 % for 4 bands |
| 7 | Installed and confirmed in REAPER | Pass, the user reported it works well |

P3.3 results:

- **Flatness:** the worst of 8 bands across 20 Hz to 20 kHz is 0.029 dB in LR 24, 0.025 dB in LR 48 and 0.019 dB in Linear, against ±0.1 dB. Those figures include the error of measuring a level over a part-cycle.
- **Linear null:** below -90 dB against the delayed input at 2, 4 and 8 bands.
- **Latency:** the reported figure is the measured delay at 44.1, 48 and 96 kHz, with and without Limit output.
- **Crossover sweep:** the 100 Hz to 2 kHz in 200 ms test allows the output's largest step to be 25 % above the input's. The brief asks for no clicks. A sine really does change as a crossover passes through it, so the margin is there for that.
- **Band gain test:** a 0.25 dB margin, because the neighbouring bands reach 400 Hz at about -39 dB.

MultiComp deviations from the mockup:

- **Window:** 760 x 470, not about 455, because the shared header is 53 px and the mockup draws 37.
- **I/O pill:** should read "4 in 2 out" live. The render shows "2 in 2 out", because it stops the header's timer before its first update. It's on the checklist.
- **Preset dropdown:** 170 px in place of the shared 246, so that Detector fits the header.
- **Label size:** 12 px, as on the Delay.
- **Band range:** "120.0 Hz" where the mockup draws "120 Hz", from the shared `formatHz`.
- **Threshold:** the shared `Fader`, as the mockup draws it. Its handle and readout follow `fancy-core`, so it reads "-18.0" rather than "-18.0 dB".
- **Gain knob:** wider than the others, so that the make-up amount fits in brackets after its value, as P3.3 asks.
- **EDIT link:** added at the bottom of the right column, for Attack = 90 % rise and Log-scale automated freq., which P3.2 asks for and the mockup doesn't show, plus Output.
- **Spectrum:** the render uses the mockup's formula drawn per pixel, so it's busier than the mockup's line.

Decisions the brief left open, in `docs/multicomp.md`:

- **Detector and Sidechain together:** a Sidechain detector makes every band listen to the sidechain, and the checkbox does that for one band.
- **Ratio below 1:1:** upward expansion above the threshold, limited to +24 dB.
- **Dry:** the sum of the split bands, so parallel compression stays in phase.
- **Active off:** bypasses the band's compressor, and the band stays audible.
- **Linked handles:** a plain drag on a handle moves all the thresholds, because Alt-drag already means Ratio.
- **Bands and their parameters:** adding or removing a band moves the following bands' settings along, so automation written to band 3 stays on the third band.

Lessons from the MultiComp build:

- **A test that can't fail looks like a pass.** The "missing sidechain" test first ran at 1:1, where nothing could happen. It now also checks that the same band does compress the main input.
- **Read the mockup's source first.** It gave exact axis formulas, and showed that the Delay's page and card colours were swapped.
- **A preview that stops the timer shows the header as it was built.** Anything the timer sets, such as the I/O pill, needs checking live.

## Kicker acceptance (§7)

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass, GUI tests on |
| 3 | State survives a REAPER project reload | Pass, confirmed by the user |
| 4 | §5.4 tests | Pass, 15 test cases and 52,834 assertions; numbers in `docs/measurements/kicker.txt` |
| 5 | Screenshots compared with the reference | Done against the Figma frame, see deviations; `docs/screens/kicker-vs-reference.png` shows Figma on the left and the build on the right |
| 6 | CPU against the §4.11 target | Pass: 0.07–0.15 % against < 0.5 %, measured on CI runners with every option on |
| 7 | Installed and confirmed in REAPER | Pass, confirmed by the user |

## Exciter acceptance (§7)

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass for both plugins, GUI tests on |
| 3 | State survives a REAPER project reload | Pass, confirmed by the user |
| 4 | §5.5 tests | Pass, 32 test cases across the suite; worst alias at 4× is -114.4 dBFS against the -80 dBFS target. Numbers in `docs/measurements/exciter.txt` |
| 5 | Screenshots compared with the reference | Done against the Figma frame; `docs/screens/exciter-vs-reference.png` shows Figma, the build, and the EDIT drawer |
| 6 | CPU against the §4.11 target | Pass: 0.09 % without oversampling against < 0.5 %. 0.77 %, 1.37 % and 2.27 % at 2×, 4× and 8× on CI runners |
| 7 | Installed and confirmed in REAPER | Pass, confirmed by the user |

Exciter deviations from Figma: knob arrows follow the §3.2 ranges as the user confirmed, so Freq 2000 Hz sits at 40 % where Figma draws 18 %. The other Kicker notes on peak-hold lines, meter tops, the caption and the About box apply here too.

## Comp acceptance (§7)

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass for all three plugins, GUI tests on |
| 3 | State survives a REAPER project reload | Pass, confirmed by the user |
| 4 | §5.2 tests | Pass, 57 test cases across the suite. Worst alias at 4× is -100.9 dBFS against the -90 dBFS target. The limiter holds its ceiling up to 200 dB above threshold in every oversampling mode. Numbers in `docs/measurements/comp.txt` |
| 5 | Screenshots compared with the reference | Done against the Figma frame; `docs/screens/comp-vs-reference.png` shows Figma above the build |
| 6 | CPU against the §4.11 target | Pass: 0.41–0.72 % without oversampling across runs, against < 1 %. About 2 % at 2× and 3–4 % at 4× on CI runners |
| 7 | Installed and confirmed in REAPER | Pass, confirmed by the user |

Comp deviations from Figma:

- **GR panel and Link channels:** added, because the Figma frame omits them and §2.5 requires them. The window is 934 × 527 rather than 834 × 527, and the Out meter's scale is 10 px wider, because Inter's two-digit numbers don't fit Figma's 13 px.
- **EDIT link:** added at the right of the Detector input row, for the settings that aren't in the mockups.
- **Checkbox states:** Figma shows Weird, Classic, Auto, Auto make-up, Preview filter and Limit output ticked. These are off by default, and Link channels is on. The comparison render sets the Figma states.
- **Knob arrows:** follow the values, as the user confirmed. Highpass at 93 Hz and Attack at 3 ms therefore sit differently from Figma, and Threshold -4.4 dB sits near the top of its fader.
- **Dry fader:** its default dot sits at the bottom, because Dry defaults to -inf. Figma draws it near 0 dB.

Lessons from the Comp build, which apply to the EQ and Verb:

- **Compute test bins from the signal.** The first aliasing test used hand-picked frequencies, missed every odd harmonic and could not fail. A result of -108 dBFS with oversampling off gave it away.
- **Sawtooth aliasing starts in the envelope.** The harmonics fold when the follower is sampled, so filters placed after it do nothing. Two fixes that assumed otherwise were tried and removed.
- **JUCE seeds raw parameter values unsnapped.** `FancyProcessor` now writes every parameter once at construction, which fixes this for all plugins. This is what keeps §4.8 bit-identical.

## EQ acceptance (§7)

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass for all four plugins, GUI tests on, including a rerun with new random seeds |
| 3 | State survives a REAPER project reload | Pass, confirmed by the user |
| 4 | §5.1 tests | Pass, 94 test cases across the suite. 18 kHz +6 dB bell at 44.1 kHz is within 0.42 dB of analog up to 20 kHz. Responses at 44.1–192 kHz are within 0.5 dB below 20 kHz for the tested types. A -12 dBFS sine reads -12.1 dB. Numbers in `docs/measurements/eq.txt` |
| 5 | Screenshots compared with the reference | Done against both Figma frames; `docs/screens/eq-vs-reference.png` shows Figma on the left and the build on the right |
| 6 | CPU against the §4.11 target | Pass: 0.16–0.46 % for 8 bands without oversampling across runs, against < 1 %. 0.5–1.1 % at 2× and 0.9–1.9 % at 4×. Linear is up to about 1.1 %. CI runners vary by about a factor of two |
| 7 | Installed and confirmed in REAPER | Pass, confirmed by the user |

EQ deviations from Figma:

- **EDIT link:** added in the header, for the settings that aren't in the mockups.
- **Window:** resizable through the host's border, minimum 696 × 464, which is §2.4's minimum at Figma scale.
- **Handles and curves:** follow the values. Figma's band 2 handle sits at 366 Hz on its own axis while its label says 487.7 Hz, and its "2.0" curve is wider than a true Q of 2, because the original control was bandwidth in octaves.
- **Gain knob:** the arc starts from the centre as §2.3 says. Figma draws it from the minimum.
- **Compact card:** knob positions follow Figma's render, not its stored positions, which disagree.
- **Analyzer label:** "pre" or "post" added at the graph's top left, as §5.1 asks.
- **Show phase:** Figma has no style for it, so it's a thin yellow line.
- **Analyzer line:** the comparison render uses a made-up spectrum, because CI has no audio.

Lessons from the EQ build, which apply to the Verb:

- **Rerun pluginval.** It picks new random settings each run, and one green run missed a NaN that the next found. A seeded fuzz test now covers this. It was confirmed by removing the fix and watching it fail. A directed test written for the same bug still passed without the fix, so it was removed.
- **Keep large buffers out of the processor object.** A hosted plugin is on the heap, but tests build it on the stack, and a 512 kB array overflowed it.
- **A ramp needs an end.** Coefficient increments must stop when the code that refreshes them is skipped.
- **Prototype DSP in Python first.** With no local compiler, it costs seconds rather than a CI run. The Vicanek formulas were checked this way, and a five-point rational fit was rejected before any C++ was written.

## Verb acceptance (§7)

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass for all five plugins, GUI tests on |
| 3 | State survives a REAPER project reload | Pass, confirmed by the user |
| 4 | §5.3 tests | Pass, 121 test cases across the suite. Numbers in `docs/measurements/verb.txt` |
| 5 | Screenshots compared with the reference | Done against the Figma frames; `docs/screens/verb-vs-reference.png` shows Figma on the left and the build on the right |
| 6 | CPU against the §4.11 target | Pass: 1.66 % in Zero latency with the audio thread at host priority, against < 5 %. 2.25-3.76 % with it at normal priority. No late blocks |
| 7 | Installed and confirmed in REAPER | Pass, confirmed by the user |

Verb deviations from Figma:

- **Info string:** says 1ch, because Mix to mono is ticked in the same mockup that says 2ch.
- **Time axis:** follows the IR's length. Figma draws 0.2 to 1.4 s beside a 4.5 s IR, so its waveform and axis disagree. The empty state uses Figma's axis.
- **Waveform:** the comparison render uses a made-up 4.5 s IR through the real file path, because CI has no IR files. It also starts at the left edge, where Figma leaves a 26 px gap, so that time 0 sits on the axis.
- **Echo lines:** follow the echoes, so at a 4.5 s scale they bunch up at the start. Figma varies their height for looks.
- **Trim handles:** added at the display's top corners, as §5.3 requires. Figma doesn't show them.
- **Echo generator label:** one spelling in both the add panel and the empty state. Figma writes "Echo generator" in the first and "Echo Generator" in the second.
- **Pre-delay:** 3.0 ms where Figma draws -3.0 dB, which the brief calls a mistake.
- **Knob arrows, Dry dot:** follow the values, as on the other plugins.
- **Cards Figma doesn't have:** Trim and Reverb Generator use Echo's grid and width. Reverse uses Normalize's width, with a one-line caption so the card isn't blank.

Decisions the brief left open, in `docs/verb.md`:

- **Module settings aren't host parameters.** The chain changes length at runtime and VST3's parameter list is fixed.
- **No project-relative paths.** VST3 gives a plugin no project path. The embedded copy covers a moved project.
- **New instances start empty,** matching Figma's empty state, so audio passes unchanged.
- **The convolver worker runs at JUCE `highest`.** A host's audio thread runs above that. If a host ran its audio below it, the worker could preempt audio for one FFT job, which would show as a dropout in a CPU spike rather than in the late-block counter.

Lessons from the Verb build:

- **Pace a threaded benchmark at real time, at host priority.** Run flat out, the worker can't keep up and the late counts mean nothing. At normal priority the worker preempts the benchmark for a whole FFT job, which looks like a 5-8 ms spike. Logging which blocks went over showed they were exactly 131,072 samples apart, and a time-critical run with a normal-priority control proved the cause.
- **Test level through the convolution, not the IR's peak.** The SRC gain was inverted in both the code and the design note, and only a test of the sum of the samples caught it.
- **Delete an editor as the host does.** Call `editorBeingDeleted` first, or the next `createEditorAndMakeActive` returns null.
- **Don't pass long patch scripts through a bash heredoc here.** Two failed on quoting before running. Write the script to the scratchpad and run the file.
- **Shared icons are open paths.** They have to be stroked; filled they show as blobs or nothing.

## Typing values (v1.0.1)

Applies to every knob and fader in all five plugins, through `ParamControl` in `fancy-core`. Clicking the value text opens a box, as does Ctrl-click anywhere on the control. Enter, clicking away or losing focus applies, and Esc cancels.

- **Header knob:** it shows no value text, so it opens with Ctrl-click only. The EDIT drawer's Output is the same parameter and has clickable text.
- **Double-click on value text:** opens the box rather than resetting. Double-click on the knob or fader itself still resets.
- **Click detection:** JUCE doesn't move focus when a plain component is clicked, so the box watches clicks through a global mouse listener while it's open.
- **Unchanged text isn't applied.** The box shows a rounded value, so applying it would move the control by itself.
- **The box sits on the layout root,** not the control, so it can be wider than a fader and still follows the UI size menu.
- **Log ranges clamp before the log.** A typed negative frequency was otherwise a NaN.
- **Test windows must be opaque.** A layered window that was never painted can't be hit-tested, so `ComponentPeer::handleMouseEvent` delivers nothing to it. `tests/ValueEntryTests.cpp` sends gestures straight to the control, and only click-away goes through the window, because the box watches for that on the desktop.

## Figma

File `v9yFzkjuAzh1qzjEweURsh`, "ReaPlugs (vst GUI) (Community)". `design/figma-spec.json` holds every frame, position, fill, radius and font; `design/` has 2× renders of the frames and main components; `design/icons/` has the glyph SVGs. Commit the data, never the token.

Values that replaced the brief's:

| Item | Brief | Figma |
|---|---|---|
| Scale | Kicker ~270 × 290 at 100 % | 392 × 472 once the 22 px host title bar is removed. The screenshots were about 2/3 scale, so §2.4 sizes and §2.2 font sizes don't apply |
| Other frames, title bar removed | Comp ~620 × 330, EQ ~560 × 380, Verb ~600 × 420 | Comp 834 × 527, EQ 802 × 625, Verb 866 × 677 |
| Knob travel | 270° | 300° from 7 o'clock |
| Unlit arc | `--knob-track` | None |
| Pointer | Small triangle | Rounded triangle; sizes in `Knob::Style` |
| Default mark | Not specified | 6 px #4C4C4C dot, shown on some knobs only |
| Text | #EDEDED, 12–13 px | #FFFFFF, Inter 14 and 16 px, numbers on scales 11 px |
| Secondary text | #909090 | #8D8D8D |
| Teal, red, purple, blue, green | #00A884, #E84E3C, #9648BA, #3690CC, #2ACC77 | #00A985, #E74D3D, #9A59B5, #3598DB, #2DCC70 |
| Header controls | #282828 | #303030, radius 3 |
| Checkbox | White tick | #141414 tick, size 14, radius 3 |
| Fader | Grey cap | #141414 handle with accent outline; 2 px track, 6 px fill, default dot |
| Meter | 5 stops | #FFCF07 to #00FF96, 8 px bars with 1 px gap in a 17 px well |
| ON pill | Light icon | Black icon and text |

Figma uses SF Compact Display for the scale numbers. That font can't be bundled, so Inter is used instead.

## Kicker deviations from Figma

- **Dry and Freq pointers:** Figma draws Dry 0 dB at full travel and Freq 50 Hz at 30 %. The §3.1 ranges put them at 83 % and 37 %, and the brief says functional specs win. Wet at -12 dB matches.
- **Header knob:** Figma draws it near maximum, while the build shows Output at 0 dB, its default.
- **Peak-hold lines:** added on the meters as §2.3 specifies. Figma doesn't show them.
- **Meter tops:** match the -5.2 and -5.4 readouts. Figma's bars are drawn at about -4.5 and -6.
- **Caption and title:** shows "Fancy Kicker", and the host draws the window title and border.
- **About:** text only. The five plugin icons aren't in the Figma file; they exist only in `reference/01__fancy_reaplugs.png`.

## Tested and not tested

Tested in CI: DSP, state, presets, automation, bypass and layouts through pluginval, editor opening, rendered screenshots.

Not tested by anything yet, because CI can't click: graph handles and their gestures, IR trim handles, dragging modules, the add panel, the chain strip, file dialogs, window resizing, drag, shift-drag, double-click, mouse wheel, right-click menus, the preset and save dialogs, the UI size menu, opening and closing the EDIT drawer, and the bypass dimming. All of these are on the REAPER checklist.

Typing values is tested in CI with real mouse events sent through a window (`tests/ValueEntryTests.cpp`). Keyboard focus inside REAPER is not, so that part stays on the checklists.

`scripts/get-latest.ps1` and `scripts/install.ps1` have installed into the real `%USERPROFILE%\VST3-Dev`.

## Build times

| Run | Total | Build | sccache hit rate |
|---|---|---|---|
| Skeleton, cold | 19m18s | 858 s | 0 % |
| Skeleton, warm | 2m17s | 64 s | 98 % |
| Kicker, large change | 5m48s | | 75 % |
| Kicker, small change to shared code | 3m19s–4m11s | | |
| Three plugins, code change | 4m59s–5m11s | | |
| Four plugins, code change | 4m48s–5m30s | | |
| Five plugins, code change | about 6m | | |

Target: warm one-plugin build under about 6 minutes (met). The full-suite target of 15 minutes is on track, at about 5 minutes for four plugins with a warm cache. The runner image changes about weekly, which refreshes the dependency cache and adds about 4 minutes of configure time to the next run.

## Decisions

- JUCE pinned to `8.0.17-security`, the newest tag on the JUCE 8 line. JUCE 9 exists, but the brief asks for JUCE 8.
- Compiler cache is sccache, because the cache action's README recommends it on Windows.
- Static MSVC runtime, so the plugins don't depend on the VC++ redistributable.
- Plugin processor and editor sources are compiled directly into `fancy-screens` and `fancy-tests`. Each plugin's `createPluginFilter()` sits in a separate `PluginEntry.cpp` so the symbols don't clash.
- CI found a real click: after an oversampling change, the dry path restarted at full level once the emptied delay line refilled. The output now stays silent until the delay and filters have settled.
- `fancy_core` is plain C++ for DSP. `fancy_ui` compiles into each consumer so it picks up that target's JUCE settings.
- Git identity is set in this repo only, using the GitHub no-reply address.
- All geometry is in native Figma units at 100 %. The Kicker window is 392 × 472, and the user confirmed they prefer this larger size.
- Knob pointers follow the parameter value and range, not the positions drawn in Figma. The user confirmed this.
- Kicker and Exciter meters are a stereo pair per side, with one readout per pair, as the zoomed mockup shows.
- Header knob and the EDIT drawer's Output are the same parameter. The drawer is an overlay on the left panel.
- Kicker `q` range (0.5–10, default 2.0) and `output` range (-24…+12 dB) were chosen here; the brief doesn't give them.
- The CI build step runs with `-k 0`, so one failed file doesn't hide the compile errors in the others.
- `tools/screens` also renders `gallery`, a sheet of the fader, dropdown and other controls the Kicker doesn't use.

## Open questions

- **Figma token.** A personal access token was pasted into the conversation and needs revoking and replacing. Do not store it anywhere in this repo. The Figma data is committed, so later sessions only need the token to re-export.
- pluginval's Steinberg VST3 validator step now runs, with the validator CI builds for Phase 2.1 H.7.

## Known warnings

- GitHub reports that `hendrikmuhs/ccache-action` and `ilammy/msvc-dev-cmd` target Node.js 20 and are being run on Node.js 24. Both work today.
