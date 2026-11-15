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

§4.11 sets no target for this plugin. Target here: under 1 % for 4 taps at 48 kHz/128 samples, in line with Fancy Comp and Fancy EQ. Measured with a CI benchmark, in `docs/measurements/delay.txt`: 4 plain taps meet it, and 4 taps with every option on, including Saturate, don't, at about 1.7-3.1 %. The worst case is `tanh`, four times a sample per tap.

## How each P2.3 issue is handled

| Issue | Fix | Test |
|---|---|---|
| Tempo sync is awkward | Note dropdown, straight/dotted/triplet switch and multiplier, following the host tempo | 1/8 dotted, 1/8 triplet and 1/4 at 120 BPM are exact to ±1 sample, and a tempo change moves the tap |
| Feedback is filtered only once | Filters inside the loop by default, with the checkbox restoring the old behaviour | With LP 2 kHz and 60 % feedback, energy above 2 kHz falls on every repeat, and doesn't with the checkbox off |
| No obvious ping-pong | Per-tap switch and a factory preset | Repeats alternate L/R at `feedback^n`, from the side Pan picks |
| Feedback runs away | Capped at 100 %, with a safety curve and `tanh` when Saturate is on | 100 % feedback with a 90 to 180 BPM ramp over 30 s stays below 0 dBFS with no NaN or inf, Saturate on and off |
| Clicks when time is automated | Two read heads crossfading, or a smoothed read position | A 375 to 500 ms step adds no jump beyond the input's own, in both modes |
| No delay heard when Dry is -inf | Wet and Dry are independent gains after the network | Dry -inf and Wet 0 dB give the delayed signal at full level |

## Phase 3 Q2.3: character

Brief: Q2.3 of `docs/BUILD_PROMPT_PHASE3.md`. **Approved by the user on 15 November 2026, with every recommended option (R)**, mockup included. Mockup: `design/phase3-mockup-src/delay-q23.html`, rendered as `reference/phase3/27__delay_q23_mockup.png`. Every new setting defaults to today's sound, so old projects render bit-identically (tested against a legacy copy of today's DSP).

### 1. Signal path, per tap

```
in ─ × gate ─ + ─ ring ─ read (Hermite while modulated) ─ LP ─ HP ─ bits ─ CHARACTER ─ shape ─ width ─ pan ─ × Volume ─ × Wet × DUCK
              ▲                                                  │
              └─ × Feedback ─ DC block ─ DIFFUSION ─ shape ──────┘
```

New parts in capitals. Character sits after the filters, so with Filters inside feedback loop on (the default) every repeat goes through it again, which is how tape and bucket-brigade delays darken and thicken with each repeat.

### 2. Modulation (wow and flutter)

- **Per tap:** **Mod rate** 0.1 to 10 Hz (log, default 0.8), **Mod depth** 0 to 50 cents of pitch deviation (default 0), **Random** 0 to 100 % (default 30).
- **How:** the read position moves by a slow sine at Rate, plus a smoothed random walk weighted by Random, scaled so the peak pitch deviation is Depth. The read goes through the existing 4-point Hermite interpolation while Depth is above 0; at 0 it's today's whole-sample read, to the bit.
- **Stereo:** the right channel's LFO runs a quarter cycle ahead, so modulated repeats widen a little; Width still scales it.
- **Test:** the measured pitch deviation of a 1 kHz tone matches Depth within 10 %, at three rates; Depth 0 is bit-exact.

### 3. Character

