# FancyBench: measuring plugins head to head

FancyBench measures our plugins and any other VST3 the same way, at the same settings, in the same session on your PC. You get a report with tables and plots. The design is `docs/phase3-Q1.md`.

## Where it is

`install.ps1` (and `scripts\get-latest.ps1`) puts it in `%USERPROFILE%\VST3-Dev-Tools`:

```
VST3-Dev-Tools\FancyBench.exe
VST3-Dev-Tools\examples\compressor.json, eq.json, delay.json, delay-echo.json
```

Install copies the example files only when they aren't already there, so your edits are kept. To get a fresh copy, delete the file and install again.

Run it from PowerShell in that folder:

```powershell
cd $env:USERPROFILE\VST3-Dev-Tools
.\FancyBench.exe run examples\compressor.json
```

The report goes to `bench-results\<date>-<time>-<name>\report.md` in the folder you run it from. Open it in VS Code or any Markdown viewer to see the plots.

## Step by step

1. **Install the competitors.** The examples compare with the free leaders from the market report:
   - TDR Kotelnikov and TDR Nova, from tokyodawn.net;
   - TAL-Dub-X, from tal-software.com;
   - Valhalla Freq Echo, from valhalladsp.com.

   Their installers put them in `C:\Program Files\Common Files\VST3`, which the examples point at.
2. **Find their parameter names.** Each plugin names its parameters its own way:

   ```powershell
   .\FancyBench.exe params "C:\Program Files\Common Files\VST3\TDR Kotelnikov.vst3"
   ```

   This lists every parameter with its index, its name in quotes and its current value as text.
3. **Edit the example.** The competitors' `params` in the examples are guesses, marked with a `todo`. Put in the names from step 2, and values that match ours. Then delete the `todo` line. While a `todo` is there, a name that doesn't resolve skips that plugin with the note, instead of failing.
4. **Run it**, and read `report.md`.
5. **Paid plugins:** the examples also list FabFilter, Soundtoys and Valhalla's paid plugins with `"disabled": true`. If you own or trial one, set it to `false`, check its path and names, and run again.

A plugin that isn't installed is skipped and listed as such. A plugin that crashes or stops responding for 2 minutes is reported as failed, and the others still run. Each plugin runs in its own process.

## Test files

```json
{
  "name": "Compressor, 4:1 at -20 dB",
  "sampleRate": 48000,
  "blockSizes": [128],
  "tempo": 120,
  "plugins": [
    { "label": "Fancy Comp", "path": "%FANCY_VST3%/Fancy Comp.vst3",
      "params": { "Threshold": "-20", "Ratio": "4" } },
    { "label": "TDR Kotelnikov", "path": "%COMMONPROGRAMFILES%/VST3/TDR Kotelnikov.vst3",
      "params": { "Threshold": "-20", "Ratio": "4" } }
  ],
  "tests": { "cpu": {}, "latency": {}, "response": {}, "thd": {}, "aliasing": {}, "null": {}, "clicks": { "param": "Threshold", "from": "-40", "to": "0" } }
}
```

- **Paths** can use environment variables. `%FANCY_VST3%` is `%USERPROFILE%\VST3-Dev` unless you set it.
- **`params`**: a parameter by its name (any case) or by `#index`, with its value as the plugin displays it:
  - `"4"` for 4:1, `"-20"` for -20 dB, `"2.5k"` or `"2500"` for 2.5 kHz, or `"Warm"` for a choice.
  - `"=0.5"` sets the raw 0-1 value instead.

  If the plugin can't parse the text, FancyBench searches the parameter's range for the value whose display matches. Some plugins, such as Valhalla's, display their current value whatever they're asked about; FancyBench detects that and applies each value it tries, then reads the display back. The report shows what each plugin displayed, and notes "found by search" or "read back" where it had to.
- **`clicks`** in a plugin's entry: its own `param`, `from` and `to` for the clicks test, since plugins name the same control differently ("Tap 1 Length", "delay", "Time Ms").
- **`preset`**: a `.vstpreset` file to load before `params`, for settings that are easier to save from REAPER (the FX window's `+` menu > Export VST preset).
- **`name`**: picks one plugin from a bundle that holds several.
- **`disabled`** leaves an entry out. **`todo`** is a note, shown if the entry is skipped.
- **`blockSizes`**: CPU runs at each. The other tests use the first.
- **`settle`** (2 s): silence rendered after the settings are applied and before any measurement, so a plugin that glides to a new delay time, or smooths its parameters, has finished.

## The tests

Every test starts each plugin fresh, in stereo with no sidechain, after a settling run. The left output channel is analysed.

| Test | Options (defaults) | What it reports |
|---|---|---|
| `cpu` | `seconds` (20), `repeats` (5), `pace` (true) | Mean, 99th percentile and worst block, as % of one core, and the whole process's CPU over the run, which includes background threads. With `pace`, blocks come at the audio rate, as in a DAW. If Fancy Comp is installed, every figure is also a ratio to it at its defaults, which compares across PCs and with CI |
| `latency` | | The latency the plugin reports, and the delay measured on noise. Through minimum-phase filtering, the measured peak can be a few samples later. A large difference means the host misaligns the plugin |
| `response` | `level` (-20 dBFS) | Magnitude and phase from 20 Hz to 20 kHz, from a 10 s sweep, with the latency removed |
| `thd` | `frequencies` (100, 1000, 5000), `levels` (-20, -6, 0) | THD (harmonics 2-10) and THD+N, in % and dB |
| `aliasing` | `frequencies` (5000, 10000, 15000), `level` (0) | The loudest non-harmonic component, the number of them, and their total, against the fundamental. Plots the spectrum |
| `null` | | Output minus input, on pink noise and on a sine. For plugins set to be neutral; below -120 dBFS is a null |
| `clicks` | `param`, `from`, `to` (here or per plugin), `period` (0.05 s), `seconds` (2) | A 1 kHz sine while the parameter jumps between two values. Counts bursts over the plugin's own static figure; a click-free plugin reads close to it |

## Reading the results

- **CPU:** compare the ratio column, or plugins from the same session. Absolute figures change with the PC and its load. Close other programs first.
- **Response and THD** are level-dependent for dynamics plugins: set the levels to match how you use them.
- **"Not steady"** beside a THD or aliasing reading means the output level changed by more than 0.1 dB during the analysis, so the figure doesn't measure distortion. A delay with feedback is the usual cause, since its echoes keep arriving: measure delays with `delay-echo.json`, which has no feedback and no dry signal.
- **The report measures; it doesn't rank.** It doesn't say which plugin sounds better.

After a session, send me the `bench-results` folder, or paste `report.md`. The results go into `docs/research/bench-<date>.md` and the market summary.

## In CI

Every build runs `FancyBench smoke` on our nine plugins:
- each plugin's reported latency against its measured delay;
- Fancy EQ's measured bell against its own design, within 0.1 dB;
- nulls at neutral settings;
- CPU ratios against the Catch2 measurements;
- the three examples, with the competitors skipped.

The output is `bench.txt` in the measurements artifact.
