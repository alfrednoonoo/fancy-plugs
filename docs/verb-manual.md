# Fancy Verb manual

Fancy Verb is a convolution reverb. You build its impulse response (IR) from a chain of modules, which can load files, generate echoes or a reverb tail, and filter or shape the result. The modules only build the IR, so changing one never touches the audio directly. The new IR crossfades in over 64 ms.

## Getting started

A new instance has an empty chain and passes audio unchanged. Click a button to add the first module, or pick a factory preset. Only Empty uses no modules; the other five use the generators, so they need no files.

## The display

The waveform shows the finished IR, and the text at the top right gives its length, sample rate, channels and peak. The green lines mark where the echo generators put their echoes.

The small handles at the top corners trim the IR. Drag either one inwards. The waveform stays whole, and the dimmed parts aren't used.

## The rack

Modules run left to right.

| Control | What it does |
|---|---|
| Power icon | Takes the module out without losing its settings |
| × | Removes the module |
| Grip at the left edge | Drag to reorder |

To add a module, click the dashed **+ Add module** box after the last card. The add panel opens in its place; pick a module and it lands at the end of the chain, or click **Cancel** (or press Esc, or click the rack's empty space) to close it. Drag the new card by its grip to move it earlier. An empty rack shows its module buttons in the middle instead.

Scroll with the mouse wheel, or drag in the chain strip at the bottom. The outline there shows the part of the rack you can see, and the power icons on its chips work too.

To type a value, click it, or Ctrl-click anywhere on a knob or fader. Enter or clicking away applies it, and Esc cancels. Units are optional, and frequencies can be typed as `2500` or `2.5k`. A number past the limits stops at the limit, and text with no number in it changes nothing.

## Modules

File, Echo generator and Reverb Generator add to whatever is already in the chain. The others change it.

**File** loads an impulse from disk or from the built-in library. The dropdown opens on the **Library** (Rooms, Halls, Plates, Chambers and Special: 32 IRs made with the Verb's own generators), then the files in your IR folder. Use `<` and `>` to step through the folder, or through a library category. A library IR is saved in the project by name, so the project stays small and opens anywhere.

**True stereo:** a normal IR sends left to left and right to right. A true-stereo IR also sends each input to the other side, so a source on the left reaches the right through its own reflections. The card shows a purple **TS** next to its title when one is loaded. It comes from:

- a four-channel file, read as left-to-left, left-to-right, right-to-left, right-to-right;
- **Pair right input…** in the dropdown, which adds a second stereo file for the right input (the first one is then the left input's); **Remove pair** takes it away;
- the library's IRs marked TS.

**Finding an IR:** type in the search field at the top of the dropdown to filter the library by name or category. Click the star beside an IR to make it a favourite: favourites are listed first, on this computer, in every project. You can also drag an audio file from Explorer or Finder onto the rack: dropped on a File card it replaces that card's file, anywhere else it adds a new File module at the end.

**Export IR...** (in the File dropdown, or right-click the waveform) saves the finished impulse, after the trim handles, as a 24-bit WAV at the session's rate. A true-stereo IR is saved as four channels, so it loads back as true stereo.

Projects saved before 1.5 that used a four-channel file keep playing its first two channels, as they did; choose the file again to make it true stereo. True stereo costs about 1.1 to 1.3 times the CPU of a normal IR.

| Control | What it does |
|---|---|
| Mix to mono | Sums the file's channels; a true-stereo IR becomes one mono IR |
| Gain dropdown | Trims the file by 0 to -18 dB. SRC-gain also keeps the level the same when the file's sample rate differs from the session's, which no SRC-gain doesn't |

**LP/HP** is a 24 dB/oct filter. Lowpass at the top of its range and Highpass at the bottom are off.

**EQ** shapes the IR at its place in the chain: a low shelf, a bell and a high shelf. It changes the impulse once, when you move a knob, so it costs nothing while playing. The curve at the top of the card shows the response. A band at 0 dB does nothing.

| Control | Range | Default |
|---|---|---|
| Low shelf Freq, Gain | 20 Hz to 1 kHz, ±18 dB | 120 Hz, 0 dB |
| Bell Freq, Gain, Q | 100 Hz to 16 kHz, ±18 dB, 0.2 to 10 | 2 kHz, 0 dB, 1.0 |
| High shelf Freq, Gain | 1 to 20 kHz, ±18 dB | 8 kHz, 0 dB |

**Echo generator** adds a run of repeats.

| Control | Range | What it does |
|---|---|---|
| Start pos. | 0–1000 ms | Where the first echo lands |
| Length | 0–5000 ms | How long the run lasts |
| Spacing | 1–1000 ms | Time between echoes |
| Error | 0–100 ms | Random timing shift for each echo, different in each channel |
| ErrDst | 0–1 | Higher keeps most echoes closer to their slots |
| Start Vol., End Vol. | -60–0 dB | Level of the first and last echo |

**Normalize** scales the chain so its peak hits Peak Level.

**Trim, Gain, Stretch** cuts Start from the front and End cut from the back, adds Fade in and Fade out, applies Gain, then resamples by Stretch, so length and pitch change together.

**Reverse** flips the IR.

**Reverb Generator** makes a tail from seeded noise, so the same settings always give the same sound.

| Control | Range | What it does |
|---|---|---|
| Length | 0.1–20 s | How much tail is generated |
| Decay | 0.05–20 s | Time to fall 60 dB |
| Density | 0–100 % | Lower starts sparser, and it fills in over 100 ms |
| Damping | 0–100 % | How fast the highs die away |
| Spread | 0–100 % | Stereo spread: 0 is mono |
| Low RT, High RT | 0.25–4 x | Decay below 250 Hz and above 4 kHz, as a multiple of Decay. 1 x leaves the decay the same at every frequency |
| Size | 2–60 m | The room for the early reflections, with Early set to Room or Hall |
| Build-up | 0–300 ms | How long the tail takes to reach full density; with Room or Hall, it also fades the tail in under the early reflections |
| Early | Off, Room, Hall, Plate | Room and Hall add discrete early reflections from a simple box model (Room has more, closer together); Plate makes the tail dense from the first sample, with no reflections |
| True stereo | off/on | Makes a true-stereo IR, with its own cross paths |
| Attack | 0–500 ms | Fades the whole tail in over this time, early reflections included, so the reverb swells in (a bloom hall, a pad) instead of starting at full level. 0 by default |

With Low RT, High RT, Size, Build-up, Early and True stereo at their defaults, the generator makes exactly the IR it did before 1.5. At Attack 0 it makes exactly the IR it did before 1.7.1.

Attack and Build-up are different: Build-up sets how long the tail takes to become dense, Attack how long it takes to become loud. Attack takes the tail's start away, so a long one makes the reverb quieter as well as less clear: about 1 dB at 50 ms and 3 dB at 150 ms with Decay 1.8 s. Turn on **Equal loudness** in SETUP to even that out, or raise Wet. Changing Attack rebuilds the IR with the usual crossfade.

**Modulate** moves the tail slowly, so long reverbs don't ring statically, without changing pitch. It plays the tail after Start in two versions with the same decay and spectrum but different fine detail, and crossfades between them. The early part never moves. Only one Modulate fits in a chain, and where it sits doesn't matter: it always acts on the finished IR.

| Control | Range | What it does |
|---|---|---|
| Start | 20–500 ms | Where the moving tail begins. Changing it rebuilds the IR |
| Rate | 0.05–2 Hz | How fast the tail moves; it drifts a little, so it never repeats exactly |
| Depth | 0–100 % | How far it moves: at 100 % the two versions swap fully |
| Spread | 0–100 % | How far apart the left and right move, for a wider motion |

Rate, Depth and Spread change smoothly while you play, without a rebuild. Modulate costs about twice the CPU of the same IR without it, because the tail is played in two versions.

## Right column

| Control | Range | Default | What it does |
|---|---|---|---|
| Pre-delay | 0–500 ms | 0 ms | Delays the wet signal |
| Center | 100L–100R | Center | Balance of the wet signal |
| Width | 0–100W | 100W | Stereo width of the wet signal |
| Wet | -inf to +12 dB | -12 dB | Reverb level |
| Dry | -inf to +12 dB | 0 dB | Direct level |

Those five, Extra width in SETUP and the header's Output knob can be automated. Module settings can't, because the chain's length changes while the plugin runs.

## SETUP

| Setting | What it does |
|---|---|
| Latency | Zero latency has none. Low CPU reports 4096 samples and does nearly all the work off the audio thread |
| Max FFT | The largest FFT used. 65536 is the lighter choice for long IRs |
| IR storage | Whether a project keeps its own copy of the IR |
| IR folder | Where to look for files that have moved |
| Equal loudness | Sets the wet level so every IR plays about as loud as any other, so a small room and a cathedral don't jump 12 dB apart. The gain it applied shows beside it. Off by default |
| Extra width | 100–200 %, 100 % by default. Widens the reverb past the right column's Width: the two multiply, so Width 100W with Extra width 150 % puts one and a half times the side in the wet signal. It raises only the side, so the mono sum stays exactly the same, and each side gets up to 3 dB louder at 200 %. Past about 150 % the left and right are more opposed than alike: check in mono, and keep to 120–150 % for most mixes. It can be automated and changes smoothly, as Width does |

The late-block counter should stay at 0. If it climbs, a background job missed its deadline and a slice of the tail dropped out, so try Low CPU.

## Moved files

A project looks for its IR in its embedded copy, then at the original path, then in your IR folder by name and contents. If all three fail, the File card shows `IR missing – Relink...` in red, and clicking it opens a file browser. IRs under 10 MB are embedded by default. An embedded copy is 24-bit FLAC, so a float IR peaking over 0 dBFS would clip in the copy.

## Using it in other hosts

### Latency

Some settings change the plugin's latency: the latency mode (zero latency or low CPU). The plugin reports each change to the host, which delays the other tracks to match. Some hosts only read the new latency when playback stops and starts, so change these settings while stopped, or stop and start once after changing them; the tooltip says so too. In a host with no delay compensation, the track plays late by the latency, and nothing else goes wrong.

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

Changes to the module chain (adding, removing, moving or editing a module, and choosing a file) are undo steps too, and A and B each keep their own chain.

## Decay module (Phase 4)

**Add > Decay** reshapes the IR's decay per band, offline, so it costs nothing while playing. Place it after the modules that make the IR (FILE, the generators). It splits the IR into Low, Mid and High at the two **crossovers** (Low 20 Hz to 1 kHz, 250 Hz by default; High 1 to 16 kHz, 4 kHz), measures each band's decay time (RT60), and makes it the **multiplier** times as long: 0.25x to 4x, 1x leaving the band as it is.

- Under each multiplier the card shows the band's RT before and after, from the last build.
- Lengthening can't invent reverb: a band only grows until it reaches the IR's own noise floor, then fades out, so a 4x on a short IR gets as long as the IR allows and the card says so.
- A band with no measurable decay (an echo pattern, a single click) is left as it is and reads "can't measure".
- An older Verb plays the IR without the module.

## Ducking and the Gate (Phase 4)

In **SETUP > After the reverb**, two stages act live on the wet signal after the IR, whatever the rack holds:

- **Ducking** (0 to 100 %): turns the reverb down while the source plays, by up to 24 dB at 100 % (fully above -20 dBFS, not at all below -50), and lets it bloom in the gaps. It comes back over about 300 ms.
- **Gate**: stops the reverb when the source stops, for a gated reverb. **Threshold** (-60 to 0 dB) is the source level under which it starts to close, **Hold** (10 to 500 ms) how long it stays open after that, and **Decay** (5 to 500 ms) how fast the reverb then fades out.
- **Source**: what both listen to, this track's input (**Main**) or the **Sidechain** input (pins 3/4 in REAPER), so a vocal can duck a reverb on a send. With nothing routed there, Sidechain acts as Main.

Both wait for Low CPU mode's latency, so they act on the wet sound you hear, not 85 ms ahead of it. At Ducking 0 and the Gate off they don't run.
