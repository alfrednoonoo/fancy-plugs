"""Pre-push model of Fancy Exciter's Wave 2 items 40 to 43 as the plugin builds them
(docs/exciter.md, Phase 4 Wave 2, sections 7 and 9; tests/ExciterW2bTests.cpp).

It follows plugins/exciter/src/ExciterDsp.cpp at Oversampling Off, where the plugin's path is
exactly this: the TPT state-variable filters, the cubic shaper with first-order ADAA and the
Character bias, the post high-pass and the 0.45 fs Butterworth low-pass. The tests run at Off for
that reason; their figures are what this script prints.

- Band 2 (item 40): hp(x, Freq 2) minus the top band's own pre high-pass, times Clip Boost,
  through its own shaper state, a post high-pass at Freq 2 and the low-pass, times
  Harmonics 2 x Mix Back.
- Punch (item 41): a linked detector on the top band's pre high-pass (before Clip Boost): its
  level smoothed over 5 ms, a fast follower (0.5 ms attack) and a slow one (20 ms attack), both
  releasing over 100 ms, transient = clip((dB(fast/slow) - 2) / 10, 0, 1), and the added part's
  amount times clip(1 + Punch (2 transient - 0.5), 0.5, 2). The design model's followers (fast
  0.5/10 ms, slow 20/200 ms, on the bare level, no dead zone) never settle in a sustain: the
  pluck's top harmonics beat at 220 Hz and the fast one rides the beats, so the sustain kept 0.7 of
  its level, not 0.5, and test 3 grew by only 6.7 dB. With equal releases, the smoothing and the
  2 dB dead zone, a sustain sits at exactly 0.5 (-6 dB).
- Width (item 43): the added part's side times Width.

Test 2: a 150 Hz voice-like stack, Freq 3 kHz, Clip Boost 12 dB, Harmonics 100 %, Mix Back
-6 dB, Band 2 at 400 Hz and Harmonics 2 100 %: the added part's energy from 400 Hz to 1 kHz
(user, 24 November 2026). From 400 Hz to 3 kHz, as the note had it, the rise is only 13.6 dB:
the top band adds the input's own highs as well as harmonics, and its 24 dB/oct skirt already
adds -0.7 dB there with Band 2 off. The design model counted harmonics only.
Test 3: a 220 Hz pluck whose harmonics all decay over 0.25 s, Freq 2 kHz, Clip Boost 12 dB,
Harmonics 100 %: the added part's first 20 ms against 100 to 300 ms, Punch 0 and 100 %.
Test 4 needs no figures: Width 0 makes the side exactly zero.

Run: python docs/research/exciter-w2b-prepush.py
"""
import math

import numpy as np

FS = 48000.0
Q = 1 / math.sqrt(2)


class Svf:
    def __init__(self):
        self.ic1 = self.ic2 = 0.0

    def coeffs(self, hz):
        g = math.tan(math.pi * hz / FS)
        k = 1 / Q
        a1 = 1 / (1 + g * (g + k))
        return k, a1, g * a1, g * g * a1

    def high(self, c, x):
        k, a1, a2, a3 = c
        v3 = x - self.ic2
        v1 = a1 * self.ic1 + a2 * v3
        v2 = self.ic2 + a2 * self.ic1 + a3 * v3
        self.ic1, self.ic2 = 2 * v1 - self.ic1, 2 * v2 - self.ic2
        return x - k * v1 - v2


class LowPass:
    def __init__(self):
        w = 2 * math.pi * 0.45 * FS / FS
        cw, alpha = math.cos(w), math.sin(w) * math.sqrt(2) * 0.5
        a0 = 1 + alpha
        self.b0 = (1 - cw) * 0.5 / a0; self.b1 = (1 - cw) / a0; self.b2 = self.b0
        self.a1 = -2 * cw / a0; self.a2 = (1 - alpha) / a0
        self.z1 = self.z2 = 0.0

    def process(self, x):
        y = self.b0 * x + self.z1
        self.z1 = self.b1 * x - self.a1 * y + self.z2
        self.z2 = self.b2 * x - self.a2 * y
        return y


def shape(x):
    return math.copysign(1.0, x) if abs(x) >= 1.5 else x - 4 / 27 * x ** 3


def anti(x):
    m = abs(x)
    if m >= 1.5:
        return m - 0.5625
    s = x * x
    return 0.5 * s - s * s / 27


class Shaper:
    def __init__(self):
        self.px = 0.0; self.pa = 0.0

    def run(self, x, bias):
        x += bias
        a = anti(x); d = x - self.px
        y = shape(0.5 * (x + self.px)) if abs(d) < 1e-6 else (a - self.pa) / d
        self.px, self.pa = x, a
        return y - shape(bias)


