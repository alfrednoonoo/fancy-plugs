# Fancy TS: design note

**Approved by the user on 22 November 2026 with every recommended option (A).** Phase 4 item 3 (`docs/enhancements.md`), a transient shaper. Name Fancy TS (the user renamed Fancy Transient on 22 November 2026), bundle ID `com.fancyplugs.ts`. Layout approved as mockup 50 (`reference/phase4/50__transient_mockup.png`, decisions 1 to 5 of `docs/research/new-plugins-features.md`, all A). Modelled first: `docs/research/ts-model.py`. Ideas only from the plugins surveyed; no names, graphics or sounds.

## What it is

It makes the start of each hit louder or quieter (Attack) and the ring after it longer or shorter (Sustain), whatever the level, so a quiet snare and a loud one are shaped the same. Accent orange `#E77E23` (shared with Sature), Phase 2 size class, 760 × 465 with the title strip.

## Engine

Per sample, in dB, on the detector (Main L+R, Sidechain, Mid or Side; Highpass and Lowpass on it, Link channels or one detector per channel):

1. **Level:** a peak envelope of the detector, up at once and down over 10 ms, so the waveform's own ripple isn't read as a transient (the model's first version read a steady sine as +4 dB of transients).
2. **Transient amount** `a`: a fast follower (0.1 ms up, 20 ms down) minus a slow one (Speed up, 20 ms down), less Dead band, over a span of 12 dB at Sensitivity 0 % down to 3 dB at 100 %, held to 0 to 1.
3. **Sustain amount** `s`: how far the level now sits under the hit's peak (a peak memory falling 6 dB a second), over the same span, times a window that runs from 1 at the last hit to 0 after Length, in Contour's shape: **Sharp** fast then slow, **Medium** straight, **Smooth** slow then fast.
4. **Gain:** `Attack × a + Sustain × s − Pump × 12 dB × p × (1 − a)`, where `p` is charged by each hit and falls over Length: Pump dips the level just after the attack.
5. Below −90 dBFS on the detector both amounts fade to 0, so silence and noise floors aren't shaped.
6. The gain is applied to the (Lookahead-delayed) audio, then **Clip output** (Limit's soft knee at 0 dBFS, oversampled), Wet/Dry and Output. **Channels** Mid only or Side only shape just that part.

What the model found (48 kHz, Speed 8 ms, Length 180 ms, Sensitivity 60 %, a kick with a 5 ms click):

| | |
|---|---|
| Attack +6: largest gain in the first 10 ms | +6.0 dB; 0.0 dB by 100 ms |
| Sustain −6: at 2 ms, at 100 ms | 0.0 dB, −4.9 dB |
| The same kick at −6 and −30 dBFS | within 1.2 dB (only where the quieter tail meets the −90 dBFS floor) |
| A steady sine, Attack and Sustain +12 | 0.00 dB once settled |
| Sustain −6 at 150 ms: Sharp, Medium, Smooth | −0.3, −3.2, −3.3 dB |
| Pump 100 % | −9.6 dB at 67 ms after the hit, −3.5 dB at 250 ms |

## Parameters

All new, so no migration; `stateVersion` 1.

| ID | Name | Range | Default | Unit, skew |
|---|---|---|---|---|
| `attack` | Attack | −24 to +24 | 0 | dB, linear, bipolar |
| `speed` | Speed | 1 to 50 | 8 | ms, log |
| `sustain` | Sustain | −24 to +24 | 0 | dB, linear, bipolar |
| `length` | Length | 20 to 1000 | 180 | ms, log |
| `pump` | Pump | 0 to 100 | 0 | % |
| `sensitivity` | Sensitivity | 0 to 100 | 60 | % |
| `highpass` | Highpass | Off, 20 to 2000 | Off | Hz, log |
| `contour` | Contour | Sharp, Medium, Smooth | Medium | choice |
| `detector` | Detector | Main L+R, Sidechain, Mid, Side | Main L+R | choice |
| `clip` | Clip output | off, on | off | toggle |
| `link` | Link channels | off, on | on | toggle |
| `wet`, `dry` | Wet, Dry | −inf, −60 to +12 | 0, −inf | dB (the suite's faders) |
| `lookahead` | Lookahead | Off, 1, 2, 5 ms | Off | choice, not automatable |
| `oversampling` | Oversampling | Off, 2x, 4x | 2x | choice, not automatable |
| `channels` | Channels | Stereo, Mid only, Side only | Stereo | choice |
| `lowpass` | Lowpass | 200 to 20000, Off | Off | Hz, log |
| `deadband` | Dead band | 0 to 6 | 1 | dB |
| `output` | Output | −24 to +12 | 0 | dB |
| `bypass` | Bypass | | off | VST3 soft bypass |

At Attack 0, Sustain 0 and Pump 0 the gain is exactly 0 dB, so the defaults pass the audio through unchanged (bit-exact with Clip off, Lookahead Off and Oversampling unused).

## Latency

Lookahead's time, plus the oversampler's when Clip output is on and Oversampling isn't Off, reported with `setLatencySamples` and the dry path delayed to match. See decision 2.

## Real-time rules (§4)

No allocation or locks in `process`; every continuous parameter smoothed over 20 ms; `ScopedNoDenormals`; 44.1 to 384 kHz and blocks of 1 to 8192; soft bypass with a crossfade against the delayed dry path; versioned state; automation without a change is bit-identical; mono and stereo with an optional stereo sidechain; factory presets and the preset browser's categories. Listen to change plays processed minus dry (the change itself) and is temporary, never saved (§0.3, `FancyProcessor::setListening`).

## Display

The input's level in grey, the output in orange and the gain applied as a white line (0 dB dashed) over the last 3 s, with each detected hit marked: what Attack and Sustain do is visible as they're turned. Clicking it does nothing (it isn't a threshold).

## Factory presets

Default; Snare snap (Attack +8, Speed 5 ms); Kick punch (Attack +6, Sustain −3, Highpass Off); Room tame (Sustain −8, Length 300 ms); Tighten loop (Attack +3, Sustain −6, Contour Sharp); Guitar pluck (Attack +5, Speed 15 ms); Sustain bloom (Sustain +6, Length 500 ms, Smooth); Pump (Pump 60 %, Length 250 ms). Categories Drums, Mix, Instruments.

## Decisions

- **Decision 1: what Contour does.**
  - **A (R).** The shape of Sustain's window over Length (Sharp, Medium, Smooth), as above: it changes how the tail is treated without changing how long.
  - **B.** Three speed presets for the whole detector (Neutron's idea): less to explain, but it overlaps Speed and Length.
- **Decision 2: Oversampling and latency.**
  - **A (R).** Oversampling only runs while Clip output is on, and the latency follows: Clip off is zero latency. Ticking Clip changes the latency, which REAPER handles; the tooltip says so.
  - **B.** Oversampling runs whenever it's set, so ticking Clip never changes latency, at a constant cost.
- **Decision 3: Sustain's reach.**
  - **A (R).** Length ends Sustain's work after each hit, so a long Sustain boost can't raise the noise between hits.
  - **B.** Sustain acts on any tail, however long after the hit: closer to some hardware, but it lifts room noise and bleed.
- **Decision 4: CPU target.** **A (R):** under Fancy Comp's at its defaults (§0.4 of the Phase 2.1 brief), with Clip off; under 1.5 × with Clip at 4x.

## Tests

1. Defaults: the output equals the input bit for bit (Clip off), at 44.1 to 96 kHz, mono and stereo, blocks 1 to 8192.
2. The model's table: Attack +6 gives +6 ± 0.3 dB in the first 10 ms of the kick and under 0.3 dB by 100 ms; Sustain −6 gives under 0.3 dB at 2 ms and −4.9 ± 0.5 dB at 100 ms.
3. Level independence: the kick at −6 and −30 dBFS within 1.5 dB everywhere above −80 dBFS.
4. A steady sine is untouched once settled (within 0.05 dB), whatever Attack and Sustain.
5. Lookahead 5 ms: the reported latency is 5 ms, the dry path lines up (Wet −inf, Dry 0 dB nulls against the input delayed by it).
6. Clip output: never over 0 dBFS; the aliasing check from Limit's clipper.
7. Detector: a hit on the sidechain shapes the main input; Highpass 200 Hz stops a kick's 60 Hz body from triggering Attack.
8. Automation, state round trip, bypass crossfade and bit-identical automation without change (§4).

## Build order

Approval of this note, then one push: `plugins/ts/` (processor, DSP, editor), the display, presets, tests, the CI screenshot against mockup 50, manual and checklist, then the CPU measurement and the REAPER check.

## As built

First push on 22 November 2026: `plugins/ts/` (`TsDsp`, `TsProcessor`, `TsEditor`), `tests/TsTests.cpp`, the suite's shared checks (host, offline render, pop-ups, presets, Listen, workflow), screenshots `ts` and `ts-edit`.

- The engine is the model's, per sample, with a −90 dBFS floor fade on both amounts. Exactly 0 dB of gain is exactly unity, so the defaults are bit-exact.
- Clip output uses Limit's soft curve; with Oversampling on, JUCE's polyphase IIR at 2x or 4x and then a hard safety clamp at 0 dBFS for the downsampler's overshoot. A change of Lookahead, Clip or Oversampling fades out, swaps and fades back in, and the processor reports the new latency.
- Test 7 is written as a low thump over steady bright noise: the detector is level-independent, so filtering a lone thump only lowers it and its onset still counts. With Highpass 500 Hz the thump sits under the noise and no hit is found.
- Not yet: the CPU measurement (§0.4) and the REAPER check.
