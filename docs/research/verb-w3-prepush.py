"""Pre-push check for Fancy Verb's Freeze and Shimmer (docs/verb.md, Phase 4 Wave 3, section 6), on
ports of what was built: verb::SpectralFreeze and verb::ShimmerLoop (plugins/verb/src/VerbW3.cpp)
and the loop's place in VerbDsp (the convolver in pieces of the loop delay, the feedback added to
its input, Freeze's 20 ms input fade, 200 ms crossfade and release at the IR's RT60). The shifter
is the line-for-line port in delay-w3-prepush.py, in plain mode.

Each test is measured the way tests/VerbW3Tests.cpp measures it, on one channel (the plugin's
channels are separate loops through their own IR, so one shows the other). Test 7's IR is a
stand-in for the library's Metal Ping (the generator can't run here): bright decaying noise,
highs 4.5 s and lows 0.45 s, high-passed at 600 Hz.

Run: python docs/research/verb-w3-prepush.py [test ...]   (tests 2 3 4 6 7 8; all by default)
"""
import importlib.util
import math
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("delay_w3_prepush", HERE / "delay-w3-prepush.py")
port = importlib.util.module_from_spec(spec)
spec.loader.exec_module(port)

RATE = 48000.0
R = int(RATE)


def portable_noise(n, seed):
    """Gaussian noise the C++ test makes alike (xorshift32 and Box-Muller), so the IRs are the same."""
    x = (0x12345678 ^ (seed * 0x9E3779B1)) & 0xffffffff or 1
    out = np.empty(n)
    for i in range(n):
        u = []
        for _ in range(2):
            x ^= (x << 13) & 0xffffffff
            x ^= x >> 17
            x ^= (x << 5) & 0xffffffff
            u.append((x + 0.5) / 4294967296.0)
        out[i] = math.sqrt(-2.0 * math.log(u[0])) * math.cos(2.0 * math.pi * u[1])
    return out


def decaying_noise(seconds, rt, seed, scale=0.05):
    n = int(seconds * RATE)
    return (scale * portable_noise(n, seed) * np.exp(-6.907755 * np.arange(n) / RATE / rt)).astype(np.float32).astype(float)


def schroeder_rt60(ir):
    e = ir ** 2
    tail = np.cumsum(e[::-1])[::-1]
    db = 10 * np.log10(np.maximum(tail / tail[0], 1e-30))
    t = np.arange(len(ir)) / RATE
    for lo, hi in ((-5, -35), (-5, -25)):
        sel = (db <= lo) & (db >= hi)
        if np.count_nonzero(sel) > 10:
            slope = np.polyfit(t[sel], db[sel], 1)[0]
            return -60.0 / slope
    return 0.0


def svf_coeffs(freq, q):
    g = math.tan(math.pi * freq / RATE)
    k = 1.0 / q
    a1 = 1.0 / (1.0 + g * (g + k))
    a2 = g * a1
    return (k, a1, a2, g * a2)


class SvfHigh:
    def __init__(self):
        self.ic1 = self.ic2 = 0.0

    def high(self, c, x):
        k, a1, a2, a3 = c
        v3 = x - self.ic2
        v1 = a1 * self.ic1 + a2 * v3
        v2 = self.ic2 + a2 * self.ic1 + a3 * v3
        self.ic1 = 2 * v1 - self.ic1
        self.ic2 = 2 * v2 - self.ic2
        return x - k * v1 - v2


class Loop:
    """verb::ShimmerLoop, one channel."""

    def __init__(self, rt60):
        self.delay = 1024
        self.ring = np.zeros(4096)
        self.mask = 4095
        self.pos = 0
        self.hp = SvfHigh()
        self.hpc = svf_coeffs(200.0, 0.70710678118654752)
        self.shifter = port.PitchShifter(RATE, aligned=False)
        self.rms = 1 - math.exp(-1 / (0.05 * RATE))
        self.att = 1 - math.exp(-1 / (0.01 * RATE))
        self.rel = 1 - math.exp(-1 / (0.1 * RATE))
        self.in_env = self.fb_env = self.peak = 0.0
        self.guard = 1.0
        self.hold_left = 0
        self.rt = max(0.05, rt60) * RATE
        self.hold = int(min(self.rt, 60 * RATE))
        self.let_go = 10 ** (-6.0 / self.rt)
        self.peaks = np.zeros(1024)

    def add_feedback(self, x):
        for i in range(len(x)):
            v = x[i]
            self.in_env += (v * v - self.in_env) * self.rms
            if self.in_env >= self.peak:
                self.peak, self.hold_left = self.in_env, self.hold
            elif self.hold_left > 0:
                self.hold_left -= 1
            else:
                self.peak *= self.let_go
            self.peaks[i] = self.peak
            at = (self.pos + i) & self.mask
            x[i] = v + self.ring[at]
            self.ring[at] = 0.0
        return x

    def feed(self, wet, i, ratio, gain):
        y = gain * self.shifter.process(self.hp.high(self.hpc, wet), ratio)
        self.fb_env += (y * y - self.fb_env) * self.rms
        ref = self.peaks[i]
        want = math.sqrt(ref / self.fb_env) if self.fb_env > ref and self.fb_env > 1e-30 else 1.0
        self.guard += (want - self.guard) * (self.att if want < self.guard else self.rel)
        self.ring[(self.pos + self.delay) & self.mask] = y * self.guard
        self.pos = (self.pos + 1) & self.mask


