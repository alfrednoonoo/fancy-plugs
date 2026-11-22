# Fancy Verb: design note

## Signal path

```
                ┌─ pre-delay ─ convolver A ─┐ equal-power
in ─┬─ (sum if ─┤                           ├─ crossfade ─ Center/Width ─ × Wet ─┐
    │  mono IR) └─ pre-delay ─ convolver B ─┘  on a swap                         ├─ + ─ × Output ─ bypass ─ out
    └─ latency delay ─ × Dry ────────────────────────────────────────────────────┘
```

Only one convolver runs unless a new impulse is being crossfaded in.

The modules never touch live audio. They build the impulse response on a worker thread, in rack order:

```
empty buffer ─ module 1 ─ module 2 ─ … ─ handle trim ─ partition FFTs ─ atomic swap
```

File, Echo generator and Reverb generator add to what's already in the buffer. The other modules process it. The chain is capped at 60 s.

## Convolver (§5.3)

All sizes are in samples, so latency is the same on every load and at every sample rate. A stage with block size N needs its first IR sample at least N behind the output if it runs on the audio thread, and 2N if it runs on the worker: one block to fill and one to compute.

| Zero latency stage | Runs on | IR range | Partitions |
|---|---|---|---|
| Direct FIR | audio | 0–128 | 128 taps |
| N = 128 | audio | 128–512 | 3 |
| N = 512 | audio | 512–4096 | 7 |
| N = 2048 | worker | 4096–16384 | 6 |
| N = 8192 | worker | 16384–65536 | 6 |
| N = 32768 | worker | 65536–end | as needed |

| Low CPU stage | Runs on | IR range | Partitions |
|---|---|---|---|
| N = 2048 | worker | 0–12288 | 6 |
| N = 8192 | worker | 12288–61440 | 6 |
| N = 32768 | worker | 61440–end | as needed |

Low CPU reports 4096 samples of latency, at every sample rate. Zero latency reports 0. With Max FFT set to 16384, the N = 8192 stage takes the rest of the IR.

**FFT:** each stage packs the two channels into one complex FFT as `L + iR` and separates them, so a stereo block costs one forward and one inverse transform. I use `juce::dsp::FFT::perform` because its real-only transforms allocate from the heap above 256 kB, and a block can reach the audio thread. `perform` takes a spin lock, which is never contended because a stage is only ever run by one thread at a time.

**Scheduling:** a stage is `idle → posted → running → done`. At a block boundary the audio thread collects the previous job and posts the next. The worker runs the posted job with the nearest deadline, which is always the smallest block. It sleeps on `std::atomic::wait` and is woken by `notify_one`, which needs no lock on the audio thread.

If a job hasn't started when its deadline arrives, the audio thread takes it over and runs it. If the worker is part way through it, the audio thread spins, for at most 200 µs in real time and for as long as needed in an offline render, so a render is bit-exact and repeatable. A real-time job that's still late plays silence in that stage for one block and adds to a counter shown in SETUP.

**Swap:** the build worker makes a new convolver with its own worker thread, and passes it to the audio thread through an atomic pointer. The audio thread crossfades equal-power over 64 ms, then returns the old one through a second atomic pointer so it's freed off the audio thread. In an offline render the audio thread waits for the build, so a render never starts on a stale IR.

**Bursts:** `reset()` clears every buffer. The wet path fades in over 20 ms after a reset, and when the first IR arrives with no old IR to crossfade from.

## Files and resampling

| Requirement | How it's handled |
|---|---|
| Sample-rate mismatch | Offline windowed-sinc resampling: double precision, Kaiser window (β 12), 64 zero crossings each side, cutoff 0.94 × the lower Nyquist |
| SRC gain | Dropdown with no SRC-gain or SRC-gain, each with 0, -6, -12 or -18 dB of trim. SRC-gain multiplies by file rate ÷ session rate, because a resampled IR has more samples per second and would otherwise convolve louder. That keeps the wet level the same as the file at its own rate |
| Multichannel files | The first two channels are used |
| Formats | Everything `registerBasicFormats` reads: WAV, AIFF, FLAC, Ogg and others |

**IR storage:** a File module saves its absolute path, a 64-bit FNV-1a hash of the file, and an embedded 24-bit FLAC copy when the SETUP option allows it. The real sample rate is saved beside the FLAC, so unusual rates still work. On load it tries embedded, absolute path, then the user's IR folder searched by file name with a matching hash.

**Project-relative paths:** §5.3 says "if the host provides it". VST3 gives a plugin no project path, in REAPER or otherwise, so this step doesn't exist here. The embedded copy covers a moved project.

If every source fails, the card shows `IR missing – Relink…`, and the info string says so too.

## Parameters

Host parameters:

| ID | Name | Range | Default | Unit | Skew | Source |
|---|---|---|---|---|---|---|
| `predelay` | Pre-delay | 0–500 | 0 | ms | linear | §5.3 |
| `center` | Center | -100–+100 | 0 (shows Center) | L/R | linear | §3.5 |
| `width` | Width | 0–100 | 100 (shows 100W) | W | linear | §3.5 |
| `wet` | Wet | -60–+12, -60 is -inf | -12.0 | dB | linear | Figma scale |
| `dry` | Dry | -60–+12, -60 is -inf | 0.0 | dB | linear | Figma scale |
| `output` | Output | -24–+12 | 0.0 | dB | linear | as the other plugins |
| `bypass` | Bypass | off/on | off | | | §4.6 |
| `latency` | Latency mode | Zero latency, Low CPU | Zero latency | | not automatable | §5.3 |
| `maxFft` | Max FFT | 16384, 65536 | 65536 | | not automatable | §3.5 |
| `irStorage` | IR storage | Embed under 10 MB, Always embed, Path only | Embed under 10 MB | | not automatable | §5.3 |

Module settings aren't host parameters. A chain's length and order change at runtime and VST3's parameter list is fixed, and moving one rebuilds the IR, so they couldn't follow automation anyway. They're saved in the plugin state. A knob change reaches the sound through the 64 ms crossfade, which takes the place of §4.2 smoothing for these.

