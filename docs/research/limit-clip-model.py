"""Model of Fancy Limit's pre-limiter clipper (docs/limit.md, Phase 4 Wave 2, item 9 of the order).

Clip-then-limit is the common mastering chain: a clipper shaves the fastest peaks (kick and snare
transients, a few ms long) so the limiter, which can only turn the whole signal down, has less to
do and pumps less. This compares, for the same push into a 0 dBFS ceiling:

  A. the limiter alone (lookahead peak limiter, 1.5 ms attack, 100 ms release, as a model);
  B. a soft clipper (a cubic knee from 6 dB under its ceiling) driven so it shaves the top 3 dB of
     the transients, then the same limiter for what's left.

on a drum-like test signal (60 Hz kicks with a 3 ms click, noise snares, a quiet pad underneath),
pushed 8 dB. Prints the limiter's gain reduction (average and on the pad between hits: pumping),
the output's loudness (mean square), and the clipper's added distortion against the signal.

Run: py -3 docs/research/limit-clip-model.py
"""
import numpy as np

RATE = 48000
rng = np.random.default_rng(3)
n = RATE * 4
t = np.arange(n) / RATE
x = 0.05 * np.sin(2 * np.pi * 220 * t)                              # the pad
for beat in range(16):
    start = int(beat * 0.25 * RATE)
    k = np.arange(int(0.25 * RATE))
    if start + len(k) > n:
        break
    if beat % 2 == 0:  # kick: a 3 ms click and a 60 Hz body
        x[start:start + len(k)] += 0.9 * np.exp(-k / (0.004 * RATE)) * rng.standard_normal(len(k)) * (k < 0.003 * RATE)
        x[start:start + len(k)] += 0.7 * np.exp(-k / (0.08 * RATE)) * np.sin(2 * np.pi * 60 * k / RATE)
    else:              # snare: a noise burst
        x[start:start + len(k)] += 0.6 * np.exp(-k / (0.03 * RATE)) * rng.standard_normal(len(k))

push = 10 ** (8 / 20)


def soft_clip(v, ceiling):
    """Linear to 6 dB under the ceiling, then a cubic knee that meets the ceiling flat."""
    knee = ceiling * 10 ** (-6 / 20)
    out = v.copy()
    over = np.abs(v) > knee
    a = np.abs(v[over])
    span = 2 * (ceiling - knee)
    y = np.where(a < knee + span, a - (a - knee) ** 2 / (2 * span), ceiling)
    out[over] = np.sign(v[over]) * y
    return out


def limiter(v, ceiling=1.0, attack_ms=1.5, release_ms=100.0):
    look = int(attack_ms * 0.001 * RATE)
    need = np.maximum(np.abs(v) / ceiling, 1.0)
    # the gain each sample needs, held over the lookahead window
    window = np.lib.stride_tricks.sliding_window_view(np.pad(need, (0, look)), look + 1).max(axis=1)
    rel = 1 - np.exp(-1 / (release_ms * 0.001 * RATE))
    g, gains = 1.0, np.empty(len(v))
    for i, w in enumerate(window):
        target = 1.0 / w
        g = target if target < g else g + (target - g) * rel
        gains[i] = g
    return v * gains, gains


def db(v):
    return 10 * np.log10(np.mean(v ** 2) + 1e-30)


pad_only = np.ones(n, bool)
for beat in range(16):
    pad_only[int(beat * 0.25 * RATE):int((beat * 0.25 + 0.12) * RATE)] = False

loud = x * push
out_a, g_a = limiter(loud)

grdb = lambda g: -20 * np.log10(g)
peak = np.max(np.abs(loud))
print(f"pushed 8 dB into a 0 dBFS ceiling; transient peak {20 * np.log10(peak):+.1f} dBFS before the clipper")
for shave in (0.0, 3.0, 6.0):
    clipped = soft_clip(loud, peak * 10 ** (-shave / 20)) if shave > 0 else loud
    out, g = limiter(clipped)
    added = clipped - loud
    distortion = f"{db(added) - db(loud):6.1f} dB" if shave > 0 else "  none  "
    print(f"  clip {shave:3.0f} dB, then limit: loudness {db(out):6.2f} dB, limiter GR mean {grdb(g).mean():4.1f} dB, "
          f"max {grdb(g).max():4.1f} dB, on the pad between hits {grdb(g)[pad_only].mean():4.1f} dB; clipper's added signal {distortion}")
