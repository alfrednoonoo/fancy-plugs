# Phase 2 add-on: Fancy Delay, Fancy XComp, Fancy FIR

## Add it to your project

1. Unzip this into your existing project folder (e.g. `C:\dev\fancy-plugs`), letting it merge with the folders already there. It only adds new files:
   - `docs/BUILD_PROMPT_PHASE2.md`: the spec
   - `reference/phase2/*.png`: the three mockups
   - `design/phase2-mockup-src/`: the HTML the mockups were drawn from (the agent can open these for exact layout numbers)
2. Tell your agent:

> Read docs/BUILD_PROMPT_PHASE2.md and the three images in reference/phase2/. Add Phase 2 to AGENTS.md, CLAUDE.md and docs/PROGRESS.md as the work that comes after the five Phase 1 plugins. Don't start building it until Phase 1 passes, unless I say so. Commit and push these files now; docs-only pushes don't trigger a build.

## To start Phase 2 earlier

Replace the last sentence with: "Start Fancy Delay right after the plugin you're working on now."
