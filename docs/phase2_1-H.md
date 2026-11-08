# Phase 2.1, item H: host compatibility

Brief: §H of `docs/BUILD_PROMPT_PHASE2_1.md`. Hosts: REAPER, Audacity, UA LUNA, Tracktion Waveform (Free and Pro), Cubase and Ableton Live, on Windows. Pro Tools is out of scope.

H adds no parameter. It doesn't change the sound in real-time playback, the latency or the saved state, so `stateVersion` stays at 1 and projects from v1.1.0 load and render bit-identical. Its CPU cost is in offline rendering only, where waiting is allowed.

## 1. Where the plugins stand

From reading the code at the end of S:

| Area | Today | Gap |
|---|---|---|
| Offline (H.1) | EQ and MultiComp design their linear-phase filters in-line, and FIR builds its engine in-line, when `isNonRealtime()` is true. Verb waits for a pending IR build before each offline block, and computes its convolution tail in-line. Kicker, Exciter, Comp, Delay and Gate have no background work. | The worker threads only start if `prepareToPlay` ran in real time, so a host that switches back to real time without preparing again leaves them stopped. Nothing tests a render against playback with automation, random blocks, an IR swap or an FFT-size change. |
| Keyboard (H.2) | Only the typed-value field, menus and dialogs take focus. `ShellEditor` uses Ctrl+S and leaves every other key. | Nothing proves that no control takes focus or swallows a key. |
| Scaling (H.3) | The plugin's own 100-200 % scales the layout root, pop-ups included. JUCE's VST3 wrapper passes the host's content scale to `setScaleFactor`. | No render at host scales, so nobody has looked. |
| Sidechain (H.4) | Comp, MultiComp, FIR and Gate declare an optional "Sidechain" input, stereo, inactive by default, and accept it mono or stereo. Only Gate warns when it's silent. | Comp, MultiComp and FIR need the warning. EQ gets its bus with item D. No test toggles the bus while processing. |
| MIDI out (H.5) | Gate declares MIDI output and writes notes into the block's MIDI buffer. | Untested with no MIDI buffer or with nothing connected. |
| Latency (H.6) | Every plugin calls `setLatencySamples` when its latency changes, and JUCE only notifies the host if the value differs. | No test counts the notices. No UI note about hosts that read latency late. |
| CI (H.7) | pluginval at strictness 10. Its Steinberg validator step is skipped, because no validator was built. | No validator, no host harness, no AudioPluginHost. |

## 2. Design

### H.1 Offline rendering

The rule: when the host says non-real-time, every bit of work that would go to a background thread is done before the block is processed, on the audio thread. In real time the threads do it, as now. The check is made **per block**, from `isNonRealtime()`, which JUCE sets from the VST3 process mode, so a host can switch between the two at any time.

| Plugin | Background work | Offline |
|---|---|---|
| EQ | Linear-phase FIR design thread | Designs in-line (exists). The thread now always runs, and skips its work while offline, so it's there when the host goes back to real time. |
| MultiComp | Linear crossover FIR design thread | Same as EQ. |
| FIR | Engine build thread (FFT size, mode, curve, profile) | Builds in-line (exists); the thread always runs, as above. |
| Verb | IR build thread, and the convolver's worker for the tail partitions | Waits for a pending IR build, and computes all partitions in-line (exists). The wait has no time limit offline: dropping or delaying audio is never allowed there. |
| Kicker, Exciter, Comp, Delay, Gate | None | Nothing to do. |

The editors' analyzer threads only draw, so they're left alone.

**Test, every plugin in each of its modes:** a 30 s signal (pink noise with sine bursts, so filters, detectors and tails all work), with automation of two or three parameters per plugin written at fixed sample positions. Rendered (a) in real time with 128-sample blocks and (b) offline with random blocks of 0 to 8192 samples, each automation point starting a new block in both. The outputs must be bit-identical, or within -120 dB of the signal where the plugin isn't deterministic across block sizes, and the test says which it is. Verb swaps its IR, and FIR changes FFT size, half way through. Real-time Verb and FIR wait for their builds at the same point in the render, which is what a real-time render would hear if the CPU kept up; the test is about offline being right, not about a slow worker.

Modes: Kicker as is; Exciter and Comp at 1×, 2× and 4×; EQ minimum and linear phase; Verb zero latency and low CPU; Delay note and ms taps; MultiComp LR 24, LR 48 and Linear; FIR all five modes; Gate with Lookahead off and on.

### H.2 Keyboard

- The editor and every control leave `setWantsKeyboardFocus` off, so a click on a knob doesn't take the keys away from the host.
- Only the typed-value field, an open menu and an open dialog take focus, and each gives it back when it closes (S already does this).
- `ShellEditor::keyPressed` keeps only Ctrl+S, and W will add Ctrl+Z and Ctrl+Shift+Z / Ctrl+Y the same way. Everything else returns false, which JUCE's Windows peer passes on to the host's window.

**Test:** for each editor, after building it and after clicking a few controls, no component has keyboard focus or wants it, apart from text fields; and space, Enter, Escape, letters, digits and arrows sent to the editor and to every component in it return "not consumed".

### H.3 Scaling and high DPI

The host's scale goes through JUCE's `AudioProcessorEditor::setScaleFactor`, on top of the plugin's own UI size. Both are transforms, so everything is drawn as vectors at the final size: no bitmap is scaled. Pop-ups sit under the same root, so they follow both.

**Test:** `fancy-screens` renders each editor, and one menu, at host scales 1.25, 1.5 and 2.0, into `screens-hostscale-<sha>`. A test checks the editor's size follows the scale and that every component stays inside the window at each scale. Clipped text is checked by eye from the renders, and noted in `docs/PROGRESS.md`.

