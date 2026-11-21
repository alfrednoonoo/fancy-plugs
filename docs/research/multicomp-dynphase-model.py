"""Model of MultiComp's dynamic-phase crossover (docs/multicomp.md, Phase 4 Wave 2, item 1).

Pro-MB's "dynamic phase" leaves the signal untouched while a band does nothing, and puts phase
shift only where gain is changing, with no latency. There's no published method, so this compares
the candidates on three bands (crossovers at 200 Hz and 2 kHz) in the analogue domain:

  S0  today: compensated Linkwitz-Riley 24 (every band shares the all-pass of both crossovers;
      flat sum, but the all-pass phase is always there).
  S1  residual on LR24: out = in + sum (g_k - 1) band_k. Identity at unity, but the bands sum to
      the all-pass, not to 1, so a gain change on all bands isn't a clean gain.
  S2  complementary split from LR24 low-passes: band 1 = L1, band 2 = (1 - L1) L2,
      band 3 = (1 - L1)(1 - L2). Sums to exactly 1: identity at unity and exact for an equal gain.
      The price: each 1 - L side only falls at 6 dB/oct, and has a bump near the crossover.
  S3  S2 with 4th-order "matched" complements: 1 - L where L's numerator matches the
      denominator to s^3, so the high side falls at 24 dB/oct and the low-pass side takes the
      bump instead (Lipshitz-Vanderkooy style, without delay).
  S4  crossfade by activity between the dry input and S0: out = (1 - a) in + a S0. Rejected
      below: halfway, the dry and the all-passed signal cancel near each crossover.

Prints, for each: deviation from the input at unity, the ripple with every band at -6 dB, the
shape of a -6 dB cut on band 2 (how much of it lands outside the band), and the steepest skirt.

Run: py -3 docs/research/multicomp-dynphase-model.py
"""
import numpy as np

F1, F2 = 200.0, 2000.0
hz = np.logspace(np.log10(20.0), np.log10(20000.0), 4000)


def butter2(f, fc):
    s = 1j * f / fc
    d = s * s + np.sqrt(2.0) * s + 1.0
    return 1.0 / d, s * s / d


def lr24(f, fc):
    low, high = butter2(f, fc)
    return low * low, high * high


def matched_low(f, fc):
    # L = N / D with D the LR24 denominator (Butterworth squared), N = D - s^4, so 1 - L = s^4 / D:
    # the complementary high-pass is exactly LR24's high-pass, and L keeps the rest.
    s = 1j * f / fc
    d = (s * s + np.sqrt(2.0) * s + 1.0) ** 2
    return (d - s ** 4) / d


def bands(scheme, f):
    l1, h1 = lr24(f, F1)
    l2, h2 = lr24(f, F2)
    a1, a2 = l1 + h1, l2 + h2

    if scheme in ("S0", "S1", "S4"):
        return [l1 * a2, h1 * l2, h1 * h2]

    if scheme == "S2":
        return [l1, (1 - l1) * l2, (1 - l1) * (1 - l2)]

    if scheme == "S3":
        m1, m2 = matched_low(f, F1), matched_low(f, F2)
        return [m1, (1 - m1) * m2, (1 - m1) * (1 - m2)]

    raise ValueError(scheme)


def response(scheme, gains, activity=1.0):
    b = bands(scheme, hz)

    if scheme == "S0":
        return sum(g * x for g, x in zip(gains, b))

    if scheme == "S4":
        s0 = sum(g * x for g, x in zip(gains, b))
        return (1 - activity) * 1.0 + activity * s0

    # S1, S2, S3: the input plus each band's change.
    return 1.0 + sum((g - 1.0) * x for g, x in zip(gains, b))


def db(x):
    return 20 * np.log10(np.maximum(np.abs(x), 1e-12))


def at(curve, f):
    return float(np.interp(np.log(f), np.log(hz), curve))


def skirt(scheme):
    # Band 2's steepest and shallowest slope an octave or more outside its crossovers.
    b2 = db(bands(scheme, hz)[1])
    low = at(b2, F1 / 4) - at(b2, F1 / 8)
    high = at(b2, F2 * 4) - at(b2, F2 * 8)
    return low, high


half = 10 ** (-6 / 20)

print("scheme  unity: |dB|max  phase max   all -6 dB: ripple   band 2 -6 dB: 50 Hz  632 Hz  8 kHz   skirts dB/oct (low, high)   bump")
for scheme in ("S0", "S1", "S2", "S3"):
    unity = response(scheme, [1, 1, 1])
    every = response(scheme, [half, half, half])
    cut = response(scheme, [1, half, 1])
    bump = max(db(x).max() for x in bands(scheme, hz))
    low, high = skirt(scheme)
    phase = np.degrees(np.abs(np.angle(unity))).max()
    print(f"{scheme:6}  {np.abs(db(unity)).max():13.3f}  {phase:8.1f}   {db(every).max() - db(every).min():16.2f}   "
          f"{at(db(cut), 50):17.2f}  {at(db(cut), 632):6.2f}  {at(db(cut), 8000):6.2f}   {low:10.1f}, {high:5.1f}             {bump:5.2f}")

