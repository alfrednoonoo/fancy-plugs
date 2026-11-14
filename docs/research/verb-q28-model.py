"""Model for Fancy Verb's Q2.8 Reverb generator (docs/verb.md, "Phase 3 Q2.8", Decision 1 A).

Pure Python. Checks two things before the recipes are baked into the library:

1. The image-source early pattern: how many reflections arrive in the first 80 ms for each
   Early type and Size, and when the first one comes, so Room and Hall read as different.
2. The per-band decay split: three independent noises through 4th-order Butterworth filters
   (low pass at 250 Hz, band 250 Hz-4 kHz, high pass at 4 kHz) sum to a flat power spectrum,
   and an octave band at 125 Hz or 8 kHz holds almost only its own band, so its RT60 reads true.

Run: python docs/research/verb-q28-model.py
"""

import math

SPEED = 343.0


def box(early, size):
    """Room and Hall proportions, the order of reflections, and the wall absorption factor."""
    if early == "room":
        return (size, 0.75 * size, 0.45 * size), 3, 0.80
    return (size, 0.6 * size, 0.5 * size), 2, 0.85


def image(n, length, position):
    return n * length + (position if n % 2 == 0 else length - position)


def pattern(early, size, ear_offset=0.09):
    dims, order, absorb = box(early, size)
    source = (0.3 * dims[0], 0.4 * dims[1], 0.5 * dims[2])
    ear = (0.7 * dims[0], 0.5 * dims[1] - ear_offset, 0.4 * dims[2])
    direct = math.dist(source, ear)
    out = []
    for nx in range(-order, order + 1):
        for ny in range(-order, order + 1):
            for nz in range(-order, order + 1):
                k = abs(nx) + abs(ny) + abs(nz)
                if k == 0 or k > order:
                    continue
                p = (image(nx, dims[0], source[0]), image(ny, dims[1], source[1]), image(nz, dims[2], source[2]))
                d = math.dist(p, ear)
                out.append(((d - direct) / SPEED, absorb ** k * direct / d))
    return sorted(out)


def butter_power(f, fc, high):
    r = (f / fc) ** 8
    return r / (1 + r) if high else 1 / (1 + r)


def band_powers(f):
    low = butter_power(f, 250.0, False)
    high = butter_power(f, 4000.0, True)
    mid = butter_power(f, 250.0, True) * butter_power(f, 4000.0, False)
    return low, mid, high


if __name__ == "__main__":
    print("Early pattern: reflections in the first 80 ms, first arrival, strongest gain")
    for early in ("room", "hall"):
        for size in (4.0, 10.0, 24.0, 50.0):
            refl = pattern(early, size)
            within = [r for r in refl if r[0] <= 0.080]
            print(f"  {early:4s} {size:5.1f} m: {len(within):3d} in 80 ms, first {refl[0][0] * 1000:6.2f} ms, "
                  f"last {refl[-1][0] * 1000:6.1f} ms, strongest {max(g for _, g in refl):.2f}")

    print("Band split: summed power (should be 1) and the own-band share at octave centres")
    for f in (63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000):
        low, mid, high = band_powers(f)
        total = low + mid + high
        own = low if f < 250 else high if f > 4000 else mid
        print(f"  {f:6d} Hz: total {10 * math.log10(total):+.2f} dB, own band {own / total * 100:5.1f} %")
