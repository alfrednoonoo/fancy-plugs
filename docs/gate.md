# Fancy Gate: design note

Phase 2.0.1, from `docs/BUILD_PROMPT_PHASE2_0_1.md`, with the mockup `reference/phase2_0_1/15__gate_mockup.png` and its source `design/phase2_0_1-mockup-src/gate.html`. Bundle ID `com.fancyplugs.gate`, plugin code `FGat`. Accent yellow `#F1C315`, which is `colours::accentYellow`, the same as FIR's Gate mode. Detector filters are blue, and global controls teal.

Nothing here is coded until the open questions at the end are answered.

## Signal path

```
main ──┬─ lookahead delay (0, or 50 ms) ─┬─ × gate gain ─ × Wet ──┐
       │                                 └─ × Dry ────────────────┤
       │                                                          + ─ × Output ─ bypass ─ out
       │      noise (L, R independent) ─ × open amount ─ × Noise ─┘
       │
main or sidechain ─ detector select ─ HP ─ LP ─ peak or RMS ─ link ─ dB ─ state machine ─ gain curve
                                                                              │
                                                                              └─ MIDI out, history, lights
```

**Preview filter** replaces the whole output with the filtered detector signal, delayed to match.

## Shared code it reuses

The brief says to reuse these from `fancy-core` and not fork them. Two of them are still inside Fancy Comp, so they move first. As with the earlier moves, Fancy Comp's tests must pass unchanged and its screenshots must stay byte-identical.

| Part | Where it is now | What happens |
|---|---|---|
| Detector input, filters, peak and RMS, link | Inside `CompDsp::compress` | Moved to `fancy/dsp/Detector.h`, with Comp using it |
| Preview filter path | Inside `CompDsp` | Comes with the detector: it outputs the filtered signal |
| Threshold-over-meter fader | `plugins/comp/src/CompMeters` | Moved to `fancy-core` as `ThresholdMeter`, with options for the close line, the scale and the read-outs |
| Gain reduction meter | `fancy-core`, `GainReductionMeter` | Used as is, in its single-bar layout, with a state light added below it |
| Out meter, Wet and Dry faders, segments, checkboxes, dropdowns | `fancy-core` | Used as is |
| Sidechain bus handling | Each processor | The same bus layout as Comp: stereo main, optional stereo or mono sidechain |

## Detector

- **Source:** Main input L+R, Main L, Main R, Sidechain L+R, Sidechain L or Sidechain R, as in Comp.
- **Filters:** a 2nd-order state-variable high-pass and low-pass on the detector only, never on the audio. Highpass runs from 20 Hz to 5 kHz, and is off at 20 Hz. Lowpass runs from 20 Hz to 22 kHz, and is off at 22 kHz.
- **Level:** peak when RMS size is 0, otherwise a one-pole RMS, Comp's detector. The result is in dB.
- **Link channels:** on, both channels use the louder level. Off, each channel has its own detector, state machine and gain.

## State machine

**States:** closed, opening, open, hold and closing. There's one machine for each channel, or one shared machine when Link is on.

- **Open threshold** is Threshold. **Close threshold** is Threshold + Hysteresis.
- **Opening:** from closed or closing, when the level reaches Threshold and the machine is armed. A re-trigger during hold or closing goes straight back to opening, from the current gain, with no jump.
- **Re-arming:** a machine is armed once the level has dropped below Threshold since it last opened. Without this, a positive Hysteresis would re-trigger on every sample and never close.
- **Hold:** while open, hold starts when the level falls below the close threshold, and runs for Hold ms. Rising back to the close threshold or above during hold returns to open.
- **Closing:** when hold runs out.

**Positive Hysteresis**, the original's unusual feature, puts the close threshold above the open threshold. Once open, the gate then starts its hold as soon as the level is under that higher point. With re-arming, a steady tone above Threshold gives exactly one attack, hold and release envelope. That makes it a fixed-shape trigger.

**The lights:** "open" is yellow and glowing, for opening and open. "hold" is yellow and dim. "closed" is grey, for closing and closed.

## Gain curve

- **Position:** a value `p` from 0 (closed) to 1 (open). It rises at `1 / attack samples` while opening, and falls at `1 / release samples` while closing.
  - Turning round mid-ramp only changes direction, so a re-trigger can't jump.
  - Attack and Release are therefore the times of a complete open or close.
