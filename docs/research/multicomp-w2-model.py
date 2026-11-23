"""Models for Fancy MultiComp's Wave 2 items (docs/multicomp.md, Phase 4 Wave 2: items 23, 24 and 25).

23. Per-band lookahead. Today one Lookahead (0 to 10 ms) delays every band's audio against its
    detector. Lookahead lets a fast attack catch a transient's start, but it also turns the gain
    down before the hit, a dip ahead of every note (worst on slow low notes). Per band, the audio
    of every band is delayed by the largest setting and each band's detector reads its own amount
    ahead. Measured for a low band (a bass note that swells in) and a high band (a snare crack):
    the overshoot past the target gain in the first 3 ms (what lookahead fixes), and the dip
    before the note starts (what it costs), at 0, 2 and 5 ms.

24. Per-band detector EQ. A band's detector hears only that band, but sometimes only part of it
    should drive the compression: in a 20-150 Hz band holding a 50 Hz kick and a 100 Hz bass, a
    high-pass at 80 Hz on the detector lets the bass, not the kick, set the gain. Measured: the
    gain reduction during kicks and during bass notes, with and without it.

25. Per-band crossover slope. The bands are built as a cascade: each band below a crossover goes
    through that crossover's own all-pass, so the sum is flat. With slopes mixed (24 at one
    crossover, 48 at another) the same rule holds if each crossover uses its own order's all-pass.
    Measured: the sum's ripple over 20 Hz to 20 kHz for three crossovers at 150 Hz, 1.5 kHz and
    6 kHz with mixed slopes, with each crossover's own all-pass and, for comparison, with the
    lower bands passed through 24 dB all-passes everywhere.

Run: py -3 docs/research/multicomp-w2-model.py
"""
import math

import numpy as np

FS = 48000


def follower_gain(level_db, threshold, ratio, attack_ms, release_ms):
    a = 1 - math.exp(-1 / (attack_ms * 0.001 * FS))
    r = 1 - math.exp(-1 / (release_ms * 0.001 * FS))
    env, out = 0.0, np.empty(len(level_db))
    for i, l in enumerate(level_db):
        want = max(0.0, (l - threshold) * (1 - 1 / ratio))
        env += (want - env) * (a if want > env else r)
        out[i] = -env
    return out


def peak_db(x, release_ms=5.0):
    r = 1 - math.exp(-1 / (release_ms * 0.001 * FS))
    env, out = 0.0, np.empty(len(x))
    for i, v in enumerate(np.abs(x)):
        env = v if v > env else env + (v - env) * r
        out[i] = 20 * math.log10(max(env, 1e-9))
    return out


# 23 --------------------------------------------------------------------------------------------
n = int(0.5 * FS)
t = np.arange(n) / FS
onset = int(0.2 * FS)
# Each band has sound playing before the onset (the previous note's tail, the hi-hats), which is
# what lookahead turns down early.
bass = np.where(t >= 0.2, np.sin(2 * np.pi * 70 * (t - 0.2)) * (1 - np.exp(-(t - 0.2) / 0.03)), 0.0) * 0.7
bass += np.where(t < 0.2, 0.06 * np.sin(2 * np.pi * 55 * t), 0.0)
rng = np.random.default_rng(1)
snare = np.where(t >= 0.2, rng.standard_normal(n) * np.exp(-np.maximum(t - 0.2, 0) / 0.04), 0.0) * 0.5
snare += np.where(t < 0.2, 0.03 * rng.standard_normal(n), 0.0)

print("23. Per-band lookahead (4:1, threshold -24 dB, attack 0.5 ms, release 120 ms):")
for name, sig in (("low band, a bass note swelling in", bass), ("high band, a snare crack", snare)):
    level = peak_db(sig)
    target = np.array([-max(0.0, (l + 24) * 0.75) for l in level])
    before = slice(onset - int(0.005 * FS), onset)                       # the 5 ms before the onset
    for look_ms in (0, 2, 5):
        d = int(look_ms * 0.001 * FS)
        ahead = np.concatenate([level[d:], np.full(d, level[-1])])   # the detector reads d samples ahead
        g = follower_gain(ahead, -24, 4, 0.5, 120)
        win = slice(onset, onset + int(0.003 * FS))
        overshoot = np.max(g[win] - target[win])                      # how far the gain lags the target
        ducked = -np.mean(g[before] - target[before])                 # the sound before it, turned down early
        print(f"    {name:<34} lookahead {look_ms} ms: the hit gets past by {max(0.0, overshoot):4.1f} dB in its first 3 ms; "
              f"the sound before it is turned down {ducked:4.1f} dB early")

