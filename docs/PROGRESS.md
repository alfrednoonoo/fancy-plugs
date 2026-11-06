# Progress

## Status

- Skeleton workflow: done.
- `fancy-core`: done, except the Verb module card, which is deferred until Verb needs it.
- Fancy Kicker: built and green in CI, **waiting for the REAPER check** in `docs/kicker-checklist.md` (§7 checks 3 and 7).
- Next: Fancy Exciter, once the Kicker is signed off.

Latest run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42246329451 (commit `5635d38`).

## Kicker acceptance (§7)

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass, GUI tests on |
| 3 | State survives a REAPER project reload | **Yours to do**; state round-trip and corrupted-state tests pass in CI |
| 4 | §5.4 tests | Pass, 15 test cases and 52,834 assertions; numbers in `docs/measurements/kicker.txt` |
| 5 | Screenshots compared with the reference | Done against the Figma frame, see deviations; `docs/screens/kicker-vs-reference.png` shows Figma on the left and the build on the right |
| 6 | CPU against the §4.11 target | Pass: 0.07–0.15 % against < 0.5 %, measured on CI runners with every option on |
| 7 | Installed and confirmed in REAPER | **Yours to do** |

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

Not tested by anything yet, because CI can't click: drag, shift-drag, double-click, ctrl-click typing, mouse wheel, right-click menus, the preset and save dialogs, the UI size menu, opening and closing the EDIT drawer, and the bypass dimming. All of these are on the REAPER checklist.

`scripts/get-latest.ps1` and `scripts/install.ps1` were run end to end, with `USERPROFILE` pointed at a scratch folder. They have not yet installed into the real `%USERPROFILE%\VST3-Dev`.

## Build times

| Run | Total | Build | sccache hit rate |
|---|---|---|---|
| Skeleton, cold | 19m18s | 858 s | 0 % |
| Skeleton, warm | 2m17s | 64 s | 98 % |
| Kicker, large change | 5m48s | | 75 % |
| Kicker, small change to shared code | 3m19s–4m11s | | |

Target: warm one-plugin build under about 6 minutes (met). The full-suite target of 15 minutes can't be measured until more plugins exist. The runner image changes about weekly, which refreshes the dependency cache and adds about 4 minutes of configure time to the next run.

## Decisions

- JUCE pinned to `8.0.17-security`, the newest tag on the JUCE 8 line. JUCE 9 exists, but the brief asks for JUCE 8.
- Compiler cache is sccache, because the cache action's README recommends it on Windows.
- Static MSVC runtime, so the plugins don't depend on the VC++ redistributable.
- Plugin processor and editor sources are compiled directly into `fancy-screens` and `fancy-tests`. Each plugin's `createPluginFilter()` sits in a separate `PluginEntry.cpp` so the symbols don't clash.
- `fancy_core` is plain C++ for DSP. `fancy_ui` compiles into each consumer so it picks up that target's JUCE settings.
- Git identity is set in this repo only, using the GitHub no-reply address.
- All geometry is in native Figma units at 100 %. The Kicker window is 392 × 472.
- Kicker and Exciter meters are a stereo pair per side, with one readout per pair, as the zoomed mockup shows.
- Header knob and the EDIT drawer's Output are the same parameter. The drawer is an overlay on the left panel.
- Kicker `q` range (0.5–10, default 2.0) and `output` range (-24…+12 dB) were chosen here; the brief doesn't give them.
- `tools/screens` also renders `gallery`, a sheet of the fader, dropdown and other controls the Kicker doesn't use.

## Open questions

- **Figma token.** A personal access token was pasted into the conversation and needs revoking and replacing. Do not store it anywhere in this repo. The Figma data is committed, so later sessions only need the token to re-export.
- **Dropdown fill.** The Figma Dropdown/Small component is #424242, but the header instances override it to #303030. Check which the Comp Detector input uses when building the Comp.
- pluginval's Steinberg VST3 validator step is skipped, because no validator path is set. The brief doesn't require it.
- `reference/07__ReaEQ.png`: §3.4 asks for the musical note on the frequency display, but its example and the mockup show it after the gain value. Decide when building the EQ.

## Known warnings

- GitHub reports that `hendrikmuhs/ccache-action` and `ilammy/msvc-dev-cmd` target Node.js 20 and are being run on Node.js 24. Both work today.