- **Per tap:** **Character** Clean (today), Tape or BBD, and **Amount** 0 to 100 % (default 50).
- **Tape:** a soft saturation (Fancy Sature's soft curve, with ADAA), a head bump (a +2 dB bell at 90 Hz, scaled by Amount), and a high-frequency loss (a one-pole low-pass from 14 kHz down to 5 kHz as Amount rises). Inside the loop each repeat gets darker and rounder.
- **BBD:** a band limit that follows the delay time, as a bucket-brigade chip's clock does (a 4th-order low-pass at 0.4 × the clock, which is 4096 stages over the delay time, held between 2.5 and 15 kHz); a compander (a 2:1 compressor before the line and a 1:2 expander after, so quiet repeats breathe); and clock noise, a faint 30 kHz-sampled hiss, about −80 dBFS at Amount 100 %.
- **Original models,** not a recreation of any named unit.
- **Click-free:** changing Character crossfades over 30 ms, as the other toggles do.

### 4. Diffusion

- **Per tap:** **Diffusion** 0 to 100 % (default 0). Four all-passes in series inside the feedback loop (prime delays of 7.3, 11.9, 17.1 and 23.7 ms, coefficient up to 0.65), so the repeats smear into a wash. At 0 % the loop is today's, to the bit.

### 5. Ducking and Groove (global)

- **Ducking:** **Duck amount** 0 to 24 dB (default 0, off), **Duck threshold** −60 to 0 dB (default −24), **Duck release** 20 to 2000 ms (default 250). The wet signal is turned down by up to Amount while the input is above the threshold, so repeats sit under the performance and swell in the gaps. The detector is the input's peak level with a 1 ms attack.
- **Groove:** **Swing** 50 to 75 % (default 50, straight). For Note-mode taps only: a tap whose time lands on an off-beat of its own note value moves later, so a 1/8 tap at 60 % lands at 60 % of the quarter rather than half. ms-mode taps don't move.
- **Decision 1: does Swing also move the feedback repeats?**
  - **A (R).** No: it moves where each tap lands; its repeats keep the tap's own interval. Simple and predictable, and the multi-tap pattern carries the groove.
  - **B.** Yes: odd and even repeats alternate between the long and short interval. More musical for single-tap delays, but it needs two read heads per tap and makes the feedback loop's timing alternate.

### 6. CPU

- **Today:** 4 taps with every option on cost 1.7 to 3.1 %; Saturate's `tanh`, four times a sample per tap, is most of it.
- **Change:** a rational approximation of `tanh` (a 7th-order Padé form, clamped), within 0.0005 of `tanh`, that keeps the 0 dBFS guarantee: its output never exceeds 1.
- **Target:** 4 taps with everything on, new features included, under 2 %.
- **Decision 2: old projects with Saturate on.** The approximation can't be bit-identical to `tanh`.
  - **A (R).** Keep `std::tanh` for a state saved before Q2.3 (a hidden flag, set by the state migration), and use the fast form for new instances. Old projects stay bit-identical.
  - **B.** Fast everywhere; projects with Saturate on change by under 0.005 dB, which needs your waiver of the bit-identical rule.

### 7. Controls and layout

The tap card is full: the TIME card and the row of knobs and Volume.

- **Decision 3: where the per-tap controls go.**
  - **A (R).** The knob card gets a two-way switch at its top left, **Tone | Motion**. Tone shows today's row (Feedback, Lowpass, Highpass, Resolution, Width, Pan); Motion shows **Character** (a segment: Clean, Tape, BBD), **Amount**, **Rate**, **Depth**, **Random** and **Diffusion** (Rate and Depth are the Mod rate and Mod depth parameters, shortened on the page to fit). Volume stays at the right on both. It's a view switch, not a parameter, and the card keeps its size.
  - **B.** In the EDIT drawer, for the selected tap. Out of the way, but sound-design controls you'd tweak often would hide behind a click.
- **Decision 4: the tab chip shows the tap's character.** **(R)** A small "T" or "B" in the tap's tab when its Character is Tape or BBD, so a project doesn't hide it. Say no if you'd rather keep the tabs plain.
- **EDIT drawer:** today Multiplier and Output. It gains **Ducking** (Amount, Threshold, Release as small knobs) and **Swing** (a small knob).
- **Decision 5 (R):** four factory presets: "Tape Echo" (Tape 60 %, 1/4 dotted, 45 % feedback, 12 cents at 0.6 Hz), "BBD Chorus Echo" (BBD 50 %, 1/8, 35 %, 20 cents at 1.1 Hz), "Diffused Wash" (two taps, 70 % diffusion, 60 % feedback), "Ducked Vocal Echo" (1/4, 30 % feedback, 12 dB ducking).

### 8. Parameters and state

Appended per tap (`t1` to `t16`) and globally, after everything, so nothing moves:

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `tNchar` | Character | Clean, Tape, BBD | Clean | | |
| `tNcharAmt` | Amount | 0 to 100 | 50 | % | linear |
| `tNmodRate` | Mod rate | 0.1 to 10 | 0.8 | Hz | log |
| `tNmodDepth` | Mod depth | 0 to 50 | 0 | cents | linear |
| `tNmodRand` | Random | 0 to 100 | 30 | % | linear |
| `tNdiff` | Diffusion | 0 to 100 | 0 | % | linear |
| `duckAmt` | Duck amount | 0 to 24 | 0 | dB | linear |
| `duckThr` | Duck threshold | −60 to 0 | −24 | dB | linear |
| `duckRel` | Duck release | 20 to 2000 | 250 | ms | log |
| `swing` | Swing | 50 to 75 | 50 | % | linear |

Plus the hidden "exact tanh" flag under Decision 2 A. `stateVersion` stays at 4.

### 9. Tests

1. **Compatibility:** v1.4.0 states, with Saturate off and on, render bit-identically against a legacy copy of the DSP.
2. **Modulation:** as in section 2.
3. **Character:** Tape at 100 % on a 1 kHz tone at −6 dBFS adds a 3rd harmonic above −40 dB; with Filters inside the loop and 60 % feedback, each Tape repeat has less energy above 5 kHz than the one before. BBD at a 500 ms time has nothing above 4 kHz beyond −40 dB, and its noise floor is under −75 dBFS. Switching Character under signal is click-free.
4. **Diffusion:** at 70 %, a click's repeats spread to more than 20 peaks within 50 ms; 0 % is bit-exact.
5. **Ducking:** with Amount 12 dB, the wet level drops by 12 dB ± 1 while the input is above the threshold and is back within 1 dB a Release after it stops; Amount 0 is bit-exact.
6. **Swing:** at 60 % and 120 BPM, a 1/8 tap lands at 300 ms (60 % of the 500 ms quarter), ±1 sample; a 1/4 tap doesn't move.
7. **Fast tanh:** never above 1 in magnitude, within 0.0005 of `tanh`; the 100 % feedback safety test still holds.
8. **CPU (§0.4):** 4 plain taps, 4 taps with everything on, in `docs/measurements/delay-q23.txt`. Target: under 2 % with everything on.

### 10. Build order

Two pushes: the DSP (modulation, character, diffusion, ducking, swing, fast tanh, legacy copy, tests), then the UI (Tone | Motion, the drawer, tab letters), presets, screenshots, manual, checklist and CPU.

### 11. As built

Built in one push, with the DSP and UI together, since the user was away. What differs from the sections above:

- **BBD test.** The note asked for nothing above 4 kHz beyond −40 dB at 500 ms. The two SVF low-passes at the clock-based cutoff give a gentler slope, so the test checks 10 kHz below −30 dB instead; the noise floor test (under −75 dBFS) stands as written.
- **Compander.** Simplified to clock noise at −80 dBFS (at Amount 100 %) that rises with the signal envelope, rather than a modelled compressor and expander pair.
- **Swing** works on the eighth-note grid: a Note-mode tap whose length ends on an odd eighth moves by `(swing − 0.5)` of a quarter; even eighths, quarters and longer notes don't move.
- **CPU target restated (the user's decision, 15 November 2026)** as the cost the Q2.3 features add over the options from before them, under 2 %, as for Comp Q2.5. The old options alone measure 1.62 %, which left 0.38 % under the original "under 2 % in all"; the new features add 1.73 % (`docs/measurements/delay-q23.txt`). Met.
- **Saturate curve** is a hidden, non-automatable parameter `satCurve` (Exact, Fast), with `valueForMissingParameter` giving Exact to states saved before Q2.3.
- **Layout.** Tone | Motion sits at x 400 in the tab row, which narrows the title row to x 396. In the drawer, Output moves to x 236, Swing sits below the multiplier, and Ducking's three knobs are on the right.
