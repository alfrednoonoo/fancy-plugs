# Phase 2: Fancy XComp, Fancy Delay, Fancy FIR

This extends `docs/BUILD_PROMPT.md`. **Every rule there still applies**: cloud builds only, the design system (§2), real-time rules (§4), acceptance checks (§7), the CC BY attribution, and "ask before changing the parameter list or layout". This file only adds what is different for three new plugins.

## P0. When and in what order

- Start Phase 2 only after all five Phase 1 plugins pass §7, unless I tell you otherwise.
- Order: **Fancy Delay** (simplest) → **Fancy XComp** (reuses Fancy Comp's detector and gain computer) → **Fancy FIR** (hardest: FFT engine and a drawable curve editor).
- Write `docs/<plugin>.md` design notes first, exactly as in Phase 1.
- Add these three to `AGENTS.md`/`CLAUDE.md` and `docs/PROGRESS.md`.

| Product name | Inspired by | Main accent |
|---|---|---|
| **Fancy Delay** | ReaDelay (multi-tap delay) | Yellow `#F1C315`; filters blue |
| **Fancy XComp** | ReaXcomp (multiband compressor) | Per-band colours (same as Fancy EQ); global controls teal |
| **Fancy FIR** | ReaFIR (FFT EQ / gate / compressor / noise subtraction) | Colour follows the mode (see P3.3); global controls teal |

## P1. Reference mockups (IMPORTANT: read this before opening them)

There are no designer mockups for these three. The images in `reference/phase2/` were drawn **in the same design system** to give you a concrete target:

| File | Plugin |
|---|---|
| `reference/phase2/11__delay_mockup.png` | Fancy Delay |
| `reference/phase2/12__xcomp_mockup.png` | Fancy XComp |
| `reference/phase2/13__fir_mockup.png` | Fancy FIR |

How to use them:
- They define the **layout, the grouping and the control type of every control** (knob vs fader vs checkbox vs dropdown vs segmented switch). Follow them the way you follow the §2.5 control map.
- Exact styling (colours, radii, font sizes, knob drawing, meter gradient, header bar) comes from the **shared `fancy-core` components you already built** and from the Figma data. Where a mockup pixel disagrees with `fancy-core`, `fancy-core` wins. Never fork a component to match a mockup; reuse it.
- They are 2× renders. Divide pixel sizes by 2. Target window sizes at 100 %: Delay ≈ 760 × 410, XComp ≈ 760 × 455, FIR ≈ 760 × 430.
- They show sample values. Defaults are in P2–P4.

## P2. Fancy Delay (ReaDelay-inspired)

### P2.1 Layout (see `11__delay_mockup.png`)
- **Header**: standard. Show the host tempo on the right ("120.0 BPM (host)").
- **Tap map** (top, full width): time axis 0 → 4 beats (auto-extends to the longest tap), with ms labels. The dry signal is a white bar at 0. Each tap is a coloured bar at its delay time; bar height = tap volume; numbered handle on top. Feedback repeats are drawn as faded bars of the same colour. **Drag a tap handle left/right** to change its time (snaps to the note grid when the tap is in Note mode; hold Shift for free). Drag up/down to change volume.
- **Tap row**: tabs `1 2 3 …` in their tap colours, `+` to add, the tap title and time ("375 ms · 1/8 dotted"), and on the right Solo `S`, power (bypass tap), `×` (delete; at least one tap must remain).
- **Time section** (left card): a `ms | Note` segmented switch. A large **Length** knob. In Note mode, a note-value dropdown (1/1 … 1/64) and a straight / dotted / triplet segmented switch, plus a read-out of the resulting ms. In ms mode, the knob shows ms and the note controls are hidden.
- **Tap controls** (right card): knobs **Feedback, Lowpass (blue), Highpass (blue), Resolution, Width, Pan (bipolar)**, and a **Volume** vertical fader in the tap colour.
- **Right column**: Out meter, **Wet** fader, **Dry** fader (teal), as in Fancy Comp.
- **Footer checkboxes**: Filters inside feedback loop, Ping-pong, Saturate feedback, Smooth time changes.
- Tap colours cycle: yellow `#F1C315`, orange `#E77E23`, purple `#9648BA`, eq-blue `#24AEC6`, eq-green `#0CC08A`, red `#E84E3C`.

### P2.2 Parameters (per tap unless marked global; keep every function the original has)

| Parameter | Range / default |
|---|---|
| Enabled (power) | on |
| Solo tap | off |
| Time mode | ms / Note, default Note |
| Length (ms) | 0–10 000 ms, default 375 |
| Length (note) | value 1/1–1/64 × {straight, dotted, triplet} × multiplier 1–16; default 1/8 dotted |
| Feedback | 0–100 % (up to 110 % only when Saturate feedback is on), default 0 % |
| Lowpass | 200 Hz–22 kHz / off, default off |
| Highpass | 20 Hz–5 kHz / off, default off |
| Resolution | 4–24 bit, default 24 (24 = bypass) |
| Stereo width | 0.00–2.00, default 1.00 |
| Volume | −inf…+12 dB, default 0 dB |
| Pan | 100 L…100 R, default C |
| Ping-pong | off (alternates each repeat L↔R; the tap's pan sets the start side) |
| Filters inside feedback loop | **on** (see P2.3) |
| Saturate feedback | off |
| Wet (global) | −inf…+12 dB, default 0 dB |
| Dry (global) | −inf…+12 dB, default 0 dB |
| Smooth time changes (global) | on |
| Max taps | 16 |

Factory presets: Slapback, 1/8 dotted, 1/4 ping-pong, Triplet echo, Lo-fi tape (Resolution 10 bit + LP 3 kHz + Saturate), Haas widener (two taps, 8 ms / 13 ms, panned hard).

### P2.3 Known issues with the original and the required fix

| Issue reported by users | Required fix + test |
|---|---|
| **Tempo sync is awkward.** Musical length is a raw count of 1/8 notes, so a triplet means typing 0.67 and a dotted note means doing arithmetic; users keep asking how to get 1/8 triplet or 1/8 dotted. | Note-value dropdown + straight/dotted/triplet switch + multiplier (P2.1). Test: at 120 BPM, 1/8 dotted = 375.0 ms, 1/8 triplet = 166.67 ms, 1/4 = 500.0 ms, exact to ±1 sample. Follow host tempo changes. |
| **Feedback is filtered only once**, so repeats sound static instead of getting darker each time. | Default: lowpass/highpass/resolution sit **inside** the feedback loop, so each repeat is filtered again. The checkbox restores the original behaviour. Test: with LP 2 kHz and 60 % feedback, energy above 2 kHz must fall on each successive repeat. |
| **No obvious ping-pong**; users need presets or several hand-panned taps. | Per-tap Ping-pong switch + factory preset. Test: repeats alternate L/R with the correct level. |
| **Feedback runs away / distorts with tempo-synced delays during tempo changes.** | Feedback capped at 100 % unless Saturate is on. When it is on, a soft-clip (tanh) sits in the loop so it can never exceed 0 dBFS. Time changes follow tempo smoothly (below). Test: feedback 100 % + tempo ramp 90→180 BPM for 30 s → output stays below 0 dBFS, no NaN/inf. |
| **Clicks / zipper noise when delay time is automated.** | "Smooth time changes" on: crossfade between two read heads over 20–50 ms when time jumps. Off: tape-style smoothed read position (pitch bends, no clicks). Test: render a step change 375→500 ms; no sample-to-sample jump larger than the input's own. |
| **ReaDelay as a standalone VST: no delay heard when Dry = −inf** (reported bug in another host). | Wet and Dry are fully independent gains after the delay network. Test: Dry −inf, Wet 0 dB → delayed signal present at the correct level. |

## P3. Fancy XComp (ReaXcomp-inspired multiband compressor)

### P3.1 Layout (see `12__xcomp_mockup.png`)
- **Header**: standard, plus a **Detector** dropdown (Main L+R, Main L, Main R, Sidechain L+R, Sidechain L, Sidechain R). I/O pill "4 in 2 out" (stereo main + stereo sidechain).
- **Graph** (top): log frequency axis 20 Hz–20 kHz, dB axis 0 to −60. Each band is a translucent region in its band colour (selected band stronger), up to its **threshold line**, with a numbered handle on the line. **Crossover lines** are thin blue (`#3690CC`) verticals with a grab tab at the bottom. **Gain reduction** per band is a yellow bar hanging from the top of the band (depth = GR). A live **spectrum** line in orange (toggle).
  - Drag a crossover line → moves that band edge. Drag a band handle up/down → threshold. Wheel or Alt-drag on a handle → ratio. Ctrl/Cmd-drag on any handle when **Link bands** is on → moves all thresholds together, relative. Double-click a handle → reset that band. Double-click empty graph → split the band under the cursor.
- **Band row**: tabs `1 2 3 4 +` in band colours, band title + its frequency range, then Solo `S`, power, `×`.
- **Band editor** card, in the selected band's colour: input meter + **Threshold vertical fader** + **GR meter** (same component as Fancy Comp), then knobs **Ratio, Knee, Attack, Release, RMS size, Gain** (band make-up, bipolar), and checkboxes **Auto make-up, Auto release, Feedback detect, Sidechain** (this band listens to the sidechain detector).
- **Right column**: Out meter, **Wet** and **Dry** faders (teal).
- **Footer**: Show spectrum, Link bands, Show grid, Limit output, and a **Crossover** dropdown (LR 24 dB · Min phase / LR 48 dB · Min phase / Linear phase).
- Band colours: 1 `#C0363C`, 2 `#9648BA`, 3 `#24AEC6`, 4 `#0CC08A`, then cycle the accent palette.

### P3.2 Parameters

| Parameter | Range / default |
|---|---|
| Bands | 1–8, default 4 with crossovers 120 Hz / 1.2 kHz / 6 kHz |
| Active, Solo (per band) | on / off |
| Top frequency (per band) | 20 Hz–20 kHz (log), crossovers can't cross each other (min 1/3 octave apart) |
| Threshold | −60…0 dB, default 0 |
| Ratio | 0.5:1 (expansion) … ∞:1, default 2:1. Below 1:1 = upward/downward expansion as in the original |
| Knee | 0–24 dB, default 6 |
| Attack | 0–500 ms, default 10 |
| Release | 0–5000 ms, default 120 |
| RMS size | 0–250 ms, default 5 |
| Gain (band make-up) | −24…+24 dB, default 0 |
| Auto make-up, Auto release (program-dependent), Feedback detect, Sidechain | per band, defaults off / on / off / off |
| Detector input (global) | see header |
| Crossover type (global) | LR 24 min-phase (default), LR 48 min-phase, Linear phase |
| Link bands (global) | off |
| Wet / Dry (global) | 0 dB / −inf |
| Limit output (global) | off; same limiter as Fancy Comp |
| Log-scale automated freq. (global) | on |

Reuse from `fancy-core`: Fancy Comp's detector, gain computer, attack/release conventions (including the documented time-constant convention and the 90 %-rise toggle), auto-release, the limiter, the Threshold+GR component, and Fancy EQ's analyzer.

### P3.3 Known issues with the original and the required fix

| Issue reported by users | Required fix + test |
|---|---|
| **"Weird phasing" in the higher bands, blamed on the crossover design.** | Linkwitz-Riley crossovers (24 dB/oct default, 48 option) **with all-pass phase compensation** in the lower bands, so every band has the same phase and the bands sum flat. Optional **linear-phase** crossover (FIR, latency reported). Tests: all bands at 1:1 and 0 dB → magnitude flat within ±0.1 dB, 20 Hz–20 kHz, in every crossover mode. Linear mode: output nulls against the latency-compensated input to below −90 dB. |
| **Adjusting several bands is tedious; users ask to raise or lower all bands together.** | **Link bands**: relative moves of Threshold, Ratio, Attack, Release and Gain across all bands. Hold Alt to edit one band while linked. |
| **No easy sidechain per band; users split bands with EQs and use separate compressors instead.** | Global detector input + per-band **Sidechain** checkbox (the band's detector is fed by the same-band-filtered sidechain). Test: a kick on the sidechain ducks only the bands that have Sidechain on. |
| **Users want a multiband limiter for mastering.** | The **Limit output** brickwall limiter (true-peak aware) after the band sum. With ratio ∞:1, 0 ms attack and Limit output on, it serves as a multiband limiter. Document this in the manual. |
| **Unpredictable level with auto make-up.** | Auto make-up estimates gain from threshold/ratio/knee, not from the signal, so it never pumps. Show the applied make-up in the Gain knob read-out. |
| **Moving crossovers while audio plays causes clicks.** | Crossover frequencies are smoothed (per-sample coefficient interpolation, §4.2). Test: sweep a crossover 100 Hz→2 kHz in 200 ms with no clicks. |

## P4. Fancy FIR (ReaFIR-inspired FFT processor)

### P4.1 Layout (see `13__fir_mockup.png`)
- **Header**: standard; the current **latency** is shown on the right ("Latency 2048 smp · 42.7 ms") and updates when FFT size or mode changes.
- **Mode** segmented switch: EQ · Gate · Compressor · Subtract · Convolve L/R. Mode colours: EQ `#9648BA`, Gate `#F1C315`, Compressor `#00A884`, Subtract `#E84E3C`, Convolve L/R `#3690CC`. The selected segment, the curve and the mode's knobs take that colour.
- **Edit** dropdown on the right: Points (smooth), Points (flat), Freehand.
- **Graph**: log frequency 20 Hz–20 kHz; dB +24 to −90 (both ends editable by double-clicking the axis labels). The input spectrum is a grey fill, the output spectrum an orange line, and the user curve is in the mode colour with a light fill. In Compressor mode, gain reduction is shown in yellow. Legend top-right.
  - Points modes: click the curve to add a point, drag to move, Alt-click to delete, double-click to reset to 0 dB, right-click to type freq/gain. Shift-drag = vertical only. Ctrl/Cmd-drag = move the whole curve. Freehand: draw with the mouse.
- **Left card** (changes per mode):
  - Subtract: **Noise profile** with a **Learn** button. While learning it shows a progress bar and seconds, and it stops by itself after the set time. When finished it shows "Profile: 2.0 s captured" with Clear and Re-learn.
  - EQ: Reset curve + preset curves (flat, tilt, de-mud). Gate/Compressor: Reset curve.
  - Convolve L/R: an explanation line and an input-routing hint.
- **Mode controls card**: Subtract: **Reduction, Offset (bipolar), Smoothing, Release** knobs. Gate: **Floor, Attack, Release**. Compressor: **Ratio, Attack, Release**. EQ: none (the curve is the control). Always shown: **FFT size** dropdown and a **Reduce artifacts** checkbox.
- **Right column**: Out meter + **Gain** fader (teal).
- **Footer**: Show analysis, Low latency, Sidechain detector (Gate/Compressor/Subtract), Listen to removed noise (Subtract) / Listen to gain-reduced part (Gate, Compressor).

### P4.2 Parameters

| Parameter | Range / default |
|---|---|
| Mode | EQ / Gate / Compressor / Subtract / Convolve L/R, default EQ |
| Edit mode | Points (smooth) / Points (flat) / Freehand, default Points (smooth) |
| FFT size | 128–32 768, default 4096 |
| Low latency | off. On = smaller analysis hop/overlap scheme (see P4.3) |
| Reduce artifacts | on (FIR mode as in the original, applied with proper windowing) |
| Analysis floor | −90 dB; Show analysis on |
| Gain (output) | −24…+24 dB, default 0 |
| Gate: floor | −inf…0 dB, default −40; Attack 0–200 ms (default 1); Release 0–2000 ms (default 80) |
| Compressor: ratio | 0.5:1…100:1, default 2:1; Attack 0–200 ms (default 5); Release 0–2000 ms (default 80) |
| Subtract: Reduction | 0 to −60 dB max cut, default −18 |
| Subtract: Offset | −20…+20 dB applied to the profile, default 0 |
| Subtract: Smoothing | 0–100 % (time + frequency smoothing of the gain mask), default 35 % |
| Subtract: Release | 10–500 ms, default 80 |
| Subtract: Learn length | 0.5–10 s, default 2 s; the profile is saved in the plugin state |
| Sidechain detector | off |
| Listen (delta) | off |

### P4.3 Known issues with the original and the required fix

| Issue reported by users | Required fix + test |
|---|---|
| **Latency is too high for live/real-time use in Subtract mode.** | Report exact latency (`setLatencySamples`) and show it in the header. **Low latency** mode: same FFT resolution but 75 % overlap with a low-latency synthesis window (or partitioned spectral processing) to cut the delay to about ¼–½ of the FFT size. Document the latency of every FFT size/mode in the manual. Test: the reported latency equals the measured impulse delay in every mode and size. |
| **"Musical noise" / warbling artifacts when subtracting hard; the guide itself warns that raising the profile adds artifacts.** | Replace plain magnitude subtraction with a **Wiener-style gain mask**: time smoothing (attack/release per bin), frequency smoothing across neighbouring bins (Smoothing knob), and a **maximum-reduction floor** (Reduction knob) so bins are never fully zeroed. WOLA with sqrt-Hann, 75 % overlap. Test: white noise + sine; at −18 dB reduction the residual must have no isolated tonal bursts (measure frame-to-frame variance of the residual spectrum against plain subtraction; it must be clearly lower) and the sine must lose < 0.5 dB. |
| **Noise learning is easy to get wrong**: users must pick Subtract before pressing play, and if "automatically build noise profile" is left on, it keeps learning the music too. | **Learn** captures a fixed length and stops by itself; it can never stay on. The profile is stored in the plugin state and survives project reload. Clear and Re-learn are explicit. Test: the profile after reload is bit-identical. |
| **Attack/release in Gate and Compressor modes depend on FFT size** (bigger FFT = slower), so changing size changes the sound. | Explicit Attack and Release per bin, defined in ms and independent of FFT size and hop. Test: the 63 % release time is within 10 % of the set value at FFT 1024 and 8192. |
| **Requests for sidechain in Compressor/Subtract modes** (dynamic, frequency-dependent ducking). | Sidechain detector option: per-bin thresholds compare against the sidechain spectrum. Test: a sidechain signal at 1 kHz reduces only bins near 1 kHz in the main signal. |
| **The analysis display is only approximate**, by the guide's own admission. | Same calibrated analyzer as Fancy EQ (§5.1). Test: a −12 dBFS sine reads −12 dB ± 0.5. |
| "ReaFir 0.x compatibility mode (low quality)" exists only for old projects. | Drop it. |
| **Changing FFT size or mode while playing glitches.** | Rebuild buffers off the audio thread and crossfade, like Fancy Verb (§5.3). Report the latency change to the host. |

## P5. Acceptance

Same as §7 of `BUILD_PROMPT.md`, plus: the CI screenshot of each plugin is compared against its `reference/phase2/` mockup, and every P2.3 / P3.3 / P4.3 test is in the Catch2 suite and green in CI.
