# Fancy Plugs against the market leaders: detailed report

November 2026. The short version is `2026-11-market-report-summary.md`.

## 1. Method and limits

**Our numbers** come from the CI benchmark and tests, recorded in `docs/measurements/`:

- They're measured on GitHub's `windows-latest` runners at 48 kHz with 128-sample blocks, stereo.
- They vary between runs by up to about three times. The Delay's 4 plain taps measured 0.37-1.08 % on the same code, and FIR measured 0.5-3.8 % across modes. So each CPU figure is a range.

**Competitor information** comes from three kinds of source, in order of trust:

1. Vendor manuals and product pages: FabFilter, iZotope, Soundtoys and Waves.
2. Independent reviews: MusicTech, Sound on Sound, Production Expert and similar.
3. Forum reports, and "best of" roundups, most of which are affiliate pages.

The roundups are used only to identify the leaders, never as evidence of quality.

**What couldn't be done:**

- **No head-to-head runs.** The competitors are commercial and licensed, so they can't be installed on the CI runners or run here. No measurement in this report runs a competitor beside one of ours.
- **No comparable CPU figures.** Nobody publishes CPU measured under the same conditions as ours. FabFilter staff only say that Pro-C 2 is "certainly not a CPU hog" [26]. The only Pro-Q figure found was one user's report of about 0.09 % per Pro-Q 2 instance, on unknown hardware [27]. So CPU isn't ranked anywhere below.
- **No sound-quality judgement.** Listening tests need ears and the competitors' audio, so this report compares features, documented behaviour and measured accuracy, not taste.

Phase 3 proposes a benchmark host to fix the first two limits on your own PC. See section 11.

## 2. Fancy Kicker: sub enhancer

**What ours is:** a band-pass boost of the existing signal around Freq (20-240 Hz), mixed in parallel. It has a 15 Hz high-pass on the wet path, optional mono below Freq, and an optional soft clip. It's zero latency, and measured 0.07-0.15 % CPU.

**The leaders:**

- **Waves MaxxBass:** psychoacoustic. It generates harmonics about an octave above the bass, so the bass is perceived on small speakers without adding low-frequency energy [21][22].
- **Waves LoAir:** subharmonic synthesis. It creates new content an octave below the input by lowering and filtering it [21].
- **Baby Audio SubCulture**, July 2026: a pitch-tracking subharmonic generator with three adaptive engines. It's best on monophonic sources, and costs $129 [23].
- **ujam Subcraft:** a sub generator with automatic sidechain ducking against the kick [24].
- **Kick synthesisers** such as Sonic Academy Kick 3 are a different category: they make the kick rather than enhance it [25].

| | Fancy Kicker | MaxxBass | LoAir and SubCulture |
|---|---|---|---|
| Adds bass that wasn't there | No | No, it's perceived only | Yes |
| Helps small speakers | No | Yes, its main purpose | No |
| Pitch tracking | No | | SubCulture yes |
| Latency | 0 | | |
| Mono low end | Yes | | |
| Click-free automation | Yes, tested | Not documented | Not documented |

**Where ours is ahead:** it's clean, precise and predictable. Automation is tested click-free, a fast Freq sweep stays bounded, and Wet and Dry at their extremes are bit-exact. It's ideal for a controlled boost of a kick's fundamental.

**Gaps:** it can't create bass that isn't already in the source, and it doesn't help small speakers. Those are the two main reasons people buy this category.

## 3. Fancy Exciter

**What ours is:**

- A parallel high-frequency exciter: a high-pass above Freq, then Clip Boost, then a cubic soft clipper with anti-derivative anti-aliasing, at 1x to 8x oversampling.
- Character sets the blend of even and odd harmonics.
- At the default 4x, the worst alias is -114.4 dBFS. With oversampling off it's -20.5 dBFS.
- CPU is 0.09 % with oversampling off and 1.37 % at 4x.

**The leaders:**

- **Ozone Exciter:** a multiband harmonic processor for mixing and mastering [16].
- **Aphex Aural Exciter**, modelled by Waves and UAD: praised for texture and warmth, with few modern controls [17].
- **Process.Audio Sugar:** a four-band parallel enhancer [18].
- **Waves Vitamin:** five bands, each with harmonics and width [17].
- **Others named:** Sonnox Oxford Inflator, Brainworx bx_saturator V2, oeksound Bloom and Arturia Bus Exciter-104.

**Where ours is ahead:**

