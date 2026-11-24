"""Pre-push model for Fancy FIR's Phase 4 Wave 2 items 27 and 28 (docs/fir.md, "ratio curve,
Mid/Side, and EQ-mode dynamics", section 5), measuring what tests/FirW2bTests.cpp measures.

Test 2, the model's mix (docs/research/fir-w2-model.py) through the engine as built: a square-root
Hann STFT of 2048 points at a hop of 512, latency 2048; each bin's detector is the loudest within
two bins either side, calibrated so a sine at amplitude A reads 20 log10 A; Fancy Comp's gain
computer with no knee; attack and release per hop (10 and 120 ms); Reduce artifacts' [1/4, 1/2, 1/4]
across bins. The tones sit on bin centres (93.75, 2507.8, 3000 and 3609.4 Hz) so that the
detector reads their level exactly, as the model's per-bin threshold did. The threshold curve is
3 dB over the verse: -9 dB below 1 kHz (the bass at -12 dB) and -31 dB above (each harsh tone at
-34 dB). Measured as the test does: each tone's amplitude in the output against the input, over
2.3 to 3.9 s of the chorus, by projection.

Tests 3 and 4 need no model: with the mid's curve flat, the mid is processed exactly as the L/R
engine processes the same samples (a mask of 1.0f, which Reduce artifacts keeps at 1.0f), so the
only difference is the decode's rounding, under -140 dB; and a dual-mono input has a side of
exactly zero, which every mode keeps at zero, so it renders bit for bit as L/R.

Run: python docs/research/fir-w2b-prepush.py
"""
import math

import numpy as np

FS = 48000
N, HOP = 2048, 512
LENGTH = FS * 4

n = np.arange(N)
hann = 0.5 - 0.5 * np.cos(2 * np.pi * n / N)
analysis = np.sqrt(hann)
synthesis = 0.5 * np.sqrt(hann)
level_scale = 2.0 / analysis.sum()
freqs = np.fft.rfftfreq(N, 1 / FS)
bins = len(freqs)

LOW_HZ = 93.75
HARSH_HZ = (2507.8125, 3000.0, 3609.375)
PHASES = (0.4, 1.9, 3.3)


def signal(seed=7):
    t = np.arange(LENGTH) / FS
    chorus = t >= 2.0
    rng = np.random.default_rng(seed)
    low = np.where(chorus, 0.5, 0.25) * np.sin(2 * np.pi * LOW_HZ * t)
    harsh = sum(np.sin(2 * np.pi * f * t + p) for f, p in zip(HARSH_HZ, PHASES)) * np.where(chorus, 0.12, 0.02)
    return (low + harsh + 0.003 * rng.standard_normal(LENGTH)).astype(np.float32).astype(np.float64)


def per_hop(ms):
    samples = ms * 0.001 * FS / HOP
    return 1.0 - math.exp(-1.0 / samples) if samples > 0 else 1.0


def run(x, ratios):
    threshold = np.where(freqs < 1000.0, -9.0, -31.0)
    slope = 1.0 - 1.0 / ratios
    attack, release = per_hop(10.0), per_hop(120.0)
    envelope = np.zeros(bins)
    out = np.zeros(LENGTH + N)
    for end in range(N, LENGTH + 1, HOP):
        frame = x[end - N:end] * analysis
        spectrum = np.fft.rfft(frame)
        power = np.abs(spectrum) ** 2
        loudest = np.array([power[max(0, k - 2):k + 3].max() for k in range(bins)])
        level = 10 * np.log10(np.maximum(loudest * level_scale ** 2, 1e-20))
        target = np.where(level > threshold, slope * (level - threshold), 0.0)
        envelope += (target - envelope) * np.where(target > envelope, attack, release)
        gain = 10 ** (-envelope / 20)
        smoothed = gain.copy()
        smoothed[1:-1] = 0.25 * gain[:-2] + 0.5 * gain[1:-1] + 0.25 * gain[2:]
        out[end - N:end] += np.fft.irfft(spectrum * smoothed, N) * synthesis
    return out[:LENGTH]  # out[i] is the input at i - N


def amplitude(y, hz, start, stop):
    t = np.arange(start, stop) / FS
    s = np.sum(y[start:stop] * np.sin(2 * np.pi * hz * t))
    c = np.sum(y[start:stop] * np.cos(2 * np.pi * hz * t))
    return 2 * math.hypot(s, c) / (stop - start)


x = signal()
start, stop = int(2.3 * FS), int(3.9 * FS)
one = np.full(bins, 4.0)
curve = np.where((freqs > 2000) & (freqs < 5000), 4.0, 1.0)

print("Test 2, the model's mix through the engine as built (reduction in dB over 2.3-3.9 s):")
for name, ratios in (("one Ratio, 4:1", one), ("ratio curve, 4:1 from 2 to 5 kHz", curve)):
    y = run(x, ratios)
    reduction = lambda hz: 20 * math.log10(amplitude(x, hz, start, stop) / amplitude(y, hz, start + N, stop + N))
    harsh = [reduction(hz) for hz in HARSH_HZ]
    print(f"    {name:<34} harsh tones {', '.join(f'{v:.2f}' for v in harsh)} (mean {np.mean(harsh):.2f}); "
          f"bass {reduction(LOW_HZ):.2f}")
print("Test 2's limits: the curve's harsh mean 9 +- 1.5 dB and bass under 0.2 dB; one ratio's bass over 1.5 dB.")
