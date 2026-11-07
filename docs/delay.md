# Fancy Delay: design note

## DSP

Up to 16 taps run in parallel. Each tap has its own stereo ring buffer and its own feedback loop:

```
in ─┬─────────────────────────────────────────────────────────────── × Dry ─┐
    │                                                                       + ─ × Output ─ bypass ─ out
    └─ × gate ─ + ─ ring ─ read head(s) ─ LP ─ HP ─ bits ─ shape ─ width ─ pan ─ × Volume ─ × Wet ─┘
                ▲                               │
                └─ × Feedback ─ DC block ─ shape ┘  (taken before LP when the filters are outside the loop)
```

- **Rings:** 10 s plus a margin, rounded up to a power of two, for every tap. All 16 are allocated in `prepare`, so nothing allocates while audio runs. That's about 4.2 MB a tap at 48 kHz, 67 MB an instance. A tap that's unused or off and has gone silent sleeps, so it costs no CPU.
- **Delay time:** always a whole number of samples, at least 1. A delay of 0 ms is therefore 1 sample. Whole samples mean a held delay is read without interpolation, so repeats don't dull from it.
- **Time changes (P2.3):**
  - Smooth time changes on: a second read head starts at the new time, and the two crossfade linearly over 30 ms. A change that arrives during a fade waits for it to end.
  - Off: the read position glides to the new time over a 150 ms time constant with 4-point Hermite interpolation, so the pitch bends like tape. It lands exactly on the whole-sample target.
- **Tempo:** Note mode reads the host's tempo every block and follows changes through the same two paths. Without a host tempo, it keeps the last one, starting at 120 BPM.
- **Note length:** `60000 / BPM × 4 / denominator × feel × multiplier`, where feel is 1, 1.5 or 2/3. Limited to 10 s.
- **Filters:** 12 dB/oct state-variable filters with Q 0.707, so they stay stable under fast modulation. Cutoffs smooth in octaves. Coefficients recompute per sample while moving and not at all once settled. Lowpass switches out at 22 kHz and Highpass at 20 Hz, through a crossfade.
- **Resolution:** rounds to `2^(bits - 1)` steps per unit. 24 bit switches it out.
- **Filters inside feedback loop:** on, the loop takes the filtered signal, so each repeat is filtered again. Off, the loop takes the signal before the filters, so every repeat is filtered once, as in the original.
- **Feedback safety:** Feedback is limited to 100 % unless Saturate feedback is on, when it can reach 110 %. The value stays on the knob, so a stored 105 % comes back when Saturate does.
  - With Saturate on, `tanh` shapes both the tap's output and its loop, including the first echo.
  - With it off and Feedback above 0 %, a safety curve is linear up to 0.9 and bends towards 1.0 above it. Without this, 100 % feedback on a sustained input would grow without limit. It acts on the signal going into the ring and on the tap's output, so a full-scale peak loses about 0.3 dB while Feedback is above 0 %. Anything below -0.9 dBFS is untouched.
  - With Feedback at 0 % and Saturate off, neither applies, so a plain delay is untouched.
- **DC block:** a 5 Hz one-pole high-pass in the feedback path only.
- **Width:** mid/side, 0.00 to 2.00, after the shaping.
- **Pan:** a balance law, so the centre is unity and a hard pan silences the other side. L gain is `min(1, 1 - pan)`.
- **Ping-pong:** the input is summed to mono and enters one side, and each repeat crosses to the other side. Pan chooses the starting side, where centre starts left, and doesn't otherwise apply. The repeat levels are `feedback^n`.
- **Solo:** while any tap is soloed, the others are muted but keep running, so unsoloing doesn't lose their tails.
- **Power off:** stops the tap's input and output. Its loop keeps running silently, so it drains and turning it back on can't replay old audio.
- **Removing a tap:** resets its parameters and drains it. Re-adding one within the delay time can let the rest of the old tail through.
- **Mono bus:** pan, width and ping-pong have no effect.
- **Wet and Dry (P2.3):** independent gains after the delay network, so Dry at -inf changes nothing about the delayed signal.

Everything continuous is smoothed: gates over 20 ms, the rest over 30 ms. Toggles (filters inside the loop, Saturate, Ping-pong) crossfade over the same time.

## Parameters

Per tap, with IDs prefixed `t1` to `t16`:

