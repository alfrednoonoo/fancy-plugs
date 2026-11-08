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
| + | Opens the add panel, and the new module lands after this one |
| Grip at the left edge | Drag to reorder |

Scroll with the mouse wheel, or drag in the chain strip at the bottom. The outline there shows the part of the rack you can see, and the power icons on its chips work too.

To type a value, click it, or Ctrl-click anywhere on a knob or fader. Enter or clicking away applies it, and Esc cancels. Units are optional, and frequencies can be typed as `2500` or `2.5k`. A number past the limits stops at the limit, and text with no number in it changes nothing.

## Modules

File, Echo generator and Reverb Generator add to whatever is already in the chain. The others change it.

**File** loads an impulse from disk. Use `<` and `>` or the dropdown to step through the folder.

| Control | What it does |
|---|---|
| Mix to mono | Sums the file's channels |
| Gain dropdown | Trims the file by 0 to -18 dB. SRC-gain also keeps the level the same when the file's sample rate differs from the session's, which no SRC-gain doesn't |

**LP/HP** is a 24 dB/oct filter. Lowpass at the top of its range and Highpass at the bottom are off.

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
| Stereo spread | 0–100 % | 0 is mono |

## Right column

| Control | Range | Default | What it does |
|---|---|---|---|
| Pre-delay | 0–500 ms | 0 ms | Delays the wet signal |
| Center | 100L–100R | Center | Balance of the wet signal |
| Width | 0–100W | 100W | Stereo width of the wet signal |
| Wet | -inf to +12 dB | -12 dB | Reverb level |
| Dry | -inf to +12 dB | 0 dB | Direct level |

Those five and the header's Output knob can be automated. Module settings can't, because the chain's length changes while the plugin runs.

## SETUP

| Setting | What it does |
|---|---|
| Latency | Zero latency has none. Low CPU reports 4096 samples and does nearly all the work off the audio thread |
| Max FFT | The largest FFT used. 65536 is the lighter choice for long IRs |
| IR storage | Whether a project keeps its own copy of the IR |
| IR folder | Where to look for files that have moved |

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
