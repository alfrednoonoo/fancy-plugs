# Model of Fancy EQ's Match fit (Phase 3 Q2.4), to check the greedy + Levenberg-Marquardt fit
# before pushing: plugins/eq/src/EqMatch.cpp follows this step for step. It uses analog
# prototypes for the bell and the second-order shelves, which is what the matched filters follow.
#
#   python docs/research/eq-match-model.py
import cmath
import math

POINTS = 479
PPO = 48


def hz_at(i):
    return 20.0 * 2.0 ** (i / PPO)


HZ = [hz_at(i) for i in range(POINTS)]


def bell(f0, g, q, f):
    a = 10 ** (g / 40)
    s = 1j * f / f0
    return abs((s * s + s * a / q + 1) / (s * s + s / (a * q) + 1))


def shelf(f0, g, low, f):
    r = (10 ** (g / 20)) ** 0.25
    s = 1j * f / f0
    k = math.sqrt(2)
    if low:
        return abs((s * s + k * r * s + r * r) / (s * s + k / r * s + 1 / (r * r)))
    return abs((r * r * s * s + k * r * s + 1) / (s * s / (r * r) + k / r * s + 1))


def curve_of(band):
    kind, f0, g, q = band
    if kind == 'bell':
        return [20 * math.log10(bell(f0, g, q, f)) for f in HZ]
    return [20 * math.log10(shelf(f0, g, kind == 'low', f)) for f in HZ]


def smooth(curve, octaves):
    half = octaves * PPO * 0.5
    reach = int(math.floor(half))
    if reach < 1:
        return list(curve)
    out = []
    for i in range(POINTS):
        s = w = 0.0
        for o in range(-reach, reach + 1):
            j = i + o
            if 0 <= j < POINTS:
                k = 0.5 + 0.5 * math.cos(math.pi * o / (half + 1))
                s += k * curve[j]
                w += k
        out.append(s / w)
    return out


def mean_between(c, lo, hi):
    v = [c[i] for i in range(POINTS) if lo <= HZ[i] <= hi]
    return sum(v) / len(v) if v else 0.0


def make_target(diff, existing, smoothing, amount, lo, hi):
    t = smooth(diff, smoothing)
    m = mean_between(t, 200, 5000)
    out = []
    for i in range(POINTS):
        f = HZ[i]
        o = math.log2(lo / f) if f < lo else (math.log2(f / hi) if f > hi else 0.0)
        fade = 0.5 + 0.5 * math.cos(math.pi * min(1.0, o / (1 / 3)))
        wanted = max(-12, min(12, amount * (t[i] - m)))
        out.append(fade * (wanted - existing[i]))
    return out


class Grid:
    def __init__(self, lo, hi):
        self.inside = [lo <= f <= hi for f in HZ]
        self.w = [(2.0 if 100 <= f <= 10000 else 1.0) if ins else 0.25 for f, ins in zip(HZ, self.inside)]
        self.rw = [math.sqrt(x) for x in self.w]
        self.wsum = sum(self.w)

    def error(self, model, target):
        return math.sqrt(sum(w * (m - t) ** 2 for w, m, t in zip(self.w, model, target)) / self.wsum)

    # What the refinement minimises: the error, and a little for every dB of gain, so two bands
    # never grow against each other.
    def cost(self, model, target, bands):
        return self.error(model, target) ** 2 + RIDGE ** 2 * sum(b[2] ** 2 for b in bands)


STEPS = [0.02, 0.05, 0.03]


def nparams(b):
    return 3 if b[0] == 'bell' else 2


def getp(b, i):
    return math.log2(b[1]) if i == 0 else (b[2] if i == 1 else math.log2(b[3]))


RANGE = [30.0, 18000.0]
RIDGE = 0.03  # 1 dB of band gain costs as much as this many dB of RMS error


def freq_bounds(kind):
    lo, hi = RANGE
    if kind == 'low':
        return lo, max(lo, 1000.0)
    if kind == 'high':
        return min(hi, 1000.0), hi
    return lo, hi


def setp(b, i, v):
    b = list(b)
    if i == 0:
        lo, hi = freq_bounds(b[0])
        b[1] = max(lo, min(hi, 2 ** v))
    elif i == 1:
        b[2] = max(-12.0, min(12.0, v))
    else:
        b[3] = max(0.3, min(8.0, 2 ** v))
    return tuple(b)


