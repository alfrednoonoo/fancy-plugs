# Fancy FIR: manual

A spectral processor. It splits the sound into thousands of narrow frequency bands, works on each, and puts them back together. The Mode switch picks what it does to them.

## Modes

| Mode | What the curve means | What it does |
|---|---|---|
| EQ | Gain | Shapes the sound with any curve you draw |
| Gate | Threshold | Lets through each frequency that's above its threshold, and turns the rest down to Floor |
| Compressor | Threshold | Compresses each frequency above its threshold by Ratio |
| Subtract | | Removes a learned noise, such as hiss, hum or air conditioning |
| Convolve L/R | | Convolves the left input with the right one |

EQ, Gate and Compressor each keep their own curve, so changing mode doesn't lose them.

## The graph

The grey fill is the input's spectrum, and the orange line is the output's, read the same way as Fancy EQ's analyzer. A -12 dBFS sine reads -12 dB.

The coloured line is the mode's curve, or the noise profile in Subtract. In Compressor, yellow below the 0 dB line shows the gain reduction at each frequency.

Double-click the top or bottom number on the left to type the graph's range.

## Drawing the curve

With **Edit** set to Points (smooth) or Points (flat):

| Gesture | What it does |
|---|---|
| Click on the curve | Adds a point there, and you can drag it straight away |
| Drag a point | Moves it |
| Shift-drag a point | Moves it up and down only |
| Ctrl-drag anywhere | Moves the whole curve up and down |
| Alt-click a point | Deletes it |
| Double-click a point | Sets it to 0 dB |
| Right-click a point | Type its frequency and gain. Frequency accepts `2500` or `2.5k` |

Points (smooth) draws a smooth curve that never overshoots its points. Points (flat) draws straight lines between them. Outside the first and last points, the curve stays level. Up to 64 points.

With **Freehand**, draw with the mouse, and Ctrl-drag to move the whole curve. Switching to Freehand starts from what the points drew. Switching back shows the points again.

In EQ, **Reset curve** goes back to flat, and **Flat**, **Tilt** and **De-mud** load starting curves. In Gate and Compressor, Reset curve goes back to the default threshold.

## Gate and Compressor

| Control | Range | What it does |
|---|---|---|
| Floor (Gate) | -inf to 0 dB | How far a closed frequency is turned down |
| Ratio (Compressor) | 0.5:1 to 100:1 | Above 1:1 compresses. Below 1:1 raises the level above the threshold, by up to +24 dB |
| Attack | 0-200 ms | How fast it reacts |
| Release | 0-2000 ms | How fast it recovers |

Attack and Release are time constants, the time to reach about 63 % of the change. They're the same at every FFT size.

Each frequency is judged by the loudest level within two of its neighbours. That way a tone opens or ducks as a whole, rather than losing its edges.

## Subtract

1. Play a section with only the noise in it.
2. Click **Learn**. It listens for the learn length, 2 s by default, then stops by itself.
3. Play the whole track. The noise profile is the red line, and anything near or below it is reduced.

The profile is saved with the project. **Clear** removes it, and **Re-learn** captures a new one. The learn length is in EDIT.

| Control | Range | What it does |
|---|---|---|
| Reduction | 0 to -60 dB | The most any frequency is turned down |
| Offset | -20 to +20 dB | Moves the profile. Higher removes more, and can thin the sound |
| Smoothing | 0-100 % | Smooths the reduction across time and frequency. Higher avoids "musical noise", the warbling bleeps of hard noise reduction |
| Release | 10-500 ms | How fast reduction returns after a louder moment |

Start with Reduction at -12 to -18 dB. Going deeper removes more, but sounds more processed.

### Learned, Adaptive or Both

The switch at the top of the card chooses where the noise profile comes from:

- **Learned** (the default): learned once from a section with only noise, as above.
- **Adaptive:** the plugin keeps estimating the noise while audio plays, so it follows noise that changes: a fan speeding up, room tone, traffic. There's nothing to learn. The card shows **Adapt speed** and a status: "Tracking", the estimated noise floor ("Floor ≈ −52 dB", the average from 100 Hz to 8 kHz) and, while it's holding back, "Speech/music held, not learned". On the graph the tracked profile is the dashed line.
- **Both:** each frequency uses whichever is higher, the learned profile or the tracked one, so the learned profile is a floor that tracking can only add to. The card has Learn and Adapt speed.

**Adapt speed** (0.5-10 s, default 2 s) is how quickly the estimate follows a change. After the noise gets louder, the estimate catches up in about that time; when it gets quieter, it follows at once. Faster follows changing noise better; slower is steadier.

The estimate looks for the quietest level at each frequency over the last Adapt speed, so speech and music, which come and go, aren't taken for noise. A tone or a held note that stands out from the frequencies around it is held, not learned, even if it lasts a long time. A sound that sits at the same level as the noise for longer than Adapt speed, such as a steady pad with no gaps, can be taken for noise: use Learned for that material.

The tracked estimate isn't saved with the project. It starts from the learned profile if there is one, or from the first few seconds of audio otherwise, and it carries over when you change FFT size. Reduction, Offset, Smoothing and Release work the same in all three.

## Sidechain detector

In Gate, Compressor and Subtract, the plugin's channels 3 and 4 are used to make the decisions:

- **Gate and Compressor:** compare the sidechain with the thresholds at each frequency. For example, a vocal on the sidechain can carve space for itself in a pad, only at the vocal's frequencies.
- **Subtract:** treats the sidechain as the noise, without a learned profile.

## Listen

**Listen to removed noise** in Subtract, or **Listen to gain-reduced part** in Gate and Compressor, plays only what's being taken away. Use it to check you're not removing what you want to keep. It's temporary: it turns off when you close the window, it's never saved in a project or preset, and it isn't automatable. While it's on, the title strip shows **LISTENING**. A project saved with it on in v1.2.0 or earlier opens with it off.

## Convolve L/R

Convolves the left channel with the right. The right channel acts as an impulse response, renewed for each block of FFT-size samples, and the result plays on both outputs. Route the sound to channel 1 and the response to channel 2. The result isn't normalised, so a dense right channel is loud, and the Gain fader is there for that.

## FFT size and latency

A larger FFT size gives finer frequency detail and smoother results, but more latency and slower reactions. Low latency halves the latency at the same frequency detail, with a little more smearing.

| FFT size | Latency | Latency, Low latency |
|---|---|---|
| 128 to 4096 | the FFT size | half the FFT size |
| 8192 to 32768 | the FFT size plus 1/8 of it | half the FFT size, plus 1/8 of it |

At 48 kHz, 2048 is 42.7 ms, and 4096 is 85.3 ms. The header shows the current latency in milliseconds, and the plugin reports it to REAPER. At 8192 and above, the extra 1/8 lets the work spread out, so large sizes don't cause dropouts.

Changing FFT size, Low latency or Convolve L/R builds a new engine in the background, then crossfades to it. Changing between the other modes is immediate.

**Reduce artifacts** smooths the gain across neighbouring frequencies, which keeps sharp curves from ringing. It's on by default.

## Gain

The Gain fader sets the output level, from -24 to +24 dB.

## Typing values

To type a value, click it, or Ctrl-click anywhere on a knob or fader. Enter or clicking away applies it, and Esc cancels. Units are optional. Text with no number in it changes nothing, and a number past the limits stops at the limit.

- Ratio accepts `3`, `3:1`, `0.5:1` and `1:2`. Here `a:b` means `a / b`, as in Fancy MultiComp.
- Curve points: right-click one to type its frequency and gain.
- The graph's range: double-click the top or bottom number.

## EDIT

The `EDIT` link at the right of the footer holds Learn length and Output.

## Using it in other hosts

### How to sidechain