class Freeze:
    """verb::SpectralFreeze for two channels at 48 kHz."""

    def __init__(self, n):
        self.size, self.hop, self.bins = 4096, 1024, 2049
        self.hist = [np.zeros(n + 20000), np.zeros(n + 20000)]
        self.w = 0
        self.win = 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(4096) / 4096)
        self.scale = 1 / math.sqrt(0.375 * 1.5)
        self.seed = 0x9e3779b9

    def push(self, l, r):
        self.hist[0][self.w], self.hist[1][self.w] = l, r
        self.w += 1

    def rand(self, count):
        out = np.empty(count)
        s = self.seed
        for i in range(count):
            s ^= (s << 13) & 0xffffffff
            s ^= s >> 17
            s ^= (s << 5) & 0xffffffff
            out[i] = s / 4294967296.0
        self.seed = s
        return out

    def capture(self):
        self.mag, self.ph = [], []
        for ch in range(2):
            power = np.zeros(self.bins)
            for f in range(8):
                start = self.w - self.size - (7 - f) * self.hop
                spec = np.fft.rfft(self.win * self.hist[ch][start:start + self.size])
                power += np.abs(spec) ** 2
                if f == 7:
                    a = np.abs(spec)
                    self.ph.append(np.where(a > 1e-20, spec / np.maximum(a, 1e-30), 1.0))
            self.mag.append(np.sqrt(power / 8))
        self.acc = [np.zeros(self.size), np.zeros(self.size)]
        self.p = 0
        for off in (3, 2, 1, 0):
            self.add(off)

    def add(self, off):
        step = np.exp(1j * (2 * self.rand(self.bins) - 1) * np.pi)
        for ch in range(2):
            self.ph[ch] = self.ph[ch] * step
            spec = self.mag[ch] * self.ph[ch]
            spec[0], spec[-1] = spec[0].real, spec[-1].real
            frame = np.fft.irfft(spec, self.size) * self.win * self.scale
            self.acc[ch][:self.size - off * self.hop] += frame[off * self.hop:]

    def block(self, n):
        """The next n samples (n a multiple of nothing in particular)."""
        out = np.zeros((2, n))
        i = 0
        while i < n:
            take = min(n - i, self.hop - self.p)
            for ch in range(2):
                out[ch, i:i + take] = self.acc[ch][self.p:self.p + take]
            self.p += take
            i += take
            if self.p == self.hop:
                for ch in range(2):
                    self.acc[ch] = np.concatenate([self.acc[ch][self.hop:], np.zeros(self.hop)])
                self.p = 0
                self.add(0)
        return out


