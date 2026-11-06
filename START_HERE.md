# Fancy Plugs — starter folder (cloud-build version)

Nothing gets compiled on your PC. GitHub builds the plugins; you download and install them with one command.

## One-time setup (about 400 MB of downloads in total, not 7 GB)

1. **Git for Windows**: https://git-scm.com/downloads/win (default options are fine).
2. **GitHub CLI**: open PowerShell and run `winget install GitHub.cli`
3. Close PowerShell, open a new one, run `gh auth login`, and follow the prompts (GitHub.com → HTTPS → log in with a web browser). Create a free GitHub account first if you don't have one.
4. **Your coding agent**, e.g. Claude Code: `irm https://claude.ai/install.ps1 | iex`
5. In REAPER: Options → Preferences → Plug-ins → VST → add `C:\Users\<you>\VST3-Dev` to "VST plug-in paths". The folder gets created by the first install.

## Optional but recommended: the Figma source file (exact colours and sizes)

1. Create a free Figma account and open https://www.figma.com/community/file/967866927752067729/reaplugs-vst-gui
2. Click **Open in Figma**. This makes your own copy.
3. Copy the file key from your copy's URL: the part after `/design/` (or `/file/`) and before the next `/`.
4. In Figma: avatar → Settings → Security → Personal access tokens → generate one with read-only "File content" access.
5. Save it on your PC (PowerShell): `setx FIGMA_TOKEN "paste-token-here"`. Then open a new terminal.
6. Give the agent only the **file key**, never the token itself.

## Start

1. Unzip this folder to a path without spaces, e.g. `C:\dev\fancy-plugs`.
2. Open a terminal in that folder and run `claude` (or `codex`, or open the folder in Antigravity).
3. Paste this as your first message:

   > Read docs/BUILD_PROMPT.md in full and follow it. All builds run in GitHub Actions; I have no local compiler. Look at every image in reference/ before building each plugin. Build only the new redesigns (below the arrow in each image), with exactly the control types they show: knobs where they use knobs, vertical faders where they use faders. My Figma file key is: ____ (FIGMA_TOKEN is set in my environment). Start with the skeleton workflow.

   Delete the Figma sentence if you skipped that step.

## Every time a build is ready

Close the REAPER project using the plugins, run `.\scripts\get-latest.ps1` in the project folder, then go to REAPER → Preferences → VST → Re-scan.

## Later sessions

"Read AGENTS.md (or CLAUDE.md) and docs/PROGRESS.md, then continue."
