# Phase 3: new plugins: Fancy DS, Fancy Room, Fancy Saturate, Fancy Limit, and Fancy Kicker's Sub mode

This extends all earlier briefs and `docs/DESIGN_SYSTEM.md`. **Every rule there still applies**: cloud builds only, `fancy-core` components only, the pop-up rules (S), host compatibility (H, including offline rendering, keyboard, scaling, sidechain bus, latency reporting, the validator and host harness), workflow (W), the CPU method, backward compatibility, and "ask before changing a parameter list or layout".

## 0. Scope and names

| Plugin | Inspired by | Accent | Size class |
|---|---|---|---|
| **Fancy DS** (de-esser) | REAPER's JS de-esser | green `#2DCC70` (approved) | Phase 2, 760 wide |
| **Fancy DS-Mini** (one-knob de-esser) | Fancy DS, simplified | blue `#3598DB` (approved exception, §6b) | compact, 392 wide |
| **Fancy Room** (algorithmic room reverb) | ReaVerbate | purple `#9A59B5` (approved; shared with Verb, see §0.2) | Phase 2, 760 wide |
| **Fancy Saturate** | none (new) | orange `#E77E23` (approved) | Phase 2, 760 wide |
| **Fancy Limit** | none (new) | red `#E74D3D` (approved) | Phase 2, 760 wide |
| **Fancy Kicker**: new **Sub** mode | Thunderkick JSFX | red, unchanged | compact, unchanged |

- **Names:** the design system calls three of these "Fancy De-esser", "Fancy Saturator" and "Fancy Limiter". The approved product names are **Fancy DS**, **Fancy Saturate** and **Fancy Limit**. Use them everywhere: product name, title strip, bundle ID (`com.fancyplugs.ds`, `.dsmini`, `.room`, `.saturate`, `.limit`), docs and presets. Update `DESIGN_SYSTEM.md` to match.
- **If Phase 3 Q5 already has notes for the limiter or saturator,** this file supersedes them on names and layout. Where they disagree on DSP, list the differences in `docs/PROGRESS.md` and ask me before choosing.
- **Not in scope:** ReaTune, ReaControlMIDI, ReaSynth, ReaSamplOmatic5000, ReaStream, ReaNINJAM.

### 0.1 Order
**Fancy Limit** (reuses Fancy Comp's limiter core) → **Fancy DS** (reuses the detector, the threshold meter and MultiComp's crossovers) → **Fancy DS-Mini** (a second front panel on DS's engine) → **Fancy Saturate** (reuses Exciter's oversampling and ADAA) → **Kicker Sub mode** → **Fancy Room** (new engine, most tuning). One milestone each, with a design note `docs/<plugin>.md` before any code, as in every earlier phase.

### 0.2 Fancy Room's accent (decided)
Purple `#9A59B5` (`uiPurple`), approved 10 November 2026. All seven palette accents are taken, and Room is a reverb like Verb, so the suite reads "purple = reverb". Two plugins sharing an accent is accepted. Add Room to the accent table in `DESIGN_SYSTEM.md` §2.

### 0.3 Rule: listen and preview switches are temporary
A switch that replaces the plugin's normal output with a diagnostic signal must never be saved in a state that could reach an export. This covers **Listen** (Fancy DS, Fancy DS-Mini), **Listen to added harmonics** (Saturate), **Listen to removed** (Limit), and, as a retrofit, the existing **Preview filter** (Comp, Gate) and **Listen to removed noise / gain-reduced part** (FIR).
- **Not saved:** not written to the plugin state or presets; always off after loading a project or preset. For the existing plugins, a value saved by an older version is ignored on load, so an old project saved with it on opens with it off. (This is a deliberate exception to "old projects sound identical", and the only one.)
- **Not automatable** and not exposed as a host parameter; it doesn't record undo steps, and A/B switching doesn't change it.
- **Switches off when the plugin window closes.**
- **While it's on,** the switch and the plugin's title in the title strip show it clearly (accent-filled checkbox plus a "LISTENING" tag in the title strip), so it can't be forgotten.
- **Tests:** save state with it on and reload → off; it isn't in the host parameter list; closing the editor turns it off; offline render never includes it unless the window is open and it's on.

