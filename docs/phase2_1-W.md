# Phase 2.1, item W: undo and redo, A/B, and level-matched Compare

Brief: §W of `docs/BUILD_PROMPT_PHASE2_1.md`. Mockup: panels 1 and 2 of `reference/phase2_1/14__phase2_1_controls.png`, source `design/phase2_1-mockup-src/p21.html`. Everything is built once, in `fancy-core`, for all nine plugins.

W adds no host parameter. It adds one block to the saved state (the A/B slots), so `stateVersion` goes from 1 to 2. Latency is unchanged. Compare has no CPU cost while the editor is closed.

## 1. Where the plugins stand

- **State:** every plugin keeps its settings in host parameters, except Verb's module chain (a value-tree child) and FIR's curves (a child) and learned noise profile (atomics, written into the state on save). EQ bands, Delay taps and MultiComp bands are parameters.
- **User edits:** the shared controls use JUCE's attachments, which wrap every change in a change gesture: drags, clicks and typed values. Most custom controls do too (EQ graph drags, Gate's threshold history, Delay's tap map, MultiComp's band graph). A few code paths set parameters without one: EQ's band add, reset and delete, mouse-wheel Q and log-scale remapping; MultiComp's split, merge, copy and reset; and preset loads.
- **Header:** large plugins place preset, `+ New`, `Parameter` and the I/O pill on the left, and the output knob and `⏻ ON` on the right. Four plugins already use the space between: Delay's tempo label, FIR's latency label, MultiComp's Detector dropdown and EQ's `⚙ EDIT` link.
- **Keyboard:** after H, no control takes focus, and `ShellEditor` only handles Ctrl+S.

## 2. Design

### 2.1 Snapshots (shared by undo and A/B)

A `Snapshot` is the plugin's settings:

- every parameter's normalised value, as `getValue()` returns it, so restoring is bit-exact. `bypass` is left out: `⏻ ON` is the plugin's own bypass, and neither undo nor A/B should flip it;
- the state's non-parameter children (Verb's chain, FIR's curves and profile), taken the way a save takes them (`copyState` plus `addToState`).

UI properties (scale, window size) and the preset name aren't settings, so they aren't in it.

**Restoring** a snapshot changes only what differs: each parameter whose value differs is set with `setValueNotifyingHost`, with no gesture, so the host's automation lanes follow without REAPER writing automation (W.3). The non-parameter children are replaced and `stateApplied()` runs only if they differ, so undoing a knob move never rebuilds Verb's impulse or FIR's engine. Verb and FIR reach a changed chain, curve or profile through their existing off-thread rebuild-and-crossfade path.

### 2.2 Undo history (W.2)

`fancy::UndoHistory`, owned by `FancyProcessor`, so it survives closing the editor, and isn't saved.

**What counts as a user edit:**

1. **A parameter inside a change gesture.** The history listens to every parameter's `parameterGestureChanged`. Only our editor starts gestures; a host's automation, its generic UI and its own undo never do. Each parameter touched by a gesture is marked.
2. **A user edit scope.** `FancyProcessor::UserEdit` (RAII) marks every parameter that changes inside it. It wraps the code paths in §1 that set parameters without a gesture, and preset loads.
3. **State outside the parameters.** `noteEdited()` already counts these (Verb's chain, FIR's curves). FIR also counts a finished noise-profile learn, which completes on the audio thread, through a virtual `getEditCounter()` that adds its profile version.

**When a step is made:** a 30 Hz timer, on the message thread, commits pending edits when no gesture is open, no user edit scope is open and no mouse button is down. So a knob drag, a curve drag or a module-knob drag is one step, and everything one click or one menu item does is one step. The step holds, for each marked parameter whose value changed, its value before and after, plus the non-parameter children before and after if they changed. A step that changes nothing is dropped. Tests call `commit()` directly.

**Never recorded:** a host's parameter changes (no gesture, no scope), state loaded by the host (`setStateInformation` rebases the history on the new state and keeps the steps, so REAPER's own undo doesn't add one), meter and analyzer state (not settings) and Compare (not a setting).

