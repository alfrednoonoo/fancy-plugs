# Fancy Chorus: design note

**Approved by the user on 22 November 2026: decisions 1 to 4 all A.** Build not started (user: later). Phase 4 item 4 (`docs/enhancements.md`), a chorus and modulation plugin. Bundle ID `com.fancyplugs.chorus`. Layout approved as mockup 51 (`reference/phase4/51__chorus_mockup.png`, decisions 1 to 5 of `docs/research/new-plugins-features.md`, all A: a new pink accent, 1 to 6 voices, Scatter later). Modelled first: `docs/research/chorus-model.py`. Ideas only from the plugins surveyed; no names, graphics or sounds.

## What it is

Copies of the sound, each slightly delayed and slowly moving in pitch, spread across the stereo field: width, shimmer and thickness. Five modes cover the classic uses. Accent pink `#E0559A`, a new eighth accent (`accentPink` in `fancy::ui::colours`). Phase 2 size class, 760 × 465 with the title strip.

## Engine

Each voice reads the input from its own delay line, `Delay + Depth × 5 ms × lfo`, with cubic (Hermite) interpolation, panned by Width. Voices share one LFO phase clock (so a preset recalls the same sound), each offset by Spread. Feedback sends the summed voices back into the delay lines. Zero latency.

| Mode | Voices' LFOs | What it's for |
|---|---|---|
| **Classic** | one LFO, voices offset evenly | the familiar two-voice chorus |
| **Ensemble** | each voice Rate plus a second LFO at 6.7 × Rate, 7 % as deep | string-machine shimmer; the voices don't beat in step |
| **Dimension** | half the depth, opposite LFOs left and right | subtle width with little wobble |
| **Doubler** | smooth random drift instead of an LFO, at Rate | a second take, no periodic swirl |
| **Flange** | Delay scaled to 0.3 to 10 ms | jet sweeps with Feedback |

Voices (1 to 6) works in every mode; the mode sets the LFO pattern and the delay scale. **Age** (0 to 100 %) adds a bucket-brigade's character to the wet path: a low-pass from 18 kHz down to 5 kHz, gentle saturation and a little hiss (−90 to −70 dBFS). **Low cut** and **High cut** filter the wet path; **Keep bass mono** sums the output's side below Low cut.

What the model found (Depth 100 % = ±5 ms):

| Rate \ Depth | 25 % | 45 % | 75 % | 100 % |
|---|---|---|---|---|
| 0.3 Hz | 4 cents | 7 | 12 | 16 |
| 0.8 Hz | 11 | 20 | 32 | 43 |
| 3 Hz | 40 | 72 | 118 | 156 |