- **Shape:** raised cosine, in dB. `gainDb = Range × (1 - s)` in Gate mode and `Range × s` in Duck mode, where `s = (1 - cos (π p)) / 2`.
- **Range at -inf:** the dB curve runs down to -120 dB, and a fully closed gate (`p = 0`) outputs exactly 0. So -inf really is silence, and the last part of the fade is inaudible.
- **Attack at 0 ms:** still fades over 32 samples or 0.5 ms, whichever is shorter. 32 samples is 0.67 ms at 48 kHz, so at 48 kHz and below the fade is 32 samples. This is what stops a click on opening, and it's documented in the manual.

## Lookahead

**Lookahead off**, the default:

- There's no delay anywhere, and the reported latency is 0.
- The gain is applied to the same sample the detector reads, so the gate opens at the first sample over Threshold, with the minimum fade above.

**Lookahead on:**

- The audio, the Dry signal and Preview are all delayed by `D`, which is 50 ms at the current sample rate, rounded to whole samples (2400 at 48 kHz). The reported latency is `D`, constant.
- The detector reads the undelayed signal.
- The output at sample `n` is the audio from `n - D` times the gain from detector time `n - D + Pre-open`. The gain is therefore Pre-open ms ahead of the audio, so the gate is already opening when the transient arrives.
- That needs a gain delay line of `D - Pre-open`. Pre-open is smoothed over 50 ms, so moving it shifts the gain's timing gently. Pre-open never changes the reported latency.

**Switching it:**

- Lookahead isn't automatable, as the brief says. Turning it on or off changes the latency, so it's a deliberate choice.
- The switch fades the output out over 5 ms, changes the delay, clears the delay lines, and fades back in once they've refilled. It's the same scheme as Comp's oversampling switch.

## The Pre-open knob while Lookahead is off

- The knob is dimmed to 50 %, reads "off", and can't be changed. Mouse drag, the wheel and typed entry are all ignored.
- **Hovering** for about 300 ms shows the standard tooltip: "Pre-open needs Lookahead. Tick 'Lookahead (+50 ms latency)' above to use it."
- **Clicking or dragging** shows the same message at once, and the Lookahead checkbox pulses with a yellow outline for about 600 ms.
- **It never switches Lookahead on itself**, because that would change the latency.
- With Lookahead on, the knob works normally and has its ordinary tooltip.

This needs a "locked" state on the shared `ParamControl`: ignore changes, report a hint, and call back on a click. It also needs a pulse on `ParamCheckbox`. Both go in `fancy-core`, and both get tests.

## Duck mode

- Duck has the same detector and state machine as Gate, with the gain inverted. Open means turned down by Range, and closed means 0 dB.
- Switching to Duck doesn't touch Wet or Dry. Their defaults, Wet 0 dB and Dry -inf, are already right for ducking.
- Switching mode while audio plays fades the gain curve across, so there's no click.

## Noise

- White noise, independent on left and right, from a fast integer generator. No allocation, and a fixed seed, so a render is repeatable.
- It's multiplied by the open amount `s` (0 when closed) and by the Noise level.
- "Only while the gate is open" uses `s` in both modes. In Duck mode, noise is added while ducking.

## MIDI

- **Note-on** when a machine enters opening from closed or closing. **Note-off** when it reaches closed.
- **The events go on the VST3 event output**, which needs `NEEDS_MIDI_OUTPUT` for this plugin only. `fancy_add_plugin` gets an option for it, and the Gate overrides `producesMidi()`.
- **Timing:** each event goes at the output sample where the gain change is heard. With Lookahead on, that's the detector sample plus `D - Pre-open`. With it off, it's the same sample. An event later than the current block is queued in a small fixed queue for the next one.
- **Why that alignment:** REAPER delays the track's playback by the reported latency, so notes placed where the gain change is heard line up with the audio REAPER plays.
- **Note and channel:** a note from 0 to 127 (default 69, A4) and a channel from 1 to 16. The footer's compact dropdown reads "A4 · ch 1", and opens a menu with a note submenu and a channel submenu.
- **Velocity from level:** off gives a fixed velocity of 100. On maps the detector's peak over the first 5 ms after opening from -60 to 0 dBFS onto 1 to 127. See open question 1.
- **Link off:** MIDI follows the linked state, the louder channel, so one note is sent per hit.
- **Turning MIDI off** while a note is on sends its note-off.

## History display

- The audio thread pushes one pair per display column into a lock-free single-producer, single-consumer queue: the peak input level and the lowest gain over that column.
- A column is 3 s divided by the display's width in pixels, about 6.6 ms, or 316 samples at 48 kHz.
- The editor draws it at 30 fps:
  - the input level as a grey fill;
  - the gate's gain as a yellow line;
  - the open threshold as a solid line and the close threshold as a dashed line.
