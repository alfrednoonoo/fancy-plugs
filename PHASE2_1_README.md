# Phase 2.1 add-on: workflow, dynamic EQ, upward compression, adaptive noise

## Add it to your project

1. Unzip into your project folder (e.g. `C:\dev\fancy-plugs`), merging with the folders already there. It only adds new files:
   - `docs/BUILD_PROMPT_PHASE2_1.md`: the spec
   - `reference/phase2_1/14__phase2_1_controls.png`: the mockup of every new control
   - `design/phase2_1-mockup-src/`: the HTML the mockup was drawn from
2. If the market report isn't in the repo yet, copy it to `docs/2026-11-market-report-detailed.md` (the spec refers to it there).
3. Tell your agent:

> Read docs/BUILD_PROMPT_PHASE2_1.md and reference/phase2_1/14__phase2_1_controls.png. Add Phase 2.1 to AGENTS.md, CLAUDE.md and docs/PROGRESS.md. Then start with item W (workflow) and write its design note before any code. Saved projects from the current version must still load and sound identical.
