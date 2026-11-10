# Fancy DS-Mini: manual

A one-knob de-esser. Pick what you're de-essing, then turn **Amount**. It runs the same engine as Fancy DS, so any setting here sounds exactly like Fancy DS set the same way, and you can carry it over (see "Save as Fancy DS preset").

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Profile | see below | Female voice | What it listens for. The ‹ › arrows step through the profiles. The line under it shows the band |
| Amount | 0-10 | 5 | How much it de-esses. 0 does nothing at all; 5 is moderate; 10 is strong |
| Listen | on/off | off | Plays only what's being removed. It's temporary: see below |
| GR | | | How far it's turning down right now |
| Out | | | The output's peak level |

**Amount** moves two things together: where turning down starts (4 dB lower per step, measured against the voice's own level) and the most it can turn down (1.8 dB more per step). At 5 it starts 20 dB below the voice and takes off at most 9 dB; at 10, 40 dB and 18 dB.

**The display** shows the last 3 s: the level in the profile's band as a grey fill, what's turned down in blue from the top, and a dashed line where turning down starts. Amount moves the line.

**Listen is temporary,** so it can't end up in an export by mistake. While it's on, the checkbox is filled blue and the title strip says **LISTENING**. It turns off when you close the plugin's window, and it's never saved: a project or preset always opens with it off. It isn't automatable, and undo and A/B leave it alone.

## Profiles

| Profile | Listens to | Speed |
|---|---|---|
| Female voice | 4.3-11.4 kHz | fast (0.5 ms in, 60 ms out) |
| Male voice | 3.5-7.1 kHz | fast |
| Sibilance (any voice) | everything above 5 kHz | fast |
| Harshness | 2.3-5.3 kHz | slower (2 ms, 120 ms) |
| Acoustic guitar | string squeak, 2.1-4.2 kHz | medium (1 ms, 80 ms) |
| Bowed strings | rosin, 2.4-6.7 kHz | slower |

The band is where the cut reaches half its depth; it's deepest in the middle. Switching profile while playing crossfades over 50 ms, so it never clicks.

## EDIT drawer

| Setting | Choices | Default | What it does |
|---|---|---|---|
| Mode | Split, Wide | Split | Split turns down only the band. Wide turns down the whole signal, which can sound more natural on some voices |
| Lookahead | Off, 2 ms | Off | Starts turning down 2 ms before an "s" arrives. Adds 2 ms of latency (96 samples at 48 kHz), which REAPER compensates |
| Link channels | on/off | on | Both channels are turned down together, on the louder one |
| Save as Fancy DS preset… | | | See below |

## Save as Fancy DS preset

When you want more control than one knob, click **Save as Fancy DS preset…** in the EDIT drawer and give it a name. Open Fancy DS, click the preset name, and pick it from the **From DS-Mini** category: Fancy DS now sounds exactly the same, with every setting this profile and Amount use (Freq, Width, Threshold, Range, Attack, Release, Mode, Lookahead, Link channels and Output) laid out for you to change.

The file goes in `%APPDATA%\Fancy Plugs\Fancy DS\Presets\From DS-Mini\`.

## Factory presets

| Category | Preset | Settings |
|---|---|---|
| | Default | Female voice, 5 |
| Vocals | Gentle female, Strong female | Female voice, 3 and 7 |
| Vocals | Gentle male, Strong male | Male voice, 3 and 7 |
| Vocals | Podcast | Sibilance, 5, Lookahead 2 ms |
| Vocals | Harsh vocal | Harshness, 4 |
| Instruments | Acoustic guitar | Acoustic guitar, 5 |
| Instruments | Strings | Bowed strings, 4 |

## Latency and CPU

None by default; 2 ms with Lookahead. The CPU is the same as Fancy DS's at the same settings, about 0.5 % of one core at 48 kHz in the CI measurements, and briefly twice that during a profile change.

## Menus, undo, A/B and Compare

As in the other Fancy plugins (see `docs/gate-manual.md`, "Menus and dialogs" and "Undo, A/B and Compare"). Click Amount's value to type one.
