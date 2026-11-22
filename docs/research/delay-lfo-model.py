"""Model of Fancy Delay's LFO shape and stereo phase (docs/delay.md, Phase 4 Wave 2, item 22).

Each tap's modulation moves its read position with a sine at Mod rate (plus Random), scaled so the
peak pitch deviation is Mod depth; the right channel runs a fixed quarter cycle ahead. The pitch is
the read position's rate of change, so the shape of the position LFO decides how the pitch moves:

  Sine:     pitch follows a cosine: it sweeps continuously, lingering at the extremes.
  Triangle: the position ramps up and down, so the pitch holds at +Depth, then at -Depth: a steady
            detune that flips each half cycle (tape-like "warble" at slow rates, a two-voice
            detune at fast ones). Rounding the corners over 10 % of the cycle keeps the flips smooth.

Printed for a 1 kHz tone through one tap, Depth 12 cents, Rate 0.8 Hz: the share of time the pitch
is within 10 % of its peak deviation, the largest pitch step between 1 ms frames, and the RMS
deviation. Then for the stereo phase: how far apart the left and right pitches swing, and how alike
they move (correlation), for offsets of 0, 45, 90 (today), 135 and 180 degrees.

Run: py -3 docs/research/delay-lfo-model.py
"""
import math

import numpy as np

RATE = 48000
DEPTH = 12.0        # cents
LFO_HZ = 0.8
n = int(5 * RATE)
t = np.arange(n) / RATE
cycle = (t * LFO_HZ) % 1.0


def triangle_position(c, round_frac=0.1):
    # A triangle from -1 to 1 whose corners are rounded over round_frac of the cycle.
    tri = np.where(c < 0.5, 4 * c - 1, 3 - 4 * c)
    if round_frac <= 0:
        return tri
    # Round by low-passing the slope over the corner width: integrate a smoothed square slope.
    slope = np.where(c < 0.5, 1.0, -1.0)
    width = max(1, int(round_frac * RATE / LFO_HZ))
    kernel = np.hanning(width)
    kernel /= kernel.sum()
    smooth = np.convolve(np.concatenate([slope[-width:], slope, slope[:width]]), kernel, mode="same")[width:-width]
    pos = np.cumsum(smooth)
    pos -= pos.mean()
    return pos / np.max(np.abs(pos))


def pitch_cents(position):
    velocity = np.gradient(position)
    return DEPTH * velocity / np.max(np.abs(velocity))


shapes = {
    "Sine (today)": np.sin(2 * np.pi * cycle),
    "Triangle, sharp corners": triangle_position(cycle, 0.0),
    "Triangle, rounded 10 %": triangle_position(cycle, 0.1),
}

print(f"Fancy Delay modulation, Depth {DEPTH} cents at {LFO_HZ} Hz:")
for name, position in shapes.items():
    cents = pitch_cents(position)
    frame = int(0.001 * RATE)
    frames = cents[::frame]
    near_peak = np.mean(np.abs(cents) >= 0.9 * DEPTH)
    step = np.max(np.abs(np.diff(frames)))
    print(f"  {name:<24} near the peak {100 * near_peak:4.0f} % of the time, largest step {step:5.2f} cents/ms, "
          f"RMS deviation {math.sqrt(np.mean(cents ** 2)):5.2f} cents")

print("  Stereo phase (sine): left and right pitch")
for offset in (0, 45, 90, 135, 180):
    left = DEPTH * np.cos(2 * np.pi * cycle)
    right = DEPTH * np.cos(2 * np.pi * cycle + math.radians(offset))
    spread = np.max(np.abs(left - right))
    corr = np.mean(left * right) / math.sqrt(np.mean(left ** 2) * np.mean(right ** 2))
    print(f"    {offset:3d} degrees: up to {spread:5.1f} cents apart, correlation {corr:+.2f}")
