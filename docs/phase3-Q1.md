# Phase 3, item Q1: the benchmark host (FancyBench)

Brief: §Q1 of `docs/BUILD_PROMPT_PHASE3.md`. Why: the market report couldn't compare our plugins with the leaders head to head (`docs/research/2026-11-market-report-summary.md`, "How this was compared"). This tool lets the user measure ours and any VST3 they own or trial, at the same settings, in the same session, on their own PC.

This note is the design. The how-to, `docs/bench.md`, is written with the code.

## 1. What it is

`FancyBench.exe`: a console program, built in CI from `tools/bench`, shipped in the build artifact and the release zip as `tools/FancyBench.exe`, beside AudioPluginHost. `install.ps1` copies it, with the example test files, to `%USERPROFILE%\VST3-Dev-Tools`.

- **Hosting:** JUCE's `VST3PluginFormat`, as `tools/hosttest` already does. Any `.vst3` loads by path. It only reads plugin files where they're installed, and never copies, ships or bundles a third-party binary.
- **No new dependencies.** JSON via `juce::JSON`, FFTs via `juce::dsp::FFT`, plots drawn with `juce::Graphics` and saved with `PNGImageFormat`. All are JUCE modules the project already uses.
- **A crash can't lose the session.** Each plugin is measured in a child process (FancyBench running itself with `--one`), so a plugin that crashes, or stops writing progress for 120 s, is reported as failed and the rest carry on.

## 2. Commands

| Command | What it does |
|---|---|
| `FancyBench params <plugin.vst3>` | Lists the plugin's parameters: index, name, current value as text, and the label. This is how the user finds the names to put in a test file |
| `FancyBench run <test.json> [--out <folder>]` | Runs a test file and writes the report |
| `FancyBench smoke <folder>` | CI only: the checks in §6 on every `.vst3` in the folder |

## 3. Test files

A test file names the plugins, their settings and the tests. The same settings are written once per plugin, because each names its parameters differently:

```json
{
  "name": "Compressor, 4:1 at -20 dB",
  "sampleRate": 48000,
  "blockSizes": [128],
  "tempo": 120,
  "plugins": [
    { "label": "Fancy Comp",
      "path": "%USERPROFILE%/VST3-Dev/Fancy Comp.vst3",
      "params": { "Ratio": "4", "Threshold": "-20", "Attack": "10", "Release": "100" } },
    { "label": "TDR Kotelnikov",
      "path": "C:/Program Files/Common Files/VST3/TDR Kotelnikov.vst3",
      "params": { "Ratio": "4", "Threshold": "-20" } }
  ],
  "tests": {
    "cpu": { "seconds": 20, "repeats": 5 },
    "latency": {},
    "response": { "level": -20 },
    "thd": { "frequencies": [100, 1000, 5000], "levels": [-20, -6, 0] },
    "aliasing": { "frequencies": [5000, 10000, 15000], "level": 0 },
    "null": {},
    "clicks": { "param": "Threshold", "from": "-40", "to": "0", "period": 0.05 }
  }
}
```

- **Parameters:** by name, case-insensitive, or by `#index`. A value is text in the plugin's own units, converted with the plugin's own `getValueForText`, so "4" means 4:1 to a compressor whatever its range. A value written `"=0.5"` is normalised instead. After setting, the report shows what the plugin displays for each value, so a value it didn't take is visible straight away. An unknown name stops that plugin with a list of the closest names.
- **Presets:** `"preset": "<file.vstpreset>"` loads a preset before `params` apply, for settings that are easier to save from REAPER.
- **Paths:** environment variables expand. A plugin that isn't installed is skipped with a note, so the example files work for whoever owns whichever plugins.
- **Host context:** stereo in and out, no sidechain, and a playhead that's playing at `tempo`, so tempo-synced delays work.

## 4. Measurements

Every test runs each plugin from a fresh instance, after a 2 s pre-roll so smoothing, lookahead and release have settled. CPU runs in real-time mode. Every other test runs with `setNonRealtime(true)`, which our plugins and most others treat as "do all the work inline", so the results don't depend on background threads.

| Test | Signal | Reports |
|---|---|---|
| **CPU** | Pink noise at −18 dBFS RMS, `seconds` long, at each block size. The audio thread runs at time-critical priority, as the Verb benchmark does | Per block: time ÷ block duration, as mean, 99th percentile and worst block. Also the process's CPU time on all threads over the run (`GetProcessTimes`), which catches work done on background threads, such as Verb's and FIR's workers. With `repeats`, the median of the means (§0.4). If Fancy Comp is installed it's timed at its defaults in the same session, and every figure is also given as a ratio to it, which is comparable with CI and between PCs |
| **Latency** | 2 s of white noise at −20 dBFS | The latency the plugin reports, the delay measured from the peak of the input-output cross-correlation, and the difference in samples. Works through filtering, where an impulse's peak would move |
| **Frequency response** | Exponential sine sweep, 20 Hz to 20 kHz, 10 s, at `level` | Magnitude and phase from the deconvolved impulse response (Farina), with the reported latency removed, at 1/24-octave points, from 20 Hz to 20 kHz. CSV, and one PNG with every plugin overlaid |
| **THD and THD+N** | A sine at each frequency and level, 1 s settled then 1 s analysed, with a 4-term Blackman-Harris window | THD from harmonics 2 to 10 below Nyquist; THD+N from everything but the fundamental between 20 Hz and 20 kHz. Both in % and dB. PNG: THD against level |
| **Aliasing** | A sine at each frequency, at `level` (0 dBFS by default) | The loudest component that isn't within 25 Hz of a harmonic (k × f below Nyquist), which leaves out the skirt of a gain that's still settling, and stands 30 dB over the noise floor and above −160 dBFS, in dBFS and relative to the fundamental, and the total of such components. PNG: the spectrum with harmonics and aliases marked |
| **Null** | Pink noise at −18 dBFS, then a 1 kHz sine at −6 dBFS, 3 s each, analysed after 1 s | The output minus the input delayed by the reported latency, as RMS and peak in dBFS, and relative to the input. For plugins set to neutral; a figure below −120 dBFS reads "null" |
| **Clicks** | A 1 kHz sine at −12 dBFS, with `param` jumping between `from` and `to` every `period` seconds, at the block size, in 32-sample blocks | The output with the sine removed (a narrow notch) and high-passed at 4 kHz, against the same with no automation: the loudest burst in dBFS and the count of bursts over −60 dBFS. A click-free plugin reads close to its static figure |