## 1. Mockups

| File | What |
|---|---|
| `reference/phase3/17__ds_mockup.png` | Fancy DS |
| `reference/phase3/18__room_mockup.png` | Fancy Room |
| `reference/phase3/19__saturate_mockup.png` | Fancy Saturate |
| `reference/phase3/20__limit_mockup.png` | Fancy Limit |
| `reference/phase3/21__kicker_sub_mockup.png` | Fancy Kicker in Sub mode |
| `reference/phase3/22__ds_mini_mockup.png` | Fancy DS-Mini |

- Drawn with the Phase 2 kit, updated to `DESIGN_SYSTEM.md` tokens (`design/phase3-mockup-src/kit2.css`, `lib2.js`). Source HTML is in the same folder.
- In the four 760-wide mockups, the header is a crop of the Fancy Delay CI screenshot, with Delay's "120.0 BPM (host)" read-out painted out, as §10 asks. The title strip is drawn to §3's measurements, because the screenshot didn't include one.
- **The Kicker and DS-Mini mockups' headers are drawn, not cropped**, because no compact-class CI screenshot was available. Use the real compact header from `ShellEditor`; only the body is the target.
- As before, the mockups set layout and control types; `fancy-core` sets the exact styling. Compare each plugin's CI screenshot side by side with its mockup and with Delay or FIR (§10, step 3).

## 2. Fancy DS (de-esser)

### 2.1 Checklist (§10)
1. **Accent:** green; doesn't follow a selection.
2. **Size class:** Phase 2, 760 × about 445 (header + body).
3. **Controls:**