def render(irs, inputs, shimmer=0.0, semitones=12.0, freeze_on=None, freeze_off=None, energy=None, rt60=None):
    """VerbDsp's wet signal (Wet 1, Dry 0, Width 1, Center 0) for one or two channels: the convolver
    in pieces of 1024 samples, Shimmer's loop and Freeze. Returns the output per channel."""
    chans = len(irs)
    n = len(inputs[0])
    if energy is None:
        energy = max(float(np.sum(h ** 2)) for h in irs)
    if rt60 is None:
        rt60 = schroeder_rt60(irs[0])
    g = shimmer * 0.9 / math.sqrt(energy)
    ratio = 2 ** (semitones / 12)
    loops = [Loop(rt60) for _ in range(chans)] if shimmer > 0 else None
    L = max(len(h) for h in irs)
    acc = [np.zeros(n + L + 2048) for _ in range(chans)]
    out = np.zeros((chans, n))
    fz = Freeze(n) if freeze_on is not None else None
    state, f_in, level, live, in_gain = "idle", 0.0, 1.0, 1.0, 1.0
    decay = 10 ** (-3.0 / (max(0.05, rt60) * RATE))
    size = 1 << int(math.ceil(math.log2(1024 + L)))
    spectra = [np.fft.rfft(h, size) for h in irs]
    for t0 in range(0, n, 1024):
        m = min(1024, n - t0)
        if fz is not None:
            if freeze_on <= t0 and (freeze_off is None or t0 < freeze_off) and state != "holding":
                fz.capture()
                f_in, level, state = 0.0, 1.0, "holding"
            elif freeze_off is not None and t0 >= freeze_off and state == "holding":
                state = "releasing"
        x = [inputs[c][t0:t0 + m].astype(float).copy() for c in range(chans)]
        if loops:
            x = [loops[c].add_feedback(x[c]) for c in range(chans)]
        if state == "holding" or in_gain < 1.0:
            target = 0.0 if state == "holding" else 1.0
            gains = np.empty(m)
            for i in range(m):
                in_gain = max(target, in_gain - 1 / 960) if target < in_gain else min(target, in_gain + 1 / 960)
                gains[i] = in_gain
            x = [v * gains for v in x]
        for c in range(chans):
            y = np.fft.irfft(np.fft.rfft(x[c], size) * spectra[c], size)[:m + L - 1]
            acc[c][t0:t0 + len(y)] += y
        wet = np.array([acc[c][t0:t0 + m] for c in range(chans)])
        if loops:
            for i in range(m):
                for c in range(chans):
                    loops[c].feed(wet[c, i], i, ratio, g)
        if state != "idle":
            frozen = fz.block(m)
            for i in range(m):
                if state == "holding":
                    f_in = min(1.0, f_in + 1 / 9600)
                    live = min(live, math.cos(f_in * math.pi / 2))
                else:
                    live = min(1.0, live + 1 / 960)
                    level *= decay
                fg = math.sin(f_in * math.pi / 2) * level
                for c in range(chans):
                    wet[c, i] = wet[c, i] * live + frozen[c % 2, i] * fg
        if fz is not None:
            for i in range(m):
                fz.push(wet[0, i], wet[chans - 1, i])
        out[:, t0:t0 + m] = wet
    return out


def env_db(x, ms=50):
    f = int(ms * 0.001 * RATE)
    k = len(x) // f
    return 10 * np.log10(np.maximum(np.mean(x[:k * f].reshape(k, f) ** 2, axis=1), 1e-30))


def padded(x):
    """The power spectrum under a symmetric Hann window, zero-padded to a power of two, as the test does."""
    size = 1 << int(math.ceil(math.log2(len(x))))
    return np.abs(np.fft.rfft(x * np.hanning(len(x)), size)) ** 2, np.fft.rfftfreq(size, 1 / RATE)


def third_octaves(x):
    spec, f = padded(x)
    out = []
    for c in 1000 * 2 ** (np.arange(-15, 13) / 3):
        sel = (f >= c * 2 ** (-1 / 6)) & (f < c * 2 ** (1 / 6))
        out.append(10 * math.log10(max(np.sum(spec[sel]), 1e-30)))
    out = np.array(out)
    return out - 10 * math.log10(np.sum(10 ** (out / 10)))


def hall():
    a, b = decaying_noise(3.5, 2.5, 1), decaying_noise(3.5, 2.5, 2)
    return [a, 0.5 * a + math.sqrt(0.75) * b]


def chord(n):
    t = np.arange(int(1.2 * RATE)) / RATE
    c = sum(np.sin(2 * np.pi * f0 * k * t) / k for f0 in (196.0, 246.9, 293.7) for k in range(1, 6))
    c *= np.minimum(1, t / 0.01) * np.exp(-t / 0.8) * 0.1
    x = np.zeros(n)
    x[:len(c)] = c
    return x


def note(n, hz=220.0, seconds=0.4, amp=0.5):
    t = np.arange(int(seconds * RATE)) / RATE
    x = np.zeros(n)
    x[:len(t)] = amp * np.sin(2 * np.pi * hz * t) * np.minimum(1, t / 0.01) * np.minimum(1, (seconds - t) / 0.02)
    return x


