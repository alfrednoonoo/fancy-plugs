# Fancy Transient and Fancy Chorus: feature survey and proposed designs

Phase 4 items 3 and 4 (`docs/enhancements.md`). Researched on 22 November 2026 from vendor pages,
manuals and 2026 round-ups (sources at the end). **Layouts and decisions 1 to 5 approved by the user on 22 November 2026, all A.** The mockups are
`reference/phase4/50__transient_mockup.png` and `51__chorus_mockup.png` (source
`design/phase4-mockup-src/transient.html`, `chorus.html`, rendered by `new_plugins.py`). Each plugin's
design note (`docs/transient.md`, `docs/chorus.md`: DSP, parameter ranges and skews, latency, state,
Python model, tests) follows the layout's approval, as Phase 3's new plugins did. Ideas only: no names,
graphics or sounds are taken from any product.

## 1. Transient shapers

### Every feature found in the category

| # | Feature | Where it's found | Fancy Transient |
|---|---|---|---|
| 1 | Attack, boost or cut (bipolar) | all: SPL Transient Designer, Kilohearts, NI Transient Master, Neutron, TransMod | **Main**, ATTACK card |
| 2 | Sustain, boost or cut (bipolar) | all | **Main**, SUSTAIN card |
| 3 | Attack speed / how fast it engages | Kilohearts Speed, SPL fast/slow, Smack Attack duration | **Main**, Speed |
| 4 | Sustain length / release stage | Trinity Shaper's release stage, Smack Attack duration | **Main**, Length |
| 5 | Pump: a gain dip after each hit | Kilohearts | **Main**, Pump |
| 6 | Release contour: sharp, medium, smooth (and global precise/balanced/loose) | Neutron | **Main**, Contour switch |
| 7 | Detection sensitivity or threshold, level-independent detection | TransMod Threshold, Smack Attack sensitivity, SPL (level independent) | **Main**, Sensitivity |
| 8 | Dead band: ignore tiny changes | TransMod | Drawer |
| 9 | Detector filter (sidechain EQ) | SPL Plus | **Main** Highpass, drawer Lowpass |
| 10 | External sidechain detection | Kilohearts | **Main**, Detector menu (Main, Sidechain, Mid, Side) |
| 11 | Soft clip on the output | Kilohearts, XLN DS-10 | **Footer**, Clip output (+ Oversampling in the drawer) |
| 12 | Mix / parallel shaping | Neutron Mix, SPL Plus | Wet and Dry faders |
| 13 | Output gain | all | Drawer, Output |
| 14 | Lookahead or zero latency | Nanopulse (zero-latency option) | Drawer, Off by default (zero latency) |
| 15 | Stereo link, Mid/Side | various | Footer Link channels; drawer Channels |
| 16 | Listen to the change (delta) | Smack Attack-style listen | Footer, temporary (§0.3) |
| 17 | Gain-change display over the waveform | Smack Attack, Neutron | **Main** display |
| 18 | Multiband (2 to 4 bands, each with its own Attack/Sustain) | Softube (2 bands), Punctuate, Neutron, Transify, Nanopulse | **Later** (decision 3) |
| 19 | Attack shape curves | Smack Attack | Later: Contour covers the release side |
| 20 | Band-specific "mojo"/character | XLN DS-10 | Not planned: Sature and Exciter do colour |

### Proposed design (mockup 50)

- **Accent orange `#E77E23`** (user, 22 November 2026: at least one new plugin orange). Shared with Sature.
- **Size:** Phase 2 class, 760 × 465 with the title strip. Right column Out, Wet, Dry (104 px).
- **Top row:** Contour (Sharp, Medium, Smooth), and the Detector input menu.
- **Display card:** the input (grey), the output (orange) and the gain applied (white, 0 dB dashed), scrolling, with each detected hit marked.
- **Cards:** ATTACK (Attack, Speed), SUSTAIN (Sustain, Length, Pump), DETECT (Sensitivity, Highpass).
- **Footer:** Clip output, Link channels, Listen to change, EDIT.
- **Drawer:** Lookahead (Off, 1, 2, 5 ms), Oversampling, Channels (Stereo, Mid only, Side only), detector Lowpass, Dead band, Output.
- **Engine (for the design note):** two envelope followers on the detector, fast and slow; Attack's gain follows fast − slow while it's positive, Sustain's follows slow − fast after the peak, both scaled to dB and independent of level (so Sensitivity sets only which hits count). Contour sets the slow follower's release curve. No latency unless Lookahead is on.

## 2. Choruses

### Every feature found in the category

