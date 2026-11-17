# Enhancement roadmap

Proposed additions for the fourteen plugins, benchmarked against the plugins that define each
category in 2025–26 and **verified against the source** on 17 November 2026 (parameter lists read
from every `plugins/*/src/*Processor.cpp`, then each item checked in the DSP and editor).

This file is the backlog. When an item's turn comes it gets its own design note, as every other
item in `docs/` has, with the model, decisions and tests the briefs require. Nothing here is
approved yet: the parameter list and the visible layout only change after you answer a design
note, per `AGENTS.md`.

## How to read the tags

**Effort** — S: one short push, the pattern of an existing feature. M: a phase item, two or three
pushes, a Python model before code. L: new engine or a new shared subsystem.

**Wave** — 1: cheap and append-only (drawer settings or new parameters whose defaults keep saved
projects bit-identical). 2: engine work, one item at a time. 3: category-defining, larger.

**▲** — cannot be hidden in a drawer, so it changes the visible layout and needs your approval
before any code.

Every item is appended after the existing parameters, so no ID or index moves; where a new setting
changes the sound it defaults to today's path, and `stateVersion` is bumped only if the meaning of
a stored value changes.

## Verification notes

Checked and **not** gaps, so nothing below re-proposes them: Comp's "Brickwall" is a factory preset
name, not a sidechain brickwall filter; FIR's "De-mud" and "Tilt" are EQ curve buttons, not a
de-hummer; Delay's `TapMap` is the drag-handle time graph, not pattern storage; EQ already has
All-pass and Tilt types and its analyzer already has Mid/Side/Left/Right; Room's Width 0 is a mono
maker already; DS already has Mid/Side **detection** (what it lacks is per-channel reduction);
Verb's Reverb generator already has Build-up; Comp's Preview is a detector listen, not a delta.

---

