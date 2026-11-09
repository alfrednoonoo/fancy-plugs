# Phase 2.1, item A: adaptive noise tracking in Fancy FIR

Brief: §A of `docs/BUILD_PROMPT_PHASE2_1.md`. Mockup: panel 5 of `reference/phase2_1/14__phase2_1_controls.png`. FIR's own design is `docs/fir.md`.

## 1. What it does

Subtract mode's noise profile is learned once today. Adaptive mode keeps estimating the noise floor while audio plays, so it follows noise that changes (fans, room tone, traffic), without taking sustained tones, speech or music for noise. Learned stays the default, and works exactly as before.

## 2. New parameters

Appended after every existing parameter.

| ID | Name | Range | Default | Unit | Skew |
|---|---|---|---|---|---|
| `source` | Profile source | Learned, Adaptive, Both | Learned | | |
| `adaptSpeed` | Adapt speed | 0.5 to 10 | 2.0 | s | log |

Both are automatable. `stateVersion` stays at 3: D's migration gives a state without them Learned and 2 s, so v1.1.0 projects load and render bit-identically. The adaptive estimate itself isn't saved: it restarts from the learned profile, if there is one, or from the first frames of audio.

## 3. DSP: minimum statistics (Martin 2001)

The estimator is R. Martin, "Noise power spectral density estimation based on optimal smoothing and minimum statistics", IEEE Trans. Speech and Audio Processing 9 (5), 2001. It runs per bin on the power averaged over the channels, in the learned profile's units (power per bin divided by the analysis window's energy), once per frame, only while Source isn't Learned and the engine is the one heard.

Martin's constants are given for a 16 ms hop. Each is turned into a time constant and re-derived for the current hop, so the estimator behaves the same at every FFT size and with Low latency:

| Martin | Value at 16 ms | Here |
|---|---|---|
| α_max, the largest smoothing factor | 0.96 | exp(−hop / 392 ms) |
| α_min, the smallest | 0.30 | exp(−hop / 13.3 ms) |
| β_max, for the variance estimate | 0.80 | exp(−hop / 71.7 ms) |
| Global correction α_c | 0.7 / 0.3 blend, floor 0.7 | as Martin, once per frame |

**Per bin, per frame:**

