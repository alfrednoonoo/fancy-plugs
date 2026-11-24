# Fancy FIR: manual

A spectral processor. It splits the sound into thousands of narrow frequency bands, works on each, and puts them back together. The Mode switch picks what it does to them.

## Modes

| Mode | What the curve means | What it does |
|---|---|---|
| EQ | Gain | Shapes the sound with any curve you draw |
| Gate | Threshold | Lets through each frequency that's above its threshold, and turns the rest down to Floor |
| Compressor | Threshold, and a ratio curve | Compresses each frequency above its threshold, by the ratio set for that frequency: a dynamic EQ |
| Subtract | | Removes a learned noise, such as hiss, hum or air conditioning |
| Convolve L/R | | Convolves the left input with the right one |
| Resonance | | New in 1.6.0: finds narrow peaks, such as a ringing snare or a boxy vocal, and turns them down only while they're there |

EQ, Gate and Compressor each keep their own curve, so changing mode doesn't lose them. New in 1.7.1: Compressor also has a ratio curve, and with **Mid/Side** on, every mode works on mid and side, each with its own curves.

## The graph

The grey fill is the input's spectrum, and the orange line is the output's, read the same way as Fancy EQ's analyzer. A -12 dBFS sine reads -12 dB.

The coloured line is the mode's curve, or the noise profile in Subtract. In Compressor, yellow below the 0 dB line shows the gain reduction at each frequency, and the green line is the ratio curve (see Ratio curve). The legend at the top right names each line.

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

**Learn** (top row, Gate and Compressor modes): play the track and click it. Over 3 s it measures each third of an octave and places the threshold curve: the Gate's just over the steady noise (6 dB above the quietest fifth of what it heard), the Compressor's at the loud parts (where the level is 80 % of the time). The points stay editable, and Undo takes the whole curve back. Click again to stop.

Each frequency is judged by the loudest level within two of its neighbours. That way a tone opens or ducks as a whole, rather than losing its edges.

## Ratio curve

New in 1.7.1. In Compressor mode, the ratio can be different at each frequency, so Compressor mode becomes a dynamic EQ: the threshold curve says where to act, the ratio curve says how hard, and Attack and Release say how fast. A cut appears only at the frequencies that go over their threshold, and only while they do.

The **Threshold | Ratio** switch at the graph's top left picks which curve the points edit. The other curve stays drawn, dimmed and dashed.

- **Threshold** (teal): the threshold curve, as before.
- **Ratio** (green): the ratio curve, on its own scale at the right of the graph, from 0.5:1 at the bottom to 20:1 at the top, with 1:1, 2:1, 4:1 and 20:1 marked. At 1:1 a frequency isn't compressed at all; below 1:1 it's raised above the threshold (upward expansion), as the Ratio knob does. Points are edited as on the threshold curve (see Drawing the curve). The point you click or drag shows its frequency and ratio, such as "3.00 kHz · 4.0:1". Double-click a point to put it back to the Ratio knob's value, and right-click it to type a ratio, such as `4`, `4:1` or `1:2`.

The **Ratio** knob moves the whole ratio curve: a point at 4:1 with the knob at 4:1 goes to 8:1 when the knob goes to 8:1, and one at 1:1 goes to 2:1. With no points the curve is flat at the knob's value, which is how Compressor mode always worked; projects from before 1.7.1 open that way and sound exactly as they did. **Reset ratio curve** (left card) makes it flat again, at the knob's value.

*Compressor mode with the threshold curve chosen. The threshold dips at 3 kHz, where the mix is harsh; the dimmed green ratio curve is 4:1 from 2 to 5 kHz and 1:1 elsewhere. Yellow shows the reduction at 3 kHz only. The left card holds Reset curve (the threshold) and Reset ratio curve; the note beside the knobs says that Ratio moves the whole ratio curve.*

**Example:** the factory preset **Dynamic de-harsh** (Dynamics) is that: 4:1 from 2 to 5 kHz and 1:1 elsewhere, Attack 10 ms, Release 120 ms. A mix whose upper mids get 15 dB louder in the chorus comes down about 9 dB there in the chorus only, while the bass, which also gets louder, isn't touched. With one ratio of 4:1 everywhere, the bass would come down too. Click **Learn** in the top row with the chorus playing to fit the threshold to your mix.

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

## Resonance

Resonance listens for narrow peaks that stand above the spectrum around them, such as a ringing drum, a whistling room mode or a harsh vocal formant, and turns each one down while it's there. The rest of the spectrum doesn't move: broadband material, including plain noise, changes by much less than 0.5 dB.

