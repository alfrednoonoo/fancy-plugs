# Fancy Plugs against the market leaders: summary

November 2026. The full evidence and sources are in `2026-11-market-report-detailed.md`.

## How this was compared

- **What was compared:** our numbers, measured in CI and recorded in `docs/measurements/`, against the leaders' own manuals and product pages, independent reviews, and user reports.
- **Not head to head.** No competitor was run beside ours. They're commercial binaries that can't be installed on the CI runners, and nobody publishes CPU figures measured the same way, so CPU can't be ranked.
- **The rankings are soft.** Most "best of 2026" lists are affiliate pages. The vendors' own documentation is treated as stronger evidence, and the lists only as a sign of what's popular.
- **Phase 3 can fix this.** It proposes a small benchmark host that runs on your PC, so plugins you own or trial can be measured against ours in the same session. Done: see "Head to head (Phase 3 Q1)" below.

## The leaders, by category

| Our plugin | Leaders in 2026 | The strongest free option |
|---|---|---|
| Kicker (sub enhancer) | Waves MaxxBass and Renaissance Bass, Waves LoAir, Baby Audio SubCulture, released July 2026 | |
| Exciter | Ozone Exciter, Aphex Aural Exciter (Waves or UAD), Process.Audio Sugar, Waves Vitamin, Softube Harmonics | |
| Comp | FabFilter Pro-C 2 and Pro-C 3, released January 2026, plus UAD and Waves hardware models | TDR Kotelnikov |
| EQ | FabFilter Pro-Q 4 | TDR Nova |
| Verb (convolution) | Audio Ease Altiverb 8, LiquidSonics Reverberate 3 | MConvolution |
| Delay | Soundtoys EchoBoy, FabFilter Timeless 3, Valhalla Delay | TAL-Dub-X, Valhalla Freq Echo |
| MultiComp | FabFilter Pro-MB, Ozone Dynamics, Waves C6 | Xfer OTT |
| FIR (spectral) | iZotope RX, soothe3 for resonances, Trackspacer for spectral ducking | |

## Where we're ahead

1. **Typed values everywhere.** Every knob and fader in all eight plugins takes typed values, including `2.5k`, `-inf`, `1:4` and `60 L`, and rejects text with no number in it.
2. **Measured, tested correctness.** Every claim is a test that runs on every build. Examples:
   - The EQ's 18 kHz bell is within 0.42 dB of analog.
   - MultiComp's bands sum within 0.03 dB.
   - FIR's latency matches its report in all 90 configurations.
   - The Comp's limiter holds its ceiling at 200 dB over threshold.

   The leaders make similar claims, but don't publish tests for them.
3. **Phase-correct parallel compression in MultiComp.** Its Dry signal is the sum of the split bands, so mixing Dry and Wet never combs, even in the minimum-phase modes. Pro-MB gets a similar result differently, with a "Dynamic Phase" mode that's flat until a band changes gain.
4. **Real-time noise reduction that learns once and stops.** FIR's Subtract mode cut the frame-to-frame variance of the residual (the "musical noise") by 2.6 times against plain subtraction, and the sine changed by 0.03 dB. RX's manual still asks you to re-learn after changing FFT size. Ours keeps the profile at a fixed resolution, so you don't have to.
5. **Free, consistent and low latency.** All eight share one look and one set of gestures. Every plugin is zero-latency by default except FIR, and every latency is reported exactly.
6. **The Delay's per-tap control.** 16 taps, each with its own feedback, filters inside the loop, bit crush, width, pan and ping-pong, on a draggable tap map. EchoBoy's Rhythm mode also has 16 taps, but with only volume and EQ for each.

## Where we fall behind

| Plugin | Main gaps against the leaders |
|---|---|
| Kicker | It boosts a band of what's already there. MaxxBass adds harmonics so bass carries on small speakers, LoAir and SubCulture synthesise a new sub an octave down, and SubCulture tracks pitch. Ours can't create bass that isn't in the source |
| Exciter | One band and one type of saturation. Leaders offer several bands, several types and width control. With Oversampling off, our aliasing is high, at -20 dBFS. It's fine at the default 4x, where it's -114 dBFS |
| Comp | No saturation stage, no upward compression, no styles and no auto threshold. Pro-C 3 has 14 styles, three saturation types, oversampling to 32x, a 6-band sidechain EQ and Atmos. Ours goes to 4x |
| EQ | 16 bands against 24, and no dynamic EQ, spectral dynamics, mid/side, EQ Match or Natural Phase |
| Verb | No impulse-response library, true stereo or modulation. Altiverb ships a large library of real spaces, and Reverberate 3 adds modulation and its Fusion-IR format. Ours needs your own files or its generators |
| Delay | No modulation (wow and flutter), tape or bucket-brigade character, diffusion, ducking, pitch or reverse, and no swing or groove |
| MultiComp | No upward compression, which is OTT's whole sound, no lookahead and no mid/side. It's also costlier on CPU: 2.2-2.4 % for 4 bands in CI, over its own target |
| FIR | No adaptive noise tracking or machine learning, which RX and Clarity Vx have. No resonance-suppression mode like soothe. Its CPU varied a lot between CI runs |
| All | Windows VST3 only: no macOS, AU, CLAP or AAX. No in-plugin undo, A/B compare or level-matched bypass. Few presets, unsigned binaries, and no IR library or sample content |

