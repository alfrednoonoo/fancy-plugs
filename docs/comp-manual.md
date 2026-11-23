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
| Preview filter | off | Lets you hear the filtered detector signal, to set Lowpass and Highpass. It's temporary: it turns off when you close the window, it's never saved in a project or preset, and it isn't automatable. While it's on, the title strip shows **LISTENING**. A project saved with it on in v1.2.0 or earlier opens with it off |
| Listen to removed | off | Plays only what the compression takes off, before make-up, so you hear which parts of the sound it's pulling down: the attack of a snare, the low end of a mix. It's temporary, like Preview filter, and the two switch each other off. Wet, Dry and Output don't apply while it's on |
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

Click **EDIT** at the right of the Detector input row to open it, and **Done** to close it. A switch in its title row picks one of five pages: **Detector**, **Character**, **Upward**, **Options** and **History**. The drawer opens on the page you used last.

### Options

| Control | Default | What it does |
|---|---|---|
| Oversampling | Off | 2x, 4x or 8x reduces aliasing at very fast settings and from Character, at the cost of CPU and latency. With it on, the detector always averages over at least 0.1 ms |
| Oversampling filter | Linear | Linear keeps wet and dry phase-aligned. Minimum has less pre-ringing |
| Release curve | Exponential | Linear falls at a constant dB rate, like older compressors |
| Feedback detection | off | The detector listens to the compressor's own output, for a gentler, more program-dependent response |
| Attack = 90 % rise | off | See below |
| Link amount | 100 % | How strongly linked channels follow the louder one |
| Output | 0.0 dB | The same control as the header knob |
| Hold | 0.0 ms | The release waits this long after the last peak. With fast settings and peak detection, the gain otherwise follows each cycle of a low note, which is distortion: a Hold a little longer than the note's period (20 ms for 50 Hz) keeps it steady. The upward lift doesn't hold |
| Range | Off | The most gain reduction Comp will apply, 1 to 40 dB; Off is the top of the knob. With Auto threshold it makes a level rider that never pulls a passage down by more than Range. Auto make-up then adds back half of what Range allows, not half of what the curve would take |

Aliasing only matters with very fast settings on bright material. In a harsh test (15 kHz sine, attack 0.1 ms, release 1 ms), the worst alias is -56.8 dBFS with oversampling off, -79.2 dBFS at 2x and -100.9 dBFS at 4x.

### Detector

Three bands shape what the detector hears, after Highpass and Lowpass: a **low shelf**, a **bell** and a **high shelf**, each with Freq and Gain, and Q on the bell. They never touch the audio you hear. A band at 0 dB is off. Boost a band to make the compressor react more to that range, as a de-esser does with the bell on sibilance, or cut the low shelf so kick drums stop pumping a mix. **Preview filter** plays the whole detector chain, bands included.

The curve at the top draws the detector's response: Highpass and Lowpass dimmer, the bands brighter. Drag a handle to set its band's Freq and Gain, and use the mouse wheel for the bell's Q.

**Auto threshold** keeps the gain reduction steady when the input gets louder or quieter, for example between a quiet verse and a loud chorus, or across songs at different levels. It measures the detector's long-term level (about 3 s) and moves Threshold and Up threshold with it, so Threshold is set as if the input sat at -18 dBFS RMS. At -18 dBFS nothing changes; 6 dB louder moves both thresholds up 6 dB. Silence and pauses don't pull it down: below -60 dBFS, or 40 dB under the long-term level, it holds. The offset shows next to the checkbox, and the Threshold meter draws the threshold in use as a thin white line under the handle. The offset is limited to ±24 dB.

### Character

A saturation stage inside the compressor, using Fancy Sature's curves.

| Control | Default | What it does |
|---|---|---|
| Type | Off | **Warm**: a soft, asymmetric curve with the lows tilted into it, so even harmonics and a thicker low end. **Bright**: a harder, symmetric knee with the highs tilted into it, so odd harmonics and edge |
| Position | Post | **Pre** saturates before the compressor, so the compressor hears the rounded peaks and reacts a little less. **Post** saturates after make-up gain |
| Drive | 6.0 dB | How hard the curve is driven. Quiet material passes untouched, so turning Drive up rounds the peaks rather than making everything louder: at Drive 12 dB, peaks above about -12 dBFS are rounded and held near that level. Lower Threshold or raise Wet if the result is quieter than you want |

The graph shows the curve for the current Type and Drive, input against output. Changing Type or Position while playing fades the stage out and back in over 20 ms, without a click. Use 4x or 8x oversampling with high Drive on bright material, to keep aliasing down.

### Upward

Upward compression raises quiet passages, as in Fancy MultiComp: below **Up threshold** the level is raised by **Up ratio** (1:2 lifts a signal 10 dB under the Up threshold by 5 dB), up to **Max lift**. Below **Floor** nothing is raised, and the lift fades out over the 8 dB above it, so noise and silence stay where they are. Up threshold is held at or below Threshold. The lift uses Attack and Release (and Auto release) like the downward part.

The graph draws the whole static curve, downward in teal and upward in green, with a dot at the current level. The GR meter still shows only the reduction.

### History

