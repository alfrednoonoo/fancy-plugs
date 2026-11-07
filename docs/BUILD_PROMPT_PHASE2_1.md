# Phase 2.1: closing the gaps from the market report

This extends `docs/BUILD_PROMPT.md` and `docs/BUILD_PROMPT_PHASE2.md`. **Every rule there still applies**: cloud builds only, the design system and shared `fancy-core` components, the real-time rules (§4), the acceptance checks (§7), and "ask before changing a parameter list or layout beyond what this file specifies".

Source: `docs/2026-11-market-report-detailed.md` (sections 5, 8, 9 and 10). This phase adds four things, nothing else:

| # | Item | Plugin(s) | Report reference |
|---|---|---|---|
| W | Workflow: undo/redo, A/B, level-matched Compare | All eight, via `fancy-core` | §10 |
| D | Dynamic EQ | Fancy EQ | §5 |
| U | Upward compression, plus Depth and Time | Fancy MultiComp | §8 |
| A | Adaptive noise tracking | Fancy FIR | §9 |

Out of scope for 2.1 (don't start these): mid/side per band, EQ Match, character/saturation styles, lookahead in MultiComp, machine-learning denoise, resonance suppression, new formats, IR libraries.

## 0. How to work

1. **Order: W → D → U → A.** W touches every plugin, so do it first while nothing else is changing. Each item is its own milestone with its own commit, a green CI run and a `docs/PROGRESS.md` entry.
2. Before each item, write a short design note in `docs/phase2_1-<item>.md`: algorithm, new parameters (with IDs), state-version change and migration, latency impact, CPU impact, and how each test below will be done.
3. **Don't break saved projects.** Every new parameter is appended (never renumber or reorder existing parameter IDs), gets a default that reproduces the old sound exactly, and bumps `stateVersion` with a migration. Test: a state saved by the current release loads and renders **bit-identical** audio after this phase.
4. **No CPU cost when unused.** A plugin with the new feature switched off must not use measurably more CPU than before (allowing for CI noise; compare medians of 5 runs).
5. W changes `fancy-core`, so every plugin rebuilds. Batch W into as few pushes as possible to save build minutes.

## 1. Mockup

`reference/phase2_1/14__phase2_1_controls.png` shows every new control: (1) the full header, (2) the compact header and Prm menu, (3) the EQ dynamic band editor and graph drawing, (4) the MultiComp upward row, transfer curve and Depth/Time knobs, (5) the FIR adaptive card. Its HTML source is in `design/phase2_1-mockup-src/`. As in Phase 2, the mockup defines placement and control types, and `fancy-core` defines the exact styling.

## W. Workflow (all plugins, built once in `fancy-core`)

### W.1 Placement
- **Large plugins** (EQ, Comp, Verb, Delay, MultiComp, FIR): in the header, between the I/O pill and the right-hand group, in this order: Undo `↶`, Redo `↷` | `A | B` segmented switch, copy `→` | `≈ Compare`. Thin separators as in the mockup. Disabled icons use `--text-disabled`.
- **Compact plugins** (Kicker, Exciter): keep only `A | B` in the header. Undo, Redo, Copy A→B, Compare and the I/O info ("2/2") move into the **Prm** menu. The visible header becomes: preset ▾ · `+` · `Prm` · `A | B` · output knob · ON.

### W.2 Undo / Redo
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
6. CPU: 8 dynamic bands < 1.5 % in CI (median of 5).

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

For each item W, D, U and A: §7 of `BUILD_PROMPT.md` (green CI, pluginval, screenshots compared with the mockup, REAPER checklist for me), plus every test above in Catch2 and green in CI, plus the backward-compatibility test from §0.3. Update each affected plugin's manual and `docs/measurements/`.

When all four are done, add a short "Phase 2.1 results" section to the market report summary showing which §5/§8/§9/§10 gaps are now closed, with the measured numbers.
