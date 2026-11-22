"""Model of Fancy Chorus (docs/chorus.md, draft): what the knobs do to pitch, comb and image.

A chorus voice is the input read from a delay line whose length moves:
    d(t) = Delay + Depth x range x lfo(t),   lfo in -1..1.
Its pitch moves by the rate the delay changes: ratio = 1 - d'(t), so the largest detune is
    cents = 1200 x log2(1 + 2 pi x Rate x Depth x range)       (sine LFO).

Checks:
  1. Detune in cents for Rate 0.1 to 5 Hz and Depth 25 to 100 %, with range = 5 ms, to choose
     the range so the defaults (0.8 Hz, 45 %) sit in a chorus's usual 10 to 25 cents.
  2. Ensemble mode's two LFOs per voice (Rate and 6.7 x Rate at 7 % of the depth): the detune is
     no longer one sine, so the voices don't beat in step.
  3. Dimension mode: two voices with opposite LFOs, one per side, plus each cross-fed into the
     other side inverted at -12 dB. The mono sum of the wet signal: how deep its combs go
     against a plain two-voice chorus panned hard.
  4. Flange: Delay 1 ms, Feedback +80 % and -80 %: the comb's peak gain and its first notch.

Run: py -3 docs/research/chorus-model.py
"""
import math

import numpy as np

RATE = 48000.0
RANGE = 0.005   # Depth 100 % = +-5 ms


def cents(rate_hz, depth, wobble=0.0):
    # Largest d'(t) of depth x range x (sin(wt) + wobble x sin(6.7 wt)).
    t = np.linspace(0, 1 / rate_hz, 4000)
    w = 2 * np.pi * rate_hz
    slope = depth * RANGE * (w * np.cos(w * t) + wobble * 6.7 * w * np.cos(6.7 * w * t))
    return 1200 * math.log2(1 + np.max(np.abs(slope)))


print("1. largest detune (cents), range +-5 ms:")
print("   Rate \\ Depth   25 %    45 %    75 %   100 %")
for rate in (0.1, 0.3, 0.8, 1.5, 3.0, 5.0):
    row = "  ".join(f"{cents(rate, d):6.1f}" for d in (0.25, 0.45, 0.75, 1.0))
    print(f"   {rate:4.1f} Hz      {row}")

print(f"2. Ensemble, 0.8 Hz, 45 %: {cents(0.8, 0.45, 0.07):.1f} cents (one sine: {cents(0.8, 0.45):.1f})")


def response(delays_s, gains, freqs):
    out = np.zeros(len(freqs), complex)
    for d, g in zip(delays_s, gains):
        out += g * np.exp(-2j * np.pi * freqs * d)
    return 20 * np.log10(np.maximum(np.abs(out), 1e-9))


freqs = np.linspace(20, 20000, 20000)
cross = 10 ** (-12 / 20)
# Wet only, voices at 7 ms and 9 ms (the LFOs' midpoints), summed to mono.
plain = response([0.007, 0.009], [1.0, 1.0], freqs)
dimension = response([0.007, 0.009, 0.007, 0.009], [1.0, 1.0, -cross, -cross], freqs)
print(f"3. mono sum of the wet signal: plain two voices, deepest notch {plain.min():.1f} dB; "
      f"Dimension's cross-feed {dimension.min():.1f} dB, spread {np.ptp(dimension):.1f} dB against {np.ptp(plain):.1f} dB")

for fb in (0.8, -0.8):
    # A delay of 1 ms with feedback: H = 1 / (1 - fb z^-d), plus the dry signal.
    z = np.exp(-2j * np.pi * freqs * 0.001)
    h = 1 + z / (1 - fb * z)
    db = 20 * np.log10(np.abs(h))
    print(f"4. Flange 1 ms, Feedback {fb:+.0%}: peak {db.max():+.1f} dB, first notch at {freqs[np.argmin(db[:2000])]:.0f} Hz")
