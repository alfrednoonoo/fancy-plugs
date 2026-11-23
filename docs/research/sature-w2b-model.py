"""Models for Fancy Sature's later Wave 2 items (docs/sature.md, Phase 4 Wave 2: items 35 to 39).

35. Curve shaping: Offset (a bias added before the curve and taken out after, so a symmetric curve
    makes even harmonics) and Knee (how soft the bend into saturation is). Measured on Tape at a
    fixed drive: the 2nd and 3rd harmonics against Offset, and THD against Knee.
36. Two curve families: Diode (half-wave, even harmonics only) and Fold (a wavefolder: past the
    ceiling the wave folds back, adding high harmonics). Measured: the harmonic series of each,
    against Tape and Clip, at -6 dBFS with drive 12 dB.
37. Low cut and High cut around the saturation (pre: shapes what's driven; post: shapes what's
    heard). Measured: with a bass-heavy input, how much of the added distortion sits below 200 Hz
    with Low cut pre at 200 Hz against none.
38. Harmonic spread: the added harmonics (output minus clean) get a per-channel phase offset, so
    a mono source's saturation spreads wide while its fundamental stays centred. Measured: the
    correlation of the added part between left and right, and of the whole signal.
39. Mid/Side: saturate the mid only, the side only, or both. Measured on a mix with a centre bass
    and wide pads: the added distortion's share in mid and side.

Run: py -3 docs/research/sature-w2b-model.py
"""
import math

import numpy as np

FS = 48000
N = FS
t = np.arange(N) / FS
rng = np.random.default_rng(31)


def soft(u, a=1.0):
    return u / np.sqrt(1 + (u / a) ** 2)


def harmonics(y, f0=1000, count=7):
    spec = np.abs(np.fft.rfft(y * np.hanning(len(y))))
    h = np.array([spec[f0 * k] for k in range(1, count + 1)])
    return 20 * np.log10(h / h[0] + 1e-12)


def thd(y, f0=1000):
    spec = np.abs(np.fft.rfft(y * np.hanning(len(y))))
    h = np.array([spec[f0 * k] for k in range(1, 15)])
    return 100 * math.sqrt(np.sum(h[1:] ** 2)) / h[0]


x = 10 ** (-6 / 20) * np.sin(2 * np.pi * 1000 * t)
drive = 10 ** (12 / 20)

# 35 -------------------------------------------------------------------------------------------
print("35. Curve shaping on Tape, -6 dBFS at drive 12 dB:")
for offset in (0.0, 0.1, 0.25):
    u = drive * x + offset
    y = (soft(u) - soft(np.array([offset]))[0]) / drive
    y -= y.mean()
    h = harmonics(y)
    print(f"    Offset {offset:4.2f}: H2 {h[1]:6.1f} dB, H3 {h[2]:6.1f} dB")
for knee in (0.5, 1.0, 2.0):
    # Knee: the soft curve's sharpness, u / (1 + |u|^p)^(1/p); p = 2 is Tape, higher is harder.
    p = 2.0 * knee
    u = drive * x
    y = u / (1 + np.abs(u) ** p) ** (1 / p) / drive
    print(f"    Knee {knee:3.1f} (p = {p:.1f}): THD {thd(y):5.2f} %, H3 {harmonics(y)[2]:6.1f} dB, H5 {harmonics(y)[4]:6.1f} dB")

# 36 -------------------------------------------------------------------------------------------
print("36. Curve families at -6 dBFS, drive 12 dB (each harmonic against the fundamental):")
u = drive * x
curves = {
    "Tape (soft)": soft(u) / drive,
    "Clip (hard)": np.clip(u, -1, 1) / drive,
    "Diode (half-wave)": np.where(u > 0, soft(u), 0.1 * soft(u)) / drive,
    "Fold": np.sin(np.pi / 2 * u) / drive,
}
for name, y in curves.items():
    y = y - y.mean()
    h = harmonics(y)
    print(f"    {name:<18} H2 {h[1]:6.1f}  H3 {h[2]:6.1f}  H4 {h[3]:6.1f}  H5 {h[4]:6.1f}  H7 {h[6]:6.1f} dB, THD {thd(y):5.1f} %")

