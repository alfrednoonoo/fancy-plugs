"""Model of Fancy Verb's decay module (docs/verb.md, Phase 4 Wave 2, item 7 of the order).

A module that changes the IR's decay time per band (Pro-R 2's decay EQ, but on the IR, offline).
The IR is split into bands with complementary masks in the frequency domain (zero-padded to twice
its length, so nothing wraps round; the masks sum to exactly 1, so the IR is unchanged when every
multiplier is 1). Each band's RT60 is measured by Schroeder backward integration (-5 to -35 dB),
and the band is multiplied by exp(-6.91 t (1 / (m T) - 1 / T)) so its RT becomes m x T.

Lengthening raises the late tail, and with it the IR's noise floor: a real IR (here white noise
at -100 dB under the peak, as in a 24-bit file) would have its floor lifted by up to +60 dB at
x 2. So each band's gain only grows until the band's envelope (10 ms RMS) comes within 10 dB of
its floor, and the band then fades out over 100 ms: the decay lengthens as far as the IR has
decay to lengthen, and no further.

Checks (RT60 2 s in every band, 4 s long, 48 kHz, crossovers 250 Hz and 4 kHz):
  1. Multipliers all 1: the output equals the input (to float rounding).
  2. Low x 0.5, Mid x 1, High x 2: measured RT per band.
  3. The first 50 ms changes by less than 0.5 dB.
  4. The noise floor at the end: before, after with the fade, after without it.

Run: py -3 docs/research/verb-decay-model.py
"""
import numpy as np

RATE, SECONDS, RT = 48000, 4.0, 2.0
rng = np.random.default_rng(1)
n = int(RATE * SECONDS)
t = np.arange(n) / RATE
ir = rng.standard_normal(n) * np.exp(-6.9078 * t / RT) + rng.standard_normal(n) * 1e-5  # floor -100 dB

size = 2 * n
freqs = np.fft.rfftfreq(size, 1 / RATE)


def masks(crossovers, width_octaves=0.5):
    """Complementary raised-cosine masks: each crossover fades over width_octaves."""
    bands, below = [], np.ones_like(freqs)
    for fc in crossovers:
        x = np.log2(np.maximum(freqs, 1e-9) / fc) / width_octaves + 0.5
        e = 0.5 - 0.5 * np.cos(np.pi * np.clip(x, 0, 1))  # 0 below the crossover, 1 above
        bands.append(below * (1 - e))
        below = below * e
    bands.append(below)
    return bands


def split(x, crossovers):
    spectrum = np.fft.rfft(x, size)
    return [np.fft.irfft(spectrum * m, size)[:n] for m in masks(crossovers)]


def rt60(x):
    energy = np.cumsum((x ** 2)[::-1])[::-1]
    edc = 10 * np.log10(energy / energy[0] + 1e-30)
    i5, i35 = np.argmax(edc <= -5), np.argmax(edc <= -35)
    return (t[i35] - t[i5]) * 60 / 30


def envelope_db(x, window=480):
    power = np.convolve(x ** 2, np.ones(window) / window, mode="same")
    return 10 * np.log10(power + 1e-30)


def decay(x, crossovers, multipliers, stop_at_floor=True):
    out = np.zeros_like(x)
    for band, m in zip(split(x, crossovers), multipliers):
        if m != 1.0:
            tb = rt60(band)
            gain = np.exp(-6.9078 * t * (1 / (m * tb) - 1 / tb))
            if m > 1.0 and stop_at_floor:
                env = envelope_db(band)
                floor = np.median(env[-int(0.2 * RATE):])
                reached = np.argmax(env < floor + 10.0)
                if reached > 0:
                    gain[reached:] = gain[reached]
                    fade = np.ones(n)
                    length = int(0.1 * RATE)
                    end = min(n, reached + length)
                    fade[reached:end] = 0.5 + 0.5 * np.cos(np.pi * np.arange(end - reached) / length)
                    fade[end:] = 0.0
                    gain = gain * fade
            band = band * gain
        out += band
    return out


same = decay(ir, [250, 4000], [1, 1, 1])
print(f"1. multipliers 1: largest change {np.max(np.abs(same - ir)):.2e} (IR peak {np.max(np.abs(ir)):.2f})")

changed = decay(ir, [250, 4000], [0.5, 1, 2])
raw = decay(ir, [250, 4000], [0.5, 1, 2], stop_at_floor=False)
def third_octave(x, centre):
    """A third-octave band around centre, for measuring a band's RT away from its crossovers."""
    spectrum = np.fft.rfft(x, size)
    # A smooth bell in log frequency (a brick wall that narrow rings for seconds by itself).
    window = np.exp(-0.5 * (np.log2(np.maximum(freqs, 1e-9) / centre) / (1 / 6)) ** 2)
    return np.fft.irfft(spectrum * window, size)[:n]


for name, centre, m in (("low", 100.0, 0.5), ("mid", 1000.0, 1.0), ("high", 10000.0, 2.0)):
    before, after = rt60(third_octave(ir, centre)), rt60(third_octave(changed, centre))
    print(f"2. {name:4} (third octave at {centre:.0f} Hz, x {m}): RT {before:.2f} s -> {after:.2f} s, aimed at {before * m:.2f} s")

early = slice(0, int(0.05 * RATE))
last = slice(n - int(0.1 * RATE), n)
level = lambda x, s: 10 * np.log10(np.mean(x[s] ** 2))
print(f"3. first 50 ms level change: {level(changed, early) - level(ir, early):+.2f} dB")
print(f"4. last 100 ms against the first 50 ms: before {level(ir, last) - level(ir, early):.1f} dB, "
      f"after {level(changed, last) - level(changed, early):.1f} dB, without the floor stop {level(raw, last) - level(raw, early):.1f} dB")
