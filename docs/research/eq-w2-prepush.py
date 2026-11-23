"""Pre-push check for Fancy EQ's Wave 2 items 15 and 16 (docs/eq.md): the numbers each test in
tests/EqW2Tests.cpp asserts, measured the way the test measures them.

1. Brickwall (order 16) high-pass at 100 Hz, minimum phase: the analog Butterworth response,
   which the matched design follows closely at 100 Hz and 48 kHz.
2. Linear phase: the FIR LinearPhase.cpp builds (the magnitude sampled on `length` bins,
   inverse FFT, centred, Blackman window), measured by FFT, against the target magnitude.
3. Character: Sature's soft curve through first-order ADAA, f(d x) / d, a 5 Hz DC blocker after
   Warm; THD, level change and H2 against H3 for a 1 kHz sine, Hann window over the last second.

Run: python docs/research/eq-w2-prepush.py
"""
import math

import numpy as np

RATE = 48000.0


def hp_db(order, f, corner):
    return -10 * math.log10(1 + (corner / f) ** (2 * order))


print("1. Brickwall high-pass at 100 Hz (analog Butterworth, order 16):")
for f in (100 / 2 ** (1 / 3), 100.0, 100 * 2 ** (1 / 3), 200.0, 1000.0):
    print(f"   {f:7.1f} Hz: {hp_db(16, f, 100.0):8.3f} dB")


def linear_fir(corner, order, length):
    half = length // 2
    k = np.arange(half + 1)
    f = k * RATE / length
    with np.errstate(divide="ignore"):
        mag = 1 / np.sqrt(1 + (corner / np.maximum(f, 1e-9)) ** (2 * order))
    mag[0] = 0.0
    h = np.fft.irfft(mag, length)
    h = np.roll(h, half)
    n = np.arange(length)
    phase = 2 * np.pi * n / length
    return h * (0.42 - 0.5 * np.cos(phase) + 0.08 * np.cos(2 * phase))


print("2. Linear phase, measured FIR against the target, where the target is above -60 dB:")
for corner in (100.0, 300.0, 1000.0):
    for length in (8192, 16384):
        h = linear_fir(corner, 16, length)
        n = 1 << 18
        spectrum = np.abs(np.fft.rfft(h, n))
        f = np.arange(len(spectrum)) * RATE / n
        with np.errstate(divide="ignore"):
            target = -10 * np.log10(1 + (corner / np.maximum(f, 1e-9)) ** 32)
        measured = 20 * np.log10(np.maximum(spectrum, 1e-12))
        band = (target > -60) & (f < 20000)
        worst = np.max(np.abs(measured[band] - target[band]))
        print(f"   corner {corner:6.0f} Hz, FIR {length:5d}: worst error {worst:.3f} dB")


# Symmetry: h[half + i] == h[half - i] for the zero-phase design centred on half.
h = linear_fir(1000.0, 16, 8192)
half = 4096
print(f"   symmetry about sample {half}: worst |h[half+i] - h[half-i]| = {np.max(np.abs(h[half + 1:] - h[half - 1:0:-1])):.2e}")


def soft(u, negative):
    a = np.where(u < 0, negative, 1.0)
    r = u / a
    return u / np.sqrt(1 + r * r)


def soft_integral(u, negative):
    a = np.where(u < 0, negative, 1.0)
    r = u / a
    return u * u / (np.sqrt(1 + r * r) + 1)


def character(x, drive_db, negative, rate, dc_block):
    d = 10 ** (drive_db / 20)
    u = d * x
    prev = np.concatenate(([0.0], u[:-1]))
    diff = u - prev
    big = np.abs(diff) >= 1e-6
    y = np.where(big, (soft_integral(u, negative) - soft_integral(prev, negative)) / np.where(big, diff, 1.0),
                 soft(0.5 * (u + prev), negative)) / d
    if dc_block:
        r = math.exp(-2 * math.pi * 5.0 / rate)
        out = np.empty_like(y)
        x1 = y1 = 0.0
        for i, v in enumerate(y):
            y1 = v - x1 + r * y1
            x1 = v
            out[i] = y1
        y = out
    return y


