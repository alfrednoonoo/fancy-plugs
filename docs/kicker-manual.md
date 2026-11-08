# Fancy Kicker manual

Fancy Kicker adds a focused low-frequency boost to a kick or bass. It filters a narrow band around Freq and mixes it back in with the original signal.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Freq | 20–240 Hz | 50.0 Hz | Centre of the boosted band |
| Wet | -inf to +12 dB | -12.0 dB | Level of the filtered band |
| Dry | -inf to +12 dB | 0.0 dB | Level of the original signal |
| Header knob | -24 to +12 dB | 0.0 dB | Output level, also shown in the EDIT drawer |
| ON pill | | on | Bypass, with a short fade. The controls dim while it's off |

### EDIT drawer

Click **EDIT** to open it and **Done** to close it.

| Control | Range | Default | What it does |
|---|---|---|---|
| Filter Q | 0.5–10 | 2.00 | Width of the band. Higher is narrower and rings longer |
| Output | -24 to +12 dB | 0.0 dB | The same control as the header knob |
| Mono below Freq | off/on | off | Sums the input to the filter to mono, so the added low end is identical in both channels. The dry signal stays stereo |
| Soft clip | off/on | off | Rounds peaks at the output so they can't exceed 0 dBFS. It starts acting at about -3 dBFS |

## Using the knobs

- **Drag** up or down. Hold **Shift** for fine control.
- **Double-click** to reset to the default.
- **Click the value** to type a new one, or **Ctrl-click** anywhere on the control. Enter or clicking away applies it, and Esc cancels. Units are optional, and frequencies can be typed as `2500` or `2.5k`. A number past the limits stops at the limit, and text with no number in it changes nothing.
- **Mouse wheel** to nudge, with **Shift** for smaller steps.
- **Right-click** for the host's automation menu.

## Presets and window size

Use the dropdown to load a preset and **+** to save your own. User presets are stored in `%APPDATA%\Fancy Plugs\Fancy Kicker\Presets`.

**Prm** lists the parameters, sets the UI size (100, 125, 150 or 200 %) and has the About box.

## Latency and phase

Latency is 0 samples at all settings. The filtered signal is minimum-phase, so it stays aligned with the dry signal at Freq, apart from a small phase lead from a 15 Hz filter that keeps DC and infrasonic energy out of the wet path. At 50 Hz, wet and dry at equal level sum to about +5.8 dB. The sum is smaller at the bottom of the Freq range.

## Using it in other hosts

### Rendering and export

Offline renders, Audacity's Apply, freezing and exporting all sound the same as playback. When the host renders faster than real time, the plugin does the work it would otherwise do in the background before each block, so nothing is skipped.

## Menus and dialogs

Menus, tooltips, typed values and prompts are drawn inside the plugin window, in its own style, and follow the UI size. The preset menu groups **Factory** and **User** presets, marks the loaded one, and has **Save as new preset…** (Ctrl+S while the plugin window has focus), **Reload** to discard changes, **Rename…**, **Delete…** and **Open preset folder**. A **•** after the preset's name means something has changed since it loaded.

Typed values take their unit or not: `120`, `120 ms` and `0.12 s` all mean 120 ms, and `2k` means 2000 Hz.

The plugin asks before anything that can't be undone, such as deleting a user preset. The host's own menus, such as REAPER's right-click automation menu, and any bar or buttons the host draws around the plugin window keep the host's look.

## Undo, A/B and Compare

**A | B** in the header holds two complete sets of settings. Click the other letter to switch: the plugin glides to the other settings. Both sets, and which one is active, are saved with the project. Each starts as a copy of the other.

The **Prm** menu has the rest:

- **Undo** and **Redo** step back and forward through your changes: each drag, click, typed value or preset load is one step, up to 100. Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y do the same while the plugin window has the keyboard focus; in REAPER the host usually keeps it, so use the menu. Automation, and changes the host makes (REAPER's own undo, loading a project), aren't steps. The history isn't saved with the project.
- **Copy A → B** (or B → A) copies the active set into the other one.
- **≈ Compare (level-matched)** plays the input instead of the output, at the volume that matches the output's loudness, so you hear what the plugin does without a change in loudness fooling you. The match is measured while Compare is off, over the last 3 seconds, and holds while it's on. It isn't a parameter: it can't be automated, isn't saved and turns off when you close the window. It's separate from **ON**.
- **I/O** shows the plugin's inputs and outputs.


## Credits

UI design based on "ReaPlugs (vst GUI)" by Eugene Reznik, CC BY 4.0 — https://www.figma.com/community/file/967866927752067729. Modified from the original.
