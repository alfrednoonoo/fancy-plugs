"""Pre-push check for Fancy Delay's reverse and pitch-shifted taps (docs/delay.md, Phase 4 Wave 3,
section 6), on a line-for-line port of fancy::dsp::PitchShifter (fancy-core/include/fancy/dsp/
PitchShifter.h) and of one DelayDsp tap in ms mode with no filters, character or modulation.

The design model (delay-reverse-pitch-model.py) reads the future when it aligns a grain; the built
shifter is causal: it matches the last 10 ms either head would have read. This runs each test of
section 6 the way tests/DelayW3Tests.cpp measures it, so the limits are checked before CI.

Run: python docs/research/delay-w3-prepush.py [test ...]   (tests 2 4 5 6 7 8; all by default)
"""
import math
import sys

import numpy as np

RATE = 48000.0


class Svf:
    def __init__(self):
        self.ic1 = self.ic2 = 0.0

    @staticmethod
    def coeffs(freq, q, rate):
        g = math.tan(math.pi * freq / rate)
        k = 1.0 / q
        a1 = 1.0 / (1.0 + g * (g + k))
        a2 = g * a1
        return (k, a1, a2, g * a2)

    def low(self, c, x):
        k, a1, a2, a3 = c
        v3 = x - self.ic2
        v1 = a1 * self.ic1 + a2 * v3
        v2 = self.ic2 + a2 * self.ic1 + a3 * v3
        self.ic1 = 2 * v1 - self.ic1
        self.ic2 = 2 * v2 - self.ic2
        return v2


def hermite(newer, near, far, older, f):
    c1 = 0.5 * (far - newer)
    c2 = newer - 2.5 * near + 2.0 * far - 0.5 * older
    c3 = 0.5 * (older - newer) + 1.5 * (near - far)
    return ((c3 * f + c2) * f + c1) * f + near


class Head:
    def __init__(self):
        self.active = False
        self.k = 0
        self.length = 1
        self.delay = 0.0
        self.pc, self.ps, self.sc, self.ss = 1.0, 0.0, 1.0, 0.0


