# Fancy Plugs

VST3 effects for personal use on Windows. Fancy Kicker, Fancy Exciter, Fancy Comp, Fancy EQ and Fancy Verb are built and signed off in REAPER. Phase 2 adds Fancy Delay, then Fancy XComp and Fancy FIR; see `docs/PROGRESS.md` for status.

## How builds work

Nothing compiles on the local PC. Each push runs `.github/workflows/build.yml` on a GitHub Windows runner, which builds the plugins, runs the DSP tests and pluginval, renders UI screenshots, and uploads the results as artifacts. Changes that only touch `docs/`, `reference/` or Markdown files don't start a build.

To build a single plugin, run the workflow by hand and pick it:

```powershell
gh workflow run build.yml -f plugin=kicker
```

## Installing a build

Run this in the project folder:

```powershell
.\scripts\get-latest.ps1
```

It downloads the newest successful build of the current branch and copies the plugins to `%USERPROFILE%\VST3-Dev`. It doesn't need admin rights. If REAPER is running, it first asks you to close any project using these plugins, because Windows won't overwrite a loaded DLL.

Add `%USERPROFILE%\VST3-Dev` to REAPER's VST plug-in paths once, then after each install go to Options > Preferences > Plug-ins > VST and click Re-scan.

## Attribution

UI design based on "ReaPlugs (vst GUI)" by Eugene Reznik, CC BY 4.0 — https://www.figma.com/community/file/967866927752067729. Modified from the original.

The Inter typeface is used under the SIL Open Font License; see `assets/fonts/Inter-LICENSE.txt`.
