# Fancy Limit: 5-minute REAPER check

## Install

1. Close REAPER, then in the project folder run `.\scripts\get-latest.ps1`.
2. Open REAPER and go to Options > Preferences > Plug-ins > VST, then click **Re-scan**.
3. Add **Fancy Limit** to the master track of a finished mix.

## Look

- [ ] The title strip reads **FANCY LIMIT** with LIMIT in red, and the window matches `reference/phase3/20__limit_mockup.png`: Style and Target on top, Threshold and GR on the left, the history and read-outs, the Limiter card, the footer, and Out with its scale and the LUFS bar on the right.
- [ ] The I/O pill reads **2/2**.

## Limiting

- [ ] Play the loudest part and drag Threshold down to about -8 dB: it gets louder, the GR meter and the red area in the history move with the peaks, and nothing distorts audibly.
- [ ] REAPER's master meter never goes above the Ceiling (-1.0 dB). Set Ceiling to -3 dB: the output peaks drop to -3.
- [ ] Turn Lookahead to 0 ms with a lot of reduction: it gets grittier. Back at 2 ms it's clean again. REAPER's latency readout (the plugin's "PDC" in the FX chain) doesn't change while you turn it.
- [ ] Switch **Clean**, **Punchy** and **Dense** on drums: Punchy keeps more snap on the hits, Dense sounds denser and louder.
- [ ] Untick **Auto release** on a bass-heavy section with Release at 10 ms: the bass gets grainy. Tick it again: clean.
- [ ] Tick **Listen to removed**: you hear only the peaks being taken off.
- [ ] Tick it, close the plugin window, and open it again: it's unticked, the sound is normal, and the title strip has no **LISTENING** tag. While it's ticked, the tag is there.
- [ ] Set Link to 0 % on a mix with a loud hit on one side only: the other side no longer dips with it.

## Loudness

- [ ] Click the read-out panel, play the whole song, and check **Integrated** settles to a steady number. Short-term moves with the music, and the LUFS bar follows it.
- [ ] Pick **Broadcast · -23 LUFS** in Target: the red line on the LUFS bar moves down. The sound doesn't change.
- [ ] Pick **Custom…**, type `-12`, and press Enter: the dropdown reads **Custom · -12.0 LUFS** and the red line moves.
- [ ] Render the song (File > Render) with the plugin window closed, then open it: Integrated shows the loudness of the whole render.

## EDIT drawer

- [ ] Click **⚙ EDIT**: the drawer covers the left part with Oversampling, Oversampling filter, Dither, Meter ballistics and Reset loudness. **Done** closes it.
- [ ] Change Oversampling while stopped: the plugin's latency in REAPER's FX chain changes, and playback is still in time with the other tracks. The tooltip mentions the latency.
- [ ] Set Dither to 16-bit shaped and render a 16-bit WAV: no errors, and it sounds the same.

## Presets, menus, undo, A/B and Compare

- [ ] The preset browser lists **Mastering**, **Mixing** and **Utility**, and the ‹ › arrows step through the presets.
- [ ] Hovering a row in any menu shows a red bar at its left, and the current choice has a red check mark.
- [ ] Click the Release value and type `0.2 s`: Release moves to 200 ms.
- [ ] Move a control, click **↶**: it goes back. Set something on **B**, switch A and B while playing: no clicks.
- [ ] With 6 dB of limiting, click **≈ Compare**: you hear the unlimited input at about the same loudness. Click again to go back.
- [ ] The ON pill switches cleanly, and with it off the sound is the untouched mix, in time.
- [ ] Drag the window's corner to make it bigger and smaller: everything scales together, nothing overlaps.
