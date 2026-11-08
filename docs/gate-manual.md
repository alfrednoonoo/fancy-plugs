# Fancy Gate: manual

A noise gate and ducker with hysteresis, hold, optional 50 ms lookahead, a sidechain input and MIDI triggering.

## How it opens and closes

The gate opens when the detector's level reaches **Threshold**. It closes only when the level falls below Threshold plus **Hysteresis**, the dashed line, so a signal hovering around the threshold doesn't make it chatter. After the level falls below that line, **Hold** keeps the gate open, then **Release** closes it.

Opening takes **Attack**. At 0 ms the gate still fades over 0.5 ms, or 32 samples above 64 kHz, so it never clicks. Release has the same minimum.

A gate that's closing re-opens on the next hit. A gate that's open doesn't re-trigger until the level has dropped below Threshold again.

## The display

| Part | What it shows |
|---|---|
| Threshold meter | The input level, with the Threshold handle over it. The dashed line is the close level. Below it are Threshold and the close level in dB |
| Gain | How far the gate turns the signal down right now, and the light: **open** (yellow), **hold** (orange) or **closed** (grey) |
| History | The last 3 s: the input level as a grey fill and the gate's gain as a yellow line, with the open and close levels. Click or drag on it to set Threshold |

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Threshold | -90 to 0 dB | -40 dB | Level where the gate opens |
| Hysteresis | -24 to +12 dB | -6 dB | Where it closes, relative to Threshold |
| Pre-open | 0-50 ms | 5 ms | How early the gate starts to open, before the hit. Needs Lookahead |
| Attack | 0-500 ms | 0.5 ms | How fast it opens |
| Hold | 0-2000 ms | 20 ms | How long it stays open after the level falls |
| Release | 1-5000 ms | 100 ms | How fast it closes |
| Range | -inf to 0 dB | -inf | How far the closed gate turns down. -inf is silence |
| RMS size | 0-250 ms | 0 ms | How long the detector averages. 0 is peak detection |
| Lowpass, Highpass | 20 Hz-22 kHz | Off | Filters on the detector only. A Highpass at 100 Hz stops bass spill opening a vocal gate |
| Detector input | Main or Sidechain, L+R, L or R | Main input L+R | What the gate listens to |
| Lookahead | On or off | Off | Delays the audio by 50 ms, so the gate can open before the hit. Adds 50 ms latency, which REAPER compensates |
| Mode | Gate or Duck | Gate | Duck turns the signal down by Range while the detector is over Threshold |
| Preview filter | On or off | Off | Plays what the detector hears, through Lowpass and Highpass |
| Link channels | On or off | On | Both channels open together, on the louder one |
| Wet, Dry | -inf to +12 dB | 0 dB, -inf | The gated signal and the untouched signal |
| Noise | -inf to 0 dB | -inf | White noise, only while the gate is open. In Duck mode, while it ducks |

Every knob and fader takes typed values: click the value, or Ctrl-click the control. Frequencies accept `2500` and `2.5k`, and the MIDI note accepts `A4`, `C#3` or a number.

## Pre-open

Pre-open only works with Lookahead on. Without it, the knob is dimmed, reads "off" and can't be changed. Clicking it explains why and makes the Lookahead checkbox flash. It never turns Lookahead on for you, because that changes the latency.

With Lookahead on, the latency is always 50 ms. Pre-open only sets how early within that the gate opens, so moving it never changes the latency.

## Sidechain in REAPER

1. On the track that should trigger the gate, open the routing window and add a send to the gated track.
2. Set the send to go to **channels 3/4** of the gated track.
3. On the gated track, set the track's channel count to 4.
4. In Fancy Gate, set **Detector input** to **Sidechain L+R**.

If a Sidechain source is chosen and nothing arrives for 2 s while the main input plays, the history shows "No sidechain signal: check track routing (REAPER: pins 3/4)".

## MIDI

Tick **MIDI on open/close** to send a note when the gate opens and its note-off when it closes. The dropdown beside it sets the note, the channel and the velocity window. To play an instrument from it in REAPER, add a send from the gate's track to the instrument's track with MIDI only, or put the instrument after Fancy Gate on the same track.

- **Velocity from level off:** every note has velocity 100, and lands exactly where the gate is heard opening.
- **Velocity from level on:** the velocity is the hit's peak over the velocity window after the gate opens, from -60 dBFS (1) to 0 dBFS (127). Without Lookahead, the note goes out one window late, 2 ms by default, because the peak isn't known until then. With Lookahead, it's on time unless Pre-open is within one window of 50 ms. The checkbox's tooltip gives the delay whenever there is one.

Turning MIDI off, or changing Lookahead, ends a sounding note.

## Factory presets

| Preset | For |
|---|---|
| Default | A clean start: Threshold -40, hard gating |
| Vocal clean-up | Gentle -18 dB gating of a vocal's pauses, with a 100 Hz detector high-pass |
| Kick tight | A short kick, listening below 150 Hz |
| Snare (with spill) · lookahead | Snare with hi-hat spill, opening 2 ms early |
| Tom (with spill) · lookahead | Toms with cymbal spill, listening below 400 Hz |
| Duck under kick | Ducks by 8 dB from a kick on the sidechain |
| Drum trigger to MIDI | Notes with velocity from each hit |
| Gentle expander | -12 dB of downward expansion |

## Latency

0 samples with Lookahead off. 50 ms with it on, at every sample rate.

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

### MIDI out in other hosts

Gate's MIDI on open/close needs a host that routes MIDI out of an audio effect. Where a host doesn't, the gate still gates and nothing else changes.

| Host | MIDI out of Gate |
|---|---|
| REAPER | untested |
| Audacity | no MIDI routing for effects |
| Tracktion Waveform | untested |
| UA LUNA | untested |
| JUCE AudioPluginHost | untested |
| Ableton Live | Live's audio effects can't send MIDI out |
| Cubase | untested |

The table is filled in from the host checks in `docs/host-compat.md`.

### Latency

Some settings change the plugin's latency: Lookahead, which adds 50 ms. The plugin reports each change to the host, which delays the other tracks to match. Some hosts only read the new latency when playback stops and starts, so change these settings while stopped, or stop and start once after changing them; the tooltip says so too. In a host with no delay compensation, the track plays late by the latency, and nothing else goes wrong.

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