# 24 --------------------------------------------------------------------------------------------
n = FS * 2
t = np.arange(n) / FS
kick = np.zeros(n)
for k in range(4):
    s = int(k * 0.5 * FS)
    m = np.arange(int(0.3 * FS))
    kick[s:s + len(m)] += 0.8 * np.sin(2 * np.pi * 50 * m / FS) * np.exp(-m / (0.08 * FS))
bassline = 0.25 * np.sin(2 * np.pi * 100 * t) * ((t % 0.5) >= 0.25)


def hp(x, hz, order=2):
    g = math.tan(math.pi * hz / FS)
    y = x
    for q in ([0.7071] if order == 2 else [0.5412, 1.3066]):
        k = 1 / q
        a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2
        ic1 = ic2 = 0.0
        out = np.empty(len(y))
        for i, v in enumerate(y):
            v3 = v - ic2
            v1 = a1 * ic1 + a2 * v3
            v2 = ic2 + a2 * ic1 + a3 * v3
            ic1, ic2 = 2 * v1 - ic1, 2 * v2 - ic2
            out[i] = v - k * v1 - v2
        y = out
    return y


band = kick + bassline
kicks = (t % 0.5) < 0.15
basses = ((t % 0.5) >= 0.3) & ((t % 0.5) < 0.45)
print("24. Per-band detector EQ, a 20-150 Hz band with a 50 Hz kick and a 100 Hz bass (4:1, threshold -24 dB):")
for name, det in (("detector hears the whole band", band), ("detector high-passed at 80 Hz (24 dB/oct)", hp(band, 80, 4))):
    g = follower_gain(peak_db(det, 20), -24, 4, 5, 120)
    print(f"    {name:<44} reduction during kicks {-np.mean(g[kicks]):4.1f} dB, during bass notes {-np.mean(g[basses]):4.1f} dB")

# 25 --------------------------------------------------------------------------------------------
f = np.logspace(math.log10(20), math.log10(20000), 2000)
s = 2j * np.pi * f


def butterworth(fc, order):
    # Butterworth low-pass and high-pass of order 2 or 4, as sections with Butterworth Qs.
    w = 2 * np.pi * fc
    u = s / w
    lp = np.ones_like(s)
    hpf = np.ones_like(s)
    for q in ([0.7071067811865476] if order == 2 else [0.5411961001461970, 1.3065629648763766]):
        den = u * u + u / q + 1
        lp = lp / den
        hpf = hpf * u * u / den
    return lp, hpf


def lr(fc, slope):
    # Linkwitz-Riley: Butterworth squared. LR 24 dB/oct is 2nd-order squared, LR 48 is 4th-order
    # squared. Low-pass plus high-pass is an all-pass.
    lp, hpf = butterworth(fc, 2 if slope == 24 else 4)
    lp, hpf = lp * lp, hpf * hpf
    return lp, hpf, lp + hpf


crossovers = [150, 1500, 6000]


def cascade(slopes, own_allpass=True):
    parts = [lr(fc, sl) for fc, sl in zip(crossovers, slopes)]
    total = 0
    for b in range(len(parts) + 1):
        h = np.ones_like(s)
        for k in range(b):
            h = h * parts[k][1]                    # the high-passes below this band
        if b < len(parts):
            h = h * parts[b][0]                    # its own low-pass
        for k in range(b + 1, len(parts)):         # the all-passes of the crossovers above it
            h = h * (parts[k][2] if own_allpass else lr(crossovers[k], 24)[2])
        total = total + h
    return 20 * np.log10(np.abs(total))


print("25. Per-band crossover slope, crossovers at 150 Hz, 1.5 kHz and 6 kHz:")
for slopes in ((24, 24, 24), (48, 48, 48), (48, 24, 48), (24, 48, 24)):
    own = cascade(slopes, True)
    wrong = cascade(slopes, False)
    print(f"    slopes {slopes} dB/oct: the sum is flat within {np.ptp(own):.4f} dB with each crossover's own all-pass; "
          f"within {np.ptp(wrong):.2f} dB if every lower band used a 24 dB all-pass")
