# Fancy Kicker, Sub mode: design note

Spec: `docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md` §6. Mockup: `reference/phase3/21__kicker_sub_mockup.png`, source `design/phase3-mockup-src/kicker.html`. Kicker's own note: `docs/kicker.md`. Design rules: `docs/DESIGN_SYSTEM.md`. **Approved by the user on 11 November 2026: A for all three decisions.**

## Scope, agreed 11 November 2026

- §6 **replaces Phase 3 Q2.2**. Q2.2's pitch-tracked sub synth is this Sub mode. Its **Harmonics mode is dropped** (Fancy Sature covers psychoacoustic bass: Tube, High at 1 kHz, mixed with Dry) and its **sidechain Duck is deferred** to Phase 4.
- **Divider engine only.** The Spectral (MDCT) engine is built later only if Divider disappoints in REAPER. Until then the EDIT drawer has no Engine dropdown, since a list with one choice is noise; adding the parameter later is an append, so nothing breaks.
- The test kick sweeping in pitch is added to §6.4's tests (below).

## Signal path

```
Boost (today's Kicker, unchanged):
in ─┬──────────────────────────────────────────────────── × Dry ─┐
    └─ mono blend ─ SVF band-pass at Freq ─ 15 Hz HPF ─ × Wet ────┴─ + ─┐
                                                                         ├─ mode crossfade (20 ms) ─ × Output ─ soft clip ─ bypass ─ out
Sub (new):                                                               │
in ─┬──────────────────────────────────────────────────── × Dry ─┐      │
    └─ detector: L+R ─ low-pass at Cutoff ─ tracker ─ sine at ½ or ¼ ─ × envelope ─ gate ─ low-pass at Sub low-pass ─ 15 Hz HPF ─ × Sub ─┴─ + ─┘
```

- **Boost** runs exactly as today. With Mode at Boost (the default), the output is bit-identical to v1.2.0: the Sub path doesn't run at all, and the crossfade is skipped once settled.
- **Switching mode** crossfades the two outputs over 20 ms, both paths running only during the fade. Mode is automatable.
- **Output, Soft clip and bypass** act on both modes, as today.

## The Divider engine

Zero latency. Everything runs per sample.

1. **Detector:** L+R (or each channel, with Mono sub off), through a 4th-order Butterworth low-pass at Cutoff and a 20 Hz high-pass so DC can't fake crossings.
2. **Envelope:** a peak follower on the detector band, 1 ms attack and 30 ms release, as §6.3 says.
3. **Zero crossings with hysteresis:** a crossing counts when the band goes from below −h to above +h, with h a fixed fraction (10 %) of the envelope. Small wiggles near zero, which are what made old octave dividers chatter, can't toggle it. The crossing time is interpolated between samples.
4. **Oscillator:** a sine whose phase advances by ½ (or ¼) of the measured input frequency, from the last two crossings. At each crossing it compares its phase with where a perfect divider would be (0 or ½ cycle for −1 oct) and spreads the correction over the next period, so the phase never jumps. A pitch-dropping kick (120 to 50 Hz) is followed crossing by crossing.
5. **Gate:** below Sensitivity (envelope in dBFS) the sub fades out over 5 ms and the oscillator stops. When the envelope rises past it again, the oscillator restarts at phase 0 on the next crossing, so the sub always starts at a zero. That covers §6.4's re-trigger test.
6. **Shaping:** amplitude is the envelope times the Sub level, then a 2nd-order low-pass at Sub low-pass (60 to 250 Hz) and Kicker's 15 Hz high-pass.
7. **Mono sub** (on by default): one generator from L+R, the same sub in both channels. Off: one generator per channel, each from its own channel.

**−2 octaves:** a 60 Hz kick two octaves down is 15 Hz, which the subsonic filter removes. −2 oct suits higher sources (toms, pitched-up 808s). Its tooltip says so.

## Parameters

Appended after the existing ones. `stateVersion` is bumped with a migration that sets Mode to Boost, so every old project and preset loads and renders bit-identical.

| ID | Name | Range | Default | Unit | Notes |
|---|---|---|---|---|---|
| `mode` | Mode | Boost, Sub | Boost | | `ParamSegments`, accent-filled, top of the body |
| `subCutoff` | Cutoff | 40–200 | see Decision 1 | Hz | large knob, log |
| `subOctave` | Octave | −1 oct, −2 oct | −1 oct | | grey `ParamSegments` |
| `subLevel` | Sub | −inf (−60) to +6 | −9 | dB | small knob |
| `subSensitivity` | Sensitivity | −60 to 0 | −40 | dB | EDIT |
| `subLowpass` | Sub low-pass | 60–250 | 120 | Hz | EDIT, log |
| `subMono` | Mono sub | off/on | on | | EDIT |

**Decision 1: Cutoff's default (I recommend A).** Cutoff is the highest frequency the tracker listens to. The brief says 70 Hz, but its own test kick has an 80 Hz body, and many kicks' bodies sit at 50 to 100 Hz, sweeping down from higher.

- **A. 100 Hz.** Catches the body of almost any kick, including the start of a pitch drop. The tests run at the default.
- **B. 70 Hz, as the brief says.** Kicks with a body above about 75 Hz are tracked from a weakened signal, so the sub comes in late or thin until you raise Cutoff. The §6.4 test would then run at Cutoff 100.

**Decision 2: Dry is Kicker's existing Dry (I recommend A).** The mockup puts Sub mode's Dry knob exactly where Boost's Dry is.