def build(bands):
    curves = [curve_of(b) for b in bands]
    total = [sum(c[i] for c in curves) for i in range(POINTS)]
    return curves, total


def solve(a, b, n):
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(a[r][c]))
        if abs(a[p][c]) < 1e-14:
            return False
        if p != c:
            a[c], a[p] = a[p], a[c]
            b[c], b[p] = b[p], b[c]
        for r in range(c + 1, n):
            f = a[r][c] / a[c][c]
            if f == 0:
                continue
            for k in range(c, n):
                a[r][k] -= f * a[c][k]
            b[r] -= f * b[c]
    for r in range(n - 1, -1, -1):
        s = b[r] - sum(a[r][k] * b[k] for k in range(r + 1, n))
        b[r] = s / a[r][r]
    return True


def refine(bands, target, grid):
    curves, total = build(bands)
    error = grid.error(total, target)
    cost = grid.cost(total, target, bands)
    n = sum(nparams(b) for b in bands)
    if n == 0:
        return bands, error
    lam = 1e-2
    for _ in range(12):
        cols = []
        for bi, b in enumerate(bands):
            for p in range(nparams(b)):
                v = getp(b, p)
                t = setp(b, p, v + STEPS[p])
                ch = getp(t, p) - v
                if abs(ch) < 0.5 * STEPS[p]:
                    t = setp(b, p, v - STEPS[p])
                    ch = getp(t, p) - v
                mv = curve_of(t)
                cols.append([grid.rw[i] * (mv[i] - curves[bi][i]) / ch if ch else 0.0 for i in range(POINTS)])
        res = [grid.rw[i] * (total[i] - target[i]) for i in range(POINTS)]
        normal = [[sum(cols[j][i] * cols[k][i] for i in range(POINTS)) / grid.wsum for k in range(n)] for j in range(n)]
        grad = [sum(cols[j][i] * res[i] for i in range(POINTS)) / grid.wsum for j in range(n)]
        idx = 0
        for b in bands:
            normal[idx + 1][idx + 1] += RIDGE ** 2
            grad[idx + 1] += RIDGE ** 2 * b[2]
            idx += nparams(b)
        improved = settled = False
        for _attempt in range(6):
            m = [row[:] for row in normal]
            st = [-g for g in grad]
            for j in range(n):
                m[j][j] += lam * normal[j][j] + 1e-9
            if not solve(m, st, n):
                lam *= 6
                continue
            trial = []
            idx = 0
            for b in bands:
                for p in range(nparams(b)):
                    b = setp(b, p, getp(b, p) + st[idx])
                    idx += 1
                trial.append(b)
            tc, tt = build(trial)
            tcost = grid.cost(tt, target, trial)
            if tcost < cost:
                settled = cost - tcost < 1e-6
                bands, curves, total, cost = trial, tc, tt, tcost
                error = grid.error(tt, target)
                lam = max(lam * 0.3, 1e-6)
                improved = True
                break
            lam *= 6
        if not improved or settled:
            break
    return bands, error