**Undo and redo** restore the step's before or after values (§2.1) with recording paused, then rebase. The limit is 100 steps; the oldest goes first. A new edit clears redo.

**Undoing a preset load** restores the settings, but the preset dropdown keeps the loaded name, marked as changed (•), because the name isn't a setting.

**Keys:** Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y in `ShellEditor::keyPressed`, which only runs while the plugin window has the keyboard focus. Every other key still goes to the host. H's keyboard test changes to expect these three to be handled.

### 2.3 A/B (W.3)

Two slots, each a snapshot. The live state is the active slot; the other is stored.

- **Switch:** the live snapshot goes into the inactive slot, the inactive snapshot is restored (§2.1), and the active flag flips. One undo step, which switches back.
- **Copy (`→`):** the inactive slot becomes a copy of the live state. One undo step, which puts the old slot back.
- **Saved with the project:** a `WORKFLOW` child in the state holds the active slot (`A` or `B`) and the inactive slot as `PARAM` children (plain values, like the main state, so unknown parameters are dropped and new ones keep their defaults) plus its non-parameter children. `applyState` takes it out before the plugin sees the state.
- **Old projects** (no `WORKFLOW`): slot A is active, and slot B starts as a copy of A. Nothing else changes, so they load and render bit-identically.

**The 20 ms crossfade:** see question 2 in §4.

### 2.4 Compare (W.4)

`fancy::dsp::CompareStage`, run by `FancyProcessor` around each block.

- **Wrapping the block:** `FancyProcessor` makes `prepareToPlay` and `processBlock` final, and calls the plugin's `prepareAudio` and `processAudio`, so each plugin gets Compare without per-plugin code. This is a rename in each plugin.
- **Dry path:** before the plugin runs, the main input channels go into a delay line of the reported latency (up to 131,072 samples, allocated in `prepareToPlay`), so the dry signal is time-aligned with the output.
- **Loudness:** ITU-R BS.1770 K-weighting (the two standard biquads, recomputed for the sample rate), mean square summed over channels, short-term over 3 s as 30 blocks of 100 ms. Both the processed output and the delayed dry signal are measured while Compare is off.
- **Gain:** processed loudness minus dry loudness, clamped to ±24 dB. It isn't updated while either is below -70 LUFS, and it's frozen when Compare goes on.
- **Output:** while on, the delayed dry signal times the gain. Switching either way is a 20 ms equal-power crossfade. `⏻ ON` is separate: it acts inside the plugin, as now.
- **Not a parameter:** an atomic flag that the editor sets. It isn't saved, isn't automatable and isn't in the undo history. It turns off when the editor closes, so nobody leaves a plugin compared without knowing.
- **CPU when unused:** the measurement and the dry delay only run while an editor is open. With the editor closed the stage is one flag check per block.
- **Read-out:** "+2.4 dB" (the frozen gain, one decimal) beside the button while Compare is on. See question 1 in §4.

### 2.5 Controls (W.1)

Built from S's components: icon buttons, a two-segment `A | B` switch and pill buttons, with the mockup's sizes: 24 × 22 icons, 1 px `#434343` separators with 6 px either side, disabled icons in `textDisabled`. Undo and Redo show the step's name in their tooltip ("Undo Threshold").

- **Large plugins** (EQ, Comp, Verb, Delay, MultiComp, FIR, Gate): `↶ ↷ | A B → | ≈ Compare`, between the I/O pill and the output knob. Compare is lit while on.
- **Compact plugins** (Kicker, Exciter): `A | B` takes the I/O pill's place in the header. The Prm menu gets, above its parameter list, Undo (Ctrl+Z), Redo (Ctrl+Shift+Z), Copy A → B, Compare (level-matched), then Parameters…, which opens the parameter list as a submenu, and "I/O: 2 in / 2 out" (disabled), as in panel 2. UI size and About stay at the bottom.

## 3. Latency and CPU

