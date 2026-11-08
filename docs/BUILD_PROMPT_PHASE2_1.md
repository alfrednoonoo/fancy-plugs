# Phase 2.1: closing the gaps from the market report

This extends `docs/BUILD_PROMPT.md` and `docs/BUILD_PROMPT_PHASE2.md`. **Every rule there still applies**: cloud builds only, the design system and shared `fancy-core` components, the real-time rules (§4), the acceptance checks (§7), and "ask before changing a parameter list or layout beyond what this file specifies".

Source: `docs/2026-11-market-report-detailed.md` (sections 5, 8, 9 and 10), plus one design fix I found while testing (item S) and host compatibility (item H). This phase adds six things, nothing else:

**Status at the start of Phase 2.1:** Phases 1, 2 and 2.0.1 are **complete**. All nine plugins exist and pass their acceptance checks: Kicker, Exciter, Comp, EQ, Verb, Delay, MultiComp, FIR and **Gate** (finished in Phase 2.0.1). Phase 2.1 **builds no new plugin**. It changes existing ones. Treat Fancy Gate exactly like the other eight: apply S, H and W to it, and don't rebuild, re-scaffold or re-plan it. Its 2.0.1 behaviour (zero latency by default, the Lookahead switch, the Pre-open hint, velocity window) stays as built unless an item below says otherwise.

| # | Item | Plugin(s) | Report reference |
|---|---|---|---|
| S | Styled menus, dropdown lists, tooltips, value editors and dialogs | All nine, via `fancy-core` | Design fix, not in the report |
| H | Host compatibility: offline rendering, keyboard, scaling, sidechain/MIDI buses, latency changes, Steinberg validator + host-behaviour harness in CI, free/trial host testing plan | All nine | Not in the report |
| W | Workflow: undo/redo, A/B, level-matched Compare | All nine, including Fancy Gate, via `fancy-core` | §10 |
| D | Dynamic EQ | Fancy EQ | §5 |
| U | Upward compression, plus Depth and Time | Fancy MultiComp | §8 |
| A | Adaptive noise tracking | Fancy FIR | §9 |

