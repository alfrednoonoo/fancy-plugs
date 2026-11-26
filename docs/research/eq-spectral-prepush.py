"""Pre-push check for Fancy EQ's spectral dynamics (docs/eq.md, Phase 4 Wave 3, section 6), on a
port of what was built: eq::SpectralStage (plugins/eq/src/EqSpectral.cpp) on fancy::dsp::StftFrames
(square-root Hann both ways, a quarter-frame hop, latency one frame), with the band's shape weight
from the bell at -1 dB, the three-bin level calibrated on a sine, and the per-bin gain computer.

Each test is measured as tests/EqW3Tests.cpp measures it. Test 2's "today's band" figure is the
design model's (eq-spectral-model.py: -4.1 dB at 1.5 kHz), since the IIR band is unchanged.

Run: python docs/research/eq-spectral-prepush.py
"""
import math

import numpy as np

RATE = 48000.0
R = 48000


def peaking_power(f, f0, q, gain_db):
    a = 10 ** (gain_db / 40)
    w0 = 2 * math.pi * f0 / RATE
    alpha = math.sin(w0) / (2 * q)
    b = np.array([1 + alpha * a, -2 * math.cos(w0), 1 - alpha * a])
    aa = np.array([1 + alpha / a, -2 * math.cos(w0), 1 - alpha / a])
    z = np.exp(-1j * 2 * np.pi * f / RATE)
    h = (b[0] + b[1] * z + b[2] * z * z) / (aa[0] + aa[1] * z + aa[2] * z * z)
    return np.abs(h) ** 2


def distance_past(d):
    return np.where(d <= -3, 0.0, np.where(d < 3, (d + 3) ** 2 / 12, d))


class Stage:
    def __init__(self, f0, q, threshold, range_db, reach=12.0, attack=5.0, release=80.0, n=1024, below=False):
        self.n, self.hop = n, n // 4
        self.bins = n // 2 + 1
        k = np.arange(n)
        self.win = np.sqrt(0.5 - 0.5 * np.cos(2 * np.pi * k / n))
        self.syn = 0.5 * self.win
        f = np.arange(self.bins) * RATE / n
        db = 10 * np.log10(np.maximum(peaking_power(np.minimum(f, 0.499 * RATE), f0, q, -1.0), 1e-30))
        w = np.maximum(0.0, -db)
        w = np.minimum(1.0, w / w.max())
        w[w < 1e-4] = 0.0
        self.weight = w
        # Calibration: a unit sine's three-bin power reads 0.5, on a bin and halfway between.
        k0 = n // 8
        total = 0.0
        for off in (0.0, 0.5):
            x = self.win * np.cos(2 * np.pi * (k0 + off) * k / n)
            p = np.abs(np.fft.rfft(x)) ** 2
            total += 0.5 * (p[k0 - 1] + p[k0] + p[k0 + 1])
        self.cal = 0.5 / total
        self.thr, self.range, self.reach, self.below = threshold, range_db, reach, below
        self.att = math.exp(-self.hop / (attack * 0.001 * RATE))
        self.rel = math.exp(-self.hop / (release * 0.001 * RATE))

    def levels(self, spec):
        p = np.abs(spec) ** 2
        p3 = p.copy()
        p3[1:] += p[:-1]
        p3[:-1] += p[1:]
        return 10 * np.log10(p3 * self.cal + 1e-30)

    def run(self, x, key=None, trace_bin=None):
        """Mono. Returns the output (latency n: output[i] is input[i - n] processed) and, if asked,
        one bin's change per frame."""
        n, hop = self.n, self.hop
        out = np.zeros(len(x) + n)
        change = np.zeros(self.bins)
        trace = []
        for t in range(n, len(x) + 1, hop):
            frame = x[t - n:t]
            spec = np.fft.rfft(self.win * frame)
            src = spec if key is None else np.fft.rfft(self.win * key[t - n:t])
            level = self.levels(src)
            d = self.thr - level if self.below else level - self.thr
            target = self.range * np.minimum(1.0, distance_past(d) / self.reach) * self.weight
            coef = np.where(np.abs(target) > np.abs(change), self.att, self.rel)
            change = target + coef * (change - target)
            change[(target == 0) & (np.abs(change) < 1e-6)] = 0.0
            if trace_bin is not None:
                trace.append((t, change[trace_bin]))
            y = np.fft.irfft(spec * 10 ** (change / 20), n) * self.syn
            out[t - n:t] += y
        # The sample written at position p is heard at p + n.
        delayed = np.concatenate([np.zeros(n), out[:len(x)]])[:len(x)]
        return delayed, trace


