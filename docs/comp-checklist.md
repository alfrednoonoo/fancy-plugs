# Fancy Comp: 10-minute REAPER check

This one takes a little longer than the others, because there's more to try. Use a loop with clear dynamics, such as drums or a vocal.

## Install

1. Close any REAPER project using these plugins.
2. In PowerShell, run:
   ```powershell
   cd C:\dev\fancy-plugs
   .\scripts\get-latest.ps1
   ```
3. In REAPER, go to Options > Preferences > Plug-ins > VST and click **Re-scan**.
4. Add **Fancy Comp** to a track.

## Compression

- [ ] Lowering Threshold compresses more, the GR meter moves, and the red blocks at the top of the input meter light up.
- [ ] Ratio, Knee, Attack and Release each change the sound as you'd expect.
- [ ] Auto make-up brings the level back up.
- [ ] Attack feels reasonable at familiar settings. Ticking **Attack = 90 % rise** in EDIT makes it faster.
- [ ] Classic, Auto and Weird each change the response, without clicks when toggled.
- [ ] RMS size at 0 ms grabs peaks more tightly than at 50 ms.

## Detector

- [ ] Highpass at about 150 Hz stops a kick from pumping the mix.
- [ ] Preview filter lets you hear what the detector hears.
- [ ] Tick it, close the plugin window, and open it again: it's unticked, the sound is normal, and the title strip has no **LISTENING** tag. While it's ticked, the tag is there.
- [ ] Mid and Side settings respond differently on a stereo source.

## Sidechain

- [ ] Route a kick to channels 3 and 4 of a bass track, then pick **Sidechain L+R**. The bass ducks with the kick.

## Wet and dry

- [ ] The Wet and Dry faders work. With Dry raised, compression sounds parallel, with no comb filtering. Try this with Pre-comp at 10 ms too.

## Limiter

- [ ] Tick Limit output, then raise the header knob far enough to push the output over 0 dB. The Out meter stays at or below 0.
- [ ] REAPER shows extra latency once Limit output is on.

## Latency settings

- [ ] Changing Pre-comp while playing gives one short dropout after you stop turning, and no click.
- [ ] Oversampling at 2x and 4x in EDIT does the same, and the compressor keeps working.

## Controls

- [ ] Drag, Shift-drag, double-click, Ctrl-click typing and the mouse wheel all work on knobs and faders.
- [ ] Click a value, type `-12` and press Enter: the control moves to it. Repeat and click on empty space instead: it applies too. Repeat and press Esc: nothing changes. Try it on a knob, on Threshold and on Wet.
- [ ] Right-click on a knob shows REAPER's menu.
- [ ] Hovering over Attack shows its tooltip.
- [ ] The Detector input dropdown opens and changes setting.
- [ ] EDIT opens the drawer, and Done or EDIT closes it.
- [ ] The ON pill switches cleanly, and the controls dim while it's off.

## Presets and size

- [ ] Choosing a preset from the dropdown changes the settings.
- [ ] **+ New** saves a preset, and it appears in the dropdown, including in a new instance.
- [ ] Parameter > UI size at 200 % resizes the window and everything stays sharp.
- [ ] Parameter > About shows the design attribution.

## State

- [ ] Change plenty of settings, including the detector, the EDIT drawer settings and the UI size, then save the project.
- [ ] Close REAPER, reopen the project, and confirm the settings and sound are the same.

## Report back

Tell me "Comp OK", or list which items failed and what you saw. Once it's OK, I'll start the EQ.

## Menus and pop-ups (Phase 2.1)

- [ ] Click the preset dropdown: the menu opens just under it, in the plugin's dark style, with **FACTORY** (and **USER** once you've saved one) sections and a check mark on the loaded preset. Hovering a row shows a lighter row with a teal bar at its left.
- [ ] Change any control: the dropdown shows the name with a **•** after it, and the menu offers **Reload "…"**. Reload puts the preset back and the dot goes.
- [ ] **Save as new preset…** opens a small dialog inside the plugin window. Save one, then **Rename…** and **Delete…** it from the menu. Delete asks first, with a red Delete button. **Open preset folder** opens Explorer.
- [ ] Arrow keys move through an open menu, Enter picks, Esc closes.
- [ ] Click Detector input: the list has **MAIN INPUT**, **SIDECHAIN** and **MID/SIDE** sections.
- [ ] Hover Attack for about half a second: a dark tooltip appears under it.
- [ ] Click the Attack value and type `0.01 s`: Attack moves to 10.0 ms.