def fit(target, max_bands, lo, hi):
    RANGE[0], RANGE[1] = lo, hi
    grid = Grid(lo, hi)
    bands = []
    if max_bands >= 3:
        low_end = mean_between(target, lo, lo * 2)
        high_end = mean_between(target, hi * 0.5, hi)
        low_edge = mean_between(target, lo, lo * 2 ** (1 / 6))
        high_edge = mean_between(target, hi / 2 ** (1 / 6), hi)
        if abs(low_end) >= 1.0 and low_edge * low_end > 0 and abs(low_edge) >= 0.7 * abs(low_end):
            corner = lo * 2
            sg = 1 if low_end > 0 else -1
            for i in range(POINTS):
                if HZ[i] < lo:
                    continue
                corner = HZ[i]
                if target[i] * sg < 0.5 * abs(low_end):
                    break
            bands.append(('low', min(max(lo, 1000.0), max(corner, lo * 1.5)), max(-12, min(12, low_end)), 0.7071))
        if abs(high_end) >= 1.0 and high_edge * high_end > 0 and abs(high_edge) >= 0.7 * abs(high_end):
            corner = hi * 0.5
            sg = 1 if high_end > 0 else -1
            for i in range(POINTS - 1, -1, -1):
                if HZ[i] > hi:
                    continue
                corner = HZ[i]
                if target[i] * sg < 0.5 * abs(high_end):
                    break
            bands.append(('high', max(min(hi, 1000.0), min(corner, hi / 1.5)), max(-12, min(12, high_end)), 0.7071))
    bands, error = refine(bands, target, grid)
    while len(bands) < max_bands:
        _, total = build(bands)
        worst, worst_v = -1, 0.0
        for i in range(POINTS):
            v = grid.w[i] * abs(target[i] - total[i])
            if grid.inside[i] and v > worst_v:
                worst, worst_v = i, v
        if worst < 0:
            break
        r = [target[i] - total[i] for i in range(POINTS)]
        peak = r[worst]
        if abs(peak) < 0.25:
            break
        sg = 1 if peak > 0 else -1
        left = right = worst
        while left > 0 and r[left - 1] * sg > 0.5 * abs(peak):
            left -= 1
        while right < POINTS - 1 and r[right + 1] * sg > 0.5 * abs(peak):
            right += 1
        width = (right - left + 2) / PPO
        q = max(0.3, min(8.0, 1 / (2 * math.sinh(0.5 * math.log(2) * width))))
        cand, cand_err = refine(bands + [('bell', HZ[worst], max(-12, min(12, peak)), q)], target, grid)
        if error - cand_err < min(0.1, 0.05 * error):
            break
        bands, error = cand, cand_err
    bands = [b for b in bands if abs(b[2]) >= 0.1]
    return sorted(bands, key=lambda b: b[1]), error


def rms_between(c, lo, hi):
    v = [c[i] ** 2 for i in range(POINTS) if lo <= HZ[i] <= hi]
    return math.sqrt(sum(v) / len(v))


def run(name, known, max_bands=8, smoothing=1 / 3, lo=30.0, hi=18000.0, ripple=0.0):
    _, diff = build(known)
    if ripple:
        # What a finite average leaves behind, roughly: a deterministic ripple.
        diff = [d + ripple * math.sin(i * 0.9) * math.cos(i * 0.37) for i, d in enumerate(diff)]
    target = make_target(diff, [0.0] * POINTS, smoothing, 1.0, lo, hi)
    bands, err = fit(target, max_bands, lo, hi)
    _, total = build(bands)
    _, true = build(known)
    m = mean_between(smooth(true, smoothing), 200, 5000)
    wanted = [t - m for t in true]
    rms = rms_between([a - b for a, b in zip(total, wanted)], 50, 16000)
    print(f'{name}: {len(bands)} bands, weighted error {err:.3f} dB, RMS against the known EQ 50 Hz-16 kHz {rms:.3f} dB')
    for b in bands:
        print(f'    {b[0]:5s} {b[1]:9.1f} Hz {b[2]:+6.2f} dB Q {b[3]:.2f}')
    return rms


if __name__ == '__main__':
    six = [('low', 120.0, 4.0, 0.707), ('bell', 300.0, -3.0, 1.4), ('bell', 1000.0, 5.0, 2.0),
           ('bell', 2500.0, -4.0, 1.0), ('bell', 6000.0, 3.0, 3.0), ('high', 9000.0, -5.0, 0.707)]
    worst = 0.0
    worst = max(worst, run('six bands, the test case', six))
    worst = max(worst, run('six bands with 0.4 dB of ripple', six, ripple=0.4))
    worst = max(worst, run('four bells', [('bell', 80.0, 6.0, 1.0), ('bell', 450.0, -6.0, 3.0), ('bell', 3000.0, 4.0, 0.7), ('bell', 12000.0, -3.0, 2.0)]))
    worst = max(worst, run('a tilt', [('low', 300.0, 6.0, 0.707), ('high', 3000.0, 6.0, 0.707)]))
    import random
    random.seed(4)
    for case in range(6):
        known = []
        for _ in range(random.randint(2, 7)):
            kind = random.choice(['bell', 'bell', 'bell', 'low', 'high'])
            f = 2 ** random.uniform(math.log2(40 if kind != 'high' else 2000), math.log2(15000 if kind != 'low' else 500))
            known.append((kind, f, random.uniform(-8, 8), 0.707 if kind != 'bell' else 2 ** random.uniform(-1, 2)))
        worst = max(worst, run(f'random {case}', known))
    run('level only', [])
    print('worst RMS', round(worst, 3), 'dB; the test asks for under 1 dB')
