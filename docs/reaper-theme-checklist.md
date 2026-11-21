# REAPER theme: checklist (Stage A)

For the user, in REAPER 7.86 at 125 %. Install as in `theme/README.md`. Send a screenshot for each item marked 📷, and note anything that looks wrong.

## Setup

- [ ] Fancy Plugs is selected under Options > Themes.
- [ ] Dark mode is "Forced on" and REAPER restarted. 📷 the title bar and an open menu (File, and a right-click on a track).

## Main window (📷 the whole window with a few coloured tracks and the mixer open)

- [ ] Toolbar, dockers and window background are dark grey, text white.
- [ ] Track panel area and mixer background are grey (`#262626` / `#303030`), track names white.
- [ ] Arrange: very dark lanes, faint grid, white edit cursor, **teal** play cursor.
- [ ] Ruler: dark, grey numbers; a time selection shows teal in the ruler.
- [ ] Meters: yellow at the top to green at the bottom, red clip indicator.
- [ ] Track and item labels, ruler and transport clock are in Inter (the transport clock a bolder Inter).

## Colour palette action (📷 the palette open, with the mouse over a swatch)

- [ ] It opens at the mouse, at the right size for 125 %.
- [ ] 12 colours in rainbow order on the top row, their dark shades below, "Default colour" underneath.
- [ ] Hover: a white ring round the swatch; the bottom line names it with its hex value.
- [ ] Clicking colours the selected tracks and closes it; Ctrl+Z undoes it in one step.
- [ ] With all selected tracks in one palette colour, that swatch shows a check mark.
- [ ] With no tracks selected it says "Select tracks first" and clicking does nothing.
- [ ] Esc closes it.

## Light tint

- [ ] With REAPER's track colour options set to tint panels and colour items, each coloured track shows a light tint on its panel and coloured items. 📷 Note which options you used.

## Other windows

- [ ] FX browser and media explorer lists: grey, with a dark teal selection.
- [ ] MIDI editor: dark grey background, light white keys, teal for the pressed key and the selected notes' area. 📷

# Stage B preview (images)

Pick **Fancy Plugs (Stage B preview)** under Options > Themes. Stage A stays installed as **Fancy Plugs**, so switching back is one click.

- [ ] It loads without an error, and the colours are Stage A's. 📷 the whole window with the mixer open.
- [ ] Track panel and mixer buttons are the suite's rounded grey buttons: record arm (red dot when armed), M (orange when on), S (yellow when on), phase, monitor (speaker), envelope, FX (teal when the track has FX), IO (dots for sends and receives), IN/OUT/OFF. 📷 a track armed, muted and soloed.
- [ ] Hovering lightens a button; pressing darkens it.
- [ ] Faders and pan: thin dark tracks with rounded ends, black caps with a white line. 📷 the track panel and a mixer strip.
- [ ] The track name field is a dark rounded well.
- [ ] Transport: rounded buttons; play and repeat teal when on, record red when recording. 📷 while playing with repeat on.
- [ ] Everything is sharp at 125 % (it should use the 150 % images).
- [ ] Note anything cut off, stretched, misaligned or missing.

## Stage B, second round

Re-select **Fancy Plugs (Stage B preview)** under Options > Themes so REAPER reloads it. First round: passed in the user's screenshots on 21 November 2026, apart from what this round adds.

- [x] Main toolbar: lighter grey buttons with bold icons, each in its own colour; toggles (metronome, grid, snap, ripple, crossfades, lock…) filled with that colour when on, grey when off. 📷 the toolbar.
- [x] Beside FX on the track panel: a small switch, teal dot with FX on, grey ring with the chain bypassed.
- [x] The input FX button (next to IN on an armed track) is a rounded "FX", teal when it has input FX.
- [ ] The record input box on an armed track is a dark grey rounded box; its text and arrow are readable. 📷 Not in the built-in layout, which draws its own box over the meter: moved to Stage C.
- [x] The folder button on a normal track is an outlined folder.
- [x] The Rate slider at the bottom is a thin dark track with a black cap.
- [x] Play turns teal while playing. 📷 the transport while playing.
- [x] The last two main toolbar buttons, marquee selection and razor editing, show the theme's icons (a yellow dashed box with a cursor, a red razor blade), not REAPER's.

# Stage C preview (layouts), first round: track panel

Pick **Fancy Plugs (Stage C preview)** under Options > Themes. Have three tracks: one coloured red, one coloured blue, one with no colour; arm one of them. 📷 the window with the mixer open.

- [x] Each track panel has two rows: record arm, name, M, S, FX and a small switch; then a volume knob, its dB value, a pan knob, envelope and polarity, and IO.
- [x] A thin strip in the track's colour on the left edge; none on the uncoloured track.
- [ ] The knobs are grey discs whose pointer is the track's colour (red, blue), white on the uncoloured track. Turning them works; double-click resets.
- [x] On the armed track, envelope and polarity give way to the input box, monitoring, record mode and input FX.
- [ ] A thin stereo meter on the right edge moves with playback.
- [ ] Make a track short (drag its bottom edge up): only the first row shows. Make one tall: the rows stay at the top.
- [ ] Make the track panel wider and narrower: the name and input box stretch, the buttons stay together on the right.
- [x] The mixer and transport still look as in Stage B (REAPER's own layout). Note anything missing there.

## Stage C, mixer

Re-select **Fancy Plugs (Stage C preview)**. Same three tracks, one armed, the mixer open and tall enough to show a full strip. 📷

- [x] Each strip: FX list at the top, a pan knob with its pointer in the track's colour, the fader and a thin stereo meter, a column of small buttons beside them, the dB value, then M, S and record arm.
- [x] The name sits on a bar in the track's colour with dark text; white on grey for the uncoloured track.
- [x] On the armed strip the dB value gives way to the input box, and monitoring and input FX replace envelope and polarity in the button column; nothing overlaps.
- [ ] The master: wider meter, teal pan pointer, a menu button (three lines) in the column, M, S and mono (one ring, two rings in stereo), the name on a teal bar.
- [x] Making the mixer taller or shorter stretches the fader and meter; the bottom rows stay at the bottom.
- [ ] With the FX list hidden (right-click the mixer, Show FX inserts), no empty band at the top of the strips; with it shown, the list sits above the pan knob.

## Stage C: transport, meters, compact layouts

Re-select **Fancy Plugs (Stage C preview)**. Same three coloured tracks, one armed; play something with the mixer open.

Transport 📷 (stopped, and once while playing)
- [ ] One row, centred: start, play, pause, stop, end, record, repeat, automation; then the time on a dark well, BPM and time signature, play rate with its slider, and the selection.
- [ ] Make the transport narrower: the row stays centred, and the rate, its slider and the selection drop out before anything overlaps.

Meters 📷 (while playing)
- [ ] Track panel, mixer and master meters are solid (no stripes): red at the top, yellow, green at the bottom; the unlit part a dark trace of the same colours; the clip end red when it clips.
- [ ] No dB numbers on the track panel and track strip meters; the master keeps its scale.
- [ ] The track panel's narrow vertical meter shows the colours the right way up (red at the top). Note it if it's sideways or striped.

Compact layouts 📷
- [ ] Right-click a track's panel > Layout > Compact (or the track's Layout menu): one 28 px row with strip, number, record arm, name, two knobs, M, S, FX, the switch and the meter.
- [ ] Right-click a mixer strip > Layout > Compact: a 52 px strip with pan knob, fader and meter, FX and record arm, M and S, and the name bar.
- [ ] Switching back to the default layout restores the full panel and strip.