def coef(ms):
    return 1 - math.exp(-1 / (ms * 0.001 * FS))


def exciter(x, freq, boost_db, harmonics, mix_db, freq2=None, harmonics2=0.0, punch=0.0, character=0.5):
    """Mono, Oversampling Off. Returns the added part (output minus the input) and the top band's part."""
    boost, mix = 10 ** (boost_db / 20), 10 ** (mix_db / 20)
    bias = 0.75 * (1 - character)
    pre, post, lp, sh = Svf(), Svf(), LowPass(), Shaper()
    pre2, post2, lp2, sh2 = Svf(), Svf(), LowPass(), Shaper()
    c = pre.coeffs(freq)
    c2 = pre2.coeffs(min(freq2, freq)) if freq2 else None
    ls, fa, sa, rel = coef(5), coef(0.5), coef(20), coef(100)
    level = fast = slow = 0.0
    added, top = np.empty(len(x)), np.empty(len(x))
    for i, v in enumerate(x):
        hi = pre.high(c, v)
        weight = 1.0
        if punch > 0:
            level += (abs(hi) - level) * ls
            fast += (level - fast) * (fa if level > fast else rel)
            slow += (level - slow) * (sa if level > slow else rel)
            db = 20 * math.log10((fast + 1e-9) / (slow + 1e-9))
            t = min(max((db - 2) / 10, 0.0), 1.0)
            weight = min(max(1 + punch * (2 * t - 0.5), 0.5), 2.0)
        wet = lp.process(post.high(c, sh.run(hi * boost, bias)))
        part = wet * harmonics * mix * weight
        top[i] = wet * harmonics * mix
        if harmonics2 > 0:
            low = pre2.high(c2, v) - hi
            wet2 = lp2.process(post2.high(c2, sh2.run(low * boost, bias)))
            part += wet2 * harmonics2 * mix * weight
        added[i] = part
    return added, top


def band_db(y, lo, hi):
    spec = np.abs(np.fft.rfft(y * np.hanning(len(y)))) ** 2
    f = np.fft.rfftfreq(len(y), 1 / FS)
    return 10 * math.log10(np.sum(spec[(f >= lo) & (f < hi)]) / len(y) + 1e-30)


# Test 2 -----------------------------------------------------------------------------------
n = 24000
t = np.arange(n) / FS
voice = sum(np.sin(2 * np.pi * 150 * h * t) / h for h in range(1, 20)) * 0.3
seg = slice(n // 2, n)  # the second half, after the filters settle
off, top_off = exciter(voice, 3000, 12, 1.0, -6)
on, top_on = exciter(voice, 3000, 12, 1.0, -6, freq2=400, harmonics2=1.0)
a, b = band_db(off[seg], 400, 1000), band_db(on[seg], 400, 1000)
print("Test 2: the added part, 400 Hz to 1 kHz (second half, Hann):")
print(f"    Band 2 off {a:7.1f} dB, on {b:7.1f} dB, rise {b - a:5.1f} dB (test: over 30)")
print(f"    above 3 kHz: off {band_db(off[seg], 3000, 20000):6.1f} dB, on {band_db(on[seg], 3000, 20000):6.1f} dB (reported, not tested)")
print(f"    top band's part identical with Band 2 on: {np.array_equal(top_off, top_on)}")

# Test 3 -----------------------------------------------------------------------------------
n = 16800  # 350 ms
t = np.arange(n) / FS
pluck = sum(np.sin(2 * np.pi * 220 * h * t) * np.exp(-t / 0.25) / h for h in range(1, 12)) * 0.5


def att_sus(y):
    att = 10 * math.log10(np.mean(y[:int(0.02 * FS)] ** 2) + 1e-30)
    sus = 10 * math.log10(np.mean(y[int(0.1 * FS):int(0.3 * FS)] ** 2) + 1e-30)
    return att, sus


print("Test 3: a 220 Hz pluck, harmonics decaying over 0.25 s, Freq 2 kHz, Clip Boost 12 dB:")
diffs = []
for p in (0.0, 1.0):
    y, _ = exciter(pluck, 2000, 12, 1.0, -6, punch=p)
    att, sus = att_sus(y)
    diffs.append(att - sus)
    print(f"    Punch {int(p * 100):3d} %: attack {att:6.1f} dB, sustain {sus:6.1f} dB, difference {att - sus:5.2f} dB")
print(f"    grows by {diffs[1] - diffs[0]:5.2f} dB (test: 10 to 14)")