## Would people choose ours over the best?

**You, for your own use: yes, in most cases.** The suite was built around the ReaPlugs workflow you wanted, with known flaws fixed and typed values everywhere. It's free, and it works in REAPER. FIR and MultiComp are the strongest against their originals.

**A wider audience: mostly no, today.**

- **What they choose on:** sound character, workflow, presets, reputation and platform. On character and depth of features, the leaders are ahead in every category.
- **Platform:** with no macOS, AU or CLAP, most of the market can't use the suite at all.
- **Where we can compete:** as free tools. Against TDR Nova, Kotelnikov, OTT and the original ReaPlugs, the suite's polish, consistency, typed values and tested accuracy would stand out, and FIR has no free equal I could find.
- **Where we're closest to the paid leaders:** FIR's real-time noise reduction, and MultiComp's flat, phase-safe crossovers. Neither would win on its own against RX or Pro-MB.

## Phase 2.1 results

Phase 2.1 closed the gaps the report found in §5, §8, §9 and §10, with numbers from CI (`docs/PROGRESS.md` has every test; CPU by the brief's §0.4 method, as a median of five runs and a ratio in the same run):

| Report section | Gap | Phase 2.1 | Measured |
|---|---|---|---|
| §10, across the suite | No undo, A/B or level-matched bypass | **Closed** (W): 100-step undo and redo, A/B with copy, level-matched Compare, in all nine plugins | 50 random edits undo and redo bit-exactly; Compare within 0.5 LU on a +6 dB EQ boost; no click switching A/B; nothing added with the window closed |
| §5, EQ | No dynamic EQ | **Closed** (D): every bell, shelf, tilt and notch can be dynamic, Above or Below, with a sidechain | Within 0.2 dB of Range, no movement from a tone two octaves away, no zipper noise; 8 dynamic bands 0.98 % (target 1.5 %) |
| §8, MultiComp | No upward compression, OTT's whole sound | **Closed** (U): upward compression per band with Max lift and Floor, global Depth and Time, a "Big squash" preset | The static curve within 0.3 dB of the formula; Depth 0 % is the plain band sum; upward adds about 1 % CPU on 4 bands |
| §9, FIR | No adaptive noise tracking | **Closed** (A): Learned, Adaptive or Both, Martin's minimum statistics with tone holding | Follows a 10 dB rise in 2 s ± 25 % at every FFT size; a sustained sine loses under 0.5 dB; 1.13 × Learned's CPU |
| §10 | Host compatibility (not in the report) | Offline rendering checked against playback, keys passed to the host, sidechain and latency handling, Steinberg's validator and a host harness in CI | Every plugin passes the validator's 47 tests and the harness's checks |

Still open, as before: EQ mid/side, EQ Match and Natural Phase; MultiComp lookahead and mid/side (its CPU, about 2.3 % for 4 bands, is unchanged); FIR's machine-learning and resonance suppression; the Kicker, Exciter, Comp, Verb and Delay gaps; platforms other than Windows VST3. Phase 3 picks these up.

## Head to head (Phase 3 Q1)

On 9 November 2026 the user ran FancyBench on their PC against the free leaders. The details are in `bench-2026-11-09.md`.

| Category | Competitor | CPU, ours against theirs | Notes |
|---|---|---|---|
| Compressor | TDR Kotelnikov | 0.79 % against 3.56 % | Fancy Comp distorts less while compressing in three of four cases; Kotelnikov's detector ignores bass by design |
| EQ | TDR Nova | 0.49 % against 1.11 % | Identical bells; Nova clicks on sudden gain jumps (38 bursts), Fancy EQ doesn't; Nova has auto gain on by default |
| Delay | Valhalla Freq Echo, Deelay | 0.49 % against 0.33 % and 0.65 % | Fancy Delay and Deelay are bit-clean; Valhalla adds slight analogue-style colour |

- **CPU, which the report couldn't rank:** in these categories ours is as cheap as the free leader or cheaper.
- **All report their latency exactly.** Ours add none; Kotelnikov and Nova add 3.8 ms each.
- **Still open:** the paid leaders, and MultiComp, Verb and FIR.

## What Phase 3.0 should do

The prompt is in `docs/BUILD_PROMPT_PHASE3.md`. In order:

1. **A benchmark host** that runs on your PC, so you can measure our plugins against any VST3 you own or trial: CPU, latency, frequency response, THD, aliasing and null tests.
2. **Shared features:** A/B compare, undo, level-matched bypass and a better preset browser.
3. **The biggest gaps first:**
   - Kicker: sub synthesis and harmonic modes.
   - Delay: modulation and character.
   - EQ: dynamic EQ and mid/side.
   - Comp: saturation and upward compression.
   - MultiComp: upward compression, lookahead and CPU.
4. **Verb's content:** a library of original impulse responses made with its own generators, true stereo and modulation.
5. **Optional:** CLAP and macOS builds. This only matters if you want the plugins to reach other people, and the original brief excluded it, so it's your decision.