- At 4x the aliasing is very low, and it's measured.
- Harmonics at 0 % is bit-exact.
- It's very light on CPU without oversampling.
- The latency is reported and compensated in every mode.

**Gaps:**

- **One band.** The leaders are mostly multiband.
- **One shaper.** There's only even-to-odd control, with no tube, tape or transformer types, no width or mid/side control, and no auto-gain.
- **Oversampling off.** It's a usable choice, but aliases audibly on hot material. The manual or UI could warn about that.

## 4. Fancy Comp

**What ours is:**

- A feed-forward compressor, with optional feedback detection.
- Detector filters, RMS size, a Weird knee, Classic or Auto ballistics, and pre-comp lookahead up to 250 ms.
- Oversampling to 4x, a stereo sidechain, a true-peak limiter, and Wet and Dry.
- CPU is 0.41-0.72 % with oversampling off. Latency is 0, or 104 samples with the limiter. At 4x the worst alias is -100.9 dBFS.

**The leaders:**

- **FabFilter Pro-C 3**, January 2026, $199 [1][2]:
  - 14 compression styles, including Vari-Mu, El-Op, Upward and the multiband TTM.
  - Character saturation: Tube, Diode or Bright, before or after compression.
  - Oversampling to 32x, and lookahead up to 20 ms.
  - A 6-band sidechain EQ with mid/side, and Auto Threshold.
  - A hold control up to 500 ms, and Dolby Atmos support.
- **FabFilter Pro-C 2:** still the "best overall" in May 2026 roundups [3][4].
- **TDR Kotelnikov:** the usual free pick [3].
- **Others:** Waves CLA-2A for vocals and UAD SSL G-Bus for buses [3].

| | Fancy Comp | Pro-C 3 |
|---|---|---|
| Styles | 1, plus Classic and Auto ballistics | 14 |
| Saturation | No | 3 types, pre or post |
| Upward compression | No | Yes |
| Oversampling | 4x | 32x |
| Lookahead | Pre-comp to 250 ms | Up to 20 ms |
| Sidechain filtering | High-pass and low-pass | 6-band EQ with mid/side |
| Auto threshold | No | Yes |
| Limiter on output | True-peak, tested | Not in the compressor |
| Formats | VST3, Windows | VST, VST3, AU, AAX, CLAP, and iPad [5] |

**Where ours is ahead:**

- The limiter is tested at 200 dB over threshold.
- The aliasing figures are measured and published.
- Mid and Side detector modes.
- The ballistics are documented.

**Gaps:**

- The leaders' most popular features: character saturation, styles, upward compression and auto threshold.
- The sidechain EQ is only a high-pass and low-pass.

## 5. Fancy EQ

**What ours is:**

- 16 bands, using matched-z filters that don't cramp. The worst error against analog is 0.42 dB for an 18 kHz bell at 44.1 kHz.
- Linear phase from 2048 to 16384 points, with latency from 24 to 192 ms.
- Oversampling to 4x, and a calibrated analyzer: a -12 dBFS sine reads -12.09 dB.
- CPU is 0.16-0.46 % for 8 bands.

**The leader:** FabFilter Pro-Q 4 [6][7][8].

- Up to 24 bands, with dynamic EQ and spectral dynamics, which act only on the frequencies in a band that cross the threshold.
- Zero Latency, Natural Phase and Linear Phase modes.
- EQ Match, Character modes, an instance list, and Atmos support.

Pro-Q 4 has its costs:

- **Linear-phase latency** is about 116 ms at Medium, 209 ms at High and 1509 ms at Maximum, at 44.1 kHz.
- **Spectral bands** always run in linear phase, adding about 100 ms even in Zero Latency mode. In some hosts, that delays the whole project [8][9].

TDR Nova is the usual free pick, as a free dynamic EQ [7].

| | Fancy EQ | Pro-Q 4 |
|---|---|---|
| Bands | 16 | 24 |
| Dynamic EQ | No | Yes |
| Spectral dynamics | No | Yes, with about 100 ms of latency |
| Mid/side or L/R per band | No | Yes |
| Match EQ | No | Yes |
| Analog-like phase | Matched-z magnitude, minimum phase | Natural Phase |
| Linear-phase latency | 24-192 ms | About 116-1509 ms |
| Cramping near Nyquist | Fixed, and tested | Fixed |

**Where ours is ahead:**

- Shorter linear-phase latency choices.
- Tested accuracy at every sample rate.
- A calibrated analyzer.
- Low CPU.
- Typed values, including the note name of each frequency.

**Gaps:**