Out of scope for 2.1 (don't start these): AAX / Pro Tools, mid/side per band, EQ Match, character/saturation styles, lookahead in MultiComp, machine-learning denoise, resonance suppression, new formats, IR libraries.

## 0. How to work

1. **Order: S → H → W → D → U → A.** (Phase 2.0.1 is done, so start straight away.) S, H and W all touch every plugin, so do them first while nothing else is changing. W's new menus (Prm menu, A/B) are built with S's components, and D, U and A must pass H's offline-render and host tests from the start. Each item is its own milestone with its own commit, a green CI run and a `docs/PROGRESS.md` entry.
2. Before each item, write a short design note in `docs/phase2_1-<item>.md`: algorithm, new parameters (with IDs), state-version change and migration, latency impact, CPU impact, and how each test below will be done.
3. **Don't break saved projects.** Every new parameter is appended (never renumber or reorder existing parameter IDs), gets a default that reproduces the old sound exactly, and bumps `stateVersion` with a migration. Test: a state saved by the current release loads and renders **bit-identical** audio after this phase.
4. **CPU measurement method** (used here; Fancy Gate's 2.0.1 CPU figure should be re-measured this way once, as the baseline). Within **one** CI build, run each benchmark 5 times and take the median; don't spend separate builds on it. In the same run, benchmark a fixed **reference**: Fancy Comp at its default settings. Record both the absolute % and the **ratio to the reference** in `docs/measurements/`. Absolute targets are checked on the absolute median. Regressions are checked on the ratio, because the ratio cancels out most of the difference between runner machines: a change fails if the ratio rises more than 15 % over the previous release's ratio. Absolute CI figures are indicative only; real numbers come from my PC later.
5. **No CPU cost when unused.** A plugin with the new feature switched off must not use measurably more CPU than before, judged by the ratio method above.
6. S and W change `fancy-core`, so every plugin rebuilds. Batch each of them into as few pushes as possible to save build minutes.

## 1. Mockup

`reference/phase2_1/14__phase2_1_controls.png` shows every new control: (1) the full header, (2) the compact header and Prm menu, (3) the EQ dynamic band editor and graph drawing, (4) the MultiComp upward row, transfer curve and Depth/Time knobs, (5) the FIR adaptive card. Its HTML source is in `design/phase2_1-mockup-src/`. As in Phase 2, the mockup defines placement and control types, and `fancy-core` defines the exact styling.

`reference/phase2_1/16__menus_mockup.png` (source `design/phase2_1-mockup-src/menus.html`) is the target for item S: the preset menu, every state of a menu row, parameter dropdowns, a right-click menu with a submenu, a tooltip, the typed-value field and a confirm dialog. Unlike the other mockups, **this one also defines the exact styling of these elements**, because `fancy-core` has none for them yet. The values are listed in S.2.

## S. Styled pop-ups (all plugins, built once in `fancy-core`)

### S.1 The problem
Opening any dropdown in any plugin shows JUCE's default menu: blue-grey background, square corners, system font, no accent colour. Seen in a host on Windows with Fancy Gate's preset menu, and it affects every plugin. The shared LookAndFeel styles knobs, faders and buttons, but not the pop-up elements.

### S.2 Design rules (match `16__menus_mockup.png`)

| Element | Rule |
|---|---|
| Menu panel | Background `#262626`; 1 px border `#434343`; corner radius 6 px; 4 px inner padding; drop shadow (0, 10 px, 28 px blur, black 55 %). Opens 4 px below its dropdown, left-aligned, at least as wide as the dropdown. If there's no room below, open above |
| Row | 26 px tall; Inter 11.5 px `#EDEDED`; text starts 26 px from the left (room for the check mark); 10 px right padding; corner radius 4 px |
| Hover / keyboard focus | Background `#383838`, plus a 2 px rounded bar at the row's left edge in the **plugin's accent colour** (Gate yellow, Comp teal, EQ/MultiComp: the selected band's colour, Verb purple, Delay yellow, FIR: the current mode's colour, Kicker/Exciter red) |
| Selected / current value | Check mark in the accent colour, 10 px from the left |
| Disabled | Text `#5A5A5A`; not hoverable |
| Shortcut / hint text | Right-aligned, 10.5 px `#909090` |
| Section label | Uppercase, 9.5 px, letter-spacing 0.08 em, `#909090`, semibold; not selectable |
| Separator | 1 px `#3a3a3a`, 6 px side inset, 4 px vertical margin |
| Submenu | `›` at the right in `#909090`; opens to the right, top-aligned with its row, 2 px overlap; opens left if there's no room; opens on hover after ~200 ms or on → key |
| Long lists | More than 12 items: fixed height of 12 rows; thin scrollbar (4 px thumb `#4C4C4C` on a `#1E1E1E` track); the current value is scrolled into view when the menu opens |
| Dropdown while open | The dropdown control itself turns `#383838` |
| Tooltip | Background `#1E1E1E`; 1 px border `#434343`; radius 5 px; Inter 10.5 px, line-height 1.45; max width 250 px; shadow; delay ~600 ms (300 ms for the dimmed-control hints such as Pre-open) |
| Typed value | Double-click a value read-out, or Ctrl-click a knob or fader: the read-out becomes a field with background `#141414` and a 1 px border plus caret in the accent colour. Enter accepts, Esc cancels, clicking away accepts. Units are optional and parsed ("120", "120ms", "0.12 s", "2k", "-6 dB", "1/8."). Out-of-range values clamp |
| Confirm dialog | Only for destructive actions (delete user preset, delete a band/tap/module that has non-default settings, clear a learned noise profile). In-plugin overlay, not an OS window: panel `#262626`, radius 8 px, 16 px padding; title 12.5 px semibold; body 11 px `#909090`; buttons Cancel (`#383838`) and the action (red `#E84E3C` for destructive, accent otherwise). Esc = Cancel, Enter = the action |
| Font and scale | Everything uses the bundled Inter and follows the plugin's UI scale (100–200 %) |

### S.3 Preset menu content (all plugins)
- Sections **Factory** and **User**, each with its presets; ✓ on the loaded one.
- Below a separator: **Save as new preset…** (Ctrl+S), **Reload "<name>"** (shown only when modified, hint "discard changes"), **Rename…** and **Delete…** (enabled only for a loaded user preset; Delete asks to confirm), **Open preset folder**.
- The dropdown shows the loaded preset's name, with " •" after it once anything has changed. It shows "Default" (not "No preset") when the plugin is at its defaults. This fixes the mismatch where the header said "No preset" while the list's first item was "Default".

### S.4 Implementation notes
- Do it in the shared LookAndFeel: override popup-menu drawing (background, item, section header, separator, submenu arrow, scroll arrows/scrollbar), combo-box drawing, tooltip, label/text-editor colours for value editing, and the alert/dialog drawing. Use the existing colour tokens; add `--menu-hover #383838` and `--menu-sep #3a3a3a` to the token list.
- **Attach every pop-up to the plugin editor** (e.g. JUCE `PopupMenu::Options().withParentComponent(editor)`), so menus are drawn inside the plugin window with its LookAndFeel and UI scale. A top-level pop-up window can fall back to default styling, open at the wrong DPI scale or appear behind the host window. If a menu would be clipped by a small plugin window (Kicker, Exciter), use the top-level form, but set the LookAndFeel and scale on it explicitly.
- Replace any OS-native alert boxes with the in-plugin dialog. The **file browser** (Verb's Browse…, preset import) stays native: that's expected on Windows and fine.
- **Out of our control:** the host's own right-click menu (the parameter context menu REAPER shows, e.g. for automation or MIDI learn) and the host's own window chrome, such as a preset bar or Preview/Apply/Cancel buttons that some hosts draw around the plugin. Don't try to restyle those; the menus we draw are what S covers. Note this in the manual.
- Audit first: list **every** pop-up, dropdown, tooltip, value editor and dialog in all nine plugins in `docs/phase2_1-S.md` (preset menu, Parameter/Prm menu, detector inputs, note/channel, crossover type, FFT size, edit mode, EQ band type and band right-click menu, Verb add-module menu, file list, SRC-gain list, Delay note value, …). Each must be ticked off as converted.

### S.5 Tests
1. **Screenshot gallery in CI:** extend `fancy-screens` to open every audited pop-up programmatically and save a screenshot of each at 100 % and 200 %, into a `screens-popups-<sha>` artifact. Compare them with `16__menus_mockup.png` and list any deviations in `docs/PROGRESS.md`.
2. A unit test walks every audited menu and checks it's attached to the editor and uses the Fancy LookAndFeel (no item drawn by the default LookAndFeel).
3. Keyboard: ↑/↓ move the highlight, Enter selects, Esc closes, → / ← open and close submenus.
4. Typed values: a table of inputs ("120", "120ms", "0.12 s", "2k", "-6", "-6 dB", "1/8.", "abc", "") parses to the expected value or is rejected without changing the parameter.
5. My REAPER checklist gets one item per plugin: open the preset menu and one other dropdown, hover a knob for its tooltip, type a value.

## H. Host compatibility (all plugins)

The suite must work in these hosts on Windows, not only REAPER: **REAPER**, **Audacity**, **UA LUNA** (including its free edition), **Tracktion Waveform** (Free and Pro), **Cubase**, **Ableton Live**. Pro Tools is out of scope (it needs AAX).

### H.1 Offline rendering (highest priority: a real bug risk in every host)
- Audacity's **Apply**, REAPER's render faster than real time, and every host's export/bounce/freeze can call the plugin much faster than real time, often with large or irregular blocks.
- Anything that runs on a background thread (Verb's partitioned convolution tail and IR rebuilds, FIR rebuilds, linear-phase EQ and MultiComp FIR builds, any crossfaded rebuild) can fall behind there and leave gaps or glitches that are never heard during playback.
- **Fix:** when the host signals non-real-time processing (VST3 `kOffline` process mode / JUCE `isNonRealtime()`), do that work **synchronously** on the audio thread: finish pending rebuilds before processing the block and compute all convolution partitions in-line. Switch back to the background path in real-time mode. Never drop or delay audio in offline mode to protect CPU.
- **Tests (CI, every plugin, every mode):** render the same 30 s test signal (with parameter automation in it) (a) in real-time mode with 128-sample blocks and (b) in offline mode with random block sizes from 1 to 8192, and require identical output (bit-identical where the algorithm is deterministic, otherwise within −120 dB). Include an IR swap and an FFT-size change in the middle of the render for Verb and FIR.

### H.2 Keyboard
- Only take keyboard focus while a value field is being typed in, or a menu or dialog is open. Otherwise every key, especially the **space bar** (transport), must pass through to the host. UA documents "key passthrough issues" with third-party VST3 plugins in LUNA on Windows; this is the cause to avoid.
- In-plugin shortcuts (Ctrl+Z etc., from W) act only when the plugin window has focus, and are never required: there's always a button or menu item.
- Test: a unit test sends space, Enter and letters to the editor with no field active and checks that every key is reported as not consumed.

### H.3 Scaling and high-DPI
- Support host-driven scaling (VST3 content-scale support) as well as the plugin's own 100–200 % menu, and per-monitor DPI on Windows. UA documents graphical issues with third-party VST3 GUIs in LUNA above 100 % scaling, and Ableton Live has its own "auto-scale plug-in window" option, so test both.
- Pop-ups (item S) must open at the same scale as the plugin.
- Test: CI screenshots at host scale 1.25, 1.5 and 2.0 must have no clipped text and no blurry (bitmap-scaled) controls.

### H.4 Sidechain bus
- Declare the sidechain as an **optional auxiliary input bus** named "Sidechain", **inactive by default**, supporting mono and stereo. Hosts differ in how you turn it on: Cubase has a sidechain button on the plugin, Ableton Live 10.1+ has a "Sidechain" panel in its plug-in device, Waveform configures sidechain inputs from a button at the top-left of the plugin window, and REAPER uses pins 3/4.
- An inactive or silent sidechain is treated as silence; detector options that use it show the "No sidechain signal" warning (as in Fancy Gate) in **every** plugin that has a sidechain: Gate, Comp, MultiComp, FIR, and EQ (dynamic bands).
- pluginval and a unit test cover: sidechain bus inactive, mono, stereo, and toggled while processing.
- In each plugin's manual: a short "How to sidechain in REAPER / Cubase / Live / Waveform / LUNA" section. Audacity has no sidechain routing, so say so.

### H.5 MIDI output (Fancy Gate)
- Keep the VST3 event output bus. Many hosts don't route MIDI out of an **audio effect**. Ableton Live's audio effects can't, and Audacity has no MIDI routing for effects; Cubase support for MIDI from audio inserts is unclear. If the host doesn't connect the bus, nothing must break: the gate still works as a gate.
- In the manual, show which hosts route it, filled in from my host tests (H.7), not assumed.

### H.6 Latency changes
- Every latency change (Gate Lookahead, EQ/MultiComp linear phase, FIR size or mode, Verb Low-CPU) must be reported with a VST3 latency-changed restart (`setLatencySamples` → `restartComponent(kLatencyChanged)`).
- Some hosts only re-read latency when playback stops or the plugin is reset. The UI should say so where it matters: next to those switches, a small note in the tooltip: "Latency changes take effect in some hosts after you stop and start playback."
- Test: toggling each of those settings triggers exactly one latency-changed notification.

### H.7 Automated host checks in CI (no host licences needed)
Most host problems come from a plugin mishandling something the VST3 standard allows. These can be tested without the hosts:

1. **Steinberg's VST3 validator.** Build the `validator` command-line tool from the VST3 SDK (its source ships with the SDK; it's designed for build servers) and run it on every `.vst3` in every CI build, alongside pluginval. Any failure fails the build. This is Steinberg's own conformance check, the closest thing to "will Cubase accept it".
2. **Host-behaviour harness.** A CI test program that loads each built `.vst3` **as a host would** (via JUCE's VST3 hosting) and runs scenarios that real hosts are known to produce. Cover at least:
   - offline mode with random block sizes 1–8192 (H.1), including blocks of 0 and 1 samples;
   - sample-rate change without reloading (44.1 → 48 → 96 kHz) and back;
   - re-activating the plugin (deactivate/activate) mid-session, and processing after a long silence;
   - restoring state **before** the first `prepareToPlay` and **after** it, and restoring component and controller state separately (the VST3 split some hosts use), then checking the UI shows the restored values;
   - sidechain bus inactive → active → inactive while processing; mono and stereo main buses;
   - host bypass (`kIsBypass`) on and off with latency, checking the bypassed output is time-aligned;
   - latency changes (H.6) reported exactly once;
   - automation flood: every parameter changed every block for 10 s, no NaN/inf, no allocation (use a malloc-counting hook in debug builds);
   - opening and closing the editor 50 times and resizing it, with no leaks or crashes.
   Write down where each scenario comes from (host documentation, forum report, or the VST3 spec) in `docs/host-compat.md`.
3. **A test host for me.** Build JUCE's free **AudioPluginHost** in CI once (cache it) and attach it to the artifacts as `tools/AudioPluginHost.exe`. It shows sidechain and MIDI routing as visible wires, so I can check sidechain input and the Gate's MIDI output on my PC without a paid DAW.

### H.8 Real-host testing plan (what I can do)
Two tiers, chosen so I don't have to buy anything:

| Tier | Hosts | When |
|---|---|---|
| **1: free, every milestone** | REAPER, Audacity, Tracktion Waveform Free, UA LUNA (free as part of UA's "UAD Explore FREE" bundle), JUCE AudioPluginHost (from H.7) | After S, after H, then after each later item |
| **2: time-limited trials, once** | Ableton Live 12 Suite (30-day trial; full edition), Cubase (trial) | Once, when S and H are both done, so one trial period covers a stable build. Schedule all Tier 2 checks inside a single week of the trial |

- Create `docs/host-compat.md`: one row per plugin and one column per host, plus rows for the cross-cutting checks: loads, UI and pop-ups, scaling at 150 %, space bar reaches the host, automation, save and reload project, offline render/export matches playback, sidechain, MIDI out (Gate), latency compensation.
- The agent can't run these hosts. **Give me a short checklist per host** (10–15 minutes each; Tier 2 checklists can be longer, since I'll only do them once) and fill in the table from my results. Cells start as "untested", never as assumed passes. Waveform Pro is covered by Waveform Free unless a check depends on a Pro-only feature; say so where it does.
- If a host fails a check, fix the plugin where the cause is ours; where it's the host's limitation, note it in the manual and in the table.

### H.9 Design rule: degrade gracefully
Every plugin's **core job** must work in every host above, even where a host lacks a feature: no sidechain → the plugin works on its own input and shows the "No sidechain signal" warning only when a sidechain source is selected; no MIDI routing → the Gate still gates; no latency compensation → nothing breaks, and the manual explains the delay. No feature may make a plugin refuse to load or go silent in a host that lacks it.

## W. Workflow (all plugins, built once in `fancy-core`)

### W.1 Placement
- **Large plugins** (EQ, Comp, Verb, Delay, MultiComp, FIR, Gate): in the header, between the I/O pill and the right-hand group, in this order: Undo `↶`, Redo `↷` | `A | B` segmented switch, copy `→` | `≈ Compare`. Thin separators as in the mockup. Disabled icons use `--text-disabled`.
- **Compact plugins** (Kicker, Exciter): keep only `A | B` in the header. Undo, Redo, Copy A→B, Compare and the I/O info ("2/2") move into the **Prm** menu. The visible header becomes: preset ▾ · `+` · `Prm` · `A | B` · output knob · ON.

### W.2 Undo / Redo
- Build the Prm menu, A/B and confirm prompts with item S's components.
- History of **user** edits only: knob/fader gestures (one gesture = one step, using begin/end change-gesture), typed values, preset loads, and structural changes (add/remove/reorder band, tap or module; IR file change; learned noise profile).
- **Never record**: host automation playback, parameter changes the host sends (e.g. REAPER's own undo), meter or analyzer state, Compare toggles.
- 100 steps per instance; history is not saved with the project.
- Keyboard: Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y **only while the plugin window has keyboard focus**, and pass every other key to the host. REAPER often keeps key focus itself, so the buttons and the menu are the primary way in.
- Tests: 50 random edits then 50 undos returns state **bit-identical** to the start; redo replays to bit-identical; automation playback during editing adds no history steps.

### W.3 A / B
- Two complete state slots, A and B, each holding the full plugin state, including structural state (bands, taps, modules, IR, noise profile). The active slot is the live state.
- Switching slots swaps state with a 20 ms equal-power crossfade (render old and new in parallel for the fade; for Verb and FIR, reuse their off-thread rebuild-and-crossfade path).
- `→` copies the active slot into the other one. Both slots and the active-slot flag are saved with the project.
- Switching A/B is one undo step.
- The host sees normal parameter changes when you switch (so automation lanes stay truthful), but marked as a non-gesture change so REAPER doesn't write automation from it.
- Tests: A→B→A restores bit-identical state; the switch produces no sample jump beyond the crossfade.

### W.4 Compare (level-matched bypass)
- **Purpose:** hear the plugin's effect without the loudness change fooling you.
- While Compare is on, the output is the latency-compensated **dry** signal with gain applied so its loudness matches the processed signal. Gain = difference in short-term loudness (ITU-R BS.1770 K-weighted, 3 s window) between the processed and dry signals, measured continuously while Compare is **off** and **frozen** at the moment Compare is switched on.
- Compare is **not** a host parameter (not automatable, not saved) and is independent of the `⏻ ON` bypass.
- 20 ms crossfade in and out. Show a small "+2.4 dB" read-out next to the button while it's on.
- Clamp the matching gain to ±24 dB. If either signal is below −70 LUFS during measurement, don't update the gain.
- Tests: with an EQ boost of +6 dB at 1 kHz on pink noise, Compare output loudness is within 0.5 LU of the processed loudness; toggling produces no clicks.

## D. Dynamic EQ (Fancy EQ)

### D.1 What it does
Any band can be made **dynamic**: its gain moves between its static Gain and Gain + Range, depending on how loud the signal is within that band's own frequency region.

### D.2 Controls (per band; placement as in mockup panel 3)
| Parameter | Range / default |
|---|---|
| Dynamic (DYN chip) | off |
| Threshold | −60…0 dB, default −24 |
| Range | −30…+30 dB, default −6 (negative = cut when active, positive = boost) |
| Attack | 0.1–200 ms, default 5 |
| Release | 5–2000 ms, default 80 |
| Mode | Above (acts when the band level rises above threshold, default) / Below (acts when it falls below) |
| Sidechain | off (on = detector reads the sidechain input instead of the main input) |

- Available for Bell, Low shelf, High shelf, Tilt and Notch. Hidden for LP/HP/BP/All-pass (their gain isn't the point).
- **Linear-phase mode:** dynamic bands aren't possible without breaking linear phase. When the EQ is in Linear mode, the DYN chip is disabled with the tooltip "Dynamic bands need Minimum phase", and any dynamic bands run static. Document this.
- Fancy EQ gains a stereo sidechain input bus (I/O becomes "4 in 2 out"). The bus is optional for the host.

### D.3 DSP
- **Detector** per dynamic band: a band-pass at the band's frequency and Q (for shelves: a 2nd-order shelf-shaped high-/low-pass at the shelf frequency), fed by main or sidechain, stereo-linked (max of L/R).
- Level: RMS over 5 ms, then a log-domain gain computer with a 6 dB soft knee. Gain change = Range × (how far past threshold, in dB, reaching full Range at 12 dB past threshold), smoothed with Attack/Release.
- Apply the gain by recomputing the band's matched-z coefficients **every 16 samples**, with per-sample interpolation of the coefficients, so there is no zipper noise.
- **Graph:** the static curve as now; the dynamic reach drawn as a lighter fill with a dashed edge; the white live curve moves inside it at display rate (30 fps from a lock-free FIFO of current gains).
- Zero added latency.

### D.4 Tests
1. DYN on, signal 20 dB below threshold → output **bit-identical** to the same band with DYN off.
2. Sine at band centre, 12 dB above threshold, Range −6 dB → steady-state gain within 0.2 dB of −6 dB; reaches 63 % of it within 1.2× the Attack time.
3. A sine **outside** the band (two octaves away) at +12 dB over threshold → the band doesn't move (< 0.1 dB).
4. Tone burst sweeping across threshold: no zipper noise (THD+N of the output stays within 3 dB of the static-EQ case).
5. Below mode and Sidechain mode each get a test of the same kind.
6. CPU: 8 dynamic bands < 1.5 % in CI (method in §0.4).

## U. Upward compression (Fancy MultiComp)

### U.1 What it does
Each band can lift **quiet** material (upward compression) as well as push down loud material (the existing downward compressor). Global Depth and Time controls make the "heavy, squashed" multiband sound quick to dial in.

### U.2 Controls (placement as in mockup panel 4)
Per band, in a new **UPWARD** row under the existing controls:

| Parameter | Range / default |
|---|---|
| Upward on | **off** (so existing projects sound identical) |
| Up threshold | −80…0 dB, default −40. Signals below it are lifted |
| Up ratio | 1:1…1:10, default 1:2. Example: at 1:2, a signal 20 dB below Up threshold is lifted by 10 dB (subject to Max lift) |
| Max lift | 0–30 dB, default 12. Hard cap on how much gain is added |
| Floor | −100…−40 dB, default −70. Below the floor no lift is added; the lift fades in over the 8 dB above the floor, so noise and silence don't get raised |

Global, as small teal knobs above Wet/Dry in the right column:

| Parameter | Range / default |
|---|---|
| Depth | 0–100 %, default 100 %. Scales **both** upward and downward gain change, in dB, of every band (0 % = no gain change at all) |
| Time | 10–1000 %, default 100 %. Multiplies every band's attack and release |

The band's small **transfer-curve** display (mockup) plots input vs output level, showing the floor, up threshold and down threshold as vertical guides.

### U.3 DSP
- One gain computer per band producing gain change in dB = downward part (existing) + upward part:
  `lift = min(MaxLift, (UpThr − x) × (1 − 1/UpRatio))` for `x < UpThr`, multiplied by a fade from 0 at Floor to 1 at Floor + 8 dB, and 0 when `x < Floor`.
- Both parts use the band's Attack/Release (× Time). Total gain change in dB × Depth.
- If Up threshold is set above the downward Threshold, clamp it to the downward Threshold and show the clamp in the transfer curve.
- **Factory preset "Big squash"**: 3 bands with crossovers at 88 Hz and 2.5 kHz; every band downward 4:1 above −18 dB and upward 1:4 below −40 dB, Max lift 18 dB, Floor −70 dB; fast attack (about 1–5 ms), release about 100–300 ms; Depth 100 %. (A generic heavy upward/downward multiband setting; don't name it after a commercial product.)
- Upward gain raises the noise floor by design; the Floor control is the safety. Mention it in the manual.

### U.4 Tests
1. Upward off, Depth 100 %, Time 100 % → bit-identical to the current release.
2. Static transfer curve: sine levels from −90 to 0 dB in 2 dB steps through one band (others muted) → measured output level matches the formula within 0.3 dB, including the Max-lift cap and the Floor fade.
3. Depth 0 % → output equals the Dry path (band sum) within 0.01 dB.
4. Time 200 % → measured 63 % attack and release times double (±10 %).
5. Silence and −90 dB noise input with Upward on → output not raised (Floor works).
6. "Big squash" on pink noise and a drum loop: no NaN/inf, output below 0 dBFS with Limit output on.

## A. Adaptive noise tracking (Fancy FIR, Subtract mode)

### A.1 What it does
Today the noise profile is learned once. Adaptive mode **keeps estimating the noise floor while audio plays**, so it follows noise that changes (fans, room tone, traffic), without learning speech or music as noise.

### A.2 Controls (placement as in mockup panel 5)
In the Subtract left card:

| Parameter | Range / default |
|---|---|
| Profile source | `Learned` (default, existing behaviour) / `Adaptive` / `Both` |
| Adapt speed | 0.5–10 s, default 2 s. How quickly the estimate follows a change in noise level |

- Learned: unchanged. Adaptive: the Learn button is hidden and the card shows a status ("Tracking", the estimated average noise floor in dB, and "Speech/music held, not learned" when the detector holds). Both: per bin, the maximum of the learned profile and the adaptive estimate is used, with the learned profile as a floor.
- On the graph, the adaptive estimate is drawn as a **dashed** line in the mode colour; the learned profile stays solid.
- Reduction, Offset, Smoothing and Release apply in all three sources.

### A.3 DSP
- Use a published minimum-statistics noise estimator (Martin 2001, "Noise power spectral density estimation based on optimal smoothing and minimum statistics"), or IMCRA (Cohen 2003). Name the one you chose and its parameters in the design note.
- Window length for the minimum search is derived from **Adapt speed**, and must be independent of FFT size and hop (convert seconds to frames using the current hop).
- Apply bias compensation so the estimate isn't systematically low.
- Holding: when speech/music presence probability in a bin is high, freeze that bin's estimate so sustained tones aren't absorbed into the profile.
- The adaptive estimate is **not** saved in the project (it restarts from the learned profile, or from the first second of audio). Changing FFT size resamples the current estimate to the new bin layout, like the learned profile already does.

### A.4 Tests
1. Source = Learned → bit-identical to the current release.
2. **Tracking:** pink noise at −50 dBFS for 10 s, then −40 dBFS for 10 s (Adapt speed 2 s) → the estimated floor reaches within 2 dB of the new level in 2 s ± 25 %; the same within ±25 % at FFT sizes 1024 and 8192.
3. **Not learning the signal:** a sustained 1 kHz sine at −20 dBFS over −60 dBFS noise for 30 s → the sine loses < 0.5 dB at the end of the 30 s.
4. **Speech-like test:** alternating 300 ms tone bursts and 300 ms gaps over constant noise → the noise estimate stays within 2 dB of the true noise level.
5. Musical noise: on the existing white-noise + sine test, residual variance in Adaptive mode is no worse than 1.5× the Learned mode figure (37.4 dB² today).
6. CPU in Adaptive mode at FFT 2048 < 1.5× the Learned mode figure.

## 2. Acceptance

For each item S, H, W, D, U and A: §7 of `BUILD_PROMPT.md` (green CI, pluginval, screenshots compared with the mockup, REAPER checklist for me), plus every test above in Catch2 and green in CI, plus the backward-compatibility test from §0.3. Update each affected plugin's manual and `docs/measurements/`.

When all four are done, add a short "Phase 2.1 results" section to the market report summary showing which §5/§8/§9/§10 gaps are now closed, with the measured numbers.
