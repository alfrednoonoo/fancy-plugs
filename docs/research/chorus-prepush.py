"""Pre-push check for Fancy Chorus (docs/chorus.md): a sample-by-sample port of ChorusDsp's
signal path (plugins/chorus/src/ChorusDsp.cpp), run against tests/ChorusTests.cpp's checks so
the first CI run has the thresholds right. Age is left out (the tests run with Age 0).

Run: py -3 docs/research/chorus-prepush.py
"""
import math

import numpy as np

RATE = 48000.0
CLASSIC, ENSEMBLE, DIMENSION, DOUBLER, FLANGE = range(5)
SINE, TRIANGLE, RANDOM = range(3)


def coef(ms):
    return 1.0 - math.exp(-1.0 / (ms * 0.001 * RATE))


class Smoother:
    def __init__(self, ms, value):
        self.c = coef(ms) if ms > 0 else 1.0
        self.cur = self.tgt = value

    def next(self):
        if self.cur != self.tgt:
            self.cur += (self.tgt - self.cur) * self.c
            if abs(self.tgt - self.cur) < 1e-9:
                self.cur = self.tgt
        return self.cur

    def settled(self):
        return self.cur == self.tgt


class Svf:
    def __init__(self):
        self.ic1 = self.ic2 = 0.0

    @staticmethod
    def coefs(f, q=0.7071067811865476):
        g = math.tan(math.pi * min(max(f, 10.0), 0.45 * RATE) / RATE)
        k = 1 / q
        a1 = 1 / (1 + g * (g + k))
        return k, a1, g * a1, g * g * a1

    def tick(self, c, x):
        k, a1, a2, a3 = c
        v3 = x - self.ic2
        v1 = a1 * self.ic1 + a2 * v3
        v2 = self.ic2 + a2 * self.ic1 + a3 * v3
        self.ic1 = 2 * v1 - self.ic1
        self.ic2 = 2 * v2 - self.ic2
        return v2, x - k * v1 - v2   # low, high