| ID | Name | Range | Default | Unit | Skew | Source |
|---|---|---|---|---|---|---|
| `used` | Used | off/on | tap 1 on | | | Internal, as in Fancy EQ |
| `on` | On | off/on | on | | | P2.2 |
| `solo` | Solo | off/on | off | | | P2.2 |
| `mode` | Time mode | ms, Note | Note | | | P2.2 |
| `ms` | Length | 0-10000 | 375 | ms | centred on 500 | P2.2 |
| `note` | Note value | 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64 | 1/8 | | | P2.2 |
| `feel` | Feel | Straight, Dotted, Triplet | Dotted | | | P2.2 |
| `mult` | Multiplier | ×1-×16 | ×1 | | | P2.2 |
| `fb` | Feedback | 0-110 | 0 | % | linear | P2.2 |
| `lp` | Lowpass | 200-22000, top is Off | Off | Hz | log | P2.2 |
| `hp` | Highpass | 20-5000, bottom is Off | Off | Hz | log | P2.2 |
| `bits` | Resolution | 4-24 | 24 | bit | linear | P2.2 |
| `width` | Width | 0.00-2.00 | 1.00 | | linear | P2.2 |
| `vol` | Volume | -inf-+12 | 0.0 | dB | linear | P2.2 |
| `pan` | Pan | 100 L-100 R | C | | linear | P2.2 |
| `ping` | Ping-pong | off/on | off | | | P2.2 |
| `inloop` | Filters inside feedback loop | off/on | on | | | P2.2 |
| `sat` | Saturate feedback | off/on | off | | | P2.2 |

Global:

| ID | Name | Range | Default | Unit | Source |
|---|---|---|---|---|---|
| `wet` | Wet | -inf-+12 | 0.0 | dB | P2.2 |
| `dry` | Dry | -inf-+12 | 0.0 | dB | P2.2 |
| `smooth` | Smooth time changes | off/on | on | | P2.2 |
| `output` | Output | -24-+12 | 0.0 | dB | Header knob, as on every plugin |
| `bypass` | Bypass | off/on | off | | §4.6 |

The brief lists Ping-pong, Filters inside feedback loop and Saturate feedback per tap, and the mockup draws them in the footer. They sit there and follow the selected tap.

## Controls the mockup doesn't show

- **Multiplier:** P2.2 asks for it and the mockup has no control. It's a dropdown in an EDIT drawer, as the brief's rule for extra controls says, opened from an `EDIT` link at the right of the footer.
- **Output:** a small knob in the same drawer, as on Kicker, Exciter and Comp, because the header knob shows no value.

## Typed values

Every knob and fader uses the shared `Knob` and `Fader`, so clicking a value or Ctrl-clicking a control opens the box from v1.0.1.

- Frequency knobs accept `2500` and `2.5k`. Off is the end of the range, so type `22k` on Lowpass or `20` on Highpass.
- Pan accepts `60 L`, `L60`, `60 R` and `-60`.
- In Note mode the Length knob steps through the note values, and accepts `1/8` or just `8`.
- Length in ms mode is in milliseconds.

## Latency

None. The delay is the effect, and the dry path has no delay. `getTailLengthSeconds` reports 10 s.

## State format

The standard `FancyProcessor` XML. It adds one non-audio property, `selectedTap`, the tab shown in the editor. There's no audio in the state. Removed taps keep their parameters at defaults.

## CPU

§4.11 sets no target for this plugin. Target here: under 1 % for 4 taps at 48 kHz/128 samples, in line with Fancy Comp and Fancy EQ. Measured with a CI benchmark, in `docs/measurements/delay.txt`: 4 plain taps meet it, and 4 taps with every option on, including Saturate, don't, at about 1.7-2.1 %. The worst case is `tanh`, four times a sample per tap.

## How each P2.3 issue is handled

| Issue | Fix | Test |
|---|---|---|
| Tempo sync is awkward | Note dropdown, straight/dotted/triplet switch and multiplier, following the host tempo | 1/8 dotted, 1/8 triplet and 1/4 at 120 BPM are exact to ±1 sample, and a tempo change moves the tap |
| Feedback is filtered only once | Filters inside the loop by default, with the checkbox restoring the old behaviour | With LP 2 kHz and 60 % feedback, energy above 2 kHz falls on every repeat, and doesn't with the checkbox off |
| No obvious ping-pong | Per-tap switch and a factory preset | Repeats alternate L/R at `feedback^n`, from the side Pan picks |
| Feedback runs away | Capped at 100 %, with a safety curve and `tanh` when Saturate is on | 100 % feedback with a 90 to 180 BPM ramp over 30 s stays below 0 dBFS with no NaN or inf, Saturate on and off |
| Clicks when time is automated | Two read heads crossfading, or a smoothed read position | A 375 to 500 ms step adds no jump beyond the input's own, in both modes |
| No delay heard when Dry is -inf | Wet and Dry are independent gains after the network | Dry -inf and Wet 0 dB give the delayed signal at full level |
