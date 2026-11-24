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

Click **EDIT** to open it and **Done** to close it. The switch at its top has two pages: **Main**, described here, and **Duck and clip** (see "Duck and Clip oversampling" below).

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

Latency is 0 samples with Clip oversampling and Ceiling Off (the defaults); each reports its own latency while it's on. The filtered signal is minimum-phase, so it stays aligned with the dry signal at Freq, apart from a small phase lead from a 15 Hz filter that keeps DC and infrasonic energy out of the wet path. At 50 Hz, wet and dry at equal level sum to about +5.8 dB. The sum is smaller at the bottom of the Freq range.

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


## Sub mode

The **Boost | Sub** switch at the top picks what Kicker does. **Boost** (the default) lifts the kick's own low end, as above. **Sub** creates new bass below the kick: it follows the kick's low end and plays a clean sine an octave (or two) below it, at the kick's own level, locked to its cycles so it stays punchy. It adds no latency.

| Control | Range | Default | What it does |
|---|---|---|---|
| Cutoff | 40–200 Hz | 100 Hz | The highest frequency Sub listens to. Set it just above the kick's body |
| −1 oct / −2 oct | | −1 oct | How far below. −2 oct suits toms and pitched-up 808s; two octaves below a low kick is under 15 Hz and is filtered out |
| Sub | −inf to +6 dB | −9 dB | The level of the sub it creates |
| Dry | −inf to +12 dB | 0 dB | The kick itself. The same Dry as in Boost |

In **⚙ EDIT** (Sub):

| Setting | Range | Default | What it does |
|---|---|---|---|
| Sensitivity | −60 to 0 dB | −40 dB | Below this level Sub stays silent, so bleed and noise don't make sub |
| Sub low-pass | 60–250 Hz | 120 Hz | Smooths the sub: nothing it adds goes much above this |
| Output, Soft clip | | | As in Boost |
| Mono sub | on/off | on | The same sub in both channels, from L+R. Off: each channel makes its own |

Switching mode while playing crossfades over 20 ms. Presets **Sub: Deep 808**, **Sub: Tight Punch** and **Sub: Tom Drop** are in the Kick category.

## Phase 4 additions

- **Sub length:** ends the sub this long after each hit, with a short fade, so a long tail doesn't run into the next kick. Off at the top.
- **Make-room:** cuts the source kick's lowest end below its frequency (60 Hz by default), so the sub has it to itself.
- **Invert sub:** flips the sub, for kicks whose own low end cancels it.
- **Sub meter:** the thin bar between In and Out shows the sub's level.
- **Auto gain** (drawer): turns the output down by as much as Boost or Sub adds, so you hear the shape, not the level. **Ceiling** (drawer): holds the output's true peak under this level; Off at the bottom.

## Duck and Clip oversampling

The drawer's second page, **Duck and clip**.

| Control | Range | Default | What it does |
|---|---|---|---|
| Duck | 0 to 24 dB | 0.0 dB | Turns the whole output, the dry signal and what Kicker adds, down by up to this much while a kick hits. 0 turns it off |
| Release | 20 to 500 ms | 120 ms | How fast the output comes back after each kick |
| Duck from | Sidechain, Host sync | Sidechain | What Duck listens to: a kick on the sidechain input, or a pulse on the host's beat grid |
| Rate | 1/1 to 1/16, dotted, triplet | 1/4 | Host sync only: one pulse per note of this length |
| Shape | 5 to 100 % | 50 % | Host sync only: how long each pulse holds the duck, as a share of the step |
| Clip oversampling | Off, 2x, 4x | Off | Runs Soft clip at 2x or 4x the sample rate, so a hard-driven kick's click doesn't alias |

**Duck** is for Kicker on a bass: when the kick hits, the bass and its added low end both fill 40 to 80 Hz, and the low end clips or blurs. Duck turns the bass down while the kick plays, then lets it back over Release, so the two take turns. Route the kick to Kicker's sidechain (in REAPER, pins 3/4 of the bass track, fed by a send from the kick). Duck follows the kick's peak: it ducks fully when the kick is above -12 dBFS and not at all below -24 dBFS, so a kick's tail doesn't hold the bass down. With **Host sync**, there's no kick track to route: Duck pulses on the host's beat at Rate, for Shape of each step. Without a host tempo it runs on at 120 BPM.

While Duck is above 0, a red **DUCK** tag shows beside EDIT on the main window, so a ducking Kicker is never a surprise; click it to open this page. With Duck from Sidechain and nothing routed to the sidechain, the main window shows "No sidechain signal".

**Clip oversampling** only matters with **Soft clip** ticked on the Main page. On a typical kick driven into the clipper, 4x leaves the aliasing and error about 35 dB lower than Off, and 2x about 20 dB lower. It reports its latency while it's on (55 samples at 2x and 68 at 4x, at 48 kHz), so hosts keep the track in time. Off is Kicker as it was.

## Credits

UI design based on "ReaPlugs (vst GUI)" by Eugene Reznik, CC BY 4.0 — https://www.figma.com/community/file/967866927752067729. Modified from the original.