| Module | Setting | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| File | Mix to mono | off/on | on | | |
| File | Gain mode | 8 choices above | no SRC-gain, -18 dB | | |
| LP/HP | Lowpass | 20–24000, top is off | 24000 | Hz | log |
| LP/HP | Highpass | 20–24000, bottom is off | 20 | Hz | log |
| Echo | Start pos. | 0–1000 | 0 | ms | log-like |
| Echo | Length | 0–5000 | 500 | ms | log-like |
| Echo | Spacing | 1–1000 | 100 | ms | log |
| Echo | Error | 0–100 | 0 | ms | log-like |
| Echo | ErrDst | 0–1 | 0.5 | | linear |
| Echo | Start Vol. | -60–0 | -6 | dB | linear |
| Echo | End Vol. | -60–0 | -24 | dB | linear |
| Normalize | Peak Level | -60–0 | 0 | dB | linear |
| Trim | Start | 0–60000 | 0 | ms | log-like |
| Trim | End cut | 0–60000 | 0 | ms | log-like |
| Trim | Fade in | 0–5000 | 0 | ms | log-like |
| Trim | Fade out | 0–5000 | 0 | ms | log-like |
| Trim | Gain | -60–+24 | 0 | dB | linear |
| Trim | Stretch | 25–400 | 100 | % | log |
| Reverb | Length | 0.1–20 | 2.5 | s | log |
| Reverb | Decay | 0.05–20 | 1.8 | s (RT60) | log |
| Reverb | Density | 0–100 | 80 | % | linear |
| Reverb | Damping | 0–100 | 40 | % | linear |
| Reverb | Stereo spread | 0–100 | 100 | % | linear |

Reverse has no settings. Each module also has a power state. The IR display's handles are chain settings `trimStart` and `trimEnd`, saved in seconds.

**Echo generator:** echoes start at Start pos. and repeat every Spacing for Length, with their level running from Start Vol. to End Vol. in dB. Error moves each echo in time by up to its value, and ErrDst shapes that: `|offset| = Error × |u|^(1 + 4 × ErrDst)`, with `u` from -1 to 1, so higher values keep most echoes near their slots. The two channels draw different offsets.

**Reverb generator:** seeded noise, so the same settings always give the same IR. Each sample is noise under an RT60 envelope, thinned by Density at the start and filling in over 100 ms. Damping lowers a one-pole low pass as the tail goes on. Stereo spread sets how much the channels' noise differs.

**Stretch:** resamples the IR, so length and pitch change together.

## Factory presets

Presets are chains, so `FactoryPreset` gains an optional chain description.

Empty, Small Room, Plate, Hall, Long Tail and Slapback. The last five use the generators, so they need no IR files.

A new instance starts on the Figma empty state, with Dry at 0 dB, so it passes audio unchanged until a module is added.

## State

`stateVersion` 1, in the shared format, with a `CHAIN` child holding `trimStart`, `trimEnd` and one `MODULE` per card. Modules have a `type`, `on` and settings, and File adds `path`, `hash`, `flac` and `rate`.

`FancyProcessor::applyState` currently deletes every child that isn't a known parameter, so it changes to leave non-`PARAM` children alone, and a new virtual `stateApplied()` lets the Verb rebuild. Module types it doesn't know are dropped, numbers are checked and clamped, and FLAC data that won't decode is treated as missing.

## Latency

`setLatencySamples` is called when the mode changes, on the thread that changed it, and in `prepareToPlay`. The dry path is delayed to match. The wet path is silent for that swap, so the change makes a short gap but no click, and it can only happen from SETUP or a state load.

## §4 rules

| Rule | Handling |
|---|---|
| 1 Real time | Audio thread: no allocation or locks. `std::atomic` hand-offs only, and a mutex only on the build side |
| 2 Smoothing | Pre-delay 40 ms through an interpolated read; Center, Width, Wet, Dry and Output 20 ms |
| 3 Denormals | `ScopedNoDenormals`. Convolution has no feedback path |
| 4 Latency | Above |
| 5 Rates and blocks | All block sizes are handled inside the convolver. Pre-delay buffer sized for 500 ms at the session rate |
| 6 Bypass | 20 ms crossfade to the delayed dry path |
| 7 State | Above |
| 8 Automation | Regression test as in the other plugins |
| 9 Buses | Mono and stereo, in = out |
| 10 Presets | Above |
| 11 CPU | Benchmark: 4 s stereo IR at 48 kHz/128 samples, in Zero latency and Low CPU |

`getTailLengthSeconds` returns the IR length plus Pre-delay.

## UI

Geometry comes from `Reaverb1` and `RevBlocks` in `design/figma-spec.json`. The window is 866 × 677 once the host title bar is removed.

| Area | Bounds, relative to the body |
|---|---|
| IR display | 0, 0, 734 × 200 |
| Rack | 0, 200, 734 × 352 |
| Chain strip | 0, 552, 734 × 72 |
| Right column | 734, 0, 132 × 624 |

Cards are 296 (File), 147 (LP/HP), 356 (Echo) and 163 (Normalize) wide with 4 px gaps. The add panel is 203 wide. Figma has no card for Trim, Reverse or Reverb generator, so they're laid out from the same parts: Trim 356 and Reverb 356, in Echo's two-row grid, and Reverse 163.

SETUP opens a drawer with dropdowns for Latency mode, Max FFT and IR storage, a button for the IR folder, and the late-block counter.

## Thread priority

The convolver's worker runs at JUCE `highest`, which is below the audio thread in a host. On a machine with a free core it only ever costs that worker time. The benchmark in `tests/VerbTests.cpp` runs its audio thread at time-critical priority for that reason, and keeps a normal-priority run as a control.

## Testing plan

1. Convolver output against a direct time-domain convolution, for both modes, stereo and mono, at block sizes 1, 7, 128, 1000 and 8192.
2. Latency reported equals the measured impulse position, every load.
3. A swap under audio stays below a set level jump, and no allocation-prone path runs on the audio thread.
4. The resampler: error below -60 dB up to 0.85 × the lower Nyquist, and -100 dB for a tone above it.
5. Each module against its expected output, including that the same seed gives the same IR.
6. State round trip, corrupted state, an embedded FLAC with the file deleted, and the missing-IR state.
7. Offline renders are bit-identical across runs, §4.8 automation, and bypass.
8. CPU benchmark.

## Phase 3 Q2.8: IR library, true stereo and modulation

