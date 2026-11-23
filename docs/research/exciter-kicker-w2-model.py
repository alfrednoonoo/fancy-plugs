"""Models for Fancy Exciter's Wave 2 items (docs/exciter.md, items 40 to 44) and Fancy Kicker's
item 45 (docs/kicker.md).

40. Two-band exciter: a second, lower band (Freq 2 and Harmonics 2 below a split) beside today's.
    Measured on a vocal-like tone stack: where the added harmonics land with one band at 3 kHz
    and with a second band at 400 Hz (warmth) added.
41. Transient emphasis (Punch): the harmonics' level follows a transient detector, so attacks get
    more and sustains less. Measured on a plucked note: the added part's level in the first 20 ms
    against 100-300 ms, at Punch 0 and 100 %.
43. Harmonics width: the added harmonics' side scaled, as Sature's Spread question (item 38): what
    a mono fold-down keeps of the added part at 100, 150 and 200 %.
44. Minimum-phase oversampling. Exciter adds its excited path to a delayed dry path. That path
    carries the input above Freq as well as the new harmonics (the shaper has unity gain for
    small signals), so if its oversampling filters aren't linear-phase, their phase shift against
    the dry delay combs the sum. Measured: the small-signal response of dry + wet (Harmonics 100 %,
    Mix Back -6 dB) above Freq, with the wet path through a minimum-phase low-pass (an 8th-order
    Butterworth at 0.45 of the base rate, where the last half-band stage cuts; a stand-in) and the dry delayed by its
    best constant match, against linear phase (flat by construction).
45. Kicker's soft clipper, oversampled: the alias energy a 0 dBFS kick's click makes through the
    clipper at 1x, 2x and 4x.

Run: py -3 docs/research/exciter-kicker-w2-model.py
"""
import math

import numpy as np

FS = 48000
N = FS
t = np.arange(N) / FS
rng = np.random.default_rng(41)


def shaper(x):
    y = x - 4 * x ** 3 / 27
    return np.where(np.abs(x) > 1.5, np.sign(x), y)


def hp(sig, hz, fs=FS):
    g = math.tan(math.pi * hz / fs)
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


def band_energy(y, lo, hi):
    spec = np.abs(np.fft.rfft(y * np.hanning(len(y)))) ** 2
    f = np.fft.rfftfreq(len(y), 1 / FS)
    return 10 * math.log10(np.sum(spec[(f >= lo) & (f < hi)]) / len(y) + 1e-30)


# 40 -------------------------------------------------------------------------------------------
voice = sum(np.sin(2 * np.pi * 150 * h * t) / h for h in range(1, 20)) * 0.3
drive = 10 ** (12 / 20)
high_band = hp(shaper(drive * hp(voice, 3000)) / drive, 3000) - hp(hp(voice, 3000), 3000)
low_in = hp(voice, 400) - hp(voice, 3000)                     # 400 Hz to the split at 3 kHz
low_band = shaper(drive * low_in) / drive - low_in
print("40. Where the added harmonics land, a 150 Hz voice-like tone stack (drive 12 dB):")
for name, added in (("one band at 3 kHz (today)", high_band), ("plus a second band, 400 Hz to 3 kHz", high_band + low_band)):
    print(f"    {name:<38} 400 Hz-3 kHz {band_energy(added, 400, 3000):6.1f} dB, above 3 kHz {band_energy(added, 3000, 20000):6.1f} dB")

# 41 -------------------------------------------------------------------------------------------
n = FS // 2
tt = np.arange(n) / FS
pluck = sum(np.sin(2 * np.pi * 220 * h * tt) * np.exp(-tt / (0.25 / h)) / h for h in range(1, 12)) * 0.5
excited = hp(pluck, 2000)


def follower(x, a_ms, r_ms):
    a = 1 - math.exp(-1 / (a_ms * 0.001 * FS)); r = 1 - math.exp(-1 / (r_ms * 0.001 * FS))
    env, out = 0.0, np.empty(len(x))
    for i, v in enumerate(np.abs(x)):
        env += (v - env) * (a if v > env else r)
        out[i] = env
    return out


