# Build Prompt: "Fancy Plugs" — Five VST3 Audio Plugins (redesigned ReaPlugs-style suite)

You are a senior audio DSP + C++ plugin engineer. Build a suite of five VST3 audio effect plugins with a shared UI library, based on the reference design screenshots provided alongside this prompt (if no screenshots are attached, use the written spec below as the source of truth).

**Scope: personal use only.** These plugins are for my own studio and will not be distributed, sold or published. Optimise for sound quality, stability and a faithful UI — not for release engineering. No installers, code signing, notarization, licensing/copy-protection, update checks or telemetry.

Each plugin is an original implementation — do NOT copy, disassemble or reference Cockos binaries or source code. Use distinct names (below), not "Rea*", so they never collide with the stock plugins in the host's FX browser, preset folders or plugin cache (use bundle IDs under `com.fancyplugs.*`). Working product names:

| # | Product name (working) | Inspired by | Accent colour |
|---|---|---|---|
| 1 | **Fancy EQ** | ReaEQ (parametric EQ + analyzer) | Purple / teal / green band colours |
| 2 | **Fancy Comp** | ReaComp (compressor) | Teal, with blue for detector filters |
| 3 | **Fancy Verb** | ReaVerb (modular convolution reverb) | Purple (file/IR), blue (filters), teal (generators) |
| 4 | **Fancy Kicker** | "50 Hz Kicker" JSFX (kick sub enhancer) | Red/coral |
| 5 | **Fancy Exciter** | "Exciter" JSFX (high-end harmonic exciter) | Red/coral |

---

## 0. How to work (read first)

1. **Do not build all five at once.** Work in this order, and get each one passing the acceptance checks in §7 before moving on:
   1. Shared core + UI component library (`fancy-core`)
   2. Fancy Kicker (smallest — validates the whole pipeline end to end)
   3. Fancy Exciter
   4. Fancy Comp
   5. Fancy EQ
   6. Fancy Verb (hardest — convolution engine + modular rack)
2. Before writing code for each plugin, write a short design note (`docs/<plugin>.md`) covering: DSP algorithm, parameter list with ranges/defaults/units/skew, latency, state format, and how each "known issue fix" in §5 is addressed. Then implement.
3. Ask me before making any decision that changes the parameter list, the visual layout, or adds a dependency not listed here.
4. Commit after each milestone with a clear message (initialise a git repo first if there isn't one).
5. **Agent notes file:** at the start, save the key rules from this prompt (build order, "build the NEW redesign exactly — knobs where it has knobs, faders where it has faders", the §2.5 control map, colour tokens, the §1 cloud-build workflow, §4 real-time rules) into `AGENTS.md` and an identical `CLAUDE.md` at the repo root, so whichever coding agent I use next (Claude Code, Codex or Antigravity) picks them up in a new session. Keep this full prompt as `docs/BUILD_PROMPT.md`.
6. This work will span multiple sessions. At the end of each session, update `docs/PROGRESS.md` with what's done, what's next, and any open questions, so the next session can resume.
7. **You cannot compile locally — there is no C++ toolchain on my machine.** Every build happens in GitHub Actions (§1). Your loop is: edit → commit → push → watch the run with `gh run watch` → on failure read `gh run view --log-failed`, fix, push again. Never tell me something "builds" or "passes" until the Actions run for that commit is green. Batch related changes into one push so we don't burn build minutes on every small edit.

## 1. Tech stack

- **Language:** C++20.
- **Framework:** JUCE 8 (CMake build, `juce_add_plugin`). For personal, non-distributed use JUCE's free licensing is fine; keep the JUCE splash screen if the licence tier requires it. Formats: **VST3** only (required). Skip AU/CLAP unless I ask. Standalone target for UI testing.
- **VST3 SDK:** 3.8+ (MIT-licensed). JUCE bundles what it needs.
- **DSP helpers allowed:** JUCE `dsp` module (Oversampling, FFT, IIR, Convolution as a reference only — see §5.3 for why we write our own partitioned convolver), plus any MIT/BSD single-header libs if justified in the design note.
- **No** GPL-incompatible code, no closed-source dependencies.
- **Target: Windows 10/11 x64 only**, VST3 only.
- **NO local build toolchain.** I have not installed Visual Studio, CMake or Ninja and I won't. All compiling, testing and validation runs on GitHub's hosted Windows runners (`windows-latest`, which ships with Visual Studio Build Tools, CMake and Ninja preinstalled). Locally I only have Git, the GitHub CLI (`gh`), my coding agent and REAPER.
- **Repo:** a **private** GitHub repo named `fancy-plugs` under my account. Create it with `gh repo create fancy-plugs --private --source . --push` if it doesn't exist. Private repos on the free plan get a monthly Actions allowance (roughly 1,200 Windows-runner minutes), so builds must be efficient — see below.
- **JUCE:** fetch with CMake `FetchContent` pinned to a specific JUCE 8 tag. Don't vendor JUCE into the repo.
- **Workflow `.github/workflows/build.yml`:**
  - Triggers: `push` to any branch, plus `workflow_dispatch` with an optional `plugin` input (`all` | `kicker` | `exciter` | `comp` | `eq` | `verb`) to build just one.
  - `concurrency:` group per branch with `cancel-in-progress: true`, so a newer push cancels an older running build.
  - Skip the run entirely when only `docs/**`, `reference/**` or `*.md` changed (`paths-ignore`).
  - Steps: checkout → set up MSVC dev environment (e.g. `ilammy/msvc-dev-cmd`) → **cache** the JUCE/FetchContent download and a compiler cache (sccache or ccache via `hendrikmuhs/ccache-action`) → configure with Ninja, Release → build → run Catch2 DSP tests → download pluginval's Windows release and run it at strictness 10 on every built `.vst3` (if pluginval's GUI tests hang on the runner, add `--skip-gui-tests` and note it in PROGRESS.md) → render UI screenshots (see below) → upload artifacts.
  - Target: a warm-cache build of one plugin under ~6 minutes, a full build under ~15. Report actual run times in `docs/PROGRESS.md`.