class PitchShifter:
    window_ms, search_ms, minimum_ms, max_grain_ms, match_ms, max_ratio = 40.0, 10.0, 1.0, 400.0, 10.0, 2.0
    centre_bias = 0.05  # of the score, at the ends of the search: equal matches go to the middle

    def __init__(self, rate=RATE, aligned=True):
        self.rate = rate
        self.aligned = aligned
        self.window = self.window_ms * 0.001 * rate
        self.search = self.search_ms * 0.001 * rate
        self.minimum = self.minimum_ms * 0.001 * rate
        self.centre = self.minimum + 0.5 * self.search + 0.5 * self.window
        self.max_grain = self.max_grain_ms * 0.001 * rate
        self.stride = max(4, round(4.0 * rate / 48000.0))
        self.fine_step = max(1, self.stride // 4)
        self.match_count = max(8, int(self.match_ms * 0.001 * rate) // self.stride)
        self.fine_count = max(8, int(0.5 * self.match_ms * 0.001 * rate) // self.fine_step)
        self.max_delay = self.minimum + self.search + self.window + self.max_ratio * self.match_ms * 0.001 * rate + 8.0
        size = 1
        while size < int(self.max_delay) + 16:
            size <<= 1
        self.size, self.mask = size, size - 1
        self.line = np.zeros(size, dtype=np.float32)
        self.heads = [Head(), Head()]
        self.write = 0
        self.countdown = 0
        self.started = False
        self.filter_ratio = -1.0
        self.filter_countdown = 0
        self.lo_a, self.lo_b = Svf(), Svf()
        self.ca = self.cb = None

    def read(self, delay):
        whole = int(delay)
        f = delay - whole
        d, m, w = self.line, self.mask, self.write
        return hermite(float(d[(w - whole + 1) & m]), float(d[(w - whole) & m]), float(d[(w - whole - 1) & m]), float(d[(w - whole - 2) & m]), f)

    def read_sum_many(self, delays):
        whole = delays.astype(int)
        f = delays - whole
        a = self.line[(self.write - whole) & self.mask].astype(float)
        b = self.line[(self.write - whole - 1) & self.mask].astype(float)
        return a + (b - a) * f

    def grain_length(self, ratio):
        away = abs(1.0 - ratio)
        length = self.window / away if away * self.max_grain > self.window else self.max_grain
        return max(4, int(round(length)))

    def start_head(self, h, length, delay):
        h.active, h.k, h.length = True, 0, length
        h.delay = min(max(delay, 2.0), self.max_delay)
        step = 2 * math.pi / length
        h.sc, h.ss = math.cos(step), math.sin(step)
        h.pc, h.ps = 1.0, 0.0

    def start_grain(self, ratio):
        nxt = self.heads[0] if not self.heads[0].active else (self.heads[1] if not self.heads[1].active else None)
        if nxt is None:
            self.countdown = 1
            return
        out = self.heads[1] if nxt is self.heads[0] else self.heads[0]
        length = self.grain_length(ratio)
        travel = (ratio - 1.0) * length
        direction = -1.0 if ratio > 1.0 else 1.0

        def start_delay(shift):
            return self.centre + 0.5 * travel + direction * (shift - 0.5 * self.search)

        best = 0.5 * self.search
        if self.aligned and out.active:
            # The match spans `match` samples ending as far ahead as every read is already written
            # (at least 2 samples back), and no further back than `match` before now.
            match = self.match_count * self.stride
            nearest = min(out.delay, start_delay(0.0), start_delay(self.search))
            k_end = min(match, int((nearest - 3.0) / ratio))
            k_start = k_end - match

            def reads(delay, count, spacing):
                return self.read_sum_many(delay - ratio * (k_start + spacing * np.arange(count)))

            target = reads(out.delay, self.match_count // 2, 2 * self.stride)
            energy = float(np.dot(target, target))
            if energy > 1e-12:
                bias = lambda sh: self.centre_bias * abs(sh - 0.5 * self.search) / (0.5 * self.search)
                shifts = np.arange(0.0, self.search + 1e-9, float(self.stride))
                scores = [np.dot(c := reads(start_delay(sh), self.match_count // 2, 2 * self.stride), target) / math.sqrt(np.dot(c, c) * energy + 1e-30) - bias(sh) for sh in shifts]
                coarse = float(shifts[int(np.argmax(scores))])
                ft = reads(out.delay, self.fine_count, self.fine_step)
                fe = float(np.dot(ft, ft))
                lo, hi = max(0.0, coarse - (self.stride - 1)), min(self.search, coarse + (self.stride - 1))
                shifts = np.arange(lo, hi + 1e-9, float(self.fine_step))
                scores = [np.dot(c := reads(start_delay(sh), self.fine_count, self.fine_step), ft) / math.sqrt(np.dot(c, c) * fe + 1e-30) - bias(sh) for sh in shifts]
                best = float(shifts[int(np.argmax(scores))])
        self.start_head(nxt, length, start_delay(best))
        self.countdown = max(1, length // 2)

    def process(self, x, ratio):
        ratio = min(max(ratio, 0.5), 2.0)
        if ratio != self.filter_ratio:
            self.filter_countdown -= 1
            if self.filter_countdown <= 0:
                cutoff = 0.45 * self.rate / max(1.0, ratio)
                self.ca = Svf.coeffs(cutoff, 0.54119610014619698, self.rate)
                self.cb = Svf.coeffs(cutoff, 1.3065629648763766, self.rate)
                self.filter_ratio = ratio
                self.filter_countdown = 16
        low = self.lo_b.low(self.cb, self.lo_a.low(self.ca, x))
        self.line[self.write] = low if math.isfinite(low) else 0.0
        self.write = (self.write + 1) & self.mask
        if not self.started:
            self.started = True
            h = self.heads[0]
            self.start_head(h, self.grain_length(ratio), self.centre)
            h.k = h.length // 2
            h.pc, h.ps = -1.0, 0.0
            self.countdown = 0
        self.countdown -= 1
        if self.countdown <= 0:
            self.start_grain(ratio)
        total, gain_sum = 0.0, 0.0
        for h in self.heads:
            if not h.active:
                continue
            g = 0.5 - 0.5 * h.pc
            total += g * self.read(h.delay)
            gain_sum += g
            h.delay = min(max(h.delay - (ratio - 1.0), 2.0), self.max_delay)
            c = h.pc * h.sc - h.ps * h.ss
            s = h.ps * h.sc + h.pc * h.ss
            n = 1.5 - 0.5 * (c * c + s * s)
            h.pc, h.ps = c * n, s * n
            h.k += 1
            if h.k >= h.length:
                h.active = False
        return total / max(gain_sum, 0.5)


def safety(v):
    a = abs(v)
    if a <= 0.9:
        return v
    return math.copysign(0.9 + 0.1 * math.tanh((a - 0.9) / 0.1), v)


def pitched_tap(x, tap_ms, semitones, feedback, freeze_at=None):
    """One tap in ms mode: the ring read tap time minus the shifter's average delay before, shifted,
    and fed back. Freeze (from sample freeze_at, snapped, no crossfade) writes back the raw read."""
    sh = PitchShifter()
    ratio = 2 ** (semitones / 12)
    target = round(tap_ms * 0.001 * RATE)
    lead = round(sh.centre)
    head = max(2, target - lead)
    ring = np.zeros(1 << 20, dtype=np.float32)
    mask = len(ring) - 1
    y = np.zeros(len(x))
    for n in range(len(x)):
        raw = float(ring[(n - head) & mask])
        shifted = sh.process(raw, ratio)
        y[n] = shifted
        loop = shifted * feedback
        if freeze_at is not None and n >= freeze_at:
            loop, inp = raw, 0.0
        else:
            inp = float(x[n])
        w = inp + loop
        if feedback > 0.0:
            w = safety(w)
        ring[n & mask] = w
    return y


def bands_db(x):
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / RATE)
    out = []
    centre = 250.0
    while centre < 16000:
        lo, hi = centre / 2 ** (1 / 6), centre * 2 ** (1 / 6)
        e = spec[(f >= lo) & (f < hi)].sum()
        out.append(10 * math.log10(e + 1e-30))
        centre *= 2 ** (1 / 3)
    return np.array(out)


def peak_hz(x):
    n = len(x)
    spec = np.abs(np.fft.rfft(x * np.hanning(n), 8 * n))
    i = int(np.argmax(spec))
    a, b, c = np.log(spec[i - 1:i + 2] + 1e-30)
    return (i + 0.5 * (a - c) / (a - 2 * b + c)) * RATE / (8 * n)


def test4():
    print("4. 1 kHz at +7 st through a 300 ms tap, no feedback:")
    n = int(1.6 * RATE)
    t = np.arange(n) / RATE
    x = 0.5 * np.sin(2 * np.pi * 1000 * t)
    y = pitched_tap(x, 300, 7, 0.0)
    seg = y[int(0.5 * RATE):int(1.5 * RATE)]
    hz = peak_hz(seg)
    frame = int(0.002 * RATE)
    rms = np.sqrt(np.convolve(seg ** 2, np.ones(frame) / frame, mode="valid"))
    waver = 20 * math.log10(rms.max() / rms.min())
    print(f"   pitch {hz:.2f} Hz (limit 1498.31 +- 1), wavers {waver:.2f} dB (limit 2)")
    # Timing: a 200 ms burst of 1 kHz with 10 ms raised-cosine edges, the envelopes cross-correlated.
    n = int(1.0 * RATE)
    burst = np.zeros(n)
    k = np.arange(int(0.2 * RATE))
    edge = int(0.01 * RATE)
    env = np.minimum(1.0, np.minimum(k, len(k) - 1 - k) / edge)
    env = 0.5 - 0.5 * np.cos(np.pi * env)
    burst[int(0.1 * RATE):int(0.1 * RATE) + len(k)] = 0.5 * np.sin(2 * np.pi * 1000 * k / RATE) * env
    y = pitched_tap(burst, 300, 7, 0.0)
    e_in = np.sqrt(np.convolve(burst ** 2, np.ones(frame) / frame, mode="same"))
    e_out = np.sqrt(np.convolve(y ** 2, np.ones(frame) / frame, mode="same"))
    lags = np.arange(int(0.25 * RATE), int(0.35 * RATE))
    corr = [np.dot(e_in[:n - L], e_out[L:]) for L in lags]
    lag = lags[int(np.argmax(corr))] / RATE * 1000
    print(f"   lands at {lag:.2f} ms (limit 300 +- 2)")


def test5():
    print("5. Cascade: +12 st, 60 % feedback, 500 ms tap, a 300 ms 220 Hz note with 20 ms raised-cosine edges:")
    tap, note = 0.5, 0.3
    n = int((5 * tap + 0.2) * RATE)
    x = np.zeros(n)
    k = np.arange(int(note * RATE))
    edge = int(0.02 * RATE)
    env = np.minimum(1.0, np.minimum(k, len(k) - 1 - k) / edge)
    x[:len(k)] = 0.5 * np.sin(2 * np.pi * 220 * k / RATE) * (0.5 - 0.5 * np.cos(np.pi * env))
    y = pitched_tap(x, tap * 1000, 12, 0.6)
    prev = None
    for r in range(1, 5):
        seg = y[int((tap * r - 0.03) * RATE):int((tap * r + note + 0.07) * RATE)]
        hz = peak_hz(seg)
        e = 10 * math.log10(np.sum(seg ** 2) + 1e-30)
        want = 220 * 2 ** r
        step = "" if prev is None else f", {prev - e:.2f} dB under the last (limit 4.4 +- 0.5)"
        print(f"   repeat {r}: {hz:.1f} Hz (want {want} +- 1 %: {abs(hz / want - 1) * 100:.2f} %){step}")
        prev = e


def test6(seconds=20.0):
    print(f"6. No aliasing: +12 st, 95 % feedback, {seconds:.0f} s of a 5 kHz tone at 0.1, 300 ms tap:")
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    x = 0.1 * np.sin(2 * np.pi * 5000 * t)
    y = pitched_tap(x, 300, 12, 0.95)
    seg = y[-int(2 * RATE):]
    spec = np.abs(np.fft.rfft(seg * np.blackman(len(seg)))) * 2 / np.sum(np.blackman(len(seg)))
    f = np.fft.rfftfreq(len(seg), 1 / RATE)
    low = 20 * math.log10(spec[(f > 20) & (f < 4000)].max() + 1e-30)
    print(f"   finite: {np.all(np.isfinite(y))}; largest part below 4 kHz {low:.1f} dBFS (limit -80); peak {np.abs(y).max():.3f}")


def kellet_pink(n, seed):
    """Paul Kellet's pink filter on Gaussian noise, as the C++ test makes it, scaled to 0.1 RMS."""
    rng = np.random.default_rng(seed)
    b = [0.0] * 7
    out = np.zeros(n)
    for i, white in enumerate(rng.standard_normal(n)):
        b[0] = 0.99886 * b[0] + white * 0.0555179
        b[1] = 0.99332 * b[1] + white * 0.0750759
        b[2] = 0.96900 * b[2] + white * 0.1538520
        b[3] = 0.86650 * b[3] + white * 0.3104856
        b[4] = 0.55000 * b[4] + white * 0.5329522
        b[5] = -0.7616 * b[5] - white * 0.0168980
        out[i] = sum(b[:6]) + b[6] + white * 0.5362
        b[6] = white * 0.115926
    return out * 0.1 / np.sqrt(np.mean(out ** 2))


def test7(seeds=(5, 6, 7)):
    print("7. Freeze with Pitch +12 (60 % feedback, 300 ms tap, 1 s of pink noise, frozen at 1.2 s;")
    print("   third octaves from 250 Hz, 2 s from 0.4 s after against 2 s from 9 s after):")
    for seed in seeds:
        n = int(12.5 * RATE)
        x = np.zeros(n)
        x[:int(RATE)] = kellet_pink(int(RATE), seed)
        freeze = int(1.2 * RATE)
        y = pitched_tap(x, 300, 12, 0.6, freeze_at=freeze)
        a = bands_db(y[freeze + int(0.4 * RATE):freeze + int(2.4 * RATE)])
        b = bands_db(y[freeze + int(9.0 * RATE):freeze + int(11.0 * RATE)])
        keep = a > a.max() - 60
        print(f"   seed {seed}: largest change {np.max(np.abs(a - b)[keep]):.2f} dB (limit 1)")


def reverse_tap(x, window, fade):
    """The reverse head: delay 2k at sample k of each window (ratio 1), the 5 ms raised-cosine fade."""
    ring = np.zeros(1 << 20, dtype=np.float32)
    mask = len(ring) - 1
    y = np.zeros(len(x))
    k = 0
    for n in range(len(x)):
        delay = max(2, 2 * k)
        ramp = min(1.0, min(k, window - k) / fade)
        y[n] = float(ring[(n - delay) & mask]) * (0.5 - 0.5 * math.cos(math.pi * ramp))
        ring[n & mask] = x[n]
        k += 1
        if k >= window:
            k = 0
    return y


def test2():
    print("2. Reverse, 500 ms, on 437.25 Hz at 0.5 (4 s):")
    n = int(4 * RATE)
    t = np.arange(n) / RATE
    x = 0.5 * np.sin(2 * np.pi * 437.25 * t)
    w = int(0.5 * RATE)
    y = reverse_tap(x, w, 0.005 * RATE)
    fwd = np.concatenate([np.zeros(w), x[:-w]])
    level = 10 * math.log10(np.mean(y[2 * w:] ** 2) / np.mean(fwd[2 * w:] ** 2))
    spec = np.fft.rfft(y)
    spec[np.fft.rfftfreq(n, 1 / RATE) < 6000] = 0
    high = np.fft.irfft(spec, n)
    half = int(0.01 * RATE)
    worst = max(np.max(np.abs(high[e - half:e + half])) for e in range(2 * w, n - half, w))
    print(f"   level {level:+.3f} dB against a forward tap (limit 0.1); click {20 * math.log10(worst / 0.5):.1f} dB under the tone (limit -80)")
    imp = np.zeros(int(2.5 * RATE))
    at = int(0.62 * RATE)
    imp[at] = 1.0
    y = reverse_tap(imp, w, 0.005 * RATE)
    lands = int(np.argmax(np.abs(y)))
    print(f"   impulse at {at}: lands at {lands}, want 2 x window end - t = {2 * 2 * w - at}")


def test8():
    """Switching, as tests/DelayW3Tests.cpp does it: a 20 ms tap, Dry 1, a 200 Hz tone at 0.25;
    Reverse on at 0.15 s, Pitch +7 at 0.3, Reverse off at 0.45, Pitch -5 at 0.65, 0 at 0.8. The
    worst second difference after 50 ms (limit 2e-3). Run 45792294707 failed it at 0.0519: the
    tap counted as warmed from its start, so Pitch's first fade-in met the shifter's empty line;
    warmed = 3600 below reproduces that exactly."""
    R = RATE

    class Sm:
        def __init__(s, ms, v=0.0):
            s.c = 1 - math.exp(-1 / (ms * 0.001 * R))
            s.cur = s.tgt = v

        def next(s):
            if s.cur != s.tgt:
                s.cur += (s.tgt - s.cur) * s.c
                if abs(s.tgt - s.cur) < 1e-9:
                    s.cur = s.tgt
            return s.cur

    def herm_read(ring, w, d):
        whole = int(d)
        f = d - whole
        m = len(ring) - 1
        return hermite(ring[(w - whole + 1) & m], ring[(w - whole) & m], ring[(w - whole - 1) & m], ring[(w - whole - 2) & m], f)

    class Sm:
        def __init__(s, ms, v=0.0): s.c = 1 - math.exp(-1 / (ms * 0.001 * R)); s.cur = s.tgt = v
        def next(s):
            if s.cur != s.tgt:
                s.cur += (s.tgt - s.cur) * s.c
                if abs(s.tgt - s.cur) < 1e-9: s.cur = s.tgt
            return s.cur
    def herm_read(ring, w, d):
        whole = int(d); f = d - whole; m = len(ring) - 1
        return hermite(ring[(w - whole + 1) & m], ring[(w - whole) & m], ring[(w - whole - 1) & m], ring[(w - whole - 2) & m], f)
    n = int(R)
    x = 0.25 * np.sin(2 * np.pi * 200 * np.arange(n) / R)
    ring = np.zeros(1 << 16); m = len(ring) - 1
    head = 960; lead = 1248; warmN = 3600; fadeN = 240
    pitch = Sm(30); pmix = Sm(30); rmix = Sm(30)
    sh = None; running = False; warmed = 0; wanted = False
    ratio = 1.0; rfor = 0.0
    K = 0; W = 960; rdel = 0.0; since = 1 << 30
    rev = False; st = 0.0
    at = lambda s: int(round(s * R / 64)) * 64
    y = np.zeros(n); tag = np.zeros(n, dtype=int)
    for i in range(n):
        if i == at(0.15): rev = True
        if i == at(0.3): st = 7.0
        if i == at(0.45): rev = False
        if i == at(0.65): st = -5.0
        if i == at(0.8): st = 0.0
        pitch.tgt = st; wanted = st != 0 and not rev; rmix.tgt = 1.0 if rev else 0.0
        semis = pitch.next()
        if wanted and warmed < warmN: warmed += 1; pmix.tgt = 0.0
        else: pmix.tgt = 1.0 if wanted else 0.0
        pm = pmix.next(); rm = rmix.next()
        if semis != rfor: ratio = 2 ** (semis / 12); rfor = semis
        read = ring[(i - head) & m]
        pitching = pm > 0 or wanted
        if pitching:
            if sh is None: sh = PitchShifter(R)
            pin = ring[(i - max(2, head - lead)) & m]
            s_ = sh.process(pin, ratio); running = True
            read += (s_ - read) * pm
        elif running:
            sh = None; running = False; warmed = 0
        out = read
        if K == 0:
            W = 960; rdel = 0.0
        if rm > 0:
            edge = min(K, W - K, since)
            env = 1.0 if edge >= fadeN else 0.5 - 0.5 * math.cos(math.pi * edge / fadeN)
            v = herm_read(ring, i, max(2.0, rdel)) * env
            out += (v - out) * rm
        rdel = min(rdel + 1 + ratio, 480000 - 4); since = min(since + 1, 1 << 30)
        K += 1
        if K >= W: K = 0
        ring[i & m] = x[i]
        y[i] = x[i] + out
    d2 = np.abs(y[2:] - 2 * y[1:-1] + y[:-2])
    print(f"8. Switching Reverse and Pitch: worst second difference {d2[2400:].max():.5f} (limit 0.002)")


if __name__ == "__main__":
    tests = sys.argv[1:] or ["2", "4", "5", "6", "7", "8"]
    for name in tests:
        {"2": test2, "4": test4, "5": test5, "6": test6, "7": test7, "8": test8}[name]()


def test_shifter():
    """tests/PitchShifterTests.cpp: the shifter alone, on steady tones, 1 s, measured from 0.3 s."""
    def run(rate, hz, st, aligned=True, seconds=1.0):
        sh = PitchShifter(rate, aligned)
        n = int(seconds * rate)
        x = 0.5 * np.sin(2 * np.pi * hz * np.arange(n) / rate)
        y = np.array([sh.process(v, 2 ** (st / 12)) for v in x])
        seg = y[int(0.3 * rate):]
        spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 8 * len(seg)))
        i = int(np.argmax(spec))
        a, b, c = np.log(spec[i - 1:i + 2] + 1e-30)
        f = (i + 0.5 * (a - c) / (a - 2 * b + c)) * rate / (8 * len(seg))
        frame = int(0.002 * rate)
        rms = np.sqrt(np.convolve(seg ** 2, np.ones(frame) / frame, mode="valid"))
        level = 10 * math.log10(np.mean(seg ** 2) / 0.125)
        return f, level, 20 * math.log10(rms.max() / rms.min())
    print("Shifter alone (frequency, level, waver over 2 ms RMS):")
    for rate, hz, st, aligned in ((48000, 440, 12, True), (48000, 1000, -12, True), (48000, 440, 12, False),
                                  (44100, 1000, 7, True), (96000, 1000, 7, True), (384000, 1000, 7, True)):
        f, level, waver = run(float(rate), hz, st, aligned, 1.0 if rate < 200000 else 0.6)
        want = hz * 2 ** (st / 12)
        print(f"   {rate} Hz, {hz} Hz {st:+d} st {'aligned' if aligned else 'plain  '}: {f:.2f} Hz (want {want:.2f}), level {level:+.2f} dB, wavers {waver:.2f} dB")