print()
print("S4, the crossfade, with every band at unity: the dip halfway through a fade")
for a in (0.25, 0.5, 0.75):
    print(f"  activity {a:.2f}: deepest {db(response('S4', [1, 1, 1], a)).min():7.1f} dB")

print()
print("Upward (band 2 at +6 dB): the largest boost outside the band (below 100 Hz or above 4 kHz)")
for scheme in ("S0", "S2", "S3"):
    up = db(response(scheme, [1, 2.0, 1]))
    outside = np.concatenate ([up[hz < 100], up[hz > 4000]])
    print(f"  {scheme}: {outside.max():5.2f} dB")

# S5: S1 with the common part of the gains taken out first: out = r in + sum (g_k - r) band_k.
# An equal change on every band is then a plain gain with no phase, and phase appears only in
# proportion to how far the bands' gains differ. r is a choice: the mean of the gains, their
# geometric mean (the mean in dB), the median, or the gain nearest 1 (min |dB|).
def s5(gains, choice):
    b = bands("S0", hz)
    g = np.array(gains, dtype=float)

    if choice == "mean":
        r = g.mean()
    elif choice == "dB mean":
        r = float(np.exp(np.log(g).mean()))
    elif choice == "median":
        r = float(np.median(g))
    elif choice == "nearest 1":
        r = float(g[np.argmin(np.abs(np.log(g)))])
    else:
        raise ValueError(choice)

    return r + sum((gk - r) * x for gk, x in zip(g, b))


cases = {
    "band 2 -6": [1, half, 1],
    "bands 1, 2 -6": [half, half, 1],
    "typical comp (-3, -6, -1.5 dB)": [10 ** (-3 / 20), half, 10 ** (-1.5 / 20)],
    "band 2 +6 (upward)": [1, 2.0, 1],
    "outer bands -6": [half, 1, half],
    "deep: -12, 0, -12": [0.25, 1, 0.25],
}

print()
print("Magnitude error against today's LR24 shape (|S0|), and the largest phase shift, per gain set")
print(f"{'case':32} " + "  ".join(f"{name:>16}" for name in ("S1", "S5 mean", "S5 dB mean", "S5 median", "S5 nearest 1")))
for name, gains in cases.items():
    target = db(response("S0", gains))
    cells = []
    for candidate in ("S1", "mean", "dB mean", "median", "nearest 1"):
        out = response("S1", gains) if candidate == "S1" else s5(gains, candidate)
        error = np.abs(db(out) - target).max()
        phase = np.degrees(np.abs(np.angle(out))).max()
        cells.append(f"{error:6.2f} dB {phase:4.0f}deg")
    print(f"{name:32} " + "  ".join(f"{c:>16}" for c in cells))

# S6: the band gains as minimum-phase high shelves in series, one at each crossover:
# out = g1 * shelf (f1, g2 / g1) * shelf (f2, g3 / g2). Equal gains make every shelf exactly 1, so
# the signal is untouched; a shelf, and its phase, appears only where neighbouring bands differ.
# Order 2 is one RBJ-style section (Q 0.707); order 4 is two, with Butterworth Qs, each taking
# half the step in dB.
def shelf(f, fc, gain, order):
    s = 1j * f / fc
    qs = [np.sqrt(0.5)] if order == 2 else [0.5411961001461969, 1.3065629648763766]
    out = np.ones_like(s)
    for q in qs:
        a = gain ** (1.0 / (2 * len(qs)))  # each section's sqrt of its share of the gain
        out *= a * (a * s * s + np.sqrt(a) / q * s + 1) / (s * s + np.sqrt(a) / q * s + a)
    return out


def s6(gains, order):
    g1, g2, g3 = gains
    return g1 * shelf(hz, F1, g2 / g1, order) * shelf(hz, F2, g3 / g2, order)


print()
print("S6, shelves in series: magnitude error against |S0| and the largest phase shift")
print(f"{'case':32} {'order 2':>18} {'order 4':>18}   (unity: exact, no phase)")
for name, gains in cases.items():
    target = db(response("S0", gains))
    cells = []
    for order in (2, 4):
        out = s6(gains, order)
        cells.append(f"{np.abs(db(out) - target).max():6.2f} dB {np.degrees(np.abs(np.angle(out))).max():4.0f}deg")
    print(f"{name:32} " + " ".join(f"{c:>18}" for c in cells))

# Where the S6 error sits: the crossover itself. LR sums the bands' amplitudes there (the
# arithmetic mean of the gains), a shelf passes the geometric mean.
gains = [1, half, 1]
target, out = db(response("S0", gains)), db(s6(gains, 4))
worst = hz[np.argmax(np.abs(out - target))]
print(f"\nband 2 -6, order 4: worst error at {worst:.0f} Hz; at 200 Hz S0 {at(target, 200):.2f} dB, S6 {at(out, 200):.2f} dB; "
      f"band centre 632 Hz S0 {at(target, 632):.2f}, S6 {at(out, 632):.2f}")
