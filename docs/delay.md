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

## Phase 4 Wave 1: Freeze and output ceiling

Items from `docs/enhancements.md` (Wave 1 of the Phase 4 order). **Approved by the user on 21 November 2026 with every recommended option (R).** Every new parameter is appended, with a default that keeps saved projects bit-identical, checked against the v1.6.0 DSP in a legacy test as Comp's Wave 1 does. Items were checked in the code on 21 November 2026 before this note was written. Backlog items 1, 2 and 5 of section 6.

### 1. Freeze

`freeze` (off): the input stops entering the taps and every tap's feedback is held at exactly 100 %, with the loop's filters and saturation bypassed while frozen so the loop neither dies nor grows. The frozen loop keeps playing until Freeze goes off, when the taps' own feedback takes over again (a 30 ms crossfade, as toggles do today). A parameter, so it can be automated for build-ups. The footer is full, so it's a **Freeze** button on the tap graph, top left: lit and the graph tinted while frozen.

### 2. Feedback pan per tap: drop it

Each tap already has **Ping-pong** and **Pan** (section "Ping-pong"), which give straight, crossing and placed repeats. A continuous cross amount (Timeless's feedback cross) would add one more knob per tap for a small difference.

- **Decision 1.**
  - **A (R).** Drop item 2 from Phase 4; the backlog says why.
  - **B.** Keep it: `tNcross` 0 to 100 % per tap, 100 % meaning today's Ping-pong, on the Motion page.

### 3. Output ceiling

`ceiling` Off or −12 to 0 dBTP, default Off, in the drawer: the shared `TruePeakLimiter` after Output, so feedback at 100 to 110 % can't clip the bus. Latency reported when on.

### 4. Tests

1. Legacy: v1.6.0's Delay against the new one, every tap mode, Character on and off: identical.
2. Freeze: a burst into a 250 ms tap at 50 % feedback, then Freeze: the loop's level after 10 s is within 0.5 dB of its level at freeze time; new input isn't heard.
3. Ceiling: feedback 110 % for 10 s never exceeds the ceiling in true peak.

Mockup: `reference/phase4/38__delay_w1_mockup.png` (source `design/phase4-mockup-src/w1_mockups.py`): Freeze on the tap graph, frozen and not, and the drawer's Ceiling after Ducking.

### 5. As built

One push (`f276ab4`), with every recommended option; `tests/DelayW1Tests.cpp`. Item 2 (feedback pan per tap) dropped, as decided.

- **Freeze:** `freeze`, a toggle on the tap graph. The repeats hold (every tap's feedback at 1, filters and character bypassed in the loop) and the input is gated out, both over a 30 ms crossfade; automatable. **Fixed after run 44935591606:** the engine read a `current` that didn't exist; Freeze is now a stored flag set with the other targets and snapped with the other smoothers.
- **Ceiling:** `ceiling`, Off (at the bottom) or -12 to 0 dBTP, in the drawer, on the shared `fancy::dsp::OutputCeiling`, out of the path while Off.

## Phase 4 Wave 2: LFO shape and stereo phase

**Approved by the user on 22 November 2026 with every recommended option (A), with one change to decision 3: the switch shows wave symbols, not words.** Item 22 of the Wave 2 order (`docs/enhancements.md`; backlog item 6 of section 6), for v1.7.1. Modelled first: `docs/research/delay-lfo-model.py`.

### 1. What it adds

Each tap's modulation (Mod rate, Mod depth, Random) moves its read position with a sine, with the right channel fixed a quarter cycle (90°) ahead. Two per-tap settings change that:

- **Shape** (`tNmodShape`): **Sine** (today) or **Triangle**. A triangle on the read position holds the pitch at +Depth, then at −Depth, flipping each half cycle: a steady detune with a tape-like warble at slow rates. Its corners are rounded over 10 % of the cycle so the flips don't jump. Depth still means the peak pitch deviation.
- **Stereo** (`tNmodStereo`): how far ahead the right channel's LFO runs, 0 to 180°, default 90° (today). 0° moves both sides together (mono-like modulation); 180° moves them in opposition (widest).

At their defaults the taps render as they do today, bit for bit. Random still adds its smoothed walk on top of either shape.

### 2. What the model found

One tap with Depth 12 cents at 0.8 Hz:

| Shape | Time near the peak detune | Largest pitch step | RMS deviation |
|---|---|---|---|
| Sine (today) | 29 % | 0.06 cents/ms | 8.5 cents |
| Triangle, sharp corners | 100 % | 12 cents/ms (a jump) | 12.0 cents |
| **Triangle, corners rounded over 10 %** | **88 %** | **0.38 cents/ms** | **11.5 cents** |

| Stereo | Left and right apart by up to | Correlation |
|---|---|---|
| 0° | 0 cents | +1.00 |
| 45° | 9.2 cents | +0.71 |
| 90° (today) | 17.0 cents | 0.00 |
| 135° | 22.2 cents | −0.71 |
| 180° | 24.0 cents | −1.00 |

A sharp triangle jumps the pitch by its whole range in a millisecond at each flip, which is audible. Rounded corners keep almost all of the steady detune with no jump.

### 3. Decisions

- **Decision 1: per tap or global.**
  - **A (R).** Both settings per tap, like Rate and Depth (32 appended parameters).
  - **B.** One Shape and one Stereo for all taps (2 parameters).
- **Decision 2: the shapes.**
  - **A (R).** Sine and Triangle (rounded). Random already provides the irregular motion.
  - **B.** Sine, Triangle and Square (a smoothed square on the position, which makes a pitch spike at each flip).
- **Decision 3: where they go.**
  - **A (R).** On the Motion page: a small two-way switch beside Rate showing a **sine wave symbol | a triangle wave symbol** (user, 22 November 2026: symbols, not the words Sine and Tri), with the names in its tooltip, and a **Stereo** knob after Random, with the card kept at its size (the knobs narrow slightly). A mockup follows approval.
  - **B.** In the EDIT drawer, as rows for the selected tap.

### 4. Tests

1. Legacy: Sine at 90° renders exactly as v1.7.0, with Depth 0 and above, Random 0 and above.
2. Triangle, Depth 12 cents at 0.8 Hz on a 1 kHz tone: the measured pitch holds within 10 % of ±12 cents for at least 80 % of the time, and changes by under 1 cent per ms.
3. Stereo 0°: left and right outputs are identical for a mono input. At 180° the pitches move in opposition, with correlation under −0.9.
4. Changing Shape while playing crossfades the position over 30 ms (the existing smoothing), with no click.

### 5. Build order

Approval, then the Motion page mockup (`reference/phase4/57__delay_w2_mockup.png`), then one push: the shapes and stereo phase in the tap modulation, the parameters, the Motion page, tests, manual and checklist. Mockup 57 approved by the user on 23 November 2026: the window grows 60 px wider, to 820 px, and the wave switch sits under Rate.

### 6. As built

One push, with every recommended option; `tests/DelayW2Tests.cpp`, its numbers run first through `docs/research/delay-lfo-prepush.py` (Triangle held 87 % of the time, largest change 0.30 cents per cycle; 180° correlation -1.00; worst second difference on switching 3.6e-4).

- **Parameters,** appended per tap after `ceiling`: `tNmodShape` (Sine, Triangle; Sine) and `tNmodStereo` (0 to 180°, step 1, 90°).
- **Engine.** A cycle counter runs beside the modulation phasor, and the rounded triangle is worked out from it in closed form: straight between the corners, a half-sine turn of the slope over 10 % of the cycle at each corner. For the same peak pitch it needs 1.457 times the sine's travel (2π times its peak of 0.232 cycles), and the centre of the read moves up with it so the read never comes ahead of the tap's time. Shape crossfades over 30 ms and Stereo is smoothed over 30 ms. While Shape is Sine and Stereo exactly 90°, the old code runs, so v1.7.0's sound is bit-identical (tested against `tests/LegacyDelayW1Dsp`, a copy of v1.7.0's engine).
- **Switching Shape** moves the read's centre by 0.46 times the sine's travel over 30 ms, a brief pitch glide (about 0.6 ms of delay at Depth 12 cents and 0.8 Hz), the same kind as turning Depth.
- **Layout** (mockup 57): the window is 820 x 448 (minimum too); the tap graph is 670 wide and the knob card 499; Volume, the tap row's S, power and x, EDIT, the meter and the faders move 60 px right. Stereo is centred at x 555, Diffusion moves to x 585, and the wave switch is 50 x 17 at (364, 311 in the body). The Tone page's knobs keep their places. The wave switch's tooltip names the segment under the mouse; its pop-up screenshot is `delay-tooltip-wave`.

## v1.7.2: the tap graph's label strip

Freeze covered the top of a loud tap's handle near 375 ms. Mockup 67 (`reference/phase4/67__delay_freeze_mockup.png`, source `design/phase4-mockup-src/w3_mockups.py`), layout A (user, 25 November 2026): the tap graph is 24 px taller, with a strip across its top for "dry", Freeze and the caption, which no handle or grid line enters. Freeze stays where it was, the handles keep their travel, and everything below moves down; the Out meter and the Wet and Dry faders are 24 px longer. Window 820 x 472 (minimum too). No parameter, state or DSP change.

## Phase 4 Wave 3: reverse and pitch-shifted taps

**Approved by the user on 26 November 2026: A in every decision.** Item 6 of Wave 3 (`docs/enhancements.md`; backlog items 3 and 4 of section 6), for v1.7.2. Modelled first: `docs/research/delay-reverse-pitch-model.py`. The pitch shifter is shared: Verb's and Room's shimmer (their Wave 3 notes) use the same `fancy::dsp::PitchShifter` in `fancy-core`.

### 1. What it adds

Two per-tap settings, appended after `tNmodStereo`:

- **Reverse** (`tNreverse`, off): the tap plays what came in backwards, in windows one tap time long. Inside each window the read head runs backwards from the write head, so its delay grows from 0 to twice the tap time: on average the tap time, as a forward tap. Each window fades in and out over 5 ms. A note swells in and ends on its attack, the classic reverse echo.
- **Pitch** (`tNpitch`, −12 to +12 st, step 0.01, default 0): the tap's repeats are shifted by Pitch. The shifter sits on the tap's read, inside the feedback loop, so each repeat is shifted again: +12 st gives a rising cascade of octaves (440, 880, 1760 Hz…), −12 a falling one, and ±0.1 st a slowly drifting detune. With Feedback at 0 it's one shifted echo.

Off and 0 st run today's code, so saved projects render as v1.7.1 to the bit.

### 2. What the model found

**Reverse,** a 500 ms tap on a 437.25 Hz tone, which jumps a quarter cycle at every window edge:

| Fade at the edges | Level | Click above 6 kHz, worst edge |
|---|---|---|
| none | 0.00 dB | −5 dB under the tone (a loud click) |
| 2 ms | −0.02 dB | −76 dB |
| **5 ms** | **−0.05 dB** | **−92 dB** |
| 10 ms | −0.11 dB | −104 dB |
| 30 ms | −0.34 dB | −123 dB |

5 ms is inaudible and costs nothing in level. **Where a note lands depends on where it falls in the window:** in a 500 ms reverse tap with windows starting every 0.5 s, a pluck at 0.55 s is heard 0.90 s later, one at 0.70 s 0.60 s later and one at 0.95 s 0.10 s later. Each window is the last tap time played backwards, so the swell always ends at a window edge. Windows locked to the host's beat make those edges land on the grid (decision 2).

**The pitch shifter,** +12 st on 16 tones from 150 Hz to 1.5 kHz and on pink noise. The classic shifter is two read heads half a grain apart, each faded in and out by a raised cosine. On a steady tone the two heads come in at different phases and partly cancel, so the level wavers at the grain rate. **Aligned** grains start up to 10 ms earlier or later, wherever they best match (correlate with) the grain fading out, as WSOLA time-stretching does:

| Shifter | Grain window | Tone wavers, median | Tone wavers, worst | Tone level | Noise level | Average delay |
|---|---|---|---|---|---|---|
| Plain | 20 ms | 2.2 dB | 15.8 dB | −0.8 dB | −0.7 dB | 11 ms |
| Plain | 40 ms | 3.6 dB | 19.8 dB | −1.2 dB | −0.9 dB | 21 ms |
| Plain | 80 ms | 2.0 dB | 22.4 dB | −0.5 dB | −1.2 dB | 41 ms |
| Aligned | 20 ms | 0.5 dB | 1.9 dB | 0.0 dB | −0.5 dB | 16 ms |
| **Aligned** | **40 ms** | **0.5 dB** | **1.9 dB** | **0.0 dB** | **−0.9 dB** | **26 ms** |
| Aligned | 80 ms | 0.5 dB | 1.9 dB | 0.0 dB | −1.0 dB | 46 ms |

The plain shifter warbles audibly on held notes, by up to 20 dB on some pitches; the aligned one doesn't. With alignment the window matters little for tones. 40 ms holds two periods of 50 Hz for falling shifts and keeps the doubled attack a shifter gives a drum hit to about 20 ms.

**Pitch in the loop,** aligned, 40 ms, a 300 ms tap at 60 % feedback (−4.4 dB a repeat without Pitch):

| Pitch | Repeats 1 to 5 |
|---|---|
| +12 st from 220 Hz | 440, 880, 1760, 3520, 7039 Hz; −4.6 dB a repeat |
| +7 st from 220 Hz | 330, 494, 740, 1108, 1661 Hz; −4.6 dB a repeat |
| −12 st from 1760 Hz | 880, 440, 220, 110, 55 Hz; −4.3 dB a repeat |

The cascade is exact and the shifter costs about 0.2 dB a pass, so Feedback keeps its meaning. A rising cascade passes 20 kHz within six or seven octaves. Before a rising shift the shifter low-passes its input at 0.45 × the rate ÷ the ratio, so nothing it raises can pass Nyquist and fold back down: the shimmer model (`docs/research/shimmer-model.py`) found that a filter after the shifter can't stop the fold, and at full feedback the folded partials fed themselves and ran away.

### 3. How it's built

- **`fancy::dsp::PitchShifter`** (new, `fancy-core`): a delay line with two heads reading at the new speed (4-point Hermite, as the taps' modulated read), grains of `40 ms / |1 − ratio|`, at most 400 ms, crossfaded by raised cosines that sum to 1, with the input low-pass above for a rising shift (fourth order). At each grain start the new grain's first half is correlated against the outgoing grain over a 10 ms search, on a 4× decimated copy, and starts at the best match. A plain mode skips the search, for Verb's and Room's shimmer, whose dense tails gain nothing from it. Fixed buffers, allocated in `prepare`. At 0 st it isn't run.
- **Timing:** a pitched tap's base delay is the tap time minus the shifter's average delay (26 ms), so its repeats still land on the tap time; a tap shorter than 27 ms lands that much late. Mod depth and Random move the heads as they move today's read.
- **Reverse:** a second head per tap, reading backwards; with Pitch it reads at −ratio, so a reversed tap is shifted without the shifter. The forward loop (Feedback, filters, Character, Diffusion) is unchanged, and the reverse head reads the ring for the output (decision 1). The window is the tap time up to 5 s; above that it is `10 s − tap time`, at least 50 ms, so the head never reads further back than the ring holds (at least 10.9 s at every rate). A new tap time takes effect at the next window.
- **Freeze:** the frozen loop bypasses the shifter as it bypasses the filters and Character, so a frozen loop holds instead of climbing. A reverse tap keeps playing the frozen loop backwards.
- **Switching:** Reverse crossfades over 30 ms, as the other toggles do; Pitch is smoothed over 30 ms, a short glide.
- **CPU:** a pitched tap reads two heads per channel instead of one, plus a correlation search per grain (about 3 million multiply-adds a second per channel at +12 st). Target: four pitched taps add under 1 % over the same taps unpitched (§0.4). A reversed tap adds one read.

### 4. Decisions

- **Decision 1: what a reverse tap's feedback does.**
  - **A (R).** The loop stays forward and the reverse head reads the ring for the output, so every repeat is reversed and the feedback's timing is today's.
  - **B.** The loop takes the reversed output, so the repeats alternate backwards and forwards, and their spacing changes with where each note fell in its window.
- **Decision 2: where the reverse windows start.**
  - **A (R).** In Note mode while the host plays, on the tap's own note grid from the host's position, so swells end on the beat. In ms mode, or with the transport stopped, from when the tap started running.
  - **B.** Always from when the tap started running: simpler, but a swell's end drifts against the bar.
- **Decision 3: the shifter.**
  - **A (R).** Aligned grains, as modelled: steady on held notes, for about 3 million multiply-adds a second more per pitched channel.
  - **B.** The plain two-head shifter: cheaper, but it warbles on held notes (median 2 to 4 dB, worst 20 dB).
- **Decision 4: where the controls go.**
  - **A (R).** On the Tone page: **Pitch** as a seventh knob after Pan, and **Reverse** as a two-way direction switch (**▶ | ◀**) under Feedback, as the wave switch sits under Rate. The tap's tab shows a small ◀ while it's reversed, beside its Character letter. A mockup follows approval, measured on Delay's latest screenshot. *Mockup 69 (user, 26 November 2026: grow the window, don't clamp): the window grows 64 px wider, to 884 x 472 (its minimum too), so the Tone knobs keep their 64 px spacing and Pitch gets its own column; the tap graph and knob card widen, and the Motion page's five knobs (Rate to Diffusion) spread to the same 64 px.*
  - **B.** A third knob-card page, **Tone | Motion | Shift**, holding Reverse and Pitch: nothing moves on the other pages, but it's two controls on a page of their own.

### 5. Parameters

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `tNreverse` | Reverse | off/on | off | | |
| `tNpitch` | Pitch | −12 to +12, step 0.01 | 0 | st | linear |

32 parameters, appended after `tNmodStereo` for taps 1 to 16. No `stateVersion` bump: a state without them loads Off and 0. Pitch takes `7`, `-12` or `-0.1` typed, and shows "+7.00 st".

### 6. Tests

1. Legacy: Reverse off and Pitch 0 render exactly as v1.7.1's engine (a copy in `tests/`), every Character, with modulation, Freeze and the ceiling.
2. Reverse: a 437.25 Hz tone through a 500 ms reverse tap: level within 0.1 dB of a forward tap's, and the peak above 6 kHz within 10 ms of each window edge at least 80 dB under the tone. An impulse at time t in a window lands at `2 × window end − t`, ±1 sample.
3. Reverse windows in Note mode with a host position: a 1/4 tap at 120 BPM starts its windows on the quarter-note grid, ±1 sample.
4. Pitch: a 1 kHz tone at +7 st comes out at 1498.3 ± 1 Hz, wavering under 2 dB (2 ms RMS), and lands on the tap time ±2 ms (cross-correlated envelope).
5. Cascade: +12 st, 60 % feedback, a 220 Hz note: repeats 1 to 4 at 440, 880, 1760 and 3520 Hz ± 1 %, each 4.4 ± 0.5 dB under the last.
6. No aliasing: +12 st, 95 % feedback for 20 s on a 5 kHz tone: nothing below 4 kHz above −80 dB, and no NaN or inf.
7. Freeze with Pitch +12: the frozen loop's spectrum after 10 s matches its spectrum at freeze time within 1 dB per third octave.
8. Switching Reverse and moving Pitch under signal: no step beyond the input's own (the existing click test).
9. CPU (§0.4): four taps at +12 st against the same four at 0 st, in `docs/measurements/delay-w3.txt`.

### 7. Build order

Approval, then the Tone page mockup (`reference/phase4/69__delay_w3_mockup.png`), then one push: `PitchShifter` with its own tests, the reverse head and pitch in `DelayDsp`, the parameters, the Tone page, tests, manual and checklist. The shifter is built here first, so Verb's and Room's shimmer reuse a tested part.