Brief: Q2.8 of `docs/BUILD_PROMPT_PHASE3.md`. **Approved by the user on 14 November 2026, with every recommended option (R):** 1 A, 2 A, 3 A, 4 A, 5 A and the five presets. Mockup: `design/phase3-mockup-src/verb-q28.html`, rendered as `reference/phase3/24__verb_q28_mockup.png`, waiting for the user's approval before any code.

### 1. Signal path

```
                ┌─ pre-delay ─ convolver ──────────────────────────┐
in ─┬─ (sum if ─┤   direct: L→L, R→R (today)                       ├─ crossfade on a swap ─ Center/Width ─ × Wet ─┐
    │  mono IR) │   true stereo: L→L+R, R→L+R (section 3)          │                                               ├─ + ─ × Output ─ bypass ─ out
    │           └─ second tail convolver × LFO (Modulate, sec. 4) ─┘                                               │
    └─ latency delay ─ × Dry ──────────────────────────────────────────────────────────────────────────────────────┘
```

With no true-stereo IR and no Modulate module, the path is today's, to the bit (section 7).

### 2. IR library

32 original impulse responses, made by the Verb's own generators, with no recorded or third-party audio.

| Category | IRs |
|---|---|
| Rooms (8) | Booth, Small Room, Bedroom, Studio Live, Wood Room, Tiled Room, Drum Room, Garage |
| Halls (7) | Recital Hall, Small Hall, Concert Hall, Dark Hall, Bright Hall, Cathedral, Arena |
| Plates (6) | Short Plate, Vocal Plate, Bright Plate, Dark Plate, Long Plate, Drum Plate |
| Chambers (5) | Small Chamber, Echo Chamber, Stone Chamber, Stairwell, Tunnel |
| Special (6) | Reverse Swell, Gated Room, Nonlinear, Slap Room, Metal Ping, Endless Cloud |

- **Decision 1: how the generators make 32 different spaces.** Today's Reverb generator is seeded noise under one RT60 envelope, with Density, Damping and Stereo spread. Thirty-two IRs from that alone would differ mainly in length and brightness.
  - **A (R).** Extend the Reverb generator with new settings, appended and defaulting to today's output:
    - **Low decay** and **High decay:** multipliers of Decay below 250 Hz and above 4 kHz, 0.25x to 4x.
    - **Early:** off, Room, Hall or Plate. Room and Hall add a pattern of discrete early reflections from a simple image-source box; Plate has none.
    - **Size:** the box for the early pattern, 2 to 60 m.
    - **Build-up:** how long the tail takes to reach full density, 0 to 300 ms.
    - **True stereo:** section 3.

    The library is built at build time by a small tool that runs the plugin's own generator code on recipes in `plugins/verb/library/*.json`, as Sature's gain table is. The same settings are in the Reverb card, so the user has the same tools.
  - **B.** Today's generators only, combined with Echo, LP/HP and Trim in chains. No new settings, but rooms, halls and plates would sound much alike.
- **Decision 2: where the IRs live.** At 48 kHz, 24-bit and about 2.5 s on average, with the true-stereo halls and plates at four channels, the library is about 30 MB as WAV and about 22 MB as FLAC.
  - **A (R).** Embedded in `Fancy Verb.vst3` as FLAC, through JUCE binary data, so it's always there. A File module that plays a library IR saves `factory:<name>` instead of a path, hash and embedded copy, so projects stay small. The plugin grows by about 22 MB.
  - **B.** Store only the recipes (a few kB) and render each IR on the build worker when it's chosen, in well under a second. The plugin stays small and the IRs are the same, but they aren't WAV files, which the brief asked for. An `Export IR…` item in the card's menu could write one out.
  - **C.** WAV files in the release zip, copied by `install.ps1` into the IR folder. The plugin stays small, but a project then depends on the folder, so every project would carry an embedded copy again.
- **Dropdown:** the File card's dropdown gets a **Library** section, with the five categories as sub-sections, above the user's IR folder. The `<` `>` buttons step through the library within a category.
- **Level:** every library IR is normalized to the same loudness (equal energy over its first 500 ms), so switching IRs doesn't jump in level.
- **Rates:** the library is made at 48 kHz and resampled by the existing offline resampler at other rates, like any file.

### 3. True stereo

Today each IR channel convolves its own input channel: L→L and R→R. True stereo adds the cross paths, so a source on the left also reaches the right output through its own reflections.

- **Convolver:** each stage already packs the input as `L + iR` in one complex FFT. With four IR spectra (LL, LR, RL, RR), it separates the input's two spectra, forms `YL = XL·HLL + XR·HRL` and `YR = XL·HLR + XR·HRR`, and packs them back for one inverse FFT. The FFTs don't change; the spectral multiply-adds double. Expected cost: about 1.4 times the direct path, with twice the IR memory.
- **Mono bus:** the output is the input through `(LL + LR + RL + RR) / 2`, which is what the stereo output would give summed to mono.
- **Modules:** generators add their left and right outputs to LL and RR, or to all four paths with True stereo on. LP/HP, Normalize, Trim, Reverse and Stretch process all four alike. Mix to mono sums everything to a direct mono IR, as now.
- **Display:** the IR display keeps its two lanes, showing what reaches each output: LL + RL on top, LR + RR below. A small `TS` mark on the File card shows that a true-stereo IR is loaded.
- **Decision 3: which files count as true stereo.**
  - **A (R).** A four-channel file is read as LL, LR, RL, RR, the most common order. A stereo file can also be paired with a second stereo file, one per input, with **Pair right input…** in the card's dropdown. The pair is saved like the first file: path, hash and embedded copy.
  - **B.** Four-channel files only. Simpler, but IRs that come as pairs would need converting first.

### 4. Modulation

A slow movement in the tail, so long reverbs don't ring statically, without changing pitch.

