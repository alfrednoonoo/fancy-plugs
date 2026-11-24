"""Pre-push model for Fancy Kicker's Phase 4 push: Clip oversampling (Wave 2 item 45) and the
sidechain Duck (docs/kicker.md), measuring exactly what tests/KickerW2Tests.cpp measures, on the
same signals, before the push (24 November 2026).

Clipper, test 2 (as the user decided on 24 November 2026): the model's kick, its 3 ms click
low-passed at 18 kHz, through Kicker's soft clipper Off, 2x and 4x; the error against a 16x ideal
reference measured below 20 kHz after removing the latency. 4x at least 30 dB under Off, 2x at
least 15 dB. The click's noise comes from a small LCG (state 45), which the C++ test copies, so
both see the same signal; kicker-clip-prepush.py used numpy's generator. The oversampler is the
Kaiser stand-in for JUCE's half-band equiripple stages, as in kicker-clip-prepush.py.

Duck, tests 2 and 3, on kicker-duck-model.py's signals, with KickerDucker's detector (peak held
instantly, released over 30 ms; full above -12 dBFS, none below -24; 1 ms attack, Release):
  2. Duck 6 dB, Release 120 ms: the output under each kick (0 to 0.12 s after it) at least 5 dB
     under Duck 0's, and over the last 100 ms before the next kick (0.4 to 0.5 s) within 2 dB.
  3. A key held at -30 dBFS: no duck at all.

Run: python docs/research/kicker-w2-prepush.py
"""
import math

import numpy as np

FS = 48000


# Clipper ---------------------------------------------------------------------------------------
def lcg_noise(count, state=45):
    out = np.empty(count)
    for i in range(count):
        state = (1664525 * state + 1013904223) % (1 << 32)
        out[i] = state / float(1 << 32) * 2.0 - 1.0
    return out


def clip_signal():
    n = FS // 4
    t = np.arange(n) / FS
    noise = lcg_noise(n)
    kick = np.sin(2 * np.pi * 55 * t) * np.exp(-t / 0.12) + 1.2 * math.sqrt(3.0) * noise * np.exp(-t / 0.0015) * (t < 0.003)
    kick *= 1.6
    fade = int(0.02 * FS)
    kick[-fade:] *= 0.5 + 0.5 * np.cos(np.pi * np.arange(fade) / fade)
    x = np.zeros(32768)
    x[2000:2000 + n] = kick
    return lowpass(x, 18000.0)


def lowpass(x, hz):
    spec = np.fft.rfft(x)
    spec[np.fft.rfftfreq(len(x), 1 / FS) > hz] = 0
    return np.fft.irfft(spec, len(x))


def soft_clip(x):
    knee = 0.7
    m = np.abs(x)
    return np.where(m <= knee, x, np.sign(x) * (knee + (1 - knee) * np.tanh((m - knee) / (1 - knee))))


def halfband(transition, stopband_db):
    a = -stopband_db
    beta = 0.1102 * (a - 8.7)
    taps = int(math.ceil((a - 7.95) / (14.36 * transition))) | 1
    taps += (taps + 1) % 4 == 0 and 2 or 0
    m = np.arange(taps) - (taps - 1) / 2
    h = 0.5 * np.sinc(0.5 * m) * np.kaiser(taps, beta)
    return h / h.sum()


STAGES = [halfband(0.06, -100.0), halfband(0.12, -90.0)]


def oversampled_clip(x, stages):
    if stages == 0:
        return soft_clip(x), 0.0
    y, delay = x, 0.0
    for s in range(stages):
        z = np.zeros(2 * len(y))
        z[::2] = 2 * y
        y = np.convolve(z, STAGES[s])[: len(z)]
        delay = 2 * delay + (len(STAGES[s]) - 1) // 2
    y = soft_clip(y)
    for s in reversed(range(stages)):
        y = np.convolve(y, STAGES[s])[: len(y)][::2]
        delay = (delay + (len(STAGES[s]) - 1) // 2) / 2
    return y, delay


def ideal_reference(x):
    big = np.fft.irfft(np.fft.rfft(x), len(x) * 16) * 16
    return np.fft.irfft(np.fft.rfft(soft_clip(big))[: len(x) // 2 + 1], len(x)) / 16


def shift(x, delay):
    f = np.fft.rfftfreq(len(x), 1 / FS)
    return np.fft.irfft(np.fft.rfft(x) * np.exp(2j * np.pi * f * delay / FS), len(x))


x = clip_signal()
ref = lowpass(ideal_reference(x), 20000.0)
errors = []
for stages in (0, 1, 2):
    y, delay = oversampled_clip(x, stages)
    y = lowpass(shift(y, delay), 20000.0)
    errors.append(10 * math.log10(np.mean((y - ref) ** 2) / np.mean(ref ** 2)))
print(f"Clipper test 2 (LCG click, low-passed at 18 kHz, error below 20 kHz): peak in {np.max(np.abs(x)):.2f}")
print(f"    Off {errors[0]:6.1f} dB, 2x {errors[1]:6.1f} dB, 4x {errors[2]:6.1f} dB; "
      f"2x under Off by {errors[0] - errors[1]:.1f} dB (needs 15), 4x by {errors[0] - errors[2]:.1f} dB (needs 30)")


# Duck ------------------------------------------------------------------------------------------
n = FS * 4
t = np.arange(n) / FS
beat = 0.5

bass = np.zeros(n)
notes = (55.0, 65.4, 73.4, 82.4)
for k in range(16):
    s = int(k * 0.25 * FS)
    m = np.arange(int(0.25 * FS))
    bass[s:s + len(m)] = 0.35 * np.sin(2 * np.pi * notes[(k // 2) % 4] * m / FS) * np.minimum(1, m / (0.005 * FS))

kick = np.zeros(n)
for k in range(8):
    s = int(k * beat * FS)
    m = np.arange(int(0.3 * FS))
    kick[s:s + len(m)] += 0.8 * np.sin(2 * np.pi * 50 * m / FS) * np.exp(-m / (0.12 * FS))


def duck_gain(key, depth_db, release_ms):
    a = 1 - math.exp(-1 / (0.001 * FS))
    r = 1 - math.exp(-1 / (release_ms * 0.001 * FS))
    peak_r = 1 - math.exp(-1 / (0.03 * FS))
    peak, env, out = 0.0, 0.0, np.empty(len(key))
    for i, v in enumerate(np.abs(key)):
        peak = v if v > peak else peak + (v - peak) * peak_r
        db = 20 * math.log10(max(peak, 1e-9))
        want = depth_db * min(1.0, max(0.0, (db + 24) / 12))
        env += (want - env) * (a if want > env else r)
        if want == 0.0 and env < 1e-4:
            env = 0.0
        out[i] = 10 ** (-env / 20)
    return out


g = duck_gain(kick, 6.0, 120.0)
phase = t % beat
during = phase < 0.12
before_next = (phase >= 0.4) & (phase < 0.5) & (t > beat)   # after the first kick


def ratio_db(mask):
    return 10 * math.log10(np.mean((bass * g)[mask] ** 2) / np.mean(bass[mask] ** 2))


print(f"Duck test 2: under each kick {ratio_db(during):.2f} dB (needs -5 or lower); "
      f"last 100 ms before the next kick {ratio_db(before_next):.2f} dB (needs within 2)")
held = duck_gain(np.full(FS, 10 ** (-30 / 20)), 6.0, 120.0)
print(f"Duck test 3: a key held at -30 dBFS, smallest gain {np.min(held):.6f} (needs exactly 1)")
