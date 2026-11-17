# Phase 4: plan and enhancement backlog

Proposed additions for the fourteen plugins, benchmarked against the plugins that define each
category in 2025–26 and **verified against the source** on 17 November 2026 (parameter lists read
from every `plugins/*/src/*Processor.cpp`, then each item checked in the DSP and editor). Reviewed
the same day against the design notes and the standing decisions in `docs/PROGRESS.md`; see
"Review" at the end for what was removed or rescoped and why.

**Phase 4 is this file** (user, 17 November 2026): the three items Phase 3 deferred (Kicker's
sidechain Duck, a transient shaper and a chorus) plus every item below. It starts after the Mac
release (`docs/mac.md`). The scope is agreed; each item still gets its own design note, approved
before any code, with the model, decisions and tests the briefs require, and the parameter list and
the visible layout only change through those notes, per `AGENTS.md`. A new plugin also goes through
the design rules (`docs/DESIGN_SYSTEM.md`) and an HTML mockup first.

## Phase 4 order

Proposed; to be confirmed with the user before the first item, as Phase 3's order was.

1. **Wave 1**, batched by plugin: one design note per plugin covering its S items, so each plugin
   is opened, tested and checked in REAPER once. Order: Comp, EQ, Limit, Gate, DS, Verb, Room,
   Delay, Exciter, Kicker, FIR, DS-Mini.
2. **Tempo-synced trigger** in `fancy-core`, then **Kicker's sidechain Duck** with it (Kicker 7),
   then Comp 9 and Gate 9.
3. **Fancy Transient** (new plugin): design rules, mockup, design note.
4. **Fancy Chorus** (new plugin, chorus or modulation): design rules, mockup, design note.
5. **Wave 2**, one item at a time, in the order under "Suggested order".
6. **Wave 3**, the same way; the ▲ items need layout approval first.

The plugin names in 3 and 4 are placeholders until their design notes.

## How to read the tags

**Effort** — S: one short push, the pattern of an existing feature. M: a phase item, two or three
pushes, a Python model before code. L: new engine or a new shared subsystem.

**Wave** — 1: cheap and append-only (drawer settings or new parameters whose defaults keep saved
projects bit-identical). 2: engine work, one item at a time. 3: category-defining, larger.

**▲** — cannot be hidden in a drawer, so it changes the visible layout and needs your approval
before any code.

Every item is appended after the existing parameters, so no ID or index moves, and no existing
parameter's range changes (that would change what saved values mean). Where a new setting changes
the sound, its default is today's path, bit-identical. State version 3 already gives a parameter
missing from a saved state its default, as Kicker's Sub mode relied on (`docs/kicker-sub.md`), so
`stateVersion` is bumped only if the meaning of a stored value changes.

## Verification notes

Checked and **not** gaps, so nothing below re-proposes them: Comp's "Brickwall" is a factory preset
name, not a sidechain brickwall filter; FIR's "De-mud" and "Tilt" are EQ curve buttons, not a
de-hummer; Delay's `TapMap` is the drag-handle time graph, not pattern storage; EQ already has
All-pass and Tilt types, slopes from 6 to 48 dB/oct, and a per-band channel (Stereo, Left, Right,
Mid, Side); its analyzer already has Mid/Side/Left/Right; Room's Width 0 is a mono maker already,
and Room already has Freeze and Ducking; DS already has Mid/Side **detection** (what it lacks is
per-channel reduction); Verb's Reverb generator already has Build-up; Comp's Preview is a detector
listen, not a delta; Kicker's Sub mode already follows a falling kick's pitch and restarts at phase 0
on each hit; Exciter's oversampling Off already has zero latency; Delay's taps each have a lowpass
and a highpass.

---

## 1. Fancy Kicker (Boost + Sub)

