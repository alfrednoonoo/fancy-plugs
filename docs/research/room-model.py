"""Fancy Room: a model of the engine and of the brief's 3.4 tests (docs/room.md, Phase 2).

Pure Python, no numpy. The C++ engine is a transcription of this file: the same
constants, tables and order of operations. Run:

    python -I docs/research/room-model.py [metallic rt60 freeze mono tail]
"""
import math
import random
import sys
from multiprocessing import Pool

FS = 48000

# Late network: line lengths in ms at Size 100 %. Eco uses every other line.
LINE_MS = [40.0, 43.1, 46.7, 49.9, 53.3, 57.6, 61.2, 65.9,
           70.3, 74.6, 79.7, 84.1, 89.9, 95.3, 101.6, 108.7]
# Input diffusers: four all-passes per channel, ms at Size 100 %.
DIFF_MS = ([4.77, 3.59, 12.73, 9.31], [4.91, 3.37, 13.17, 9.67])
ER_TAPS = 16
MOD_MAX_MS = 0.6                   # Mod 100 %
FREEZE_RAMP = int(0.020 * FS)      # 20 ms
MOD_STEP = 16                      # samples between exact modulation values
# Every line is modulated: modulating only some (linear reads lose highs on
# those lines alone) left their modes decaying differently, +6 to +10 dB peaks.
QUALITY = {                        # lines, modulated lines, cubic reads
    "eco": (8, tuple(range(8)), False),
    "normal": (16, tuple(range(16)), False),
    "high": (16, tuple(range(16)), True),
}


def frac(x):
    return x - math.floor(x)


def size_scale(size):
    return 0.25 + 0.75 * size


def er_table(ch, scale):
    """(delay in samples, gain) per tap. Times 4..80 ms at Size 100 %."""
    taps = []
    for k in range(ER_TAPS):
        j = frac(k * 0.618034 + ch * 0.31)
        t_ms = 4.0 + 76.0 * ((k + j) / ER_TAPS) ** 1.4
        gain = 0.9 * (1.0 - 0.75 * t_ms / 80.0) * (1.0 if (k * 7 + ch * 3) % 3 else -1.0)
        taps.append((round(t_ms * scale * FS / 1000.0), gain * 0.35))
    return taps


def er_longest_s(size):
    return max(d for ch in (0, 1) for d, _ in er_table(ch, size_scale(size))) / FS


def biquad(kind, f0, q=0.7071):
    w = 2 * math.pi * f0 / FS
    c, s = math.cos(w), math.sin(w)
    a = s / (2 * q)
    if kind == "lp":
        b = [(1 - c) / 2, 1 - c, (1 - c) / 2]
    elif kind == "hp":
        b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]
    else:  # band pass, 0 dB peak
        b = [a, 0.0, -a]
    a0 = 1 + a
    return [b[0] / a0, b[1] / a0, b[2] / a0, -2 * c / a0, (1 - a) / a0]


def filt(coef, x):
    b0, b1, b2, a1, a2 = coef
    y = []
    x1 = x2 = y1 = y2 = 0.0
    for v in x:
        o = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, v, y1, o
        y.append(o)
    return y


DEFAULTS = dict(predelay_ms=12.0, size=0.55, decay=1.8, diffusion=0.70, mod=0.25,
                width=1.0, damping=0.45, lowcut=150.0, highcut=9000.0,
                er_db=0.0, quality="normal", freeze_at=None)


