"""Model of Fancy Comp's Q2.5 Character stage and Auto threshold (docs/comp.md, Phase 3 Q2.5).

Pure Python, so it runs anywhere. It mirrors CompDsp::CharacterStage: an emphasis tilt, Drive,
Sature's curves with first-order ADAA, the curve's added part (shaped minus its own two-sample
average) brought back through the inverse tilt, and Warm's DC blocker. The oversampler is ideal:
the test tone is generated at the oversampled rate and only components that would land below the
base rate's Nyquist are read, so a real build can only be worse by its filters' leakage.

Run: python docs/research/comp-q25-model.py
"""

import math

# --- Sature's curves (fancy-core/include/fancy/dsp/Saturation.h) ----------------------------

TUBE_NEGATIVE = 1.4
TRANSISTOR_KNEE = 0.5


def soft(u, a):
    r = u / a
    return u / math.sqrt(1.0 + r * r)


def soft_int(u, a):
    r = u / a
    return u * u / (math.sqrt(1.0 + r * r) + 1.0)


def knee(u, k):
    a = abs(u)
    end = 2.0 - k
    if a <= k:
        y = a
    elif a >= end:
        y = 1.0
    else:
        y = 1.0 - (end - a) * (end - a) / (4.0 * (1.0 - k))
    return math.copysign(y, u)


def knee_int(u, k):
    a = abs(u)
    end = 2.0 - k
    w = 1.0 - k
    lin = min(a, k)
    r = 0.5 * lin * lin
    if a > k:
        t = min(a, end)
        span = end - k
        rest = end - t
        r += (t - k) - (span ** 3 - rest ** 3) / (12.0 * w)
    if a > end:
        r += a - end
    return r


def curve(kind):
    if kind == "warm":
        return (lambda u: soft(u, TUBE_NEGATIVE if u < 0 else 1.0),
                lambda u: soft_int(u, TUBE_NEGATIVE if u < 0 else 1.0))
    return (lambda u: knee(u, TRANSISTOR_KNEE), lambda u: knee_int(u, TRANSISTOR_KNEE))


class Tilt:
    """Saturation.h's TiltCoefficients and Tilt."""

    def __init__(self, total_db, rate):
        self.flat = total_db == 0.0
        g = 10.0 ** (total_db / 40.0)
        w = math.tan(math.pi * min(1000.0, 0.45 * rate) / rate)
        a0 = 1.0 + w * g
        self.b0 = (g + w) / a0
        self.b1 = (w - g) / a0
        self.a1 = (w * g - 1.0) / a0
        self.s = 0.0

    def __call__(self, x):
        if self.flat:
            return x
        y = self.b0 * x + self.s
        self.s = self.b1 * x - self.a1 * y
        return y


def character(xs, kind, drive_db, rate):
    shape, integral = curve(kind)
    tilt = 4.0 if kind == "bright" else -4.0
    pre, post = Tilt(tilt, rate), Tilt(-tilt, rate)
    d = 10.0 ** (drive_db / 20.0)
    dc_coef = math.exp(-2.0 * math.pi * 5.0 / rate)
    prev_u = prev_int = 0.0
    dc_in = dc_out = 0.0
    out = []
    for x in xs:
        u = d * pre(x)
        f = integral(u)
        diff = u - prev_u
        shaped = shape(0.5 * (u + prev_u)) if abs(diff) < 1e-6 else (f - prev_int) / diff
        linear = 0.5 * (u + prev_u)
        prev_u, prev_int = u, f
        added = post((shaped - linear) / d)
        if kind == "warm":
            y = added - dc_in + dc_coef * dc_out
            dc_in, dc_out = added, y
            added = y
        out.append(x + added)
    return out