Sub mode stays a divider (decided 11 November 2026). No Harmonics mode: Fancy Sature covers
small-speaker bass.

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Sub length: a decay that ends the sub before the next hit, independent of Sensitivity | S | 1 | Drawer; `subLen` |
| 2 | Make-room: resonant high-pass at Freq cutting the source (Dropkick, Rumble Forge) | S | 1 | Drawer; `roomCut`, `roomFreq` |
| 3 | Sub polarity invert (the detector's delay compensation stays as it is) | S | 1 | Drawer; `subPolarity` |
| 4 | Sub-only output meter | S | 1 | Right column; no parameter |
| 5 | Oversample the soft clipper (reuse `SwitchedOversampler`) | S | 2 | Drawer; `clipOs` |
| 6 | Auto gain (RMS/LUFS trim) and a true-peak-safe ceiling | S | 1 | Drawer; `autoGain`, `ceiling` |
| 7 | Sidechain Duck — already planned for Phase 4, not repeated here. Consider tempo-synced ducking with it (Comp 9) | M | — | `docs/PROGRESS.md`, Phase 4 |

## 2. Fancy Exciter

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Listen to added harmonics (delta), via the existing `ListenCheckbox` | S | 1 | Footer; DSP only |
| 2 | THD / harmonics read-out on Sature's background-analysis path (the 1 kHz test tone) | S | 1 | Meter area; no parameter |
| 3 | Two-band exciter: separate Freq and Harmonics below and above a split (TwinTube, Vitamin) | M | 2 | Drawer for band 2 to keep the layout |
| 4 | Transient emphasis: more harmonics on attacks (Aphex's transient-discriminate generator) | S | 2 | Drawer; `punch` |
| 5 | Low cut / high cut with slopes and pre-or-post placement (Decapitator, ChromaGlow) | S | 2 | Drawer; `lowCut`, `highCut`, `filtersPre` |
| 6 | Per-band stereo width on the generated harmonics only (Vitamin) | S | 2 | Drawer; `harmWidth` |
| 7 | Output true-peak ceiling / auto trim so Clip Boost 24 cannot pass 0 dBFS | S | 1 | Drawer; `ceiling`, `autoGain` |
| 8 | Minimum-phase oversampling filter, for lower latency at 2x–8x (Off already has none) | S | 2 | Drawer; `osPhase`, as Comp and Limit have |

## 3. Fancy Comp

The most complete plugin in the set; the gaps are the standard ones.

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Hold (0–500 ms) in the ballistics; every leading compressor has it | S | 1 | Options page; `hold`, default 0 = today |
| 2 | Range (max GR); pairs with Auto threshold as a level rider | S | 1 | Options page; `range`, default Off = no cap, so today's output to the bit |
| 3 | Delta / listen to removed, through `setListening` as DS and Sature do (Comp's listen today is the detector Preview) | S | 1 | Footer; DSP only |
| 4 | GR history (3 s) in the GR panel, with a peak-hold read-out | S | 1 | GR panel; new shared widget |
| 5 | Lookahead, 0–20 ms, reported as latency; 0 = today (Pro-C 3 has 1–20 ms; Comp has none) | S | 1 | Options page; `lookahead` |
| 6 | Per-channel GR read-out when unlinked (one `GainReductionMeter` today) | S | 1 | GR panel; no parameter |
| 7 | Sidechain EQ from 3 to 6 bands, with shelf and brickwall cut types | M | 2 | Detector page; three new bands |
| 8 | Link modes Stereo / M>S / S>M beyond Link % | M | 2 | Options page; `linkMode` |
| 9 | Tempo-synced trigger: duck on a host-tempo pattern with no sidechain kick (Pro-C 3's Host Sync) | M | 2 | Detector menu; `syncRate`, `syncShape` |

## 4. Fancy EQ

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Band solo: solo what the band contributes (Pro-Q 4, soothe2 pattern) | S | 1 | Double-click tab / Alt-click handle; temporary, never a parameter (§0.3) |
| 2 | Auto gain from the magnitude response (Pro-Q, Kirchhoff, Crave, SlickEQ) | S | 1 | Drawer; `autoGain` |
| 3 | Dynamic ratio per band (0.5:1 … ∞); today Range is the only shape. Default keeps today's curve | S | 1 | DYN page; 24 params |
| 4 | Brickwall cut slope (6–48 dB/oct exist) | S | 2 | Type menu; appended `bNslope` choice |
| 5 | Analyzer freeze / place, and pre and post spectra at once | S | 1 | Analyzer menu (one setting today: Off/Pre/Post) |
| 6 | ▲ Band list and band groups/link (Kirchhoff Band List, Slate Lock) | M | 2 | New panel; needs layout approval |
| 7 | Character output stage: Clean / Subtle / Warm (Pro-Q 4 has the same three) | S | 2 | Output page; `character` |
| 8 | Spectral dynamics: per-frequency gain inside a band (Pro-Q 4). Needs FIR's STFT code moved into `fancy-core`, and adds latency to an EQ that has none at its defaults, so on only when a band asks for it | L | 3 | DYN page |

## 5. Fancy Verb

Verb's modules build the impulse response; only Modulate acts on the sound live (`docs/verb.md`,
section 4). Items that must follow the input level, or need more than the IR, are a new kind of
stage after the convolver and are costed that way.

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Auto gain / equal loudness on IR and preset changes (the library normalises, user files only through the Normalize module) | S | 1 | SETUP drawer |
| 2 | Post-EQ module, 3 bands, applied to the IR | S | 1 | New module card |
| 3 | Drag-drop IR, Export IR, library search and favourites (Altiverb's browser is the bar) | S | 1 | File card menu |
| 4 | Decay module: 3–6 band RT multipliers applied to the IR (Pro-R 2's decay EQ, on the IR) | M | 2 | New module card |
| 5 | Attack / onset control for the generator (Build-up covers density, not onset) | S | 2 | Reverb card |
| 6 | Ducking: a live stage after the convolver, with its own sidechain input bus (Pro-R 2, Cinematic Rooms Pro) | M | 2 | New module card; new bus |
| 7 | Level gate: a live stage after the convolver (a fixed gated sound is already possible with Trim) | M | 2 | New module card |
| 8 | Width up to 200 % as a new parameter; Width keeps its 0–100 range | S | 2 | SETUP drawer; `width2x` |
| 9 | Freeze (input mute plus tail loop); needs its own design study for a convolver | M | 3 | New live stage |
| 10 | Shimmer / pitch feedback; a live feedback stage, not an IR module | M | 3 | New live stage |

## 6. Fancy Delay

Already ahead of the field on per-tap feedback, character, diffusion and ducking.

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Freeze: stop the input, hold the loops (Timeless 3, Rose) | S | 1 | Footer; `freeze` |
| 2 | Feedback cross-mix / feedback pan per tap (Timeless) | S | 1 | Motion page; `tNfbPan` |
| 3 | Reverse taps: read the ring backwards over the tap window | M | 3 | Tone page; `tNreverse` |
| 4 | Pitch-shifted taps, ±12 st | M | 3 | Motion page; `tNpitch` |
| 5 | Output true-peak ceiling so 100–110 % feedback cannot clip the bus | S | 1 | Drawer; `ceiling` |
| 6 | LFO waveform choice and per-tap LFO phase (the quarter-cycle stereo offset is fixed) | S | 2 | Motion page |

## 7. Fancy MultiComp

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Dynamic-phase crossover: phase changes only while gains move, zero latency (Pro-MB). Research risk: no published method | M | 2 | Crossover menu |
| 2 | Per-band lookahead, common delay = max (today one global value) | S | 2 | Band row; eight params |
| 3 | Per-band crossover slope; mixed slopes need all-pass correction to keep the sum flat | M | 2 | EDIT drawer |
| 4 | Learn crossovers from the spectrum (Neutron 5) | S | 1 | Graph button; action, not a parameter |
| 5 | Per-band GR history: stacked 3 s traces | M | 2 | Graph; shared widget |
| 6 | Per-band detector EQ (HP/LP/tilt before each detector) | S | 2 | Band drawer |
| 7 | True band mute, separate from Active off (which keeps the band in the mix) | S | 1 | Band row; `bNmute` |

## 8. Fancy FIR

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Resonance tilt: depth weighted towards highs or lows (soothe3's Tilt) | S | 1 | Resonance controls; `resTilt`, 0 = today |
| 2 | Depth nodes on the Resonance reduction curve (soothe2's node graph); generalises item 1 | S | 2 | Curve editor; `resNodes` in state |
| 3 | Auto threshold for Gate and Compressor curves (measure, then place the curve) | S | 1 | Left card button; action |
| 4 | Per-bin ratio curve for Compressor mode (one ratio today) | S | 2 | Curve 2 in Compressor mode |
| 5 | Mid/Side engine mode, separate curves per domain | M | 2 | Mode row; `msMode` |
| 6 | Attack / Release in EQ mode when the curve is a threshold (Pro-Q 4 spectral dynamics) | M | 2 | Controls card |

## 9. Fancy Gate

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | MIDI input trigger: note-on opens, note-off closes (the plugin emits MIDI but cannot be played; its AU is already a music effect) | S | 1 | Footer; `midiIn` |
| 2 | Ratio (1:1 … ∞) so Range can give a soft expander curve; default ∞ = today's fade | S | 1 | Envelope card; `ratio` |
| 3 | Auto threshold: learn floor and peaks over 3 s | S | 1 | Footer button; action, sets params |
| 4 | Detector modes: Peak, RMS, Percussive/transient (StandardGATE) | M | 2 | Level card; `detMode` |
| 5 | De-bleed: tighten the detector filters as level rises (Gate:Drums) | S | 1 | Detector card; `deBleed` |
| 6 | Upward expansion as the companion to Duck (Pro-G's Upward) | S | 2 | Mode segment; appended `mode` choice |
| 7 | ▲ Multiband gating, 2–3 bands (Neutron 5) | M | 3 | New band row; needs layout approval |
| 8 | Mid/Side detector sources to match Comp's list | S | 1 | Detector menu; two appended choices |
| 9 | Tempo-synced trigger for Duck mode, shared with Comp 9 | S | 2 | Detector menu; after Comp 9 |

## 10. Fancy Limit

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Gain Lock: hold output LUFS while Threshold moves (Pro-L 2, Elevate, Ozone) | S | 1 | Footer; `gainLock` |
| 2 | Auto gain to Target: a button that drives Threshold to the LUFS target | S | 1 | Loudness panel button; action |
| 3 | Pre-limiter clipper with its own drive (Ozone Soft Clip, Limitless; clip-then-limit is the common mastering chain) | M | 2 | Drawer; `clip`, `clipDrive` |
| 4 | DC filter, and a release shape (linear/exponential, Comp's code) | S | 1 | Drawer; `dcFilter`, `releaseCurve` |
| 5 | True-peak max-hold statistics with a reset (Elephant) | S | 1 | Loudness panel; no parameter |
| 6 | ▲ Multiband limiting, 2–4 bands with per-band release (Ozone IRC 5, Limitless) | M | 3 | New graph; needs layout approval |
| 7 | M/S and per-channel link modes (Link % exists) | S | 2 | Drawer; `channel` |
| 8 | External sidechain | S | 2 | Bus layout; detector menu |

## 11. Fancy DS

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Auto threshold: learn the sibilance band over 2 s (Pro-DS Auto, RX) | S | 1 | Footer button; action |
| 2 | Dual-band split, separate low and high sibilance detectors (Pro-DS) | M | 2 | Detection card; second band |
| 3 | Detection filter slope: 24 and 48 dB/oct beside today's 12 (one SVF) | S | 1 | Detection card; `slope` |
| 4 | Per-channel M/S reduction (detection already offers Mid and Side) | S | 2 | Mode area; `reduceChan` |
| 5 | Output true-peak ceiling (reuse `TruePeakLimiter`) | S | 1 | Drawer; `ceiling` |
| 6 | Hiss mode: broadband high-shelf variant of the same engine | S | 2 | Detection segment; appended `shape` choice |

## 12. Fancy DS-Mini

One knob by design; settings that need DS's controls stay in DS.

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Auto Amount: ride Amount from the measured sibilance-to-vocal ratio | S | 1 | Amount area; `autoAmount` |
| 2 | Hiss profile, once DS has its hiss mode | S | 2 | Profile list |

## 13. Fancy Sature

Sature saturates one band, between Low and High; everything outside it passes clean by design
(`docs/sature.md`). 4x stays the most oversampling (Decision 7, A).

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Saturate the outer bands too: own Drive and Mix below Low and above High, through the same `added` path; default 0 = today | M | 2 | Drawer; four params |
| 2 | Per-band dynamics (Saturn 2's gate↔compress per band), after item 1 | M | 3 | Band section; `dyn` per band |
| 3 | Two more curve families: diode/rectify (even only) and fold | M | 2 | Type segment; appended choices |
| 4 | Curve shaping: Offset + Knee warps any curve (Kraftur) | S | 2 | Drawer; `curveOffset`, `curveKnee` |
| 5 | Low cut / high cut with slopes, pre or post (Decapitator, ChromaGlow, Harmonics) | S | 2 | Drawer; `lowCut`, `highCut` |
| 6 | Modulation: one LFO plus envelope follower on Drive, Tone or Character | M | 3 | Drawer; three params + target |
| 7 | Harmonic spread: per-channel harmonic phase so added content widens (Kelvin) | S | 2 | Drawer; `spread` |
| 8 | Mid/Side saturation | S | 2 | Drawer; `channel` |

## 14. Fancy Room

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Mod Rate and Mod Depth split from the single Mod % (Illusion, Crystalline) | S | 1 | Space card; `modRate`, `modDepth` |
| 2 | Per-band decay: 3-band RT multipliers before Damping (Pro-R 2's decay EQ) | M | 2 | Tone card; `lowDecay`, `highDecay` |
| 3 | 3-band reverb EQ after the network (cuts alone cannot sculpt a tail) | S | 1 | Tone card; three bands |
| 4 | Attack / onset control (Crystalline, Pro-R 2) | S | 2 | Space card; `attack` |
| 5 | External sidechain for Ducking, plus a duck listen; a new input bus, so the host checks run again | M | 2 | Bus layout; detector menu |
| 6 | Gate for drums (Pro-R 2, Lustrous Plates) | S | 1 | Tone card; `gate`, `gateDecay` |
| 7 | Auto gain on Size and Decay changes | S | 1 | Drawer; `autoGain` |
| 8 | Shimmer / pitch feedback path | M | 3 | Tone card; `shimmer` |

---

## Cross-cutting work

Build these once in `fancy-core`, then switch them on per plugin. They are what makes the set feel
like one product rather than fourteen.

| Piece | Status today | Fixes |
|---|---|---|
| Generic delta (listen to removed) | `setListening` / `ListenCheckbox` exist; DS, DS-Mini, Sature and FIR use them for a real listen, Comp and Gate for detector preview only; Limit's is a parameter, by your choice | Comp, MultiComp, EQ, Room, Exciter, Kicker |
| Level / GR history widget | `DeEsserHistory` (DS, DS-Mini), Gate's own, Limit's own, one `GainReductionMeter` | Comp, MultiComp, and unifies the three |
| Loudness-matched auto gain | `LoudnessMeter` exists (Limit) | EQ, Verb, Room, Kicker, Limit |
| Analyzer freeze and pre/post overlay | EQ's analyzer, one setting Off/Pre/Post | EQ, FIR, Verb, DS |
| Tempo-synced trigger | none; Delay reads the host tempo | Comp, Gate, Kicker's Phase 4 Duck |
| Mid/Side processing path | Detectors offer Mid/Side (Comp, DS); EQ bands have a channel; no other plugin processes M/S as a stage | Limit, Sature, FIR |
| Crossover with dynamic phase | MultiComp has LR24, LR48 and linear | MultiComp, Gate (multiband), Limit (multiband) |

**Deliberately out of scope:** immersive/Atmos; cross-instance features (Pro-Q 4's instance list,
EQ collision against other instances), plumbing for one user; oversampling above 8x (Sature's
Decision 7, Limit's CPU target); hardware-emulation catalogues (Opto/FET/VCA style models: Comp's
Character, Classic and Weird already cover its colour); machine-learning restoration (Ozone's
Unlimiter, stem EQ); MIDI learn (REAPER and Live map host parameters already); a CLAP build (JUCE
8.0.17, as pinned, has no native CLAP, so it needs a new dependency, and nothing here uses CLAP-only
features).

## Suggested order (within the Phase 4 order above)

1. **Wave 1** — every S item above with no new engine: EQ band solo, auto gain and analyzer freeze;
   Comp Hold, Range, delta, GR history and lookahead; Limit gain lock, auto gain and DC filter; Gate
   MIDI in, ratio, auto threshold and Mid/Side sources; DS auto threshold and slope; Delay freeze and
   ceiling; Verb auto gain, Post-EQ module and IR browser; Room Mod Rate/Depth, reverb EQ and gate;
   Exciter delta and THD; Kicker sub length, make-room, polarity and auto gain; FIR resonance tilt.
2. **Wave 2** — one design note per item, each with a Python model first, in this order:
   MultiComp dynamic-phase crossover, FIR depth nodes, Comp tempo-synced trigger (then Gate's),
   Sature outer-band drive, Verb decay module, Verb ducking and level gate, Limit clipper, DS
   dual-band, Gate detector modes.
3. **Wave 3** — Limit multiband (▲), Gate multiband (▲), EQ spectral dynamics, Verb freeze and
   shimmer, Room shimmer, Delay reverse and pitch taps.

The three items marked ▲ (EQ's band list, Gate's multiband, Limit's multiband) change the visible
layout and need your approval before a design note is written; everything else fits the drawer and
EDIT patterns already in use.

---

## Review (17 November 2026)

**Removed, against a standing decision:** Kicker's pitch-tracked sub with target note and glide
(Sub mode is the divider; a third engine needs your decision first) and its small-speaker
harmonics (the Harmonics mode dropped on 11 November); Sature's 16x (Decision 7, A); Comp's style
models (hardware emulation, out of scope); EQ collision against other instances (out of scope).

**Removed, low value or duplicated:** FIR de-hum (4096-point bins are 11.7 Hz wide at 48 kHz, too
coarse for hum; an EQ notch does it better); FIR sidechain in Resonance (resonances belong to the
signal itself); Delay tap patterns (presets hold every tap), manual tap tempo (the target hosts
send tempo) and per-tap filter types (each tap has LP and HP); DS reduction tone (Band/Shelf and
Freq shape the cut); DS-Mini custom profile (user presets) and detector width (against its one-knob
design); Limit 16x/32x (CPU target); Room tail control (the tail follows Decay and Freeze); Kicker
sub band width (Sub mono exists); EQ fractional slopes; all-pass bands in Comp's detector EQ.

**Corrected:** Kicker's retrigger already exists (only sub length remains); Kicker alignment would
duplicate the detector compensation (only polarity remains); Exciter's Off oversampling already has
zero latency; Comp's Range defaults to Off, not 60 dB, to stay bit-identical; Limit's listen is a
parameter, not `setListening`; Verb's Width can't change range, so 200 % is a new parameter; Sature
has one saturated band, so per-band items were rewritten around saturating the outer bands.

**Re-costed:** Verb ducking, level gate, freeze and shimmer are live stages after the convolver,
not IR modules; ducking also needs a sidechain bus (S to M). EQ spectral dynamics (M to L). MultiComp
per-band slopes (S to M). Room's external sidechain (S to M).

**Added from research:** Comp lookahead (Comp has none; Pro-C 3 offers 1–20 ms); a tempo-synced
trigger for Comp, Gate's Duck and Kicker's Phase 4 Duck (Pro-C 3's Host Sync); FIR resonance tilt
(soothe3); a minimum-phase oversampling filter for Exciter; DS filter slope.
