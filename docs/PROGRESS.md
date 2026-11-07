# Progress

## Status

- Skeleton workflow: done.
- `fancy-core`: done, except the Verb module card, which is deferred until Verb needs it.
- Fancy Kicker: done, signed off in REAPER by the user.
- Fancy Exciter: done, signed off in REAPER by the user.
- Fancy Comp: done, signed off in REAPER by the user.
- Fancy EQ: built and green in CI, **waiting for the REAPER check** in `docs/eq-checklist.md` (§7 checks 3 and 7).
- Next: Fancy Verb, once the EQ is signed off.

Latest run: LATEST_RUN.

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
| 3 | State survives a REAPER project reload | **Waiting for the user**; state round-trip and corrupted-state tests pass in CI |
| 4 | §5.1 tests | Pass, 94 test cases across the suite. 18 kHz +6 dB bell at 44.1 kHz is within 0.42 dB of analog up to 20 kHz. Responses at 44.1–192 kHz are within 0.5 dB below 20 kHz for the tested types. A -12 dBFS sine reads -12.1 dB. Numbers in `docs/measurements/eq.txt` |
| 5 | Screenshots compared with the reference | Done against both Figma frames; `docs/screens/eq-vs-reference.png` shows Figma on the left and the build on the right |
| 6 | CPU against the §4.11 target | Pass: 0.37–0.46 % for 8 bands without oversampling across runs, against < 1 %. About 1 % at 2×, 1.9 % at 4×, and 0.9–1.1 % in Linear on CI runners |
| 7 | Installed and confirmed in REAPER | **Waiting for the user** |

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

Not tested by anything yet, because CI can't click: graph handles and their gestures, window resizing, drag, shift-drag, double-click, ctrl-click typing, mouse wheel, right-click menus, the preset and save dialogs, the UI size menu, opening and closing the EDIT drawer, and the bypass dimming. All of these are on the REAPER checklist.

`scripts/get-latest.ps1` and `scripts/install.ps1` were run end to end, with `USERPROFILE` pointed at a scratch folder. They have not yet installed into the real `%USERPROFILE%\VST3-Dev`.

## Build times

| Run | Total | Build | sccache hit rate |
|---|---|---|---|
| Skeleton, cold | 19m18s | 858 s | 0 % |
| Skeleton, warm | 2m17s | 64 s | 98 % |
| Kicker, large change | 5m48s | | 75 % |
| Kicker, small change to shared code | 3m19s–4m11s | | |
| Three plugins, code change | 4m59s–5m11s | | |
| Four plugins, code change | 4m48s–5m30s | | |

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
- pluginval's Steinberg VST3 validator step is skipped, because no validator path is set. The brief doesn't require it.

## Known warnings

- GitHub reports that `hendrikmuhs/ccache-action` and `ilammy/msvc-dev-cmd` target Node.js 20 and are being run on Node.js 24. Both work today.