def level_db(xs, start, hz, rate):
    """Windowed single-bin DFT, as CompTests' levelAt."""
    n = len(xs) - start
    re = im = wsum = 0.0
    for i in range(n):
        w = 0.5 - 0.5 * math.cos(2.0 * math.pi * i / n)
        ph = 2.0 * math.pi * hz * i / rate
        v = xs[start + i] * w
        re += v * math.cos(ph)
        im -= v * math.sin(ph)
        wsum += w
    a = 2.0 * math.hypot(re, im) / wsum
    return 20.0 * math.log10(max(a, 1e-12))


def sine(hz, amp, rate, n):
    return [amp * math.sin(2.0 * math.pi * hz * i / rate) for i in range(n)]


def harmonics(kind, drive_db):
    rate = 48000.0 * 4
    xs = sine(1000.0, 1.0, rate, 19200)
    ys = character(xs, kind, drive_db, rate)
    return [level_db(ys, 4800, 1000.0 * k, rate) for k in (1, 2, 3)]


def worst_alias(kind, drive_db, factor, base=44100.0, hz=10000.0, amp=1.0):
    rate = base * factor
    n = int(rate * 0.05)
    xs = sine(hz, amp, rate, n)
    ys = character(xs, kind, drive_db, rate)
    worst = -300.0
    for k in range(2, 80):
        f = (k * hz) % rate
        f = min(f, rate - f)
        if f >= base / 2 or f < 20.0:
            continue
        if any(abs(f - j * hz) < 1.0 for j in range(1, 3)):
            continue
        worst = max(worst, level_db(ys, n // 4, f, rate))
    return worst


def small_signal(kind, drive_db, factor):
    rate = 48000.0 * factor
    worst = 0.0
    for hz in (20.0, 100.0, 1000.0, 10000.0, 20000.0):
        n = int(rate * max(0.05, 8.0 / hz))
        xs = sine(hz, 0.001, rate, n)
        ys = character(xs, kind, drive_db, rate)
        worst = max(worst, abs(level_db(ys, n // 4, hz, rate) - level_db(xs, n // 4, hz, rate)))
    return worst


def auto_threshold():
    """Settling of the long-term level after a 12 dB step, as the DSP runs it (3 s one-pole on the
    mean square, gate at -60 dBFS)."""
    rate = 48000.0
    coef = 1.0 - math.exp(-1.0 / (3.0 * rate))
    p = 10 ** (-18 / 10)
    target = 10 ** (-6 / 10)
    t = 0
    rows = []
    for seconds in (1, 3, 6, 9, 12, 15):
        while t < seconds * rate:
            p += coef * (target - p)
            t += 1
        rows.append((seconds, 10 * math.log10(p) + 18))
    return rows


if __name__ == "__main__":
    print("Harmonics, 1 kHz at 0 dBFS, 4x (fundamental, 2nd, 3rd, dBFS):")
    for kind in ("warm", "bright"):
        for drive in (0.0, 12.0, 24.0):
            print(f"  {kind:6s} drive {drive:4.0f}: " + ", ".join(f"{v:7.1f}" for v in harmonics(kind, drive)))

    print("Small signal, -60 dBFS, worst change 20 Hz-20 kHz (dB):")
    for kind in ("warm", "bright"):
        for factor in (1, 4):
            print(f"  {kind:6s} x{factor}: {small_signal(kind, 24.0, factor):.4f}")

    print("Worst alias below 22.05 kHz, 10 kHz sine at 44.1 kHz, ideal oversampler (dBFS):")
    for amp_db in (0.0, -6.0):
        for kind in ("warm", "bright"):
            for drive in (12.0, 24.0):
                row = [worst_alias(kind, drive, f, amp=10 ** (amp_db / 20)) for f in (1, 4, 8)]
                print(f"  {amp_db:4.0f} dBFS {kind:6s} drive {drive:4.0f}: off {row[0]:7.1f}  4x {row[1]:7.1f}  8x {row[2]:7.1f}")

    print("Auto threshold: offset after a step from -18 to -6 dBFS RMS (target +12 dB):")
    for seconds, offset in auto_threshold():
        print(f"  {seconds:2d} s: {offset:+.2f} dB")
