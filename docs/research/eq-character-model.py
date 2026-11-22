"""Model of Fancy EQ's Character output stage (docs/eq.md, Phase 4 Wave 2, item 16 of the order).

Clean / Subtle / Warm after the bands, reusing Sature's soft curve as Comp's Character does:
f(d * x) / d, which has slope 1 at zero, so quiet material passes at unity and only peaks pick up
harmonics. Subtle is a light, symmetric drive (odd harmonics, very low); Warm a stronger, asymmetric
one (negative limit 1.4, Comp's Warm), so even harmonics lead.

For a 1 kHz sine at several levels this prints the level change, THD, and the 2nd and 3rd
harmonics against the fundamental. The drives are chosen so Subtle stays under 0.1 % THD at
-12 dBFS, and Warm reaches about 1 % at -6 dBFS.

Run: py -3 docs/research/eq-character-model.py
"""
import math

import numpy as np

RATE = 48000
N = RATE  # 1 s, so bins are 1 Hz


def soft(u, positive, negative):
    a = np.where(u < 0, negative, positive)
    r = u / a
    return u / np.sqrt(1 + r * r)


def stage(x, drive_db, negative):
    d = 10 ** (drive_db / 20)
    y = soft(d * x, 1.0, negative) / d
    return y - np.mean(y)  # the DC blocker after an asymmetric curve


def analyse(y):
    spectrum = np.abs(np.fft.rfft(y * np.hanning(len(y))))
    h = [spectrum[1000 * k] for k in range(1, 8)]
    thd = math.sqrt(sum(v * v for v in h[1:])) / h[0]
    return h, thd


t = np.arange(N) / RATE
print("Fancy EQ Character on a 1 kHz sine:")
for name, drive, negative in (("Subtle (drive -9 dB, symmetric)", -9.0, 1.0), ("Warm (drive -1.5 dB, negative limit 1.4)", -1.5, 1.4),
                                ("Warm (drive -1.5 dB, negative limit 2.5)", -1.5, 2.5), ("Warm (drive -3 dB, negative limit 4)", -3.0, 4.0)):
    print(f"  {name}")
    for level in (-18, -12, -6, 0):
        x = 10 ** (level / 20) * np.sin(2 * np.pi * 1000 * t)
        y = stage(x, drive, negative)
        h, thd = analyse(y)
        change = 20 * math.log10(h[0] / analyse(x)[0][0])
        print(f"    {level:3d} dBFS: level {change:+.2f} dB, THD {100 * thd:6.3f} %, "
              f"H2 {20 * math.log10(h[1] / h[0] + 1e-12):6.1f} dB, H3 {20 * math.log10(h[2] / h[0] + 1e-12):6.1f} dB")
