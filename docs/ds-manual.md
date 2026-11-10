# Fancy DS: manual

A de-esser: it turns down "s", "sh" and "t" sounds in a voice, or harsh highs on cymbals and buses, only while they're too loud. By default it adds no latency and leaves the sound untouched when it isn't working.

## How it works

A detector listens to one frequency band, the sibilance band, around **Freq**. When that band gets louder than **Threshold**, the plugin turns it down, by at most **Range**.

- **Split** (default) turns down only the sibilance band, so the rest of the voice keeps its level. **Wide** turns the whole signal down, which sounds more natural on some voices and on buses.
- **Relative threshold** (default on) compares the band with the voice's own level, measured over 50 ms, instead of with 0 dBFS. Threshold then means "this many dB below the voice", so the same setting works on quiet and loud passages, and on a voice that moves away from the mic. Untick it for a fixed threshold, as on a classic de-esser.
- **Attack** and **Release** set how fast the cut comes in and goes away. The defaults (0.5 ms and 60 ms) catch an "s" without making the vowel after it dip.
- In Split, the cut is a bell that widens as it gets deeper: Width marks where the cut is half its depth, as on an EQ.

When nothing is being cut, the output is the input, bit for bit.

## The display

| Part | What it shows |
|---|---|
| Threshold meter | The detector level, with the Threshold handle over it, scaled 0 to -60 dB. Below it, Threshold in green, with "relative" under it when Relative threshold is on |
| GR | How far the band is being turned down right now, in green |
| History | The last 3 s: the detector level as a grey fill, Threshold as a green line, and the cut as a green fill from the top. Click or drag on it to set Threshold |
| Frequency ruler | 1 to 20 kHz. The green bar is the detection band and the blue handle is Freq. Drag the handle to move Freq, or the bar's edges to change Width. In Shelf, the bar runs from Freq to 20 kHz and only the handle moves |
| Out | The output's peak level |

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Threshold | -60 to 0 dB | -28 dB | How loud the band must get before it's turned down |
| Freq | 2-16 kHz | 6.2 kHz | The centre of the band, or where the shelf starts. Typed values take `6200` or `6.2k` |
| Width | 0.3-3 oct | 1.0 oct | How wide the band is. Locked in Shelf |
| Band, Shelf | | Band | Band listens to a range around Freq. Shelf listens to everything above Freq, for cymbals and bright buses |
| Range | -24 to 0 dB | -12 dB | The most it ever turns down |
| Attack | 0.1-20 ms | 0.5 ms | How fast the cut comes in |
| Release | 5-500 ms | 60 ms | How fast it lets go |
| Detector | Main input L+R, Mid, Side, L, R; Sidechain L+R, L, R | Main input L+R | What the detector listens to. Mid and Side catch sibilance in the centre or at the sides |
| Mode | Split, Wide | Split | See above |
| Relative threshold | on/off | on | See above |
| Listen | on/off | off | Plays only what's being removed. Use it to set Freq: sweep until you hear just the "s" sounds. It's temporary: see below |
| Link channels | on/off | on | Both channels get the same cut. Off, each side is de-essed on its own (Main input L+R and Sidechain L+R only) |
| Gain | -24 to +12 dB | 0 dB | The output level. The header knob is the same control |

**Listen is temporary,** so it can't end up in an export by mistake. While it's on, the checkbox is filled green and the title strip says **LISTENING**. It turns off when you close the plugin's window, and it's never saved: a project or preset always opens with it off. It isn't automatable, and undo and A/B leave it alone. An offline render only includes it if the window is open and it's on.

### EDIT drawer

| Setting | Choices | Default | What it does |
|---|---|---|---|
| Lookahead | Off, 1, 2, 5 ms | Off | Delays the audio but not the detector, so the cut is already moving when an "s" arrives. Adds that much latency |
| Split filter | Minimum phase, Linear phase | Minimum phase | Linear phase splits the band with linear-phase filters, so nothing around the band shifts in phase. Its band edges are steeper, so it takes a little less off the edges of an "s". Adds about 53 ms of latency |

## Setting it up on a vocal

1. Put Fancy DS after the vocal's EQ and compressor.
2. Tick **Listen** and play a part with plenty of "s" sounds. Drag the blue handle on the ruler until you hear mostly the "s" sounds. Untick Listen.
3. Lower **Threshold** until the GR meter shows a few dB on each "s" and nothing on the vowels. The history shows where each cut lands.
4. If it still sounds lispy, raise Range towards 0 dB. If the "s" sounds still poke out, lower Range, or try 1 or 2 ms of Lookahead.

The presets are good starting points: **Bright female vocal** and **Deep male vocal** move Freq, and **Podcast voice** adds lookahead.

## Factory presets

| Category | Preset | Settings |
|---|---|---|
| Vocals | Gentle vocal | Threshold -24, Range -6 |
| Vocals | Strong vocal | Threshold -32, Range -15, Release 40 ms |
| Vocals | Bright female vocal | Freq 7.5 kHz, Width 1.2 |
| Vocals | Deep male vocal | Freq 5 kHz, Width 1.0 |
| Vocals | Podcast voice | Threshold -26, Range -9, Lookahead 2 ms |
| Mix | Harsh cymbals | Split, Shelf at 8 kHz, Range -6, Release 120 ms |
| Mix | Bus taming | Wide, Shelf at 6 kHz, Threshold -30, Range -4 |

## Latency

None by default. Lookahead adds 1, 2 or 5 ms (48, 96 or 240 samples at 48 kHz). The linear-phase split adds 2559 samples at 48 kHz (53.3 ms); it keeps about the same time at other rates. Hosts with delay compensation, REAPER included, line everything up for you. Change these while stopped, because some hosts only read a new latency when playback starts.

## CPU

About 0.5 % of one core at 48 kHz with 128-sample blocks at the defaults, in the CI measurements, and 1.7 % with the linear-phase split. Those runners are slower and noisier than a typical PC.

## Rendering and export

Offline renders and exports sound the same as playback, at any block size, in both split filters.

## Menus, undo, A/B and Compare

The preset browser, menus, tooltips, typed values and dialogs work as in the other Fancy plugins (see `docs/gate-manual.md`, "Menus and dialogs" and "Undo, A/B and Compare"). Typed values take their unit or not: `60`, `60 ms` and `0.06 s` all mean 60 ms.
