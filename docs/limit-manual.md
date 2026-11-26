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
| GR | How far the limiter is turning the signal down right now, in red. With bands, it's the LIMITER card's limiter after them; each band has its own GR in the BANDS card |
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

### Bands (Phase 4)

The **BANDS** card, between the history and the LIMITER card, splits the signal into up to four frequency bands and limits each on its own before the LIMITER card's limiter. A plain limiter turns everything down when the kick hits, so the vocal and the cymbals pump with it; with the bass limited in its own band, the bands above keep their level. In a test mix pushed 8 dB, three bands cut the dip in a steady tone after each kick from about 5 dB to under 1.5 dB.

| Control | Range | Default | What it does |
|---|---|---|---|
| Bands | 1, 2, 3, 4 | 1 | How many bands. 1 is the plain limiter, exactly as before. 2 to 4 add 5 ms of latency |
| Crossover | LR 24 dB, LR 48 dB (minimum phase) | LR 24 | The crossovers' slope. Both add back up flat |
| Crossovers | 20 Hz to 20 kHz | 150 Hz, 2.5 kHz, 8 kHz | Where the bands meet: the blue lines on the strip. Drag one, or click it to type a frequency (`2500` or `2.5k`). They stay at least a third of an octave apart. 2 bands use the first, 3 the first two |
| Release (each band) | 1-1000 ms | 120, 80, 50, 40 ms | How fast that band recovers. Auto release, when on, works on every band from its own Release |
| GR (each band) | | | How far that band is turning down right now |

- The strip shows each band's gain reduction falling from the top in its colour. With one band it shows the limiter's own.
- Each band is limited 3 dB under Ceiling, so the bands do most of the work and the LIMITER card's limiter only catches what they add up to. That costs about 0.4 dB of loudness against a plain limiter at the same push, and halves the pumping again.
- **Clicky drums:** a kick's click reaches every band, so the top band still ducks briefly on each hit. Turn on **EDIT > Clip before limiting** for those.
- Listen to removed plays what the bands and the limiter take away together.

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

## Phase 4 additions

- **Gain lock** (footer): turns the output down by as much as Threshold pushes it up, so you hear what the limiting does rather than the extra level.
- **Auto to target** (top row): play the track and click it. It listens for 10 s and moves Threshold once, so the integrated loudness meets the Target. Greyed while Gain lock is on.
- **5 Hz high-pass** (drawer): removes DC offset before the limiter, so it can't use up headroom.
- **Release curve** (drawer): Exponential recovers fast and then gently, as before; Linear recovers at a constant rate in dB.
- **Overs:** under the true-peak read-out, the number of samples that would have passed Ceiling before the final clip. It should stay 0. Click to reset.

## Latency

The latency is fixed by the oversampling setting and the number of bands (2 to 4 bands add 5 ms, 240 samples at 48 kHz), never by Lookahead: 5 ms of lookahead room plus 1 ms for the final true-peak stage, plus the oversampling filters. At 48 kHz that's 312 samples with oversampling off and 318 at the default 4x, minimum phase (about 6.6 ms); linear phase adds more. Hosts with delay compensation, REAPER included, line everything up for you. Some hosts only read a new latency when playback stops and starts, so change Oversampling while stopped.

## CPU

About 3.6 % of one core at 48 kHz with 128-sample blocks, at 4x with True peak, in the CI measurements; 2.6 % with True peak off, 2.0 % with oversampling off and 5.6 % at 8x. Those runners are slower and noisier than a typical PC. Most of the cost is the true-peak checking, so untick True peak if you don't need it, for example on a drum bus.

## Rendering and export

Offline renders and exports sound the same as playback, dither included: its noise is the same every time. The loudness read-outs keep measuring during a render even with the window closed, so after a render, open the plugin and read the Integrated loudness of the whole song.

## Menus, undo, A/B and Compare

The preset browser, menus, tooltips, typed values and dialogs work as in the other Fancy plugins (see `docs/gate-manual.md`, "Menus and dialogs" and "Undo, A/B and Compare"). Typed values take their unit or not: `80`, `80 ms` and `0.08 s` all mean 80 ms.

**≈ Compare** is especially useful here: it plays the input at the output's loudness, so you hear what the limiting does without the extra loudness fooling you.

## Clip before limiting (Phase 4)

**EDIT > Clip before limiting** puts a soft clipper in front of the limiter, the usual clip-then-limit mastering chain. It shaves the fastest peaks (kick and snare clicks, a few ms long), so the limiter, which can only turn the whole signal down, has less to do: at the same push you get up to about 1 dB more loudness and gentler limiting, for a little distortion hidden inside the transients.

- **Clip** (0 to 12 dB, 3 by default): how hard it works. Its ceiling sits 12 dB minus Clip over Ceiling, so 12 clips right at Ceiling and 0 almost never touches anything. 2 to 4 dB is the usual range, for dense, drum-heavy material.
- The curve is linear up to 6 dB under its ceiling, then bends smoothly into it, which gives fewer harsh harmonics than a hard clip. It runs at Limit's oversampled rate, so at 2x and up its harmonics fold back far less.
- The history shows what the clipper takes in orange over the limiter's red. True peak and the final clip at Ceiling work as before.