So the defaults (0.8 Hz, 45 %) detune by about 20 cents, a typical chorus, and fast rates reach vibrato. Ensemble's second LFO raises it to 29 cents with a less regular motion. Flange at 1 ms with Feedback ±80 % peaks at +16 and +12 dB, so Feedback stops at ±90 % and Output is the place to trim. Dimension's cross-feed doesn't make the mono sum's combs any shallower (the model's check 3), so the manual doesn't claim it's more mono-compatible, only steadier.

## Parameters

All new; `stateVersion` 1.

| ID | Name | Range | Default | Unit, skew |
|---|---|---|---|---|
| `mode` | Mode | Classic, Ensemble, Dimension, Doubler, Flange | Ensemble | choice |
| `rate` | Rate | 0.05 to 10 | 0.8 | Hz, log |
| `syncRate` | Rate (synced) | 4/1 to 1/32, plain, dotted, triplet | 1/1 | choice, shown while Tempo sync is on |
| `depth` | Depth | 0 to 100 | 45 | % |
| `delay` | Delay | 1 to 30 | 7 | ms, log (Flange: 0.3 to 10) |
| `feedback` | Feedback | −90 to +90 | 0 | %, bipolar |
| `voices` | Voices | 1 to 6 | 3 | integer |
| `spread` | Spread | 0 to 100 | 60 | % (LFO phase spread) |
| `width` | Width | 0 to 100 | 100 | % (pan spread) |
| `lowCut` | Low cut | Off, 20 to 1000 | 150 | Hz, log |
| `highCut` | High cut | 1 k to 20 k, Off | 9 k | Hz, log |
| `age` | Age | 0 to 100 | 25 | % |
| `sync` | Tempo sync | off, on | off | toggle |
| `bassMono` | Keep bass mono | off, on | on | toggle |
| `lfoShape` | LFO shape | Sine, Triangle, Smooth random | Sine | choice |
| `input` | Input | Stereo, Mono | Stereo | choice |
| `lfoPhase` | LFO phase | 0 to 180 | 90 | ° (right against left) |
| `ducking` | Ducking | 0 to 100 | 0 | % |
| `wet`, `dry` | Wet, Dry | −inf, −60 to +12 | −3, 0 | dB |
| `output` | Output | −24 to +12 | 0 | dB |
| `bypass` | Bypass | | off | VST3 soft bypass |

## Real-time rules (§4)

No allocation or locks in `process` (delay lines sized for 40 ms at 384 kHz in `prepare`); every continuous parameter smoothed, Delay over 50 ms so it glides rather than clicks; `ScopedNoDenormals`; the feedback path DC-blocked; 44.1 to 384 kHz and blocks of 1 to 8192; soft bypass; automation without a change is bit-identical; mono and stereo. Listen to wet plays the wet path alone and is temporary (§0.3).

## Display

Each voice's delay over the last 2 s as a line in the accent at falling opacity, and on the right each voice's place in the stereo field. Rate, Depth, Delay, Voices, Spread and Width all show on it as they're turned.

## Factory presets

Default; Classic chorus; Wide pad (Ensemble, 5 voices); Dimension width (Dimension, Depth 25 %); Vocal double (Doubler); Slow flange (Flange, Feedback +60 %); Vibrato (Classic, Rate 5 Hz, Wet 0, Dry −inf); Lo-fi wobble (Classic, Age 80 %, Rate 0.3 Hz). Categories Mix, Vocals, Instruments, Creative.

## Decisions

Chosen by the user on 22 November 2026: 1A, 2A, 3A, 4A. Still open, to settle before coding: what Ducking listens to and ducks (proposed: the wet dips while the input is loud, as in Delay), and whether 3A's switch to 2 voices on a mode change should only apply in presets.

- **Decision 1: Feedback polarity.** Mockup 51 drew a polarity dropdown in the drawer as well as a bipolar Feedback knob.
  - **A (R).** The bipolar knob alone (−90 to +90 %); the drawer's slot shows nothing new. One control for one job.
  - **B.** Keep the dropdown and make the knob 0 to 90 %.
- **Decision 2: Oversampling.** Mockup 51 drew it in the drawer.
  - **A (R).** Leave it out: only Age's gentle saturation is non-linear, and it adds no audible aliasing at its level. Zero latency always.
  - **B.** Off, 2x, 4x for Age, with latency while on.
- **Decision 3: Voices beyond two in Classic and Dimension.**
  - **A (R).** Voices works in every mode; Classic and Dimension default to 2 when chosen.
  - **B.** Classic and Dimension fixed at 2 voices, the knob dimmed.
- **Decision 4: CPU target.** **A (R):** under 1.5 × Fancy Comp at its defaults (3 voices), under 3 × at 6 voices.

## Tests

1. Depth 0, Feedback 0, one voice, Age 0, cuts Off, Wet 0 dB, Dry −inf: the output is the input delayed by Delay, within interpolation error (−100 dB).
2. Detune: a 1 kHz sine at Rate 0.8 Hz, Depth 45 %: the instantaneous frequency swings ±20 ± 3 cents.
3. Voices and Width: with Width 0 the wet output is mono; with Width 100 and 2 voices the voices sit at the edges.
4. Feedback +90 % is stable for 60 s of noise (no growth past +25 dB, no denormals), and DC can't build up.
5. Tempo sync: at 120 BPM, 1/4 is 2 Hz, and the LFO's phase follows the host's position.
6. Keep bass mono: below Low cut the side channel of the output is the dry's.
7. Every mode, rate and block size: no clicks when Delay, Voices or Mode change (largest step under the signal's own).
8. Automation, state round trip, bypass crossfade, bit-identical automation without change (§4).

## Build order

Approval of this note, then one push: `fancy::ui::colours::accentPink`, `plugins/chorus/`, the display, presets, tests, the CI screenshot against mockup 51, manual and checklist, then the CPU measurement and the REAPER check.