## Undo, A/B and Compare (Phase 2.1 W)

- [ ] The header has **↶ ↷ | A B → | ≈ Compare** between the I/O pill and the output knob, with thin lines between the groups. Nothing overlaps at 100 % or 200 %.
- [ ] Move a control: **↶** lights up, and its tooltip says what it will undo. Click it: the control goes back. **↷** puts it forward again.
- [ ] Set something you'll recognise, click **B**, change it, then click **A** and **B** in turn while playing: each comes back exactly, with no click.
- [ ] **→** copies the active set into the other one.
- [ ] Save the project with B active and reopen it: B is still active, and A is as you left it.
- [ ] Play a loop and click **≈ Compare**: you hear the input at about the same loudness as the output, and the button shows the matching gain. Click again to go back. No clicks either way.
- [ ] Write some automation and play it back with the window open: Undo doesn't pick it up.

## Character, Upward, Auto threshold and sidechain EQ (Phase 3 Q2.5)

- [ ] Open a project saved with v1.3.0 that uses Fancy Comp: it loads, looks and sounds as before. Render it and null it against a v1.3.0 render if you have one.
- [ ] Open EDIT: the title row has **Detector | Character | Upward | Options**. Options holds the old drawer, and Oversampling now offers 8x.
- [ ] **Character:** on a drum bus, Type Warm, Post, Drive 6 dB: thicker, without a jump in level. Bright: more edge. Switch Type and Position while playing: no clicks.
- [ ] Pre against Post with Drive 18 dB: with Pre the GR meter moves a little less.
- [ ] Type Off: Position and Drive dim, and the sound is exactly the plugin without Character.
- [ ] **Upward:** on a vocal, 3:1 down at -20 dB, then Upward on with Up ratio 1:3: quiet words come up; the green part of the curve and the dot show the lift; the GR meter shows only the reduction. Silence between phrases isn't raised.
- [ ] **Auto threshold:** on a song with a quiet verse and a loud chorus, Threshold so the chorus gets about 4 dB of reduction, then Auto threshold on: the verse gets about the same after a few seconds. The thin white line on the Threshold meter moves, and the offset shows in the Detector page.
- [ ] Stop and start: the offset holds through the pause.
- [ ] **Sidechain EQ:** bell +12 dB at 6 kHz on a vocal, Preview filter on: you hear the sibilance boosted; off, the compressor now ducks on the "s" sounds. Drag the bell's handle on the curve and use the wheel on it: the knobs follow.
- [ ] Low shelf -12 dB at 100 Hz on a mix bus: the kick pumps the mix less.
- [ ] Oversampling 8x: the latency changes once with a short dropout, and REAPER compensates it.
- [ ] The three new presets (Warm Bus, Upward Vocal, Level Rider) load from the preset browser, in Bus and Vocals.
- [ ] Save the project with every stage on, close and reopen: everything comes back, including the drawer's settings.

## Hold, Range, Listen to removed and History (Phase 4 Wave 1)

- [ ] Open a project saved with v1.6.0 that uses Fancy Comp: it loads, looks and sounds as before. Hold reads 0.0 ms and Range Off.
- [ ] The footer has five checkboxes, evenly spaced: Link channels, Auto make-up, Preview filter, **Listen to removed**, Limit output.
- [ ] **Listen to removed:** on a drum bus with a few dB of reduction: you hear only the hits being pulled down. Preview filter on: Listen to removed goes off, and the other way round. Close the window: both off.
- [ ] **Hold:** on a bass, Attack 1 ms, Release 20 ms, RMS size 0: buzzy. Hold 30 ms (Options page): the buzz goes.
- [ ] **Range:** Ratio inf:1, Threshold -40 dB on a loud track: the GR meter stops at Range (try 6 and 12 dB). Top of the knob reads Off. With Auto make-up on, the level comes back by half of Range.
- [ ] **History:** EDIT > History while playing: grey input, teal output, yellow reduction line, dashed threshold. Beat lines move with the music; Grid off hides them. Try each zoom. Click the graph: it freezes and the caption says paused; click again. Click "peak": it clears.
- [ ] Linked off on a stereo source with different channels: a second, lighter reduction line.
- [ ] Save with Hold and Range set, close and reopen: both come back; neither listen switch is on.