Signals are generated with fixed seeds, so a run repeats exactly, apart from CPU.

## 5. Report

`bench-results/<date>-<time>-<test name>/` (or `--out`):

- `report.md`: the settings each plugin displayed, then one table per test with a row per plugin, and the plots inline. A skipped or failed plugin is listed with the reason.
- One CSV per test, and `response.png`, `thd.png`, `aliasing-<f>.png`, `cpu.png`.
- `bench.json`: the test file as run, with the paths and the PC's CPU name and Windows version, so a result can be repeated.

## 6. Tests

**Catch2 (`tests/BenchTests.cpp`)**, on the analysis code alone, with synthetic signals whose answers are known. The analysis lives in `tools/bench/src/Analysis.{h,cpp}` so the test runner can link it:

1. **Latency:** a delay of 0, 1, 64 and 1234 samples measures exactly; through a 31-tap linear-phase band-pass the peak lands at the delay plus 15; a silent output reads as silent.
2. **Response:** a known biquad (a +6 dB bell at 1 kHz, Q 1) measures within 0.05 dB and 1° of its analytic response from 20 Hz to 20 kHz.
3. **THD:** `tanh` at known drives matches the THD from its series to 0.1 dB; a clean sine reads below −140 dB.
4. **Aliasing:** a hard clipper at 15 kHz, 48 kHz with no oversampling, finds the alias at 48 − 3 × 15 = 3 kHz at its computed level, within 0.5 dB.
5. **Null:** identical signals read null, and a 0.1 dB gain difference reads −38.8 dB within 0.1 dB.
6. **Clicks:** a gain step on a sine counts one burst per step, and a 20 ms raised-cosine ramp counts none.
7. **Test files:** a parameter by name, by index and as `=0.5`; an unknown name; a missing plugin; a bad file gives a message, not a crash.

**CI (`FancyBench smoke dist`)**, on our nine plugins, after the measurement step. It fails the build if:

1. **Every plugin's** measured latency is more than 3 samples after what it reports, or at all before it, at defaults and with FIR at FFT 1024 (its default is 4096). The cross-correlation peak can sit a few samples after the reported latency through minimum-phase filtering, such as MultiComp's crossovers.
2. **Fancy EQ** with band 2 at +6 dB, 1 kHz, Q 1 differs by more than 0.1 dB from the response of the bell the EQ designs, which a hidden Catch2 case prints into the measurements from `eq::design`. This checks the bench end to end against a known answer.
3. **Neutral settings** don't null where they should: Fancy EQ at defaults, and Comp with Wet at −inf and Dry at 0 dB.
4. **CPU ratios** to Fancy Comp, from the bench, differ by more than 25 % from W's Catch2 measurement's ratios in the same run (`workflow cpu:` lines), for Comp, EQ, MultiComp and Delay, on the same uniform noise, unpaced. The two time the same plugins differently (hosted against direct), so this checks the bench's timing rather than the runner.
5. **The three example files** run against our plugins without errors, with the competitors skipped as not installed.

The smoke results upload with the measurements, and the figures go to `docs/measurements/bench.txt`.

## 7. Example test files

In `tools/bench/examples/`, each with Fancy's plugin and the free leaders from the market report, which the user can install at no cost. JSON has no comments, so paid ones are entries with `"disabled": true`, to switch on if the user owns or trials one. The competitors' parameter names are filled in from `FancyBench params` once the user has them installed. Until then each competitor entry carries a `"todo"` note, and the run skips it if a name doesn't resolve. `install.ps1` copies the examples only when they aren't there, so the user's edits survive an update:

| File | Ours | Free | Paid (as comments) |
|---|---|---|---|
| `compressor.json` | Fancy Comp | TDR Kotelnikov | FabFilter Pro-C 3 |
| `eq.json` | Fancy EQ | TDR Nova | FabFilter Pro-Q 4 |
| `delay.json` | Fancy Delay | TAL-Dub-X, Valhalla Freq Echo | Soundtoys EchoBoy, FabFilter Timeless 3, Valhalla Delay |

## 8. Acceptance (§Q3)

Q1 is done when CI is green with the tests above, and the user has run `FancyBench.exe` on their PC with at least one competitor in each of the three categories. Those results go to `docs/research/bench-<date>.md`, and the market summary gets the head-to-head numbers. A plugin's sound isn't judged here: the bench reports measurements, not rankings.

## 9. Not in Q1

- A window. The console and the Markdown report cover it, and a GUI would be a layout to design and approve. It can come later.
- Loudness matching between plugins, sidechain and MIDI tests, and mid/side measurements.
- CLAP, AU or VST2 plugins.