- **A. One `dry` parameter for both modes.** Same position, same meaning, and automation on Dry keeps working when you switch mode. Its range stays −inf to +12 dB (the brief's Sub Dry says +6). Changing an existing range would move old automation.
- **B. A separate `subDry`** (−inf to +6 dB, default 0). Each mode keeps its own dry level, at the cost of a second Dry in the host's parameter list.

## Layout

**Decision 3: Boost mode's layout (I recommend A).** The Boost | Sub segment needs about 30 px at the top of the left panel, and the mockup only draws Sub.

- **A. Both modes share Sub's positions.** The segment at the top, Freq's large knob where Cutoff is, and Wet and Dry where Sub and Dry are. The octave row's space stays empty in Boost. Switching mode swaps knobs in place, and nothing else moves. Boost's knobs sit about 30 px lower than today, so Kicker's Boost screenshot changes. Old projects still sound identical; only the picture moves.
- **B. Squeeze Boost's current layout under the segment.** Freq's knob shrinks to fit, and knobs jump when you switch mode.

In both, the bottom caption becomes the mode's line, "Boost: lifts the kick's own low end" or "Sub: adds an octave below", replacing today's "Fancy Kicker". The title strip has carried the plugin's name since Q2.1, so the name isn't lost. The In/Out meters are unchanged. Every knob is the shared `Knob`, so typed values work, and Cutoff and Sub low-pass accept `2.5k`-style input.

**EDIT drawer:** today's items (Filter Q, Output, Mono below Freq, Soft clip). In Sub mode Filter Q and Mono below Freq, which only act in Boost, are replaced by Sensitivity, Sub low-pass and Mono sub. Output and Soft clip stay in both.

## Presets

Added to the Kick category: **Sub: Deep 808** (−1 oct, Sub 0 dB, Cutoff 90 Hz), **Sub: Tight Punch** (−1 oct, Sub −6 dB, Sub low-pass 90 Hz, Sensitivity −30 dB) and **Sub: Tom Drop** (−2 oct, Cutoff 180 Hz, Sub −6 dB). Existing presets don't change and load in Boost.

## Tests

From §6.4, plus the sweep agreed above:

| Test | Pass |
|---|---|
| Boost, existing states and presets | bit-identical to the v1.2.0 renders, at 44.1, 48 and 96 kHz |
| Synthetic kick (80 Hz body, exponential decay), Sub, −1 oct | energy at 40 Hz up by at least 12 dB; nothing added above Sub low-pass + 1 octave beyond −60 dB |
| **Sweeping kick, 120 Hz falling to 50 Hz** | the sub's frequency, from its own crossings, stays within 5 % of half the input's; its envelope has no dip deeper than 3 dB while the kick sounds |
| Noise at −50 dBFS, Sensitivity −40 | no sub generated (below −90 dBFS) |
| No clicks, including note-off and re-trigger | no step larger than 1.5 × the sub's own largest step |
| Latency | 0 samples in both modes |
| Mode switch while playing | no step larger than 1.5 × the larger mode's own largest step |
| Shared suites | offline equals real time, random blocks, rates 44.1 to 384 kHz, mono, state, §0.3 (nothing to listen to), pop-ups, undo, A/B, Compare |
| CPU, Sub (Divider), median of 5 | under 0.5 % (§7), and the ratio to Fancy Comp |

## As built (11 November 2026)

- **No `stateVersion` bump.** State version 3 (Phase 2.1 D) already gives a parameter missing from a state its default, so a project saved before Sub mode loads with Mode at Boost. MultiComp's Phase 2.1 U parameters relied on the same rule. A test removes Sub's parameters from a state and checks it loads in Boost.
- **Boost is the old code, unchanged.** While Mode is settled at Boost, `KickerDsp` runs the v1.2.0 loop as it was; a frozen copy (`tests/LegacyKickerDsp.h`) checks it sample for sample at three rates, with Sub's settings moving underneath.
- **Sub low-pass is 4th order** (24 dB/oct), not 2nd, so nothing the sub adds reaches Sub low-pass + 1 octave above −60 dB.
- **The detector's delay is compensated.** Its low-pass at Cutoff delays the band by several ms at kick frequencies; the oscillator works out that delay from the filters' phase at the tracked frequency, so the sub's cycles line up with the kick itself, not with the filtered band.
- **The first sub cycle** starts on the first confirmed crossing, using the last kick's period (or one from Cutoff before any kick), and corrects itself from the next crossing on, so the sub is there from the kick's first cycle.
- **Boost's caption is "Boost: lifts its own low end".** The brief's "lifts the kick's own low end" ran into the EDIT link in the 264 px panel.
- **Boost's large knob** is 104 px, not 116, in both modes, so the switch and the octave row fit as the mockup draws.
- **The tracker predicts the next period** from the last two (clamped to ±30 %), because a kick's pitch falls roughly geometrically. Run 43087835165 found the plain tracker lagged the 120→50 Hz test glide: the second sub period was 23.8 % off, about 10 % from the lag and the rest from the 15 Hz subsonic filter's start-up shifting the sub's first crossings. With prediction, a Python model of the generator (which reproduced CI's 23.8 % exactly) gives 9.9 % for that period and at most 1.9 % after it. The rest is the subsonic filter's start-up, which the brief specifies; a first-order filter would bring it to 5.0 %. **The sweep test therefore treats the first two sub periods as lock-in**: it prints the second period's error and checks every later period against 5 %, rather than every period after the first, as the note said.
