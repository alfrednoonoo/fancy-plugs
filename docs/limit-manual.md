# Fancy Limit: manual

A true-peak brickwall limiter with lookahead, three styles, Auto release, a loudness meter (EBU R128) and dither, for the end of a mix or a master.

## How it works

The input is pushed up by **Threshold**, then held at **Ceiling**. At Threshold -6 dB, everything comes up 6 dB, and the peaks that would go over Ceiling are turned down instead. Nothing leaves the plugin above Ceiling.

- **Lookahead** lets the limiter see each peak coming and turn down smoothly before it arrives, instead of clipping it.
- **True peak** also watches the points between samples, where the waveform can rise above every sample once it's converted to analogue or encoded to MP3 or AAC. With it on, Ceiling is in dBTP, the unit streaming services and broadcasters ask for.
- **Release** sets how fast the gain comes back after a peak. **Auto release** speeds it up for short peaks and slows it down when the limiting is sustained, which avoids pumping and keeps bass clean.

Inside, it runs at 4x the sample rate by default, then a final true-peak stage checks the output once more after the oversampling filters, and a last clip at Ceiling catches anything left.

## The display

| Part | What it shows |
|---|---|
| Threshold meter | The input level, with the Threshold handle over it, scaled 0 to -30 dB. Below it, Threshold in red |
| GR | How far the limiter is turning the signal down right now, in red |
| History | The last 4 s: the input after Threshold's push as a grey fill, the output as a white line, the gain reduction in red from the top, and the ceiling as a dashed red line |
| Read-outs | **Short-term** loudness (3 s), **Integrated** loudness since the start or the last reset, the highest **True peak** of the output, and the loudness **Range** (LRA). Click the panel to reset them |
| Out | The output's peak level, with a dB scale |
| LUFS | Short-term loudness, with a red line at the Target |

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Threshold | -30 to 0 dB | 0 dB | How much the input is pushed up into the limiter |
| Ceiling | -12 to 0 dB | -1.0 dB | The highest level the output reaches |
| Release | 1-1000 ms | 80 ms | How fast the gain recovers |
| Lookahead | 0-5 ms | 2 ms | How early it starts to turn down before a peak. 0 ms is instant and can distort; the latency doesn't change |
| Link | 0-100 % | 100 % | How much the two channels share their gain reduction. 100 % keeps the stereo image still; lower lets a peak on one side leave the other alone |
| Style | Clean, Punchy, Dense | Clean | See below |
| Target | Streaming -14, Podcast -16, Broadcast -23 LUFS, Custom… | Streaming | Moves the red line on the LUFS meter. It doesn't change the sound |
| True peak | on/off | on | Holds the points between samples under Ceiling too |
| Auto release | on/off | on | Program-dependent release |
| Listen to removed | on/off | off | Plays only what the limiter takes away |

**Styles** only change how the gain moves; all four knobs keep working in each:

- **Clean** turns down with the smoothest curve over the whole lookahead. The least distortion; the default for mastering.
- **Punchy** starts turning down later, so drum hits keep more of their front edge. The ceiling still holds.
- **Dense** releases twice as fast, for a louder, more even result.

**Custom…** in the Target dropdown asks for a value from -36 to -5 LUFS.

### EDIT drawer

| Setting | Choices | Default | What it does |
|---|---|---|---|
| Oversampling | Off, 2x, 4x, 8x | 4x | Runs the limiter at a higher rate, which reduces aliasing when it works hard. Changes the latency |
| Oversampling filter | Minimum phase, Linear phase | Minimum phase | Minimum phase has the least latency; linear phase keeps the phase of the highs. Changes the latency |
| Dither | Off, 16-bit, 16-bit shaped, 24-bit, 24-bit shaped | Off | Adds very quiet noise for a bounce to a lower bit depth. Shaped moves it up where it's harder to hear. Only use it on the last plugin before a 16- or 24-bit bounce, and keep Ceiling at -0.1 dB or lower |
| Meter ballistics | EBU, RMS | EBU | What the LUFS bar shows: short-term loudness, or plain RMS over 300 ms |
| Reset loudness | button | | Same as clicking the read-outs |

## Getting a master to a loudness target

1. Put Fancy Limit last on the master track, and pick the Target for where it's going.
2. Play the loudest part and lower Threshold until the GR meter shows a few dB on the peaks.
3. Click the read-outs to reset them, play the whole song, and read **Integrated**. Adjust Threshold until it lands near the Target. The LUFS bar's red line shows where short-term loudness sits against it as you go.
4. Leave Ceiling at -1.0 dBTP for streaming. Broadcast specs usually ask for -2 dBTP or lower.

A song that only reaches the target with a lot of reduction will sound squashed. **Listen to removed** lets you hear what's being taken away: if it has much more than the drum hits in it, back Threshold off.

## Factory presets

| Category | Preset | Settings |
|---|---|---|
| Mastering | Streaming master | Clean, Threshold -6, Ceiling -1.0 |
| Mastering | Loud and dense | Dense, Threshold -10, Ceiling -1.0 |
| Mastering | Broadcast safe | Clean, Threshold -3, Ceiling -2.0, Target Broadcast -23 |
| Mixing | Drum bus catch | Punchy, Threshold -4, Ceiling -0.3, Lookahead 1 ms |
| Mixing | Transparent peak guard | Clean, Threshold 0, Ceiling -0.3 |
| Utility | 16-bit bounce | Ceiling -0.3, Dither 16-bit shaped |

## Latency

The latency is fixed by the oversampling setting, never by Lookahead: 5 ms of lookahead room plus 1 ms for the final true-peak stage, plus the oversampling filters. At 48 kHz that's 312 samples with oversampling off and 318 at the default 4x, minimum phase (about 6.6 ms); linear phase adds more. Hosts with delay compensation, REAPER included, line everything up for you. Some hosts only read a new latency when playback stops and starts, so change Oversampling while stopped.

## CPU

About 3.6 % of one core at 48 kHz with 128-sample blocks, at 4x with True peak, in the CI measurements; 2.6 % with True peak off, 2.0 % with oversampling off and 5.6 % at 8x. Those runners are slower and noisier than a typical PC. Most of the cost is the true-peak checking, so untick True peak if you don't need it, for example on a drum bus.

## Rendering and export

Offline renders and exports sound the same as playback, dither included: its noise is the same every time. The loudness read-outs keep measuring during a render even with the window closed, so after a render, open the plugin and read the Integrated loudness of the whole song.

## Menus, undo, A/B and Compare

The preset browser, menus, tooltips, typed values and dialogs work as in the other Fancy plugins (see `docs/gate-manual.md`, "Menus and dialogs" and "Undo, A/B and Compare"). Typed values take their unit or not: `80`, `80 ms` and `0.08 s` all mean 80 ms.

**≈ Compare** is especially useful here: it plays the input at the output's loudness, so you hear what the limiting does without the extra loudness fooling you.
