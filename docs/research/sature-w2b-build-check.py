"""Build checks for Fancy Sature's Wave 2 items 35 to 39 (docs/sature.md, "Phase 4 Wave 2: curve
shaping, curve families, cuts, harmonic spread and Mid/Side"), run before the push, measuring what
tests/SatureW2bTests.cpp measures. Plain numpy (no scipy).

1. The Hilbert pair Spread uses (Olli Niemitalo's 90-degree IIR allpass pair): its phase
   difference across 20 Hz to 20 kHz at 48 kHz.
2. Spread on the plugin's own added part. The design model divided the curve's output by Drive,
   so its added part was small; the plugin's isn't (at Drive 12 dB a 0.4 sine's added part carries
   a large share of the fundamental). Rotating all of it would move the fundamental off centre, so
   Spread rotates only the harmonics: the added part minus its best linear fit to the clean signal
   over 50 ms. Measured: the harmonics' L/R correlation at 50 %, and the fundamental's level change.
3. Auto gain for Diode and Fold: the build-time table measures the bare curve at 1x; the plugin runs
   at 4x. Their loudness at 1x and 4x on the table's pink noise, to see whether the 1 LU test holds.
4. Test 4's numbers: intermodulation around 2 kHz in the added part with no cut and a 12 dB/oct Low
   cut at 200 Hz Pre, Oversampling Off, Auto gain off (the plugin's added part isn't divided by Drive).

Run: python docs/research/sature-w2b-build-check.py
"""
import math

import numpy as np

FS = 48000
rng = np.random.default_rng(39)

A = [0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737]
B = [0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278]


def allpass_chain(x, coefs):
    y = np.array(x, dtype=float)
    for c in coefs:
        a = c * c
        out = np.zeros(len(y))
        x1 = x2 = y1 = y2 = 0.0
        for i, v in enumerate(y):
            o = a * (v + y2) - x2
            x2, x1 = x1, v
            y2, y1 = y1, o
            out[i] = o
        y = out
    return y


def hilbert_pair(x):
    a = allpass_chain(x, A)
    b = allpass_chain(x, B)
    a = np.concatenate(([0.0], a[:-1]))   # the first chain's one-sample delay (b then leads by 90 degrees)
    return a, b


# 1 ---------------------------------------------------------------------------------------------
print("1. Phase difference of the Hilbert pair at 48 kHz:")
n = FS
imp = np.zeros(n); imp[0] = 1.0
ha, hb = hilbert_pair(imp)
fa, fb = np.fft.rfft(ha), np.fft.rfft(hb)
f = np.fft.rfftfreq(n, 1 / FS)
diff = np.degrees(np.angle(fb / fa))
worst = 0.0
for hz in (20, 100, 1000, 5000, 10000, 20000):
    print(f"    {hz:6d} Hz: {diff[int(hz)]:7.2f} deg")
print(f"    worst error from 20 Hz to 20 kHz: {max(abs(abs(diff[20:20001]) - 90)):.2f} deg")


def soft(u, a=1.0):
    return u / np.sqrt(1 + (u / a) ** 2)


def one_pole(x, ms):
    c = 1 - math.exp(-1 / (ms * 0.001 * FS))
    out, s = np.empty(len(x)), 0.0
    for i, v in enumerate(x):
        s += c * (v - s)
        out[i] = s
    return out


def remove_tone(x, hz):
    t = np.arange(len(x)) / FS
    basis = np.stack([np.sin(2 * np.pi * hz * t), np.cos(2 * np.pi * hz * t)], 1)
    coef, *_ = np.linalg.lstsq(basis, x, rcond=None)
    return x - basis @ coef, math.hypot(*coef)


# 2 ---------------------------------------------------------------------------------------------
print("2. Spread on a mono 220 Hz sine at 0.4, Tape, Drive 12 dB, Auto gain off, Oversampling Off:")
secs = 1.5
t = np.arange(int(secs * FS)) / FS
x = 0.4 * np.sin(2 * np.pi * 220 * t)
added = soft(10 ** (12 / 20) * x) - x
k = one_pole(added * x, 50.0) / np.maximum(one_pole(x * x, 50.0), 1e-12)
h = added - k * x
ha, hb = hilbert_pair(h)
settle = FS // 2
for spread in (0.0, 0.25, 0.5):
    th = spread * np.pi / 2
    if spread == 0:
        left_added = right_added = added
    else:
        left_added = k * x + math.cos(th) * ha + math.sin(th) * hb
        right_added = k * x + math.cos(th) * ha - math.sin(th) * hb
    left, right = x + left_added, x + right_added
    lh, l1 = remove_tone(left[settle:], 220)
    rh, r1 = remove_tone(right[settle:], 220)
    corr = np.mean(lh * rh) / math.sqrt(np.mean(lh ** 2) * np.mean(rh ** 2))
    if spread == 0:
        base = l1
    print(f"    Spread {int(spread * 100):3d} %: harmonics' L/R correlation {corr:+.3f}, fundamental L {20 * math.log10(l1 / base):+.3f} dB, "
          f"R {20 * math.log10(r1 / base):+.3f} dB")