def pink(n, rms_db, seed=9):
    rng = np.random.default_rng(seed)
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / RATE)
    spec[1:] /= np.sqrt(f[1:])
    spec[0] = 0
    x = np.fft.irfft(spec, n)
    return x * 10 ** (rms_db / 20) / np.sqrt(np.mean(x ** 2))


def band_db(y, lo, hi):
    spec = np.abs(np.fft.rfft(y * np.hanning(len(y)))) ** 2
    f = np.fft.rfftfreq(len(y), 1 / RATE)
    return 10 * math.log10(np.sum(spec[(f >= lo) & (f < hi)]))


def test2():
    n = 3 * R
    t = np.arange(n) / RATE
    gate = ((t % 1.0) < 0.5).astype(float)
    ramp = np.convolve(gate, np.ones(96) / 96, mode="same")
    tone = 10 ** (-14 / 20) * math.sqrt(2) * np.sin(2 * np.pi * 2500 * t) * ramp
    noise = pink(n, -24)
    x = tone + noise
    st = Stage(2500, 1.0, -30, -9)
    y, _ = st.run(x)
    lat = st.n
    on_out, on_in = slice(int(1.15 * R) + lat, int(1.5 * R) + lat), slice(int(1.15 * R), int(1.5 * R))
    cut = band_db(y[on_out], 2425, 2575) - band_db(x[on_in], 2425, 2575)
    n15 = band_db(y[on_out], 1400, 1600) - band_db(x[on_in], 1400, 1600)
    n40 = band_db(y[on_out], 3800, 4200) - band_db(x[on_in], 3800, 4200)
    print(f"2. Selective: tone {cut:+.2f} dB (9 +- 0.5), noise at 1.5 kHz {n15:+.2f}, at 4 kHz {n40:+.2f} (under 0.5); today's band -4.1 (over 3)")


def test3():
    n = 2 * R
    t = np.arange(n) / RATE
    x = 10 ** (-18 / 20) * math.sqrt(2) * np.sin(2 * np.pi * 1000 * t)   # -18 dBFS RMS, Threshold -30: 12 dB over
    st = Stage(1000, 1.0, -30, -6)
    y, _ = st.run(x)
    seg = slice(R, 2 * R)
    change = 10 * math.log10(np.mean(y[seg] ** 2) / np.mean(x[seg] ** 2))
    print(f"3. Calibration: a 1 kHz sine 12 dB over Threshold, Range -6: {change:+.2f} dB (-6 +- 0.5)")


def test4():
    x = pink(R, -20, 3)
    st = Stage(2500, 1.0, -30, 0.0)
    y, _ = st.run(x)
    err = np.max(np.abs(y[2048:] - np.concatenate([np.zeros(st.n), x])[2048:len(x)]))
    print(f"4. Range 0: largest difference {err:.2e} (under 1e-6; float32 in the plugin)")


def test6():
    n = 2 * R
    t = np.arange(n) / RATE
    on = (t >= 0.5).astype(float)
    x = 10 ** (-14 / 20) * math.sqrt(2) * np.sin(2 * np.pi * 2500 * t) * on + pink(n, -40, 5)
    st = Stage(2500, 1.0, -30, -9)
    k = int(round(2500 / (RATE / st.n)))
    y, trace = st.run(x, trace_bin=k)
    final = trace[-1][1]
    for samples, value in trace:
        if samples >= 0.5 * R and value <= 0.9 * final:
            print(f"6. Timing: 90 % of the cut ({final:+.2f} dB at the tone's bin) {1000 * (samples - 0.5 * R) / RATE:.1f} ms after the tone starts (under 30)")
            break


for test in (test2, test3, test4, test6):
    test()