- **Clicking or dragging** on it sets Threshold at that height, as one gesture, so it can be undone in Phase 2.1.

## Sidechain warning

The warning is set when a Sidechain source is selected and the sidechain's peak stays below -90 dBFS for 2 s while the main input is above -60 dBFS. It clears within 0.5 s of sidechain signal arriving. The editor shows "No sidechain signal: check track routing (REAPER: pins 3/4)" next to the dropdown.

## Parameters

| ID | Name | Range | Default | Unit | Skew | Notes |
|---|---|---|---|---|---|---|
| `threshold` | Threshold | -90-0 | -40 | dB | linear | Open threshold |
| `hysteresis` | Hysteresis | -24-+12 | -6 | dB | linear | Close = Threshold + Hysteresis. Bipolar knob |
| `lookahead` | Lookahead | off/on | off | | | Not automatable |
| `preopen` | Pre-open | 0-50 | 5 | ms | linear | Only with Lookahead |
| `attack` | Attack | 0-500 | 0.5 | ms | skewed | Minimum fade at 0 |
| `hold` | Hold | 0-2000 | 20 | ms | skewed | |
| `release` | Release | 1-5000 | 100 | ms | skewed | |
| `range` | Range | -inf-0 | -inf | dB | linear | -inf at -80. In Duck mode, how far it ducks |
| `rms` | RMS size | 0-250 | 0 | ms | skewed | 0 is peak |
| `detector` | Detector input | 6 choices | Main input L+R | | | |
| `lowpass` | Lowpass | 20-22000, top is off | Off | Hz | log | Detector only |
| `highpass` | Highpass | 20-5000, bottom is off | Off | Hz | log | Detector only |
| `preview` | Preview filter | off/on | off | | | |
| `mode` | Mode | Gate, Duck | Gate | | | |
| `link` | Link channels | off/on | on | | | |
| `wet` | Wet | -inf-+12 | 0 | dB | linear | |
| `dry` | Dry | -inf-+12 | -inf | dB | linear | |
| `noise` | Noise | -inf-0 | -inf | dBFS | linear | Grey fader |
| `midi` | MIDI on open/close | off/on | off | | | |
| `midiNote` | MIDI note | 0-127 | 69 | | | Shown as a note name |
| `midiChannel` | MIDI channel | 1-16 | 1 | | | |
| `velocity` | Velocity from level | off/on | off | | | |
| `output` | Output | -24-+12 | 0 | dB | linear | Header knob, as on every plugin |
| `bypass` | Bypass | off/on | off | | | §4.6 |

The note, channel and Velocity controls are dimmed to 50 % until MIDI is on.

**Typed values:** every knob and fader uses the shared `Knob`, `Fader` and `ThresholdMeter`:

- Hysteresis accepts `+6` or `-6`.
- Range and Noise accept `-inf`.
- Lowpass and Highpass accept `2.5k`.
- Times are in milliseconds.
- The locked Pre-open knob ignores typed values, as the brief requires.

## Layout

The mockup at 1x is 740 × 405 with a 37 px header. The shared header is 53 px, so the window is **740 × 421**, as on the other Phase 2 plugins.

- **Header:** standard large header. The I/O pill counts the sidechain, as on MultiComp, so it reads "4 in 2 out".
- **Row under the header:** "Detector input" and its dropdown, the Lookahead checkbox with "(+50 ms latency)" in secondary text, and on the right "Mode" with a Gate | Duck switch. The switch is yellow with black text when selected.
- **Left column:** Threshold over the stereo input meter, with the close line dashed. Below it "-38.0 dB" in yellow and "close -44.0" in grey. Then the Gain meter, single bar, with its read-out and the state light.
  - **Threshold scale:** the brief's range goes down to -90 dB, but the mockup labels its meter from 0 to -60. The fader covers the whole range, so the scale reads 0, -18, -36, -54, -72 and -90, in six steps like the mockup's. The brief's ranges win over the mockup.
- **Centre:** the history display, about 456 × 70 px. Below it the Envelope card (Pre-open, Attack, Hold, Release, Hysteresis), the Level card (Range, RMS size) and the blue Detector filter card (Lowpass, Highpass), all in the shared tight knob style.
- **Right column,** 150 px: the Out meter, then Wet, Dry and Noise faders. Wet and Dry are teal, Noise grey `#909090`.
- **Footer:** Preview filter, Link channels, MIDI on open/close, the "A4 · ch 1" dropdown and Velocity from level, in 12 px labels as on the other Phase 2 plugins.

The page is `#303030` and the cards `#262626`, as the mockup source has them.

