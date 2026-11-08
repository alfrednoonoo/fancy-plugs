# Fancy Delay: manual

Up to 16 delay taps, each with its own time, feedback, filters and place in the stereo field.

## The tap map

The top of the window shows time from left to right, with beats marked and times in ms. The white bar is the dry signal. Each tap is a coloured bar, and faded bars are its feedback repeats. The axis covers four beats and extends for longer taps.

| Gesture | What it does |
|---|---|
| Drag a handle left or right | Sets the time. In Note mode it snaps to note lengths |
| Shift-drag a handle | Sets a free time, and moves the tap to ms mode |
| Drag a handle up or down | Sets Volume |
| Click a handle | Selects the tap |

## Tabs

The numbered tabs select a tap, and `+` adds one after it. On the right, `S` solos the tap, the power icon turns it off, and `×` deletes it. One tap always remains.

While any tap is soloed, the others are silent but keep running, so their tails are still there when you unsolo. A tap that's off takes no input and makes no sound.

## Time

- **ms:** Length is a time, from 0 to 10,000 ms. 0 ms is one sample, because a loop can't be shorter.
- **Note:** the Length knob or the dropdown picks a note value from 1/1 to 1/64. The switch below picks straight, dotted or triplet. The time follows the host's tempo, and the line under the switch shows the result. A multiplier from ×1 to ×16 is in EDIT.

Note lengths stop at 10 seconds. If the host gives no tempo, the plugin keeps the last one it had, starting at 120 BPM.

**Smooth time changes** decides what happens when a time moves. On, the old and new times crossfade over 30 ms, so there's no pitch change. Off, the delay glides to the new time, and the pitch bends like tape.

## Tap controls

| Control | Range | What it does |
|---|---|---|
| Feedback | 0-100 %, or 110 % with Saturate on | How much of each repeat comes back |
| Lowpass | 200 Hz-22 kHz, Off at the top | Darkens the tap |
| Highpass | 20 Hz-5 kHz, Off at the bottom | Thins the tap |
| Resolution | 4-24 bit, off at 24 | Reduces bit depth for a lo-fi sound |
| Width | 0.00-2.00 | 0 is mono, 1 is unchanged, above 1 is wider |
| Pan | 100 L-100 R | Balance, so the centre is unchanged and a hard pan silences the other side |
| Volume | -inf to +12 dB | The tap's level |

Feedback above 100 % only applies with Saturate on. If it's set higher and Saturate is off, the knob keeps its position but the tap runs at 100 %.

## Footer

These follow the selected tap, except Smooth time changes, which applies to the whole plugin.

- **Filters inside feedback loop:** on, each repeat goes through the filters and Resolution again, so repeats get darker as they fade. Off, every repeat is filtered once and keeps the same tone.
- **Ping-pong:** repeats alternate left and right. The input is summed to mono, and Pan chooses the starting side, with the centre starting left.
- **Saturate feedback:** adds a soft clipper to the tap's output and loop, including the first echo. It keeps the tap below 0 dBFS at any feedback, and allows up to 110 %.

With Feedback above 0 % and Saturate off, a safety curve keeps a loop from running away. It only touches peaks above -0.9 dBFS.

## Wet and Dry

Wet sets the level of all the taps, and Dry the level of the input. They're independent, so Dry at -inf gives only the delayed signal. For use on a send, set Dry to -inf.

## Typing values

To type a value, click it, or Ctrl-click anywhere on a knob or fader. Enter or clicking away applies it, and Esc cancels. Units are optional. Text with no number in it changes nothing, and a number past the limits stops at the limit.

- Frequencies accept `2500` or `2.5k`. To switch a filter off, type `22k` for Lowpass or `20` for Highpass.
- Pan accepts `60 L`, `L60`, `60 R` or `-60`.
- Length in Note mode accepts `1/8` or `8`.
- Length in ms mode takes milliseconds.

The header knob has no value text, so it opens with Ctrl-click. Output in EDIT is the same control.

## EDIT

The `EDIT` link in the footer holds the note multiplier for the selected tap, and Output.

## Latency

None.

## Memory

Each instance reserves about 67 MB at 48 kHz, and about 540 MB at 384 kHz, so that adding a tap never allocates while audio runs.

## Using it in other hosts

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

Adding and deleting taps, and dragging them on the map, are undo steps too.