def h2u(cycle, seed):
    h = ((cycle & 0xFFFFF) * 0x9E3779B97F4A7C15 + seed * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
    h ^= h >> 30; h = (h * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
    h ^= h >> 27; h = (h * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
    h ^= h >> 31
    return (h >> 11) * (2.0 / 9007199254740992.0) - 1.0


def lfo_value(shape, p, seed):
    if shape == TRIANGLE:
        f = p - math.floor(p)
        return 4 * f if f < 0.25 else (2 - 4 * f if f < 0.75 else 4 * f - 4)
    if shape == RANDOM:
        c = math.floor(p); f = p - c
        a, b = h2u(c, seed), h2u(c + 1, seed)
        return a + (b - a) * f * f * (3 - 2 * f)
    return math.sin(2 * math.pi * p)


def flange_ms(d):
    pos = math.log(min(max(d, 1.0), 30.0)) / math.log(30.0)
    return 0.3 * (10 / 0.3) ** pos


def centre_ms(mode, d):
    return flange_ms(d) if mode == FLANGE else d


def sweep_ms(mode, d, depth):
    c = centre_ms(mode, d)
    w = depth * c if mode == FLANGE else (0.5 * depth * 5 if mode == DIMENSION else depth * 5)
    return min(w, 0.95 * c)


def position(v, n):
    return 0.0 if n <= 1 else -1 + 2 * v / (n - 1)


def targets(**kw):
    t = dict(mode=CLASSIC, rate=0.8, sync=False, beats=4.0, bpm=120.0, playing=False, ppq=0.0, depth=0.45, delay=7.0,
             feedback=0.0, voices=1, spread=0.6, width=1.0, lowOn=False, highOn=False, low=150.0, high=9000.0,
             bassMono=False, shape=SINE, mono=False, phase=90.0, ducking=0.0, wet=1.0, dry=0.0)
    t.update(kw)
    return t


def run(t, left, right, block=333, change=None):
    n = len(left)
    size = 1 << math.ceil(math.log2(int(math.ceil(0.03835 * RATE)) + 8 + 96))
    lines = [np.zeros(size), np.zeros(size)]
    mask = size - 1
    w = 0
    s = {k: Smoother(20, t[k]) for k in ("rate", "depth", "feedback", "width", "phase", "ducking", "low", "high", "wet", "dry")}
    s["delay"] = Smoother(50, t["delay"])
    blends = {k: Smoother(20, 1.0 if t[k] else 0.0) for k in ("lowOn", "highOn", "bassMono")}
    nv = t["voices"]
    gains = [Smoother(30, 1.0 if v < nv else 0.0) for v in range(6)]
    poss = [Smoother(30, position(min(v, nv - 1), nv)) for v in range(6)]
    offs = [Smoother(50, t["spread"] * v / nv) for v in range(6)]
    norm = Smoother(20, 1 / math.sqrt(nv))
    sw = Smoother(1.0, 1.0)
    mode, shape, sync = t["mode"], t["shape"], t["sync"]
    lows, highs, bm = [Svf(), Svf()], [Svf(), Svf()], Svf()
    dcin, dcout = [0.0, 0.0], [0.0, 0.0]
    dcc = 1 - 2 * math.pi * 5 / RATE
    peak = duck = 0.0
    pfall, datt, drel = coef(30), coef(1), 24 / (0.25 * RATE)
    phase, aligned, now = 0.0, False, 0
    out_l, out_r = np.zeros(n), np.zeros(n)
    SEG = 8
    seg_left, seg_valid = 0, False
    lnow = [[0.0, 0.0] for _ in range(6)]; lstep = [[0.0, 0.0] for _ in range(6)]; lend = [[0.0, 0.0] for _ in range(6)]

    def lfo_at(v, ch, base, roff):
        if mode == DOUBLER:
            return lfo_value(RANDOM, base + (roff if ch else 0), 17 + v * 2 + ch)
        if mode == DIMENSION:
            return (1 if ch == 0 else -1) * lfo_value(shape, base, 101 + v)
        l = lfo_value(shape, base + (roff if ch else 0), 101 + v)
        if mode == ENSEMBLE:
            l += 0.07 * math.sin(2 * math.pi * (6.7 * (base + (roff if ch else 0)) + 0.37 * v))
        return l
    cl = ch_ = cb = None
    for start in range(0, n, block):
        if change:
            change(t, start)
        nv = max(1, min(6, t["voices"]))
        beats = t["beats"]
        if sync and t["playing"]:
            hp = t["ppq"] / beats
            if not aligned or abs(hp - phase) > 2 * t["bpm"] / 60 / beats / RATE + 1e-9:
                phase = hp
                seg_valid, seg_left = False, 0
            aligned = True
        else:
            aligned = False
        for k in s:
            s[k].tgt = t[k]
        for k in blends:
            blends[k].tgt = 1.0 if t[k] else 0.0
        norm.tgt = 1 / math.sqrt(nv)
        for v in range(6):
            wp = position(min(v, nv - 1), nv)
            wo = t["spread"] * v / nv
            if gains[v].settled() and gains[v].cur == 0.0:
                poss[v].cur = poss[v].tgt = wp
                offs[v].cur = offs[v].tgt = wo
            else:
                poss[v].tgt = wp
                if v < nv:
                    offs[v].tgt = wo
            gains[v].tgt = 1.0 if v < nv else 0.0
        for i in range(start, min(n, start + block)):
            if (t["mode"], t["shape"], t["sync"]) != (mode, shape, sync):
                sw.tgt = 0.0
                if sw.settled():
                    mode, shape, sync = t["mode"], t["shape"], t["sync"]
                    aligned = False
                    seg_valid, seg_left = False, 0
                    s["delay"].cur = s["delay"].tgt
                    sw.tgt = 1.0
            if now % 16 == 0:
                cl, ch_, cb = Svf.coefs(s["low"].cur), Svf.coefs(s["high"].cur), Svf.coefs(s["low"].cur)
            x = [left[i], right[i]]
            feed = [0.5 * (x[0] + x[1])] * 2 if t["mono"] else list(x)
            rate = s["rate"].next()
            cps = (t["bpm"] / 60 / beats if sync else rate) / RATE
            depth, delay, fb = s["depth"].next(), s["delay"].next(), s["feedback"].next()
            width, roff = s["width"].next(), s["phase"].next() / 360
            nm = norm.next()
            c, swp = centre_ms(mode, delay), sweep_ms(mode, delay, depth)
            if seg_left == 0:
                for v in range(6):
                    base = phase + offs[v].cur
                    for ch in range(2):
                        e = lfo_at(v, ch, base + SEG * cps, roff)
                        st = lend[v][ch] if seg_valid else lfo_at(v, ch, base, roff)
                        lnow[v][ch], lstep[v][ch], lend[v][ch] = st, (e - st) / SEG, e
                seg_valid, seg_left = True, SEG
            wet = [0.0, 0.0]; fsum = [0.0, 0.0]; gsum = 0.0
            for v in range(6):
                g = gains[v].next(); p = poss[v].next() * width; off = offs[v].next()
                if g == 0.0:
                    continue
                y = [0.0, 0.0]
                for ch in range(2):
                    l = lnow[v][ch]
                    d = max(3.0, (c + swp * l) * 0.001 * RATE)
                    pos = w - d; idx = math.floor(pos); f = pos - idx
                    L = lines[ch]
                    xm1, x0, x1, x2 = L[(idx - 1) & mask], L[idx & mask], L[(idx + 1) & mask], L[(idx + 2) & mask]
                    y[ch] = x0 if f == 0 else ((((0.5 * (x2 - xm1) + 1.5 * (x0 - x1)) * f + (xm1 - 2.5 * x0 + 2 * x1 - 0.5 * x2)) * f + 0.5 * (x1 - xm1)) * f + x0)
                fsum[0] += g * y[0]; fsum[1] += g * y[1]; gsum += g
                m = 0.5 * (y[0] + y[1]); sd = 0.5 * (y[0] - y[1]) * width * (1 - abs(p))
                gl = gr = 1.0
                if p != 0.0:
                    a = (p + 1) * 0.25 * math.pi
                    gl, gr = math.cos(a) * math.sqrt(2), math.sin(a) * math.sqrt(2)
                wet[0] += g * (gl * m + sd); wet[1] += g * (gr * m - sd)
            for v in range(6):
                for ch in range(2):
                    lnow[v][ch] += lstep[v][ch]
            seg_left -= 1
            for ch in range(2):
                sm = fsum[ch] / gsum if gsum > 0 else 0.0
                b = sm - dcin[ch] + dcc * dcout[ch]
                dcin[ch], dcout[ch] = sm, b
                lines[ch][w] = feed[ch] + max(-4, min(4, fb * b))
            w = (w + 1) & mask
            lb, hb = blends["lowOn"].next(), blends["highOn"].next()
            s["low"].next(); s["high"].next()
            wv = [wet[0] * nm, wet[1] * nm]
            for ch in range(2):
                v_ = wv[ch]
                v_ += lb * (lows[ch].tick(cl, v_)[1] - v_)
                v_ += hb * (highs[ch].tick(ch_, v_)[0] - v_)
                wv[ch] = v_
            mb = blends["bassMono"].next()
            m = 0.5 * (wv[0] + wv[1]); sd = 0.5 * (wv[0] - wv[1])
            sd += mb * (bm.tick(cb, sd)[1] - sd)
            wv = [m + sd, m - sd]
            da = s["ducking"].next()
            lvl = max(abs(x[0]), abs(x[1]))
            peak = lvl if lvl > peak else peak + (lvl - peak) * pfall
            dg = 1.0
            if da > 0 or duck > 0:
                want = 24 * da if 20 * math.log10(max(peak, 1e-9)) > -24 else 0.0
                duck = duck + (want - duck) * datt if want > duck else max(want, duck - drel)
                duck = 0.0 if duck < 1e-6 else duck
                dg = 10 ** (-duck / 20)
            wg, dgain, fade = s["wet"].next(), s["dry"].next(), sw.next()
            out_l[i] = wv[0] * dg * fade * wg + x[0] * dgain
            out_r[i] = wv[1] * dg * fade * wg + x[1] * dgain
            phase += cps
            now += 1
    return out_l, out_r


def sine(seconds, hz, level):
    t = np.arange(int(seconds * RATE)) / RATE
    x = level * np.sin(2 * np.pi * hz * t)
    return x, x.copy()


def cycle_freqs(x, start):
    c = []
    for i in range(start + 1, len(x)):
        a, b = x[i - 1], x[i]
        if a < 0 <= b:
            c.append(i - 1 + a / (a - b))
    return RATE / np.diff(c)


def db(x):
    return 20 * math.log10(max(x, 1e-12))


print("Fancy Chorus pre-push (the C++ tests' checks):")

# Test 2.
l, r = sine(3.0, 1000.0, 0.5)
ol, _ = run(targets(), l, r)
f = cycle_freqs(ol, int(0.1 * RATE))
up, down = 1200 * math.log2(f.max() / 1000), 1200 * math.log2(f.min() / 1000)
print(f"  2. detune {down:+.1f} to {up:+.1f} cents (17 to 23): {'ok' if 17 < up < 23 and -23 < down < -17 else 'FAIL'}")

# Test 6.
n = int(RATE)
x = 0.5 * np.sin(2 * np.pi * 60 * np.arange(n) / RATE)
def part(inl, inr, bassmono, side):
    a, b = run(targets(voices=3, lowOn=True, low=150.0, bassMono=bassmono), inl, inr)
    p = 0.5 * (a + (-b if side else b))
    return math.sqrt(np.mean(p[n // 2:] ** 2))
sd = db(part(x, -x, True, True) / part(x, -x, False, True))
md = db(part(x, x, True, False) / part(x, x, False, False))
print(f"  6. side at 60 Hz {sd:.1f} dB (< -12), mid {md:+.3f} dB: {'ok' if sd < -12 and abs(md) < 0.1 else 'FAIL'}")

# Test 7 (one block size, both rates).
for rate_hz in (0.3, 3.0):
    l, r = sine(3.5, 200.0, 0.5)
    at = lambda sec: int(sec * RATE)
    def change(t, start):
        t["delay"] = 20.0 if start >= at(0.5) else 5.0
        t["voices"] = 1 if start >= at(1.5) else (6 if start >= at(1.0) else 3)
        t["mode"] = DIMENSION if start >= at(3.0) else DOUBLER if start >= at(2.5) else FLANGE if start >= at(2.0) else CLASSIC
    ol, orr = run(targets(voices=3, rate=rate_hz), l, r, 333, change)
    step = max(np.abs(np.diff(ol)).max(), np.abs(np.diff(orr)).max())
    unit = 0.5 * 2 * math.pi * 200 / RATE
    print(f"  7. rate {rate_hz} Hz: largest step {step / unit:.2f} x the input's (< 6): {'ok' if step < 6 * unit else 'FAIL'}")

# Test 8.
n = int(2 * RATE)
tt = np.arange(n) / RATE
x = np.where(np.arange(n) < RATE, 0.25, 0.01) * np.sin(2 * np.pi * 440 * tt)
ref, _ = run(targets(), x, x.copy())
duc, _ = run(targets(ducking=1.0), x, x.copy())
def gain_at(sec):
    a, b = int(sec * RATE), int((sec + 0.01) * RATE)
    return db(math.sqrt(np.mean(duc[a:b] ** 2)) / math.sqrt(np.mean(ref[a:b] ** 2)))
g1, g2, g3 = gain_at(0.5), gain_at(1.1), gain_at(1.33)
print(f"  8. ducking: loud {g1:.2f} dB (-24 +-1), 100 ms after {g2:.1f} dB (< -3), 330 ms after {g3:.2f} dB (> -1): "
      f"{'ok' if abs(g1 + 24) < 1 and g2 < -3 and g3 > -1 else 'FAIL'}")