The plugin has an optional input called **Sidechain**, mono or stereo, which is off until the host turns it on. Choose a sidechain source in the plugin as well, or it keeps listening to its main input.

- **REAPER:** set the track to 4 channels, send the source track to channels 3/4, and check the plugin's pins (the I/O button) map 3/4 to the sidechain.
- **Cubase:** switch on the sidechain button at the top of the plugin window, then pick the source in the plugin's sidechain routing.
- **Ableton Live 10.1 and later:** open the Sidechain section of the plugin's device and choose the source track.
- **Tracktion Waveform:** use the input button at the top left of the plugin window to add a sidechain input from another track.
- **UA LUNA:** use LUNA's sidechain routing for the insert, as UA's LUNA manual describes.
- **Audacity** has no sidechain routing. The plugin works on its main input, and shows the warning if a sidechain source is chosen.

If a sidechain source is chosen and nothing arrives for 2 s while the main input plays, the plugin shows "No sidechain signal: check track routing (REAPER: pins 3/4)".

### Latency

Some settings change the plugin's latency: FFT size and Low latency. The plugin reports each change to the host, which delays the other tracks to match. Some hosts only read the new latency when playback stops and starts, so change these settings while stopped, or stop and start once after changing them; the tooltip says so too. In a host with no delay compensation, the track plays late by the latency, and nothing else goes wrong.

### Rendering and export

Offline renders, Audacity's Apply, freezing and exporting all sound the same as playback. When the host renders faster than real time, the plugin does the work it would otherwise do in the background before each block, so nothing is skipped.

One exception: if you change FFT size, Low latency or Convolve L/R during playback in Gate, Compressor or Convolve L/R mode, the new engine starts a block or more later than it would in a render. From then until the next such change, what you heard and a render of the same passage can differ slightly, at about -26 to -13 dB. Both sound right; they just aren't sample-identical. EQ and Subtract modes aren't affected, and neither is a render of a project where those settings stay put.

## Menus and dialogs

Menus, tooltips, typed values and prompts are drawn inside the plugin window, in its own style, and follow the UI size. The preset menu groups **Factory** and **User** presets, marks the loaded one, and has **Save as new preset…** (Ctrl+S while the plugin window has focus), **Reload** to discard changes, **Rename…**, **Delete…** and **Open preset folder**. A **•** after the preset's name means something has changed since it loaded.

Typed values take their unit or not: `120`, `120 ms` and `0.12 s` all mean 120 ms, and `2k` means 2000 Hz.

The plugin asks before anything that can't be undone, such as deleting a user preset. The host's own menus, such as REAPER's right-click automation menu, and any bar or buttons the host draws around the plugin window keep the host's look.

## Undo, A/B and Compare

The header has, between the I/O pill and the output knob:

- **↶ Undo** and **↷ Redo** step back and forward through your changes: each drag, click, typed value or preset load is one step, up to 100. Their tooltips name the step. Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y do the same while the plugin window has the keyboard focus; in REAPER the host usually keeps it, so use the buttons. Automation, and changes the host makes (REAPER's own undo, loading a project), aren't steps. The history isn't saved with the project.
- **A | B** holds two complete sets of settings. Click the other letter to switch: the plugin glides to the other settings. Both sets, and which one is active, are saved with the project. Each starts as a copy of the other. **→** copies the active set into the other one. Switching and copying are undo steps.
- **≈ Compare** plays the input instead of the output, at the volume that matches the output's loudness, so you hear what the plugin does without a change in loudness fooling you. While it's on, the button is lit and shows the matching gain, such as **+2.4 dB**. The match is measured while Compare is off, over the last 3 seconds, and holds while it's on; it stays within ±24 dB, and isn't updated while either signal is quieter than -70 LUFS. Compare isn't a parameter: it can't be automated, isn't saved and turns off when you close the window. It's separate from **ON**.

Drawing or editing a curve, clearing it, and learning or clearing a noise profile are undo steps too, and A and B each keep their own curves and profile.

