# Fancy Plugs: agent notes

`CLAUDE.md` and `AGENTS.md` are identical; change both together. The full brief is `docs/BUILD_PROMPT.md` and its rules override this summary. Session state is in `docs/PROGRESS.md`: read it first and update it at the end of each session.

## What this is

Five original VST3 effects with a shared UI library, for personal use on Windows 10/11 x64. No installers, signing, licensing, updates or telemetry. Never copy or reference Cockos binaries or source. Bundle IDs are `com.fancyplugs.*`.

## Build order

Finish each step's §7 acceptance checks before starting the next:

1. `fancy-core` (shared DSP + UI components)
2. Fancy Kicker
3. Fancy Exciter
4. Fancy Comp
5. Fancy EQ
6. Fancy Verb

Write `docs/<plugin>.md` before coding each plugin: DSP algorithm, parameters (range, default, unit, skew), latency, state format, and how each §5 fix is handled.

## Working rules

- Ask before changing the parameter list or visual layout, or adding a dependency not named in the brief.
- Look at every image in `reference/` before building each plugin (index in §9).
- Commit after each milestone.

## Cloud-build workflow

There is no local compiler and there never will be one. Every build runs on GitHub's `windows-latest` runners.

- Loop: edit, commit, push, `gh run watch`; on failure read `gh run view --log-failed`, fix, push.
- Never say something builds or passes until the Actions run for that commit is green.
- Batch related changes into one push; the private repo has about 1,200 Windows minutes a month.
- UI: download `screens-<sha>` with `gh run download` and compare it to `reference/` before claiming a match.
- Install on the PC with `scripts\get-latest.ps1`, into `%USERPROFILE%\VST3-Dev`. Never write to `C:\Program Files`.
- The only test on the user's machine is listening in REAPER.

## Build the NEW redesign exactly

Build only what is below the arrow in each image, with its exact control types: knobs where it has knobs, faders where it has faders. Take nothing from the old grey REAPER version above the arrow.

Source priority: Figma data > §2.1/§2.4 measured values > screenshots. The control map below and the §3/§5 functional changes apply even if Figma differs.

Do not copy literally: host window title bars, the "[LOOSER]" tag (show just the plugin name), "Treshold" (use Threshold), Verb Pre-delay in dB (use ms).

## §2.5 control map

| Plugin | Knobs | Vertical faders | Other controls |
|---|---|---|---|
| Kicker | Freq (large), Wet, Dry | none | `⚙ EDIT` link; In/Out meters |
| Exciter | Freq, Clip Boost, Harmonics, Mix Back | none | `⚙ EDIT` link; In/Out meters |
| Comp | Ratio, Knee, Attack, Release, Pre-comp, RMS size, Lowpass, Highpass | **Threshold** (over the input meter), **Wet**, **Dry** | GR meter, Out meter; Detector input dropdown; checkboxes Weird, Classic, Auto, Link channels, Auto make-up, Preview filter, Limit output |
| EQ | Band editor: Freq, Gain, Q | **Gain** (right edge) | Draggable band handles on the graph; band tabs 1–4; band-type dropdown; power / + / × buttons; checkboxes Log-scale automated freq., Show grid, Show phase, Compact mode |
| Verb | Pre-delay, Center, Width (small), all module parameters (Lowpass, Highpass, Start pos., Length, Spacing, Error, ErrDst, Start Vol., End Vol., …) | **Wet**, **Dry** | IR waveform display with trim handles; FILE module file dropdown, `<` `>` buttons, Browse…, Mix to mono checkbox, SRC-gain dropdown; module power / × / + / drag handle; chain strip chips; NORMALIZE toggle; ⚙ SETUP |
| All | Small teal output/mix knob in the header | none | Preset dropdown, `+ New`/`+`, `Parameter`/`Prm`, I/O pill, `⏻ ON` pill |

If a control isn't in this table and isn't visible in the redesign, ask before choosing its type. New controls that aren't in the mockups go in drawers or popovers as dropdowns, toggles or small knobs; they never change the visible main layout.

## Colour tokens (§2.1)

```
bg-page #121212   bg-header #1E1E1E   bg-body #303030    bg-section #262626
bg-well #141414   bg-verb #292929     bg-verb-alt #1F1F1F
knob-body #4B4B4B (gradient #4F4F4F to #454545)   knob-track #212121
grey-3 #282828    grey-5 #434343      grey-6 #4C4C4C
text-secondary #909090   text-primary #FFFFFF at ~92 % (#EDEDED)

accent-yellow #F1C315  accent-orange #E77E23  accent-red #E84C3C  accent-purple #965AB7
accent-blue #3798DC    accent-teal #1EBAA1    accent-green #2ACC77

Prefer inside the plugins:
ui-teal #00A884   ui-teal-bright #00B478   ui-red #E84E3C    ui-blue #3690CC
ui-purple #9648BA  ui-purple-ir #9858B8 (top edge #B06AD0)
ui-eq-blue #24AEC6  ui-eq-green #0CC08A   ui-eq-red #C0363C

Meter, loud to quiet: #F7D10A #C7D924 #82E64D #3CF473 #0DFC90; yellow from about -6 dB; red clip cap #E84C3C above 0 dBFS.
EQ band fills: band colour at ~35 % opacity; curves and handles at 100 %.
```

## §4 real-time rules

1. No allocation, locks, file I/O, logging or `shared_ptr` refcount churn on the audio thread. Use atomics and lock-free FIFOs for UI to DSP.
2. Smooth every continuous parameter (about 20–50 ms); filters recompute per sample or every ≤16 samples with interpolation. Unit-test with a sweep render.
3. `juce::ScopedNoDenormals` in every process call; DC-safe feedback paths.
4. Report all latency with `setLatencySamples`, update it when it changes, and delay the dry path to match.
5. Work at 44.1–384 kHz and block sizes 1–8192, including variable sizes.
6. VST3 soft bypass (`kIsBypass`) with a short crossfade and a latency-compensated dry path.
7. Versioned state (`stateVersion`), with migration; ignore unknown params; never crash on corrupted data.
8. Enabling automation without changing a value must give bit-identical output (regression test).
9. Mono and stereo buses, plus stereo sidechain on Comp.
10. Factory presets, plus a user preset folder; `+ New` saves a preset.
11. Measure CPU at 48 kHz/128 samples against the §4.11 targets.
