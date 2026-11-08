# Fancy Exciter manual

Fancy Exciter adds brightness and presence. It takes the part of the signal above Freq, drives it into a soft clipper to create new harmonics, and mixes that back in with the original.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| Freq | 500–16000 Hz | 2000 Hz | Only content above this is excited |
| Clip Boost | 0–24 dB | 0.0 dB | How hard the signal drives the clipper. More gives denser harmonics |
| Harmonics | 0–100 % | 0 % | How much of the excited signal is added. At 0 % the plugin changes nothing |
| Mix Back | -inf to +6 dB | -6.0 dB | Overall level of the excited signal |
| Header knob | -24 to +12 dB | 0.0 dB | Output level, also shown in the EDIT drawer |
| ON pill | | on | Bypass, with a short fade. The controls dim while it's off |

Harmonics starts at 0 %, so the plugin is silent in effect until you raise it.

### EDIT drawer

Click **EDIT** to open it and **Done** to close it.

| Control | Range | Default | What it does |
|---|---|---|---|
| Character | 0–100 % | 50 % | 0 % favours even harmonics, which sound warmer. 100 % gives odd harmonics only, which sound edgier |
| Output | -24 to +12 dB | 0.0 dB | The same control as the header knob |
| Oversampling | Off, 2x, 4x, 8x | 4x | Reduces harsh aliasing, at the cost of CPU and latency |

## Oversampling

| Setting | Latency | Worst aliasing in a hard test | CPU (CI runner) |
|---|---|---|---|
| Off | 0 samples | -20.5 dBFS | 0.09 % |
| 2x | 55 samples | -39.7 dBFS | 0.77 % |
| 4x | 69 samples | -114.4 dBFS | 1.37 % |
| 8x | 74 samples | -112.3 dBFS | 2.27 % |

The test is a full-scale 10 kHz sine at 44.1 kHz with Clip Boost at 12 dB and Harmonics at 100 %, which is far harsher than normal material. Stay on 4x unless you need less latency or CPU. 8x adds no audible benefit over 4x.

REAPER compensates for the latency automatically. Changing the setting while playing causes a short dropout of about 50 ms, so choose it before you start rather than automating it.

## Using the knobs

- **Drag** up or down. Hold **Shift** for fine control.
- **Double-click** to reset to the default.
- **Click the value** to type a new one, or **Ctrl-click** anywhere on the control. Enter or clicking away applies it, and Esc cancels. Units are optional, and frequencies can be typed as `2500` or `2.5k`. A number past the limits stops at the limit, and text with no number in it changes nothing.
- **Mouse wheel** to nudge, with **Shift** for smaller steps.
- **Right-click** for the host's automation menu.

## Presets and window size

Use the dropdown to load a preset and **+** to save your own. User presets are stored in `%APPDATA%\Fancy Plugs\Fancy Exciter\Presets`.

**Prm** lists the parameters, sets the UI size (100, 125, 150 or 200 %) and has the About box.

## Menus and dialogs

Menus, tooltips, typed values and prompts are drawn inside the plugin window, in its own style, and follow the UI size. The preset menu groups **Factory** and **User** presets, marks the loaded one, and has **Save as new preset…** (Ctrl+S while the plugin window has focus), **Reload** to discard changes, **Rename…**, **Delete…** and **Open preset folder**. A **•** after the preset's name means something has changed since it loaded.

Typed values take their unit or not: `120`, `120 ms` and `0.12 s` all mean 120 ms, and `2k` means 2000 Hz.

The plugin asks before anything that can't be undone, such as deleting a user preset. The host's own menus, such as REAPER's right-click automation menu, and any bar or buttons the host draws around the plugin window keep the host's look.

## Credits

UI design based on "ReaPlugs (vst GUI)" by Eugene Reznik, CC BY 4.0 — https://www.figma.com/community/file/967866927752067729. Modified from the original.
