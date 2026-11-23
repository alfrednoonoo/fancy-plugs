# Fancy MultiComp: manual

A multiband compressor with up to 8 bands. Each band has its own compressor, and the bands sum back flat when they aren't compressing.

## The graph

Frequency runs from left to right. The vertical scale is level, from 0 to -60 dB. Each band is a coloured region up to its threshold, and the orange line is the input's spectrum, so you can see how far each band's level is above its threshold. Yellow bars hanging from the top show each band's gain reduction, at 3.2 pixels per dB.

| Gesture | What it does |
|---|---|
| Drag a band's handle up or down | Sets Threshold |
| Mouse wheel on a handle, or Alt-drag it | Sets Ratio |
| Drag a blue line | Moves the crossover. It stops 1/3 octave from its neighbours |
| Right-click a blue line | Picks that crossover's slope, LR 24 or LR 48 dB/oct (see Crossover). Dimmed in Linear and Dynamic phase |
| Double-click a handle | Resets the band, keeping its frequency range |
| Double-click an empty spot | Splits the band there. Both halves keep its settings, so the sound doesn't change |

The **Spectrum | GR history** switch at the graph's top left swaps the graph for the GR history (see below) and back. It's a view, not a setting: it isn't saved and can't be automated.

## GR history

With **GR history** chosen, the graph shows each band's gain reduction over the last 3 seconds, one lane per band in its colour, newest on the right. The trace hangs down from the lane's top line as the band turns the level down (12 dB fills a lane with four bands), and the number at the right of each lane is that band's largest reduction in the 3 seconds shown. It's recorded only while the plugin window is open, so it costs nothing when it's closed. **Learn** steps aside while the history shows, and the handles and crossover lines can't be dragged until you switch back to **Spectrum**.

## Bands

The numbered tabs select a band, and `+` splits the selected band in half. On the right, `S` solos the band, the power icon bypasses its compressor, and `×` deletes it, giving its range to the band above. One band always remains. A bypassed band stays in the mix, and only soloed bands are heard while any band is soloed.

## Band controls

| Control | Range | What it does |
|---|---|---|
| Threshold | -60 to 0 dB | Level where the band starts to act |
| Ratio | 0.5:1 to ∞:1 | Above 1:1 compresses. Below 1:1 raises the level above the threshold, by up to +24 dB |
| Knee | 0-24 dB | How gradually it starts |
| Attack | 0-500 ms | How fast gain reduction arrives |
| Release | 0-5000 ms | How fast it goes away |
| RMS size | 0-250 ms | How long the detector averages. 0 is peak detection |
| Gain | -24 to +24 dB | The band's make-up gain |
| Look | 0-10 ms | The band's lookahead: its detector hears the audio this much early, so its gain starts moving before a transient arrives. Every band's audio waits for the largest Look, so the bands stay lined up and the latency is the largest Look. A band with sharp hits (a snare in the highs) gains most from 1 to 2 ms; a band whose notes swell in (the bass) needs none, and more than it needs turns the sound before each hit down early |
| Auto make-up | off/on | Adds half the gain reduction a 0 dBFS level would get. It comes from Threshold, Ratio and Knee, so it can't pump. The amount shows in brackets after Gain |
| Auto release | off/on | Recovers fast from transients and slowly from sustained compression |
| Feedback detect | off/on | The detector listens to the band's output |
| Sidechain | off/on | This band listens to the sidechain, filtered to the same range |

Attack and Release are time constants: the time to reach about 63 % of the target. Attack = 90 % rise in EDIT makes Attack the time to reach 90 %.

The gain in a band is always stereo-linked, set by the louder channel, so the band's image holds.

## Upward compression

Each band can lift quiet material as well as push down loud material. The **UPWARD** row under the band's controls holds:

