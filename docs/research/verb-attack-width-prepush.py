"""Pre-push check for Fancy Verb's Attack and Extra width (docs/verb.md, Phase 4 Wave 2, items 17
and 18), measuring what tests/VerbW2bTests.cpp measures.

The Reverb generator at its defaults (Length 2.5 s, Decay 1.8 s, Density 80 %, Damping 40 %,
Spread 100 %, Early off) runs the original generator path: density ramps over 100 ms (amplitude
held by 1 / sqrt(probability)), a one-pole lowpass whose cutoff falls with time, a 50 ms end fade,
then unit energy. Attack multiplies the tail by a raised cosine after that, as IrBuilder does.

Test 2 measures on the left channel: where the 10 ms energy peaks (frames from 0) and C80
(energy before 80 ms over energy after, dB), with Attack 0 and 150 ms, over several seeds.
Test 3 (sample ratio equals the raised cosine) and the Extra width tests are exact by construction.

Run: py -3 docs/research/verb-attack-width-prepush.py
"""
import math

import numpy as np

RATE = 48000.0


def generator(seed, length=2.5, decay=1.8, density=0.8, damping=0.4, rate=RATE):
    rng = np.random.default_rng(seed)
    n = int(length * rate)
    t = np.arange(n) / rate
    fill = np.minimum(1.0, np.arange(n) / (0.1 * rate))
    p0 = 0.02 + 0.98 * density * density
    prob = p0 + (1 - p0) * fill
    keep = rng.random(n) < prob
    scale = np.where(keep, np.exp(-6.907755278982137 * t / decay) / np.sqrt(prob), 0.0)
    x = (rng.random(n) * 2 - 1) * scale                      # spread 100 %: no common part
    cutoff = np.maximum(200.0, 20000.0 * np.exp(-t * damping * 4.0 / decay))
    coef = 1 - np.exp(-2 * math.pi * cutoff / rate)
    y = np.empty(n)
    s = 0.0
    for i in range(n):
        s += (x[i] - s) * coef[i]
        y[i] = s
    fade = min(n, int(0.05 * rate))
    y[n - fade:] *= (np.arange(fade)[::-1] / fade)            # sample length-1-i times i / fade
    return y


def attack(y, ms, rate=RATE):
    a = ms * 0.001 * rate
    i = np.arange(len(y))
    return y * np.where(i < a, 0.5 - 0.5 * np.cos(np.pi * i / a), 1.0)


def measures(y, rate=RATE):
    frame = int(0.01 * rate)
    frames = len(y) // frame
    energy = [np.sum(y[k * frame:(k + 1) * frame] ** 2) for k in range(frames)]
    peak_ms = 10 * int(np.argmax(energy))
    split = int(0.08 * rate)
    c80 = 10 * math.log10(np.sum(y[:split] ** 2) / np.sum(y[split:] ** 2))
    return peak_ms, c80


print("Test 2: Attack 150 ms, Decay 1.8 s, generator defaults")
for seed in range(1, 7):
    y = generator(seed)
    p0, c0 = measures(y)
    p1, c1 = measures(attack(y, 150))
    print(f"  seed {seed}: peak {p0:3d} -> {p1:3d} ms (110 to 150), C80 {c0:+.1f} -> {c1:+.1f} dB, fall {c0 - c1:.1f} dB (10 to 14)")

print("Test 2 as tested: both channels' energy together, 30 seeds")
peaks, falls = [], []
for seed in range(100, 130):
    left, right = generator(seed), generator(seed + 1000)
    frame = int(0.01 * RATE)
    both = lambda l, r: [np.sum(l[k*frame:(k+1)*frame]**2) + np.sum(r[k*frame:(k+1)*frame]**2) for k in range(len(l)//frame)]
    la, ra = attack(left, 150), attack(right, 150)
    peaks.append(10 * int(np.argmax(both(la, ra))))
    split = int(0.08 * RATE)
    c = lambda l, r: 10*math.log10((np.sum(l[:split]**2)+np.sum(r[:split]**2))/(np.sum(l[split:]**2)+np.sum(r[split:]**2)))
    falls.append(c(left, right) - c(la, ra))
print(f"  peak ms: min {min(peaks)}, max {max(peaks)}; C80 fall: {min(falls):.1f} to {max(falls):.1f} dB")