# 3 ---------------------------------------------------------------------------------------------
def pink(seconds):
    white = rng.uniform(-1, 1, int(seconds * FS) + FS // 2)
    b = [0.0] * 7
    out = np.empty(len(white))
    for i, w in enumerate(white):
        b[0] = 0.99886 * b[0] + w * 0.0555179
        b[1] = 0.99332 * b[1] + w * 0.0750759
        b[2] = 0.96900 * b[2] + w * 0.1538520
        b[3] = 0.86650 * b[3] + w * 0.3104856
        b[4] = 0.55000 * b[4] + w * 0.5329522
        b[5] = -0.7616 * b[5] - w * 0.0168980
        out[i] = sum(b[:6]) + b[6] + w * 0.5362
        b[6] = w * 0.115926
    out = out[FS // 2:]
    out -= out.mean()
    return out * 0.125892541179416721 / math.sqrt(np.mean(out ** 2))


def k_weight_db(x, fs):
    # BS.1770's K weighting through its two biquads, designed at fs (the standard's formulas).
    import cmath  # noqa: F401
    def biquad(sig, b, a):
        y = np.zeros(len(sig)); x1 = x2 = y1 = y2 = 0.0
        for i, v in enumerate(sig):
            o = b[0] * v + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2
            x2, x1, y2, y1 = x1, v, y1, o
            y[i] = o
        return y
    f0, G, Q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    K = math.tan(math.pi * f0 / fs); Vh = 10 ** (G / 20); Vb = Vh ** 0.4996667741545416
    a0 = 1 + K / Q + K * K
    b1 = [(Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0]
    a1 = [1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0]
    f0, Q = 38.13547087602444, 0.5003270373238773
    K = math.tan(math.pi * f0 / fs)
    a0 = 1 + K / Q + K * K
    b2 = [1, -2, 1]
    a2 = [1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0]
    y = biquad(biquad(x, b1, a1), b2, a2)
    return 10 * math.log10(np.mean(y ** 2) + 1e-30)


def fold(u):
    return np.sin(np.pi / 2 * u)


def diode(u):
    return np.where(u > 0, soft(u), 0.1 * soft(u))


def up4(x):
    # Band-limited 4x by FFT zero padding (an ideal oversampler).
    X = np.fft.rfft(x)
    Y = np.zeros(len(x) * 4 // 2 + 1, complex)
    Y[:len(X)] = X
    return np.fft.irfft(Y, len(x) * 4) * 4


def down4(y):
    Y = np.fft.rfft(y)
    n = len(y) // 4
    return np.fft.irfft(Y[:n // 2 + 1], n) / 4


print("3. Loudness after the bare curve at 1x (the table) and band-limited at 4x, pink noise at -18 dBFS RMS:")
noise = pink(2.0)
ref = k_weight_db(noise, FS)
for name, curve in (("Diode", diode), ("Fold", fold)):
    for drive in (12, 24, 36):
        g = 10 ** (drive / 20)
        y1 = curve(g * noise)
        y1 = y1 - y1.mean()
        y4 = down4(curve(g * up4(noise)))
        y4 = y4 - y4.mean()
        l1, l4 = k_weight_db(y1, FS) - ref, k_weight_db(y4, FS) - ref
        print(f"    {name:<5} Drive {drive:2d}: 1x {l1:+6.2f} dB, 4x {l4:+6.2f} dB, so 4x after the table's gain is {l4 - l1:+.2f} LU")


# 4 ---------------------------------------------------------------------------------------------
def svf_hp(sig, hz, q):
    g = math.tan(math.pi * hz / FS); k = 1 / q
    a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2
    ic1 = ic2 = 0.0
    out = np.empty(len(sig))
    for i, v in enumerate(sig):
        v3 = v - ic2
        v1 = a1 * ic1 + a2 * v3
        v2 = ic2 + a2 * ic1 + a3 * v3
        ic1, ic2 = 2 * v1 - ic1, 2 * v2 - ic2
        out[i] = v - k * v1 - v2
    return out


print("4. Intermodulation around 2 kHz in the added part (80 Hz at -6 dBFS + 2 kHz at -18), Tape, Drive 12:")
t = np.arange(int(1.5 * FS)) / FS
xin = 10 ** (-6 / 20) * np.sin(2 * np.pi * 80 * t) + 10 ** (-18 / 20) * np.sin(2 * np.pi * 2000 * t)
results = {}
for name, driven in (("no cut", xin), ("Low cut 200 Hz Pre", svf_hp(xin, 200, 0.7071))):
    add = soft(10 ** (12 / 20) * driven) - driven
    seg = add[FS // 2:]
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    ff = np.fft.rfftfreq(len(seg), 1 / FS)
    im = np.sum(spec[(ff > 1800) & (ff < 2200) & (np.abs(ff - 2000) > 5)])
    results[name] = 10 * math.log10(im + 1e-30)
    print(f"    {name:<20} {results[name]:7.2f} dB")
print(f"    drop {results['no cut'] - results['Low cut 200 Hz Pre']:.1f} dB (test: at least 12)")
