# Fancy EQ manual

Fancy EQ is a parametric equalizer with up to 16 bands, a spectrum analyzer and an optional linear-phase mode. It doesn't cramp near Nyquist, so a high boost keeps its shape at 44.1 kHz.

## The graph

| Gesture | What it does |
|---|---|
| Drag a handle | Sets Freq and Gain. Shift makes it finer |
| Alt-drag a handle, or use the mouse wheel | Sets Q |
| Double-click a handle | Resets the band |
| Double-click empty space | Adds a band there |
| Right-click a handle | Band type, Slope, On, Delete band |

The white line is the total response. The filled shapes are the individual bands, and the orange line is the analyzer. The left scale is EQ gain in dB. The right scale is for the analyzer, in dBFS.

To type a value, click it, or Ctrl-click anywhere on a knob or fader. Enter or clicking away applies it, and Esc cancels. Units are optional, and frequencies can be typed as `2500` or `2.5k`. A number past the limits stops at the limit, and text with no number in it changes nothing.

## Band controls

Click a tab or a handle to choose a band. In Compact mode these controls float over the graph, and handles do the choosing.

| Control | Range | Default | What it does |
|---|---|---|---|
| Type | See below | Shelf, Band, Band, Shelf | The filter's shape |
| Freq | 20 Hz–24 kHz | 80, 500, 2000, 8000 Hz | Centre or corner frequency |
| Gain | -24 to +24 dB | 0.0 dB | Boost or cut. The note in brackets is the one nearest Freq |
| Q | 0.1–30 | 0.707 or 1.0 | Narrower as it rises. On pass filters and shelves, 0.707 is flat, and more adds a resonant peak |
| Power icon | | on | Takes the band out without losing its settings |
| + | | | Adds a band above the selected one |
| × | | | Removes the selected band |

| Type | Gain | Slope | Notes |
|---|---|---|---|
| Low shelf, High shelf | yes | yes | Gain is reached well past Freq, with half of it at Freq |
| Band | yes | no | A bell. It never turns into a notch, however deep the cut |
| Notch | no | no | A true null at Freq |
| Low pass, High pass | no | yes | |
| Band pass | no | no | Unity at Freq |
| All pass | no | no | Changes phase only |
| Tilt | yes | yes | Pivots at Freq: -Gain/2 below and +Gain/2 above |

Changing Freq, Gain or Q is smooth. Changing a band's type or slope, or switching it, fades the band out and back in over about 20 ms.

## Dynamic bands

A Band (bell), Low shelf, High shelf, Tilt or Notch can be made **dynamic** with the **DYN** chip under its type. Its gain then moves between its Gain and Gain + Range, following how loud the signal is in that band's own region, so it only boosts or cuts when it needs to. With DYN on, the dynamic controls appear to the right of Q, in the band's colour:

| Control | Range | Default | What it does |
|---|---|---|---|
| Threshold | -60 to 0 dB | -24 dB | The level where the band starts to act. A sine's level is its RMS: a full-scale sine is -3 dB |
| Range | -30 to +30 dB | -6 dB | How far the gain can move. Negative cuts, positive boosts. It's all used 12 dB past Threshold, with a soft start over the 6 dB around it |
| Attack | 0.1-200 ms | 5 ms | How fast the band moves when it starts to act |
| Release | 5-2000 ms | 80 ms | How fast it goes back |
| Above, Below | | Above | Above acts when the level rises over Threshold (taming a harsh peak); Below acts when it falls under (lifting a weak region) |
| Sidechain | | off | The band listens to the sidechain input instead of the main input |

The band hears the input before the EQ: a band pass at its Freq and Q for a bell or notch, everything below Freq for a low shelf, and everything above it for a high shelf or tilt. While the signal stays clear of Threshold, a dynamic band is exactly the same as a static one.

**On the graph,** a dynamic band shows its static curve as usual, plus its reach, the curve at Gain + Range, as a lighter fill with a dashed edge. The white total curve includes where each dynamic band is now, so it moves inside the reach as you play. For the selected band, the top-left corner reads, for example, "reach −6 dB · now −3.6 dB".

**A dynamic notch** works as a dynamic cut: it lets everything through while quiet, and cuts by up to Range at its Freq and Q as the level rises. A positive Range does nothing on a notch. Band pass, low pass, high pass and all pass can't be dynamic.

**Linear phase:** dynamic bands need Minimum phase. In Linear mode the DYN chip is greyed out, with the tooltip "Dynamic bands need Minimum phase", and dynamic bands keep their static Gain. They come back to life when you switch to Minimum.

Dynamic bands add no latency. In Compact mode the floating card widens while the selected band is dynamic.

## Main controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Gain fader | -inf to +12 dB | 0.0 dB | Level after the bands |
| Header knob | -24 to +12 dB | 0.0 dB | Output level |
| ON pill | | on | Bypass, with a short fade. The dry signal stays time-aligned |

## Checkboxes

| Checkbox | Default | What it does |
|---|---|---|
| Log-scale automated freq. | on | With it on, an automation envelope moves Freq evenly in octaves. With it off, it moves Freq evenly in Hz. Ticking it doesn't move any band, but existing Freq envelopes will then mean something different |
| Show grid | on | The grid lines. The numbers always show |
| Show phase | off | The total phase in yellow, with the graph's height being ±180°. It's flat in Linear mode |
| Compact mode | off | Hides the lower panel, so the graph gets the room |

## EDIT drawer