## 1. Fancy Kicker (Boost + Sub)

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | ▲ Pitch-tracked sub: note display, target note, glide (SubKick 2, SubCulture). Keeps the divider for character | M | 3 | New row in Sub mode; `subTrack`, `subNote`, `subGlide` |
| 2 | Sub length + phase-safe retrigger, so a sub cannot ring through the next hit | S | 1 | Drawer; `subLen` |
| 3 | Small-speaker harmonics: 2nd/3rd of the sub (Little Foot 2's Harmonic mode) | S | 1 | Drawer; `subHarmonics` |
| 4 | Make-room: resonant high-pass at Freq cutting the source (Dropkick, Rumble Forge) | S | 1 | Drawer; `roomCut`, `roomFreq` |
| 5 | Polarity invert and alignment delay (±5 ms) instead of hidden compensation | S | 1 | Drawer; `subPolarity`, `subAlign` |
| 6 | Sub band width, and a sub-only output meter | M | 2 | Right column; `subWidth` |
| 7 | Oversample the soft clipper (reuse `SwitchedOversampler`) | S | 2 | Drawer; `clipOs` |
| 8 | Auto gain (RMS/LUFS trim) and a true-peak-safe ceiling | S | 1 | Drawer; `autoGain`, `ceiling` |
| 9 | Sidechain Duck — already planned for Phase 4, not repeated here | M | — | `docs/PROGRESS.md`, Phase 4 |

## 2. Fancy Exciter

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Listen to added harmonics (delta), via the existing `ListenCheckbox` | S | 1 | Footer; DSP only |
| 2 | THD / harmonics read-out on Sature's background-FFT path | S | 1 | Meter area; no parameter |
| 3 | Two-band exciter: separate Freq and Harmonics below and above a split (TwinTube, Vitamin) | M | 2 | Drawer for band 2 to keep the layout |
| 4 | Transient emphasis: more harmonics on attacks (Aphex's transient-discriminate generator) | S | 2 | Drawer; `punch` |
| 5 | Low cut / high cut with slopes and pre-or-post placement (Decapitator, ChromaGlow) | S | 2 | Drawer; `lowCut`, `highCut`, `filtersPre` |
| 6 | Per-band stereo width on the generated harmonics only (Vitamin) | S | 2 | Drawer; `harmWidth` |
| 7 | Output true-peak ceiling / auto trim so Clip Boost 24 cannot pass 0 dBFS | S | 1 | Drawer; `ceiling`, `autoGain` |
| 8 | Oversampling filter phase choice, enabling a zero-latency Off path | S | 2 | Drawer; `osPhase` |

## 3. Fancy Comp

The most complete plugin in the set; the gaps are the standard ones.

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Hold (0–500 ms) in the ballistics; every leading compressor has it | S | 1 | Options page; `hold`, default 0 = today |
| 2 | Range (max GR); pairs with Auto threshold as a level rider | S | 1 | Options page; `range`, default 60 dB = today |
| 3 | Delta / listen to removed (Limit, DS and Sature already use `setListening`; Comp's listen is the detector Preview) | S | 1 | Footer; DSP only |
| 4 | GR history (3 s) in the GR panel, with a peak-hold read-out | S | 1 | GR panel; new shared widget |
| 5 | Auto gain (loudness-matched, 2–3 s) beside Auto make-up (`LoudnessMeter` exists) | S | 1 | Options page; `autoGain` |
| 6 | Per-channel GR read-out when unlinked (one `GainReductionMeter` today) | S | 1 | GR panel; no parameter |
| 7 | Style models: Clean / Opto / FET / VCA / Vari-Mu as ballistics + curve presets (Pro-C 3) | M | 2 | Options page; `style`, default Clean |
| 8 | Sidechain EQ from 3 to 6 bands, with all-pass and shelf types (Pro-C 3's section) | M | 2 | Detector page; six new bands |
| 9 | Link modes Stereo / M>S / S>M beyond Link % | M | 2 | Options page; `linkMode` |

## 4. Fancy EQ

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Band solo: solo what the band contributes (Pro-Q 4, soothe2 pattern) | S | 1 | Double-click tab / Alt-click handle; no parameter |
| 2 | Auto gain from the magnitude response (Pro-Q, Kirchhoff, Crave, SlickEQ) | S | 1 | Drawer; `autoGain` |
| 3 | Dynamic ratio per band (0.5:1 … ∞) instead of only a cap; default keeps today's shape | S | 1 | DYN page; 24 params |
| 4 | Spectral dynamics: per-frequency gain inside a band, with density and tilt (Pro-Q 4) | M | 2 | DYN page; new `fancy-core` STFT path (shares FIR's) |
| 5 | Brickwall and fractional slopes for cuts (Pro-Q 4 has 10 shapes) | S | 2 | Type menu; `bNslope` extension |
| 6 | Analyzer freeze / place, and pre and post spectra at once | S | 1 | Analyzer menu (one setting today: Off/Pre/Post) |
| 7 | Collision / masking against a reference instance or the sidechain (Pro-Q 4, Neutron) | M | 3 | Analyzer overlay; instance registry in `fancy-core` |
| 8 | ▲ Band list and band groups/link (Kirchhoff Band List, Slate Lock) | M | 2 | New panel; needs layout approval |
| 9 | Character output stage: Clean / Subtle / Warm | S | 2 | Output page; `character` |

## 5. Fancy Verb

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Ducking module: amount, threshold, release, external sidechain (Pro-R 2, Cinematic Rooms Pro) | S | 1 | New module card |
| 2 | Gate module: auto gate with threshold and decay (Pro-R 2, Lustrous Plates) | S | 1 | New module card |
| 3 | Auto gain / equal loudness on IR and preset changes (the library normalises, user files do not) | S | 1 | SETUP drawer |
| 4 | Decay module: 3–6 band RT multipliers applied to the IR (Pro-R 2's decay EQ, on the IR) | M | 2 | New module card |
| 5 | Post-EQ module, 3 bands, after the chain | S | 1 | New module card |
| 6 | Attack / onset control for the generator (Build-up covers density, not onset) | S | 2 | Reverb card |
| 7 | Drag-drop IR, Export IR, library search and favourites (Altiverb's browser is the bar) | S | 1 | File card menu |
| 8 | Width to 200 % (today 0–100 W) and a wet-only low mono maker | S | 2 | Main layout knob range |
| 9 | Freeze (input mute plus tail loop); needs its own design study for a convolver | M | 3 | New module |
| 10 | Shimmer / pitch feedback module | M | 3 | New module |

## 6. Fancy Delay

Already ahead of the field on per-tap feedback, character, diffusion and ducking.

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Freeze: stop the input, hold the loops (Timeless 3, Rose) | S | 1 | Footer; `freeze` |
| 2 | Tap patterns: save/recall a tap set across instances (`TPAT` child in state + menu) | S | 1 | Time card menu |
| 3 | Feedback cross-mix / feedback pan per tap (Timeless) | S | 1 | Motion page; `tNfbPan` |
| 4 | Reverse taps: read the ring backwards over the tap window | M | 3 | Tone page; `tNreverse` |
| 5 | Pitch-shifted taps, ±12 st | M | 3 | Motion page; `tNpitch` |
| 6 | Manual tap tempo and BPM entry when the host gives no tempo | S | 1 | Time card; `tapTempo`, `bpm` |
| 7 | Per-tap filter type (LP/HP/BP/shelf) | S | 2 | Tone page; `tNfilter` |
| 8 | Output true-peak ceiling so 100–110 % feedback cannot clip the bus | S | 1 | Drawer; `ceiling` |
| 9 | LFO waveform choice and per-tap LFO phase (the quarter-cycle stereo offset is fixed) | S | 2 | Motion page |

## 7. Fancy MultiComp

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Dynamic-phase crossover: phase changes only while gains move, zero latency (Pro-MB) | M | 2 | Crossover menu |
| 2 | Per-band lookahead, common delay = max (today one global value) | S | 2 | Band row; eight params |
| 3 | Per-band crossover slope (12/24/48 LR, 6–48 linear) | S | 2 | EDIT drawer |
| 4 | Learn crossovers from the spectrum (Neutron 5) | S | 1 | Graph button; action, not a parameter |
| 5 | Per-band GR history: stacked 3 s traces | M | 2 | Graph; shared widget |
| 6 | Per-band detector EQ (HP/LP/tilt before each detector) | S | 2 | Band drawer |
| 7 | True band mute, separate from Active off (which keeps the band in the mix) | S | 1 | Band row; `bNmute` |

## 8. Fancy FIR

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Depth nodes on the Resonance reduction curve (soothe2's node graph) | S | 2 | Curve editor; `resNodes` in state |
| 2 | Auto threshold for Gate and Compressor curves (measure, then place the curve) | S | 1 | Left card button; action |
| 3 | Per-bin ratio curve for Compressor mode (one ratio today) | S | 2 | Curve 2 in Compressor mode |
| 4 | Mid/Side engine mode, separate curves per domain | M | 2 | Mode row; `msMode` |
| 5 | Attack / Release in EQ mode when the curve is a threshold (Pro-Q 4 spectral dynamics) | M | 2 | Controls card |
| 6 | De-hum: harmonic comb from a detected or typed fundamental | S | 2 | Left card button (De-mud covers one case only) |
| 7 | Sidechain in Resonance (greyed today) | S | 1 | Footer; enables the existing bus |

## 9. Fancy Gate

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | MIDI input trigger: note-on opens, note-off closes (the plugin emits MIDI but cannot be played) | S | 1 | Footer; `midiIn` |
| 2 | Ratio (1:1 … ∞) so Range can give a soft expander curve; default ∞ = today's fade | S | 1 | Envelope card; `ratio` |
| 3 | Auto threshold: learn floor and peaks over 3 s | S | 1 | Footer button; action, sets params |
| 4 | Detector modes: Peak, RMS, Percussive/transient (StandardGATE) | M | 2 | Level card; `detMode` |
| 5 | De-bleed: tighten the detector filters as level rises (Gate:Drums) | S | 1 | Detector card; `deBleed` |
| 6 | Upward expansion as the companion to Duck (Pro-G's Upward) | S | 2 | Mode segment; `mode` choice |
| 7 | ▲ Multiband gating, 2–3 bands (Neutron 5) | M | 3 | New band row; needs layout approval |
| 8 | Mid/Side detector sources to match Comp's list | S | 1 | Detector menu; two choices |

## 10. Fancy Limit

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Gain Lock: hold output LUFS while Threshold moves (Pro-L 2, Elevate, Ozone) | S | 1 | Footer; `gainLock` |
| 2 | Auto gain to Target: a button that drives Threshold to the LUFS target | S | 1 | Loudness panel button; action |
| 3 | Pre-limiter clipper with its own drive (Ozone Soft Clip, Limitless) | M | 2 | Drawer; `clip`, `clipDrive` |
| 4 | DC filter and release shape (linear/exponential, share Comp's code) | S | 1 | Drawer; `dcFilter`, `releaseCurve` |
| 5 | True-peak max-hold statistics with a reset (Elephant) | S | 1 | Loudness panel; no parameter |
| 6 | ▲ Multiband limiting, 2–4 bands with per-band release (Ozone IRC 5, Limitless) | M | 3 | New graph; needs layout approval |
| 7 | M/S and per-channel link modes | S | 2 | Drawer; `channel` |
| 8 | 16x / 32x oversampling as appended choices | S | 2 | Oversampling choice |
| 9 | External sidechain | S | 2 | Bus layout; detector menu |

## 11. Fancy DS

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Auto threshold: learn the sibilance band over 2 s (Pro-DS Auto, RX) | S | 1 | Footer button; action |
| 2 | Tone of the reduction: tilt the cut between 5–8 kHz and 8–12 kHz | S | 1 | Reduction card; `cutTone` |
| 3 | Dual-band split, separate low and high sibilance detectors (Pro-DS) | M | 2 | Detection card; second band |
| 4 | Detection filter slope (12/24/48 dB/oct) | S | 1 | Detection card; `slope` |
| 5 | Per-channel M/S reduction (detection already offers Mid and Side) | S | 2 | Mode area; `reduceChan` |
| 6 | Output true-peak ceiling (reuse `TruePeakLimiter`) | S | 1 | Drawer; `ceiling` |
| 7 | Hiss mode: broadband high-shelf variant of the same engine | S | 2 | Detection segment; `shape` choice |

## 12. Fancy DS-Mini

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Auto Amount: ride Amount from the measured sibilance-to-vocal ratio | S | 1 | Amount area; `autoAmount` |
| 2 | Custom profile: save current DS settings as a seventh profile | S | 1 | Profile menu; state child |
| 3 | Detector width for voices outside the six profiles | S | 1 | EDIT drawer; `width` |
| 4 | Hiss profile, once DS has its hiss mode | S | 2 | Profile list |

## 13. Fancy Sature

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Per-band Drive and Mix for the Low and High bands | S | 1 | Drawer; four params, defaults = today |
| 2 | Per-band dynamics (Saturn 2's gate↔compress per band) | M | 2 | Band section; `dyn` per band |
| 3 | Two more curve families: diode/rectify (even only) and fold | M | 2 | Type segment |
| 4 | Curve shaping: Offset + Knee warps any curve (Kraftur) | S | 2 | Drawer; `curveOffset`, `curveKnee` |
| 5 | Low cut / high cut with slopes, pre or post (Decapitator, ChromaGlow, Harmonics) | S | 2 | Drawer; `lowCut`, `highCut` |
| 6 | Modulation: one LFO plus envelope follower on Drive, Tone or Character | M | 3 | Drawer; three params + target |
| 7 | 16x oversampling as an appended choice (Decision 7's B, measurable now) | S | 2 | Oversampling choice |
| 8 | Harmonic spread: per-channel harmonic phase so added content widens (Kelvin) | S | 2 | Drawer; `spread` |
| 9 | Mid/Side saturation | S | 2 | Drawer; `channel` |

## 14. Fancy Room

| # | Item | Effort | Wave | Where it goes |
|---|---|---|---|---|
| 1 | Mod Rate and Mod Depth split from the single Mod % (Illusion, Crystalline) | S | 1 | Space card; `modRate`, `modDepth` |
| 2 | Per-band decay: 3-band RT multipliers before Damping (Pro-R 2's decay EQ) | M | 2 | Tone card; `lowDecay`, `highDecay` |
| 3 | 3-band reverb EQ after the network (cuts alone cannot sculpt a tail) | S | 1 | Tone card; three bands |
| 4 | Attack / onset control (Crystalline, Pro-R 2) | S | 2 | Space card; `attack` |
| 5 | External sidechain for Ducking, plus a duck listen | S | 1 | Bus layout; detector menu |
| 6 | Gate module for drums (Pro-R 2, Lustrous Plates) | S | 1 | Tone card; `gate`, `gateDecay` |
| 7 | Tail control, independent of Decay, for export and CPU | S | 1 | Drawer; `tail` |
| 8 | Auto gain on Size and Decay changes | S | 1 | Drawer; `autoGain` |
| 9 | Shimmer / pitch feedback path | M | 3 | Tone card; `shimmer` |

---

## Cross-cutting work

Build these once in `fancy-core`, then switch them on per plugin. They are what makes the set feel
like one product rather than fourteen.

| Piece | Status today | Fixes |
|---|---|---|
| Generic delta (listen to removed) | `setListening` / `ListenCheckbox` exist; five plugins use them for real listen, Comp and Gate for detector preview only | Comp, MultiComp, EQ, Verb, Room, Exciter, Kicker |
| Level / GR history widget | `DeEsserHistory` (DS, DS-Mini), Gate's own, Limit's own, one `GainReductionMeter` | Comp, MultiComp, and unifies the three |
| Loudness-matched auto gain | `LoudnessMeter` exists (Limit) | EQ, Comp, MultiComp, Verb, Room, Kicker, Limit |
| Analyzer freeze and pre/post overlay | EQ's analyzer, one setting Off/Pre/Post | EQ, FIR, Verb, DS |
| 16x / 32x oversampling | `SwitchedOversampler` (Sature, Exciter) | Sature, Exciter, Comp, Limit |
| Mid/Side processing path | Detectors offer Mid/Side (Comp, DS); no plugin processes M/S as a stage | Limit, Sature, Gate, FIR, Delay |
| Crossover with dynamic phase | MultiComp has LR24, LR48 and linear | MultiComp, Gate (multiband), Limit (multiband) |

**Deliberately out of scope:** immersive/Atmos, cross-instance EQ collision (plumbing for one user),
oversampling above 32x, hardware-emulation catalogues.

## Suggested order

1. **Wave 1** — every S item above with no new engine: EQ band solo + auto gain; Comp Hold, Range,
   delta and GR history; Limit gain lock and auto gain; Gate MIDI in, ratio and auto threshold; DS
   auto threshold and reduction tone; Delay freeze and tap patterns; Verb duck, gate and auto gain;
   Room Mod Rate/Depth, reverb EQ and gate; Sature per-band drive/mix; Exciter delta and THD; Kicker
   sub length, harmonics, make-room, polarity and auto gain.
2. **Wave 2** — one design note per item, each with a Python model first, in this order: EQ spectral
   dynamics, MultiComp dynamic-phase crossover, FIR depth nodes, Sature per-band dynamics, Verb
   decay module, Limit clipper then multiband, DS dual-band, Gate detector modes.
3. **Wave 3** — Limit spectral limiting, Verb freeze and shimmer, Delay reverse and pitch taps,
   Kicker three-band sub, EQ collision display.

The four items marked ▲ (Kicker's pitch row, EQ's band list, Gate's multiband, Limit's multiband)
change the visible layout and need your approval before a design note is written; everything else
fits the drawer and EDIT patterns already in use.
