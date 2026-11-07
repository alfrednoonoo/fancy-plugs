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
| SRC gain | Dropdown with no SRC-gain or SRC-gain, each with 0, -6, -12 or -18 dB of trim. SRC-gain multiplies by session rate ÷ file rate, which keeps the wet level the same as the file at its own rate |
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

## Testing plan

1. Convolver output against a direct time-domain convolution, for both modes, stereo and mono, at block sizes 1, 7, 128, 1000 and 8192.
2. Latency reported equals the measured impulse position, every load.
3. A swap under audio stays below a set level jump, and no allocation-prone path runs on the audio thread.
4. The resampler: error below -60 dB up to 0.85 × the lower Nyquist, and -100 dB for a tone above it.
5. Each module against its expected output, including that the same seed gives the same IR.
6. State round trip, corrupted state, an embedded FLAC with the file deleted, and the missing-IR state.
7. Offline renders are bit-identical across runs, §4.8 automation, and bypass.
8. CPU benchmark.
