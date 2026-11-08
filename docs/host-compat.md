# Host compatibility

Phase 2.1 item H. The suite should work in REAPER, Audacity, UA LUNA, Tracktion Waveform (Free and Pro), Cubase and Ableton Live on Windows. Pro Tools is out of scope, as it needs AAX.

Every cell starts as "untested" and only changes from the user's own results. CI can't run these hosts; what it does check is in "Automated checks" below.

## Results

Tier 1 hosts are free and get checked after every Phase 2.1 item. Tier 2 hosts are trials, checked once, within one week of the trial, after S and H are both done.

### Per plugin: loads, and the window and pop-ups work

| Plugin | REAPER | Audacity | Waveform Free | LUNA | AudioPluginHost | Live 12 (trial) | Cubase (trial) |
|---|---|---|---|---|---|---|---|
| Kicker | untested | untested | untested | untested | untested | untested | untested |
| Exciter | untested | untested | untested | untested | untested | untested | untested |
| Comp | untested | untested | untested | untested | untested | untested | untested |
| EQ | untested | untested | untested | untested | untested | untested | untested |
| Verb | untested | untested | untested | untested | untested | untested | untested |
| Delay | untested | untested | untested | untested | untested | untested | untested |
| MultiComp | untested | untested | untested | untested | untested | untested | untested |
| FIR | untested | untested | untested | untested | untested | untested | untested |
| Gate | untested | untested | untested | untested | untested | untested | untested |

### Across the suite

| Check | REAPER | Audacity | Waveform Free | LUNA | AudioPluginHost | Live 12 (trial) | Cubase (trial) |
|---|---|---|---|---|---|---|---|
| UI and pop-ups look right | untested | untested | untested | untested | untested | untested | untested |
| Scaling at 150 % | untested | untested | untested | untested | untested | untested | untested |
| Space bar reaches the host | untested | untested | untested | untested | n/a | untested | untested |
| Automation writes and plays back | untested | n/a | untested | untested | n/a | untested | untested |
| Save and reload project | untested | n/a | untested | untested | untested | untested | untested |
| Offline render or export matches playback | untested | untested | untested | untested | n/a | untested | untested |
| Sidechain (Comp, MultiComp, FIR, Gate) | untested | n/a: no sidechain routing | untested | untested | untested | untested | untested |
| MIDI out (Gate) | untested | n/a: no MIDI routing | untested | untested | untested | untested | untested |
| Latency compensation | untested | n/a | untested | untested | n/a | untested | untested |

Waveform Pro is covered by Waveform Free; no check below needs a Pro-only feature.

### Host limits found so far

None yet. When a host fails a check because of the host itself, it goes here and in the plugin's manual.

## Automated checks

These run in CI on every build, against the built `.vst3` files, and need no host licence.

- **pluginval**, strictness 10, with Steinberg's validator step on.
- **Steinberg's VST3 validator**, built from `steinbergmedia/vst3sdk` at tag `v3.8.0_build_66`, on every bundle. Any failure fails the build. It's Steinberg's own conformance test, the closest thing to "will Cubase load it".
- **`fancy-hosttest`**, in `tools/hosttest`, which loads each bundle through JUCE's VST3 hosting and runs the scenarios below. Its output is saved as `measurements/hosttest.txt` in the measurements artifact.
- **`fancy-tests`**, in-process: offline renders against real time (`tests/HostRenderTests.cpp`), and keyboard, buses, sidechain, MIDI, latency notices and the allocation-counting automation flood (`tests/HostTests.cpp`).

### Where each scenario comes from

| Scenario | Why hosts do it | Source |
|---|---|---|
| Offline with random blocks of 0 to 8192 samples | Audacity's Apply, REAPER's render faster than real time, and every host's export, bounce and freeze call the plugin faster than real time, with large or irregular blocks. VST3 allows any block size up to the maximum, and blocks of 0 samples, which some hosts send to flush parameter changes | Brief H.1; VST3 SDK, `IAudioProcessor::process` and `ProcessSetup` (`kOffline`) |
| 44.1 → 48 → 96 → 44.1 kHz without reloading | Changing the project or device sample rate while the plugin stays loaded | VST3 SDK: `setupProcessing` may be called again after `setActive (false)` |
| Deactivate and reactivate, then 60 s of silence | Hosts deactivate plugins on stop, on track freeze and when they suspend silent tracks | VST3 SDK, `IComponent::setActive` |
| State restored before `prepareToPlay` and while playing | Projects load state before activating the plugin; presets and undo restore it while it plays | VST3 SDK: `IComponent::setState`, then `IEditController::setComponentState` |
| Component and controller state set separately | VST3 keeps the processor and the controller apart, and hosts restore each one separately | VST3 SDK, "Persistence" |
| Sidechain inactive → stereo → mono → inactive | Hosts switch the auxiliary bus on and off with their sidechain controls, while the plugin is inactive | Brief H.4; VST3 SDK, `IComponent::activateBus` and `setBusArrangements` |
| Mono main bus | Mono tracks in Live, Cubase and LUNA | Brief H.9 |
| Host bypass with latency | Hosts use the `kIsBypass` parameter and expect the bypassed signal to stay time-aligned | Brief §4.6; VST3 SDK, `kIsBypass` |
| One latency notice per change | Hosts rebuild delay compensation on `restartComponent (kLatencyChanged)` | Brief H.6; VST3 SDK, `IComponentHandler::restartComponent` |
| Every parameter every block for 10 s | Dense automation, and control surfaces | Brief H.7.2 |
| Editor opened and closed 50 times, and resized | Hosts create a new view each time the window opens, and resize it | Brief H.7.2; VST3 SDK, `IPlugView` |

