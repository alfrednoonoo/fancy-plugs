"""Models for Fancy Room's Wave 2 items (docs/room.md, Phase 4 Wave 2: items 19, 20 and 21).

19. Per-band decay. Each of Room's delay lines has a gain g (Decay) and a one-pole damping filter
    whose top end decays faster (Damping). A Low RT multiplier adds a first-order low shelf at
    250 Hz to each line, so the lows decay Low RT x Decay. From the loop filter's response the
    decay time at each frequency is RT(f) = -3 d / (fs log10 |H(f)|), for a line of d samples.

20. Attack. Room's network can't fade its impulse response in the way Verb's generator can. The
    late network's input is fed instead through taps spread over Attack ms with a rising
    raised-cosine weighting, so the tail swells in. Compared with an ideal fade-in of the same
    tail, by clarity C80, when the energy peaks, and the total level.

21. External sidechain for Ducking: the same ducker (peak, 50 ms release; 24 dB x Ducking above
    -20 dBFS, none below -50; 10 ms attack, 300 ms release in dB), keyed by another track. A reverb
    on drums ducked by a vocal: the reduction follows the vocal's phrases, not the drums.

Run: py -3 docs/research/room-w2-model.py
"""
import math

import numpy as np

FS = 48000

# 19 ---------------------------------------------------------------------------------------------
d = round(0.040 * FS)          # a 40 ms line
T, D = 2.0, 0.45               # Decay and Damping (Room's defaults are 1.8 s and 45 %)
g = 10 ** (-3.0 * d / (FS * T))
ghf = 10 ** (-3.0 * d / (FS * T * (1.0 - 0.7 * D)))


def rt_at(f, low_mult):
    w = 2 * math.pi * f / FS
    z = complex(math.cos(w), math.sin(w))
    b = (1 - ghf / g) / (1 + ghf / g)
    damping = g * (1 - b) / (1 - b * z ** -1)          # DC gain g, Nyquist gain ghf
    shelf = 1.0
    if low_mult != 1.0:
        glow = 10 ** (-3.0 * d / (FS * T * low_mult)) / g   # the extra low gain per pass
        wc = math.tan(math.pi * 250.0 / FS)
        # Bilinear first-order low shelf: gain glow at DC, 1 at Nyquist.
        b0, b1 = (1 + glow * wc), (glow * wc - 1)
        a0, a1 = (1 + wc), (wc - 1)
        shelf = (b0 + b1 * z ** -1) / (a0 + a1 * z ** -1)
    h = abs(damping * shelf)
    return -3.0 * d / (FS * math.log10(h))


print("19. Room per-band decay, Decay 2 s, Damping 45 %: RT (s) at each frequency")
freqs = (63, 125, 250, 500, 1000, 2000, 4000, 8000)
print("    Low RT    " + "".join(f"{f:>7d}" for f in freqs))
for mult in (0.5, 1.0, 2.0):
    print(f"    x {mult:<4}    " + "".join(f"{rt_at(f, mult):7.2f}" for f in freqs))

# 20 ---------------------------------------------------------------------------------------------
rng = np.random.default_rng(3)
n = int(2.5 * FS)
t = np.arange(n) / FS
tail = rng.standard_normal(n) * np.exp(-6.91 * t / 1.8)


def metrics(ir):
    split = int(0.08 * FS)
    c80 = 10 * math.log10(np.sum(ir[:split] ** 2) / np.sum(ir[split:] ** 2))
    frame = int(0.01 * FS)
    energy = np.array([np.sum(ir[i:i + frame] ** 2) for i in range(0, n - frame, frame)])
    return c80, 10 * int(np.argmax(energy)), 10 * math.log10(np.sum(ir ** 2) / np.sum(tail ** 2))


print("20. Room Attack: the late tail (Decay 1.8 s), smeared input against an ideal fade-in")
for attack_ms in (50, 150, 300):
    a = attack_ms * 0.001
    ideal = tail * np.where(t < a, 0.5 - 0.5 * np.cos(np.pi * t / a), 1.0)
    taps = 16
    positions = [int(a * FS * k / (taps - 1)) for k in range(taps)]
    weights = np.array([0.5 - 0.5 * math.cos(math.pi * (k + 1) / taps) for k in range(taps)])
    weights /= math.sqrt(np.sum(weights ** 2))
    smeared = np.zeros(n)
    for p, w in zip(positions, weights):
        smeared[p:] += w * tail[:n - p]
    ci, pi_, li = metrics(ideal)
    cs, ps, ls = metrics(smeared)
    print(f"    Attack {attack_ms:3d} ms: ideal C80 {ci:+6.1f} dB, peak {pi_:3d} ms, level {li:+.1f} dB;  "
          f"16 smeared taps C80 {cs:+6.1f} dB, peak {ps:3d} ms, level {ls:+.1f} dB")

# 21 ---------------------------------------------------------------------------------------------
m = FS * 6
tt = np.arange(m) / FS
drums_wet = 10 ** (-12 / 20) * rng.standard_normal(m)
vocal = np.where((tt % 2.0) < 1.2, 10 ** (-10 / 20), 10 ** (-60 / 20)) * rng.standard_normal(m)


def duck_db(key, amount=1.0):
    peak_release = 1 - math.exp(-1 / (0.05 * FS))
    att, rel = 1 - math.exp(-1 / (0.01 * FS)), 1 - math.exp(-1 / (0.3 * FS))
    peak, env, out = 0.0, 0.0, np.empty(len(key))
    for i, v in enumerate(np.abs(key)):
        peak = v if v > peak else peak + (v - peak) * peak_release
        db = 20 * math.log10(max(peak, 1e-9))
        want = 24 * amount * min(1.0, max(0.0, (db + 50) / 30))
        env += (want - env) * (att if want > env else rel)
        out[i] = env
    return out


own = duck_db(drums_wet)
keyed = duck_db(vocal)
singing = (tt % 2.0) < 1.2
print("21. Room Ducking 100 % on a drum reverb (input -12 dBFS RMS):")
print(f"    keyed by its own input:   {np.mean(own[singing]):4.1f} dB while the vocal sings, {np.mean(own[~singing & (tt > 0.5)]):4.1f} dB in the gaps")
print(f"    keyed by a vocal (sidechain): {np.mean(keyed[singing & (tt > 0.5)]):4.1f} dB while it sings, {np.mean(keyed[~singing & (tt > 0.5)]):4.1f} dB in the gaps")