- **Depth:** the most a peak is turned down, 0 to 24 dB. A peak is never pushed below 4 dB above the spectrum around it, so a peak 12 dB up comes down by at most about 8 dB, whatever Depth says.
- **Sharpness:** how narrow a peak must be to count. High values find only narrow rings; low values also take broader bumps.
- **Attack and Release:** how fast the cut arrives and lets go, in ms.
- **Depth curve:** the dashed line on the graph is the most each frequency may be cut, Depth × the curve × Tilt. Click the line to add a node and drag it: up for less cut there, down for more, from 0 % to 200 % of Depth. Alt-click deletes a node, right-click types it, Ctrl-drag moves the whole curve, and **Reset curve** (Range card) makes it flat again.
- **Tilt:** leans Depth towards the highs (positive) or the lows (negative), -100 to +100 %. At +100 % the depth doubles at 16 kHz and halves at 62 Hz; at 0 % it's the same everywhere.
- **Range (Low and High):** Resonance only works between these two, fading out over half an octave outside them. The graph dims what's outside. Low starts at 100 Hz: below that, a 4096-point FFT can't tell a resonance from the slope of the spectrum. For low resonances, raise the FFT size and lower Low.

Both channels get the same cut, so the stereo image never moves. (With Mid/Side on, the mid and the side each get their own cut instead, from their own peaks.) The graph shows the cut in green, hanging from the 0 dB line. **Listen to removed resonances** plays only what's taken out.

## Mid/Side

New in 1.7.1. With **Mid/Side** ticked in EDIT, every mode works on mid (what's the same in both channels, the centre) and side (the difference, the width) instead of left and right, and turns the result back into left and right at the output. Convolve L/R stays left and right, and a mono track is unchanged.

A **Mid | Side** switch appears right after the Mode switch. It picks which one the graph shows and the points edit: the mid and the side each have their own curves in every mode (EQ, threshold, ratio and Resonance's depth curve), and the graph's spectra, reduction and **Learn** in the top row follow the side you're on. The two start as you'd expect: the mid keeps the curves you had in left and right, and the side starts from each mode's default (flat in EQ, -60 dB for the Gate's threshold, -30 dB for the Compressor's). The switch is a view: it isn't saved, and it changes nothing you hear.

In Subtract, one **Learn** captures two noise profiles at once, the mid's and the side's, and Adaptive tracks each on its own. A profile learned before Mid/Side was on is used for the mid only: learn again after turning it on. In Resonance, the mid and the side are each searched for peaks of their own.

Turning Mid/Side on or off changes how the plugin works inside, so it crossfades over the latency (as changing FFT size does) instead of switching at once. The latency doesn't change.

*Mid/Side on, with Side chosen in the switch after the Mode switch, and Ratio chosen over the graph: the side's ratio curve, on its own scale at the right (1:1 to 20:1), with the selected point showing "3.00 kHz · 4.0:1". The side's threshold curve is dimmed and dashed.*

**Example:** a voice in the centre with hiss mostly in the side, as from a wide room mic. In Subtract with Mid/Side on, learn on a section with only the hiss: the side's profile holds most of the hiss and the mid's much less, so the side is cleaned hard while the voice in the centre is judged against its own, much quieter noise and is barely touched. In left and right, both channels would carry the whole hiss, and the voice would pick up faint artefacts.

## Sidechain detector

In Gate, Compressor and Subtract, the plugin's channels 3 and 4 are used to make the decisions. In Resonance it's greyed: Resonance listens to its own input.


- **Gate and Compressor:** compare the sidechain with the thresholds at each frequency. For example, a vocal on the sidechain can carve space for itself in a pad, only at the vocal's frequencies.
- **Subtract:** treats the sidechain as the noise, without a learned profile.
- **With Mid/Side on,** the sidechain is turned into mid and side too, so the mid is compared with the sidechain's mid and the side with its side.

## Listen

**Listen to removed noise** in Subtract, **Listen to gain-reduced part** in Gate and Compressor, or **Listen to removed resonances** in Resonance, plays only what's being taken away. Use it to check you're not removing what you want to keep. It's temporary: it turns off when you close the window, it's never saved in a project or preset, and it isn't automatable. While it's on, the title strip shows **LISTENING**. A project saved with it on in v1.2.0 or earlier opens with it off.

## Convolve L/R

Convolves the left channel with the right. The right channel acts as an impulse response, renewed for each block of FFT-size samples, and the result plays on both outputs. Route the sound to channel 1 and the response to channel 2. The result isn't normalised, so a dense right channel is loud, and the Gain fader is there for that.

## FFT size and latency

The header shows the latency in use, such as **Latency 42.7 ms**, so you can see what an FFT size costs. A larger FFT size gives finer frequency detail and smoother results, but more latency and slower reactions. Low latency halves the latency at the same frequency detail, with a little more smearing.

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
- Curve points: right-click one to type its frequency and gain (a ratio on the ratio curve, a percentage on Resonance's depth curve).
- The graph's range: double-click the top or bottom number.

## EDIT

The `EDIT` link at the right of the footer holds Learn length, Output and, new in 1.7.1, **Mid/Side** (off by default; see Mid/Side). The window is 70 px wider than before 1.7.1, to fit the Mid | Side switch; the graph takes the extra width.

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

