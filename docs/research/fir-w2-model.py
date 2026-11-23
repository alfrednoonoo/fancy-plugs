"""Models for Fancy FIR's Wave 2 items (docs/fir.md, Phase 4 Wave 2: items 27, 28 and 29).

27. A ratio curve for Compressor mode. Today one Ratio applies at every frequency above the
    threshold curve. A second curve, Ratio per frequency, lets a mix's harsh 2-5 kHz be held at
    4:1 while the lows pass at 1:1. Measured on a mix whose upper mids get loud in a chorus: the
    reduction at 3 kHz and at 100 Hz, in the verse and the chorus, with one ratio (4:1) and with
    the curve (4:1 from 2 to 5 kHz, 1:1 elsewhere).

29. Attack and release in EQ mode, "when the curve is a threshold": a dynamic EQ per frequency.
    With item 27, Compressor mode with a ratio curve already is one: a cut that appears only where
    and while a bin is over its threshold, with Attack and Release. The model shows the same run
    as 27 is exactly that: 3 kHz turned down only in the loud chorus.

28. A Mid/Side mode: the spectral process runs on mid and side instead of left and right, with a
    curve for each. Measured on a stereo recording whose hiss sits mostly in the side (a wide room
    mic): Subtract in L/R against Subtract on the side only, the hiss's reduction and how much the
    centre (the voice) changes.

Run: py -3 docs/research/fir-w2-model.py
"""
import math

import numpy as np

FS = 48000
N, HOP = 2048, 512
WINDOW = np.hanning(N)
rng = np.random.default_rng(12)


def stft(x):
    frames = [x[i:i + N] * WINDOW for i in range(0, len(x) - N, HOP)]
    return np.array([np.fft.rfft(f) for f in frames])


def istft(spectra, length):
    out = np.zeros(length)
    norm = np.zeros(length)
    for k, spec in enumerate(spectra):
        i = k * HOP
        out[i:i + N] += np.fft.irfft(spec) * WINDOW
        norm[i:i + N] += WINDOW ** 2
    return out / np.maximum(norm, 1e-9)


freqs = np.fft.rfftfreq(N, 1 / FS)


def band_level(spectra, lo, hi):
    sel = (freqs >= lo) & (freqs < hi)
    return 10 * np.log10(np.mean(np.abs(spectra[:, sel]) ** 2, axis=1) + 1e-20)


# 27 and 29 ---------------------------------------------------------------------------------------
n = FS * 4
t = np.arange(n) / FS
chorus = t >= 2.0
low = np.where(chorus, 0.5, 0.25) * np.sin(2 * np.pi * 100 * t)   # the bass is 6 dB louder in the chorus too
harsh = np.zeros(n)
for f0 in (2500, 3000, 3600):
    harsh += np.sin(2 * np.pi * f0 * t + rng.uniform(0, 6))
harsh *= np.where(chorus, 0.12, 0.02)
mix = low + harsh + 0.003 * rng.standard_normal(n)

spectra = stft(mix)
level = 20 * np.log10(np.abs(spectra) + 1e-12)
# The threshold curve sits 3 dB over the verse at every frequency, as Learn would place it.
verse_frames = (np.arange(spectra.shape[0]) * HOP / FS) < 1.8
threshold = np.max(level[verse_frames], axis=0) + 3.0


def compress(level, threshold, ratio_curve, attack_ms=10.0, release_ms=120.0):
    hop_s = HOP / FS
    a = 1 - math.exp(-hop_s / (attack_ms * 0.001))
    r = 1 - math.exp(-hop_s / (release_ms * 0.001))
    gr = np.zeros(level.shape[1])
    out = np.zeros_like(level)
    for k in range(level.shape[0]):
        want = np.maximum(0.0, level[k] - threshold) * (1 - 1 / ratio_curve)
        gr += (want - gr) * np.where(want > gr, a, r)
        out[k] = gr
    return out


frames_t = np.arange(spectra.shape[0]) * HOP / FS
verse_f, chorus_f = (frames_t > 0.3) & (frames_t < 1.8), frames_t > 2.3
# The bins at the tones themselves (each tone spreads over about five bins).
b3k = np.zeros(len(freqs), bool)
for f0 in (2500, 3000, 3600):
    b3k |= np.abs(freqs - f0) <= 1.5 * FS / N
b100 = np.abs(freqs - 100) <= 1.5 * FS / N
one = np.full(len(freqs), 4.0)
curve = np.where((freqs > 2000) & (freqs < 5000), 4.0, 1.0)
print("27/29. Compressor mode, the threshold curve 3 dB over the verse; in the chorus 2.5-3.6 kHz is 15 dB louder and the bass 6 dB:")
for name, ratios in (("one Ratio, 4:1", one), ("Ratio curve: 4:1 from 2 to 5 kHz, 1:1 elsewhere", curve)):
    gr = compress(level, threshold, ratios)
    print(f"    {name:<48} 3 kHz: verse {np.mean(gr[verse_f][:, b3k]):4.1f} dB, chorus {np.mean(gr[chorus_f][:, b3k]):4.1f} dB; "
          f"100 Hz: verse {np.mean(gr[verse_f][:, b100]):4.1f} dB, chorus {np.mean(gr[chorus_f][:, b100]):4.1f} dB")

# 28 --------------------------------------------------------------------------------------------
n = FS * 3
t = np.arange(n) / FS
voice = np.zeros(n)
for h in range(1, 30):
    voice += np.sin(2 * np.pi * 180 * h * t + rng.uniform(0, 6)) / h
voice *= 0.1 * (0.5 + 0.5 * np.sin(2 * np.pi * 2 * t) ** 2)
hiss_side = 0.02 * rng.standard_normal(n)       # decorrelated hiss: mostly side
hiss_mid = 0.004 * rng.standard_normal(n)
left = voice + hiss_mid + hiss_side
right = voice + hiss_mid - hiss_side


def subtract(x, noise_profile, amount=1.0):
    spec = stft(x)
    power = np.abs(spec) ** 2
    gain = np.sqrt(np.maximum(0.05, 1 - amount * noise_profile / np.maximum(power, 1e-20)))
    return istft(spec * gain, len(x))


def profile(noise):
    return np.mean(np.abs(stft(noise)) ** 2, axis=0)


def db(v):
    return 10 * math.log10(np.mean(v ** 2) + 1e-30)


mid_in, side_in = 0.5 * (left + right), 0.5 * (left - right)
# L/R: each channel learns its own noise (the hiss in it) and subtracts it.
l_out = subtract(left, profile(hiss_mid + hiss_side))
r_out = subtract(right, profile(hiss_mid - hiss_side))
mid_lr, side_lr = 0.5 * (l_out + r_out), 0.5 * (l_out - r_out)
# M/S: only the side is processed; the mid passes untouched.
side_ms = subtract(side_in, profile(hiss_side))
mid_ms = mid_in

sl = slice(N, n - N)
print("28. Subtract on a voice in the centre with hiss mostly in the side:")
for name, m_out, s_out in (("L/R (today)", mid_lr, side_lr), ("Mid/Side, the side only", mid_ms, side_ms)):
    hiss_cut = db(side_in[sl]) - db(s_out[sl])
    centre_change = db((m_out - mid_in)[sl]) - db(mid_in[sl])
    print(f"    {name:<26} the side's hiss down {hiss_cut:4.1f} dB; the centre changed by {centre_change:6.1f} dB relative to itself")