- **UI screenshots in CI:** add a small console target `fancy-screens` that instantiates each plugin's editor offscreen, renders it with `Component::createComponentSnapshot` at scale 1.0 and 2.0, and writes PNGs. Upload them as a separate artifact `screens-<sha>`. Download them (`gh run download`) and compare against `reference/` yourself before claiming the UI matches.
- **Artifacts:** upload `FancyPlugs-VST3-<short-sha>` containing every built `.vst3` bundle plus `install.ps1`, with `retention-days: 14`.
- **Installing on my PC (no admin rights):** write `scripts/get-latest.ps1`. It uses `gh run download` to fetch the newest successful artifact for the current branch into a temp folder, checks whether REAPER is running (and if so tells me to close the project using these plugins first, because Windows won't overwrite a loaded DLL), copies each `.vst3` bundle into `%USERPROFILE%\VST3-Dev\` (overwriting), and prints "Done — in REAPER: Options → Preferences → Plug-ins → VST → Re-scan". Tell me once to add `%USERPROFILE%\VST3-Dev` to REAPER's "VST plug-in paths". Never write to `C:\Program Files`.
- **Testing:** Catch2 for DSP unit tests and pluginval strictness 10, both in CI. The only test that happens on my machine is listening in REAPER.

Repo layout:
```
/fancy-core        shared DSP utils, parameter helpers, preset manager, UI LookAndFeel + components
/plugins/kicker
/plugins/exciter
/plugins/comp
/plugins/eq
/plugins/verb
/tests
/docs
/assets            fonts, icons
/reference         the 13 design screenshots (read-only, see §9)
/scripts           get-latest.ps1 (runs on my PC: download newest CI build + install)
/tools/screens     the fancy-screens offscreen-render target
/design            Figma exports, if I provide them (see §2.0b)
/.github/workflows build.yml
```

## 2. Visual design system (shared by all five)

The design language is flat, dark, minimal: "more knobs, fewer sliders" (the designer's words — this means *fewer*, not *none*: the redesign deliberately keeps vertical faders for wet/dry, threshold and gain), dark theme with saturated accent colours, "less but better".

### 2.0 Reference images — IMPORTANT

The reference screenshots are in `reference/` at the repo root (see §9 for the file list). Open and look at each one before building the matching plugin.

**Several images show two versions of the same plugin: the CURRENT REAPER version (grey, small, mostly horizontal sliders, macOS-style window with red/yellow/green traffic-light buttons) at the top, then a downward arrow (↓), then the NEW redesign (dark) below it. Build ONLY the new redesign below the arrow, and reproduce its control types exactly: where the redesign uses a knob, build a knob; where it uses a vertical fader (e.g. Verb and Comp Wet/Dry, Comp Threshold, EQ Gain), build a vertical fader; same for checkboxes, dropdowns and buttons. Do not convert faders into knobs or knobs into faders, and do not take any control type from the old version above the arrow. §2.5 lists every control and its type — follow it.**

If you can't open the images, the written spec in §2–§3 is the source of truth; the colours in §2.1 were measured from the flat screenshots.

### 2.0b Figma source file (preferred over screenshots when available)

The designer published the original vector design: **"ReaPlugs (vst GUI)" by Eugene Reznik** — https://www.figma.com/community/file/967866927752067729/reaplugs-vst-gui — licensed **CC BY 4.0** ("Feel free to use it for your own GUIs"). I may duplicate it into my Figma account and give you either:

- **(a) Exports** in `design/`: one PNG at 2× per plugin frame, plus SVGs of the knob, fader, meter, checkbox, ON pill and the five plugin icons; or
- **(b) API access:** the key of my duplicated file and a Figma personal access token, stored as the environment variable `FIGMA_TOKEN` on my machine (never commit it, never print it in logs).

If (b): fetch `GET https://api.figma.com/v1/files/<key>` with header `X-Figma-Token`, and save the useful parts as `design/figma-spec.json` (frame sizes, every fill colour, corner radius, stroke width, font family/size/weight, and the x/y/w/h of every control). Render frames and components with `GET /v1/images/<key>?ids=…&format=png&scale=2` (and `format=svg` for icons) into `design/`. Commit the JSON and images, not the token.

**Priority when sources disagree:** Figma data (exact values) > §2.1/§2.4 measured values > screenshots. Wherever Figma gives an exact value, replace the measured value in this prompt's tokens and note the change in `docs/PROGRESS.md`. The §2.5 control map and my functional changes (§3, §5) still apply even if the Figma file differs.

**Attribution (required by CC BY 4.0):** add to `README.md` and to each plugin's about/info popover: "UI design based on 'ReaPlugs (vst GUI)' by Eugene Reznik, CC BY 4.0 — https://www.figma.com/community/file/967866927752067729". Note that it's modified.

### 2.1 Colour tokens (measured from the reference screenshots)
These values were sampled pixel-by-pixel from the flat screenshots; use them as-is.

```
// Greys — from the "Key UI principles" swatch row and the plugin bodies
--bg-page        #121212   // outer background behind the plugin windows (swatch 1)
--bg-header      #1E1E1E   // top header bar (preset dropdown / + / Prm / ON row)
--bg-body        #303030   // main control area of Kicker/Exciter/Comp (dominant colour)
--bg-section     #262626   // meter panel on the right of Kicker/Exciter; Comp inner boxes
--bg-well        #141414   // inside of meter bars (unlit)
--bg-verb        #292929   // Verb module cards and IR display area
--bg-verb-alt    #1F1F1F   // Verb right column / chain strip
--knob-body      #4B4B4B   // knob face, very subtle vertical gradient #4F4F4F (top) → #454545 (bottom)
--knob-track     #212121   // unlit part of the value arc
--grey-3         #282828   // swatch 3 — dropdowns, header buttons
--grey-5         #434343   // swatch 5 — borders, separators
--grey-6         #4C4C4C   // swatch 6
--text-secondary #909090   // swatch 7 — file paths, units, captions, inactive labels
--text-primary   #FFFFFF   // swatch 8 — knob labels, section titles (render at ~92 % opacity → #EDEDED)

// Accent palette — from the "Key UI principles" accent swatch row (left → right)
--accent-yellow  #F1C315
--accent-orange  #E77E23   // EQ spectrum analyzer line (draw at ~70 % opacity)
--accent-red     #E84C3C   // Kicker & Exciter knob arcs, pointers, values, "⚙ EDIT"
--accent-purple  #965AB7   // EQ band 2; Verb FILE module icon + "Mix to mono" checkbox
--accent-blue    #3798DC   // all LP/HP filter knobs (Comp detector filters, Verb LP/HP)
--accent-teal    #1EBAA1   // palette teal
--accent-green   #2ACC77   // EQ band 4 / high shelf

// In-plugin variants actually used in the mockups (prefer these inside the plugins)
--ui-teal        #00A884   // "⏻ ON" pill fill, Comp knob arcs + values, checkbox fill, fader caps
--ui-teal-bright #00B478   // Verb echo/reverb-generator knobs and module power icons
--ui-red         #E84E3C   // Kicker/Exciter knob arcs (≈ accent-red)
--ui-blue        #3690CC   // Verb/Comp LP/HP knob arcs + values
--ui-purple      #9648BA   // EQ band 2 fill/handle, Verb FILE accents
--ui-purple-ir   #9858B8   // Verb IR waveform (draw with a slight lighter top edge, e.g. #B06AD0)
--ui-eq-blue     #24AEC6   // EQ band 3 (bell boost) fill/handle
--ui-eq-green    #0CC08A   // EQ band 4 (high shelf) fill/handle
--ui-eq-red      #C0363C   // EQ band 1 (low shelf) handle/fill

// Meter gradient (measured top → bottom of the Kicker meter)
// #F7D10A (top / loud) → #C7D924 → #82E64D → #3CF473 → #0DFC90 (bottom / quiet)
// Yellow begins around −6 dB; add a red clip cap #E84C3C above 0 dBFS.
// EQ band fills: the band colour at ~35 % opacity; curves/handles at 100 %.
```

### 2.2 Typography
Inter (bundle the font, OFL licence). Labels 12–13 px medium; values 11–12 px regular in the accent colour of that control; section titles 12 px semibold UPPERCASE with letter-spacing (e.g. `FILE`, `LP/HP`, `ECHO GENERATOR`). All UI must scale (100/125/150/200%) — build everything vector, no bitmaps.

### 2.3 Components (build once in `fancy-core`)
- **Knob:** flat dark circle (`--knob-body`), a small **filled triangle pointer** inside the face pointing at the value angle, and an **outer arc ring** (≈3 px) lit in the accent colour from the minimum (or from centre for bipolar params like gain) to the current value; unlit remainder `--knob-track`. 270° travel. Label above (`--text-primary`), value below in accent colour with unit ("50.0 Hz", "-12.0 dB", "4:1", "42 %"). Interactions: vertical drag, shift = fine, double-click = reset to default, ctrl/cmd-click = type a value, mouse-wheel, right-click = host context menu (automation etc.). Three sizes: L (Kicker Freq), M (standard), S (Verb Center/Width).
- **Vertical fader:** thin dark track, rectangular cap with a teal centre line; value readout below (e.g. "-12.2"). Used for Wet/Dry/Gain/Threshold.
- **Meter:** vertical segmented-look gradient bar(s) with a dB scale (0, 6, 12 … 54), peak-hold line, numeric peak readouts below each bar ("-5.2  -5.4"). 30 fps repaint from a lock-free FIFO; no allocations on the audio thread.
- **Header bar (every plugin):** left: preset dropdown ("No preset" + up/down chevrons), `+ New` (save preset) button, `Parameter` button (opens a menu: learn/automation/modulation shortcuts or simply a parameter list), I/O pill ("2 in 2 out"). Right: small teal **Output/Mix knob** and a teal **"⏻ ON"** pill toggle (bypass; when off, the pill turns grey and the body dims to 50 %). Compact variant for small plugins: `+`, `Prm`, `2/2`.
- **Checkbox:** rounded square, teal fill + white check when on; label to the right.
- **Dropdown:** `--bg-raised` pill with up/down chevrons at right.
- **Module card** (Verb): `--bg-section` card with header row: power icon (bypass, coloured in the module's accent), UPPERCASE title, `×` remove button top-right, `||` drag handle on the left edge for reordering, `+` at bottom-right to insert a module after it.

### 2.4 Window sizes (at 100 %)
- Kicker / Exciter: ~ 270 × 290 (controls panel left, meter panel right).
- Comp: ~ 620 × 330.
- EQ: ~ 560 × 380, resizable (min 480 × 320), graph fills available space.
- Verb: ~ 600 × 420, module rack scrolls horizontally.

### 2.5 Control-type map (from the new redesign — follow exactly)

| Plugin | Knobs | Vertical faders | Other controls |
|---|---|---|---|
| Kicker | Freq (large), Wet, Dry | — | `⚙ EDIT` link; In/Out meters |
| Exciter | Freq, Clip Boost, Harmonics, Mix Back | — | `⚙ EDIT` link; In/Out meters |
| Comp | Ratio, Knee, Attack, Release, Pre-comp, RMS size, Lowpass, Highpass | **Threshold** (fader over the input meter), **Wet**, **Dry** | GR meter, Out meter; Detector input dropdown; checkboxes Weird, Classic, Auto, Link channels, Auto make-up, Preview filter, Limit output |
| EQ | Band editor: Freq, Gain, Q | **Gain** (right edge) | Draggable band handles on the graph; band tabs 1–4; band-type dropdown; power / + / × buttons; checkboxes Log-scale automated freq., Show grid, Show phase, Compact mode |
| Verb | Pre-delay, Center, Width (small), all module parameters (Lowpass, Highpass, Start pos., Length, Spacing, Error, ErrDst, Start Vol., End Vol., …) | **Wet**, **Dry** | IR waveform display with trim handles; FILE module file dropdown, `<` `>` buttons, Browse…, Mix to mono checkbox, SRC-gain dropdown; module power / × / + / drag handle; chain strip chips; NORMALIZE toggle; ⚙ SETUP |
| All | Small teal output/mix knob in the header | — | Preset dropdown, `+ New`/`+`, `Parameter`/`Prm`, I/O pill, `⏻ ON` pill |

If a control isn't in this table and isn't visible in the redesign, ask me before choosing its type. New controls I've asked for that aren't in the mockups (e.g. EDIT-drawer settings, oversampling) go in drawers/popovers as dropdowns, toggles or small knobs — never change the visible main layout to fit them.

## 3. Per-plugin UI spec

### 3.1 Fancy Kicker
Left body: **Freq** (large knob, red, default 50.0 Hz, range 20–240 Hz, log skew). Below: **Wet** (red, default -12.0 dB, range -inf/−60…+12 dB) and **Dry** (red, default 0.0 dB, −inf…+12 dB). Bottom-left: `⚙ EDIT` link in red (opens an "advanced" drawer: filter Q, output trim, mono-sum-below toggle — see §5.4). Bottom caption: plugin name in `--text-secondary`. Right panel: In/Out stereo meters with peak readouts.

### 3.2 Fancy Exciter
Same shell as Kicker. 2 × 2 grid of red knobs: **Freq** (default 2000 Hz, 500–16 000 Hz, log), **Clip Boost** (default 0.0 dB, 0–+24 dB; shown "0.2 dB" in mock), **Harmonics** (default 0 %, 0–100 %; shown "42 %"), **Mix Back** (default −6.0 dB, −inf…+6 dB). `⚙ EDIT` drawer: oversampling (Off/2×/4×/8×, default 4×), harmonic character (even ↔ odd), output trim. Right: In/Out meters.

### 3.3 Fancy Comp
- Header bar as standard.
- Row under header: **Detector input** dropdown (Main input L+R, Main L, Main R, Sidechain L+R, Sidechain L, Sidechain R, Mid, Side).
- Left column: **Threshold** vertical fader overlaid on an input meter (default 0.0 dB, range −60…0 dB) with value readout ("−4.4 dB"); next to it a **GR** meter (yellow, top-down, 0 to −30 dB) with two readouts ("−2.8 dB −2.6 dB").
- Main grid (teal knobs): **Ratio** (default 4:1; 1:1…∞:1, skewed), **Knee** (default 0.5 dB; 0–24 dB) with checkbox **Weird** under it, **Attack** (default 3.0 ms; 0–500 ms) with checkbox **Classic**, **Release** (default 200 ms; 0–5000 ms) with checkbox **Auto**.
- Second row, left box (teal): **Pre-comp** (lookahead, 0–250 ms), **RMS size** (0–250 ms, default 5.0 ms). Right box (blue): detector **Lowpass** (default 24 kHz / off) and **Highpass** (default 20 Hz / off; mock shows 93 Hz).
- Right column: **Out** meter (stereo), **Wet** fader, **Dry** fader, readouts beneath.
- Footer checkboxes: **Link channels**, **Auto make-up**, **Preview filter** (listen to detector), **Limit output**.
- Add (not in mock, put in Parameter menu or a small `⚙` popover so the layout stays clean): Oversampling (Off/2×/4×), Feedback detection mode, Mix linking.

### 3.4 Fancy EQ
- Graph area with grid, left dB axis (+18…−18, configurable range ±6/±12/±18/±24), right axis for gain fader scale, log frequency axis 20 Hz – 20 kHz (labels 50, 100, 200, 500, 1k, 2k, 5k, 10k, 20k).
- Each band drawn as a **translucent filled region** between 0 dB and its own curve in the band's colour (band 1 coral/red, 2 purple, 3 blue→teal, 4 green; further bands cycle the accent palette), the **composite curve in white**, and a live **spectrum analyzer line in orange** (pre or post, toggle).
- Numbered circular **band handles** on the curve (white fill, band number inside; selected handle uses band colour). Drag = freq/gain, wheel/alt-drag = Q, double-click = reset band, right-click = band type menu, double-click empty space = add band.
- **Band editor panel** (a floating dark rounded card in Compact mode, or a docked strip under band tabs `1 2 3 4` in normal mode): type dropdown (`∩ Band`), **Freq** knob (shows "487.7 Hz"), **Gain** knob (shows "−5.1 dB (B4)" — append nearest musical note to the frequency display), **Q/BW** knob ("2.0"), power (band bypass), `+` add band, `×` delete band. Knobs in the selected band's colour.
- Right edge: master **Gain** vertical fader ("−7.3").
- Footer checkboxes: **Log-scale automated freq.**, **Show grid**, **Show phase**, **Compact mode**.
- Band types: Low shelf, High shelf, Bell (Band), Notch, Low pass, High pass, Band pass, All pass, Tilt. Slopes for LP/HP/shelves: 6/12/18/24/36/48 dB/oct.
- Up to 16 bands, default preset = 4 bands (low shelf, bell, bell, high shelf) all at 0 dB.

### 3.5 Fancy Verb (modular convolution reverb)
- **Top: IR display** — waveform of the *final processed* impulse (after all modules), purple/magenta, with a time axis (0.2, 0.4 … s), and an info string top-right: "4.5s, 48kHz, 2ch, max: −34.3dB". Start/end trim handles draggable on the waveform (see §5.3).
- **Right column:** **Pre-delay** knob (teal; ms — the mock shows a dB value by mistake, use ms), two small knobs **Center** and **Width** (stereo image of the wet signal; mock labels "Center/100W"), then **Wet** and **Dry** faders with readouts.
- **Middle: module rack** — horizontally scrolling row of module cards. Each module is a stage in an *impulse-response build chain* (modules process the IR, not the live audio). Module types:
  - **FILE** (purple): breadcrumb path (`/Users/…/VS8F Reverb IR/VS8F-1`), file dropdown (other IRs in same folder), `<` `>` prev/next, `Browse…`, checkbox **Mix to mono**, dropdown for sample-rate conversion gain (e.g. "no SRC-gain, −18dB").
  - **LP/HP** (blue): Lowpass, Highpass knobs.
  - **ECHO GENERATOR** (teal): Start pos., Length, Spacing, Error, ErrDst, Start Vol., End Vol.
  - **REVERB GENERATOR** (teal): Size/Length, Decay, Density, Damping, Stereo spread (algorithmic IR synthesis, seeded noise so it's deterministic).
  - **NORMALIZE**, **TRIM/GAIN/STRETCH**, **REVERSE**.
- **Bottom: chain strip** — a mini-map of the chain as small chips (`⏻ FILE`, `⏻ LP/HP`, `⏻ ECHO` …) with the visible portion outlined; `⏻ NORMALIZE` toggle; `⚙ SETUP` (latency mode, max FFT size, IR storage mode — see §5.3).
- **Empty state:** grey waveform area, centred list of buttons to add the first module (File, Lp/Hp, Echo generator, Normalize, Trim/Gain/Stretch, Reverse, Reverb generator).
- Drag modules by the `||` handle to reorder; `+` opens the same add-menu; `×` removes; power toggles bypass. Every change rebuilds the IR **off the audio thread** (§5.3).

## 4. Cross-cutting engineering requirements (all plugins)

1. **Real-time safety:** no allocation, locks, file I/O, logging or `std::shared_ptr` refcount churn on the audio thread. Use atomics / lock-free FIFOs for UI↔DSP.
2. **Parameter smoothing:** every continuous parameter is smoothed (≈20–50 ms, per-sample or per-sub-block; filters recompute coefficients per sample or every ≤16 samples with interpolation) so automation and fast knob moves never produce zipper noise or clicks. Unit-test this with a sweep render.
3. **Denormals:** `juce::ScopedNoDenormals` in every process call plus DC-safe feedback paths.
4. **Latency:** any lookahead/oversampling/convolution latency is reported to the host via `setLatencySamples` and updated correctly when it changes; dry path is delayed to match so wet/dry mixes are phase-aligned.
5. **Sample-rate / block-size agnostic:** works at 44.1–384 kHz and block sizes 1–8192, including variable block sizes. Filter responses must not change audibly with sample rate (see §5.1).
6. **Bypass:** implement VST3 soft bypass (`kIsBypass` param) with a short crossfade and latency-compensated dry path.
7. **State:** versioned state (`stateVersion` int) saved as XML/JSON + binary blobs; old versions migrate; unknown params ignored. Loading state must never crash, even with corrupted data.
8. **Automation sanity:** enabling automation on a parameter without changing its value must produce bit-identical output (regression test).
9. **Bus layouts:** mono, stereo, and (Comp) stereo + stereo sidechain. Report supported layouts correctly.
10. **Presets:** factory presets in each plugin + user preset folder in the OS-standard location; `+ New` saves a preset.
11. **CPU:** measure and document per-instance CPU at 48 kHz/128 samples. Targets: Kicker/Exciter < 0.5 % (no OS) , Comp < 1 %, EQ (8 bands, no OS) < 1 %, Verb with a 4 s stereo IR in low-latency mode < 5 % on a modern 4-core laptop.

## 5. Known issues with the originals — and the required fix for each

These come from user reports about the original plugins. Each must be addressed and covered by a test or documented measurement.

### 5.1 EQ
| Issue reported by users | Required fix |
|---|---|
| **Cramping near Nyquist** — bells/shelves at high frequencies (e.g. 18 kHz boost) bend down toward Nyquist; users have requested native oversampling for years and resorted to third-party wrappers. | Use a cramping-free biquad design (Vicanek "matched second-order" or Orfanidis prescribed-Nyquist-gain designs) by default — zero latency, analog-matched magnitude up to Nyquist. Additionally offer optional 2×/4× oversampling. Test: a 18 kHz +6 dB bell at 44.1 kHz must match the analog prototype within 0.5 dB up to 20 kHz. |
| **Filter response changes with sample rate** (low-pass behaves differently at 48k vs 96k). | Same fix as above; add a test comparing magnitude responses at 44.1/48/96/192 kHz (≤0.5 dB deviation below 20 kHz). |
| **No linear-phase option** — a common reason users switch EQs for multi-mic or parallel-processing work. | Add a global "Phase: Minimum / Linear" mode. Linear uses FFT/FIR built from the summed magnitude response, with reported latency and a quality setting (FIR length). |
| **Slope only steepens by stacking bands**; lowering bandwidth on HP just adds a resonant bump. | Explicit slope selector (6–48 dB/oct) using cascaded Butterworth/Linkwitz-Riley sections; separate resonance/Q control. |
| **Spectrum analyzer readings disagree with other analyzers/what users hear**. | Calibrated analyzer: Hann/Blackman-Harris window, 4096–16384 FFT, 50–75 % overlap, −4.5 dB/oct pink tilt optional (default off), display labelled "pre"/"post", peak-hold, adjustable smoothing. Verify against a reference sine at −12 dBFS. |
| Notch silently replaces peaking filter past −24 dB cut (surprising). | Keep Notch as its own explicit band type; Bell never morphs into something else. |
| Automation clicks / zipper | Covered by §4.2 with per-sample coefficient interpolation. |

### 5.2 Compressor
| Issue reported by users | Required fix |
|---|---|
| **Oversampling was buggy and was removed**; users still want anti-aliasing for fast attack/release settings. | Proper polyphase oversampling (2×/4×, linear- or minimum-phase choice) around the gain computer + gain application, with reported latency. Measure aliasing with a 15 kHz sine at fast attack/release: aliasing products < −90 dBFS at 4×. |
| **"Limit output" fails at extreme levels** (stops limiting ~150 dB over threshold) and **limit ceiling varies with oversampling mode**. | Output limiter is a separate true-peak-aware brickwall stage after makeup gain, ceiling exact regardless of OS mode or input level (test inputs up to +200 dB above threshold in float). |
| **Attack time "feels 3× slower than set"** — users compare to other comps and think it's broken. Experts note this is time-constant semantics, not a bug. | Do not "fix" the math silently. Use a documented convention (attack/release = time to reach ~63 % of target, i.e. one time constant) and show it in the tooltip; add an optional "Attack time = 90 % rise" toggle for users who want the other convention. Document in the manual. |
| **Max gain reduction changes with attack time when RMS size is near 0** — explained by devs as by design (attack filters the detector). | Keep by-design behaviour but add a detector mode switch: "Smoothed detector" (classic) vs "Branching peak + smoothed gain" (max GR independent of attack). Default to the second for predictability. |
| **Release appears linear in dB/time**; users expect exponential. | Offer release curve: Exponential (default) / Linear (legacy). Auto-release = program-dependent dual time constant. |
| **Automation enabling changed the audio** (historic bug: input attenuated as soon as a param was armed). | Regression test §4.8. |
| Lookahead (Pre-comp) latency must be correct. | Report latency; dry path delayed; Wet/Dry stays phase-coherent. |

### 5.3 Convolution reverb
| Issue reported by users | Required fix |
|---|---|
| **High/unpredictable latency**, especially reported on macOS, sometimes appearing *after changing settings* in a live setup, and still noticeable even with zero-latency/low-latency options enabled. | Implement a **non-uniform partitioned convolver**: a direct-form (time-domain) or tiny-FFT head for true zero latency, followed by progressively larger uniform FFT partitions for the tail, each running on a background thread with deadline scheduling. Modes in SETUP: Zero latency (default), Low CPU (reports N samples latency). Latency must be deterministic (same value every load) and must never change without the host being informed. |
| **CPU spikes / glitches when changing settings while playing.** | All IR rebuilds (module chain, SRC, normalisation, partition FFTs) happen on a worker thread; the new convolver is swapped in atomically with a 50–100 ms equal-power crossfade. Never block the audio thread. |
| **No IR start/end trim** — a feature users specifically search other convolvers for. | Draggable start/end handles on the IR display + TRIM module, with fade-in/out lengths. |
| **IR files go missing when projects/drives move** (IR referenced by absolute path). | State stores: absolute path, path relative to the project (if host provides it), file hash, *and* an optional embedded copy of the IR (setting: "Embed IR in project", default ON for IRs < 10 MB, compressed FLAC). On load: try embedded → relative → absolute → user IR search folders; if all fail, show a clear "IR missing – Relink…" state in the FILE module instead of silently outputting dry. |
| **Loud bursts on reset / IR load** (common convolver problem). | Fade-in output over 20 ms after any reset or IR swap; clear all FFT buffers on `reset()`. |
| Pre-delay label shows dB in the mock. | Pre-delay is in ms (0–500). |
| Sample-rate mismatch between IR file and session. | High-quality offline resampling (windowed-sinc / r8brain-style) during rebuild; SRC gain option as in the FILE module dropdown. |

### 5.4 Kicker (sub enhancer)
No well-documented public bug reports exist for the original JSFX; these fixes are preventive and based on the algorithm type (resonant low-frequency boost, 20–240 Hz, often automated for FX sweeps):
- Frequency automation sweeps must be click-free (§4.2) — specifically test a 20→240 Hz sweep over 200 ms.
- Add a **DC blocker / subsonic HPF (~15 Hz, 2nd order)** after the effect so wet signal never accumulates DC or infrasonic energy.
- Optional **"Mono below Freq"** in the EDIT drawer, so the added sub is mono-compatible.
- Optional output **soft-clip safety** (off by default) so +12 dB wet doesn't hard-clip downstream.
- Phase-coherent wet/dry (wet path uses the same filter group delay compensation or is documented as minimum-phase).

### 5.5 Exciter
No well-documented public bug reports exist for the original JSFX; fixes are based on how exciters work (high-pass → nonlinear waveshaper → mix back), which is a known source of aliasing without oversampling:
- **Oversampling** around the waveshaper (default 4×), plus **ADAA** (1st-order antiderivative anti-aliasing) on the shaper. Test: 10 kHz sine, Harmonics 100 %, 44.1 kHz — aliased components < −80 dBFS.
- Post-shaper **low-pass at ~0.45·fs** and DC blocker on the generated harmonics.
- **Even/odd character** control (asymmetric vs symmetric shaper).
- Gain-compensated so toggling the plugin with Harmonics at 0 % is unity.

## 6. DSP notes (minimum quality bar)
- Biquads in transposed direct form II, double precision state.
- Oversampling via `juce::dsp::Oversampling` (polyphase IIR for zero-ish latency, FIR for linear phase), user-selectable.
- Compressor gain computer in the log domain with soft knee (quadratic interpolation over knee width); "Weird knee" = legacy alternate knee shape, documented.
- Analyzer FFT on a background timer thread fed by a lock-free FIFO.

## 7. Acceptance checks per plugin (must pass before moving on)
1. The GitHub Actions run for the milestone commit is **green** and its `FancyPlugs-VST3-<sha>` artifact contains the plugin. Paste the run URL into `docs/PROGRESS.md`.
2. pluginval strictness 10 passes in that CI run.
3. Loads, saves and reloads state in **REAPER** (my only required host), including: saving a project, closing REAPER, reopening and confirming identical sound and settings. If you can't run REAPER yourself, give me a short manual test checklist for that plugin instead.
4. All §5 tests for that plugin pass; results (plots/numbers) saved under `docs/measurements/`.
5. The CI-rendered screenshots (100 % and 200 %) are copied into `docs/screens/`, compared side by side against the reference image (and the Figma frame if available); list any deviations.
6. CPU measurement recorded against the §4.11 target (measured in CI with a benchmark test; note that CI runners are slower and noisier than my PC).
7. I have run `scripts\get-latest.ps1`, loaded it in REAPER, and confirmed it works — give me a 5-minute checklist for this.

## 8. Deliverables
- Source repo as above, README explaining the cloud-build flow and `get-latest.ps1`, with the CC BY attribution.
- `docs/` with design notes, measurements, and a short user manual per plugin (parameter reference, latency per mode, attack/release convention).
- Out of scope (personal use): code signing, notarization, installers, licence/copy protection, auto-update, AU/CLAP formats, cross-platform CI, multi-host certification. Don't spend time on these unless I ask.

## 9. Reference image index (`reference/`)

| File | What it shows | What to use |
|---|---|---|
| `01__fancy_reaplugs.png` | Cover: all five redesigned plugins on a laptop, plus five plugin icons | Overall look; the five line-art **plugin icons** (use as each plugin's header/about icon) |
| `02__the_story.png` | Designer's intro text | Context only — nothing to build |
| `03__key_UI_principles.png` | "More knobs, less sliders", grey + accent swatches, "Simplicity" | **Colour palette source** (§2.1) and the knob + fader style |
| `04__kicker.png` | Kicker: old REAPER version (top) → new redesign (bottom) | **Bottom only** |
| `05__exciter.png` | Exciter: old REAPER version (top) → new redesign (bottom) | **Bottom only** |
| `06__kicker_and_exciter.png` | Angled close-up render of new Kicker + Exciter over a DAW | Detail (knob pointer, arc, meter); not for exact colour |
| `07__ReaEQ.png` | EQ: old version (top) → new normal mode (middle) → new **Compact mode** (bottom) | **Middle and bottom** (both modes must exist) |
| `07__ReaEQ_2.png` | Angled close-up of the new EQ band editor + curves | Detail (band handles, fills, footer checkboxes) |
| `08__ReaComp.png` | Comp: old REAPER version (top) → new redesign (bottom) | **Bottom only** |
| `09__ReaVerb.png` | Verb: old version (top) → new version → annotated module card (bypass, remove, drag-to-rearrange, add) → **empty state** → **adding a new module** menu | Everything except the top old version |
| `09__ReaVerb_2.png` | Flat, straight-on new Verb UI | **Primary layout + colour reference for Verb** |
| `09__ReaVerb_3.png` | Angled close-up of Verb FILE / LP/HP / ECHO modules | Detail only |
| `10__All_5.png` | All five new plugins together, flat | Cross-check consistency between plugins |

Notes on things in the mockups NOT to copy literally:
- Window title bars like "VST: ReaEQ (Cockos) – Track 1 [4/4]" are REAPER's own host chrome — don't draw them inside the plugin.
- The Kicker/Exciter caption "[LOOSER]" is the original author tag (misspelled in the mock); show just the plugin name instead.
- Comp's "Treshold" is a typo → "Threshold". Verb Pre-delay shows "dB" by mistake → use ms.

Start by checking `gh auth status` and `git --version`, creating the private repo, and getting a **skeleton** workflow green (JUCE fetched + one empty plugin builds + artifact uploads) before writing real DSP. Then confirm the plan, list your assumptions, and build `fancy-core` and Fancy Kicker.