| Setting | Options | Default | What it does |
|---|---|---|---|
| Phase | Minimum, Linear | Minimum | Linear keeps every frequency time-aligned, at the cost of latency. Use it for multi-mic or parallel work |
| FIR length | 2048–16384 | 8192 | Linear mode only. Longer holds low-frequency detail better and adds latency |
| Oversampling | Off, 2x, 4x | Off | Minimum mode only. Rarely needed, because the filters already hold their shape |
| Slope | 6–48 dB/oct | 12 dB/oct | For the selected band |
| Analyzer | Off, Pre, Post | Post | Before or after the EQ. The graph's corner shows which |
| Size | 4096, 8192, 16384 | 8192 | Larger shows more low-frequency detail and reacts more slowly |
| Window | Hann, Blackman-Harris | Blackman-Harris | Blackman-Harris separates a loud tone from nearby quiet ones better |
| Overlap | 50 %, 75 % | 75 % | Higher updates more smoothly |
| Graph range | Auto, ±6, ±12, ±18, ±24 dB | Auto | Auto is ±12 dB, or ±18 dB in Compact mode |
| Pink tilt | off/on | off | Adds 4.5 dB per octave about 1 kHz, so pink noise reads flat |
| Peak hold | off/on | off | Adds a fainter line at the highest level. Turn it off to clear it |
| Smoothing | 0–100 % | 50 % | How slowly the analyzer line moves |

## Analyzer calibration

A sine at -12 dBFS reads -12 dB on the right-hand scale. The analyzer shows the sum of both channels divided by two, so a signal in only one channel reads 6 dB low.

## Latency

| Setting | Latency at 44.1 or 48 kHz |
|---|---|
| Minimum, oversampling off | 0 samples |
| Minimum, 2x | 5 samples |
| Minimum, 4x | 6 samples |
| Linear, 2048 | 1152 samples |
| Linear, 4096 | 2304 samples |
| Linear, 8192 | 4608 samples |
| Linear, 16384 | 9216 samples |

At 88.2 kHz and above, Linear's FIR is 2, 4 or 8 times longer in samples, so its latency in ms stays about the same.

Changing Phase, FIR length or Oversampling gives one short dropout while the new latency settles.

## Resizing

Drag the window's edge in REAPER. The graph takes the extra room. Parameter > UI size still scales everything.

## Presets

Default, Low Cut, Vocal Presence, Smile, Air, Telephone and Linear Master. **+ New** saves your own.

## Using it in other hosts

### How to sidechain

The plugin has an optional input called **Sidechain**, mono or stereo, which is off until the host turns it on. It's only used by dynamic bands with **Sidechain** ticked; every other band ignores it.

- **REAPER:** set the track to 4 channels, send the source track to channels 3/4, and check the plugin's pins (the I/O button) map 3/4 to the sidechain.
- **Cubase:** switch on the sidechain button at the top of the plugin window, then pick the source in the plugin's sidechain routing.
- **Ableton Live 10.1 and later:** open the Sidechain section of the plugin's device and choose the source track.
- **Tracktion Waveform:** use the input button at the top left of the plugin window to add a sidechain input from another track.
- **UA LUNA:** use LUNA's sidechain routing for the insert, as UA's LUNA manual describes.
- **Audacity** has no sidechain routing. A band with Sidechain ticked hears nothing there, so it stays static, and the plugin shows the warning.

If a dynamic band has Sidechain ticked and nothing arrives for 2 s while the main input plays, the plugin shows "No sidechain signal: check track routing (REAPER: pins 3/4)".

### Latency

Some settings change the plugin's latency: Phase, FIR length and Oversampling. The plugin reports each change to the host, which delays the other tracks to match. Some hosts only read the new latency when playback stops and starts, so change these settings while stopped, or stop and start once after changing them; the tooltip says so too. In a host with no delay compensation, the track plays late by the latency, and nothing else goes wrong.

### Rendering and export

Offline renders, Audacity's Apply, freezing and exporting all sound the same as playback. When the host renders faster than real time, the plugin does the work it would otherwise do in the background before each block, so nothing is skipped.

## Menus and dialogs

Menus, tooltips, typed values and prompts are drawn inside the plugin window, in its own style, and follow the UI size. The preset menu groups **Factory** and **User** presets, marks the loaded one, and has **Save as new preset…** (Ctrl+S while the plugin window has focus), **Reload** to discard changes, **Rename…**, **Delete…** and **Open preset folder**. A **•** after the preset's name means something has changed since it loaded.

Typed values take their unit or not: `120`, `120 ms` and `0.12 s` all mean 120 ms, and `2k` means 2000 Hz.

The plugin asks before anything that can't be undone, such as deleting a user preset. The host's own menus, such as REAPER's right-click automation menu, and any bar or buttons the host draws around the plugin window keep the host's look.

## Undo, A/B and Compare

The header has, between the I/O pill and the output knob:

- **↶ Undo** and **↷ Redo** step back and forward through your changes: each drag, click, typed value or preset load is one step, up to 100. Their tooltips name the step. Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y do the same while the plugin window has the keyboard focus; in REAPER the host usually keeps it, so use the buttons. Automation, and changes the host makes (REAPER's own undo, loading a project), aren't steps. The history isn't saved with the project.
- **A | B** holds two complete sets of settings. Click the other letter to switch: the plugin glides to the other settings. Both sets, and which one is active, are saved with the project. Each starts as a copy of the other. **→** copies the active set into the other one. Switching and copying are undo steps.
- **≈ Compare** plays the input instead of the output, at the volume that matches the output's loudness, so you hear what the plugin does without a change in loudness fooling you. While it's on, the button is lit and shows the matching gain, such as **+2.4 dB**. The match is measured while Compare is off, over the last 3 seconds, and holds while it's on; it stays within ±24 dB, and isn't updated while either signal is quieter than -70 LUFS. Compare isn't a parameter: it can't be automated, isn't saved and turns off when you close the window. It's separate from **ON**.

Adding, deleting and resetting bands, and dragging them on the graph, are undo steps too.

