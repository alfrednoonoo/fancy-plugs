# Phase 2.1, item U: upward compression, Depth and Time in Fancy MultiComp

Brief: §U of `docs/BUILD_PROMPT_PHASE2_1.md`. Mockup: panel 4 of `reference/phase2_1/14__phase2_1_controls.png`. MultiComp's own design is `docs/multicomp.md`.

## 1. What it does

Each band can lift quiet material (upward compression) as well as push down loud material (the existing compressor). Two global controls, Depth and Time, scale every band's gain change and every band's attack and release, so the "heavy, squashed" multiband sound is quick to dial in.

## 2. New parameters

Appended after every existing parameter, so no ID or index moves. Per band, `N` from 1 to 8:

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `bNup` | Band N Upward | off/on | **off** | | |
| `bNuthr` | Band N Up threshold | -80 to 0 | -40 | dB | linear |
| `bNuratio` | Band N Up ratio | 1:1 to 1:10 | 1:2 | | log |
| `bNlift` | Band N Max lift | 0 to 30 | 12 | dB | linear |
| `bNfloor` | Band N Floor | -100 to -40 | -70 | dB | linear |

Global:

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `depth` | Depth | 0 to 100 | 100 | % | linear |
| `time` | Time | 10 to 1000 | 100 | % | log, 100 % at the centre |

Up ratio stores `r`, shown and typed as `1:r` (`2`, `1:2` and `1:2.0` all mean 1:2). All are automatable and accept typed values. Splitting, merging, copying and resetting a band carry the upward settings with the rest. Link bands applies to Up threshold as it does to Threshold.

**Factory preset "Big squash":** 3 bands, crossovers at 88 Hz and 2.5 kHz. Every band: downward 4:1 above -18 dB, upward on at 1:4 below -40 dB, Max lift 18 dB, Floor -70 dB, Attack 3 ms, Release 200 ms (Auto release off, so the release is what's set), Depth 100 %.

## 3. State and compatibility

- `stateVersion` stays at 3. D's migration already gives a parameter missing from a state its default, so a v1.1.0 project loads with Upward off, Depth 100 % and Time 100 %.
- With those defaults the band's gain is computed by the same expression as today: Time multiplies the attack and release by exactly 1.0, Depth multiplies nothing, and the upward part is skipped. So existing projects render bit-identically (test 1).

## 4. DSP

Per band, in `MultiCompDsp::compressBand`, from the same detector level `x` (dB) the downward part uses:

- **Downward:** unchanged.
- **Upward:** with `U` the Up threshold clamped to at most the downward Threshold, `lift = min(MaxLift, (U − x) × (1 − 1/r))` for `x < U`, else 0. It's multiplied by a fade from 0 at Floor to 1 at Floor + 8 dB, and is 0 below Floor, so silence and noise aren't raised.
- **Ballistics:** the lift has its own envelope follower with the band's attack and release (and Auto release), so attack means the lift growing and release means it going away, as the downward part does for its reduction.
- **Total:** `gain dB = Gain + Depth × (lift − reduction)`, and Auto make-up is scaled by Depth too, so Depth 0 % leaves only the band's own Gain: the band sum, the Dry path, when Gain is 0. With Upward off and Depth 100 %, the expression is the existing `Gain − reduction`.
- **Time:** every band's attack and release (and the Auto release's fast stage, which follows the release) are multiplied by Time before the ballistics are set, so 200 % doubles the 63 % times.
- **Smoothing:** Depth is smoothed per sample like the band's other controls (20 ms); the upward parameters go through the same smoothers as Threshold. Time takes effect when the ballistics are next set, as Attack and Release do now.
- **Meter:** the GR meter keeps showing reduction. Lift shows on the band's transfer curve.

**Latency:** none added. **CPU:** with Upward off, one extra comparison per sample; with it on, one more follower and a few operations per sample per band. Measured by the §0.4 method, 4 bands with Upward off and on, as a ratio to Fancy Comp.

## 5. Controls (panel 4)

The user approved the taller window and leaving out the "Downward" label on 8 November 2026.

The window grows from 760 × 470 to 760 × 589, the height of the new row, so nothing that exists moves sideways:

- **UPWARD row:** a second card under the band card, the full card width (13 to 623, 112 px high). From the left: the label **UPWARD** in the band colour, then Up thresh, Up ratio, Max lift and Floor (the band's tight knobs), an **Upward on** checkbox, and the band's **transfer curve** at the right end.
- **Transfer curve:** input level against output level from -90 to 0 dB, with a dashed 1:1 line, the band's static curve (downward, upward, Max lift and Floor, scaled by Depth) and vertical guides at Floor, Up threshold and Threshold. A clamped Up threshold is drawn at the Threshold with a small "clamped" note.
- **Depth and Time:** two small teal knobs at the top of the right column; the Out meter and the Wet and Dry faders move down under them, at their current sizes.
- **Footer:** moves down with the new row; nothing in it changes.
- The existing DOWNWARD card and graph keep their positions, so the mockup's "Downward" label isn't added.

## 6. Tests (Catch2)

1. **Compatibility:** Upward off, Depth 100 %, Time 100 %: bit-identical to the same band processed without the new parameters, and a state without the new IDs loads with those defaults and renders bit-identically.
2. **Static curve:** sines from -90 to 0 dB in 2 dB steps through one band (others off by Solo), slow ballistics settled: the output level matches the formula within 0.3 dB, including the Max-lift cap and the Floor fade.
3. **Depth 0 %:** output equals the Dry path (band sum) within 0.01 dB, with compression and upward on.
4. **Time 200 %:** measured 63 % attack and release times double, ±10 %.
5. **Floor:** silence, and noise at -90 dB, with Upward on: the output isn't raised.
6. **Big squash:** on pink noise and a drum-like loop, with Limit output on: no NaN or inf, and the output stays below 0 dBFS.
7. **Host:** H's offline-render test with upward bands and Depth and Time automated; the flood test covers the new parameters.
8. **Clamp:** an Up threshold above Threshold acts as Threshold.