- Dynamic EQ, the most requested modern feature, which TDR Nova gives away free.
- Mid/side per band.
- EQ Match.
- 24 bands.

## 6. Fancy Verb: convolution

**What ours is:**

- A non-uniform partitioned convolver, with zero latency or a Low CPU mode at 4096 samples. A 4 s stereo IR used 1.66 % CPU.
- A modular IR-building chain: File, Echo generator, Reverb generator, Lp/Hp, Trim, Normalize and Reverse.
- Pre-delay, centre and width, with an embedded copy of the IR for moved projects.

**The leaders:**

- **Audio Ease Altiverb 8:** the industry standard. Its library of IRs comes from real spaces, from concert halls to aircraft cockpits [10][11].
- **LiquidSonics Reverberate 3:**
  - Modulated true-stereo convolution, with split modulation and the Fusion-IR format for capturing algorithmic reverbs [12][13].
  - About $99, and called "the most advanced convolution reverb in 2026" by one guide [11].
- **Others:** Seventh Heaven, built on Fusion-IR, the free MConvolution, and Kilohearts Convolver [10].
- **Algorithmic reverbs** such as Valhalla and Pro-R 2 dominate the general "best reverb" lists. Convolution is now a niche within reverb.

| | Fancy Verb | Altiverb 8 | Reverberate 3 |
|---|---|---|---|
| IR library | None, generators only | Large, real spaces | Yes |
| True stereo, 4 channels | No | Yes | Yes |
| Modulation | No | | Yes |
| IR editing | Modular chain | Yes | Yes |
| Zero latency | Yes, tested | | |

**Where ours is ahead:**

- The modular IR-building chain is unusual. Building a reverb from generators, filters and trims is closer to a sound-design tool than most convolution reverbs offer.
- Zero latency at low CPU, with the engine tested against direct convolution.

**Gaps:**

- **No content.** A convolution reverb is mostly bought for its library.
- No true stereo and no modulation.
- No algorithmic engine.

## 7. Fancy Delay

**What ours is:**

- 16 taps, each with:
  - feedback, which can reach 110 % with Saturate on;
  - low-pass and high-pass filters inside the feedback loop by default;
  - bit crush, width, pan, ping-pong and volume.
- Note values with straight, dotted and triplet feel, plus a multiplier, following the host tempo.
- A smooth time-change mode, or tape-style glide, and a draggable tap map.
- **Measured:** with a 2 kHz low-pass and 60 % feedback, the energy above 2 kHz falls from 21.4 % to 0.5 % over five repeats.
- **CPU** is 0.37-1.08 % for 4 plain taps, and 1.67-3.11 % with every option on.

**The leaders** [14][15][19][20]:

- **Soundtoys EchoBoy**, about $199:
  - About 30 styles modelled on classic units: tape, Space Echo, Memory Man and others.
  - Saturation, tape flutter and diffusion.
  - A Rhythm mode with up to 16 taps, each with volume and EQ.
  - Groove, Feel and Accent controls, and tap tempo.
- **FabFilter Timeless 3:** a modular tape delay with deep modulation and routing.
- **Valhalla Delay**, about $50:
  - Seven modes: Tape, HiFi, BBD, Digital, Ghost, Pitch and RevPitch.
  - Five styles: Single, Dual, Ratio, PingPong and Quad.
  - Diffusion, modulation, and delays up to 20 s.
  - Valhalla Delay has no dynamics control, so no ducking [14].

| | Fancy Delay | EchoBoy | Valhalla Delay |
|---|---|---|---|
| Taps | 16, each fully independent | 16 in Rhythm mode, volume and EQ each | Up to 4 in Quad |
| Character models | Saturate (tanh) and bit crush | About 30 styles | 7 modes |
| Modulation, wow and flutter | No | Yes | Yes |
| Diffusion | No | Yes | Yes |
| Groove and swing | No | Yes | No |
| Pitch or reverse | No | No | Pitch and RevPitch |
| Filters inside the loop | Yes, tested to darken each repeat | | |
| Note values | 1/1-1/64 with dotted, triplet and ×1-16 | Yes, plus tap | Yes, dotted and triplet |
| Maximum time | 10 s | | 20 s |

**Where ours is ahead:**

- Independent control of each of 16 taps.
- The visual tap map.
- Repeats that are proven to darken each pass.
- Zero latency, and click-free time changes, both tested.

**Gaps:** character. The leaders sell their sound: tape, bucket-brigade and modulation. Ours is clean digital with a saturator. There's also no diffusion, groove, ducking or modulation.