## Latency

0 with Lookahead off. With it on, 50 ms at every sample rate, which is 2205 samples at 44.1 kHz, 2400 at 48 kHz and 19200 at 384 kHz. Pre-open never changes it.

## State format

The standard `FancyProcessor` XML. There's nothing beyond the parameters: the history and lights aren't saved.

## Factory presets

| Name | Settings |
|---|---|
| Vocal clean-up | Threshold -45, Hysteresis -6, Attack 2, Hold 60, Release 200, Range -18, Highpass 100 Hz |
| Kick tight | Threshold -30, Attack 0.5, Hold 30, Release 80, Lowpass 150 Hz |
| Snare (with spill) · lookahead | Lookahead on, Pre-open 2, Threshold -28, Hold 40, Release 120, Highpass 150 Hz, Lowpass 6 kHz |
| Tom (with spill) · lookahead | Lookahead on, Pre-open 2, Threshold -32, Hold 80, Release 300, Lowpass 400 Hz |
| Duck under kick | Duck, Sidechain L+R, Threshold -30, Range -8, Attack 1, Hold 20, Release 150 |
| Drum trigger to MIDI | MIDI on, Velocity from level on, Threshold -24, Hold 20, Release 50 |
| Gentle expander | Threshold -50, Hysteresis -6, Range -12, Attack 5, Release 250 |

## CPU

The brief's target is under 0.5 % with filters and RMS on and Lookahead off, at 48 kHz and 128 samples. It asks for the median of 5 CI runs. To save build minutes, the benchmark repeats the measurement 5 times in each run and records the median. The figure from successive runs is also kept, as for the other plugins. See open question 2.

## How each issue in §5 is handled

| Issue | Fix | Test |
|---|---|---|
| No floor or range control | Range | Range -20, closed: output 20.0 ± 0.1 dB below the input. Open: 0 dB |
| Hysteresis is misunderstood | Drawn on the meter and history, the "close" read-out, and a tooltip | Threshold -30, Hysteresis -6: opens at -30, stays open to -36, closes below -36, all ± 0.1 dB |
| Positive Hysteresis | Re-arming | Hysteresis +6, a steady tone 3 dB over Threshold: exactly one attack, hold and release envelope |
| Duck mode is confusing | An explicit Mode, with Range as its depth | Duck, Range -12, sidechain over Threshold: main at -12.0 ± 0.1 dB. Sidechain silent: 0 dB |
| Sidechain doesn't seem to work | Plain source names, the warning, and a REAPER how-to in the manual | Sidechain silent for 2 s with main signal: the warning is set. It clears within 0.5 s of sidechain signal |
| Clicks with fast attack | The minimum raised-cosine fade | A 1 kHz sine at -6 dBFS opened at the worst phase: no step over 1.5 times the sine's own largest step |
| Lookahead latency | Zero by default, and a constant 50 ms when on | Off: latency 0, and an impulse comes out at sample 0. On: the same latency for Pre-open 0, 10 and 50 ms, with the output aligned to the input in all three |
| MIDI triggering with velocity | Sample-placed MIDI and Velocity from level | Hits at -24, -12 and -3 dBFS: velocities rise in that order and match the mapping within ±2. Note-on within 1 sample of the gate's opening in the output |
| Chattering | Hysteresis -6 and Hold 20 by default | A signal moving ±2 dB around Threshold at 20 Hz: at most one open and close a second |

The Pre-open lock gets its own tests: with Lookahead off, the knob reports the hint as its tooltip and ignores drag, wheel and typed values. With it on, it has its normal tooltip and accepts all three. The usual §4 tests also apply: sample rates, block sizes, bit-identical automation, state, bypass, fuzzing and mono.

## Open questions for the user

1. **Velocity with Lookahead off.** Velocity comes from the peak over the first 5 ms after opening, and the note has to be within 1 sample of the opening. With Lookahead off, there's no latency to wait in, so the 5 ms after opening hasn't happened yet when the note must go out. The options are:
   - **(a)** With Lookahead off, use the peak up to the opening sample. The note is on time, and velocity is less precise. With Lookahead on, it's exact. Recommended.
   - **(b)** With Lookahead off, send the note 5 ms late, so velocity is exact.
   - **(c)** Velocity from level switches Lookahead on. The brief rules this out for the Pre-open knob, so it probably applies here too.
2. **"Median of 5 CI runs" for CPU.** Five runs is about 30 minutes of build time. Taking the median of 5 repeats in one run gives nearly the same answer, with the usual caveat about runner variation. Is that acceptable?