fast, slow = follower(excited, 0.5, 10), follower(excited, 20, 200)
transient = np.clip(20 * np.log10((fast + 1e-9) / (slow + 1e-9)) / 12, 0, 1)   # 0 to 1 over a 12 dB jump
print("41. Punch on a plucked 220 Hz note (the added part's level):")
for punch in (0.0, 1.0):
    weight = 1 + punch * (2 * transient - 0.5)                       # +6 dB on attacks, -6 dB in sustains at 100 %
    added = (shaper(drive * excited) / drive - excited) * np.clip(weight, 0.5, 2.0)
    att = 10 * math.log10(np.mean(added[:int(0.02 * FS)] ** 2) + 1e-30)
    sus = 10 * math.log10(np.mean(added[int(0.1 * FS):int(0.3 * FS)] ** 2) + 1e-30)
    print(f"    Punch {int(punch * 100):3d} %: attack (first 20 ms) {att:6.1f} dB, sustain (100-300 ms) {sus:6.1f} dB, difference {att - sus:5.1f} dB")

# 43 -------------------------------------------------------------------------------------------
print("43. Harmonics width on a stereo source whose added part is 30 % correlated:")
a = rng.standard_normal(N); b = rng.standard_normal(N); c = rng.standard_normal(N)
l_add, r_add = math.sqrt(0.3) * a + math.sqrt(0.7) * b, math.sqrt(0.3) * a + math.sqrt(0.7) * c
m0, s0 = 0.5 * (l_add + r_add), 0.5 * (l_add - r_add)
for w in (1.0, 1.5, 2.0):
    l, r = m0 + w * s0, m0 - w * s0
    corr = np.mean(l * r) / math.sqrt(np.mean(l ** 2) * np.mean(r ** 2))
    side_gain = 10 * math.log10(np.mean(l ** 2) / np.mean(l_add ** 2))
    print(f"    Width {int(w * 100)} %: each side {side_gain:+.1f} dB, mono fold-down unchanged, correlation {corr:+.2f}")

# 44 -------------------------------------------------------------------------------------------
f = np.linspace(2000, 20000, 400)
fs4 = 4 * FS
s = 2j * np.pi * f / (2 * np.pi * 0.45 * FS)                        # the final half-band cuts at the base rate's Nyquist
qs = [1 / (2 * math.cos(math.pi * (2 * k + 1) / 16)) for k in range(4)]
h = np.ones_like(s)
for q in qs:
    h = h / (s * s + s / q + 1)
h = h * h                                                            # up and down: twice through
phase = np.unwrap(np.angle(h))
delay = -np.polyfit(2 * np.pi * f, phase, 1)[0]                      # the best constant delay
residual = phase + 2 * np.pi * f * delay
wet = 10 ** (-6 / 20)
mag_min = 20 * np.log10(np.abs(1 + wet * np.abs(h) * np.exp(1j * residual)))
print("44. Dry plus excited path (small signals, Harmonics 100 %, Mix Back -6 dB), 2-20 kHz:")
print(f"    linear phase (today): flat at {20 * math.log10(1 + wet):+.2f} dB")
print(f"    minimum phase, dry delayed by its best match: {mag_min.min():+.2f} to {mag_min.max():+.2f} dB "
      f"(residual phase up to {math.degrees(np.max(np.abs(residual))):.1f} degrees)")

# 45 -------------------------------------------------------------------------------------------
print("45. Kicker's soft clipper on a 0 dBFS kick with a 3 ms click, alias energy above the click's content:")
n = FS // 4
tk = np.arange(n) / FS
kick = np.sin(2 * np.pi * 55 * tk) * np.exp(-tk / 0.12) + 1.2 * rng.standard_normal(n) * np.exp(-tk / 0.0015) * (tk < 0.003)
kick *= 1.6


def clip(x):
    return np.tanh(x)


for factor in (1, 2, 4):
    up = np.fft.irfft(np.fft.rfft(kick), n * factor) * factor          # ideal upsampling
    y = clip(up)
    spec = np.fft.rfft(y)
    down = np.fft.irfft(spec[: n // 2 + 1], n) / factor                # ideal decimation, aliases removed above
    ideal = clip(np.fft.irfft(np.fft.rfft(kick), n * 16) * 16)
    ref = np.fft.irfft(np.fft.rfft(ideal)[: n // 2 + 1], n) / 16
    err = down - ref
    print(f"    {factor}x: alias and error {10 * math.log10(np.mean(err ** 2) / np.mean(ref ** 2) + 1e-30):6.1f} dB under the signal")