## 8. Fancy MultiComp

**What ours is:**

- Up to 8 bands, with LR 24, LR 48 or linear-phase crossovers. The bands sum flat to 0.019-0.029 dB, and Linear nulls below -90 dB.
- Per band: Fancy Comp's compressor with expansion down to 0.5:1, a sidechain, Link bands, a limiter and a spectrum.
- The Dry signal is the split bands summed before compression, so it stays in phase.
- CPU is 2.17-2.42 % for 4 LR bands.

**The leaders:**

- **FabFilter Pro-MB:** the top pick in several guides [28][29]. Its Dynamic Phase mode has a flat phase response when no band changes gain, with no latency, and minor phase effects appear only when a band acts. It also has Linear Phase, and Minimum Phase with static phase at the crossovers. Lookahead adds 20 ms [30].
- **Xfer OTT:** free. Three bands with linked upward and downward compression, and Depth and Time controls. It's built into Ableton Live [31][32].
- **Others:** Ozone's Dynamics, Waves C6 with four bands and two floating EQ bands, Devious Machines Multiband X6, and Leapwing DynOne [28].

| | Fancy MultiComp | Pro-MB | OTT |
|---|---|---|---|
| Bands | 1-8 | Up to 6 | 3 |
| Upward compression | Only expansion below 1:1 | Yes | Yes, its whole sound |
| Phase when idle | LR all-pass, magnitude flat | Flat, in Dynamic Phase | |
| Linear phase | Yes, 53 ms | Yes | No |
| Lookahead | No | 20 ms | No |
| Band sidechain | Yes, per band | Yes | No |
| Parallel Dry in phase | Yes, by design | | |

**Where ours is ahead:**

- 8 bands.
- A per-band sidechain that uses the same crossovers.
- Measured flatness in all three modes.
- Phase-safe parallel compression.
- Link bands that ignore automation and preset loads.

**Gaps:**

- Upward compression in the OTT style, which is why many producers reach for a multiband at all.
- Lookahead.
- Mid/side.
- CPU above its own target.
- In the LR modes, the crossover phase shift is always there, where Pro-MB's Dynamic Phase avoids it until a band acts.

## 9. Fancy FIR: spectral

**What ours is:**

- A short-time Fourier transform engine, with five modes: EQ, Gate, Compressor, Subtract and Convolve L/R.
- FFT sizes from 128 to 32768, Low latency at N/2, and the latency reported exactly.
- **Subtract:** residual variance of 37.4 dB² against 98.4 dB² for plain subtraction, and the sine changed by -0.03 dB.
- **Release** is independent of FFT size: 1963 ms against 2000 ms set.
- **Sidechain** isolation: -28.5 dB around 1 kHz, and 0.00 dB elsewhere.

**The leaders:**

- **iZotope RX**, RX 11 Advanced at about $1,199 [33]:
  - Spectral De-noise has algorithms of increasing quality and latency, an Adaptive mode, and a zero-latency Voice De-noise.
  - The manual names musical noise as the main artifact, and asks you to re-learn after changing FFT size [34].
- **Waves Clarity Vx Pro:** machine-learning vocal cleaning, called the fastest real-time option [35].
- **Others named:** Cedar DNS One for broadcast, and Sonnox Restore [35].
- **oeksound soothe2 and soothe3:** resonance suppression, a different job from noise reduction [36]. One user measured soothe2 at 2048 samples of latency, and soothe3 adds a zero-latency mode [37].
- **Wavesfactory Trackspacer:** sidechain spectral ducking. Users call it coarser than soothe, at about 32 bands, but lighter on CPU [37].
- **Pro-Q 4's spectral dynamics** overlap with FIR's Gate and Compressor [8].

| | Fancy FIR | RX Spectral De-noise | soothe3 | Trackspacer |
|---|---|---|---|---|
| Learned noise profile | Yes. It stops by itself and keeps across FFT sizes | Yes, re-learn after a size change | | |
| Adaptive or ML denoise | No | Yes | | |
| Spectral gate and compressor | Yes | | Resonance only | |
| Sidechain spectral ducking | Yes | | Yes | Yes |
| Drawable curve | Yes | | | |
| Latency | N or N/2, exact | Depends on the algorithm | 0 to about 2048 | |
| Musical-noise control | Wiener mask, smoothed. Measured | Several algorithms | | |

**Where ours is ahead:**

