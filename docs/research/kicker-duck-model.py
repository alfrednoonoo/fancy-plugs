"""Model of Fancy Kicker's sidechain Duck (docs/kicker.md, Phase 4: Kicker 7, moved from Phase 3 Q2.2).

Kicker on a bass adds low end: Boost swells the bass's own note, Sub adds an octave below. When the
kick hits, both fill 40-80 Hz at once and the low end clips or blurs. Duck turns Kicker's added
part (its wet signal) down while a kick on the sidechain hits, then lets it back over Release.

The test, 4 s at 48 kHz, 120 BPM: a bass line (55 to 82 Hz, 8th notes) through Kicker's added sub
(modelled as the bass an octave down at -6 dB), and a kick on every beat on the sidechain (a 50 Hz
body decaying over 120 ms). Duck follows the kick's peak (1 ms attack) and lets go over Release.

Printed: the low end's peak (40-80 Hz, kick + bass + sub) during kicks and between them, and how
much of the added sub is left between kicks (it should come back), at Duck 0, 6, 12 and 24 dB.

Run: py -3 docs/research/kicker-duck-model.py
"""
import math

import numpy as np

FS = 48000
n = FS * 4
t = np.arange(n) / FS
beat = 0.5

bass = np.zeros(n)
notes = (55.0, 65.4, 73.4, 82.4)
for k in range(int(4 / 0.25)):
    s = int(k * 0.25 * FS)
    m = np.arange(int(0.25 * FS))
    f = notes[(k // 2) % 4]
    bass[s:s + len(m)] = 0.35 * np.sin(2 * np.pi * f * m / FS) * np.minimum(1, m / (0.005 * FS))
sub = np.zeros(n)
for k in range(int(4 / 0.25)):
    s = int(k * 0.25 * FS)
    m = np.arange(int(0.25 * FS))
    f = notes[(k // 2) % 4] / 2
    sub[s:s + len(m)] = 0.5 * 0.35 * np.sin(2 * np.pi * f * m / FS) * np.minimum(1, m / (0.005 * FS))

kick = np.zeros(n)
for k in range(int(4 / beat)):
    s = int(k * beat * FS)
    m = np.arange(int(0.3 * FS))
    kick[s:s + len(m)] += 0.8 * np.sin(2 * np.pi * 50 * m / FS) * np.exp(-m / (0.12 * FS))


def duck_gain(key, depth_db, release_ms):
    a = 1 - math.exp(-1 / (0.001 * FS))
    r = 1 - math.exp(-1 / (release_ms * 0.001 * FS))
    peak_r = 1 - math.exp(-1 / (0.03 * FS))
    peak, env, out = 0.0, 0.0, np.empty(len(key))
    for i, v in enumerate(np.abs(key)):
        peak = v if v > peak else peak + (v - peak) * peak_r
        db = 20 * math.log10(max(peak, 1e-9))
        want = depth_db * min(1.0, max(0.0, (db + 24) / 12))          # full above -12 dBFS, none below -24
        env += (want - env) * (a if want > env else r)
        out[i] = 10 ** (-env / 20)
    return out


during = (t % beat) < 0.12
between = ((t % beat) > 0.3) & ((t % beat) < 0.48)


def low_db(x, mask):
    return 10 * math.log10(np.mean(x[mask] ** 2) + 1e-30)


print("Kicker Duck: bass + added sub, kick on the sidechain (Release 120 ms; the duck full above -12 dBFS, none below -24):")
for target in ("the added sub only", "the whole bass (sub and dry)"):
    for depth in (0, 6, 12):
        g = duck_gain(kick, depth, 120)
        bass_out = (bass * g if "whole" in target else bass) + sub * g
        clash = low_db(bass_out, during)                        # the bass's low end while the kick plays
        back = 10 * math.log10(np.mean((bass_out[between]) ** 2) / np.mean((bass + sub)[between] ** 2))
        peak = 20 * math.log10(np.max(np.abs((kick + bass_out)[during])))
        print(f"    {target:<30} Duck {depth:2d} dB: the bass under each kick {clash:6.1f} dB, peak {peak:+5.1f} dBFS; "
              f"between kicks the bass is back to {back:+5.1f} dB")
