# Phase 2.0.1: Fancy Gate (ReaGate-inspired)

This extends `docs/BUILD_PROMPT.md` and `docs/BUILD_PROMPT_PHASE2.md`. **Every rule there still applies**: cloud builds only, the design system and shared `fancy-core` components, the real-time rules (§4), the acceptance checks (§7), the CC BY attribution, and "ask before changing the parameter list or layout".

## 0. When

- Build Fancy Gate **after Phase 2 and before Phase 2.1**. Phase 2.1's workflow item (W) then covers Fancy Gate as well. Treat it as a **large** plugin there (full header).
- Write `docs/gate.md` (the design note) first, as in Phase 1.
- Reuse from `fancy-core`: Fancy Comp's detector, detector filters, RMS window, "Preview filter" path, sidechain bus handling, the Threshold-over-meter fader component, the reduction meter, and Wet/Dry faders. Don't fork them.
- Add Fancy Gate to `AGENTS.md`, `CLAUDE.md` and `docs/PROGRESS.md`.

| Product name | Inspired by | Main accent |
|---|---|---|
| **Fancy Gate** | ReaGate | Yellow `#F1C315` (same as Fancy FIR's Gate mode); detector filters blue; global controls teal |

## 1. Reference mockup

`reference/phase2_0_1/15__gate_mockup.png` (HTML source in `design/phase2_0_1-mockup-src/`). It was drawn in the same design system as the Phase 2 mockups, not by the original designer. It defines layout and control types; `fancy-core` defines exact styling. Window ≈ 740 × 405 at 100 %.

## 2. Layout

- **Header**: standard large header. I/O pill "4 in 2 out" (stereo main + stereo sidechain).
- **Row under the header**: **Detector input** dropdown on the left (Main input L+R, Main L, Main R, Sidechain L+R, Sidechain L, Sidechain R). Next to it, a **Lookahead** checkbox, labelled "Lookahead (+50 ms latency)", **off by default**. On the right, a **Mode** switch: `Gate | Duck`.
  - If a Sidechain source is chosen but the sidechain bus is silent for more than 2 s while the main input has signal, show a small warning next to the dropdown: "No sidechain signal: check track routing (REAPER: pins 3/4)".
- **Left column**:
  - **Threshold**: a vertical fader over the stereo input meter (same component as Fancy Comp). The **open** threshold is the fader cap (solid yellow line). The **close** threshold is a dashed yellow line at Threshold + Hysteresis. Read-outs below: "−38.0 dB" and "close −44.0".
  - **Gain** meter: gain reduction from the top, in yellow, with a read-out. Below it, a **state light**: "open" (yellow, glowing), "hold" (yellow, dim), "closed" (grey).
- **History display** (top of the centre): about 3 s of scrolling input level (grey fill) with the gate's gain as a yellow line, and the open (solid) and close (dashed) thresholds as horizontal lines. It refreshes at 30 fps from a lock-free FIFO. It's display only, but **clicking or dragging on it sets the threshold** at that height.
- **Envelope card** (yellow knobs): **Pre-open, Attack, Hold, Release, Hysteresis** (bipolar). While Lookahead is off, the Pre-open knob is dimmed to 50 %, reads "off", and can't be turned.
  - **Hover hint:** hovering over the dimmed Pre-open knob for about 300 ms shows a tooltip (standard `fancy-core` tooltip style): **"Pre-open needs Lookahead. Tick 'Lookahead (+50 ms latency)' above to use it."**
  - **Click or drag on the dimmed knob:** the knob doesn't move. The same message appears straight away, and the Lookahead checkbox briefly pulses with a yellow outline (about 600 ms) to show where it is.
  - Never switch Lookahead on automatically from the knob: it changes the plugin's latency, so it must stay a deliberate choice.
  - Once Lookahead is on, the hint no longer appears, and the knob gets its normal tooltip.
  - Tests: with Lookahead off, the Pre-open component reports the hint text as its tooltip and ignores value changes from mouse drag, wheel and typed entry; with Lookahead on, it has the normal tooltip and accepts changes. Add the hover and click behaviour to my REAPER checklist.
- **Level card** (yellow knobs): **Range, RMS size**.
- **Detector filter card** (blue knobs): **Lowpass, Highpass**.
- **Right column**: Out meter, then **Wet, Dry, Noise** vertical faders. Wet and Dry are teal; Noise is grey `#909090` because it's a special-purpose level.
- **Footer checkboxes**: Preview filter, Link channels, MIDI on open/close, then a compact **note · channel** dropdown ("A4 · ch 1") and **Velocity from level**. The last two are dimmed (50 %) until MIDI is on.

## 3. Parameters (keep every function the original has)

| Parameter | Range / default | Notes |
|---|---|---|
| Threshold | −90…0 dB, default −40 | Open threshold |
| Hysteresis | −24…+12 dB, default −6 | Close threshold = Threshold + Hysteresis. Negative = closes lower than it opens (stops chatter). Positive (the original's unusual feature) = once open, the gate closes as soon as the level falls below a point *above* the open threshold, which turns it into a fixed-shape envelope trigger. Keep it. |
| Lookahead | **off** (default) / on | Off = **zero latency**, Pre-open disabled. On = reports a constant 50 ms latency and enables Pre-open (see §4) |
| Pre-open | 0–50 ms, default 5 | Only active when Lookahead is on |
| Attack | 0–500 ms, default 0.5 | |
| Hold | 0–2000 ms, default 20 | |
| Release | 1–5000 ms, default 100 | |
| Range | −inf…0 dB, default −inf | **New.** How far the gate turns the signal down when closed (−inf = full gate; −10 dB = gentle). In Duck mode: how far it ducks when open |
| RMS size | 0–250 ms, default 0 (peak) | |
| Detector input | see §2 | |
| Lowpass / Highpass (detector) | 20 Hz–22 kHz / off; HP 20 Hz–5 kHz / off | Detector only, never the audio path |
| Preview filter | off | Outputs the filtered detector signal so you can hear what the gate is listening to |
| Mode | Gate / Duck, default Gate | Replaces the original "Invert gate (duck)" checkbox (§5) |
| Link channels | on | Off = each channel gated by its own detector |
| Wet | −inf…+12 dB, default 0 dB | Gated signal level |
| Dry | −inf…+12 dB, default −inf | Ungated signal mixed in parallel |
| Noise | −inf…0 dBFS, default −inf | Adds white noise **only while the gate is open**, scaled by the gate gain (the original's noise mix, e.g. for snare sizzle) |
| MIDI on open/close | off | Note-on when the gate opens, note-off when it closes, on the VST3 event output bus |
| MIDI note / channel | 0–127 (default 69 = A4) / 1–16 (default 1) | As in the original |
| Velocity from level | off | On: velocity follows the detector's peak level over the **velocity window** after the gate opens (−60…0 dBFS mapped to 1–127). For drum triggering. See §4 for timing |
| Velocity window | 1–5 ms, default 2 ms | Set in the note/channel dropdown's pop-up, next to note and channel |

Factory presets: Vocal clean-up, Kick tight, **Snare (with spill)** and **Tom (with spill)** (these two turn Lookahead on, with Pre-open 2 ms, and say so in the preset name: "… · lookahead"), Duck under kick (Duck, sidechain), Drum trigger to MIDI, Gentle expander (Range −12 dB).

## 4. DSP

- **Detector**: main or sidechain, through the HP/LP filters, then peak (RMS size 0) or RMS, in the log domain. Stereo-linked = max of L/R.
- **State machine** with states closed → opening → open → hold → closing. It opens when level ≥ Threshold, starts Hold when level < Threshold + Hysteresis, and starts closing when Hold has run out. A re-trigger during hold or closing goes straight back to opening, from the *current* gain, without jumping. **Re-arming:** a re-trigger needs the level to have dropped below Threshold since the last opening. Without this, positive Hysteresis (close threshold above open threshold) would re-trigger on every sample and never close. Test: with Hysteresis +6 dB, a steady tone 3 dB above Threshold produces exactly one attack-hold-release envelope, not a stuck-open gate.
- **Gain curve**: smooth (raised-cosine) attack and release shapes between 0 dB and Range, computed in dB, so the result is click-free.
  - **Attack 0 ms** still uses a minimum 32-sample raised-cosine fade (or 0.5 ms, whichever is shorter), so opening never makes a click. Document it.
- **Lookahead off (default)**: no delay anywhere; report 0 latency. The detector and the gain are applied to the same sample, so the gate opens at the first sample over threshold (plus the click-free minimum fade).
- **Lookahead on**: delay the audio path and detect on the undelayed signal. To keep REAPER's latency compensation stable, report a **constant 50 ms** while Lookahead is on, and pad the audio delay so total latency stays 50 ms whatever Pre-open is set to. Turning Lookahead on or off changes the reported latency (REAPER re-compensates), so do it as a deliberate setting, not something automation can flip: Lookahead is **not automatable**. Pre-open changes never change the reported latency.
- **Duck mode**: same detector and state machine, but the gain is applied inverted: when "open", the signal is turned **down** by Range; when "closed", it passes at 0 dB.
- **Noise**: a white-noise generator with independent L/R, multiplied by the current gate gain (0 when closed) and the Noise level.
- **MIDI**: sample-accurate note events at the sample where the state changes, compensated for the reported latency so notes line up with the audio REAPER plays back.
- **Velocity timing.** Don't take velocity from the level at the moment of opening: by definition that level is just over the threshold for every hit, so soft and hard hits would get nearly the same velocity. Velocity must come from the peak **after** opening, over the velocity window. So:
  - Lookahead on and Pre-open ≥ velocity window: the window is already in the past when the note is due, so the note is **on time** and velocity is exact.
  - Lookahead off (the default), or Pre-open shorter than the window: the note-on is sent **late by (window − Pre-open)**, 2 ms by default, and velocity is exact.
  - When Velocity from level is on and the note will be late, the checkbox's tooltip says: "Notes are sent 2 ms late to measure how hard each hit is. Turn on Lookahead for on-time notes." (with the actual delay).
  - Velocity from level off: notes are always on time, with velocity 100.
- Denormal protection; no allocation on the audio thread; all controls smoothed (§4.2).

## 5. Known issues with the original, and the required fix

These come from user reports (forum, Reddit and Facebook search excerpts, and a Sound On Sound article), plus the official Effects Guide.

| Issue reported by users | Required fix + test |
|---|---|
| **No floor/range control.** Users ask whether "adding a little of the dry signal is the same as having a floor control", and use Dry as a workaround. Dry mixes the ungated signal in parallel, which isn't the same thing (it isn't phase-coherent with lookahead, and it changes the open level too). | Dedicated **Range** control (§3). Test: Range −20 dB, gate closed → output exactly 20.0 ± 0.1 dB below input; open → 0 dB. |
| **Hysteresis is misunderstood.** Users debate whether it sets separate open and close thresholds; the positive range is unusual. | Make it visible: the close threshold is drawn on the Threshold meter and the history display, and shown as a read-out ("close −44.0"). Tooltip explains negative vs positive. Test: Threshold −30, Hysteresis −6 → opens at ≥ −30 dB, stays open down to −36 dB, closes below −36 dB (±0.1 dB). |
| **Duck ("Invert gate") mode is confusing or seems broken.** Users can't get inverse ducking to work, and one reports that a set-up which used to work (invert on, wet and dry at unity, threshold down) stopped working. With wet and dry both at unity, the ducked part is filled back in by Dry. | Duck is an explicit **Mode**, and its depth is **Range**, independent of Wet/Dry. Defaults in Duck mode: Wet 0 dB, Dry −inf. Test: Duck, Range −12 dB, sidechain above threshold → main signal −12.0 ± 0.1 dB; sidechain silent → 0 dB. |
| **Sidechain (Aux input) doesn't seem to work.** Users report ReaGate not detecting audio from the auxiliary input; the usual cause is REAPER routing (track channels and plugin pins 3/4). | Detector dropdown names the sources plainly; the "No sidechain signal" warning (§2); a short REAPER sidechain how-to in the manual. Test: with Sidechain L+R selected and the sidechain bus silent for 2 s while the main input has signal, the warning flag is set; it clears within 0.5 s of sidechain signal arriving. |
| **Clicks when the gate opens with very fast attack.** (One Reddit "click in a closed gate" report turned out to be the user's audio interface, not ReaGate, so it isn't counted. Opening clicks at 0 ms attack are a general gate problem.) | Minimum raised-cosine fade (§4). Test: a 1 kHz sine at −6 dBFS gated open at the worst phase → no sample-to-sample step larger than the sine's own maximum step × 1.5. |
| **Lookahead adds latency, and changing it during playback upsets compensation.** It also matters for live use and recording, where any latency hurts. | **Zero latency by default**; Lookahead is an explicit, non-automatable switch with a constant 50 ms when on (§4). Tests: Lookahead off → reported latency 0 and an impulse comes out at sample 0; Lookahead on → reported latency identical for Pre-open 0, 10 and 50 ms, and the output is time-aligned with the input in all three. |
| **People use ReaGate as a drum or guitar trigger for MIDI**, and want velocity to follow how hard the hit was. | Velocity from level (§3), sample-accurate and latency-compensated MIDI. Tests: (1) three hits at −24, −12 and −3 dBFS with the same rise time → velocities rise in that order, match the mapping within ±2, and are at least 20 apart, in both Lookahead modes. (2) Lookahead on, Pre-open ≥ window → note-on within 1 sample of the gate-open sample in the latency-compensated timeline. (3) Lookahead off → note-on exactly one velocity window after the gate-open sample (±1 sample). (4) Velocity from level off → note-on within 1 sample of gate open, velocity 100. |
| **Chattering on signals hovering around the threshold.** | Hysteresis default −6 dB (rather than 0) and Hold default 20 ms. Test: a signal wobbling ±2 dB around the threshold at 20 Hz → at most one open/close cycle per second with the defaults. |

## 6. Acceptance

§7 of `BUILD_PROMPT.md` (green CI, pluginval, screenshots compared with `15__gate_mockup.png`, a REAPER checklist for me that includes a sidechain test and a MIDI-trigger test), plus every §5 test in Catch2 and green in CI. CPU target: < 0.5 % with filters and RMS on, Lookahead off, measured by the CPU method in `BUILD_PROMPT_PHASE2_1.md` §0.4.
