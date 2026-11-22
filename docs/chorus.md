# Fancy Chorus: design note

**Approved by the user on 22 November 2026: decisions 1 to 4 all A.** Built from 22 November 2026 at the user's word; see "As built". Phase 4 item 4 (`docs/enhancements.md`), a chorus and modulation plugin. Bundle ID `com.fancyplugs.chorus`. Layout approved as mockup 51 (`reference/phase4/51__chorus_mockup.png`, decisions 1 to 5 of `docs/research/new-plugins-features.md`, all A: a new pink accent, 1 to 6 voices, Scatter later). Modelled first: `docs/research/chorus-model.py`. Ideas only from the plugins surveyed; no names, graphics or sounds.

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

Default; Classic chorus (2 voices); Wide pad (Ensemble, 5 voices); Dimension width (Dimension, 2 voices, Depth 25 %); Vocal double (Doubler); Slow flange (Flange, Feedback +60 %); Vibrato (Classic, Rate 5 Hz, Wet 0, Dry −inf); Lo-fi wobble (Classic, Age 80 %, Rate 0.3 Hz). Categories Mix, Vocals, Instruments, Creative.

## Decisions

Chosen by the user on 22 November 2026: 1A, 2A, 3A, 4A, and the two follow-up points below (both as recommended, 22 November 2026).

- **Ducking** stays one knob. The wet path dips while the input is loud, as in Delay: the detector is the input's peak level (1 ms attack), the threshold is fixed at −24 dB and the release at 250 ms (Delay's defaults). Ducking 100 % turns the wet down by up to 24 dB (Delay's largest Amount), scaled linearly in dB; 0 % is bit-exact with no ducking.
- **3A's "2 voices" lives in the presets only.** Changing Mode never writes Voices; the Classic chorus and Dimension width factory presets store Voices 2. No parameter changes another, so automation and saved projects load the same whatever order their values arrive in.

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
7. Every mode, rate and block size: no clicks when Delay, Voices or Mode change (largest step under the signal's own). Changing Mode leaves Voices as it was.
8. Ducking: at 100 % the wet drops by 24 ± 1 dB while the input is above −24 dB and is back within 1 dB about 250 ms after it stops; 0 % is bit-exact.
9. Automation, state round trip, bypass crossfade, bit-identical automation without change (§4).

## Build order

Approval of this note, then one push: `fancy::ui::colours::accentPink`, `plugins/chorus/`, the display, presets, tests, the CI screenshot against mockup 51, manual and checklist, then the CPU measurement and the REAPER check.

## As built

First push on 22 November 2026: `fancy::ui::colours::accentPink`, `plugins/chorus/` (`ChorusDsp`, `ChorusProcessor`, `ChorusEditor`), `tests/ChorusTests.cpp` (tests 1 to 8 and the CPU measurement), the suite's shared checks (host, offline render, pop-ups, presets, Listen, workflow), screenshots `chorus` and `chorus-edit`, the manual and the checklist. Pre-push: `docs/research/chorus-prepush.py`, a per-sample port of the DSP run against tests 2, 6, 7 and 8.

- One delay line per channel, sized for 30 ms plus Ensemble's full sweep at 384 kHz; every voice reads both with Hermite interpolation, never closer than 3 samples, so feedback only uses samples already written. Feedback is the voices' gain-weighted average, DC-blocked at 5 Hz, with a ±4 guard that |Feedback| ≤ 90 % never reaches.
- Each voice's gain, stereo place and LFO offset (Spread × v / Voices) glide over 30 to 50 ms. The pre-push model found that without the offset glide a change of Voices made every voice's delay jump (a click 45 times the tone's own largest step); with it the largest step is 1.7 times.
- Mode, LFO shape and Tempo sync fade the wet out over about 5 ms, switch, and fade back in; Delay jumps to its new value while the wet is silent.
- Flange maps Delay's 1 to 30 ms logarithmically onto 0.3 to 10 ms and sweeps ±Depth × that delay; the knob shows the delay in use. Every mode's sweep is held under 95 % of its centre delay, so short delays sweep less rather than flatten at zero.
- Width narrows each voice (its side × Width × (1 − |place|)) and pans it with an equal-power law, unity at the centre, so one voice at the centre is the input delayed (test 1). The voices sum with 1/√Voices.
- Smooth random is a smoothstep between hashed values per LFO cycle, so it is the same on every playback and follows Tempo sync's position. Age is tanh saturation (drive 1 to 4), a low-pass from 18 to 5 kHz and hiss from −90 to −70 dBFS.
- Ducking: the input's peak (30 ms release) against −24 dBFS; 1 ms attack, and a release that falls 24 dB in 250 ms, linear in dB. At 0 % the gain is exactly 1.
- Presets as listed, with two additions kept by the user (22 November 2026): Vibrato has one voice (three would be a chorus) and Slow flange a Rate of 0.2 Hz.
- The LFOs are worked out every 8 samples and followed in straight lines between, on the DSP's own clock (so any block size gives the same output); each segment starts where the last ended. Pan laws, Age's values, Flange's delay map and the filter coefficients are worked out only when what they depend on moves, and Age uses Fancy Delay's fast tanh.
- CPU (§0.4, median of five, ratio to Fancy Comp), by run: 45040470828 2.35 x at the defaults and 3.70 x at 6 voices; 45045046397 (cached values) 1.73 x and 2.76 x; 45048863820 (a cast floor, no gain) 1.77 x and 2.86 x; **45052618864 (LFO segments) 1.15 x and 1.48 x**, both under target (1.5 x and 3 x). Every Chorus test and the offline render (bit-identical at random block sizes) passed in each.
- REAPER check passed by the user on 22 November 2026.
