# Fancy DS: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy DS** to a vocal track with plenty of "s" sounds.

## Look

- [ ] The title strip reads **FANCY DS** with DS in green, and the window matches `reference/phase3/17__ds_mockup.png`: Detector and Mode on top, Threshold and GR on the left, the history over the frequency ruler, the DETECTION and REDUCTION cards, the footer, and Out and Gain on the right.
- [ ] Under the Threshold meter: **-28.0 dB**, with **relative** under it. Untick Relative threshold: "relative" goes away.
- [ ] The I/O pill reads **2/2**.

## De-essing

- [ ] Play the vocal: the GR meter and the green fill at the top of the history move on the "s" sounds, and hardly at all on vowels. The "s" sounds are softer and the voice isn't dull or lispy.
- [ ] Tick **Listen**: you hear mostly the "s" sounds. Drag the blue handle on the ruler: what you hear moves up and down in pitch. Drag the green bar's edge: the band gets wider or narrower, and Width follows. Untick Listen.
- [ ] Click in the history: the green Threshold line jumps there, and the Threshold handle moves with it.
- [ ] Turn Range to 0 dB: nothing is cut. Back to -12.
- [ ] Ride the track's volume fader down 12 dB with Relative threshold on: the "s" sounds are still caught about as much.
- [ ] Switch **Wide**: the whole voice dips on each "s", not just the highs. Back to **Split**.
- [ ] Switch **Shelf**: the bar on the ruler runs from Freq to 20 kHz, and Width is locked: clicking it shows a hint. Back to **Band**.
- [ ] Untick **Link channels** on a stereo source with an "s" on one side only: the other side no longer dips.

## Sidechain

- [ ] Send another track to the plugin's channels 3/4, and pick **Sidechain L+R** in Detector: the cuts follow that track instead.

## EDIT drawer

- [ ] Click **⚙ EDIT**: the drawer shows Lookahead and Split filter. **Done** closes it.
- [ ] Set Lookahead to 2 ms while stopped: the plugin's latency in REAPER's FX chain changes, and the vocal is still in time with the other tracks.
- [ ] Set Split filter to **Linear phase** while stopped: the latency grows to about 53 ms, the vocal stays in time, and it still de-esses. Back to Minimum phase.

## Presets, menus, undo, A/B and Compare

- [ ] The preset browser lists **Vocals** and **Mix**, and the ‹ › arrows step through the presets.
- [ ] Hovering a row in any menu shows a green bar at its left, and the current choice has a green check mark.
- [ ] Click the Freq value and type `7.5k`: Freq moves to 7.50 kHz.
- [ ] Move a control, click **↶**: it goes back. Set something on **B**, switch A and B while playing: no clicks.
- [ ] Click **≈ Compare**: you hear the input at about the same level. Click again to go back.
- [ ] The ON pill switches cleanly, and with it off the sound is the untouched vocal, in time, with and without Lookahead.
- [ ] Render the track (File > Render): it sounds the same as playback.
- [ ] Drag the window's corner to make it bigger and smaller: everything scales together, nothing overlaps.
