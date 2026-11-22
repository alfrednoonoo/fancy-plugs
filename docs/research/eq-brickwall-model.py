"""Model of Fancy EQ's brickwall cut slope (docs/eq.md, Phase 4 Wave 2, item 15 of the order).

EQ's cuts go to 48 dB/oct (8th order, four Butterworth sections). A "brickwall" cut is for jobs
where everything past the corner must go and nothing before it may be touched: rumble under a
vocal, or the top octave before a lossy encoder. Steeper minimum-phase filters ring more: their
impulse response lasts longer and the low end of a kick smears.

For a high-pass at 100 Hz, minimum phase (Butterworth cascades), at 48 kHz, this prints:
  - the attenuation a third of an octave and an octave below the corner;
  - the level change a third of an octave above it (what's kept should stay);
  - the ringing: how long the impulse response takes to fall 60 dB below its peak.

In Linear phase mode EQ builds its FIR from the magnitude response, so there a brickwall is the
same magnitude with no phase shift; its ringing is symmetric, before and after, and its length is
the FIR's.

Run: py -3 docs/research/eq-brickwall-model.py
"""
import math

import numpy as np

RATE = 48000.0


def butterworth_qs(order):
    return [1 / (2 * math.cos(math.pi * (2 * k + 1) / (2 * order))) for k in range(order // 2)]


def response_db(order, f, corner):
    # Butterworth high-pass magnitude: 1 / sqrt(1 + (fc/f)^(2n)).
    return -10 * math.log10(1 + (corner / f) ** (2 * order))


def impulse_ring_ms(order, corner):
    n = int(0.5 * RATE)
    x = np.zeros(n)
    x[0] = 1.0
    y = x
    for q in butterworth_qs(order):
        g = math.tan(math.pi * corner / RATE)
        k = 1 / q
        a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2
        ic1 = ic2 = 0.0
        out = np.empty(n)
        for i, v in enumerate(y):
            v3 = v - ic2
            v1 = a1 * ic1 + a2 * v3
            v2 = ic2 + a2 * ic1 + a3 * v3
            ic1, ic2 = 2 * v1 - ic1, 2 * v2 - ic2
            out[i] = v - k * v1 - v2
        y = out
    envelope = np.abs(y)
    peak = envelope.max()
    above = np.flatnonzero(envelope > peak * 1e-3)
    return 1000 * above[-1] / RATE


corner = 100.0
print("Fancy EQ high-pass at 100 Hz, minimum phase:")
for slope in (24, 48, 72, 96):
    order = slope // 6
    third = response_db(order, corner / 2 ** (1 / 3), corner)
    octave = response_db(order, corner / 2, corner)
    above = response_db(order, corner * 2 ** (1 / 3), corner)
    print(f"  {slope:2d} dB/oct: a third of an octave below {third:6.1f} dB, an octave below {octave:6.1f} dB, "
          f"a third above {above:5.2f} dB, rings for {impulse_ring_ms(order, corner):5.1f} ms (to -60 dB)")
