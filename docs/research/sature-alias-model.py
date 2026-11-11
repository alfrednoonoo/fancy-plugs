# Pure-Python aliasing model: 10 kHz sine at -6 dBFS, Drive 24 dB, 44.1 kHz,
# ideal up/down filters, optional first-order ADAA. The signal repeats every
# 441 base samples' worth, so one period's DFT is exact.
import math, cmath
FS, F0, AMP = 44100, 10000, 0.5

def knee(k):
    w = 1 - k; e = 2 - k
    def f(u):
        a = abs(u)
        y = a if a <= k else (1.0 if a >= e else 1 - (e - a) ** 2 / (4 * w))
        return math.copysign(y, u)
    def F(u):
        a = abs(u)
        r = 0.5 * min(a, k) ** 2
        if a > k:
            t = min(a, e)
            r += (t - k) - ((e - k) ** 3 - (e - t) ** 3) / (12 * w)
        if a > e:
            r += a - e
        return r
    return f, F

alg = (lambda u: u / math.sqrt(1 + u * u), lambda u: math.sqrt(1 + u * u))

def run(f, F, factor, adaa, drive_db):
    n = 441 * factor          # one period at the oversampled rate
    g = AMP * 10 ** (drive_db / 20)
    u = [g * math.sin(2 * math.pi * F0 * i / (FS * factor)) for i in range(n)]
    y = []
    for i in range(n):
        p = u[i - 1]
        d = u[i] - p
        if not adaa:
            y.append(f(u[i]))
        elif abs(d) < 1e-9:
            y.append(f(0.5 * (u[i] + p)))
        else:
            y.append((F(u[i]) - F(p)) / d)
    binhz = FS * factor / n   # 100 Hz
    worst = 0.0
    for b in range(1, int(22050 / binhz) + 1):
        hz = b * binhz
        if hz % F0 == 0:
            continue
        s = sum(y[i] * cmath.exp(-2j * math.pi * b * i / n) for i in range(n))
        worst = max(worst, 2 * abs(s) / n)
    return 20 * math.log10(worst + 1e-30)

if __name__ == "__main__":
  for name, curve in (("Clip 0.9-1.1", knee(0.9)), ("Transistor 0.5-1.5", knee(0.5)), ("Tape alg", alg)):
      for factor in (1, 2, 4, 8):
          for adaa in (False, True):
              print(f"{name:20s} x{factor} {'ADAA1' if adaa else 'naive'}  drive24: {run(*curve, factor, adaa, 24):7.1f} dBFS")
