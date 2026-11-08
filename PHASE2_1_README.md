# Phase 2.1 add-on (updated): styled pop-ups, workflow, dynamic EQ, upward compression, adaptive noise

## Add it to your project

1. Unzip into your project folder (e.g. `C:\dev\fancy-plugs`), merging with the folders already there, and let it overwrite the older Phase 2.1 files. Don't re-extract the older Phase 2.0.1 add-on afterwards: it contains an out-of-date copy of this spec. It contains:
   - `docs/BUILD_PROMPT_PHASE2_1.md`: the updated spec (new item S: styled menus, tooltips, value editors and dialogs)
   - `reference/phase2_1/14__phase2_1_controls.png`: the new controls
   - `reference/phase2_1/16__menus_mockup.png`: the menu/tooltip/dialog design
   - `design/phase2_1-mockup-src/`: the HTML both mockups were drawn from
2. If the market report isn't in the repo yet, copy it to `docs/2026-11-market-report-detailed.md`.
3. Tell your agent:

> docs/BUILD_PROMPT_PHASE2_1.md has been updated with two new items: S (styled pop-ups, shown in reference/phase2_1/16__menus_mockup.png) and H (host compatibility, including offline rendering). Phase 2.0.1 is complete, so Fancy Gate is one of the nine existing plugins that 2.1 modifies, not something to build. Order is S → H → W → D → U → A, starting now. Update AGENTS.md, CLAUDE.md and docs/PROGRESS.md, then start S by writing the pop-up audit in docs/phase2_1-S.md before any code.
