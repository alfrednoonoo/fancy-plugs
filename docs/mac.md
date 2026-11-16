# Fancy Plugs on Mac: plan

**Approved by the user on 15 November 2026, every recommended option (R):** VST3 and AU, no Developer ID (ad-hoc signing and the install script), one universal build, macOS 11 and later, Gate's AU as a music effect, and a listening check on a Mac before the Mac release. `docs/BUILD_PROMPT_PHASE3.md` makes Mac its own phase, after Windows is finished. This note is the plan for it: what has to change, how it's built and tested without a Mac on this side, and the decisions only the user can make. No code until it's approved.

Goal: all fourteen plugins on macOS, on Intel and Apple silicon (M1 and later), sounding and looking as they do on Windows, from the same source and the same CI.

## 1. Where the code stands

JUCE is cross-platform, and almost everything is already portable. A search of the source for Windows-only code finds five places:

| Where | What's Windows-only | Change |
|---|---|---|
| `plugins/verb/src/VerbLibrary.cpp`, `plugins/verb/CMakeLists.txt` | The IR library is a Windows resource (`.rc`, `FindResourceW`) | Windows keeps its resource. On Mac the same file is embedded with the assembler's `.incbin`, in step 2, when there's a Mac build to test it: JUCE binary data would turn the 22 MB file into about 100 MB of source (`docs/verb.md`). Same bytes, so the same IRs |
| `plugins/verb/src/Convolver.cpp` | SSE intrinsics (`_mm_*`) in the convolver's inner loops | Keep SSE on x86 (Intel Macs too); on Apple silicon, plain loops doing the same float operations in the same order (`ConvolverKernels.h`), which a test proves bit-identical to SSE |
| `tools/bench/src/Host.cpp`, `Main.cpp` (FancyBench) | Thread priority and timing via Win32 | A small platform layer: Win32 on Windows, Mach thread policy and `mach_absolute_time` on Mac |
| `tests/VerbTests.cpp`, `tests/FirQ27Tests.cpp` | CPU benchmarks use Win32 priority and cycle counters | The same platform layer; the correctness tests are already portable |
| `scripts/install.ps1`, `get-latest.ps1` | PowerShell, `%USERPROFILE%\VST3-Dev` | `install.sh` and `get-latest.sh`, into `~/Library/Audio/Plug-Ins/VST3` (and `Components` for AU) |

Already fine: fonts (Inter is embedded), Retina scaling (JUCE, and the screenshots already render at 2x), the user preset folder (`userApplicationDataDirectory` is `~/Library` on Mac), and denormal handling (`ScopedNoDenormals` sets flush-to-zero on ARM too).

## 2. Decisions