## Host sync (Phase 4 Wave 2)

- [ ] Open a project saved with v1.6.0 that uses Fancy Comp: it sounds as before, with its Detector input unchanged.
- [ ] Detector input Host sync on a pad at 120 BPM, Ratio 8, Threshold -20: it ducks on every beat, exactly on the grid; the History page's beat lines line up with the ducks.
- [ ] Rate 1/8 dotted, 1/16 triplet: the ducks follow. Shape changes the length most with a short Release.
- [ ] Stop the song: it keeps ducking at the tempo; play from a bar: the ducks land on the beat again. Loop a bar: no doubled or missing duck at the loop point.
- [ ] Render offline twice: the renders match.

## Mac (docs/mac.md, step 5)

On a Mac (Apple silicon, and Intel if one is available), after installing with `install.sh`:

- [ ] The VST3 and the Audio Unit load in Ableton Live: window, pop-ups and resizing work; Cmd-click (not Ctrl-click) on a control opens the typed-value box.
- [ ] The checks above, once, with the VST3 in Ableton Live.
- [ ] A project saved on Windows opens on the Mac with every setting, and a render matches the Windows render by ear (CI holds them to within -100 dB).
- [ ] The Audio Unit in Live: automation follows, an export matches playback, and the Set reopens as it was. (Logic too, if someone has it.)

## Optical (Phase 4 Wave 2)

- [ ] Open a project saved with v1.6.0 that uses Fancy Comp: it sounds as before, Optical off.
- [ ] EDIT > Options > Optical on a vocal, Ratio 4, about 6 dB of reduction: smoother, slower-breathing compression than with it off at the same knobs; Classic and Auto dim on the main window.
- [ ] Drum loop, heavy reduction for 10 s, then a quiet passage: the gain comes back noticeably more slowly than after a short burst.
- [ ] Hold and Range still change the sound with Optical on; Classic and Auto don't.
- [ ] With Host sync chosen, the Host sync group sits under the Optical checkbox without overlapping it.
- [ ] Render offline twice: the renders match.

## Level history in dB (v1.7.1)

- [ ] EDIT > History on a vocal or a quiet passage: the waveforms are in dB (-60 at the centre line, 0 at the edges), with faint lines and grey labels -12 to -60 at the left. Breaths and quiet syllables are visible; the dashed threshold pair sits at its level (Threshold -24: well out from the centre). The yellow GR line, zoom, Grid, pause and peak work as before.

## Six-band detector EQ, cut slopes and link modes (v1.7.1)

- [ ] Load a project saved with v1.7.0 that uses Comp's detector bands: it sounds the same, and bands 1 to 3 show Low shelf, Bell and High shelf with their old values.
- [ ] EDIT > Detector: click tabs 1 to 6; the band's Type, Freq, Gain and Q show below. Set band 4 to Bell: its handle appears on the curve with a 4; drag it, and the wheel changes its Q. Set a band to Off: its tab dims and its handle goes.
- [ ] Preview filter with Highpass at 90 Hz on a mix with a kick: switch Cut slopes Highpass between 12, 24 and 48 dB; the kick gets quieter in the preview each step, and the curve and its "HP ... dB/oct" label follow.
- [ ] Options > Link mode on a stereo mix with a vocal: Mid/Side keeps the guitars' width steady while the vocal is controlled; M>S narrows the image as the vocal gets loud; S>M turns the middle down by the sides. The GR read-outs say M and S. Back to Stereo: the GR labels return to dB, and switching causes no click.

## History on the main page (v1.7.2)

- [x] The window is taller (934 x 747): the HISTORY card is at the top of the middle, over the knob cards; the knobs are smaller, each with its label and value; the meters and faders are longer; the EDIT drawer has four pages (no History).
- [x] Play a drum loop and turn Ratio, Attack and Release: the teal output and the yellow GR line change as you turn. Drag Threshold: the dashed lines move with the handle.
- [x] Zoom, Grid, pause and peak work as they did on the History page.
- [x] Open EDIT: the drawer opens over the knobs and the waveform shortens above it, still moving; Done brings it back to full height.
- [x] Every knob still types a value (click its value, or Ctrl-click it), and double-click resets it.
- [x] A project saved with v1.7.1 sounds the same.
