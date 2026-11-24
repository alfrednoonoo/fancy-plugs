"""Pre-push model for Fancy Kicker's item 45, the oversampled clipper (docs/kicker.md, "Phase 4
Wave 2: an oversampled clipper", test 2), written before any code (24 November 2026).

The design model (docs/research/exciter-kicker-w2-model.py, part 45) oversampled with ideal FFT
resampling and measured the error against an ideal reference that keeps everything up to 24 kHz.
The plugin will use fancy::dsp::SwitchedOversampler: JUCE half-band FIR equiripple stages, the
first with a normalised transition width of 0.06 and -100 dB stopband, the second 0.12 and
-90 dB. The first stage's passband ends near 21 kHz at 48 kHz, so the 3 ms click's content
from 21 to 24 kHz is cut by the filters themselves, not by aliasing. Measured as the model did,
that loss counts as error, and the test as written could fail for a reason that has nothing to
do with aliasing.

This model uses Kaiser-windowed half-band filters with the same transition widths and stopbands
(a stand-in for JUCE's equiripple design, which has the same band edges) and Kicker's own soft
clipper (knee 0.7, then tanh), and measures the error two ways:
  A. as the design model: everything up to 24 kHz;
  B. below 20 kHz only, with both the output and the reference low-passed there (a brick-wall in
     the FFT), which is what the test should measure: aliasing lands everywhere, the filters'
     own transition band only above 21 kHz.

Run: python docs/research/kicker-clip-prepush.py
"""
import math

import numpy as np

FS = 48000
rng = np.random.default_rng(45)
n = FS // 4
t = np.arange(n) / FS
kick = np.sin(2 * np.pi * 55 * t) * np.exp(-t / 0.12) + 1.2 * rng.standard_normal(n) * np.exp(-t / 0.0015) * (t < 0.003)
kick *= 1.6
# Faded out over its last 20 ms and followed by silence, so the filters' linear convolution and
# the reference's circular FFT see the same signal (cut off at 0.25 s, the two disagree at the
# ends by -32 dB, which hides everything else).
fade = int(0.02 * FS)
kick[-fade:] *= 0.5 + 0.5 * np.cos(np.pi * np.arange(fade) / fade)
kick = np.concatenate([np.zeros(2000), kick, np.zeros(n)])  # and silence first, for the filters' pre-ringing


def soft_clip(x):
    knee = 0.7
    m = np.abs(x)
    return np.where(m <= knee, x, np.sign(x) * (knee + (1 - knee) * np.tanh((m - knee) / (1 - knee))))


def halfband(transition, stopband_db):
    # Kaiser-windowed sinc at a quarter of the (oversampled) rate, sized for the transition.
    a = -stopband_db
    beta = 0.1102 * (a - 8.7)
    taps = int(math.ceil((a - 7.95) / (14.36 * transition))) | 1
    taps += (taps + 1) % 4 == 0 and 2 or 0   # keep the centre on an odd tap pattern
    m = np.arange(taps) - (taps - 1) / 2
    h = 0.5 * np.sinc(0.5 * m) * np.kaiser(taps, beta)
    return h / h.sum()


STAGES = [halfband(0.06, -100.0), halfband(0.12, -90.0)]


def up(x, h):
    y = np.zeros(2 * len(x))
    y[::2] = 2 * x
    return np.convolve(y, h)[: 2 * len(x)], (len(h) - 1) // 2


def down(x, h):
    return np.convolve(x, h)[: len(x)][::2], (len(h) - 1) // 2


def oversampled_clip(x, stages):
    if stages == 0:
        return soft_clip(x), 0.0
    y, delay = x, 0.0
    for s in range(stages):
        y, d = up(y, STAGES[s])
        delay = 2 * delay + d
    y = soft_clip(y)
    for s in reversed(range(stages)):
        y, d = down(y, STAGES[s])
        delay = (delay + d) / 2
    return y, delay


def ideal_reference(x):
    big = np.fft.irfft(np.fft.rfft(x), len(x) * 16) * 16
    return np.fft.irfft(np.fft.rfft(soft_clip(big))[: len(x) // 2 + 1], len(x)) / 16


def lowpass(x, hz):
    spec = np.fft.rfft(x)
    spec[np.fft.rfftfreq(len(x), 1 / FS) > hz] = 0
    return np.fft.irfft(spec, len(x))


def shift(x, delay):
    # A fractional delay removed in the FFT, so the comparison is exact for any latency.
    f = np.fft.rfftfreq(len(x), 1 / FS)
    return np.fft.irfft(np.fft.rfft(x) * np.exp(2j * np.pi * f * delay / FS), len(x))


print("Item 45, a 0 dBFS kick with a 3 ms click through Kicker's soft clipper (error under the signal, dB):")
for cut, label in ((None, "click to 24 kHz"), (20000, "click low-passed at 20 kHz"), (18000, "click low-passed at 18 kHz")):
    x = kick if cut is None else lowpass(kick, cut)
    ref = ideal_reference(x)
    row = []
    for stages in (0, 1, 2):
        y, delay = oversampled_clip(x, stages)
        y = shift(y, delay)
        full = 10 * math.log10(np.mean((y - ref) ** 2) / np.mean(ref ** 2))
        below = 10 * math.log10(np.mean((lowpass(y, 20000) - lowpass(ref, 20000)) ** 2) / np.mean(lowpass(ref, 20000) ** 2))
        row.append((full, below))
    print(f"    {label:<28} A (to 24 kHz): Off {row[0][0]:6.1f}, 2x {row[1][0]:6.1f}, 4x {row[2][0]:6.1f};   "
          f"B (below 20 kHz): Off {row[0][1]:6.1f}, 2x {row[1][1]:6.1f}, 4x {row[2][1]:6.1f};   4x under Off by B {row[0][1] - row[2][1]:.1f} dB")
print("The note's test 2 (4x at least 35 dB under Off, measured as A on a full-band click) would fail: the oversampler's")
print("own band edge, near 21 kHz, changes what the clipper is fed. Proposed: the click low-passed at 18 kHz, measured")
print("below 20 kHz, 4x at least 30 dB under Off and 2x at least 15 dB.")
