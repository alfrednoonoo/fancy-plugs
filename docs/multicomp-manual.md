# Fancy MultiComp: manual

A multiband compressor with up to 8 bands. Each band has its own compressor, and the bands sum back flat when they aren't compressing.

## The graph

Frequency runs from left to right. The vertical scale is level, from 0 to -60 dB. Each band is a coloured region up to its threshold, and the orange line is the input's spectrum, so you can see how far each band's level is above its threshold. Yellow bars hanging from the top show each band's gain reduction, at 3.2 pixels per dB.

| Gesture | What it does |
|---|---|
| Drag a band's handle up or down | Sets Threshold |
| Mouse wheel on a handle, or Alt-drag it | Sets Ratio |
| Drag a blue line | Moves the crossover. It stops 1/3 octave from its neighbours |
| Double-click a handle | Resets the band, keeping its frequency range |
| Double-click an empty spot | Splits the band there. Both halves keep its settings, so the sound doesn't change |

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
| Auto make-up | off/on | Adds half the gain reduction a 0 dBFS level would get. It comes from Threshold, Ratio and Knee, so it can't pump. The amount shows in brackets after Gain |
| Auto release | off/on | Recovers fast from transients and slowly from sustained compression |
| Feedback detect | off/on | The detector listens to the band's output |
| Sidechain | off/on | This band listens to the sidechain, filtered to the same range |

Attack and Release are time constants: the time to reach about 63 % of the target. Attack = 90 % rise in EDIT makes Attack the time to reach 90 %.

The gain in a band is always stereo-linked, set by the louder channel, so the band's image holds.

## Detector and sidechain

In REAPER, a sidechain arrives on the plugin's channels 3 and 4.

The **Detector** in the header picks what the bands listen to. With a Main option, bands hear the main input, and a band with **Sidechain** ticked hears the sidechain. With a Sidechain option, every band hears the sidechain.

To duck only the bass of a mix from a kick, tick Sidechain on the lowest band and leave the rest off.

## Link bands

Moving Threshold, Ratio, Attack, Release or Gain on one band moves the others by the same distance, so their offsets stay until one reaches a limit. Hold Alt to edit one band. On the graph, dragging any handle moves all the thresholds, and Alt-drag moves all the ratios. It's for your own moves only, so automation and presets aren't affected.

## Crossover

| Crossover | Latency at 48 kHz | Use |
|---|---|---|
| LR 24 dB · Min phase | None | The default. Gentle overlap between bands |
| LR 48 dB · Min phase | None | Sharper bands, with a little more phase shift around the crossovers |
| Linear phase | 2559 samples, 53 ms | For mastering. No phase shift at all |

All three sum flat. Below about 150 Hz, Linear phase's bands overlap more than the LR types, because its transition is about 75 Hz wide at any frequency.

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

The `EDIT` link at the bottom right holds Attack = 90 % rise, Log-scale automated freq. and Output.

## Latency

| Crossover | Limit output off | Limit output on |
|---|---|---|
| LR 24, LR 48 | 0 | 104 samples at 48 kHz |
| Linear phase | 2559 samples at 48 kHz | 2663 samples at 48 kHz |

The plugin reports this to REAPER, and changing the crossover or Limit output fades the sound briefly.

## Menus and dialogs

Menus, tooltips, typed values and prompts are drawn inside the plugin window, in its own style, and follow the UI size. The preset menu groups **Factory** and **User** presets, marks the loaded one, and has **Save as new preset…** (Ctrl+S while the plugin window has focus), **Reload** to discard changes, **Rename…**, **Delete…** and **Open preset folder**. A **•** after the preset's name means something has changed since it loaded.

Typed values take their unit or not: `120`, `120 ms` and `0.12 s` all mean 120 ms, and `2k` means 2000 Hz.

The plugin asks before anything that can't be undone, such as deleting a user preset. The host's own menus, such as REAPER's right-click automation menu, and any bar or buttons the host draws around the plugin window keep the host's look.