- **Latency:** unchanged in every plugin. Compare's dry path uses the latency the plugin already reports.
- **CPU:** undo and A/B run on the message thread. Compare costs nothing with the editor closed. With it open, its measurement (K-weighting and a squared sum on 2 × 2 channels) adds about 0.1-0.2 % at 48 kHz, 0.1-0.3 × Fancy Comp's default cost. That's more than this note first estimated ("well under 1 %" of Comp), and it only runs while the window is open. Measured by the §0.4 method in `docs/measurements/workflow.txt`.

## 4. Questions for the user

Both were built as recommended, while the user was away, and are waiting for the user's decision. The click test answered question 2 with numbers: no plugin's output jumps when switching from its defaults to its last factory preset (§5), so the test is now required.

1. **Header space (layout change).** The large header doesn't have room for the workflow group in the plugins under 800 px wide. The group needs about 250 px between the I/O pill and the output knob (about 300 px with Compare's read-out). Today there's 380 px on Comp, 240 px on Verb, 255 px on EQ (less its EDIT link), 205 px on Delay, FIR and MultiComp (less their label or dropdown), and 185 px on Gate. Recommended:
   - Built: on EQ, Delay, FIR, MultiComp and Gate, the left buttons use the compact labels (`+`, `Prm`, `4/2`), and the preset dropdown is narrower: 120 px on Delay, FIR and MultiComp, 150 px on Gate and 170 px on EQ. Verb keeps the full labels with a 230 px dropdown; Comp is unchanged.
   - Built: everything stays in the header. Delay's tempo label and EQ's `⚙ EDIT` are unchanged. FIR's latency label shows milliseconds only ("Latency 42.7 ms"). MultiComp's Detector dropdown loses its "Detector" label, which moves into its tooltip.
   - Built: Compare's read-out ("+2.4 dB") is drawn inside the lit button, in place of its name, rather than beside it, because none of the narrow headers has room for both.
2. **A/B crossfade.** The brief asks to render the old and new settings in parallel for the 20 ms fade. For the seven plugins without a rebuild path, that means a second copy of each plugin's DSP, copied from the live one on the audio thread at the switch (Delay's lines alone are several MB). EQ's and MultiComp's design threads would also have to be made copyable. Recommended instead: every continuous parameter already glides over 20-50 ms (§4.2), Verb and FIR crossfade their rebuilds, and the test (no sample jump beyond what the crossfade allows) is run on all nine plugins with random A and B settings. Any plugin that fails gets the parallel render, for the parameters that jump.

## 5. Tests (Catch2, all nine plugins)

- **Undo:** 50 random user edits (gestures, user edit scopes, and for Verb and FIR, structural edits), then 50 undos, give a snapshot identical to the start: parameters bit-exact, non-parameter state equivalent. 50 redos give the end state.
- **Automation:** host-style parameter changes (no gesture), from the audio thread and the message thread, before, during and after user edits, add no steps. `setStateInformation` adds none.
- **Steps:** one drag is one step; 101 edits keep 100; a new edit clears redo; a preset load is one step; a step that changes nothing isn't kept.
- **A/B:** A → B → A restores a snapshot identical to A; copy works and is one step; switching is one step and its undo switches back; both slots and the active flag survive save and load; a v1.1.0 state loads with B equal to A and renders bit-identically (the §0.3 test).
- **Switch click:** a sine through each plugin, switching between random A and B settings; the largest sample-to-sample step within 50 ms of the switch is no more than the largest of the old and new outputs' own steps, plus what a 20 ms crossfade between them can add.
- **Compare:** EQ with a +6 dB bell at 1 kHz on pink noise; after 3 s, Compare's output is within 0.5 LU of the processed output's loudness. Toggling it shows no step beyond the crossfade. The gain stays within ±24 dB, holds below -70 LUFS and doesn't change while Compare is on. The output is the dry signal, delayed by the reported latency, in every plugin.
- **Keys and UI:** Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y are handled and every other key still goes to the host. The buttons enable and disable with the history. The compact Prm menu has the mockup's rows. Every new menu, tooltip and button is in the screenshot gallery.
- **Real time:** the flood test (no allocation, no NaN) includes Compare on, Compare off and A/B switches.
