# Phase 2.0.1 add-on: Fancy Gate

## Add it to your project

1. Unzip into your project folder (e.g. `C:\dev\fancy-plugs`), merging with the folders already there. It adds:
   - `docs/BUILD_PROMPT_PHASE2_0_1.md`: the Fancy Gate spec
   - `reference/phase2_0_1/15__gate_mockup.png`: the mockup
   - `design/phase2_0_1-mockup-src/`: the HTML the mockup was drawn from
   - `docs/BUILD_PROMPT_PHASE2_1.md`: an **updated** Phase 2.1 spec. It now waits for Fancy Gate and includes it in the workflow item. Let it overwrite the old copy.
2. Tell your agent:

> Read docs/BUILD_PROMPT_PHASE2_0_1.md and reference/phase2_0_1/15__gate_mockup.png. Fancy Gate is built after Phase 2 and before Phase 2.1; docs/BUILD_PROMPT_PHASE2_1.md has been updated to match. Update AGENTS.md, CLAUDE.md and docs/PROGRESS.md with the new order, then write docs/gate.md before any code.
