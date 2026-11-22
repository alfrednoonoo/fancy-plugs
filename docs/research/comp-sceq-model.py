"""Model of Fancy Comp's six-band sidechain EQ (docs/comp.md, Phase 4 Wave 2, item 13 of the order).

Comp's detector has Highpass and Lowpass (12 dB/oct SVFs) and three fixed bands: a low shelf, a
bell and a high shelf. Two common jobs it can't do well:

  1. Keep the kick out of a mix-bus compressor's detector. A 12 dB/oct high-pass at 150 Hz still
     lets a 55 Hz kick through at -17 dB; the compressor pumps on every kick.
  2. Make the detector ignore one resonance (a ringing snare at 220 Hz) while also tilting the
     highs: that needs a cut bell as well as the existing bell and shelves.

Test 1, 4 s at 48 kHz: a mix bed (pink-ish noise at -18 dBFS RMS from 200 Hz up) with a 55 Hz kick
every 0.5 s peaking at -6 dBFS. Comp at 4:1, Threshold -20 dB, Attack 10 ms, Release 100 ms, RMS
20 ms. The GR printed is the mean and the swing (mean in the 100 ms after kicks minus mean before
them), with the high-pass at 150 Hz in each slope.

Run: py -3 docs/research/comp-sceq-model.py
"""
import math

import numpy as np

RATE = 48000
rng = np.random.default_rng(4)
n = RATE * 4


def svf(sig, hz, q, kind):
    g = math.tan(math.pi * hz / RATE)
    k = 1 / q
    a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2
    ic1 = ic2 = 0.0
    out = np.empty(len(sig))
    for i, v in enumerate(sig):
        v3 = v - ic2
        v1 = a1 * ic1 + a2 * v3
        v2 = ic2 + a2 * ic1 + a3 * v3
        ic1, ic2 = 2 * v1 - ic1, 2 * v2 - ic2
        out[i] = v2 if kind == "low" else v - k * v1 - v2
    return out


# Butterworth Qs for cascades of 2nd-order sections.
def butterworth_qs(order):
    return [1 / (2 * math.cos(math.pi * (2 * k + 1) / (2 * order))) for k in range(order // 2)]


def highpass(sig, hz, slope_db):
    order = slope_db // 6
    y = sig
    for q in butterworth_qs(order):
        y = svf(y, hz, q, "high")
    return y


bed = rng.standard_normal(n)
bed = svf(bed, 200, 0.7071, "high")
bed *= 10 ** (-18 / 20) / math.sqrt(np.mean(bed ** 2))
x = bed.copy()
kicks = []
for k in range(8):
    s = int(k * 0.5 * RATE)
    kicks.append(s)
    m = np.arange(int(0.3 * RATE))
    kick = np.sin(2 * np.pi * 55 * m / RATE) * np.exp(-m / (0.1 * RATE))
    x[s:s + len(m)] += 10 ** (-6 / 20) * kick


def gr(detector):
    c = 1 - math.exp(-1 / (0.02 * RATE))
    a = 1 - math.exp(-1 / (0.01 * RATE))
    r = 1 - math.exp(-1 / (0.1 * RATE))
    ms, env, out = 0.0, 0.0, np.empty(n)
    for i, v in enumerate(detector):
        ms += (v * v - ms) * c
        level = 10 * math.log10(max(ms, 1e-20))
        want = max(0.0, (level + 20) * 0.75)
        env += (want - env) * (a if want > env else r)
        out[i] = env
    return out


after = np.zeros(n, bool)
before = np.zeros(n, bool)
for s in kicks:
    after[s:s + int(0.1 * RATE)] = True
    if s > int(0.2 * RATE):
        before[s - int(0.1 * RATE):s] = True

def report(title, sig):
    print(title)
    for name, det in (("no filter", sig), ("high-pass 12 dB/oct (today)", highpass(sig, cut, 12)),
                      ("high-pass 24 dB/oct", highpass(sig, cut, 24)), ("high-pass 48 dB/oct", highpass(sig, cut, 48)),
                      ("high-pass 96 dB/oct (brickwall)", highpass(sig, cut, 96))):
        g = gr(det)
        print(f"  {name:<34} GR {np.mean(g):4.1f} dB, pumping {np.mean(g[after]) - np.mean(g[before]):+4.1f} dB on each kick")


cut = 150
report("1. Mix bed at -18 dBFS RMS, 55 Hz kick at -6 dBFS, high-pass at 150 Hz (4:1, threshold -20 dB):", x)

# 2. A bass line at 100-130 Hz that should still drive the compressor, so the cut sits at 90 Hz,
# and a louder kick.
bass = np.zeros(n)
for k in range(16):
    s0 = int(k * 0.25 * RATE)
    m = np.arange(int(0.25 * RATE))
    bass[s0:s0 + len(m)] += np.sin(2 * np.pi * (98, 110, 123, 131)[k % 4] * m / RATE)
y = bed + 10 ** (-16 / 20) * bass
for s0 in kicks:
    m = np.arange(int(0.3 * RATE))
    y[s0:s0 + len(m)] += 10 ** (-2 / 20) * np.sin(2 * np.pi * 55 * m / RATE) * np.exp(-m / (0.1 * RATE))
cut = 90
report("2. The same bed, a bass line at 98-131 Hz to keep, a 55 Hz kick at -2 dBFS, high-pass at 90 Hz:", y)
