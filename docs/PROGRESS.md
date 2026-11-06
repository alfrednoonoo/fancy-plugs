# Progress

## Status

- Skeleton workflow: done.
- `fancy-core`: done, except the Verb module card, which is deferred until Verb needs it.
- Fancy Kicker: built and green in CI, **waiting for the REAPER check** in `docs/kicker-checklist.md` (§7 checks 3 and 7).
- Next: Fancy Exciter, once the Kicker is signed off.

Latest run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42241644263 (commit `1c9b644`).

## Kicker acceptance (§7)

| # | Check | Result |
|---|---|---|
| 1 | Actions run green, artifact contains the plugin | Pass, run above |
| 2 | pluginval strictness 10 | Pass, GUI tests on |
| 3 | State survives a REAPER project reload | **Yours to do**; state round-trip and corrupted-state tests pass in CI |
| 4 | §5.4 tests | Pass, 15 test cases and 52,834 assertions; numbers in `docs/measurements/kicker.txt` |
| 5 | Screenshots compared with the reference | Done, see deviations; `docs/screens/kicker-vs-reference.png` |
| 6 | CPU against the §4.11 target | Pass: 0.07–0.15 % against < 0.5 %, measured on CI runners with every option on |
| 7 | Installed and confirmed in REAPER | **Yours to do** |

## Kicker deviations from the reference

Only the first is a behaviour choice; the rest are small visual differences.

- **Knob positions:** the mockup draws Dry 0 dB near full travel and Freq 50 Hz further left than the §3.1 ranges allow. Knob angles here follow the ranges, so Dry 0 dB sits at 3 o'clock.
- **Grey dot beside Freq:** left out. It appears only on some knobs in the mockups and has no stated meaning.
- **Unlit arc:** visible in dark grey as §2.3 specifies. It's barely visible in the mockup.
- **Peak-hold lines:** added on the meters as §2.3 specifies. The mockup doesn't show them.
- **Meter tops:** match the -5.2 and -5.4 readouts. The mockup's bars are drawn lower than its own readouts.
- **About:** the Parameter menu's About item shows a text box with the attribution. The five line-art plugin icons from `reference/01__fancy_reaplugs.png` are not drawn yet.

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
| Kicker, small change to shared code | 3m41s | | |

Target: warm one-plugin build under about 6 minutes (met). The full-suite target of 15 minutes can't be measured until more plugins exist. The runner image changes about weekly, which refreshes the dependency cache and adds about 4 minutes of configure time to the next run.

## Decisions

- JUCE pinned to `8.0.17-security`, the newest tag on the JUCE 8 line. JUCE 9 exists, but the brief asks for JUCE 8.
- Compiler cache is sccache, because the cache action's README recommends it on Windows.
- Static MSVC runtime, so the plugins don't depend on the VC++ redistributable.
- Plugin processor and editor sources are compiled directly into `fancy-screens` and `fancy-tests`. Each plugin's `createPluginFilter()` sits in a separate `PluginEntry.cpp` so the symbols don't clash.
- `fancy_core` is plain C++ for DSP. `fancy_ui` compiles into each consumer so it picks up that target's JUCE settings.
- Git identity is set in this repo only, using the GitHub no-reply address.
- Kicker window is 265 × 315, measured from the mockup, rather than the brief's ~270 × 290.
- Kicker and Exciter meters are a stereo pair per side, with one readout per pair, as the zoomed mockup shows.
- Header knob and the EDIT drawer's Output are the same parameter. The drawer is an overlay on the left panel.
- Kicker `q` range (0.5–10, default 2.0) and `output` range (-24…+12 dB) were chosen here; the brief doesn't give them.
- `tools/screens` also renders `gallery`, a sheet of the fader, dropdown and other controls the Kicker doesn't use.

## Open questions

- **Figma file key.** Not supplied yet. What was sent instead was a personal access token, which needs revoking and replacing. Do not store it anywhere in this repo. Once the key arrives, fetch Figma data and replace measured values.
- pluginval's Steinberg VST3 validator step is skipped, because no validator path is set. The brief doesn't require it.
- `reference/07__ReaEQ.png`: §3.4 asks for the musical note on the frequency display, but its example and the mockup show it after the gain value. Decide when building the EQ.

## Known warnings

- GitHub reports that `hendrikmuhs/ccache-action` and `ilammy/msvc-dev-cmd` target Node.js 20 and are being run on Node.js 24. Both work today.