### H.4 Sidechain

- The bus stays as it is: an optional input named "Sidechain", inactive by default, mono or stereo.
- An inactive bus reads as silence. A mono one feeds both detector channels.
- **Warning:** Gate's detection, sidechain below -90 dBFS for 2 s while the main input is above -60 dBFS and a sidechain source is chosen, moves into `fancy-core` as `SidechainWatch`. Comp, MultiComp and FIR show the same message, in a small red strip that appears over the top of the body only while the warning is on. That isn't a control, so the layout doesn't change. The text loses Gate's REAPER pins note, which doesn't help in other hosts: "No sidechain signal: check the sidechain routing in your host." The manuals say where that routing is in each host.
- **Tests:** for Comp, MultiComp, FIR and Gate: inactive, mono and stereo sidechains; the bus switched on and off while processing, with no NaN and no output step beyond the input's; and the warning on with a silent sidechain selected, off without one.
- **Manuals:** "How to sidechain" for REAPER (pins 3/4), Cubase (the sidechain button at the top of the plugin window), Live 10.1+ (the Sidechain section of the device), Waveform (the input button at the top left of the plugin window) and LUNA, plus a line that Audacity has no sidechain routing. EQ's comes with D.

### H.5 MIDI out (Gate)

Gate keeps its event output. A test processes with a MIDI buffer the host never reads and with the bus disconnected (JUCE's VST3 wrapper hands the plugin an empty buffer then), and checks the audio is identical to a normal run. The manual's host table starts "untested" and is filled in from the user's host tests.

### H.6 Latency changes

`setLatencySamples` is already called on every change, and JUCE's VST3 wrapper turns it into `restartComponent (kLatencyChanged)`. Settings that change latency:

| Plugin | Setting |
|---|---|
| Exciter, Comp | Oversampling |
| EQ | Phase, FIR length, Oversampling |
| MultiComp | Crossover type (Linear), Limit output |
| FIR | FFT size, Low latency |
| Gate | Lookahead |
| Verb | Latency (zero / low CPU) |

Each of those gets the tooltip line "Latency changes take effect in some hosts after you stop and start playback." Settings that have a tooltip keep theirs, with the line added after it.

**Test:** an `AudioProcessorListener` on each processor counts `latencyChanged` notices; switching each setting during processing gives exactly one, and switching back gives one more.

### H.7 Checks in CI

1. **Steinberg's validator.** Built from Steinberg's `vst3sdk` repository at a pinned tag (`validator` target, hosting examples on, plugin examples off), cached by tag so it's built once. It runs on every `.vst3` in every build, and any failure fails the build. pluginval also gets its path, so its own Steinberg step stops being skipped.
2. **Host-behaviour harness.** A new console tool, `fancy-hosttest`, loads each built `.vst3` through JUCE's VST3 hosting, as a host does, and runs:
   - offline with random blocks of 0 to 8192 samples, against real time (H.1);
   - 44.1 → 48 → 96 → 44.1 kHz without reloading;
   - deactivate and reactivate mid-session, and processing after 60 s of silence;
   - state restored before the first `prepareToPlay` and after it; JUCE's hosting sets the component state and then the controller state, the VST3 split, and the parameters read back through the controller must match;
   - sidechain inactive → active → inactive while processing, and mono and stereo main buses;
   - host bypass on and off with latency, with the bypassed output time-aligned to the processed one;
   - every parameter changed every block for 10 s, with no NaN or inf;
   - the editor opened and closed 50 times, and resized, with no crash.

   **Allocations:** the plugins link the C runtime statically, so a hook in the harness can't see the plugin's allocations. The no-allocation part of the automation flood runs in `fancy-tests` instead, where the plugin code is compiled in: a counting `operator new` that fails if the audio thread allocates during `processBlock`. Leaks are left to JUCE's leak detector, which reports at exit.

   `docs/host-compat.md` lists where each scenario comes from: host documentation, a forum report or the VST3 spec.
3. **AudioPluginHost.** JUCE's `extras/AudioPluginHost` is built once per JUCE version and cached, and added to the VST3 artifact as `tools/AudioPluginHost.exe`. It shows sidechain and MIDI routing as wires, so the user can check them without a paid DAW.

### H.8 Real-host plan

`docs/host-compat.md`: one row per plugin and one column per host, plus the cross-cutting rows (loads, UI and pop-ups, 150 % scaling, space bar reaches the host, automation, save and reload, offline render matches playback, sidechain, MIDI out, latency compensation). Every cell starts "untested". A 10-15 minute checklist per Tier 1 host (REAPER, Audacity, Waveform Free, LUNA, AudioPluginHost), and a longer one per Tier 2 host (Live 12 Suite trial, Cubase trial) for one week of a trial, after S and H are done. Waveform Pro is covered by Free unless a check needs a Pro feature, which the checklist says.

### H.9 Degrade gracefully

Covered by the above: no sidechain means the plugin works on its main input and warns only when a sidechain source is chosen; no MIDI routing means Gate still gates; no latency compensation breaks nothing, and the manual explains the delay. A test loads each plugin with only a mono main bus and no sidechain, and checks it processes.

## 3. Order of work

To save build minutes, in three pushes:

1. H.1, H.2, H.4, H.5, H.6 in the plugins and `fancy-tests`, and H.3's renders.
2. H.7: the validator, the harness and AudioPluginHost in CI.
3. `docs/host-compat.md`, the host checklists and the manuals' host sections.

## 4. New dependency

Steinberg's `vst3sdk` repository, for the validator only. The brief names it in H.7.1. It's used as a tool in CI and isn't linked into the plugins. JUCE's AudioPluginHost comes with JUCE, which is already a dependency.
