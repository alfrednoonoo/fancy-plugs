> **This repo has moved to [alfrednoonoo/soul-plugs](https://github.com/alfrednoonoo/soul-plugs)**, with its full history and every release. The suite is now called Soul Plugs; the website is [alfrednoonoo.github.io/soul-plugs](https://alfrednoonoo.github.io/soul-plugs/). Nothing new is published here.

# Fancy Plugs

Sixteen effects for Windows 10 and 11 (x64, VST3) and macOS 11 or later (Intel and Apple silicon, VST3 and Audio Units), built for personal use and signed off by ear in REAPER. Version **1.7.2**.

| Plugin | What it does |
|---|---|
| Fancy Kicker | Low-end enhancer: Boost (one big Freq knob, Wet and Dry) or, new in 1.3.0, Sub, which adds an octave or two below; new in 1.7.1, Duck and clip oversampling |
| Fancy Exciter | Parallel high-frequency exciter with clip boost and a mix-back control; new in 1.7.1, Band 2, Punch, High cut and Width |
| Fancy Comp | Compressor with a Threshold fader over the input meter, a filtered detector, a sidechain and an output limiter; new in 1.4.0, Character, upward compression, Auto threshold and a detector EQ, which has six bands from 1.7.1, with link modes; new in 1.7.2, a level history on the main page |
| Fancy EQ | Up to 24 bands with draggable handles, a live analyzer, dynamic bands and, new in 1.4.0, mid/side per band and Match; new in 1.7.1, a brickwall slope, Character and a band list with groups; new in 1.7.2, spectral dynamics |
| Fancy Verb | Convolution reverb built from a chain of modules: impulse file, filters, echo generator, normalize; new in 1.5.0, a built-in IR library, true stereo and Modulate; new in 1.7.1, Attack and Extra width; new in 1.7.2, Freeze and Shimmer |
| Fancy Delay | Up to 16 delay taps, each with its own feedback, loop filters, bit crush, width, pan and ping-pong; new in 1.5.0, Tape and BBD character, modulation, diffusion, ducking and swing; new in 1.7.1, LFO shape and stereo phase; new in 1.7.2, reverse and pitch-shifted taps |
| Fancy MultiComp | Up to 8 bands of compression behind flat-summing crossovers, with upward compression and, new in 1.5.0, lookahead and mid/side per band; new in 1.7.1, lookahead and a detector EQ in each band, a slope per crossover and a GR history |
| Fancy FIR | Spectral processor: drawable EQ, Gate, Compressor, noise Subtract and Convolve, with adaptive noise tracking and, new in 1.6.0, Resonance; new in 1.7.1, a ratio curve and Mid/Side |
| Fancy Gate | Gate with a filtered detector, a sidechain and a MIDI trigger output; new in 1.7.1, a Transient detector and Upward mode; new in 1.7.2, up to three bands |
| Fancy Limit | New in 1.3.0: true-peak brickwall limiter with lookahead, three styles, Auto release, a loudness meter and dither; new in 1.7.2, up to four bands |
| Fancy DS | New in 1.3.0: de-esser with a relative threshold, split or wide cuts, minimum- or linear-phase split filters and a history display; new in 1.7.1, Hiss mode and Reduce |
| Fancy DS-Mini | New in 1.3.0: one-knob de-esser with voice and instrument profiles, on the same engine as Fancy DS; new in 1.7.1, a Hiss profile |
| Fancy Sature | New in 1.3.0: saturator with Tape, Tube, Transistor and Clip curves, even/odd Character, Tone, oversampling and auto gain; new in 1.7.1, Diode and Fold, cuts, Spread and Channels |
| Fancy Room | New in 1.3.0: algorithmic reverb from small room to long hall, with Freeze, tempo-synced pre-delay, a send mode and ducking; new in 1.7.1, Low decay, Attack and sidechain ducking; new in 1.7.2, Shimmer |
| Fancy TS | New in 1.7.0: transient shaper that turns each hit's attack and sustain up or down, whatever the level, with Pump, a sidechain and a soft clipper |
| Fancy Chorus | New in 1.7.0: chorus with five modes (Classic, Ensemble, Dimension, Doubler, Flange), 1 to 6 voices, Tempo sync, Age and ducking |

## What's new in 1.7.2

Something new in seven plugins (each plugin's manual has the details):

- **Limit:** up to four bands, each limited on its own before the final limiter, so a loud kick pumps the rest of the mix far less. Two to four bands add 5 ms of latency.
- **Gate:** up to three bands, each opened by its own band of the detector: on a tom mic, the lows open for the tom and the highs stay shut through the cymbals. Bands add no latency.
- **EQ:** spectral dynamics. A dynamic band with SPECTRAL on reacts per frequency inside its shape, so a ringing resonance in a wide band is cut on its own. Delete or Backspace deletes the selected band, and the band list grows with the window.
- **Verb:** Freeze, which holds what the reverb is playing, and Shimmer, which feeds the tail back an interval higher on each pass.
- **Room:** Shimmer, with a choice of intervals.
- **Delay:** a Reverse switch and Pitch (-12 to +12 st) for each tap; pitched repeats climb or fall on every pass.
- **Comp:** a HISTORY card on the main page, with the input, output, gain reduction and threshold in dB.

Comp, Limit and Gate are taller and Delay is wider, to fit the new controls without shrinking the old ones; Comp's knobs are smaller to make room for its history. Projects saved with 1.0 to 1.7.1 load and sound exactly as before. Every new setting defaults to the old behaviour.

## What was new in 1.7.1

Something new in thirteen plugins (each plugin's manual has the details):

- **Gate:** a Transient detector that opens on hits, not level, and an Upward mode that lifts what's above Threshold, with an output ceiling.
- **Comp:** six detector bands with types, 12, 24 or 48 dB/oct detector cuts, link modes (Stereo, Mid/Side, M>S, S>M) and a level history in dB.
- **EQ:** a Brickwall (96 dB/oct) cut slope, a Character output stage, and a band list with groups.
- **Verb:** Attack on the Reverb generator, and Extra width up to 200 %.
- **Room:** Low decay, Attack and ducking from a sidechain.
- **Delay:** a sine or triangle LFO and its stereo phase, per tap.
- **MultiComp:** a lookahead and a detector EQ (HP, LP and Tilt) for each band, a slope for each crossover, and a gain-reduction history.
- **FIR:** a ratio curve, so Compressor can be a different ratio at each frequency, and Mid/Side for every mode but Convolve. Changing engine settings no longer dips the level.
- **DS and DS-Mini:** Hiss, which turns the highs down in the gaps, and Reduce (Both, Mid or Side) in DS.
- **Sature:** Diode and Fold curves, Offset, low and high cuts, Spread, and Mid or Side only.
- **Exciter:** Band 2, Punch, High cut and Width.
- **Kicker:** Duck, from a sidechain or the host's tempo, and clip oversampling.

Gate, Delay, FIR, Kicker and Room have slightly larger windows to fit the new controls. Projects saved with 1.0 to 1.7.0 load and sound exactly as before. Every new setting defaults to the old behaviour.

## What was new in 1.7.0

- **Mac.** Every plugin now comes for macOS 11 or later as a universal VST3 and Audio Unit (Intel and Apple silicon), in its own zip. Fancy Gate's Audio Unit is a music effect, so it can send MIDI.
- **Two new plugins:** **Fancy TS**, a transient shaper, and **Fancy Chorus**, a chorus with five modes (table above).
- **Something new in every plugin** (each plugin's manual has the details):
  - **Comp:** Hold, Range, Listen to removed, a GR history, Host sync (a tempo-synced trigger) and an Optical mode.
  - **EQ:** band solo, auto gain, dynamic reach and an analyzer freeze.
  - **Limit:** Gain Lock, Auto to Target, a DC filter, a release shape, peak statistics and a clipper before the limiter.
  - **Gate:** a MIDI trigger input, a ratio, auto threshold, Mid/Side sources, de-bleed and Host sync.
  - **DS:** Learn, a detection filter slope, an output ceiling and **dual-band detection**: a second, Low band for "sh" sounds, in yellow, beside the band for "s".
  - **DS-Mini:** Auto Amount.
  - **Verb:** equal loudness, a Post-EQ module, an IR browser, a decay module, and ducking and a level gate.
  - **Room:** mod rate, a reverb EQ, a gate and auto gain.
  - **Delay:** Freeze and an output ceiling. **MultiComp:** learned crossovers, band mute and a dynamic-phase crossover. **FIR:** resonance tilt, learned curves and Resonance depth nodes.
  - **Exciter:** listen to the added harmonics, a THD read-out, a ceiling and auto gain. **Kicker:** sub length, make-room, polarity, a sub meter, auto gain and a ceiling. **Sature:** outer-band drive.
- **Simpler installing:** unzip and drag on Windows, one double-click on a Mac (see "Installing").

Projects saved with 1.0 to 1.6 load and sound exactly as before. Every new setting defaults to the old behaviour.

## What was new in 1.6.0

- **Fancy FIR: Resonance mode.** A sixth mode that finds narrow peaks standing above the spectrum around them, such as a ringing snare, a boxy vocal or a room mode, and turns each one down by up to Depth only while it's there. Broadband material, plain noise included, changes by well under 0.5 dB.
- **Depth, Sharpness, Attack, Release and Range.** Sharpness sets how narrow a peak must be to count; Range (Low and High) keeps it to part of the spectrum, and the graph dims what's outside. The cut shows in green, hanging from 0 dB.
- **Linked stereo:** both channels get the same cut, so the image never moves. **Listen to removed resonances** plays only what's taken out.
- **Two new presets:** Resonance Tamer and Harsh Vocal (Restoration).

Projects saved with 1.0 to 1.5 load and sound exactly as before.

## What was new in 1.5.0

- **Fancy Verb: IR library.** 32 impulse responses built in, in Rooms, Halls, Plates, Chambers and Special, from a LIBRARY section in the File module's dropdown. A project that uses one stays small, because it stores the name, not the audio.
- **Fancy Verb: true stereo.** Library IRs marked TS, four-channel WAVs and a pair of stereo files (**Pair right input…**) keep each input side's own reflections, so a source panned left still fills the room. The Reverb generator gains Low and High RT, Early reflections (Room or Hall, with Size; Plate is dense from the start), Build-up and a True stereo switch.
- **Fancy Verb: Modulate.** A new module that moves a long tail gently, without pitch wobble, with Rate and Depth. Offline renders stay identical.
- **Fancy Verb: Add module** is now a card in the chain with every module type.
- **Fancy MultiComp: lookahead** from 0 to 10 ms, reported to the host, and **a channel per band:** Stereo, Left, Right, Mid or Side, from a chip on the band row.
- **Fancy Delay: character and motion.** Each tap can be Clean, Tape or BBD, with Amount, and the Motion page adds Rate, Depth, Random and Diffusion. The EDIT drawer adds Ducking and Swing.
- **New presets:** Library Hall, Library Plate, True Stereo Room, Modulated Hall and Endless (Verb); Lookahead Glue and Mid/Side Master (MultiComp); Tape Echo, BBD Chorus Echo, Diffused Wash and Ducked Vocal Echo (Delay).

Projects saved with 1.0 to 1.4 load and sound exactly as before. Every new setting defaults to the old behaviour.

## What was new in 1.4.0

- **Fancy EQ: 24 bands and a channel per band.** Up to 24 bands, and each one can work on Stereo, Left, Right, Mid or Side, from a small chip under its type. The white curve shows the total for the selected band's channel, Linear phase follows the channels in use, and the analyzer can show any one channel.
- **Fancy EQ: Match.** Learn a reference and your material, from the sidechain or captured one after the other, and Match writes ordinary bands into free slots to move your track towards the reference, as one undo step. Amount, Smoothing, the number of bands and a frequency range set how far it goes.
- **Fancy Comp: Character.** A Warm or Bright saturation stage, before or after the compressor, with Drive, using Fancy Sature's curves.
- **Fancy Comp: upward compression,** as in Fancy MultiComp, with Up threshold, Up ratio, Max lift and a Floor, and a transfer curve that shows both directions.
- **Fancy Comp: Auto threshold** follows the input's long-term level, so the gain reduction stays steady between quiet and loud passages. The Threshold meter shows the threshold in use.
- **Fancy Comp: detector EQ and 8x.** A low shelf, a bell and a high shelf shape what the detector hears, with a curve you can drag. Oversampling now goes to 8x. The EDIT drawer has four pages: Detector, Character, Upward and Options.
- **Three new Comp presets:** Warm Bus, Upward Vocal and Level Rider.

Projects saved with 1.0 to 1.3 load and sound exactly as before. Every new setting defaults to the old behaviour.

## What was new in 1.3.0

- **Five new plugins:** Fancy Limit, Fancy DS, Fancy DS-Mini, Fancy Sature and Fancy Room (table above).
- **Sub mode in Fancy Kicker.** A Boost | Sub switch at the top. Sub tracks the kick's pitch and adds a clean sub an octave or two below, under Cutoff. Boost sounds exactly as before.
- **Preset browser in every plugin**, with categories, user folders, favourites and search (see "Choosing presets" below), and the ‹ › arrows on the preset name.
- **Free resizing.** Drag a window's edge or corner to resize it, from 100 % to 200 % of its size; the layout scales with it.
- **Title strip.** Every window shows the plugin's name at the top.
- **Listen switches are temporary.** Comp's and Gate's Preview filter and FIR's Listen switches now turn off when you close the window and are never saved, like the new plugins' Listen. A project saved with one of them on opens with it off; nothing else about old projects changes.

Projects saved with 1.0, 1.1 and 1.2 load and sound exactly as before.

## The plugins

The screenshots below are rendered by CI from the current build, at 2× scale.

### Fancy Kicker

<img src="docs/images/kicker.png" width="432" alt="Fancy Kicker in Boost mode"> <img src="docs/images/kicker-sub.png" width="432" alt="Fancy Kicker in Sub mode">

### Fancy Exciter

<img src="docs/images/exciter.png" width="392" alt="Fancy Exciter">

### Fancy Comp

<img src="docs/images/comp.png" width="934" alt="Fancy Comp">

The EDIT drawer's Upward page, with the static curve and the current lift:

<img src="docs/images/comp-upward.png" width="934" alt="Fancy Comp's EDIT drawer on the Upward page">

### Fancy EQ

Shown with band 2 set as a dynamic cut. The dashed outline shows the most it can cut, and the filled curve shows what it's cutting right now.

<img src="docs/images/eq-dynamic.png" width="802" alt="Fancy EQ with a dynamic band">

The Match card over a learned reference (purple), the current material (grey) and the target (dashed):

<img src="docs/images/eq-match.png" width="802" alt="Fancy EQ's Match card">

Band 2 with SPECTRAL on, cutting one resonance inside its bell:

<img src="docs/images/eq-spectral.png" width="802" alt="Fancy EQ with a spectral dynamic band">

### Fancy Verb

<img src="docs/images/verb.png" width="866" alt="Fancy Verb">

A true-stereo hall from the built-in library:

<img src="docs/images/verb-library.png" width="866" alt="Fancy Verb with a library IR in true stereo">

### Fancy Delay

<img src="docs/images/delay.png" width="884" alt="Fancy Delay">

The Motion page, with a Tape tap:

<img src="docs/images/delay-motion.png" width="884" alt="Fancy Delay's Motion page">

Tap 1 reversed and pitched up a fifth:

<img src="docs/images/delay-reverse.png" width="884" alt="Fancy Delay with a reversed, pitched tap">

### Fancy MultiComp

Shown with the Upward row for the selected band and the global Depth and Time knobs.

<img src="docs/images/multicomp.png" width="760" alt="Fancy MultiComp with upward compression">

Band 2 set to Mid (the M chip and the M on its handle):

<img src="docs/images/multicomp-channel.png" width="760" alt="Fancy MultiComp with a mid-only band">

### Fancy FIR

Shown in Subtract mode with Adaptive tracking. The dashed line is the tracked noise profile.

<img src="docs/images/fir-adaptive.png" width="830" alt="Fancy FIR with adaptive noise tracking">

Resonance mode, turning down resonances at 700 Hz and 3 kHz:

<img src="docs/images/fir-resonance.png" width="830" alt="Fancy FIR in Resonance mode">

### Fancy Gate

<img src="docs/images/gate.png" width="790" alt="Fancy Gate">

Three bands, each gated on its own band of the detector:

<img src="docs/images/gate-bands.png" width="790" alt="Fancy Gate with three bands">

### Fancy Limit

<img src="docs/images/limit.png" width="760" alt="Fancy Limit">

Three bands, each limited on its own before the final limiter:

<img src="docs/images/limit-bands.png" width="760" alt="Fancy Limit with three bands">

### Fancy DS

<img src="docs/images/ds.png" width="760" alt="Fancy DS">

### Fancy DS-Mini

<img src="docs/images/dsmini.png" width="392" alt="Fancy DS-Mini">

### Fancy Sature

<img src="docs/images/sature.png" width="760" alt="Fancy Sature">

### Fancy Room

<img src="docs/images/room.png" width="760" alt="Fancy Room">

### Fancy TS

<img src="docs/images/ts.png" width="760" alt="Fancy TS">

### Fancy Chorus

<img src="docs/images/chorus.png" width="760" alt="Fancy Chorus">

## Installing

Download your system's zip from the [latest release](https://github.com/alfrednoonoo/fancy-plugs/releases/latest). Each has a `How to install.txt`.

### Windows

1. Close your DAW, and unzip `FancyPlugs-<version>-win64.zip`.
2. Drag the `.vst3` folders from its `VST3` folder into `C:\Program Files\Common Files\VST3`, the folder every Windows DAW looks in. Windows asks once for administrator approval: click **Continue**.
3. Open your DAW and rescan its plugins (REAPER: Options > Preferences > Plug-ins > VST > Re-scan).

To update, close the DAW first and copy the new folders over the old ones.

### Mac (Intel and Apple silicon)

1. Close your DAW, and double-click `FancyPlugs-<version>-macos.zip` to unzip it.
2. Double-click `Install Fancy Plugs.command`. The first time, macOS blocks it, because the plugins aren't signed with an Apple Developer ID: open **System Settings > Privacy & Security**, click **Open Anyway** next to its name, and confirm with your password. A Terminal window then installs every VST3 and Audio Unit into your own Library folders (`~/Library/Audio/Plug-Ins`), clears macOS's download flag on them, and says how many it installed; press Enter to close it.
3. Open your DAW and rescan. Use the VST3s in REAPER, Live, Cubase, Bitwig and Studio One, and the Audio Units in Logic Pro and GarageBand. On a Mac, **Cmd-click** a control to type a value.

If the script can't run, `How to install.txt` explains the manual way: drag the bundles into `~/Library/Audio/Plug-Ins/VST3` and `Components`, then run one line in Terminal.

### The newest CI build (for development)

`scripts\get-latest.ps1` on Windows, or `./scripts/get-latest.sh` on a Mac (it needs the GitHub CLI, `gh`, signed in), downloads the newest successful build of the current branch and installs it: on Windows into `%USERPROFILE%\VST3-Dev`, which you add to REAPER's VST paths once; on a Mac into your own Library folders. Neither needs admin rights.

## Choosing presets

Click the preset name in the header to open the preset browser. It works the same way in every plugin:

- **One click** on a preset loads it, and the browser **stays open**, so you can try several in a row.
- **Double-click** a preset, or press **Enter**, to load it and close the browser.
- Click outside the browser, or press **Esc**, to close it.
- The star marks a favourite without loading the preset.
- The ‹ › arrows on the preset name step to the previous or next preset without opening the browser.

**The full manual**, with every control of all sixteen plugins, quick starts and a glossary, is [`docs/manual/Fancy-Plugs-Manual-1.7.2.pdf`](docs/manual/Fancy-Plugs-Manual-1.7.2.pdf). Each plugin also has a manual and a REAPER checklist in `docs/`. `docs/PROGRESS.md` has the full history, test results and measurements.

## How builds work

Nothing compiles on the local PC. Each push runs `.github/workflows/build.yml` on GitHub's Windows and Mac runners. The run builds the plugins and runs the DSP tests, Steinberg's validator, pluginval and the host harness. It also renders UI screenshots, then uploads the results as artifacts. Changes that only touch `docs/`, `reference/` or Markdown files don't start a build.

To build a single plugin, run the workflow by hand and pick it:

```powershell
gh workflow run build.yml -f plugin=kicker
```

## Licence

Fancy Plugs is free software under the GNU Affero General Public License, version 3 (`LICENSE`). It's built with JUCE, used under the same licence. You may use, study, change and share it; anything you share that's built from it must be under the AGPL v3 too, with its source.

## Attribution

UI design based on "ReaPlugs (vst GUI)" by Eugene Reznik, CC BY 4.0 — https://www.figma.com/community/file/967866927752067729. Modified from the original.

The Inter typeface is used under the SIL Open Font License; see `assets/fonts/Inter-LICENSE.txt`.
