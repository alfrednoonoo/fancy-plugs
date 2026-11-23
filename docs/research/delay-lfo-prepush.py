"""Pre-push check for Fancy Delay's LFO shape and stereo phase (docs/delay.md, Phase 4 Wave 2, item 22).

Follows plugins/delay/src/DelayDsp.cpp's modulation step by step (the phasor, the cycle counter,
the random walk, the rounded triangle, the Shape crossfade and the stereo offset) and puts a tone
through a delay line read at the modulated position, then measures what tests/DelayW2Tests.cpp
measures, the same way:

  Test 2: Triangle, Depth 12 cents at 0.8 Hz, Random 0, a 1 kHz tone through a 150 ms tap. Each
          cycle's frequency from interpolated upward zero crossings after 0.5 s: the share within
          10 % of +-12 cents (want at least 80 %), and the largest change between cycles (want
          under 1 cent).
  Test 3: Stereo 0: left and right offsets identical. Stereo 180, Sine, Random 0: the per-cycle
          pitch of left and right correlates under -0.9.
  Test 4: a 200 Hz tone at 0.25 dry plus a 20 ms tap, Depth 12 cents at 0.8 Hz, Random 30 %,
          Shape switched Sine > Triangle at 0.2 s and back at 0.5 s, Stereo 90 > 0 at 0.3 s and
          > 180 at 0.6 s, 64-sample blocks: the worst second difference of the left output after
          50 ms (want under 2e-3, as Character's switch test).

The delay read is evaluated on the exact tone (no interpolation error), which the C++ Hermite read
matches to well under the margins here.

Run: python docs/research/delay-lfo-prepush.py
"""
import math

import numpy as np

RATE = 48000.0
CORNER = 0.1
PEAK = 0.25 - 0.5 * CORNER + CORNER / math.pi
SCALE = 2.0 * math.pi * PEAK
PARAMETER_MS = 30.0
CHARACTER_MS = 30.0


def rounded_triangle(cycle):
    s = cycle - 0.25
    s -= math.floor(s + 0.5)
    s = abs(s)
    half = 0.5 * CORNER
    bend = CORNER / math.pi
    if s < half:
        p = PEAK - bend * (1.0 - math.cos(math.pi * s / CORNER))
    elif s > 0.5 - half:
        p = -PEAK + bend * (1.0 - math.cos(math.pi * (0.5 - s) / CORNER))
    else:
        p = PEAK - bend - (s - half)
    return p / PEAK


class Smoother:
    def __init__(self, ms, value):
        self.c = 1.0 - math.exp(-1.0 / (ms * 0.001 * RATE))
        self.current = self.target = value

    def next(self):
        if self.current != self.target:
            self.current += (self.target - self.current) * self.c
            if abs(self.target - self.current) < 1e-9:
                self.current = self.target
        return self.current


def offsets(n, depth, rate_hz, random, events, shape=0, stereo_deg=90.0):
    """The left and right read offsets in samples, per sample. events: {sample: (shape, stereo)}
    applied at that sample (the block start)."""
    shape_mix = Smoother(CHARACTER_MS, float(shape))
    stereo = Smoother(PARAMETER_MS, stereo_deg / 360.0)
    two_pi = 2.0 * math.pi
    amplitude = (2.0 ** (depth / 1200.0) - 1.0) / (two_pi * rate_hz) * RATE
    step = two_pi * rate_hz / RATE
    ss, sc = math.sin(step), math.cos(step)
    msin, mcos, phase = 0.0, 1.0, 0.0
    walk = walk_target = 0.0
    countdown = 0
    noise = 1
    st_for, st_sin, st_cos = 0.25, 1.0, 0.0
    out = np.zeros((2, n))
    for i in range(n):
        if i in events:
            sh, deg = events[i]
            shape_mix.target = float(sh)
            stereo.target = deg / 360.0
        m = shape_mix.next()
        o = stereo.next()
        ns = msin * sc + mcos * ss
        nc = mcos * sc - msin * ss
        norm = 1.5 - 0.5 * (ns * ns + nc * nc)
        msin, mcos = ns * norm, nc * norm
        countdown -= 1
        if countdown <= 0:
            noise = (noise * 1664525 + 1013904223) & 0xFFFFFFFF
            walk_target = (noise >> 8) / 8388608.0 - 1.0
            countdown = max(1, int(RATE / (rate_hz * 4.0)))
        walk += (walk_target - walk) * (rate_hz * 6.0 / RATE)
        phase += rate_hz / RATE
        phase -= math.floor(phase)
        if m == 0.0 and o == 0.25:
            for ch, sine in enumerate((msin, mcos)):
                out[ch, i] = amplitude * (1.0 + (1.0 - random) * sine + random * walk)
            continue
        if o != st_for:
            st_sin, st_cos, st_for = math.sin(two_pi * o), math.cos(two_pi * o), o
        sines = (msin, msin * st_cos + mcos * st_sin)
        centre = 1.0 + m * (SCALE - 1.0)
        for ch in range(2):
            shaped = sines[ch]
            if m > 0.0:
                tri = rounded_triangle(phase + (0.0 if ch == 0 else o))
                shaped += m * (SCALE * tri - shaped)
            out[ch, i] = amplitude * (centre + (1.0 - random) * shaped + random * walk)
    return out


