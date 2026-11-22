"""Model of Fancy TS's engine (docs/ts.md, draft). A level-independent transient shaper:

  detector level L (dB): a peak envelope of the linked, filtered input (instant up, 10 ms down),
  so the waveform's own ripple isn't read as transients;
  fast follower F: attack 0.1 ms, release 20 ms;  slow follower S: attack Speed, release 20 ms;
  peak memory P: up at once, down 6 dB a second (how far below the hit the signal now is);
  transient amount  a = clamp((F - S - deadband) / span, 0, 1)
  sustain amount    s = clamp((P - F - deadband) / span, 0, 1) x w,
  w = 1 at a hit, falling to 0 over Length in Contour's shape (Sharp: fast then slow; Medium:
  linear; Smooth: slow then fast), so Length is how long after a hit Sustain acts;
  span = 12 dB at Sensitivity 0 % down to 3 dB at 100 %;
  gain (dB) = Attack x a + Sustain x s - Pump x 12 dB x p x (1 - a),
  p the pump envelope: charged by a, decaying over Length.

Checks, at 48 kHz, Speed 8 ms, Length 180 ms, Sensitivity 60 %:
  1. A kick (5 ms click into a 120 ms 60 Hz body): the gain over the first 10 ms and at 100 ms, for
     Attack +6 / Sustain 0 and Attack 0 / Sustain -6.
  2. Level independence: the same kick at -30 dBFS and -6 dBFS gives the same gain curve.
  3. A steady sine: no gain change once settled (no transient, no tail).
  4. Contour: the sustain gain at 60 and 150 ms for Sharp, Medium and Smooth.
  5. Pump 100 %: the dip after the hit.

Run: py -3 docs/research/ts-model.py
"""
import math

import numpy as np

RATE = 48000.0


def coef(ms):
    return 1.0 - math.exp(-1.0 / max(1e-9, ms * 0.001 * RATE))


def kick(level_db, seconds=0.6):
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    rng = np.random.default_rng(2)
    x = np.zeros(n)
    start = int(0.1 * RATE)
    k = t[: n - start]
    body = np.sin(2 * np.pi * 60 * k) * np.exp(-k / 0.12)
    click = rng.standard_normal(len(k)) * np.exp(-k / 0.0015) * (k < 0.005)
    x[start:] = 0.5 * body + 0.8 * click
    return x * 10 ** (level_db / 20) / np.max(np.abs(x)), start


def shaper(x, attack_db=0.0, sustain_db=0.0, pump=0.0, speed_ms=8.0, length_ms=180.0, sensitivity=0.6,
           contour="medium", deadband_db=1.0):
    span = 12.0 - 9.0 * sensitivity
    env = 0.0
    fast = slow = peak = -120.0
    p, since = 0.0, 10 ** 9
    gains = np.zeros(len(x))
    length = length_ms * 0.001 * RATE
    down = 6.0 / RATE
    for i, v in enumerate(x):
        env = abs(v) if abs(v) > env else env + (abs(v) - env) * coef(10.0)
        level = 20 * math.log10(max(env, 1e-6))
        fast += (coef(0.1) if level > fast else coef(20.0)) * (level - fast)
        slow += (coef(speed_ms) if level > slow else coef(20.0)) * (level - slow)
        peak = max(level, peak - down)
        a = min(1.0, max(0.0, (fast - slow - deadband_db) / span))
        if a > 0.0:
            since = 0
        else:
            since += 1
        u = min(1.0, since / length)
        w = {"sharp": math.exp(-5 * u) * (1 - u),
             "medium": 1 - u,
             "smooth": 0.5 + 0.5 * math.cos(math.pi * u)}[contour]
        s = min(1.0, max(0.0, (peak - fast - deadband_db) / span)) * w
        p = max(p * (1 - 1 / length), a)
        gains[i] = attack_db * a + sustain_db * s - pump * 12.0 * p * (1 - a)
    return gains


x, start = kick(-6.0)
ms = lambda m: start + int(m * 0.001 * RATE)
g = shaper(x, attack_db=6.0)
print(f"1. Attack +6: peak {g[start:ms(10)].max():+.1f} dB in the first 10 ms, {g[ms(100)]:+.2f} dB at 100 ms")
g = shaper(x, sustain_db=-6.0)
print(f"   Sustain -6: {g[ms(2)]:+.2f} dB at 2 ms, {g[ms(100)]:+.1f} dB at 100 ms")

quiet, _ = kick(-30.0)
g1, g2 = shaper(x, attack_db=6.0, sustain_db=-6.0), shaper(quiet, attack_db=6.0, sustain_db=-6.0)
print(f"2. -6 against -30 dBFS: largest gain difference {np.max(np.abs(g1 - g2)):.2f} dB")

t = np.arange(int(1.0 * RATE)) / RATE
sine = 0.5 * np.sin(2 * np.pi * 440 * t)
g = shaper(sine, attack_db=12.0, sustain_db=12.0)
print(f"3. steady sine, Attack and Sustain +12: gain after 0.5 s between {g[int(0.5 * RATE):].min():+.2f} and {g[int(0.5 * RATE):].max():+.2f} dB")

for contour in ("sharp", "medium", "smooth"):
    g = shaper(x, sustain_db=-6.0, contour=contour)
    print(f"4. Contour {contour:6}: sustain gain {g[ms(60)]:+.1f} dB at 60 ms, {g[ms(150)]:+.1f} dB at 150 ms")

g = shaper(x, pump=1.0)
print(f"5. Pump 100 %: deepest {g[start:ms(300)].min():+.1f} dB, at {(np.argmin(g[start:ms(300)])) / RATE * 1000:.0f} ms after the hit; "
      f"{g[ms(250)]:+.1f} dB at 250 ms")