1. **Optimal smoothing:** `α = α_c · α_max / (1 + (P/σ² − 1)²)`, held between α_min and α_max, and `P ← α P + (1 − α) |Y|²`.
2. **Bias:** from the first and second moments of P (smoothed with `β = min(α², β_max)`), `1/Q_eq = var(P) / (2 σ⁴)`, held to at most 0.5. `B_min = 1 + 2 (D − 1) / Q̃_eq` with `Q̃_eq = (Q_eq − 2 M(D)) / (1 − M(D))`, and `M(D)` interpolated from Martin's table (D = 1 to 160; above 160 it's held at 0.91). The sub-window gets its own B_min with V in place of D. The global factor `B_c = 1 + 2.12 √(mean 1/Q_eq)` covers the variance of the estimate. **Calibration:** with these as published, the first build's estimate read 3.9-4.5 dB low on white and pink noise at FFT 1024, 2048 and 8192. At 75 % overlap the variance estimate's short memory (β) under-reads the variance of the heavily smoothed power, so Q_eq comes out too high and B_min too small. A constant 4.2 dB is applied with B_c, measured in CI and checked by tests 2 and 4 at every size.
3. **Minimum search:** `D` frames in **U = 8 sub-windows of V frames**. A running minimum of `P · B_min · B_c` restarts every V frames, and its value goes into a ring of the last 8; `σ²` is the smallest of those 8 and the current sub-window's running minimum.
4. **Window from Adapt speed:** `D = Adapt speed × 8 / 9.5 / hop`, in frames, so the window is a number of seconds at every FFT size. `V = max(1, round(D / 8))`, recomputed at each sub-window boundary, so turning the knob never reallocates. A rise in noise is followed once the old minimum has left all 8 sub-windows: that takes the 8, the sub-window the change landed in, the analysis frame and the smoothing, about 9.5 sub-windows, which is Adapt speed (test 2's 2 s ± 25 %). With the whole of Adapt speed as the window, the third build followed in 2.05 s at FFT 1024 but 2.73 s at 8192, where the sub-windows and the frame are longest. A fall is followed at once.

**Holding (speech, music, tones):** a bin is held when its power, smoothed over 200 ms, stands more than 6 dB over the average of the bins 4 to 16 below it **and** over the average of the bins 4 to 16 above it. A tone, a voice's partials or a held chord stand out from the bins around them on both sides. Noise doesn't, even steeply sloped pink noise (one side is always as loud), and a real rise in the noise lifts every bin at once.

The first build compared each bin's Martin-smoothed power with its noise estimate, and with the median of that ratio. Martin's optimal smoothing stops smoothing whenever a frame deviates, so on plain noise that power spiked past 4× about 2 % of the time, and with the 300 ms hangover around 40 % of bins were held. A held bin can only fall, so the estimate read 3-4 dB low and missed the tracking times. A model of the estimator in Python (75 % overlap as correlated periodograms) reproduced the CI figure and showed the neighbourhood test holding 0 % of white noise, before this change was built. A held bin doesn't feed its minimum search, so its estimate stays where it was, and it keeps that for 300 ms after it last stood out. While more than 2 % of the bins below 8 kHz are held, the card shows "Speech/music held, not learned".

**Starting:** from the learned profile where there is one. Otherwise, from the first frame's power, each bin taken as the smaller of its own power and the median power within 16 bins either side. That way a tone or note already playing isn't taken for noise; the first build, starting from the plain first frame, learned a steady sine as noise (test 3).

**FFT size change:** the estimate is published, as the learned profile is, on the same fixed grid of 2049 frequencies. A new engine starts from it, so a size change keeps the estimate.

**Which profile the Wiener filter uses:**

| Source | Noise power per bin |
|---|---|
| Learned | The learned profile × Offset, exactly as today. The tracker doesn't run |
| Adaptive | The adaptive estimate × Offset |
| Both | The larger of the learned profile and the adaptive estimate, × Offset. With no learned profile, the adaptive estimate |

A sidechain still takes priority as the noise, as today. Reduction, Offset, Smoothing and Release apply in all three.

**Latency:** none added. **CPU:** a few dozen operations per bin per frame and one median per frame (`nth_element` on a buffer allocated with the engine). Test 6 holds it under 1.5 × Learned at FFT 2048.

## 4. Controls (panel 5)

- The Subtract card's title row gets a **Learned | Adaptive | Both** switch in the mode colour.
- **Learned:** the card as today (Learn button, progress and hint).
- **Adaptive:** the Learn button is hidden. The card shows an **Adapt speed** knob in the mode colour, and a status beside it: a green dot and "Tracking", "Floor ≈ −52 dB" (the estimate's average from 100 Hz to 8 kHz, in the graph's dB), and "Speech/music held, not learned" while bins are held.
- **Both:** the Learn button and the Adapt speed knob, side by side, and the status line under them.
- **Graph:** the adaptive estimate is drawn as a dashed line in the mode colour, above or beside the learned profile, which stays solid. Its legend entry reads "tracked profile".

## 5. Tests (Catch2)

1. **Learned:** bit-identical to the current release, and a state without the new IDs loads as Learned and renders bit-identically.
2. **Tracking:** pink noise at −50 dBFS for 10 s, then −40 dBFS for 10 s, Adapt speed 2 s: the estimate's average from 100 Hz to 8 kHz reaches within 2 dB of the new noise's learned level in 2 s ± 25 % after the step, at FFT 1024, 2048 and 8192.
3. **Not learning the signal:** a 1 kHz sine at −20 dBFS over noise at −60 dBFS for 30 s, Adaptive: the sine loses less than 0.5 dB at the end.
4. **Speech-like:** 300 ms tone bursts and 300 ms gaps over constant noise: the estimate stays within 2 dB of the noise's learned level.
5. **Musical noise:** the existing white noise and sine test in Adaptive gives a residual variance no worse than 1.5 × Learned's.
6. **CPU:** Adaptive at FFT 2048 under 1.5 × Learned (§0.4 method).
7. **Size change:** after a change from 2048 to 4096, the estimate carries over within 1 dB.
8. **Host:** H's offline-render test in Adaptive with Adapt speed automated; the flood test covers the new parameters.