def render(inL, inR, n_total, **kw):
    """Wet output only (Wet 0 dB, Dry off). Input lists may be shorter than n_total."""
    p = dict(DEFAULTS, **kw)
    n_lines, modulated, cubic = QUALITY[p["quality"]]
    lines_ms = LINE_MS if n_lines == 16 else LINE_MS[::2]
    scale = size_scale(p["size"])
    T, D = p["decay"], p["damping"]

    lengths = [round(ms * scale * FS / 1000.0) for ms in lines_ms]
    depth = p["mod"] * MOD_MAX_MS * FS / 1000.0
    is_mod = [i in modulated for i in range(n_lines)]
    rate = [2 * math.pi * (0.31 + 0.97 * frac(i * 0.618034 + 0.17)) / FS for i in range(n_lines)]
    phase = [2 * math.pi * frac(i * 0.4142 + 0.29) for i in range(n_lines)]
    g = [10 ** (-3.0 * d / (FS * T)) for d in lengths]
    ghf = [10 ** (-3.0 * d / (FS * T * (1.0 - 0.7 * D))) for d in lengths]
    bco = [(1 - gh / gg) / (1 + gh / gg) for gg, gh in zip(g, ghf)]
    sign = [1.0 if (i // 2) % 2 == 0 else -1.0 for i in range(n_lines)]
    in_gain = 1.0 / math.sqrt(n_lines / 2)
    out_gain = 1.0 / math.sqrt(n_lines / 2)
    hscale = 1.0 / math.sqrt(n_lines)

    size_pow2 = 1 << (max(lengths) + int(depth) + 8).bit_length()
    mask = size_pow2 - 1
    bufs = [[0.0] * size_pow2 for _ in range(n_lines)]
    lp_state = [0.0] * n_lines

    pre = round(p["predelay_ms"] * FS / 1000.0)
    er = [er_table(0, scale), er_table(1, scale)]
    er_gain = 10 ** (p["er_db"] / 20.0)
    hist_len = 1 << (max(d for t in er for d, _ in t) + pre + 2).bit_length()
    hmask = hist_len - 1
    histL = [0.0] * hist_len
    histR = [0.0] * hist_len

    ap_len = [[round(ms * scale * FS / 1000.0) for ms in DIFF_MS[ch]] for ch in (0, 1)]
    ap_buf = [[[0.0] * (d + 1) for d in ap_len[ch]] for ch in (0, 1)]
    ap_pos = [[0] * 4 for _ in (0, 1)]
    apg = 0.75 * p["diffusion"]

    freeze_at = p["freeze_at"]
    frozen_len = None
    outL, outR = [], []
    w = 0
    nL, nR = len(inL), len(inR)
    v = [0.0] * n_lines
    m0 = [0.0] * n_lines
    m1 = [0.0] * n_lines
    k0 = 0
    for n in range(n_total):
        xL = inL[n] if n < nL else 0.0
        xR = inR[n] if n < nR else 0.0
        hw = n & hmask
        histL[hw] = xL
        histR[hw] = xR

        # Early reflections, from the input delayed by Pre-delay.
        eL = 0.0
        for d, gk in er[0]:
            i = (n - pre - d) & hmask
            eL += gk * (0.75 * histL[i] + 0.25 * histR[i])
        eR = 0.0
        for d, gk in er[1]:
            i = (n - pre - d) & hmask
            eR += gk * (0.25 * histL[i] + 0.75 * histR[i])

        # Diffusers on the pre-delayed input.
        i0 = (n - pre) & hmask
        dl = (histL[i0], histR[i0])
        diffused = []
        for ch in (0, 1):
            s = dl[ch]
            for k in range(4):
                buf = ap_buf[ch][k]
                pos = ap_pos[ch][k]
                delayed = buf[pos]
                y = -apg * s + delayed
                buf[pos] = s + apg * y
                ap_pos[ch][k] = pos + 1 if pos + 1 < len(buf) - 1 else 0
                s = y
            diffused.append(s)

        # Freeze: one 20 ms ramp f from 0 to 1.
        f = 0.0
        if freeze_at is not None and n >= freeze_at:
            f = min(1.0, (n - freeze_at) / FREEZE_RAMP)
            if frozen_len is None:
                frozen_len = lengths[:]  # unmodulated lengths are integers already

        # Modulation: the exact sine at every 16th sample of the absolute count,
        # linear in between (CPU budget: one sine per sample, not one per line).
        if n % MOD_STEP == 0:
            k0 = n
            for i in range(n_lines):
                m0[i] = math.sin(phase[i] + rate[i] * n)
                m1[i] = math.sin(phase[i] + rate[i] * (n + MOD_STEP))
        t = (n - k0) / MOD_STEP

        # Read the lines, damp them, sum the outputs.
        lateL = lateR = 0.0
        for i in range(n_lines):
            if is_mod[i]:
                dmod = lengths[i] + depth * (m0[i] + t * (m1[i] - m0[i]))
                dd = (1.0 - f) * dmod + f * lengths[i]
                ip = math.floor(dd)
                fr = dd - ip
                b = bufs[i]
                if cubic:
                    xm1 = b[(w - ip + 1) & mask]; x0 = b[(w - ip) & mask]
                    x1 = b[(w - ip - 1) & mask]; x2 = b[(w - ip - 2) & mask]
                    c3 = (x2 - xm1) / 6.0 + (x0 - x1) * 0.5
                    c2 = (xm1 + x1) * 0.5 - x0
                    c1 = x1 - x2 / 6.0 - x0 * 0.5 - xm1 / 3.0
                    rd = ((c3 * fr + c2) * fr + c1) * fr + x0
                    # 4-point Lagrange, written as a cubic in fr
                else:
                    a0 = b[(w - ip) & mask]
                    rd = a0 + fr * (b[(w - ip - 1) & mask] - a0)
            else:
                rd = bufs[i][(w - lengths[i]) & mask]
            gi = g[i] + f * (1.0 - g[i])
            bi = bco[i] * (1.0 - f)
            y = gi * (1.0 - bi) * rd + bi * lp_state[i]
            lp_state[i] = y
            v[i] = y
            if i & 1:
                lateR += sign[i] * y
            else:
                lateL += sign[i] * y

        # Mix: fast Walsh-Hadamard, orthonormal.
        h = 1
        while h < n_lines:
            for s0 in range(0, n_lines, 2 * h):
                for j in range(s0, s0 + h):
                    a, bb = v[j], v[j + h]
                    v[j] = a + bb
                    v[j + h] = a - bb
            h *= 2
        gin = in_gain * (1.0 - f)
        for i in range(n_lines):
            bufs[i][w] = v[i] * hscale + gin * sign[i] * diffused[i & 1]
        w = (w + 1) & mask

        outL.append(lateL * out_gain + er_gain * eL)
        outR.append(lateR * out_gain + er_gain * eR)

    hp, lp = biquad("hp", p["lowcut"]), biquad("lp", p["highcut"])
    outL = filt(lp, filt(hp, outL))
    outR = filt(lp, filt(hp, outR))
    W = p["width"]
    L2, R2 = [], []
    for a, b in zip(outL, outR):
        m, s = (a + b) * 0.5, (a - b) * 0.5
        L2.append(m + W * s)
        R2.append(m - W * s)
    return L2, R2


# ---------------------------------------------------------------- measurement

def fft(re, im):
    n = len(re)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            re[i], re[j] = re[j], re[i]
            im[i], im[j] = im[j], im[i]
    size = 2
    while size <= n:
        ang = -2 * math.pi / size
        wr, wi = math.cos(ang), math.sin(ang)
        for s in range(0, n, size):
            cr, ci = 1.0, 0.0
            half = size // 2
            for k in range(half):
                a, b = s + k, s + k + half
                tr = re[b] * cr - im[b] * ci
                ti = re[b] * ci + im[b] * cr
                re[b], im[b] = re[a] - tr, im[a] - ti
                re[a] += tr
                im[a] += ti
                cr, ci = cr * wr - ci * wi, cr * wi + ci * wr
        size *= 2


def welch(chans, start, stop, nfft=4096):
    win = [0.5 - 0.5 * math.cos(2 * math.pi * i / nfft) for i in range(nfft)]
    acc = [0.0] * (nfft // 2)
    count = 0
    for x in chans:
        pos = start
        while pos + nfft <= stop:
            re = [x[pos + i] * win[i] for i in range(nfft)]
            im = [0.0] * nfft
            fft(re, im)
            for k in range(nfft // 2):
                acc[k] += re[k] * re[k] + im[k] * im[k]
            count += 1
            pos += nfft // 2
    return [a / count for a in acc], nfft


def edc_db(e):
    tot = 0.0
    out = [0.0] * len(e)
    for i in range(len(e) - 1, -1, -1):
        tot += e[i]
        out[i] = tot
    top = out[0]
    return [10 * math.log10(max(v / top, 1e-30)) for v in out]


def t30(e):
    d = edc_db(e)
    xs, ys = [], []
    for i, v in enumerate(d):
        if -35.0 <= v <= -5.0:
            xs.append(i / FS)
            ys.append(v)
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    slope = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sum((x - mx) ** 2 for x in xs)
    return -60.0 / slope


def impulse(n=1):
    return [1.0] + [0.0] * (n - 1)


# ---------------------------------------------------------------- tests

def case_metallic(_):
    L, R = render(impulse(), impulse(), int(1.6 * FS), decay=2.0, mod=0.25)
    psd, nfft = welch([L, R], int(0.3 * FS), int(1.5 * FS))
    worst, at = -99.0, 0.0
    for k in range(1, len(psd)):
        f = k * FS / nfft
        if f < 100 or f > 16000:
            continue
        lo, hi = max(1, int(k * 2 ** (-1 / 6))), min(len(psd) - 1, int(math.ceil(k * 2 ** (1 / 6))))
        sm = sum(psd[lo:hi + 1]) / (hi - lo + 1)
        r = 10 * math.log10(psd[k] / sm)
        if r > worst:
            worst, at = r, f
    return f"metallic: worst peak {worst:+.2f} dB over the 1/3-octave average at {at:.0f} Hz (limit +6)"


def case_rt60(args):
    size, decay = args
    n = int((1.3 * decay + 0.4) * FS)
    L, R = render(impulse(), impulse(), n, size=size, decay=decay)
    bands = []
    for fc in (500.0, 1000.0):
        c = biquad("bp", fc, q=1.4142)
        fl, fr = filt(c, filt(c, L)), filt(c, filt(c, R))
        bands.append(t30([a * a + b * b for a, b in zip(fl, fr)]))
    tmid = sum(bands) / 2
    broad = t30([a * a + b * b for a, b in zip(L, R)])
    err = 100 * (tmid / decay - 1)
    return (f"rt60: Size {size*100:3.0f} % Decay {decay:4.1f} s: T_mid {tmid:6.3f} s ({err:+5.1f} %, limit 10)"
            f"  500 Hz {bands[0]:.3f}  1 kHz {bands[1]:.3f}  broadband {broad:.3f}")


def noise(n, db, seed):
    r = random.Random(seed)
    a = 10 ** (db / 20) * math.sqrt(3)
    return [a * (2 * r.random() - 1) for _ in range(n)]


def case_freeze(_):
    secs = 62
    L, R = render(noise(2 * FS, -12, 1), noise(2 * FS, -12, 2), secs * FS, freeze_at=int(1.5 * FS))
    bad = any(math.isnan(x) or math.isinf(x) for x in L + R)
    lv = []
    for s in range(2, secs):
        e = sum(L[i] ** 2 + R[i] ** 2 for i in range(s * FS, (s + 1) * FS))
        lv.append(10 * math.log10(e / FS))
    dev = max(abs(x - lv[0]) for x in lv)
    return (f"freeze: 1 s levels over 60 s within {dev:.3f} dB of the first (limit 1), "
            f"last-first {lv[-1]-lv[0]:+.4f} dB, NaN/inf {bad}")


def case_mono(_):
    L, R = render(impulse(), impulse(), int(2.0 * FS))
    a, b = int(0.05 * FS), len(L)
    mono = sum((L[i] + R[i]) ** 2 for i in range(a, b))
    st = sum(L[i] ** 2 + R[i] ** 2 for i in range(a, b))
    lr = sum(L[i] * R[i] for i in range(a, b)) / math.sqrt(sum(x * x for x in L[a:]) * sum(x * x for x in R[a:]))
    return f"mono: tail mono sum {10*math.log10(mono/st):+.2f} dB against stereo energy (limit 3), L/R correlation {lr:+.3f}"


def case_tail(decay):
    p = DEFAULTS["predelay_ms"] / 1000
    tail = p + er_longest_s(DEFAULTS["size"]) + 1.5 * decay
    n = int((1.0 + tail + 0.3) * FS)
    L, R = render(noise(FS, -6, 3), noise(FS, -6, 4), n, decay=decay)
    win = int(0.05 * FS)
    rms = []
    for s in range(0, n - win, win):
        rms.append(10 * math.log10(max(sum(L[i] ** 2 + R[i] ** 2 for i in range(s, s + win)) / win, 1e-30)))
    peak = max(rms)
    end = int((1.0 + tail) * FS) // win
    after = max(rms[end:]) - peak
    return f"tail: Decay {decay:4.1f} s, reported tail {tail:6.3f} s; loudest 50 ms after it {after:+.1f} dB re peak (limit -90)"


if __name__ == "__main__":
    want = set(sys.argv[1:]) or {"metallic", "rt60", "freeze", "mono", "tail"}
    jobs = []
    if "metallic" in want:
        jobs.append((case_metallic, None))
    if "mono" in want:
        jobs.append((case_mono, None))
    if "rt60" in want:
        jobs += [(case_rt60, (s, d)) for s in (0.2, 0.55, 0.9) for d in (0.5, 2.0, 8.0)]
    if "tail" in want:
        jobs += [(case_tail, d) for d in (0.5, 1.8, 8.0)]
    if "freeze" in want:
        jobs.append((case_freeze, None))
    with Pool() as pool:
        results = [pool.apply_async(fn, (a,)) for fn, a in jobs]
        for r in results:
            print(r.get(), flush=True)