| Control | Range | Default | What it does |
|---|---|---|---|
| Upward on | | off | Turns the band's upward part on |
| Up thresh | -80 to 0 dB | -40 dB | Signals below it are lifted. Set above the band's Threshold, it acts as the Threshold, and the curve says "clamped" |
| Up ratio | 1:1 to 1:10 | 1:2 | At 1:2, a signal 20 dB below Up thresh is lifted by 10 dB. Type `2` or `1:2` |
| Max lift | 0 to 30 dB | +12 dB | The most gain it ever adds |
| Floor | -100 to -40 dB | -70 dB | No lift below it, fading in over the 8 dB above it |

Both parts use the band's Attack and Release. The small curve at the right of the row plots input level against output level, with the band's whole static curve (downward and upward, scaled by Depth) and guides at Floor (dotted), Up thresh (dashed) and Threshold (solid).

**Upward compression raises the noise floor by design.** Hiss, room tone and bleed between notes get louder along with the quiet parts you want. Floor is the safety: set it just above the noise, and anything quieter is left alone. Silence stays silent.

## Depth and Time

Two small knobs at the top of the right column act on every band:

- **Depth** (0-100 %, default 100 %) scales every band's gain change, downward and upward, and Auto make-up with it. At 0 % nothing changes the level, and you hear the bands summed, as Dry.
- **Time** (10-1000 %, default 100 %) multiplies every band's Attack and Release: 200 % makes everything twice as slow.

Together with the **Big squash** preset (three bands, 4:1 down above -18 dB and 1:4 up below -40 dB, fast attack) they make the heavy, squashed multiband sound quick to dial in: start there and bring Depth down until it's enough.

## Detector and sidechain

In REAPER, a sidechain arrives on the plugin's channels 3 and 4.

The **Detector** dropdown in the header, beside the undo and A/B controls, picks what the bands listen to. With a Main option, bands hear the main input, and a band with **Sidechain** ticked hears the sidechain. With a Sidechain option, every band hears the sidechain.

To duck only the bass of a mix from a kick, tick Sidechain on the lowest band and leave the rest off.

## Link bands

Moving Threshold, Ratio, Attack, Release or Gain on one band moves the others by the same distance, so their offsets stay until one reaches a limit. Hold Alt to edit one band. On the graph, dragging any handle moves all the thresholds, and Alt-drag moves all the ratios. It's for your own moves only, so automation and presets aren't affected.

## Crossover

| Crossover | Latency at 48 kHz | Use |
|---|---|---|
| LR 24 dB · Min phase | None | The default. Gentle overlap between bands |
| LR 48 dB · Min phase | None | Sharper bands, with a little more phase shift around the crossovers |
| Linear phase | 2559 samples, 53 ms | For mastering. No phase shift at all |
| Dynamic phase | None | The audio isn't split: while the bands aren't changing anything it passes untouched, with no phase shift at all, and phase appears only around crossovers whose bands differ, only while they do |

With LR 24 or LR 48, each crossover can have its own slope: right-click its blue line on the graph, or use the **Crossover slopes** rows in EDIT. Mixed slopes still sum flat. Picking LR 24 or LR 48 in the Crossover menu sets every crossover to that slope; Linear and Dynamic phase keep their own shapes and leave the slopes as they were, ready for when you go back.

All four sum flat. In **Dynamic phase** the band gains are applied as gentle shelves at the crossovers, so a band's shape is within about 0.5 dB of LR 24's for moves of 6 dB, and Dry is the input itself. A shelf can't silence a band, so while a band is soloed or muted MultiComp uses the LR 24 bands, switching over 20 ms. Below about 150 Hz, Linear phase's bands overlap more than the LR types, because its transition is about 75 Hz wide at any frequency.

## Wet and Dry

Dry is the input after the crossover but before compression, so it stays in phase with Wet and parallel compression doesn't comb. The default is Wet at 0 dB and Dry at -inf.

## Using it as a multiband limiter

