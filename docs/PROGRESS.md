# Progress

## Status

Skeleton workflow: done and green. Next: `fancy-core` and Fancy Kicker, once the plan is confirmed.

Run: https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42236004975 (commit `be267a9`).

The Kicker is an empty pass-through at this stage, and none of the §7 acceptance checks apply to it yet.

## Build times (one plugin plus tests and screenshots)

| Run | Total | Configure | Build | sccache hit rate |
|---|---|---|---|---|
| Cold, [42232232069](https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42232232069) | 19m18s | 236 s | 858 s | 0 % |
| Warm, [42236004975](https://github.com/alfrednoonoo/fancy-plugs/actions/runs/42236004975) | 2m17s | 20 s | 64 s | 98 % |

Target: warm one-plugin build under about 6 minutes (met). The full-suite target of 15 minutes can't be measured until more plugins exist. The runner image changes about weekly, which refreshes the dependency cache and adds about 4 minutes of configure time to the next run.

## Verified in CI

- JUCE and Catch2 fetch, and the VST3 and Standalone targets build.
- pluginval strictness 10 ran its full suite and reported SUCCESS, with GUI tests enabled.
- `fancy-screens` wrote 270×290 and 540×580 PNGs. No JUCE splash screen appears in them.
- The artifact contains `Fancy Kicker.vst3` and `install.ps1`, and downloaded correctly with `gh run download`.

## Not verified

- `scripts/get-latest.ps1` and `scripts/install.ps1` have been syntax-checked but not run.
- Nothing has been loaded in REAPER.
- pluginval's Steinberg VST3 validator step is skipped, because no validator path is set.

## Decisions

- JUCE pinned to `8.0.17-security`, the newest tag on the JUCE 8 line. JUCE 9 exists, but the brief asks for JUCE 8.
- Compiler cache is sccache, because the cache action's README recommends it on Windows.
- Static MSVC runtime, so the plugins don't depend on the VC++ redistributable.
- Plugin processor and editor sources are compiled directly into `fancy-screens`. Each plugin's `createPluginFilter()` sits in a separate `PluginEntry.cpp` so the symbols don't clash.
- Git identity is set in this repo only, using the GitHub no-reply address.

## Open questions

- Figma file key: the first message still had the `____` placeholder. `FIGMA_TOKEN` is set, but no Figma data can be fetched without the key.
- Kicker assumptions awaiting confirmation (see the end of the session's plan message): header knob is the output trim, window height, EDIT drawer as an overlay.

## Known warnings

- GitHub reports that `hendrikmuhs/ccache-action` and `ilammy/msvc-dev-cmd` target Node.js 20 and are being run on Node.js 24. Both work today.