def cycle_cents(y, start, hz):
    crossings = []
    for i in range(start, len(y)):
        if y[i - 1] < 0.0 <= y[i]:
            crossings.append((i - 1) + y[i - 1] / (y[i - 1] - y[i]))
    freqs = RATE / np.diff(crossings)
    return 1200.0 * np.log2(freqs / hz)


def tone_through(off, delay_samples, hz, amplitude):
    n = np.arange(len(off))
    return amplitude * np.sin(2.0 * math.pi * hz * (n - delay_samples - off) / RATE)


# Test 2 --------------------------------------------------------------------------------------
n = int(4.0 * RATE)
off = offsets(n, 12.0, 0.8, 0.0, {}, shape=1)
y = tone_through(off[0], 0.15 * RATE, 1000.0, 0.5)
y[: int(0.15 * RATE)] = 0.0
cents = cycle_cents(y, int(0.5 * RATE), 1000.0)
held = np.mean(np.abs(cents) >= 0.9 * 12.0)
step = np.max(np.abs(np.diff(cents)))
print(f"Test 2: Triangle holds within 10 % of +-12 cents {100 * held:.1f} % of the time (want >= 80), "
      f"largest change {step:.3f} cents per cycle (want < 1); range {cents.min():+.2f} to {cents.max():+.2f}")

# Test 3 --------------------------------------------------------------------------------------
for sh in (0, 1):
    off = offsets(int(2.0 * RATE), 12.0, 0.8, 0.3, {}, shape=sh, stereo_deg=0.0)
    print(f"Test 3: Stereo 0, shape {sh}: left and right offsets identical: {np.array_equal(off[0], off[1])}")
off = offsets(n, 12.0, 0.8, 0.0, {}, shape=0, stereo_deg=180.0)
left = cycle_cents(np.where(np.arange(n) < 0.15 * RATE, 0.0, tone_through(off[0], 0.15 * RATE, 1000.0, 0.5)), int(0.5 * RATE), 1000.0)
right = cycle_cents(np.where(np.arange(n) < 0.15 * RATE, 0.0, tone_through(off[1], 0.15 * RATE, 1000.0, 0.5)), int(0.5 * RATE), 1000.0)
k = min(len(left), len(right))
corr = np.corrcoef(left[:k], right[:k])[0, 1]
print(f"Test 3: Stereo 180, per-cycle pitch correlation {corr:+.3f} (want < -0.9)")

# Test 4 --------------------------------------------------------------------------------------
n = int(1.0 * RATE)
blocks = lambda t: int(round(t * RATE / 64)) * 64
events = {blocks(0.2): (1, 90.0), blocks(0.3): (1, 0.0), blocks(0.5): (0, 0.0), blocks(0.6): (0, 180.0)}
off = offsets(n, 12.0, 0.8, 0.3, events)
idx = np.arange(n)
dry = 0.25 * np.sin(2.0 * math.pi * 200.0 * idx / RATE)
start = int(0.05 * RATE)
for ch, name in enumerate(("left", "right")):
    wet = np.where(idx >= 0.02 * RATE, tone_through(off[ch], 0.02 * RATE, 200.0, 0.25), 0.0)
    y = dry + wet
    second = np.abs(y[2:] - 2.0 * y[1:-1] + y[:-2])
    quiet = np.max(second[start: blocks(0.2) - 2])
    print(f"Test 4 ({name}): worst second difference {np.max(second[start:]):.2e} (want < 2e-3); before any switch {quiet:.2e}")
