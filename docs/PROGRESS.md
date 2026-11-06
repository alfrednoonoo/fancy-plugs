# Progress

## Status

Milestone: skeleton workflow (in progress).

## Decisions

- JUCE pinned to `8.0.17-security`, the newest tag on the JUCE 8 line. JUCE 9 exists, but the brief asks for JUCE 8.
- Compiler cache is sccache, because the cache action's README recommends it on Windows.
- Static MSVC runtime, so the plugins don't depend on the VC++ redistributable.
- Plugin processor and editor sources are compiled directly into `fancy-screens`. Each plugin's `createPluginFilter()` sits in a separate `PluginEntry.cpp` so the symbols don't clash.

## Open questions

- Figma file key: the first message still had the `____` placeholder. `FIGMA_TOKEN` is set, but no Figma data can be fetched without the key.
