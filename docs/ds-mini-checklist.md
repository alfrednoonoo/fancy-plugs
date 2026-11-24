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

## Auto Amount (Phase 4 Wave 1)

- [ ] Open a project saved with v1.6.0 that uses Fancy DS-Mini: it loads and sounds as before, with every new setting off or at its default.
- [ ] Auto on: harsh passages get more reduction and soft ones less, never more than 3 from the knob; the arc moves.

## Hiss profile (Phase 4 Wave 2, v1.7.1)

- [ ] The profile list shows **Hiss** after a divider; the caption reads "Hiss · above 5.0 kHz, in the gaps".
- [ ] On a hissy take, Hiss at Amount 5: the pauses lose most of their hiss and the singing stays bright. Amount 0 leaves it untouched.
- [ ] **Save as Fancy DS preset** with Hiss, load it in Fancy DS: Detection is Hiss and it sounds the same.
- [ ] Open a project saved with v1.7.0 that uses Fancy DS-Mini: its profile and sound are unchanged.

## Mac (docs/mac.md, step 5)

On a Mac (Apple silicon, and Intel if one is available), after installing with `install.sh`:

- [ ] The VST3 and the Audio Unit load in Ableton Live: window, pop-ups and resizing work; Cmd-click (not Ctrl-click) on a control opens the typed-value box.
- [ ] The checks above, once, with the VST3 in Ableton Live.
- [ ] A project saved on Windows opens on the Mac with every setting, and a render matches the Windows render by ear (CI holds them to within -100 dB).
- [ ] The Audio Unit in Live: automation follows, an export matches playback, and the Set reopens as it was. (Logic too, if someone has it.)
