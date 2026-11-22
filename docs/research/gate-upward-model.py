"""Model of Fancy Gate's Upward mode (docs/gate.md, Phase 4 Wave 2, item 12 of the order).

Gate turns things down below Threshold (Gate) or while the key is loud (Duck). Upward is the third
way: above Threshold the level is raised, by (level - Threshold) x (Ratio - 1) dB, up to Boost, and
nothing below Threshold changes. It makes loud passages louder than quiet ones: contrast back into
a flattened mix, or a chorus that lifts.

Two tests, at 48 kHz, each with an RMS detector (RMS size 50 ms), Attack 10 ms and Release 200 ms:

  1. A drum loop (kick and snare over a room at -30 dBFS): does it add punch? (crest factor change)
  2. A flattened song: a verse at -24 dBFS RMS and a chorus at -16, both a busy bed with drums.
     How much louder does the chorus get than the verse, and does the verse change at all?

Run: py -3 docs/research/gate-upward-model.py
"""
import math

import numpy as np

RATE = 48000
rng = np.random.default_rng(9)


def drums(n, hit_db, room_db):
    x = 10 ** (room_db / 20) * rng.standard_normal(n) * 0.3
    for k in range(int(n / (0.25 * RATE))):
        start = int(k * 0.25 * RATE)
        m = np.arange(min(int(0.25 * RATE), n - start))
        kick = k % 2 == 0
        body = np.sin(2 * np.pi * (60 if kick else 190) * m / RATE) * np.exp(-m / ((0.12 if kick else 0.08) * RATE))
        noise = rng.standard_normal(len(m)) * np.exp(-m / ((0.002 if kick else 0.05) * RATE)) * (0.6 if kick else 0.4)
        hit = body + noise
        x[start:start + len(m)] += 10 ** (hit_db / 20) * hit / np.max(np.abs(hit))
    return x


def rms_db(sig, ms=50.0):
    c = 1 - math.exp(-1 / (ms * 0.001 * RATE))
    ms_, out = 0.0, np.empty(len(sig))
    for i, v in enumerate(sig):
        ms_ += (v * v - ms_) * c
        out[i] = 10 * math.log10(max(ms_, 1e-20))
    return out


def upward(level, threshold, ratio, boost, attack_ms=10.0, release_ms=200.0):
    over = np.maximum(0.0, level - threshold)
    want = np.minimum(boost, over * (ratio - 1.0)) if math.isfinite(ratio) else np.where(over > 0, boost, 0.0)
    a = 1 - math.exp(-1 / (attack_ms * 0.001 * RATE))
    r = 1 - math.exp(-1 / (release_ms * 0.001 * RATE))
    g, out = 0.0, np.empty(len(level))
    for i, w in enumerate(want):
        g += (w - g) * (a if w > g else r)
        out[i] = g
    return out


def db(v):
    return 20 * math.log10(max(v, 1e-12))


def crest(sig):
    return db(np.max(np.abs(sig))) - db(math.sqrt(np.mean(sig ** 2)))


print("Fancy Gate Upward:")

loop = drums(RATE * 4, -8, -30)
level = rms_db(loop)
g = upward(level, -24, 2.0, 6)
y = loop * 10 ** (g / 20)
print(f"  1. drum loop, Threshold -24, Ratio 1:2, Boost 6: crest factor {crest(y) - crest(loop):+.1f} dB (punch is Fancy TS's job)")

n = RATE * 8
bed = rng.standard_normal(n)
song = np.concatenate([bed[:n // 2] * 10 ** (-24 / 20), bed[n // 2:] * 10 ** (-16 / 20)])
song += np.concatenate([drums(n // 2, -14, -60), drums(n // 2, -8, -60)])
level = rms_db(song)
half = n // 2
before = db(math.sqrt(np.mean(song[half + RATE:] ** 2))) - db(math.sqrt(np.mean(song[RATE:half] ** 2)))
for threshold, ratio, boost in ((-20, 1.5, 6), (-20, 2.0, 6), (-18, 2.0, 4), (-20, float('inf'), 4)):
    g = upward(level, threshold, ratio, boost)
    y = song * 10 ** (g / 20)
    after = db(math.sqrt(np.mean(y[half + RATE:] ** 2))) - db(math.sqrt(np.mean(y[RATE:half] ** 2)))
    r = "inf" if not math.isfinite(ratio) else f"{ratio}"
    print(f"  2. song, Threshold {threshold}, Ratio 1:{r:<3}, Boost {boost}: chorus over verse {before:.1f} -> {after:.1f} dB, "
          f"verse +{np.mean(g[RATE:half]):.2f} dB, chorus +{np.mean(g[half + RATE:]):.1f} dB, peak {db(np.max(np.abs(y))):.1f} dBFS")