- **Decision 4: how the tail moves.**
  - **A (R). Two decorrelated tails, crossfaded.** The builder makes a second copy of the tail from **Start** on, with the same spectrum and decay but a different fine structure: short-time FFT frames of 2048 samples with their phases randomised (seeded, so it's repeatable), overlap-added. A second convolver plays that copy, and an LFO crossfades the two tails at equal power. The early part never moves. Gains changing slower than 2 Hz add no audible pitch. The cost is a second convolution of the tail only, which starts after Start, so only the large, cheap partitions run: about 1.6 times the CPU for a 3 s IR.
  - **B. A slowly moving filter on the tail.** Two gentle bells, ±2 dB, wandering between 300 Hz and 6 kHz on the tail's output. Very cheap, but it sounds like filtering rather than motion, and it still needs the early part split off.
- **Controls under A:**
  - **Start:** 20 to 500 ms, default 80.
  - **Rate:** 0.05 to 2 Hz, log, default 0.3.
  - **Depth:** 0 to 100 %, default 50. At 100 % the tails swap fully.
  - **Spread:** 0 to 100 %, default 100. The right channel's LFO runs a quarter cycle ahead, so the motion is wide.
- **The LFO** is a smoothed random walk rather than a sine, so the motion doesn't repeat audibly. It's seeded, so offline renders are repeatable, and it restarts at `reset()`.
- **Decision 5: where Modulation lives.**
  - **A (R).** A new module, **Modulate**, in the add panel, with its own card (Start, Rate, Depth and Spread as small knobs) and power button. Only one per chain: with one in the chain, the add panel greys it out. Its place in the chain doesn't matter, because it always acts on the finished IR, as the trim handles do.
  - **B.** Settings in the SETUP drawer. Fewer visible changes, but it's sound design rather than setup, and SETUP holds things set once.
- **Not host parameters,** like every module setting today (see "Parameters" above): a chain's modules change at runtime and VST3's list is fixed. Rate, Depth and Spread don't rebuild the IR, so they change smoothly over 20 ms. Start rebuilds it, through the usual 64 ms crossfade.

### 5. Presets

**Decision 6 (R):** five new factory presets that show the new parts, with categories:

- **Library Hall:** Concert Hall.
- **Library Plate:** Vocal Plate.
- **True Stereo Room:** Studio Live, true stereo.
- **Modulated Hall:** Concert Hall with Modulate.
- **Endless:** Endless Cloud with Modulate at 100 %.

The six existing presets don't change.

### 6. Controls and layout

- **Main layout:** unchanged. The File card's dropdown gains the Library section and Pair right input…, and the card gains the `TS` mark. The add panel gains Modulate, and the Reverb card gains its new settings.
- **Reverb card under Decision 1 A:** today it's 356 wide, in Echo's two-row grid, with five knobs. The new settings make ten knobs, so it becomes two rows of five small knobs, plus the Early dropdown and the True stereo checkbox.
- **Modulate card:** 247 wide, one row of four small knobs.
- **Mockup:** `design/phase3-mockup-src/verb-q28.html`, after the decisions: the File card with the Library menu open, the Reverb card, the Modulate card and the IR display with a true-stereo IR.

### 7. State and compatibility

- **State:** `stateVersion` is shared and stays at 4. Everything new is a new attribute or module type, so a v1.4.0 state has none of it and loads unchanged. A state saved by this version and opened in v1.4.0 drops the Modulate module and the pairing (unknown types and attributes are ignored, as now) and plays the direct IR.
- **New attributes:** File `factory`, `pairPath`, `pairHash`, `pairFlac` and `pairRate`; Reverb `lowDecay`, `highDecay`, `early`, `size`, `buildUp` and `trueStereo`, all defaulting to today's output.
- **Bit-identical:** the direct convolver path keeps its code. A copy of today's convolver in `tests/`, as `LegacyCompDsp` was for Comp, checks that a direct IR renders the same, sample for sample.

### 8. Tests (Catch2)

1. **Compatibility:** every existing factory preset and a File-module state render bit-identically to the legacy convolver, in both latency modes.
2. **Generators:** each new Reverb setting at its default gives today's IR exactly. Low and High decay give the set RT60 per band within 10 %, measured by Schroeder integration in octave bands. Early with Size puts the first reflections where the image-source model says, within one sample.
3. **Library:** all 32 load and are finite, they're within 0.5 dB of the shared loudness, and two runs of the tool give identical files. The binary data's total size is recorded in the measurements.
4. **True stereo:** an IR with only LR set sends a left-only input to the right output alone. The convolver's output matches a direct time-domain convolution of all four paths within -100 dB, at block sizes 1, 7, 128, 1000 and 8192, in both latency modes.
5. **Pairing:** a pair of stereo files gives the same IR as the equivalent four-channel file.
6. **Modulation:**
   - The decorrelated tail has the same octave-band energy envelope as the original within 1 dB, and a correlation with it under 0.2.
   - Depth 0 renders bit-identically to no Modulate module.
   - A 1 kHz tone through a modulated hall shows no sidebands above -60 dB beyond ±2 Hz, so no pitch change.
   - Renders are repeatable.
7. **State:** a true-stereo, paired, library and modulated chain round-trips. The same state with those parts stripped plays the direct IR without errors.
8. **CPU (§0.4):** a 3 s IR direct (today's ratio), true stereo, modulated, and both, in Zero latency and Low CPU, in `docs/measurements/verb-q28.txt`. Targets: true stereo under 1.6 times direct, and modulation under 2.2 times direct (restated by the user on 14 November 2026 from 1.8 times: Modulate convolves the tail twice).

### 9. Build order

Three pushes, checked in REAPER together:

1. True stereo in the convolver, and the File card's four-channel and paired files, with tests 1, 4, 5 and 7.
2. The Reverb generator's new settings, the library tool and the 32 IRs, and the Library menu, with tests 2 and 3.
3. Modulate and the presets, then screenshots, the manual, the checklist and CPU.

Before push 2, a Python model of the image-source early pattern and the per-band decay, to check the recipes sound as named before they're baked.

### 10. As built

Three pushes, as planned. The models are `docs/research/verb-q28-model.py`.

- **Compatibility:** `tests/LegacyVerbConvolver.*` and `tests/LegacyVerbIrBuilder.*` are the convolver and builder at commit `1e24056`. A test builds four chains (every module type, files of several shapes) with both and requires identical IRs, previews and convolver output in both latency modes.
- **True stereo (push 1):** the convolver carries the RL and LR spectra and direct taps only for a true-stereo IR. The builder's four slots hold LL, RR, RL, LR, with the cross pair empty for a direct IR. A File module is true stereo only with `ts`, set when a four-channel file is chosen or a pair made; a project that used a four-channel file before keeps its first two channels. Files with more than two channels are read per channel. CPU, 3 s IR: 1.26 x direct in Zero latency and 1.09 x in Low CPU (target 1.6 x).
- **IR display:** it has one lane, as before (the mockup drew two). The info line reads "4ch true stereo".
- **Generator (push 2):** the original loop runs when every new setting is at its default. Otherwise: three bands from independent noises through fourth-order Butterworth filters (power-complementary, so flat at 1 x), image-source early reflections (Room: order 3, absorption 0.8; Hall: order 2, 0.85), the tail faded in over Build-up with Room or Hall, Plate dense from the start, and true-stereo cross paths at 0.5 rising to 0.85 of the direct level over Build-up.
- **Library (push 2):** `tools/verb-library` builds the 32 recipes in `plugins/verb/library/recipes.json` at build time and packs them as 24-bit FLAC into one file, which is a Windows resource (`FANCYVERBLIBRARY`) in the VST3, the Standalone, the tests and the screens tool. JUCE binary data would have generated a source file of about 100 MB. **Loudness is matched over each IR's loudest 0.5 s**, not its first 0.5 s, because Reverse Swell starts at its quietest. A Mac build will need another way to embed the file.
- **Modulate (push 3):** the builder splits the finished IR at Start with a 10 ms crossfade: the early part, the tail as it is, and the tail decorrelated (STFT frames of 2048, hop 512, root-Hann, seeded random phases, energy-matched). The main convolver plays the early part and carries two tail convolvers, crossfaded at equal power per channel by `cos` and `sin` of `depth · (1 − cos φ) / 2 · π/2`, where φ runs at Rate with a smoothed random drift of ±50 %, and the right channel is Spread × a quarter cycle ahead. Rate, Depth and Spread reach the convolver as live values, without a rebuild; Start rebuilds.
- **Depth 0** isn't bit-identical to no Modulate, because the IR is then convolved in two parts; the test requires the difference to stay under 1e-4 of the peak (section 8 asked for bit-identical).
- **The display** doesn't draw the decorrelated copy's outline from the mockup; it shows the whole IR, as without Modulate.
- **Add panel:** eight buttons at 38 px spacing (42 before), to fit the 352 px panel. Modulate isn't offered in the empty rack, and it's greyed in the add panel while the chain has one (no tooltip).
- **CPU:** run 43652883285 (`9b931eb`), 3 s IR: true stereo 1.15 x direct in Zero latency and 1.07 x in Low CPU; Modulate 3.37 x and 2.08 x, over the first 1.8 x target. Modulate's tails start silent, and in Zero latency their small stages ran over zeros, so the convolver now leaves out any stage (and the direct taps) whose stretch of IR is silent in every path; that changes no output, since those stages only added zeros. The user restated the target as under 2.2 x. The figures after that change are in `docs/measurements/verb-q28.txt`.
- **Reverb card:** Stereo spread's label is "Spread" on the card, to fit the small knob.
- **Add module (user, 15 November 2026; mockup `reference/phase3/25__verb_add_module_mockup.png`):** the cards' + is gone. A dashed box after the last card (203 × 352, the panel's size) opens the add panel at the end of the chain; the panel's eight buttons are 28 px at 34 px spacing, with Cancel under them. Esc closes the panel (the editor handles it, so the rack still never takes the keyboard focus), and so does a click on the rack's empty space.
- **Accuracy test:** the true-stereo convolver is checked against direct convolution with the existing test's tolerance; section 8's -100 dB is tighter than the single-precision convolver reaches.

## Phase 4 Wave 1: equal loudness, Post-EQ module and the IR browser

Items from `docs/enhancements.md` (Wave 1 of the Phase 4 order). **Approved by the user on 21 November 2026 with every recommended option (R).** Every new parameter is appended, with a default that keeps saved projects bit-identical, checked against the v1.6.0 DSP in a legacy test as Comp's Wave 1 does. Items were checked in the code on 21 November 2026 before this note was written. Backlog items 1, 2 and 3 of section 5. As with every module setting, nothing here is a host parameter (section "Parameters"): it's chain state, saved in the plugin state.

### 1. Equal loudness

A SETUP drawer option, **Equal loudness** (off): when the IR is rebuilt (a new file, preset or module change), the wet gain is set so pink noise through the IR has the same RMS as through a reference (a 2 s exponential tail), and the change is faded over the existing 64 ms crossfade. So swapping a small room for a cathedral doesn't jump 12 dB.

- **Decision 1: what it measures.**
  - **A (R).** The IR's energy, pink-weighted (deterministic, computed by the builder on its worker thread, no audio analysis).
  - **B.** The wet output's loudness over a few seconds. Fits the music, but the level then changes after you've stopped touching anything.
- Off keeps today's wet level to the bit. The applied trim shows in the drawer ("−8.4 dB").

### 2. Post-EQ module

A new module type, **EQ**, in the add panel: three bands (low shelf, bell, high shelf, each Freq and Gain; Q on the bell) applied to the IR at its place in the chain, like LP/HP. The card shows a small response curve. A band at 0 dB does nothing.

- Built with `fancy::dsp::matched` (EQ's filters), on the IR, offline, so it costs no CPU while playing.
- **State:** a new module type; an older Verb drops it (as it drops Modulate) and plays the IR without it.

### 3. IR browser

- **Drag and drop:** drop an audio file on the rack: it becomes a new File module at the end of the chain (or replaces the selected File module's file, if one is selected). Formats as the File module reads them.
- **Export IR:** in the File card's menu (and the IR display's right-click menu): writes the finished IR, after trim, as a 24-bit WAV at the session rate, true stereo as four channels.
- **Library search and favourites:** the library list gets a search field at its top (filters by name and category as you type) and a star on each row. Favourites are kept in the user's settings file, not in projects, and a "Favourites" category lists them first.

### 4. Tests

1. Legacy: every factory preset and a File-module state render identically with Equal loudness off and no EQ module.
2. Equal loudness: two library IRs whose wet RMS differs by 10 dB come out within 1 dB of each other.
3. EQ module: a +6 dB bell at 2 kHz raises the IR's response there by 6 ± 0.3 dB; 0 dB leaves the IR bit-identical.
4. Export: the written WAV equals the built IR to 24-bit precision, at the right length and channel count.
5. Drop: a dropped WAV adds a File module with that file; a non-audio file is refused with a message.
6. Favourites survive a restart and don't appear in the saved state.

Mockup: `reference/phase4/36__verb_w1_mockup.png` (source `design/phase4-mockup-src/w1_mockups.py`): the EQ card, the library's search and stars, the drawer option.

### 5. As built

One push (Verb Phase 4 Wave 1 commit) holds the whole item.

- **Equal loudness:** `ChainSpec::equalLoudness`, saved on the chain as `eqLoud` only while it's on, so switching it off leaves the state as before. `buildIr` measures the IR after the display handles' range: each path's energy with 1/f weights from 20 Hz to 20 kHz over frames of about 1/6 s (so white noise reads its plain energy), the two outputs averaged, the cross paths added for true stereo. The reference is where a library IR at the default gain mode (-18 dB) lands (the library measures about 0.46 at 0 dB), scaled with the rate, so those play with about 0 dB of trim. **Changed after CI run 44942993322:** the first reference, a white tail at -18 dBFS, was about 42 dB too loud and pinned every trim at +24. The trim is held to ±24 dB and applied to the whole IR before the display is drawn, so the waveform and peak show what plays; the convolver's 64 ms crossfade covers the change. The drawer shows it beside the checkbox ("-8.4 dB").
- **EQ module:** type `eq`, appended (an older Verb drops it and plays the IR without it). Low shelf 20 Hz to 1 kHz (120 Hz), bell 100 Hz to 16 kHz (2 kHz, Q 0.2 to 10, 1.0), high shelf 1 to 20 kHz (8 kHz), each ±18 dB, 12 dB/oct shelves, from `fancy::dsp::matched` in double precision. A band at 0 dB isn't run, so the module at its defaults leaves the IR bit-identical. The card is 207 wide, blue as LP/HP, with the response curve (20 Hz to 20 kHz, ±18 dB) at the top and a row per band. The add panel's nine types are 31 px apart; the empty rack's buttons are two rows of four.
- **Export IR:** the build thread keeps the finished IR in file order (`verb::exportableIr`: Modulate's tail added back; true stereo as LL, LR, RL, RR, so it loads back as a true-stereo file). `VerbProcessor::exportIr` writes it as 24-bit WAV at the session rate. Reached from the IR display's right-click menu (with Reset trim) and the File dropdown.
- **Drag and drop:** `ModuleRack` is the drop target. **Changed from section 3:** the rack has no selected card, so a file dropped on a File card replaces that card's file, and anywhere else adds a File module at the end. The rack tints purple with a dashed edge and says what the drop will do; a file Fancy Verb can't read is refused with a message for 3 s.
- **Library search and favourites:** `LibraryPicker` replaces the File dropdown's menu: a search field (every typed word must appear in the category or name), FAVOURITES first, then each category, then the IR folder's files and the file actions (pair, remove pair, Export IR...), which hide while searching. Arrow keys, Enter (the first match while searching) and Esc work. Favourites live in the user's `settings.xml` beside the IR folder, never in projects.
- **Tests:** `tests/VerbW1Tests.cpp`. Screenshots `verb-eq`, `verb-picker` and `verb-drop`.

## Phase 4 Wave 2: decay module

**Approved by the user on 21 November 2026 with every recommended option (A).** Item 7 of the Wave 2 order (`docs/enhancements.md`; backlog item 4 of section 5). A module that makes the IR's decay shorter or longer per band, like Pro-R 2's decay EQ but on the IR, offline, so it costs nothing while playing. Modelled first: `docs/research/verb-decay-model.py`.

### 1. What it does

A new module type, **Decay**, in the add panel. At its place in the chain it splits the IR into bands, measures each band's RT60 (Schroeder backward integration, −5 to −35 dB), and reshapes each band's envelope so its RT becomes the multiplier × what it was, then sums the bands. A multiplier of 1 leaves the band as it is.

- Settings (module settings, saved in the chain like every module's): **Low** and **High** crossovers (20 Hz to 1 kHz, default 250 Hz; 1 to 16 kHz, default 4 kHz) and **Low**, **Mid** and **High** multipliers (0.25 × to 4 ×, default 1 ×).
- An older Verb drops the module and plays the IR without it, as with the EQ module.

### 2. What the model found

On a 4 s noise IR with RT 2 s in every band and a −100 dB noise floor (a 24-bit file's):

- **Multipliers at 1:** the IR comes back to float rounding (largest change 9 × 10⁻¹⁶). The band masks are complementary and the split is zero-padded, so nothing wraps round.
- **Low × 0.5, Mid × 1, High × 2:** measured in a third octave at each band's centre, 1.98 → 1.05 s (aim 0.99), 1.93 → 1.93 s, 2.00 → 3.96 s (aim 4.01). The first 50 ms changes by 0.3 dB.
- **Lengthening lifts the noise floor.** Growing a band's tail by ×2 also raises the file's floor at the end of a 4 s IR from −99 dB to −41 dB under the start: audible hiss. So a band's growth stops where its envelope comes within 10 dB of its floor, and the band fades out over 100 ms from there. The floor then ends at −108 dB. The decay is lengthened as far as the IR has decay to lengthen, and no further.
- **How it's measured matters.** A brick-wall band as narrow as a third octave at 100 Hz rings for seconds by itself and gave 6.6 s for a 2 s decay. Measuring in smooth bands, and splitting with half-octave crossfades, avoids that.

### 3. Decisions

- **Decision 1: how many bands.**
  - **A (R).** Three, with two movable crossovers: enough to shorten a boomy low end or brighten a dull top, and easy to read.
  - **B.** Six fixed bands (an octave or more each, as Pro-R 2's decay curve): finer, but six knobs in a card and mostly redundant on real IRs.
- **Decision 2: lengthening past the IR's decay.**
  - **A (R).** Stop at the floor, as modelled: no hiss, and the tail can't outgrow the IR. A band asked for more than it has gets as long as it can.
  - **B.** Extend the IR when lengthening, adding decorrelated tail (Modulate's decorrelation) so ×4 really is ×4. Longer tails, but invented reverb, more build time and a longer IR to convolve.
- **Decision 3: a band without a measurable decay** (an IR that doesn't fall 35 dB in that band: a short echo pattern, say).
  - **A (R).** Measured from −5 to −25 dB instead; if that fails too, the band is left as it is, and the card shows "can't measure" by its knob.
  - **B.** The band is left as it is at once, without the second try.

### 4. How it's built

- An STFT split (8192-point frames, ¾ overlap, a window that sums flat), so a 60 s IR at 384 kHz never needs one 46-million-point FFT. Complementary masks with half-octave raised-cosine crossovers, so the bands sum to the input exactly.
- Runs in the IR builder on its worker thread, like the EQ module. No CPU while playing; the build takes longer by about the IR's length × 3 FFT passes.
- The card: three knobs for the multipliers, two small knobs for the crossovers, and a small read-out of each band's measured RT before and after. A mockup follows approval.

### 5. Tests

1. Legacy: chains without a Decay module build as v1.6.0 (and as Wave 1).
2. Multipliers at 1: the IR is unchanged within 1 × 10⁻⁶.
3. RT: a generated 2 s IR with Low × 0.5 and High × 2 measures 1.0 ± 0.1 s and 4.0 ± 0.3 s at 100 Hz and 10 kHz; 1 kHz within 5 % of before.
4. Floor: High × 4 on an IR with a −100 dB floor leaves the last 100 ms no higher than before.
5. Short IR: a single impulse passes unchanged, and the card says "can't measure".

### 6. Build order

Approval, then the card mockup (`reference/phase4/47__verb_w2_decay_mockup.png`), then one push: the module in the IR builder, the card, tests, manual and checklist.

### 7. As built

Mockup 47 approved by the user on 22 November 2026; `tests/VerbDecayTests.cpp`.

- `ModuleType::decay` (stored "decay") with `lowMult`, `midMult`, `highMult`, `lowCross`, `highCross`. The Add panel's rows are 28 px apart to fit ten types; Decay isn't offered on an empty rack.
- `verb::reshapeDecay`: an 8192-point STFT (Hann, quarter hop) splits each channel with the complementary half-octave masks; each band's RT60 is measured on every channel's energy together (Schroeder, -5 to -35 dB, else -5 to -25 dB; under 50 ms counts as no decay). Each changed band's gain runs from its envelope's peak; a lengthened band stops at 10 dB over its floor (the median of its last 200 ms) and fades over 100 ms. The channel then gets `(gain - 1) x band` added for each changed band only, so bands at 1x add nothing and the IR is exact when every multiplier is 1.
- The build reports a `DecayReadout` per module (`IrPreview::decays`); the card paints each band's RT before and after, "can't measure", and a note when lengthening was stopped.
- `tools/fix_eol.py` restores line endings after a script edit (Python's text mode on Windows writes CRLF).
- Test 3 checks each band's RT against its own before (0.5 x and 2 x, within 10 %), not the nominal 1 s and 4 s: a third octave of one noise IR measures a few percent off 2 s, differently per seed (run 44974722618 measured 0.90 s).

## Phase 4 Wave 2: ducking and level gate

**Approved by the user on 21 November 2026 with every recommended option (A).** Item 8 of the Wave 2 order (`docs/enhancements.md`; backlog items 6 and 7 of section 5). Two live stages on the wet signal after the convolver: **Ducking** turns the reverb down while the source plays and lets it bloom in the gaps, and the **Gate** stops the tail when the source stops (gated drums). Fancy Room has both (Ducking from Phase 3, the gate from Phase 4 Wave 1). Modelled first: `docs/research/verb-duck-model.py`.

### 1. What it does

- **Ducking:** Room's detector and curve: a peak follower on the input with 10 ms attack and 300 ms release, turning the wet down by up to 24 dB × Ducking, fully above −20 dBFS and not at all below −50. Parameters `duck` (0 to 100 %, 0) and `duckSource` (Main, Sidechain; Main).
- **Gate:** Room's Wave 1 gate: when the detector has been under Threshold for Hold, the wet fades out over Decay. Parameters `gate` (off), `gateThr` (−60 to 0 dB, −30), `gateHold` (10 to 500 ms, 80) and `gateDecay` (5 to 500 ms, 40). It reads the same source as Ducking.
- Both are skipped at their defaults, so a saved project renders as before to the bit. The detector code moves from `RoomDsp` into `fancy-core` (`fancy::dsp::WetDucker`, `fancy::dsp::WetGate`) for both plugins, with Room held bit-identical by its legacy test.

### 2. What the model found

Verb's Low CPU mode reports 4096 samples of latency, and its dry path is delayed to match. A detector reading the input as it arrives acts that much ahead of the wet signal it should act on: at 48 kHz the gate would shut 85 ms early, cutting a 170 ms gated reverb in half, and the duck would let go 85 ms before the hit is heard. With the detector delayed by the reported latency, both act on the wet signal as Room's do, at 44.1, 48 and 96 kHz. Zero latency mode has no latency to delay.

### 3. Decisions

- **Decision 1: where they live.**
  - **A (R).** Not in the rack: the rack builds the IR offline, and these act live on the sound after it, whatever the module order. A **Ducking** knob and a **Gate** group (checkbox, Threshold, Hold, Decay) go in the SETUP drawer's free space, with the source menu. The rack keeps one meaning.
  - **B.** As the backlog first had them, two new module cards. They'd be the only cards whose place in the chain means nothing, and the only ones costing CPU while playing.
- **Decision 2: the detector's timing.**
  - **A (R).** Delayed by the reported latency, as modelled, so both act on the wet signal they're meant for.
  - **B.** Undelayed: simpler, but early by 85 ms in Low CPU mode at 48 kHz.
- **Decision 3: a sidechain input.**
  - **A (R).** Add an optional stereo **sidechain** bus (off until the host routes something to it), with `duckSource` choosing Main or Sidechain, so a vocal can duck a reverb on a send. The host compatibility checks (H) run again for the new bus.
  - **B.** No new bus: Ducking and the Gate read the main input only. On a send (Wet only) the main input is the send, which is usually right.

### 4. Tests

1. Legacy: Ducking 0 and Gate off render as v1.6.0 in both latency modes; Room renders as before after the detector moves.
2. Timing: in Low CPU mode at 44.1, 48 and 96 kHz, a 50 ms hit with Gate on (Hold 80, Decay 40): the wet is 60 dB down 10 ms after 170 ms past the hit's wet arrival, not before it.
3. Ducking 100 %: the wet is down 24 ± 0.5 dB while a −10 dBFS tone plays and back within 1 dB 1.5 s after it stops.
4. Sidechain source: with the main input silent, a signal on the sidechain ducks the wet; with the bus off, Sidechain behaves as Main.
5. Host checks (H): the bus layouts with and without the sidechain, mono and stereo.

### 5. Build order

Approval, then the SETUP drawer mockup (`reference/phase4/48__verb_w2_duck_mockup.png`), then one push: the shared detector (Room held bit-identical), Verb's stages, the bus, tests, manual and checklist.

### 6. As built

Mockup 48 approved by the user on 22 November 2026; `tests/VerbDuckTests.cpp`.

- `fancy::dsp::WetDucker` and `fancy::dsp::WetGate` (`WetDynamics.h`) carry Room's arithmetic exactly. Room itself is left on its own copy for now, so it can't move by a bit; switching it over is a later tidy-up with its legacy test.
- `duck`, `duckSource`, `gate`, `gateThr`, `gateHold`, `gateDecay` appended. `VerbDsp` runs the stages only while either is on or still letting go; the detector reads the latency-delayed dry input, or a sidechain delayed the same way through its own ring.
- An optional stereo sidechain bus (off by default) is added; `isBusesLayoutSupported` takes it off, mono or stereo, and `processAudio` reads the main and sidechain buses separately.
- The SETUP drawer's AFTER THE REVERB group; the Gate's knobs dim while it's off.

## Phase 4 Wave 2: Attack for the Reverb generator

**Draft for approval (22 November 2026).** Item 17 of the Wave 2 order (`docs/enhancements.md`; backlog item 5 of section 5), for v1.7.1. Modelled first: `docs/research/verb-attack-width-model.py` (part 1).

### 1. What it adds

**Attack** (0 to 500 ms, default 0), a new setting of the Reverb generator module. It's a raised-cosine fade-in over Attack ms on the whole generated tail, early reflections included. The tail then swells in instead of starting at full level, as a bloom hall, a reverse-like pad or a gated room does. Build-up stays what it is: how long the tail takes to become dense, not how loud its start is. At 0 the generator builds the IR it builds today, bit for bit. It's an IR setting, so changing it rebuilds the IR as the module's other settings do, with Verb's crossfade.

### 2. What the model found

A generated tail with Decay 1.8 s:

| Attack | C80 (clarity) | Energy peaks at | Total level |
|---|---|---|---|
| 0 ms (today) | −0.8 dB | 0 ms | 0 dB |
| 50 ms | −3.4 dB | 50 ms | −1.0 dB |
| 150 ms | −12.6 dB | 130 ms | −3.0 dB |
| 300 ms | −21.0 dB | 240 ms | −5.9 dB |

A long Attack takes the tail's start away, so it loses level as well as clarity (decision 2).

### 3. Decisions

- **Decision 1: where it goes.**
  - **A (R).** A tenth knob on the Reverb module's card, after Build-up, in the card's knob grid. The card grows by one knob, and a mockup follows approval.
  - **B.** No new knob: a "Build-up shapes level" toggle that makes Build-up a fade-in as well. Fewer controls, but density and loudness can't then be set apart.
- **Decision 2: the level it loses.**
  - **A (R).** Leave it. SETUP's Equal loudness (Wave 1), when on, already evens out IR changes, and the Normalize module does too. The manual says so.
  - **B.** The generator makes up the lost energy itself, so Attack never changes the reverb's level.

### 4. Tests

1. Legacy: Attack 0 builds the same IR as v1.7.0, bit for bit, for every Reverb setting and Early type.
2. Attack 150 ms with Decay 1.8 s: the IR's 10 ms energy peaks between 110 and 150 ms, and C80 falls by 10 to 14 dB.
3. The fade-in is raised-cosine: the IR's envelope at Attack / 2 is half its level at Attack, within 1 dB.
4. Changing Attack while playing rebuilds and crossfades with no click (Verb's existing rebuild test, with Attack added).

### 5. Build order

Approval, then the Reverb card mockup (with item 18's SETUP row), then one push: the setting appended to `reverbSettings`, the envelope in `IrBuilder`, the card knob, tests, manual and checklist.

## Phase 4 Wave 2: Width to 200 %

**Draft for approval (22 November 2026).** Item 18 of the Wave 2 order (`docs/enhancements.md`; backlog item 8 of section 5), for v1.7.1. Modelled first: `docs/research/verb-attack-width-model.py` (part 2).

### 1. What it adds

Width (0 to 100 %) scales the wet signal's side. **Extra width** (100 to 200 %, default 100, in SETUP) scales it further, so Width 100 % with Extra width 200 % puts twice the side in the wet signal. The two multiply, and Width keeps its range and meaning, so automation and saved projects are untouched. At 100 % the output is today's, bit for bit.

### 2. What the model found

A wet signal whose sides are correlated 0.3, as a stereo reverb's typically are:

| | Width 100 % | 150 % | 200 % |
|---|---|---|---|
| Side raised only: each side | 0 dB | +1.6 dB | +3.1 dB |
| Side raised only: mono sum | 0 dB | 0 dB | 0 dB |
| Keeping each side's level: mono sum | 0 dB | −1.6 dB | −3.1 dB |
| Correlation | +0.30 | −0.09 | −0.36 |

Raising only the side keeps the mono sum exactly as it was, so a mono fold-down sounds the same. Each side does get up to 3 dB louder. Holding each side's level instead makes the reverb quieter in mono. Past 150 % the sides are more opposed than alike (negative correlation), which is what "wider than the source" means. The manual will say to check in mono, and that 120 to 150 % is the useful range.

### 3. Decisions

- **Decision 1: the control.**
  - **A (R).** An **Extra width** knob (100 to 200 %) in SETUP that multiplies Width, as above. Width keeps its knob and range.
  - **B.** A "Width to 200 %" toggle in SETUP that doubles what the Width knob means while it's on. One knob, but the same knob position then means two different widths.
- **Decision 2: level.**
  - **A (R).** Raise only the side: the mono sum is unchanged, and each side rises by up to 3 dB.
  - **B.** Hold each side's level: the stereo level is unchanged, and the mono sum drops by up to 3 dB.

### 4. Tests

1. Legacy: Extra width 100 % renders exactly as v1.7.0 with every Center and Width.
2. Extra width 200 % with Width 100 %: the wet side is doubled within 0.05 dB, and the mono sum `(L + R) / 2` is unchanged, bit for bit.
3. Smoothing: moving Extra width from 100 to 200 % while playing changes the side over 20 ms, as Width does, with no click.

### 5. Build order

Approval, then the SETUP row in the mockup with item 17, then one push: the parameter, the side gain, the SETUP knob, tests, manual and checklist.