def test2():
    n = int(12.0 * RATE)
    irs = hall()
    x = chord(n)
    at = int(1.5 * RATE) // 1024 * 1024
    out = render(irs, [x, x], freeze_on=at)
    captured = [out[c][at - int(0.25 * RATE):at] for c in range(2)]
    frozen = [out[c][at + R // 2:at + R // 2 + 10 * R] for c in range(2)]
    a, b = third_octaves(frozen[0]), third_octaves(captured[0])
    used = b > np.max(b) - 20
    e = env_db(frozen[0])
    wander = np.percentile(e, 95) - np.percentile(e, 5)
    env = 10 ** (e / 20)
    env -= env.mean()
    ac = np.correlate(env, env, "full")[len(env) - 1:]
    ac /= ac[0]
    rep = np.max(ac[4:101])
    corr_f, corr_c = np.corrcoef(frozen[0], frozen[1])[0, 1], np.corrcoef(captured[0], captured[1])[0, 1]
    print(f"2. Freeze: third octaves within {np.max(np.abs(a[used] - b[used])):.2f} dB (limit 4), 50 ms RMS wanders {wander:.2f} dB (limit 4), "
          f"repeats {rep:.2f} (limit 0.3), L/R correlation {corr_f:+.2f} against {corr_c:+.2f} (limit 0.15)")


def test3():
    n = int(9.0 * RATE)
    irs = hall()
    rt = schroeder_rt60(irs[0])
    x = chord(n)
    at, off = int(1.5 * RATE) // 1024 * 1024, int(4.0 * RATE) // 1024 * 1024
    out = render(irs, [x, x], freeze_on=at, freeze_off=off)
    e = env_db(out[0][off:])
    t = np.arange(len(e)) * 0.05
    sel = (t >= 0.05) & (t <= 0.5 * rt)
    slope = np.polyfit(t[sel], e[sel], 1)[0]
    print(f"3. Release: falls 60 dB in {-60 / slope:.3f} s against the IR's RT60 {rt:.3f} s ({100 * (-60 / slope / rt - 1):+.1f} %, limit 10 %)")


def test4():
    n = int(10.0 * RATE)
    irs = [decaying_noise(3.5, 2.5, 1)]
    out = render(irs, [note(n)], shimmer=1.0, semitones=12)[0]
    spec, f = padded(out[2 * R:int(2.5 * R)])
    levels = {hz: 10 * math.log10(np.sum(spec[np.abs(f - hz) < hz * 0.03])) for hz in (220, 440, 880, 1760, 3520, 7040, 14080)}
    top = max(levels.values())
    e = env_db(out)
    print("4. Shimmer +12 at 100 %: at 2 s (dB against the strongest) " + ", ".join(f"{hz}: {v - top:+.1f}" for hz, v in levels.items())
          + f"; the last 50 ms is {e[-1] - np.max(e):.1f} dB under the loudest (limit -60); finite {np.all(np.isfinite(out))}")


def test6():
    n = int(3.0 * RATE)
    irs = [decaying_noise(3.5, 2.5, 1)]
    t = np.arange(n) / RATE
    x = 0.25 * np.sin(2 * np.pi * 6000 * t) * np.minimum(1, t / 0.01)
    out = render(irs, [x], shimmer=1.0, semitones=24)[0]
    spec, f = padded(out[2 * R:3 * R])
    tone = np.max(spec[np.abs(f - 6000) < 50])
    below = np.max(spec[(f > 20) & (f < 5000)])
    print(f"6. Two octaves up at 100 % on 6 kHz: the most below 5 kHz is {10 * math.log10(below / tone):.1f} dB under the tone (limit -80)")


def metal():
    a = decaying_noise(2.0, 4.5, 11, 1.0)
    lo = decaying_noise(2.0, 0.45, 12, 1.0)
    spec = np.fft.rfft(a)
    f = np.fft.rfftfreq(len(a), 1 / RATE)
    h = np.fft.irfft(spec * (f >= 3000) + np.fft.rfft(lo) * (f < 3000) * (f >= 600), len(a))
    return h * math.sqrt(2.0 / np.sum(h ** 2)) * 0.125


def test7():
    ir = metal()
    rises = []
    for hz in (196.0, 220.0, 246.9, 261.6, 329.6, 392.0):
        n = int(6.0 * RATE)
        x = note(n, hz)
        base = render([ir], [x])[0]
        shim = render([ir], [x], shimmer=1.0, semitones=7)[0]
        rises.append(np.max(env_db(shim)) - np.max(env_db(base)))
    print("7. Guard, Metal Ping stand-in at 100 %, +7 st: the loudest 50 ms rises " + ", ".join(f"{v:+.1f}" for v in rises) + " dB (limit 10)")


def test8():
    n = int(13.0 * RATE)
    irs = hall()
    x = chord(n)
    at = int(1.5 * RATE) // 1024 * 1024
    out = render(irs, [x, x], shimmer=0.75, semitones=12, freeze_on=at)
    seconds = [10 * math.log10(np.mean(out[0][at + k * R:at + (k + 1) * R] ** 2)) for k in range(1, 11)]
    print(f"8. Freeze with Shimmer 75 %: 1 s RMS from 1 to 11 s after freezing spans {max(seconds) - min(seconds):.2f} dB (limit 2)")


if __name__ == "__main__":
    chosen = sys.argv[1:] or ["2", "3", "4", "6", "7", "8"]
    for name in chosen:
        globals()["test" + name]()