def analyse(y, rate):
    seg = y[-int(rate):]
    w = np.hanning(len(seg))
    s = np.abs(np.fft.rfft(seg * w))
    h = [np.max(s[1000 * k - 2:1000 * k + 3]) for k in range(1, 8)]
    thd = math.sqrt(sum(v * v for v in h[1:])) / h[0]
    return h, thd


print("3. Character on a 1 kHz sine, 2 s, Oversampling Off (ADAA at 48 kHz):")
t = np.arange(int(2 * RATE)) / RATE
for name, drive, negative, dc in (("Subtle", -9.0, 1.0, False), ("Warm", -3.0, 4.0, True)):
    for level in (-18, -12, -6):
        x = 10 ** (level / 20) * np.sin(2 * np.pi * 1000 * t)
        y = character(x, drive, negative, RATE, dc)
        h, thd = analyse(y, RATE)
        h0, _ = analyse(x, RATE)
        change = 20 * math.log10(h[0] / h0[0])
        print(f"   {name:6s} {level:3d} dBFS: level {change:+.3f} dB, THD {100 * thd:6.3f} %, "
              f"H2 {20 * math.log10(h[1] / h[0] + 1e-15):6.1f} dB, H3 {20 * math.log10(h[2] / h[0] + 1e-15):6.1f} dB")

# DC with a full-scale asymmetric input: a half-wave-ish signal, 0.9 positive peaks and -0.9
# negative peaks of a sine plus its square (asymmetric), over 3 s; the mean of the last second.
x = 0.9 * np.sin(2 * np.pi * 100 * np.arange(int(3 * RATE)) / RATE)
y = character(x, -3.0, 4.0, RATE, True)
dc = abs(np.mean(y[-int(RATE):]))
print(f"   Warm DC after the blocker, 0.9 sine at 100 Hz, last second: {20 * math.log10(dc + 1e-20):.1f} dBFS")
y = character(x, -3.0, 4.0, RATE, False)
print(f"   (without the blocker: {20 * math.log10(abs(np.mean(y[-int(RATE):])) + 1e-20):.1f} dBFS)")

print("4. Character at Oversampling 4x (ADAA at 192 kHz; the oversampler's filters are flat at 1 kHz):")
R4 = 4 * RATE
t4 = np.arange(int(2 * R4)) / R4
for name, drive, negative, dc in (("Subtle", -9.0, 1.0, False), ("Warm", -3.0, 4.0, True)):
    for level in (-18, -12, -6):
        x = 10 ** (level / 20) * np.sin(2 * np.pi * 1000 * t4)
        y = character(x, drive, negative, R4, dc)[::4]   # the harmonics are far below 24 kHz
        h, thd = analyse(y, RATE)
        h0, _ = analyse(x[::4], RATE)
        print(f"   {name:6s} {level:3d} dBFS: level {20 * math.log10(h[0] / h0[0]):+.3f} dB, THD {100 * thd:6.3f} %")

# Aliasing: a 10 kHz sine at -6 dBFS through Warm at 192 kHz. What lands below 20 kHz at
# frequencies that aren't harmonics comes from harmonics past 96 kHz folding inside the
# oversampled domain; the decimation filters (90 and 100 dB stopbands) handle 24 to 96 kHz.
n = 1 << 18
x = 10 ** (-6 / 20) * np.sin(2 * np.pi * 10000 * np.arange(n) / R4)
y = character(x, -3.0, 4.0, R4, True)
w = np.hanning(n)
s = np.abs(np.fft.rfft(y * w)) * 4 / n
f = np.arange(len(s)) * R4 / n
mask = (f > 50) & (f < 19800) & (np.abs(f - 10000) > 200)
print(f"5. Warm, 10 kHz at -6 dBFS, 192 kHz: largest non-harmonic component below 19.8 kHz {20 * math.log10(np.max(s[mask]) + 1e-20):.1f} dBFS")
mask2 = (f > 24000) & (f < 96000)
print(f"   largest component between 24 and 96 kHz (the decimation filters take 90 dB or more off it): {20 * math.log10(np.max(s[mask2]) + 1e-20):.1f} dBFS")
