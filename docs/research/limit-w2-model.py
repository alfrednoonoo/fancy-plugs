"""Models for Fancy Limit's Wave 2 items (docs/limit.md, Phase 4 Wave 2: items 30 and 31).

30. Mid/Side linking. Limit links left and right (Link %). On a wide mix, a hard-panned transient
    (a tom fill, a crash) peaks in one channel; linked, the gain drops on both, so the centre's
    vocal dips with every side hit. Limiting mid and side instead lets a side peak turn down the
    side only. But the ceiling is in left and right: L = M + S can peak higher than either, so a
    final left/right stage must still hold the ceiling. Measured: how far the centre (the vocal)
    dips on side hits, and the left/right peak before that final stage.

31. External sidechain. A limiter keeps its own output under the ceiling, so it must hear itself.
    Keyed by another signal, it would turn down when the key is loud and let its own peaks
    through. Measured: the output's peak over the ceiling when keyed by the sidechain alone, and
    when the gain is the deeper of the two (own and key).

Lookahead peak limiter as a model: 1.5 ms lookahead, gain = min(1, ceiling / peak), 80 ms release.

Run: py -3 docs/research/limit-w2-model.py
"""
import math

import numpy as np

FS = 48000
rng = np.random.default_rng(8)
n = FS * 4
t = np.arange(n) / FS
CEILING = 10 ** (-1 / 20)


def limiter_gain(peaks):
    look = int(0.0015 * FS)
    r = 1 - math.exp(-1 / (0.08 * FS))
    ahead = np.array([np.max(peaks[i:i + look + 1]) for i in range(0, n, 1)]) if False else None
    # A running max over the lookahead window, computed in blocks for speed.
    padded = np.concatenate([peaks, np.zeros(look)])
    window_max = np.max(np.lib.stride_tricks.sliding_window_view(padded, look + 1), axis=1)[:n]
    want = np.minimum(1.0, CEILING / np.maximum(window_max, 1e-9))
    g, out = 1.0, np.empty(n)
    for i, w in enumerate(want):
        g = w if w < g else g + (w - g) * r
        out[i] = g
    return out


# A centre vocal, and a hard-left tom fill every second.
vocal = 0.35 * np.sin(2 * np.pi * 220 * t) * (0.7 + 0.3 * np.sin(2 * np.pi * 3 * t))
fill = np.zeros(n)
for k in range(4):
    s = int((0.5 + k) * FS)
    m = np.arange(int(0.2 * FS))
    fill[s:s + len(m)] += 0.9 * np.sin(2 * np.pi * 110 * m / FS) * np.exp(-m / (0.05 * FS))
left, right = (vocal + fill) * 1.4, vocal * 1.4      # pushed 3 dB into the limiter
mid, side = 0.5 * (left + right), 0.5 * (left - right)
hits = np.zeros(n, bool)
for k in range(4):
    s = int((0.5 + k) * FS)
    hits[s:s + int(0.15 * FS)] = True

print("30. A centre vocal with a hard-left tom fill, pushed 3 dB into a -1 dBFS ceiling:")
g_lr = limiter_gain(np.maximum(np.abs(left), np.abs(right)))
dip_lr = -20 * math.log10(np.mean(g_lr[hits]))
print(f"    L/R linked (today): the vocal dips {dip_lr:4.1f} dB on each fill")

g_m = limiter_gain(np.abs(mid))
g_s = limiter_gain(np.abs(side))
l_ms, r_ms = mid * g_m + side * g_s, mid * g_m - side * g_s
over = 20 * math.log10(max(np.max(np.abs(l_ms)), np.max(np.abs(r_ms))) / CEILING)
g_final = limiter_gain(np.maximum(np.abs(l_ms), np.abs(r_ms)))
dip_ms = -20 * math.log10(np.mean((g_m * g_final)[hits]))
print(f"    Mid/Side, then left/right held at the ceiling: the vocal dips {dip_ms:4.1f} dB on each fill "
      f"(before the final stage the left/right peak was {over:+.1f} dB over the ceiling)")

print("31. External sidechain: the bus pushed 3 dB over, keyed by a kick that's loud where the bus isn't:")
key = np.zeros(n)
for k in range(8):
    s = int((0.25 + 0.5 * k) * FS)
    m = np.arange(int(0.1 * FS))
    key[s:s + len(m)] += 0.9 * np.sin(2 * np.pi * 55 * m / FS) * np.exp(-m / (0.04 * FS))
own = np.maximum(np.abs(left), np.abs(right))
g_key = limiter_gain(np.abs(key))
out_key = own * g_key
g_both = np.minimum(g_key, limiter_gain(own))
out_both = own * g_both
print(f"    keyed by the sidechain alone: the output peaks {20 * math.log10(np.max(out_key) / CEILING):+.1f} dB over the ceiling")
print(f"    the deeper of own and key: {20 * math.log10(np.max(out_both) / CEILING):+.1f} dB (held), "
      f"and the bus is turned down a further {-20 * math.log10(np.mean(g_both) / np.mean(limiter_gain(own))):.1f} dB on average by the key")
