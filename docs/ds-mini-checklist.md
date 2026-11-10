# Fancy DS-Mini: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy DS-Mini** to a vocal track with plenty of "s" sounds.

## Look

- [ ] The title strip reads **FANCY DS-MINI** with DS-MINI in blue, and the window matches `reference/phase3/22__ds_mini_mockup.png` below the header: the profile pill with ‹ › inside it, the band under it, the history, the big blue Amount knob, Listen and ⚙ EDIT, and GR and Out on the right.
- [ ] The line under the profile reads **Sibilance · 4.3–11.4 kHz**.

## De-essing

- [ ] Play the vocal: the GR meter and the blue fill at the top of the history move on the "s" sounds, not on vowels, and the "s" sounds are softer.
- [ ] Turn Amount to 0: nothing changes at all. Turn it up to 10: the "s" sounds are much softer, and the dashed line moves down.
- [ ] Click › while playing: the profile steps to Male voice, the band line changes, and there's no click. Step through all six.
- [ ] Tick **Listen**: you hear only what's removed, the checkbox is filled blue, and the title strip shows a blue **LISTENING** tag. Close the window: the sound is normal. Open it: Listen is off.

## EDIT drawer

- [ ] Click **⚙ EDIT**: Mode, Lookahead, Link channels and **Save as Fancy DS preset…**. **Done** closes it.
- [ ] Set Lookahead to 2 ms while stopped: the plugin's latency in REAPER's FX chain shows 96 samples at 48 kHz, and the vocal stays in time.
- [ ] Pick Male voice and Amount 6, click **Save as Fancy DS preset…**, and call it `Test`. Add Fancy DS to the same track after DS-Mini, bypass DS-Mini, and load **From DS-Mini › Test** in Fancy DS's preset browser: it sounds the same as DS-Mini did, and Freq reads 5.00 kHz and Threshold -24.0 dB.

## Presets, menus, undo, A/B and Compare

- [ ] The preset browser lists **Vocals** and **Instruments**.
- [ ] Hovering a row in any menu shows a blue bar at its left.
- [ ] Click Amount's value and type `7.5`: Amount moves to 7.5.
- [ ] Move Amount, press Ctrl+Z: it goes back. Switch A and B while playing: no clicks.
- [ ] The ON pill switches cleanly. Render the track: it sounds the same as playback.
- [ ] Drag the window's corner: everything scales together.
