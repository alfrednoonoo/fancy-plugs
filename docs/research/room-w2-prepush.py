"""Pre-push check for Fancy Room's Wave 2 tests (docs/room.md, Phase 4 Wave 2, section 5), measuring
what tests/RoomW2Tests.cpp measures, on a transcription of RoomDsp's late path with Low decay's
shelf and Attack's taps (docs/research/room-model.py's render, with ER off, Mod 0, Pre-delay 0,
Low cut 20 Hz and High cut 20 kHz, as the tests set them).

Test 2: an impulse, Decay 2 s, Low decay x0.5 and x2: T30 (-5 to -35 dB of the Schroeder curve) in
        the 63 Hz and 1 kHz octaves, each band two cascaded band-passes (Q 1.4142), as room-model's
        case_rt60.
Test 3: an impulse, Decay 1.8 s, Attack 0 and 150 ms: the 10 ms frame where L^2 + R^2 peaks, and
        the total energy against Attack 0's. The network takes about 70 ms to build up at Attack 0,
        so the test checks that Attack 150 ms moves the peak 120 to 180 ms later (the note's "peaks
        between 120 and 180 ms" was measured on a tail that starts at full density).

Run: python docs/research/room-w2-prepush.py   (numpy; about a minute per render)
"""
import math

import numpy as np

FS = 48000
LINE_MS = [40.0, 43.1, 46.7, 49.9, 53.3, 57.6, 61.2, 65.9, 70.3, 74.6, 79.7, 84.1, 89.9, 95.3, 101.6, 108.7]
DIFF_MS = ([4.77, 3.59, 12.73, 9.31], [4.91, 3.37, 13.17, 9.67])
TAPS = 16


def tap_weights():
    w = np.array([0.5 - 0.5 * math.cos(math.pi * (k + 1) / TAPS) for k in range(TAPS)])
    return w / math.sqrt(np.sum(w ** 2))


def biquad(kind, f0, q=0.7071):
    w0 = 2 * math.pi * f0 / FS
    c, s = math.cos(w0), math.sin(w0)
    al = s / (2 * q)
    if kind == "bp":
        b = [al, 0.0, -al]
    elif kind == "hp":
        b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]
    else:
        b = [(1 - c) / 2, 1 - c, (1 - c) / 2]
    a = [1 + al, -2 * c, 1 - al]
    return [x / a[0] for x in b], [1.0, a[1] / a[0], a[2] / a[0]]


def filt(coef, x):
    (b0, b1, b2), (_, a1, a2) = coef
    y = np.empty(len(x))
    s1 = s2 = 0.0
    for i, v in enumerate(x):
        o = b0 * v + s1
        s1 = b1 * v - a1 * o + s2
        s2 = b2 * v - a2 * o
        y[i] = o
    return y


