# Fancy Comp manual

Fancy Comp is a compressor with a sidechain input, detector filters, lookahead and an output limiter.

## Main controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Threshold | -60 to 0 dB | 0.0 dB | Level where compression starts. The fader sits over the input meter |
| Ratio | 1:1 to inf:1 | 4:1 | How strongly levels above Threshold are reduced |
| Knee | 0–24 dB | 0.50 dB | How gradually compression starts |
| Attack | 0–500 ms | 3.0 ms | How quickly compression engages |
| Release | 0–5000 ms | 200.0 ms | How quickly it lets go |
| Pre-comp | 0–250 ms | 0.0 ms | Lookahead, so compression can start before a transient arrives |
| RMS size | 0–250 ms | 5.0 ms | Averaging time of the detector. 0 ms is peak detection |
| Lowpass, Highpass | 20 Hz–24 kHz | 24 kHz, 20 Hz | Filters on the detector only, not the audio. At these defaults they are off |
| Wet | -inf to +12 dB | 0.0 dB | Level of the compressed signal |
| Dry | -inf to +12 dB | -inf | Level of the uncompressed signal, for parallel compression |
| Header knob | -24 to +12 dB | 0.0 dB | Output level, also shown in the EDIT drawer |
| ON pill | | on | Bypass, with a short fade. The controls dim while it's off |

## Checkboxes

| Checkbox | Default | What it does |
|---|---|---|
| Weird | off | An alternative knee that starts at Threshold, so compression arrives later and then catches up |
| Classic | off | Puts attack and release on the detector, as older compressors do. Short peaks then get less reduction with a long attack. With it off, attack doesn't change the maximum reduction |
| Auto | off | Program-dependent release: fast after transients, slower after sustained compression |
| Link channels | on | Both channels share gain reduction, which keeps the stereo image steady |
| Auto make-up | off | Adds back half of the gain reduction a full-scale signal would get |
| Preview filter | off | Lets you hear the filtered detector signal, to set Lowpass and Highpass |
| Limit output | off | Turns on a brickwall limiter with a 0 dBFS ceiling. It adds 104 samples of latency |

## Detector input

| Setting | What the compressor listens to |
|---|---|
| Main input L+R | Each main channel. Link channels sets how they combine |
| Main L, Main R | One main channel, for both |
| Sidechain L+R, L, R | The sidechain input |
| Mid, Side | The sum or difference of the main channels |

To use the sidechain in REAPER, route another track to channels 3 and 4 of this track, then pick a Sidechain setting. In the plugin's pin connector, make sure inputs 3 and 4 reach the sidechain pins.

## EDIT drawer

Click **EDIT** at the right of the Detector input row to open it, and **Done** to close it.

| Control | Default | What it does |
|---|---|---|
| Oversampling | Off | 2x or 4x reduces aliasing at very fast settings, at the cost of CPU and latency. With it on, the detector always averages over at least 0.1 ms |
| Oversampling filter | Linear | Linear keeps wet and dry phase-aligned. Minimum has less pre-ringing |
| Release curve | Exponential | Linear falls at a constant dB rate, like older compressors |
| Feedback detection | off | The detector listens to the compressor's own output, for a gentler, more program-dependent response |
| Attack = 90 % rise | off | See below |
| Link amount | 100 % | How strongly linked channels follow the louder one |
| Output | 0.0 dB | The same control as the header knob |

Aliasing only matters with very fast settings on bright material. In a harsh test (15 kHz sine, attack 0.1 ms, release 1 ms), the worst alias is -56.8 dBFS with oversampling off, -79.2 dBFS at 2x and -100.9 dBFS at 4x.

## Attack and release times

Attack and release are time constants: the time to cover about 63 % of the distance to the target. Compressors that quote the time to reach 90 % will seem about 2.3 times faster at the same number. Tick **Attack = 90 % rise** to use that convention for Attack.

## Latency

| Setting | Latency |
|---|---|
| Base | 0 samples |
| Oversampling 2x | 55 samples, FIR |
| Oversampling 4x | 69 samples, FIR |
| Limit output | adds 104 samples at 48 kHz |
| Pre-comp | adds the set time |

Minimum-phase oversampling has a different latency, shown in REAPER. REAPER compensates for all of this automatically. Dry is delayed to match, so wet and dry stay aligned.

Pre-comp, Oversampling and Limit output change the latency. After you stop changing one of them, the plugin waits about 50 ms and then has a short dropout while it reconfigures. Set them before you play rather than automating them.

## Using the controls

- **Drag** up or down. Hold **Shift** for fine control.
- **Double-click** to reset to the default.
- **Click the value** to type a new one, or **Ctrl-click** anywhere on the control. Enter or clicking away applies it, and Esc cancels. Units are optional, and frequencies can be typed as `2500` or `2.5k`. A number past the limits stops at the limit, and text with no number in it changes nothing.
- **Mouse wheel** to nudge, with **Shift** for smaller steps.
- **Right-click** for the host's automation menu.

## Presets and window size

Use the dropdown to load a preset and **+ New** to save your own. User presets are stored in `%APPDATA%\Fancy Plugs\Fancy Comp\Presets`.

**Parameter** lists the parameters, sets the UI size (100, 125, 150 or 200 %) and has the About box.

## Credits

UI design based on "ReaPlugs (vst GUI)" by Eugene Reznik, CC BY 4.0 — https://www.figma.com/community/file/967866927752067729. Modified from the original.