## Checklists

Install first: close the host, run `.\scripts\get-latest.ps1` in the project folder, then rescan plugins in the host. The plugins are in `%USERPROFILE%\VST3-Dev`, and AudioPluginHost is in `%USERPROFILE%\VST3-Dev-Tools`.

Write each result in the tables above, or tell the agent. "Pass", "fail: what happened", or "n/a".

### REAPER (Tier 1, 10 minutes)

The plugin checklists in `docs/<plugin>-checklist.md` cover REAPER in depth. Beyond those:

- [ ] Click a knob, then press the space bar: playback starts and stops.
- [ ] File > Render, with "Full-speed offline": the rendered file sounds the same as playback. Try Verb with an IR, and FIR in Subtract mode with a learned profile.
- [ ] Set Windows display scaling to 150 % and reopen a plugin: the text is sharp, and the menus open at the same size as the window.

### Audacity (Tier 1, 10 minutes)

Audacity applies effects offline, which is the H.1 case.

- [ ] Effect > Fancy Comp on a selection, Apply: the result sounds like the preview.
- [ ] The same with Fancy Verb (load an IR first) and Fancy FIR in Subtract mode: no gaps, no clicks, and the reverb tail is complete.
- [ ] The preset menu, a dropdown and a typed value work in Audacity's effect window.
- [ ] Comp's Detector input set to a sidechain shows "No sidechain signal" after 2 s, as Audacity has no sidechain routing, and the plugin still processes its input.

### Tracktion Waveform Free (Tier 1, 15 minutes)

- [ ] Load each plugin on a track: the windows open, and the pop-ups look right.
- [ ] Space bar with a plugin window focused: the transport starts.
- [ ] Sidechain: on a bass track with Fancy Comp, use the input button at the top left of the plugin window to feed it from a kick track, and set Detector input to Sidechain L+R. The bass ducks on each kick.
- [ ] Gate with MIDI on open/close: route its MIDI to an instrument track if Waveform allows MIDI out of an audio effect, and note whether notes arrive.
- [ ] Export the project: it sounds the same as playback.
- [ ] Turn on Gate's Lookahead while stopped, then play: the gated track stays in time with the others.

### UA LUNA (Tier 1, 15 minutes)

Free with UA's "UAD Explore FREE" bundle. UA documents key pass-through issues and graphical issues above 100 % scaling with third-party VST3 plugins, which is what to look for.

- [ ] Load each plugin: the windows open, and the pop-ups look right.
- [ ] Click a knob, then press the space bar: LUNA's transport starts.
- [ ] With Windows scaling at 150 %: the plugin window isn't blurry or cropped, and the menus line up with their dropdowns.
- [ ] Sidechain Fancy Comp from another track, using LUNA's sidechain routing, and set Detector input to Sidechain L+R.
- [ ] Bounce a track with Fancy Verb: the tail is complete.

### JUCE AudioPluginHost (Tier 1, 10 minutes)

It draws routing as wires, so it's the free way to check sidechain and MIDI out.

- [ ] Open `%USERPROFILE%\VST3-Dev-Tools\AudioPluginHost.exe`, set up the audio device, and add Fancy Gate.
- [ ] Wire an audio file player or input to Gate's main inputs, and a second source to its sidechain pins; set Detector input to Sidechain L+R: the sidechain opens the gate.
- [ ] Tick MIDI on open/close and wire Gate's MIDI output to a synth: each hit plays a note.
- [ ] Do the same sidechain check with Comp, MultiComp and FIR.

### Ableton Live 12 Suite (Tier 2, trial, about 45 minutes)

- [ ] Load all nine plugins: windows, pop-ups, typed values.
- [ ] With Live's "Auto-Scale Plug-In Window" option on and off, at 150 % Windows scaling: the window and menus scale together, and nothing is cropped.
- [ ] Space bar with a plugin window focused starts the transport.
- [ ] Sidechain Fancy Comp, MultiComp, FIR and Gate from the Sidechain section of the device (Live 10.1 and later).
- [ ] Gate's MIDI out: Live doesn't route MIDI out of audio effects, so the gate should simply keep gating. Note what happens.
- [ ] Automate a knob, then play back: the knob follows.
- [ ] Turn Gate's Lookahead, EQ's linear phase or FIR's Low latency on and off while playing, then stop and start: the track stays in time.
- [ ] Freeze a track with Verb, and export the set: both match playback.
- [ ] Save, close and reopen the set: every plugin comes back as it was, presets and changed marks included.

### Cubase (Tier 2, trial, about 45 minutes)

- [ ] Load all nine plugins: windows, pop-ups, typed values.
- [ ] Sidechain: switch on the plugin's sidechain button at the top of its window, route a kick to it, and set Detector input to Sidechain L+R, for Comp, MultiComp, FIR and Gate.
- [ ] Gate's MIDI out: note whether Cubase offers it as a MIDI source.
- [ ] Space bar with a plugin window focused starts the transport.
- [ ] Automation, with read and write: the knob follows, and A/B or Compare (once W is built) don't write automation.
- [ ] Export audio mixdown, real time and faster: both match playback.
- [ ] Turn latency-changing settings on and off while stopped, then play: the track stays in time.
- [ ] Save, close and reopen the project: every plugin comes back as it was.