1. Set each band's Ratio to `inf`, Attack to 0 ms, RMS size to 0 ms and Knee to 0 dB.
2. Set the thresholds for how much each band should be held.
3. Tick **Limit output**, which adds a true-peak brickwall limiter after the bands.

The **Multiband Limiter** preset starts from this.

## Limit output

A true-peak limiter with a fixed ceiling at 0 dBFS. It adds 2.2 ms of latency.

## Typing values

To type a value, click it, or Ctrl-click anywhere on a knob or fader. Enter or clicking away applies it, and Esc cancels. Units are optional. Text with no number in it changes nothing, and a number past the limits stops at the limit.

- Ratio accepts `3`, `3:1`, `0.5:1`, `1:2` and `inf`. Here `a:b` means `a / b`, because 1:2 is expansion.
- Times are in milliseconds.

Crossovers are drawn on the graph, so they're dragged rather than typed. The header knob has no value text, so it opens with Ctrl-click. Output in EDIT is the same control.

## EDIT

The `EDIT` link at the bottom right holds Attack = 90 % rise, Log-scale automated freq., Output, Set all bands, the selected band's detector EQ and the crossover slopes.

- **Set all bands** (0 to 10 ms): turning it sets every band's **Look** to its value at once, and a band you add or reset starts there. The latency in samples is shown under it. Each band's own Look, in the band row, is what counts; host automation of this knob doesn't move the bands.
- **Detector EQ** (the selected band, named in its colour): **HP** and **LP** filter what the band's detector hears, at 24 dB/oct, never the band's audio. HP fully left and LP fully right are Off, and Off isn't run. **Tilt** (-6 to +6 dB/oct) tips the detector's hearing around the middle of the band's range: up makes its highs drive the compression more, down its lows. Example: in a low band with a kick at 50 Hz and a bass at 100 Hz, HP at 80 Hz lets the bass set the gain, so the kick passes with about half the reduction. It follows the band tab you select.
- **Crossover slopes**: a row per crossover, named by the bands it splits and its frequency, with its slope, LR 24 or LR 48 dB/oct. The same choice as right-clicking the crossover's line. With more than four crossovers the rows go into two columns. Dimmed in Linear and Dynamic phase.

Projects saved before these settings existed open with every band's Look at their old Lookahead and every slope at their old Crossover type, so they sound as they did.

## Channel

The chip on the band row, left of **S**, picks what the band compresses: **ST** (both channels, as before), **L**, **R**, **M** (mid, the sum) or **S** (side, the difference). A one-channel band leaves the other channel of its frequency range as it is: a Mid band on the lows tightens the centre without touching the width, and a Side band on the highs tames harsh stereo edges. The band's detector reads the same channel, from the sidechain too. The band's handle on the graph shows the letter. On a mono track the chip is greyed.

## Phase 4 additions

- **Learn** (graph, top right): play the track and click it. After 5 s the crossovers move so each band carries a similar share of the music's energy, weighted like pink noise, so the lows don't take every band. One undo step.
- **M** (band row): mutes the band, taking it out of the mix entirely. The power button still keeps a band in the mix, unprocessed.

## Latency

| Crossover | Limit output off | Limit output on |
|---|---|---|
| LR 24, LR 48 | 0 | 104 samples at 48 kHz |
| Linear phase | 2559 samples at 48 kHz | 2663 samples at 48 kHz |

The largest band's Look adds its own time on top: 5 ms is 240 samples at 48 kHz.

The plugin reports this to REAPER, and changing the crossover or Limit output fades the sound briefly.

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

Some settings change the plugin's latency: a Linear crossover, Limit output, and the largest band's Look. The plugin reports each change to the host, which delays the other tracks to match. Some hosts only read the new latency when playback stops and starts, so change these settings while stopped, or stop and start once after changing them; the tooltip says so too. In a host with no delay compensation, the track plays late by the latency, and nothing else goes wrong.

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

Splitting, merging, adding and deleting bands are undo steps too, and with Link bands on, one drag that moves every band is one step.