| # | Feature | Where it's found | Fancy Chorus |
|---|---|---|---|
| 1 | Rate, Depth, Delay, Mix | all | **Main**; Mix is the Wet and Dry faders |
| 2 | Feedback (and its polarity) | NI Choral, flanger modes | **Main** Feedback (bipolar), drawer polarity |
| 3 | Voices (1 to 3, up to 8 or 32) | NI Choral (1 to 3), Kilohearts Taps, Caelum Choric (32) | **Main**, 1 to 6 |
| 4 | Stereo width / spread of the voices | NI Choral Width, Kilohearts Spread | **Main**, Spread (LFO phases) and Width (pan) |
| 5 | Vintage BBD models: two-mode chorus with I+II | Arturia JUN-6, TAL-Chorus-LX, XILS Chor'X | Mode **Classic** |
| 6 | Dimension-style subtle stereo widening, mono/stereo modes | Arturia DIMENSION-D | Mode **Dimension**; drawer Input (Stereo/Mono) |
| 7 | String-ensemble triple-LFO chorus | Waves Ensemble, Kilohearts Ensemble, TriceraChorus | Mode **Ensemble** |
| 8 | Doubler / random detune | Tritik Echorus (detuning, random), Waves Doubler | Mode **Doubler** |
| 9 | Flanger (short delay with feedback) | multi-mode choruses, Choral | Mode **Flange** |
| 10 | Vibrato (wet only) | UA Brigade, JUN-6 I+II | Pull Dry to −inf (manual says so); no extra mode |
| 11 | Scatter / reverb-like diffusion | NI Choral Scatter | **Later** (decision 4) |
| 12 | LFO shape: sine, triangle, random | many | Drawer |
| 13 | Tempo sync | many | Footer, Tempo sync |
| 14 | Stereo LFO phase offset | Roland Phase (0 to 180°) | Drawer, LFO phase |
| 15 | Wet tone: low cut, high cut | many | **Main**, TONE card |
| 16 | BBD age: darkening, noise, saturation | TAL noise switch, JUN-6 | **Main**, Age |
| 17 | Keep bass mono | many "bass mono" options | Footer |
| 18 | Ducking the wet | some modulation plugins | Drawer |
| 19 | Visualising voices and LFOs | Choral, Ensemble | **Main** display |
| 20 | Up to 32 layered voices for unison textures | Caelum Choric | Not planned beyond 6 (CPU, and Doubler covers thickening) |

### Proposed design (mockup 51)

- **Accent: a new pink `#E0559A`** (decision 1). Every one of the seven accents is used; orange is Transient's.
- **Size:** Phase 2 class, 760 × 465. Right column Out, Wet, Dry.
- **Top row:** Mode (Classic, Ensemble, Dimension, Doubler, Flange). A mode sets each voice's LFO pattern, delay range and Age's flavour; the knobs keep working in every mode.
- **Display card:** each voice's delay over the last 2 s, and where each voice sits in the stereo field.
- **Cards:** MOTION (Rate, Depth, Delay, Feedback), VOICES (Voices, Spread, Width), TONE (Low cut, High cut, Age).
- **Footer:** Tempo sync, Keep bass mono, Listen to wet (temporary), EDIT.
- **Drawer:** LFO shape, Input (Stereo, Mono), Feedback polarity, Oversampling, LFO phase, Ducking, Output.
- **Engine (for the design note):** a modulated delay line per voice with cubic (or all-pass) interpolation, LFOs on a shared phase so presets recall; Age adds the BBD's band-limit, gentle saturation and a little noise. Zero latency.

## 3. Decisions for the user

1. **Accents.** **A (R):** Transient orange (shared with Sature), Chorus a new pink `#E0559A` added to `fancy::ui::colours`. **B:** Chorus orange and Transient another existing accent. **C:** both orange.
2. **Names.** **A (R):** Fancy Transient and Fancy Chorus, `com.fancyplugs.transient` and `.chorus`. **B:** other names.
3. **Transient multiband.** **A (R):** full-band now, with the detector filters; a two-band split as a later Wave item (it changes the main layout). **B:** two bands from the start, which doubles the cards.
4. **Chorus Scatter (diffused, reverb-like voices).** **A (R):** later. **B:** a sixth mode now.
5. **Chorus voices.** **A (R):** 1 to 6. **B:** 1 to 3, as most vintage units.

## 4. Vocal harmony (tenor, alto, soprano)

Asked by the user on 22 November 2026. Harmony voices are a **harmonizer**, not a chorus: they need pitch
detection on the sung note, a pitch shifter with formant preservation (so a voice moved up a fifth
doesn't sound like a cartoon), and a key and scale (or MIDI) to choose each voice's interval. A chorus
only delays and slightly detunes copies, so none of its engine carries over, and its CPU and latency
(a pitch detector needs about 20 to 40 ms) are of a different order. Proposed as its own plugin,
**Fancy Harmony**, after Fancy Chorus, with the same survey, mockup and design note: waiting for the user.

## Sources

- [The 7 Best Transient Shaper Plugins (2026)](https://artistsindsp.com/the-7-best-transient-shaper-plugins-for-drums-that-slap-2026/)
- [Best Transient Shaper Plugins (2026): Free, Stock, and Paid](https://borncreativeaudio.com/blog/best-transient-shaper-plugins)
- [8 Best Transient Shaper Plugins 2026](https://pluginoise.com/8-best-transient-shaper-plugins/)
- [Transient Shaper Plugins Guide](https://lordreverb.com/plugins/transient-shaper-plugins-guide/)
- [Kilohearts Transient Shaper](https://kilohearts.com/products/transient_shaper) and [MusicRadar on it](https://www.musicradar.com/news/kilohearts-transient-shaper-offers-direct-dynamics-control)
- [iZotope Neutron 4 Transient Shaper](https://s3.amazonaws.com/izotopedownloads/docs/neutron4/en/transient-shaper/index.html)
- [Sound On Sound: Oxford TransMod](https://www.soundonsound.com/node/4910537), [TransMod manual](https://www.hhb.co.uk/wp-content/uploads/user_manual_20141103152919_26568.pdf)
- [Best Chorus Plugins (2026)](https://modwheel.net/effects/best-chorus-plugins), [Best Chorus Plugins 2026](https://pluginreviewlab.com/best-chorus-plugins/)
- [Arturia Chorus JUN-6](https://www.arturia.com/products/software-effects/chorus-jun-6/overview), [Chorus DIMENSION-D](https://www.arturia.com/products/software-effects/chorus-dimension-d/overview)
- [Native Instruments CHORAL manual](https://native-instruments.com/ni-tech-manuals/mod-pack-manual/en/choral)
- [Kilohearts Chorus](https://kilohearts.com/products/chorus)
- [Roland SH-4d chorus parameters](https://static.roland.com/manuals/sh-4d_v200/en-US/8739085986311435.html)
