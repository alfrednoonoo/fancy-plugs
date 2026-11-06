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

## Design values

Figma is the source of truth: `design/figma-spec.json` holds every frame, control position, fill, radius and font, with renders in `design/`. All window and control geometry is in native Figma units at 100 %: Kicker 392 × 472, with each frame's 22 px host title bar removed. Read a plugin's frame before building it. Use Inter in place of SF Compact Display.

```
bg-header #1E1E1E  bg-body #303030  bg-section #262626  bg-well #141414
controls (dropdowns, buttons) #303030, radius 3
knob face #4F4F4F to #454545, 1 px black 20 % border, no unlit arc
grey-5 #424242  grey-6 #4C4C4C  text #FFFFFF  text-secondary #8D8D8D
teal #00A985  module teal #1BBC9C  red #E74D3D  purple #9A59B5  blue #3598DB  green #2DCC70
meter gradient #FFCF07 to #00FF96; red clip cap #E74D3D
knob travel: 300 degrees from 7 o'clock
```

Figma doesn't use the EQ band blue, green and red variants or the analyzer orange; those keep their §2.1 values.

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