- One plugin covers spectral EQ, gate, compressor, sidechain ducking and learned noise reduction in real time.
- Every latency is exact.
- Measured musical-noise performance.
- A profile that survives FFT-size changes.
- Free. There's no free tool I found with this combination.

**Gaps:**

- Adaptive noise tracking, for noise that changes.
- Machine-learning voice denoise, the 2026 standard for dialogue.
- A resonance-suppression mode like soothe's.
- Offline-only quality modes, of the kind RX's higher algorithms use.
- CPU stability, which was erratic across CI runs.

## 10. Across the suite

**Platform.**

- **Ours:** Windows VST3 only. The brief excluded macOS, AU, CLAP and AAX.
- **The leaders:** usually every format. FabFilter lists VST, VST3, AU, CLAP and AAX, plus iPad apps [5].

For anyone but you, this alone removes most of the market.

**Workflow features the leaders share:**

- In-plugin undo and redo.
- A/B comparison.
- Level-matched or auto-gain bypass.
- Preset browsers with categories and search.
- Resizable windows. Ours scale by menu, and only the EQ resizes freely.
- An instance list, as in Pro-Q 4 and Pro-C 3.

**What people choose on.** No real survey of plugin purchasing was found. Commentary consistently names sound and character first, then workflow, price, peer recommendation and trials [38][39]. Stock plugins win on efficiency and on session compatibility over the years [39].

**Reputation.** A Production Expert poll found Pro-Q 3 "far and away the most popular" FabFilter plugin [40]. Brand trust is a large part of why the leaders lead.

## 11. Would people choose ours?

**You, in REAPER on Windows: yes, in most cases.** The suite fixes the documented weaknesses of the ReaPlugs you started from, such as ReaEQ's cramping, ReaXcomp's phasing and ReaFIR's fixed attack and release. It adds typed values and a consistent modern interface, and it costs nothing. The exceptions are where you need character or content the suite doesn't have:

- a modelled tape echo, or a vintage compressor style;
- a library of real rooms.

**Other producers, as a free suite: plausibly, against other free tools.**

- **Against TDR Nova, Kotelnikov, OTT and the original ReaPlugs**, the suite's consistency, typed values and tested accuracy stand out.
- **FIR** covers ground no free tool covers in one plugin.
- **But the free leaders each have a signature:** Nova's dynamic EQ and OTT's upward compression. We don't have those yet.

**Against the paid leaders: not today.** On the things buyers weigh most, the leaders are ahead in every category: character, depth of features, content, platform and reputation. Our engineering accuracy is a real strength, but it isn't what most buyers choose on. The two closest are:

- **FIR's real-time noise reduction**, against RX's real-time modules.
- **MultiComp's phase-safe flat crossovers**, against Pro-MB.

Neither would displace the leader, but both could earn a place beside it.

**Unknowns that matter:** CPU on real hardware, and how the plugins sound side by side with the leaders. Both need the Phase 3 benchmark host and your ears.

## Sources

