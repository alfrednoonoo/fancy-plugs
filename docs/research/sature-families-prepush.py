"""Pre-push check for Fancy Sature's item 36, the Diode and Fold curves (docs/sature.md, "Phase 4
Wave 2", test 3), written before any code (24 November 2026).

Test 3 says "ADAA keeps their aliasing at 1x under the existing curves' limit". Two things to
settle before building:

1. Sature with Oversampling Off runs the bare curve without ADAA (tests/SatureTests.cpp: "Off,
   which runs without ADAA"); ADAA runs only in the oversampled path. So "at 1x with ADAA" isn't a
   setting the plugin has.
2. The existing limits (tests/SatureTests.cpp) are for Clip at Drive 24, a tone at -6 dBFS, 44.1
   kHz: at 1 kHz, under -90 dBFS at 4x and under -38 dBFS Off; at 10 kHz, under -50 dBFS at 4x.
   Fold at Drive 24 drives sin(pi/2 u) with u up to 8, four folds deep, so its harmonics reach
   much higher than Clip's.

This measures the worst non-harmonic component from 20 Hz to 20 kHz, as worstAliasDb does
(Blackman-Harris, 6 Hz either side of each harmonic left out), for Clip, Diode (the design model's
half-wave: soft above zero, a tenth of it below) and Fold, at Off (bare curve) and 4x (first-order
ADAA in the oversampled path, half-band filters like SwitchedOversampler's: Kaiser stand-ins with
the same transition widths and stopbands).

Run: python docs/research/sature-families-prepush.py
"""
import math

import numpy as np

FS = 44100.0


def soft(u):
    return u / np.sqrt(1 + u * u)


def soft_int(u):
    return np.sqrt(1 + u * u) - 1


CURVES = {
    "Clip": (lambda u: np.clip(u, -1, 1),
             lambda u: np.where(np.abs(u) <= 1, 0.5 * u * u, np.abs(u) - 0.5)),
    "Diode": (lambda u: np.where(u > 0, soft(u), 0.1 * soft(u)),
              lambda u: np.where(u > 0, soft_int(u), 0.1 * soft_int(u))),
    "Fold": (lambda u: np.sin(np.pi / 2 * u),
             lambda u: -2 / np.pi * np.cos(np.pi / 2 * u)),
}


def adaa(f, F, x):
    prev = np.concatenate([[x[0]], x[:-1]])
    d = x - prev
    safe = np.abs(d) > 1e-6
    out = np.where(safe, (F(x) - F(prev)) / np.where(safe, d, 1.0), f(0.5 * (x + prev)))
    return out


def halfband(transition, stopband_db):
    a = -stopband_db
    beta = 0.1102 * (a - 8.7)
    taps = int(math.ceil((a - 7.95) / (14.36 * transition))) | 1
    m = np.arange(taps) - (taps - 1) / 2
    h = 0.5 * np.sinc(0.5 * m) * np.kaiser(taps, beta)
    return h / h.sum()


STAGES = [halfband(0.06, -100.0), halfband(0.12, -90.0)]


def up(x, h):
    y = np.zeros(2 * len(x))
    y[::2] = 2 * x
    return np.convolve(y, h)[: 2 * len(x)]


def down(x, h):
    return np.convolve(x, h)[: len(x)][::2]


def worst_alias(name, tone, drive_db, factor):
    f, F = CURVES[name]
    settle, size = 8192, 1 << 16
    n = settle + size + 4096
    t = np.arange(n) / FS
    x = 0.5 * np.sin(2 * np.pi * tone * t) * 10 ** (drive_db / 20)
    if factor == 1:
        y = f(x)
    else:
        y = x
        for s in range(2):
            y = up(y, STAGES[s])
        y = adaa(f, F, y)
        for s in reversed(range(2)):
            y = down(y, STAGES[s])
    seg = y[settle + 2048: settle + 2048 + size]
    i = np.arange(size)
    ph = 2 * np.pi * i / (size - 1)
    w = 0.35875 - 0.48829 * np.cos(ph) + 0.14128 * np.cos(2 * ph) - 0.01168 * np.cos(3 * ph)
    spec = 2 * np.abs(np.fft.rfft(seg * w)) / w.sum()
    hz = np.fft.rfftfreq(size, 1 / FS)
    near = np.round(hz / tone) * tone
    mask = (hz >= 20) & (hz <= 20000) & ~((near > 0) & (np.abs(hz - near) < 6))
    return 20 * math.log10(spec[mask].max() + 1e-30)


print("Item 36, worst alias from 20 Hz to 20 kHz in dBFS (as worstAliasDb, the bare curve, no auto gain), 44.1 kHz, -6 dBFS:")
print("    existing limits (Clip, Drive 24): 1 kHz Off < -38, 1 kHz 4x < -90, 10 kHz 4x < -50")
for drive in (12.0, 24.0):
    for name in CURVES:
        a = worst_alias(name, 1000.0, drive, 1)
        b = worst_alias(name, 1000.0, drive, 4)
        c = worst_alias(name, 10000.0, drive, 4)
        print(f"    Drive {drive:4.0f}, {name:<6}: 1 kHz Off {a:7.1f}, 1 kHz 4x {b:7.1f}, 10 kHz 4x {c:7.1f}")
print("The filters here are stand-ins: Clip at Drive 24, 1 kHz, 4x reads about 7 dB worse than the plugin's own model")
print("(-94.4 dBFS), so compare Diode and Fold with Clip on the same line, not with the limits directly.")