def render(n_total, decay, low_decay=1.0, attack_ms=0.0, size=0.55, damping=0.45, diffusion=0.70):
    scale = 0.25 + 0.75 * size
    lengths = [round(ms * scale * FS / 1000.0) for ms in LINE_MS]
    n_lines = 16
    g = [10 ** (-3.0 * d / (FS * decay)) for d in lengths]
    ghf = [10 ** (-3.0 * d / (FS * decay * (1.0 - 0.7 * damping))) for d in lengths]
    bco = [(1 - gh / gg) / (1 + gh / gg) for gg, gh in zip(g, ghf)]
    shelf = low_decay != 1.0
    wc = math.tan(math.pi * 250.0 / FS)
    sb0, sb1, sa1 = [], [], []
    for d, gg in zip(lengths, g):
        glow = 10 ** (-3.0 * d / (FS * decay * low_decay)) / gg
        sb0.append((1 + glow * wc) / (1 + wc))
        sb1.append((glow * wc - 1) / (1 + wc))
        sa1.append((wc - 1) / (1 + wc))
    ss = [0.0] * n_lines
    sign = [1.0 if (i // 2) % 2 == 0 else -1.0 for i in range(n_lines)]
    gin = 1.0 / math.sqrt(n_lines / 2)
    hscale = 1.0 / math.sqrt(n_lines)
    size_pow2 = 1 << (max(lengths) + 8).bit_length()
    mask = size_pow2 - 1
    bufs = [[0.0] * size_pow2 for _ in range(n_lines)]
    lp = [0.0] * n_lines
    ap_len = [[max(1, round(ms * scale * FS / 1000.0)) for ms in DIFF_MS[ch]] for ch in (0, 1)]
    ap_buf = [[[0.0] * 8192 for _ in range(4)] for _ in (0, 1)]
    apg = 0.75 * diffusion

    # The input: an impulse, L 1 and R 0.7, then Attack's taps (exact sample positions here).
    span = attack_ms * FS / 1000.0
    weights = tap_weights() if attack_ms > 0 else np.array([1.0])
    positions = [round(span * k / (TAPS - 1)) for k in range(TAPS)] if attack_ms > 0 else [0]
    feedL = np.zeros(n_total)
    for p, w in zip(positions, weights):
        feedL[p] += w
    feedR = 0.7 * feedL

    outL, outR = np.zeros(n_total), np.zeros(n_total)
    v = [0.0] * n_lines
    w = 0
    for n in range(n_total):
        diffused = []
        for ch, x in ((0, feedL[n]), (1, feedR[n])):
            s = x
            for k in range(4):
                buf = ap_buf[ch][k]
                pos = n & 8191
                delayed = buf[(pos - ap_len[ch][k]) & 8191]
                y = -apg * s + delayed
                buf[pos] = s + apg * y
                s = y
            diffused.append(s)
        lateL = lateR = 0.0
        for i in range(n_lines):
            rd = bufs[i][(w - lengths[i]) & mask]
            y = g[i] * (1.0 - bco[i]) * rd + bco[i] * lp[i]
            lp[i] = y
            if shelf:
                o = sb0[i] * y + ss[i]
                ss[i] = sb1[i] * y - sa1[i] * o
                y = o
            v[i] = y
            if i & 1:
                lateR += sign[i] * y
            else:
                lateL += sign[i] * y
        h = 1
        while h < n_lines:
            for s0 in range(0, n_lines, 2 * h):
                for j in range(s0, s0 + h):
                    a, bb = v[j], v[j + h]
                    v[j] = a + bb
                    v[j + h] = a - bb
            h *= 2
        for i in range(n_lines):
            bufs[i][w] = v[i] * hscale + gin * sign[i] * diffused[i & 1]
        w = (w + 1) & mask
        outL[n] = lateL * gin
        outR[n] = lateR * gin
    hp, lpf = biquad("hp", 20.0), biquad("lp", 20000.0)
    return filt(lpf, filt(hp, outL)), filt(lpf, filt(hp, outR))


def t30(energy):
    edc = np.cumsum(energy[::-1])[::-1]
    db = 10 * np.log10(np.maximum(edc / edc[0], 1e-30))
    idx = np.where((db <= -5.0) & (db >= -35.0))[0]
    slope = np.polyfit(idx / FS, db[idx], 1)[0]
    return -60.0 / slope


def band_t30(L, R, fc):
    c = biquad("bp", fc, q=1.4142)
    fl, fr = filt(c, filt(c, L)), filt(c, filt(c, R))
    return t30(fl ** 2 + fr ** 2)


print("Test 2: Low decay, Decay 2 s, T30 in the 63 Hz and 1 kHz octaves")
for mult, low_target, low_tol in ((0.5, 1.0, 0.12), (2.0, 4.0, 0.4)):
    L, R = render(int(7.0 * FS), 2.0, low_decay=mult)
    t63, t1k = band_t30(L, R, 63.0), band_t30(L, R, 1000.0)
    ok = abs(t63 - low_target) <= low_tol and abs(t1k - 2.0) <= 0.2
    print(f"    x{mult}: 63 Hz {t63:.3f} s (target {low_target} +- {low_tol}), 1 kHz {t1k:.3f} s (2.0 +- 0.2)  {'ok' if ok else 'FAIL'}")

print("Test 3: Attack, Decay 1.8 s, the 10 ms frame of peak energy and the total energy")
frame = FS // 100
res = {}
for a in (0.0, 150.0):
    L, R = render(int(3.0 * FS), 1.8, attack_ms=a)
    e = L ** 2 + R ** 2
    frames = e[: len(e) // frame * frame].reshape(-1, frame).sum(axis=1)
    res[a] = (10 * int(np.argmax(frames)), 10 * math.log10(np.sum(e)))
    print(f"    Attack {a:5.1f} ms: peak at {res[a][0]} ms, energy {res[a][1]:+.2f} dB")
diff = res[150.0][1] - res[0.0][1]
shift = res[150.0][0] - res[0.0][0]
print(f"    peak {shift} ms later (120..180): {'ok' if 120 <= shift <= 180 else 'FAIL'}; energy {diff:+.2f} dB (within 1): {'ok' if abs(diff) <= 1 else 'FAIL'}")