# 37 -------------------------------------------------------------------------------------------
print("37. Low cut before the curve, bass-heavy input (80 Hz at -6 dBFS plus 2 kHz at -18), drive 12 dB:")
xin = 10 ** (-6 / 20) * np.sin(2 * np.pi * 80 * t) + 10 ** (-18 / 20) * np.sin(2 * np.pi * 2000 * t)


def hp(sig, hz):
    g = math.tan(math.pi * hz / FS)
    k = math.sqrt(2)
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


for name, driven in (("no cut", xin), ("Low cut 200 Hz, pre", hp(xin, 200))):
    added = soft(drive * driven) / drive - driven
    spec = np.abs(np.fft.rfft(added * np.hanning(N))) ** 2
    f = np.fft.rfftfreq(N, 1 / FS)
    im = np.sum(spec[(f > 1800) & (f < 2200) & (np.abs(f - 2000) > 5)])   # intermodulation around 2 kHz
    total = np.sum(spec)
    print(f"    {name:<22} added distortion {10 * math.log10(total / N):6.1f} dB; intermodulation around 2 kHz {10 * math.log10(im / N + 1e-30):6.1f} dB")

# 38 -------------------------------------------------------------------------------------------
print("38. Harmonic spread on a mono source (a 220 Hz tone with harmonics), drive 12 dB:")
mono = 0.4 * np.sin(2 * np.pi * 220 * t)
added = soft(drive * mono) / drive - mono
spec = np.fft.rfft(added)
for spread in (0.0, 0.5, 1.0):
    # Rotate the added part's phase by +/- spread x 90 degrees per channel (a Hilbert pair).
    shift = np.exp(1j * np.pi / 2 * spread)
    left_added = np.fft.irfft(spec * shift, N)
    right_added = np.fft.irfft(spec * np.conj(shift), N)
    left, right = mono + left_added, mono - 0 + right_added
    corr_added = np.mean(left_added * right_added) / math.sqrt(np.mean(left_added ** 2) * np.mean(right_added ** 2))
    corr_all = np.mean(left * right) / math.sqrt(np.mean(left ** 2) * np.mean(right ** 2))
    mono_sum = 10 * math.log10(np.mean((0.5 * (left_added + right_added)) ** 2) / np.mean(added ** 2))
    print(f"    Spread {int(spread * 100):3d} %: added part's correlation {corr_added:+.2f}, whole signal {corr_all:+.2f}, "
          f"added part in a mono fold-down {mono_sum:+.1f} dB")

# 39 -------------------------------------------------------------------------------------------
print("39. Mid/Side, a centre bass (-8 dBFS) and wide pads (-14 dBFS side), drive 12 dB:")
bass = 10 ** (-8 / 20) * np.sin(2 * np.pi * 60 * t)
pads = 10 ** (-14 / 20) * (np.sin(2 * np.pi * 440 * t) + np.sin(2 * np.pi * 554 * t)) * 0.5
mid, side = bass, pads
for name, m_on, s_on in (("both (L/R, today)", None, None), ("mid only", True, False), ("side only", False, True)):
    if m_on is None:
        left, right = mid + side, mid - side
        l = soft(drive * left) / drive
        r = soft(drive * right) / drive
        m_out, s_out = 0.5 * (l + r), 0.5 * (l - r)
    else:
        m_out = soft(drive * mid) / drive if m_on else mid
        s_out = soft(drive * side) / drive if s_on else side
    m_add = 10 * math.log10(np.mean((m_out - mid) ** 2) + 1e-30)
    s_add = 10 * math.log10(np.mean((s_out - side) ** 2) + 1e-30)
    print(f"    {name:<18} distortion added in the mid {m_add:6.1f} dB, in the side {s_add:6.1f} dB")