| Control | Component | Range / default |
|---|---|---|
| Detector (top row) | `ParamDropdown` with sections | Main L+R (default), Main Mid, Main Side, Main L, Main R; Sidechain L+R, Sidechain L, Sidechain R |
| Mode (top row, right) | `ParamSegments`, accent-filled | **Split** (default: only the detection band is turned down) / **Wide** (the whole signal is turned down) |
| Threshold (left column) | `ThresholdMeter` over the detection-band level, Gate's style with scale and read-out | −60…0 dB, default −28 |
| GR (left column) | `GainReductionMeter`, single bar, accent | display |
| Display card | own component (§2.2) | |
| Detection card: shape | `ParamSegments`, grey | Band (default) / Shelf |
| Detection card: Freq | `Knob` tight, **blue** (it's a filter frequency) | 2–16 kHz, default 6.2 kHz, log |
| Detection card: Width | `Knob` tight, accent | 0.3–3 octaves, default 1.0 (Band only; locked with a hint in Shelf) |
| Reduction card: Range | `Knob` tight | 0 to −24 dB, default −12 (maximum reduction) |
| Reduction card: Attack | `Knob` tight | 0.1–20 ms, default 0.5 |
| Reduction card: Release | `Knob` tight | 5–500 ms, default 60 |

4. **Footer:** Relative threshold (on), Listen (off), Link channels (on), then the EDIT link. **EDIT drawer:** Lookahead (Off / 1 / 2 / 5 ms, default Off; adds latency, with the latency note), Split filter (Minimum phase, default / Linear phase, which adds latency), Output trim.
5. **Right column (100 px):** Out, Gain fader (teal).

### 2.2 Display
- **Top part (scrolling history, about 3 s):** detection-band level as a grey fill, the threshold as a green line, and reduction as a green fill hanging from the top.
- **Bottom strip (frequency ruler, 1–20 kHz log):** the detection band as a translucent green bar, with a blue handle at Freq. Dragging the handle sets Freq; dragging the bar's edges sets Width.

### 2.3 DSP
- **Detector:** 2nd-order band-pass (Band) or 2nd-order high-pass (Shelf) at Freq, then peak level in dB with a 1 ms peak hold, linked across channels (max) when Link channels is on.
- **Relative threshold (default on):** compare the band level with the **full-band** level (RMS, 50 ms) instead of with 0 dBFS, so the threshold means "sibilance this much louder than the voice". The meter's scale changes to "dB relative" when it's on.
- **Gain computer:** soft knee 6 dB, ratio fixed at 8:1, limited by Range; Attack and Release smoothing in dB.
- **Split mode:** split the signal into the detection band and the rest with a complementary pair (the band and its exact remainder, so they sum back to the input bit-identically when no reduction is applied; minimum phase by default, linear phase in the drawer), and apply gain only to the band. **Wide mode:** apply the gain to the whole signal.
- **Listen:** outputs only what's being removed (the delta), so I can hear whether it's catching "s" sounds or real consonants.
- Zero latency by default.

### 2.4 Known issues and required fixes
| Issue (source) | Fix and test |
|---|---|
| **Free de-essers that "seem to do nothing"**: a REAPER user reports trying a free de-esser that "seems to be doing nothing at all", even when correctly inserted (Cockos forum, "Need de-esser so bad!"). | Default preset must visibly act on a typical vocal: −28 dB relative threshold, Split, 6.2 kHz. The Listen switch and GR meter make its action obvious. Test: on the bundled test vocal (a synthetic voice with "s" bursts at +12 dB in 5–9 kHz), the default preset reduces the bursts by at least 6 dB and the vowels by less than 0.5 dB. |
| **De-essers dull the voice or cause a lisp** when they turn down the whole signal or react to chesty low end. A recent de-esser review describes a high-pass "bias" control so the detector "ignores chesty low-end and focuses only on the bite". | **Split** by default (only the band is reduced); Range caps the maximum cut; Band/Shelf detection with an adjustable Width. Test: a 200 Hz + 3 kHz vowel at +6 dB over threshold triggers less than 1 dB of reduction. |
| **A fixed threshold misses "s" sounds when the singer gets quieter, and over-cuts when louder.** | **Relative threshold** (§2.3). Test: the same "s"-burst signal at −30, −18 and −6 dBFS gets the same reduction within 1 dB. |
| **Split de-essers can colour the sound even when not reducing** (filter phase). | Complementary split (§2.3). Test: with Range 0 dB, output is bit-identical to input (min phase) or nulls below −120 dB against the latency-compensated input (linear phase). |
| **Very fast "s" onsets slip through.** | Optional Lookahead in EDIT, with reported latency (H.6). Test: with 2 ms lookahead, the first 1 ms of a burst is already reduced by ≥ 3 dB. |

## 3. Fancy Room (ReaVerbate-inspired)

### 3.1 Checklist (§10)
1. **Accent:** purple (approved, §0.2); doesn't follow a selection.
2. **Size class:** Phase 2.
3. **Controls:**

| Control | Component | Range / default |
|---|---|---|
| Display card | own component (§3.2) | |
| Space card: Pre-delay | `Knob` tight | 0–250 ms, default 12 (or note values when Sync pre-delay is on, as Fancy Delay) |
| Space card: Size | `Knob` tight | 0–100 %, default 55 (early-reflection spacing and diffusion delay lengths) |
| Space card: Decay | `Knob` tight | 0.1–20 s, default 1.8 (RT60, independent of Size) |
| Space card: Diffusion | `Knob` tight | 0–100 %, default 70 |
| Space card: Mod | `Knob` tight | 0–100 %, default 25 (delay-line modulation depth) |
| Space card: Width | `Knob` tight, bipolar, `setShowDefaultMark` | −1.00…+2.00, default 1.00 (0 mono, −1 reversed, as the original) |
| Tone card: Damping | `Knob` tight | 0–100 %, default 45 (how much faster highs decay) |
| Tone card: Low cut | `Knob` tight, blue | 20–1000 Hz, default 150 |
| Tone card: High cut | `Knob` tight, blue | 1–20 kHz, default 9 kHz |

4. **Footer:** Sync pre-delay, Freeze, Wet only (send), then EDIT. **EDIT drawer:** Ducking (0–100 %, reverb turned down while the dry signal plays), Quality (Eco / Normal / High: number of delay lines), Early reflections level (−inf…+6 dB), Stereo input (Stereo / Mono-sum).
5. **Right column (104 px):** Out, Wet, Dry.

### 3.2 Display
An energy-decay view, 0–3 s (auto-extends to the decay), 0 to −60 dB: early reflections as thin white lines after the pre-delay; the overall decay as a filled purple slope; the decay of the highs as a dashed purple line (steeper with more Damping); a marker and label "decay 1.80 s (to −60 dB)". It's computed from the parameters, not measured from audio, so it's always readable.

### 3.3 DSP
- **Early reflections:** a tapped delay with about 12–20 taps per channel, spaced by Size, panned across the stereo field.
- **Late reverb:** a feedback delay network (8 lines in Eco, 16 in Normal and High) with an orthogonal mixing matrix, delay lengths scaled by Size from mutually prime values, input diffusion via a chain of all-pass filters (Diffusion), and per-line gain set from **Decay** (RT60) so decay time is exact and independent of Size. Damping is a one-pole low-pass per line, set so the highs' RT60 = Decay × (1 − 0.7 × Damping).
- **Modulation:** slow, randomised modulation of a few delay lengths (Mod), with interpolated reads, to stop metallic ringing. The random source is seeded, so the same settings give the same output every render (needed for H.1's offline-equals-real-time test).
- **Freeze:** feedback goes to 100 % with no input and no damping, held without blow-up.
- **Tail:** report the tail length to the host (VST3 `getTailSamples`, from Decay + Pre-delay) so hosts render the tail on export and don't cut it off.

### 3.4 Known issues and required fixes
| Issue (source) | Fix and test |
|---|---|
| **ReaVerbate is described as "not a great sounding reverb"** (REAPER blog tutorial) and as having "a digital sound" (Jake Duffie Audio). | FDN with diffusion, modulation and per-line damping (§3.3). Test: an impulse with Decay 2 s, Mod 25 %: no spectral peak in the tail more than 6 dB above the smoothed (1/3-octave) tail spectrum, measured between 0.3 and 1.5 s ("no metallic ringing"). |
| **No decay time in seconds**: the original has only Room size and Dampening, and the blog describes size only as "how big the room is", so decay length isn't directly controllable. | Separate **Decay** (RT60) and **Size**. Test: measured RT60 (from the energy-decay curve, T30 method) is within ±10 % of the Decay setting at Size 20 %, 55 % and 90 %, Decay 0.5, 2 and 8 s. |
| **No pre-delay tempo sync, freeze or send mode**: features users expect from a modern reverb. | Sync pre-delay, Freeze, Wet only (send). Test: Freeze holds the tail level within 1 dB for 60 s with no NaN/inf and no growth. |
| **Reverb tails cut off on export** in some hosts if the plugin doesn't report its tail. | `getTailSamples` (§3.3). Test (host harness): rendering 1 s of audio followed by silence produces the full tail until −90 dB. |
| **Mono compatibility of wide reverbs.** | Width control as the original, plus Stereo input mode. Test: at Width 1.00, the mono sum of the tail is within 3 dB of the stereo energy. |

## 4. Fancy Saturate

### 4.1 Checklist (§10)
1. **Accent:** orange; doesn't follow a selection.
2. **Size class:** Phase 2.
3. **Controls:**

| Control | Component | Range / default |
|---|---|---|
| Type (top row) | `ParamSegments`, accent-filled | Tape (default) / Tube / Transistor / Clip |
| Transfer-curve card | own component | display |
| Harmonics card | own component | display |
| Drive card: Drive | `Knob` tight | 0–36 dB, default 6 |
| Drive card: Character | `Knob` tight, bipolar | even 100 % … odd 100 %, default even 30 % (asymmetry: more even or more odd harmonics) |
| Drive card: Tone | `Knob` tight, bipolar | −6…+6 dB tilt after the saturator, default 0 |
| Band card: Low | `Knob` tight, blue | Off / 20 Hz–2 kHz, default Off. Below this, the signal passes clean |
| Band card: High | `Knob` tight, blue | 1 kHz–20 kHz / Off, default Off. Above this, the signal passes clean |

4. **Footer:** Auto gain (on), Listen to added harmonics (off), then EDIT. **EDIT drawer:** Oversampling (Off / 2× / 4× default / 8×; latency note), Oversampling filter (Minimum phase default / Linear phase), Input trim, DC filter (on).
5. **Right column (104 px):** Out, Wet, Dry.

### 4.2 Displays
- **Transfer curve:** input → output of the current Type, Drive and Character, with the unity diagonal dashed.
- **Harmonics:** the level of H2–H10 that the current settings add to a 1 kHz tone at −12 dBFS, as bars (even harmonics at full accent, odd at 70 %), plus the fundamental greyed and a THD read-out. Computed offline on a background thread whenever a setting changes, not measured from the audio, so it's always readable.

### 4.3 DSP
- Band split (when Low/High are set) with complementary filters: only the middle band goes through the saturator; the rest is added back time-aligned.
- Saturator per Type (all with Character as a DC-compensated bias): **Tape** (soft tanh-like curve with gentle high-frequency loss at high drive), **Tube** (asymmetric soft curve), **Transistor** (harder knee), **Clip** (hard clip with a small rounded knee). Each uses first-order anti-derivative anti-aliasing (ADAA) as in Fancy Exciter, inside the oversampler.
- Tone tilt after the saturator. DC blocker (on).
- **Auto gain:** a fixed compensation curve per Type, Drive and Character, measured offline at build time on pink noise (−18 dBFS RMS), not a signal follower, so it can't pump.
- **Listen to added harmonics:** wet minus the latency-aligned dry, with level-matching.

### 4.4 Known issues and required fixes
| Issue (source) | Fix and test |
|---|---|
| **Aliasing**: saturation creates harmonics above Nyquist that fold back as inharmonic tones (HoRNet's aliasing guide; KVR threads on anti-aliased saturation). | Oversampling (4× default) and ADAA. Test: 10 kHz sine at −6 dBFS, Drive 24 dB, 44.1 kHz, Clip type: aliased components below −90 dBFS at 4×, below −60 dBFS with oversampling Off (and the drawer says so). |
| **"Louder sounds better"**: drive raises the level, which fools comparisons. | Auto gain (§4.3) on by default. Test: pink noise at −18 dBFS RMS: output loudness within ±1 LU of input for every Type at Drive 0, 12, 24 and 36 dB. |
| **Saturating a full mix or bass-heavy track gets harsh or muddy** (KVR thread "Isolated saturation doesn't sound good"). | Band limits (Low/High), Tone, and Wet/Dry. Test: with Low = 200 Hz, a 60 Hz sine at −6 dBFS gains no harmonics above −90 dBFS. |
| **Asymmetric curves create DC offset.** | DC blocker. Test: Character ±100 %, Drive 36 dB: output DC below −100 dBFS. |
| **Latency from oversampling.** | Reported exactly (H.6); dry path aligned. Test: Wet 0 dB, Dry 0 dB, Drive 0: no comb filtering (the sum is flat within 0.1 dB, 20 Hz–20 kHz). |

## 5. Fancy Limit

### 5.1 Checklist (§10)
1. **Accent:** red; doesn't follow a selection.
2. **Size class:** Phase 2.
3. **Controls:**

| Control | Component | Range / default |
|---|---|---|
| Style (top row) | `ParamSegments`, accent-filled | Clean (default) / Punchy / Dense |
| Target (top row, right) | `ParamDropdown` | Streaming −14 LUFS (default), Podcast −16, Broadcast −23 (EBU R128), Custom… (typed) |
| Threshold (left column) | `ThresholdMeter`, Gate's style, over the input level | −30…0 dB, default 0 (the signal is pushed up by −Threshold, then limited at Ceiling) |
| GR (left column) | `GainReductionMeter`, single bar, accent | display |
| Display card | own component (§5.2) | |
| Limiter card: Ceiling | `Knob` tight | −12…0 dB, default −1.0 |
| Limiter card: Release | `Knob` tight | 1–1000 ms, default 80 (scaled by Auto release when on) |
| Limiter card: Lookahead | `Knob` tight | 0–5 ms, default 2 |
| Limiter card: Link | `Knob` tight | 0–100 %, default 100 (stereo link) |

4. **Footer:** True peak (on), Auto release (on), Listen to removed (off), then EDIT. **EDIT drawer:** Oversampling (Off / 2× / 4× default / 8×), Dither (Off default / 16-bit / 24-bit, with or without noise shaping), Loudness meter reset, Meter ballistics (EBU / RMS).
5. **Right column (100 px):** Out (with scale), and a single **LUFS** bar for short-term loudness with a red marker at the Target.

### 5.2 Display
- **Left part (scrolling history, about 4 s):** input level (grey fill), output (white line), reduction (red, from the top), and the ceiling as a dashed red line.
- **Right panel (in `#141414`):** Short-term LUFS, Integrated LUFS, True peak (dBTP) and Loudness range (LU). Clicking the panel resets the integration (with the reset also in EDIT).

### 5.3 DSP
- Reuse Fancy Comp's true-peak-aware output limiter from `fancy-core` as the core (it's already tested at 200 dB over threshold). Don't write a second limiter.
- Input gain = −Threshold. Lookahead with a smooth (Hann-shaped) gain envelope over the lookahead window. Release is program-dependent when Auto release is on (a fast and a slow release blended by how much the gain reduction is sustained).
- **Styles** set the internal envelope shapes only: Clean (slowest, smoothest attack and release curves), Punchy (lets very short transients through more), Dense (faster release). The visible knobs keep working in every Style.
- **True peak:** detect peaks on a 4× oversampled copy (ITU-R BS.1770 true-peak method) and limit so that the true peak stays at or below Ceiling. A final sample-peak safety clip at Ceiling catches anything that's left.
- **Latency:** report a **constant** 5 ms (the maximum lookahead) plus the oversampling filter delay; changing Lookahead never changes the reported latency (like Gate's Pre-open).
- **Loudness:** ITU-R BS.1770 / EBU R128 momentary, short-term, integrated (gated) and loudness range, computed on the output.

### 5.4 Known issues and required fixes
| Issue (source) | Fix and test |
|---|---|
| **Inter-sample peaks**: a limiter that only watches samples lets the reconstructed waveform go over the ceiling after conversion or lossy encoding (eMastered's guide to inter-sample peaks; FabFilter's Pro-L 2 help on oversampling and true-peak limiting). | True peak mode (§5.3), on by default. Test: a set of hard cases (997 Hz and 11.025 kHz sines at fs/4 phase offsets, square waves, full mixes), driven 30 dB over the ceiling: true peak measured by an independent 8× reference meter never exceeds the Ceiling + 0.1 dB. |
| **Distortion on bass and pumping** with fast release. | Auto release and Styles. Test: a 40 Hz sine with 10 dB of gain reduction: THD stays below 1 % with Auto release on. |
| **The ceiling is exceeded at extreme drive** (the original ReaComp limiter failed above about 150 dB over; see Phase 1 §5.2). | Reuse the tested core plus the final safety clip. Test: +200 dB over threshold, sample peaks never exceed Ceiling. |
| **Loudness targets for streaming and broadcast** are what people use a limiter for now, and need a reliable meter. | BS.1770 meter and Target dropdown. Test: the EBU Tech 3341 test signals read within ±0.1 LU (integrated) and ±0.2 dB (true peak) of the published values. |
| **Changing lookahead moves the host's latency compensation.** | Constant reported latency (§5.3). Test: identical reported latency for Lookahead 0, 2 and 5 ms; output time-aligned in all three. |

## 6. Fancy Kicker: Sub mode (Thunderkick merged in)

### 6.1 What it is
Thunderkick is Philip Stillwell's JSFX "MDCT subsynthesis filter", which "adds in sub-harmonics to enhance lowest octaves for non-pitched instruments. Mostly useful on kick drum". Its controls are Effect (dB), Cutoff, Gain (dB) and Shift. It answers the main gap the market report found in Fancy Kicker: Kicker can only boost bass that's already there, while Sub mode **creates** new bass an octave (or two) below.

### 6.2 Layout (`21__kicker_sub_mockup.png`)
- A **Boost | Sub** segment (accent-filled) at the top of the body. **Boost** is today's Kicker, unchanged and the default, so existing projects load and sound bit-identical.
- In **Sub**, the controls swap (still at most four main controls, as the compact class requires):
  - Big knob **Cutoff** (40–200 Hz, default 70): the highest frequency fed to the sub generator (Thunderkick's Cutoff).
  - An **Octave** segment under it, grey: −1 oct (default) / −2 oct (Thunderkick's Shift).
  - Small knobs **Sub** (−inf…+6 dB, default −9: the generated sub level, Thunderkick's Effect) and **Dry** (−inf…+6 dB, default 0, Thunderkick's Gain).
- Bottom: EDIT link, and a caption in `textSecondary` explaining the current mode ("Sub: adds an octave below" / "Boost: lifts the kick's own low end").
- **EDIT drawer additions** (Sub mode): Engine (**Divider**, zero latency, default / **Spectral**, smoother on complex kicks, adds latency), Sensitivity (−60…0 dB, default −40: below this the generator stays silent, so bleed and noise don't make sub), Sub low-pass (60–250 Hz, default 120), Mono sub (on).

### 6.3 DSP
- **Divider engine (default):** low-pass the input at Cutoff, track zero crossings of that band, and generate a sine at ½ (or ¼) of its frequency, phase-continuous; amplitude follows the band's envelope (attack 1 ms, release 30 ms), gated by Sensitivity, then low-passed at Sub low-pass. Zero latency.
- **Spectral engine (option):** an MDCT-based sub-synthesis like Thunderkick's (shift the low bins down an octave), with its block latency reported (H.6).
- 15 Hz subsonic high-pass on the sub, as in Boost mode; Mono sub sums the sub to mono.

### 6.4 Tests
1. Boost mode, existing state: bit-identical to the current release.
2. A synthetic kick (80 Hz body, exponential decay) in Sub mode, −1 oct: energy at 40 Hz rises by at least 12 dB; no energy added above Sub low-pass + 1 octave beyond −60 dB.
3. Noise at −50 dBFS with Sensitivity −40: no sub generated (< −90 dBFS).
4. No clicks: the generated sub has no discontinuity larger than its own maximum step × 1.5, including at note-off and re-trigger.
5. Divider engine latency 0; Spectral engine reports its latency exactly.

## 6b. Fancy DS-Mini (one-knob de-esser)

### 6b.1 What it is
For quick jobs: pick what you're de-essing, turn one knob. It runs **the same engine as Fancy DS**, so DS-Mini at a given setting sounds exactly like Fancy DS at the equivalent settings.
- Move the DS engine into `fancy-core` (e.g. `fancy::dsp::DeEsser`) and have both plugins use it. Don't copy it.
- Inspired by the "profile + one big knob" idea of simple de-essers such as MuseFX De-Ess. Take only the idea; the look is ours (`22__ds_mini_mockup.png`), and no names, icons or graphics from that product.

### 6b.2 Checklist (§10)
1. **Accent:** blue `#3598DB` (`uiBlue`). **Approved exception** to the rule that blue means a filter knob: DS-Mini has no filter knobs, so nothing inside it conflicts. Record this in `DESIGN_SYSTEM.md` §2.
2. **Size class:** compact, 392 wide (three main controls).
3. **Controls:**

| Control | Component | Range / default |
|---|---|---|
| Profile (top) | `ParamDropdown` with ‹ › step arrows, styled like the preset pill (if `fancy-core` has no such variant, add it there and to the gallery first) | see §6b.3, default Female voice |
| Profile caption | text, `textSecondary` | what the profile targets, e.g. "Sibilance · 5.5–9 kHz" |
| Display | own component: a small version of Fancy DS's history (detection level, reduction from the top, a dashed line where reduction starts) | display |
| Amount | `Knob`, `largeStyle` | 0.0–10.0, default 5.0 |

4. **Footer:** Listen (the removed part only), then the EDIT link. **EDIT drawer:** Mode (Split default / Wide), Lookahead (Off default / 2 ms, with latency note), Link channels (on), and a **"Save as Fancy DS preset…"** action that writes a Fancy DS preset with the exact equivalent settings, for when someone outgrows the Mini.
5. **Right column (116 px):** GR (single bar, accent) and Out.

### 6b.3 Profiles and the Amount mapping
Starting values; tune them by ear and on the test signals before the milestone is done, and record the final numbers in `docs/ds-mini.md`.

| Profile | Detection | Attack / Release |
|---|---|---|
| Female voice | Band, 7.0 kHz, 1.0 oct (≈5.5–9 kHz) | 0.5 / 60 ms |
| Male voice | Band, 5.0 kHz, 1.0 oct (≈4–6.5 kHz) | 0.5 / 60 ms |
| Sibilance (any voice) | Shelf from 5.0 kHz | 0.5 / 60 ms |
| Harshness | Band, 3.5 kHz, 1.2 oct (≈2.5–5 kHz) | 2 / 120 ms |
| Acoustic guitar (string squeak) | Band, 3.0 kHz, 1.0 oct | 1 / 80 ms |
| Bowed strings (rosin) | Band, 4.0 kHz, 1.5 oct | 2 / 120 ms |

- All profiles use Relative threshold and Split mode (unless changed in EDIT).
- **Amount** sets threshold and range together: relative threshold = −4 × Amount dB, Range = −1.8 × Amount dB. So **Amount 0 does nothing**, 5 is moderate (−20 dB threshold, up to −9 dB cut), and 10 is strong (−40 dB, up to −18 dB).
- Changing profile crossfades the detector settings over 50 ms, so switching while playing never clicks.

### 6b.4 Tests
1. Amount 0 → output bit-identical to the input.
2. **Equivalence:** for each profile at Amount 2, 5 and 8, DS-Mini's output equals Fancy DS's at the mapped settings, bit-identically.
3. Default (Female voice, Amount 5) on the §2.4 test vocal: "s" bursts reduced by ≥ 6 dB, vowels by < 0.5 dB.
4. Switching profiles every 100 ms on a full-level signal: no sample-to-sample step larger than the input's own maximum step × 1.5.
5. "Save as Fancy DS preset" → loading that preset in Fancy DS gives bit-identical output.
6. CPU (ratio method) within 10 % of Fancy DS.

## 7. Acceptance (each plugin)
§7 of `BUILD_PROMPT.md` plus Phase 2.1's S, H and W checks from the start: styled pop-ups (listed in `getPopupOpeners()`), offline render equals real time, keyboard pass-through, scaling screenshots, host-harness scenarios, validator, undo/A-B/Compare, and the CPU ratio method. Every test in §2.4, §3.4, §4.4, §5.4 and §6.4 is in Catch2 and green in CI. CPU targets (Fancy Comp ratio method): DS < 1 %, DS-Mini < 1 %, Room (Normal) < 3 %, Saturate (4×) < 2 %, Limit (4× true peak) < 2 %, Kicker Sub (Divider) < 0.5 %.

## 8. Sources for the issues above
- Cockos forum, "Need de-esser so bad!": https://forums.cockos.com/showthread.php?t=160053
- REAPER blog, ReaVerbate tutorial: https://reaper.blog/2016/03/reaverbate-tutorial-and-free-presets
- Jake Duffie Audio, Introduction to ReaPlugs: https://jakeduffieaudio.com/articles/reaplugs/
- REAPER Effects Guide (ReaVerbate controls): https://www.reaper.fm/guides/ReaEffectsGuide.pdf
- Keith Haydon, A JSFX resource, 2nd ed. (Thunderkick): https://www.keithhaydon.com/Reaper/JSFX2.pdf
- Cockos forum ("thunderkick is a subsynth meant for kick drum"): https://forum.cockos.com/showthread.php?p=40876
- HoRNet, What aliasing is: https://www.hornetplugins.com/what-is-aliasing-distortion-visual-guide-oversampling/
- KVR, "Isolated saturation doesn't sound good": https://www.kvraudio.com/forum/viewtopic.php?p=8075089
- eMastered, Intersample peaks: https://emastered.com/blog/intersample-peaks
- FabFilter Pro-L 2 help, oversampling: https://www.fabfilter.com/help/pro-l/using/oversampling
