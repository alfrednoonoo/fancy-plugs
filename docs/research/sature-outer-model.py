"""Model of Fancy Sature's outer-band drive (docs/sature.md, Phase 4 Wave 2, item 5 of the order).

Sature adds `added = saturated(band) - band` to the dry signal, so each band only adds its own
difference and no crossover has to sum flat. Below Low and above High get their own Drive and Mix
through the same path. What matters at the edges is how much of a frequency each band hears:

  - the mid band is a 48 dB/oct high-pass at Low times a 48 dB/oct low-pass at High;
  - the low band a 48 dB/oct low-pass at Low, the high band a 48 dB/oct high-pass at High.

An 8th-order Butterworth low-pass and high-pass at the same frequency are power complementary:
|LP|^2 + |HP|^2 = 1 at every frequency. So a sine at Low is heard by the low band and the mid band
at -3 dB each, and the energy driven into the saturators is the same as one band's anywhere else.
Also prints how much a 60 Hz sine leaks into the mid band at Low 200 Hz (the existing test) and
into the low band at 2 kHz, so each band's drive stays in its band.

Run: py -3 docs/research/sature-outer-model.py
"""
import math


def butterworth_power(f, fc, order, high):
    ratio = (f / fc) ** (2 * order)
    return ratio / (1 + ratio) if high else 1 / (1 + ratio)


LOW, HIGH, ORDER = 200.0, 6000.0, 8

print("Power each band hears (dB), Low 200 Hz, High 6 kHz, 48 dB/oct:")
print(f"{'Hz':>7} {'low':>8} {'mid':>8} {'high':>8} {'sum (power)':>12}")
for f in (30, 60, 100, 200, 400, 1000, 3000, 6000, 12000, 18000):
    low = butterworth_power(f, LOW, ORDER, False)
    mid = butterworth_power(f, LOW, ORDER, True) * butterworth_power(f, HIGH, ORDER, False)
    high = butterworth_power(f, HIGH, ORDER, True)
    db = lambda p: 10 * math.log10(max(p, 1e-30))
    print(f"{f:7} {db(low):8.1f} {db(mid):8.1f} {db(high):8.1f} {db(low + mid + high):12.3f}")

print("\nThe three bands' powers sum to 1 (0.000 dB) at every frequency where Low and High are an")
print("octave or more apart, so turning the outer drives up never saturates any frequency twice over.")
print(f"60 Hz into the mid band: {10 * math.log10(butterworth_power(60, LOW, ORDER, True)):.1f} dB (the existing test's -84 dB); "
      f"2 kHz into the low band: {10 * math.log10(butterworth_power(2000, LOW, ORDER, False)):.1f} dB.")
print("At Drive 0 an outer band's `added` is 0, and its path is skipped: today's output to the bit.")