The last seconds of what Comp did, oldest on the left: the input waveform in grey, the output in teal over it (before make-up, so the gap between them is what was taken off), both in dB, from −60 dB at the centre line to 0 dB at the top and bottom edges, with faint lines every 12 dB and their values at the left. On this scale quiet passages, breaths and room tone stay visible, and the gap between grey and teal is the reduction in dB. The gain reduction is a yellow line from the top (0 to 24 dB, scale on the right; unlinked, the right channel is a lighter second line), and the threshold in use is a dashed pair of lines on the same dB scale as the waveforms.

- **1.5 s, 3 s, 6 s, 12 s** sets how much time is shown.
- **Grid** draws beat lines, and bar lines brighter, from REAPER's tempo while it plays.
- **Click the graph** to pause it, and again to go on. The audio carries on.
- **peak** at the top right is the largest reduction in view; click it to clear it.

The history keeps recording while the drawer is closed, so it's current when you open it.

## Attack and release times

Attack and release are time constants: the time to cover about 63 % of the distance to the target. Compressors that quote the time to reach 90 % will seem about 2.3 times faster at the same number. Tick **Attack = 90 % rise** to use that convention for Attack.

## Latency

| Setting | Latency |
|---|---|
| Base | 0 samples |
| Oversampling 2x | 55 samples, FIR |
| Oversampling 4x | 69 samples, FIR |
| Oversampling 8x | a few samples more than 4x, FIR (shown in REAPER) |
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

Some settings change the plugin's latency: Oversampling. The plugin reports each change to the host, which delays the other tracks to match. Some hosts only read the new latency when playback stops and starts, so change these settings while stopped, or stop and start once after changing them; the tooltip says so too. In a host with no delay compensation, the track plays late by the latency, and nothing else goes wrong.

### Rendering and export

Offline renders, Audacity's Apply, freezing and exporting all sound the same as playback. When the host renders faster than real time, the plugin does the work it would otherwise do in the background before each block, so nothing is skipped.

## Host sync (Phase 4)

**Detector input > Host sync** ducks on the song's beat with no sidechain kick: the detector hears a virtual kick on every step of the host's grid, so Threshold, Ratio, Attack, Release, Hold and Range shape the duck as they would with a real kick. On the Options page (EDIT > Options):

- **Rate:** the grid, 1/1 to 1/16, each also dotted and triplet. 1/4 is a duck on every beat.
- **Shape:** how long each virtual kick lasts, 5 to 100 % of the step. Release shapes the duck most; Shape matters most with short Releases.

While the song is stopped it keeps ducking at the last tempo, so you can set it up, and lines up with the beat again when you press play. If the host sends no tempo it runs at 120 BPM and the Options page says so.

## Menus and dialogs

Menus, tooltips, typed values and prompts are drawn inside the plugin window, in its own style, and follow the UI size. The preset menu groups **Factory** and **User** presets, marks the loaded one, and has **Save as new preset…** (Ctrl+S while the plugin window has focus), **Reload** to discard changes, **Rename…**, **Delete…** and **Open preset folder**. A **•** after the preset's name means something has changed since it loaded.

Typed values take their unit or not: `120`, `120 ms` and `0.12 s` all mean 120 ms, and `2k` means 2000 Hz.

The plugin asks before anything that can't be undone, such as deleting a user preset. The host's own menus, such as REAPER's right-click automation menu, and any bar or buttons the host draws around the plugin window keep the host's look.

## Undo, A/B and Compare

The header has, between the I/O pill and the output knob:

- **↶ Undo** and **↷ Redo** step back and forward through your changes: each drag, click, typed value or preset load is one step, up to 100. Their tooltips name the step. Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y do the same while the plugin window has the keyboard focus; in REAPER the host usually keeps it, so use the buttons. Automation, and changes the host makes (REAPER's own undo, loading a project), aren't steps. The history isn't saved with the project.
- **A | B** holds two complete sets of settings. Click the other letter to switch: the plugin glides to the other settings. Both sets, and which one is active, are saved with the project. Each starts as a copy of the other. **→** copies the active set into the other one. Switching and copying are undo steps.
- **≈ Compare** plays the input instead of the output, at the volume that matches the output's loudness, so you hear what the plugin does without a change in loudness fooling you. While it's on, the button is lit and shows the matching gain, such as **+2.4 dB**. The match is measured while Compare is off, over the last 3 seconds, and holds while it's on; it stays within ±24 dB, and isn't updated while either signal is quieter than -70 LUFS. Compare isn't a parameter: it can't be automated, isn't saved and turns off when you close the window. It's separate from **ON**.


## Credits

UI design based on "ReaPlugs (vst GUI)" by Eugene Reznik, CC BY 4.0 — https://www.figma.com/community/file/967866927752067729. Modified from the original.

## Optical (Phase 4)

**EDIT > Options > Optical** gives Comp the ballistics of an optical cell, in place of its usual follower and Classic:

- **Attack follows the overshoot:** the further over the threshold, the faster it closes. Attack is the time at about 12 dB over; a gentle 3 dB over takes about twice as long, so peaks are caught quickly and gentle overs gently.
- **Two-stage release:** half the reduction lets go at a tenth of Release, the rest at Release.
- **Memory:** after long, heavy compression (more than 3 dB for seconds) the slow stage recovers up to four times more slowly, as a cell does, and the effect fades over a few seconds.

While it's on, Classic and Auto release dim and don't apply. Weird knee, Hold and Range still work.