1. MusicTech, Pro-C 3 review: https://musictech.com/reviews/plug-ins/fabfilter-pro-c-3-review/
2. FabFilter, Pro-C 3 product page and help: https://www.fabfilter.com/products/pro-c-3-compressor-plug-in, https://www.fabfilter.com/help/pro-c/using/styleandcharacter, https://www.fabfilter.com/news/1768435200/fabfilter-releases-pro-c-3-compressor-plug-in
3. Output, best compressor plugins 2026: https://output.com/blog/best-compression-plugins
4. Dubspot, best compressor plugins 2026: https://blog.dubspot.com/plugins/best/compressor
5. FabFilter home page, formats: https://prod.fabfilter.com/
6. Musehub, best EQ plugins 2026: https://www-cf.musehub.com/learn/best-eq-plugins
7. Samplefocus, best EQ plugins 2026: https://samplefocus.com/blog/best-eq-plugins-of-2026
8. FabFilter, Pro-Q 4 processing modes and manual: https://www.fabfilter.com/help/pro-q/using/processingmode, https://www.fabfilter.com/downloads/pdf/help/ffproq4-manual.pdf
9. FabFilter forum, Pro-Q 4 spectral latency: https://prod.fabfilter.com/forum/topic/7945/q4-spectral-mode-delay, https://prod.fabfilter.com/forum/topic/7940/project-wide-latency-caused-by-pro-q-4
10. Output, convolution reverb 101: https://output.com/blog/convolution-reverb-101-7-best-plugins-and-how-to-use-them
11. LANDR, best reverb plugins 2026: https://blog.landr.com/best-reverb-plugins, and MixingGPT: https://mixinggpt.com/blog/best-reverb-plugins-2026
12. Production Expert, Reverberate 3: https://www.production-expert.com/production-expert-1/why-liquidsonics-reverberate-3-isnt-just-another-convolution-reverb
13. Pro Tools Expert, Reverberate 3 announcement: https://www.pro-tools-expert.com/production-expert-1/2020/10/12/liquidsonics-reverberate-3-new-modulated-true-stereo-convolution-reverb
14. Musehub, best delay plugins 2026: https://www-cf.musehub.com/learn/best-delay-plugins
15. Dubspot, EchoBoy vs Timeless 3: https://blog.dubspot.com/echoboy-vs-timeless-3
16. Puremix, best harmonic enhancer 2026: https://www.puremix.com/blog/best-harmonic-enhancer-plugin-in-2026
17. Nail The Mix, best exciter: https://www.nailthemix.com/best-exciter-plugin
18. Integraudio, best exciter plugins: https://integraudio.com/best-exciter-plugins/
19. Sound on Sound, EchoBoy review: https://www.soundonsound.com/reviews/sound-toys-echoboy
20. Sound on Sound, Valhalla Delay review: https://www.soundonsound.com/reviews/valhalla-delay, and KVR: https://kvraudio.com/news/valhalla-dsp-releases-valhalladelay-44983
21. Waves, bass plugins compared: https://waves.com/bass-plugins-and-sub-enhancers-compared
22. Sound on Sound, MaxxBass: https://www.soundonsound.com/node/4923542
23. Dubspot, Baby Audio SubCulture: https://blog.dubspot.com/plugins/baby-audio-subculture
24. MusicRadar, bass enhancement plugins: https://www.musicradar.com/news/tech/10-of-the-best-vst-au-bass-enhancement-plugins-639124
25. MusicTech, best kick plugins 2026: https://musictech.com/guides/buyers-guide/best-kick-drum-plugins-2026-tested-reviewed
26. KVR forum, FabFilter CPU: https://www.kvraudio.com/forum/viewtopic.php?p=8911224
27. Gearspace, most CPU-efficient plugins: https://gearspace.com/threads/most-cpu-efficient-plugins.1100469/
28. MixingGPT, best multiband compressors 2026: https://mixinggpt.com/blog/best-multiband-compressor-plugins-2026
29. Music Industry How To, multiband compressors 2026: https://www.musicindustryhowto.com/multiband-compressor-vst-plugins/
30. FabFilter, Pro-MB processing mode: https://www.fabfilter.com/help/pro-mb/using/processingmode
31. KVR, OTT release: https://www.kvraudio.com/news/xfer-records-releases-ott-multiband-compressor-for-windows-and-os-x-vst-and-au-19972
32. Unison, OTT guide: https://unison.audio/ott-plugin/
33. Music Production Wiki, noise reduction plugins: https://musicproductionwiki.com/articles/best-noise-reduction-plugins
34. iZotope, RX Spectral De-noise manuals: https://s3.amazonaws.com/izotopedownloads/docs/rx9/en/spectral-de-noise/index.html, https://s3.amazonaws.com/izotopedownloads/docs/rx700/en/spectral-de-noise/index.html
35. Production Expert, no best de-noise plugin: https://www.production-expert.com/production-expert-1/why-theres-no-such-thing-as-a-best-de-noise-plug-in
36. Rysup Audio, resonance suppression 2026: https://rysupaudio.com/blogs/news/best-resonance-suppression-plugins-2026-soothe-2-vs-smooth-operator-pro-vs-silk-vocal-vs-rysupsmooth
37. Dubspot, soothe3 review: https://blog.dubspot.com/oeksound-soothe3-2026, and KVR, Trackspacer thread: https://www.kvraudio.com/forum/viewtopic.php?p=8973319
38. Data Calculus, selecting plugins: https://datacalculus.com/en/blog/musicians/music-producer/selecting-and-using-audio-plugins-for-music-producers
39. Production Expert, why professionals use stock plugins: https://pro-tools-expert.squarespace.com/production-expert-1/why-professionals-use-stock-plugins
40. Production Expert, favourite FabFilter plugin poll: https://www.production-expert.com/production-expert-1/favourite-fabfilter-audio-plugin-poll-the-results
41. ReaPlugs: https://www.reaper.fm/reaplugs/, and KVR, ReaFIR: https://www.kvraudio.com/product/reafir_by_cockos
