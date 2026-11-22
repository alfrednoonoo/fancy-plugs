"""Model of Fancy Comp's link modes (docs/comp.md, Phase 4 Wave 2, item 14 of the order).

Comp detects left and right, and Link channels with Link amount decides how they combine. Mixes
are built in mid and side: the vocal, bass and kick in the middle, wide guitars and reverb at the
sides. Compressing left and right means a loud vocal turns the sides down too, so the stereo width
breathes with the singing. The modes:

  Stereo:   today's left and right, linked by Link amount (100 % here).
  Mid/Side: mid and side each detected and compressed on their own (Link amount between them).
  M>S:      the mid's level compresses the side only: the width narrows while the centre is loud,
            for a vocal that steps forward. The mid passes untouched.
  S>M:      the side's level compresses the mid only.

The test, 6 s at 48 kHz: a vocal in the mid (phrases of 1 s at -12 dBFS RMS with 0.5 s gaps at
-40), wide guitars in the side (steady at -20 dBFS RMS). Comp at 4:1, Threshold -24 dB, Attack
10 ms, Release 150 ms, RMS 30 ms. Printed: the mid's and the side's gain while the vocal sings and
in the gaps, and how much the side's level moves between them (the width breathing).

Run: py -3 docs/research/comp-linkmodes-model.py
"""
import math

import numpy as np

RATE = 48000
rng = np.random.default_rng(2)
n = RATE * 6
t = np.arange(n) / RATE

singing = (t % 1.5) < 1.0
vocal = rng.standard_normal(n) * np.where(singing, 10 ** (-12 / 20), 10 ** (-40 / 20))
guitars = rng.standard_normal(n) * 10 ** (-20 / 20)
mid, side = vocal, guitars


def level_db(sig):
    c = 1 - math.exp(-1 / (0.03 * RATE))
    ms, out = 0.0, np.empty(len(sig))
    for i, v in enumerate(sig):
        ms += (v * v - ms) * c
        out[i] = 10 * math.log10(max(ms, 1e-20))
    return out


def gain_db(level):
    a = 1 - math.exp(-1 / (0.01 * RATE))
    r = 1 - math.exp(-1 / (0.15 * RATE))
    env, out = 0.0, np.empty(len(level))
    for i, l in enumerate(level):
        want = max(0.0, (l + 24) * 0.75)
        env += (want - env) * (a if want > env else r)
        out[i] = -env
    return out


left, right = mid + side, mid - side
lm, ls = level_db(mid), level_db(side)
ll, lr = level_db(left), level_db(right)

modes = {}
g = gain_db(np.maximum(ll, lr))                 # Stereo, linked 100 %
modes["Stereo (today, linked)"] = (g, g)       # the same gain on L and R is the same on M and S
modes["Mid/Side"] = (gain_db(lm), gain_db(ls))
modes["M>S"] = (np.zeros(n), gain_db(lm))
modes["S>M"] = (gain_db(ls), np.zeros(n))

sing = singing & (t > 0.3)
gap = ~singing & (t > 0.3)

print("Fancy Comp link modes: vocal in the mid (-12 dBFS RMS in phrases), guitars in the side (-20, steady):")
for name, (gm, gs) in modes.items():
    breathing = np.mean(gs[gap]) - np.mean(gs[sing])
    print(f"  {name:<24} mid {np.mean(gm[sing]):5.1f} / {np.mean(gm[gap]):5.1f} dB, side {np.mean(gs[sing]):5.1f} / {np.mean(gs[gap]):5.1f} dB "
          f"(singing / gaps): the width moves {breathing:4.1f} dB with the vocal")
