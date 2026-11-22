"""Models for Fancy Verb's Attack and Width to 200 % (docs/verb.md, Phase 4 Wave 2, items 17 and 18).

1. Attack (onset) for the Reverb generator. Build-up sets how long the tail takes to become dense,
   not how loud its start is: every generated tail starts at full level. An Attack envelope
   (a raised-cosine fade-in over Attack ms) makes it swell in, as reverse-ish pads, gated-snare
   rooms and "bloom" halls do. Measured on a generated tail (noise, Decay 1.8 s): clarity C80
   (energy before 80 ms over energy after, in dB) and when the tail's 10 ms energy peaks.

2. Width to 200 %. Verb's Width scales the wet signal's side (0 to 100 %). Above 100 % the side is
   raised further: wider, but louder on each side, and less correlated. Measured on a typical
   wet signal (left and right with correlation 0.3): the level on each side, the mono sum's
   level, and the correlation, with and without keeping each side's level constant.

Run: py -3 docs/research/verb-attack-width-model.py
"""
import math

import numpy as np

RATE = 48000
rng = np.random.default_rng(6)


def db(v):
    return 10 * math.log10(max(v, 1e-30))


# 1. Attack -------------------------------------------------------------------------------------
n = int(2.5 * RATE)
t = np.arange(n) / RATE
tail = rng.standard_normal(n) * np.exp(-6.91 * t / 1.8)   # -60 dB at Decay

print("1. Reverb generator, Decay 1.8 s:")
for attack_ms in (0, 50, 150, 300):
    a = attack_ms * 0.001
    envelope = np.ones(n) if a == 0 else np.where(t < a, 0.5 - 0.5 * np.cos(np.pi * t / a), 1.0)
    ir = tail * envelope
    split = int(0.08 * RATE)
    c80 = db(np.sum(ir[:split] ** 2)) - db(np.sum(ir[split:] ** 2))
    frame = int(0.01 * RATE)
    energy = np.array([np.sum(ir[i:i + frame] ** 2) for i in range(0, n - frame, frame)])
    peak_ms = 10 * int(np.argmax(energy))
    total = db(np.sum(ir ** 2)) - db(np.sum(tail ** 2))
    print(f"   Attack {attack_ms:3d} ms: C80 {c80:+5.1f} dB, energy peaks at {peak_ms:3d} ms, total level {total:+.1f} dB")

# 2. Width -------------------------------------------------------------------------------------
m = RATE * 2
common = rng.standard_normal(m)
left = math.sqrt(0.3) * common + math.sqrt(0.7) * rng.standard_normal(m)
right = math.sqrt(0.3) * common + math.sqrt(0.7) * rng.standard_normal(m)
mid0, side0 = 0.5 * (left + right), 0.5 * (left - right)
base_side_level = db(np.mean(left ** 2))

print("2. Width on a wet signal with correlation 0.3 (levels against Width 100 %):")
for keep_level in (False, True):
    for width in (1.0, 1.5, 2.0):
        mid, side = mid0, side0 * width
        if keep_level:
            # Keep each side's level: scale mid and side together back to Width 100 %'s.
            scale = math.sqrt(np.mean(left ** 2) / np.mean((mid + side) ** 2))
            mid, side = mid * scale, side * scale
        l, r = mid + side, mid - side
        corr = np.mean(l * r) / math.sqrt(np.mean(l ** 2) * np.mean(r ** 2))
        label = "keeping each side's level" if keep_level else "side raised only"
        print(f"   {label:<26} Width {int(width * 100):3d} %: each side {db(np.mean(l ** 2)) - base_side_level:+.1f} dB, "
              f"mono sum {db(np.mean(mid ** 2)) - db(np.mean(mid0 ** 2)):+.1f} dB, correlation {corr:+.2f}")