- **Decision 1: formats.**
  - **A (R).** VST3 and Audio Unit (AU). Logic Pro and GarageBand load only AU; REAPER, Live, Cubase, Bitwig and Studio One load VST3. JUCE builds both from the same code, with `auval` (Apple's AU validator) added to CI.
  - **B.** VST3 only. Less to test, but no Logic or GarageBand.
- **Decision 2: signing.** macOS blocks plugins downloaded from the internet unless they're signed with an Apple Developer ID and notarised by Apple. The brief says no signing.
  - **A (R).** No Developer ID: the builds are ad-hoc signed (Apple silicon needs at least that, and the linker does it), and the install script removes the quarantine flag (`xattr -dr com.apple.quarantine`), which is what lets them load. Free, and fine for you and people you hand the zip to, who run one script. A plugin dragged in by hand without the script gets "can't be opened" until the flag is removed.
  - **B.** Developer ID and notarisation: Apple Developer Program, 99 USD a year, with the certificate and an app-specific password stored as GitHub secrets, and CI notarises each release. Plugins then install like any commercial one. It changes the brief's no-signing rule, so it's yours to decide, and it can be added later without other changes.
- **Decision 3: one universal build.** **(R)** Each bundle holds both Intel (`x86_64`) and Apple silicon (`arm64`) code, so there's one download and it works on any Mac. The alternative, two downloads, saves nothing that matters (the bundles are a few MB).
- **Decision 4: oldest macOS.** **(R)** macOS 11 Big Sur, the first with Apple silicon: it's what Apple's current tools support well, and it covers Intel Macs from 2013 on. 10.15 is possible if you know someone still on it.
- **Decision 5: Fancy Gate's MIDI output in AU.** An AU effect can't send MIDI unless it's registered as a "music effect", which Logic lists in a different place. **(R)** Gate's AU is a music effect, so the trigger output works; its VST3 is unchanged. The alternative is a normal AU effect without the MIDI output.
- **Decision 6: who listens.** On Windows the last check is you, in REAPER. On Mac, CI can build, validate and test, but not listen.
  - **A (R).** You, or someone you choose, on a Mac in REAPER (and Logic, for AU), with a Mac section added to each checklist. Ideally one Intel and one Apple silicon Mac; Apple silicon alone is acceptable, because CI runs the Intel code under Rosetta as well as natively (section 3).
  - **B.** No listening check on Mac: CI only. Faster, but nobody hears it before release.

## 3. Build and test, still CI only

There's still no local compiler, Windows or Mac. A second job in the same workflow builds and tests on GitHub's macOS runners, which are free for a public repo, in parallel with the Windows job:

1. Configure with `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` and the deployment target from Decision 4; build every plugin (VST3, AU), FancyBench and the tests with Xcode's clang.
2. Run the whole test suite natively on Apple silicon, then again as Intel code under Rosetta 2, so both halves of the universal binary are tested. If GitHub still offers an Intel macOS runner, the Intel half also runs natively there.
3. pluginval and Steinberg's validator on every VST3; `auval` on every AU.
4. Screenshots, compared with the Windows ones: the layouts must match to the pixel, with text rendering allowed to differ slightly.
5. FancyBench's smoke check and the CPU measurements (section 5).
6. Upload `FancyPlugs-VST3-AU-<sha>-macos` as an artifact next to the Windows one.

The Windows job doesn't change, so a Mac failure can't hide a Windows one.

## 4. Sound across platforms

- **Within a platform** the rule stays as it is: saved projects load and render bit-identically after every change, on Windows and on Mac, each tested on its own.
- **Between platforms** bit-identity isn't possible: the compilers and maths libraries differ (for example, ARM compilers fuse multiply and add by default). A project moved from Windows to Mac must sound the same; the test is that a render of every factory preset on Mac matches the Windows render to better than −100 dB of difference.
- **To keep them close,** clang builds use `-ffp-contract=off`, so no multiply-adds are fused where MSVC doesn't fuse them.

## 5. CPU

Apple silicon runs these plugins at least as fast as the Windows runners, so the §4.11 targets hold. The Mac job measures each plugin with the §0.4 method (five runs, median, ratio to Fancy Comp in the same run) and writes `docs/measurements/mac.txt`. The ratios are the figures to compare with Windows, since absolute numbers differ between machines.

## 6. Packaging and install

The same rules as Windows: no installer, no licence, no updater.

- **Release asset:** `FancyPlugs-<version>-macos.zip` beside the Windows zip: the `.vst3` and `.component` bundles, `install.sh`, and FancyBench in `tools/`.
- **`install.sh`:** copies the bundles to `~/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/Components` (the user's own folders, so no administrator password), removes the quarantine flag (Decision 2 A), and asks Logic to rescan AUs the next time it starts.
- **`get-latest.sh`:** as `get-latest.ps1`, for the newest green build.
- **README:** a Mac install section, and the Windows section kept as it is.

## 7. Build order

Each step is one push or a few, green on both platforms before the next:

1. **Portable code, Windows only:** the Verb library as binary data, the convolver's NEON path, the platform layer for FancyBench and the benchmarks. Windows must stay bit-identical and within its CPU figures. Nothing Mac-specific is built yet, so this can't break anything a user has.
2. **Mac job, VST3:** the universal build, tests natively and under Rosetta, pluginval, Steinberg's validator, screenshots.
3. **AU:** `auval`, Gate as a music effect, the AU sidechain in Logic's terms, `docs/host-compat.md` updated for Logic, Live and REAPER on Mac.
4. **Packaging:** `install.sh`, `get-latest.sh`, the release zip, the README, a Mac section in each checklist.
5. **Listening check** (Decision 6), then the release with both zips.

Steps 1 and 2 need no decisions beyond 3 and 4; steps 3 to 5 need Decisions 1, 2, 5 and 6.

## 9. As built

- **Step 1 (push 1):** `fancy/dsp/CpuPause.h` (`pause` on x86, `yield` on ARM) replaces `_mm_pause`. The convolver's kernels moved to `plugins/verb/src/ConvolverKernels.h`: SSE on x86 as before, and portable versions with four running sums in SSE's lane order and the complex product written as SSE's `addsub` computes it. `tests/PortabilityTests.cpp` checks they match SSE bit for bit. FancyBench and the CPU tests use `tools/bench/src/Platform.h` (priority, CPU time, thread cycles, short waits, timer resolution, command line): Win32 calls moved there unchanged; the Mac half (Mach time-constraint policy, `CLOCK_THREAD_CPUTIME_ID`, `getrusage`) is written but first compiled in step 2. The Verb library stays a Windows resource. Push 1 (run 43765202754) failed to build: FancyBench still called `SetEnvironmentVariableW`, now `platform::setEnvironment`.
- **Step 2 (pushed with step 1's fix, run 43765631996):** a `macos` job in `build.yml` (macos-latest, universal `arm64;x86_64`, macOS 11, ccache), `continue-on-error` until it's first green: build, a `lipo` check that every plugin binary is universal, the tests natively and under Rosetta, measurements, Steinberg's validator, pluginval, the host harness, FancyBench's smoke checks, screenshots, and artifacts `FancyPlugs-VST3-macos-<sha>` and `screens-macos-<sha>`. `CMakeLists.txt` sets the architectures, the deployment target and `-ffp-contract=off` for clang. The Verb library on Mac: `plugins/verb/CMakeLists.txt` writes `VerbLibraryEmbed.cpp`, which embeds the same `VerbLibrary.bin` with `.incbin` and carries a hash of every input, so the compiler cache can't serve an old library. FancyBench's default `%FANCY_VST3%` on Mac is `~/Library/Audio/Plug-Ins/VST3`.
- **Run 43765631996:** Windows green, so step 1 is done. The Mac job built everything on the first try, and every plugin binary is universal. Tests: 515 of 518 passed on Apple silicon, the same 515 under Rosetta. FIR Resonance's figures matched Windows (3 kHz -2.99 dB at Depth 3, -5.32 dB at Depth 6). Failures, all fixed in the next push:
  - *Ctrl-click to type a value* (2 checks): on a Mac, Ctrl-click is a right-click. The controls already take Cmd-click there, so the test now uses JUCE's `commandModifier`, which is Ctrl on Windows. The Mac manuals will say Cmd-click (step 4).
  - *Thread CPU under Rosetta:* x86 `pause` gave the core away, so the calibration spin used 5 ms of CPU in 200 ms. It now spins without `pause`.
  - *Steinberg's validator* was never built: its steps were skipped after the test failure, and pluginval failed for want of it. Both steps now run unless the job is cancelled.
  - *FancyBench:* `expandPath` turned `/` into `\`, so on Mac no path resolved. It now uses each platform's separator.
  - *Mac CPU timings* had 127 of 1000 blocks preempted: a Mach time-constraint thread that spins is demoted by the kernel. The benchmarks now use the highest QoS class instead.
- **Run 43770725166:** Windows green. Mac: Steinberg's validator, pluginval, the host harness and FancyBench's smoke checks all passed on every plugin. Two tests still failed: a FancyBench test expected the Windows install folder (now per platform), and under Rosetta `CLOCK_THREAD_CPUTIME_ID` counts 24 MHz ticks while claiming nanoseconds, so thread CPU read 2.4 % of the truth. Thread time on Mac now comes from the kernel's `thread_info`.
- **Run 43773992670: step 2 done, both jobs green.** Mac: 516 of 518 tests passed natively and 517 of 519 under Rosetta, with 2 skipped each time as on Windows; Steinberg's validator, pluginval (strictness 10), the host harness and FancyBench passed on all fourteen universal plugins. Mac CPU (Apple silicon runner, paced): Fancy Comp 0.54 %, FIR Resonance 0.50 % (0.94 x Comp), Verb zero latency 1.16 % with no block over budget. `continue-on-error` is removed, so a Mac failure now fails the run.
- **Step 3 (push 1):** `fancy_add_plugin` builds `VST3 AU Standalone` on Apple; every AU is an effect (`aufx`, sandbox-safe) except Gate, a music effect (`aumf`, `AU_MUSIC_EFFECT`). CI stages the `.component` bundles, checks they're universal, installs them in the runner's Components folder and runs `auval -strict` on each, with type, subtype and manufacturer read from its Info.plist. `docs/host-compat.md` has a macOS section (AU notes, Logic and REAPER-on-Mac checklists). The Mac artifact is now `FancyPlugs-macos-<sha>`: as `FancyPlugs-VST3-macos-<sha>` it matched `get-latest.ps1`'s `FancyPlugs-VST3-*` pattern, so runs 43765631996 to 43773992670 would have installed the Mac bundles' files alongside the Windows ones.
- **Run 43777506992:** Windows green. `auval -strict` passed 13 of 14 AUs, Gate included as `aumf`. MultiComp failed: "Log-scale automated freq." moves every band's frequency to a new 0-1 position, so auval saw eight parameters change that it hadn't set. It (and EQ's, the same switch) is now a meta parameter, on Mac only, so Windows stays as released. FancyBench's smoke check also failed once on Mac: Delay's CPU ratio read 0.74 x Comp against 0.42 x in the in-process test, and MultiComp moved from 1.16 x to 1.83 x on the same code, so this looks like runner noise; Delay passed at 0.38 x in the run before. The check is unchanged; if it keeps flaking on Mac, FancyBench will take the median of several timings. CI pending.

## 8. Risks

- **The IR library and the convolver** are the only DSP that changes, in step 1. Verb's bit-identity tests cover both, on Windows first.
- **Text rendering** differs between Windows and Mac, so some labels may need a pixel more room; the screenshots will show it.
- **GitHub's Intel macOS runners** are being retired. Rosetta on Apple silicon runners covers the Intel code if they go; native Intel hardware would then be tested only by a listener with an Intel Mac.
- **Unsigned plugins** (Decision 2 A) depend on the install script; a future macOS could make that harder, which Decision 2 B would solve.
