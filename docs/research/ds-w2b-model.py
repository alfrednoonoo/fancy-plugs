"""Models for Fancy DS's later Wave 2 items (docs/ds.md, Phase 4 Wave 2: items 32 and 33; 34 is DS-Mini's
profile built on 33).

32. Per-channel (Mid/Side) reduction. DS can already listen to the mid or the side, but the cut
    always lands on both channels. On a mix with a centre vocal and wide cymbals, de-essing the
    vocal also dips the cymbals' highs on every "s". Reducing the mid only leaves the sides alone.
    Measured: the "s" reduction and the cymbals' dip, cutting both channels against the mid only.

33. Hiss mode. A de-esser cuts when the highs are loud. Hiss is the opposite problem: it's heard
    when the music is quiet. Hiss mode turns the band above Freq down when its level falls below
    Threshold (a downward expander on a high shelf, up to Range), so pauses lose their hiss and
    loud passages keep their highs. Measured on a vocal with tape hiss at -54 dBFS: the hiss in the
    pauses and the highs' level in the loud parts, against a static high-shelf cut of the same
    depth.

Run: py -3 docs/research/ds-w2b-model.py
"""
import math

import numpy as np

FS = 48000
rng = np.random.default_rng(21)


def svf(x, hz, q, kind):
    g = math.tan(math.pi * hz / FS)
    k = 1 / q
    a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2
    ic1 = ic2 = 0.0
    out = np.empty(len(x))
    for i, v in enumerate(x):
        v3 = v - ic2
        v1 = a1 * ic1 + a2 * v3
        v2 = ic2 + a2 * ic1 + a3 * v3
        ic1, ic2 = 2 * v1 - ic1, 2 * v2 - ic2
        out[i] = v2 if kind == "low" else (k * v1 if kind == "band" else v - k * v1 - v2)
    return out


def level_db(x, release_ms=60.0):
    r = 1 - math.exp(-1 / (release_ms * 0.001 * FS))
    env, out = 0.0, np.empty(len(x))
    for i, v in enumerate(np.abs(x)):
        env = v if v > env else env + (v - env) * r
        out[i] = 20 * math.log10(max(env, 1e-9))
    return out


def smooth(gain_db, attack_ms, release_ms):
    a = 1 - math.exp(-1 / (attack_ms * 0.001 * FS))
    r = 1 - math.exp(-1 / (release_ms * 0.001 * FS))
    env, out = 0.0, np.empty(len(gain_db))
    for i, want in enumerate(gain_db):
        env += (want - env) * (a if want < env else r)
        out[i] = env
    return out


def db(x):
    return 10 * math.log10(np.mean(x ** 2) + 1e-30)


# 32 --------------------------------------------------------------------------------------------
n = FS * 3
t = np.arange(n) / FS
ess = np.zeros(n)
s_mask = np.zeros(n, bool)
for k in range(5):
    s0 = int((0.3 + 0.5 * k) * FS)
    m = int(0.09 * FS)
    ess[s0:s0 + m] = svf(svf(rng.standard_normal(m), 9000, 0.707, "low"), 5000, 0.707, "high") * np.hanning(m) * 0.25
    s_mask[s0:s0 + m] = True
vocal = 0.15 * np.sin(2 * np.pi * 200 * t) + ess
cymbals = svf(rng.standard_normal(n), 5000, 0.707, "high") * 0.04
left, right = vocal + cymbals, vocal - cymbals            # the cymbals wide (all side)
mid, side = 0.5 * (left + right), 0.5 * (left - right)

det = svf(mid, 6200, 1.414, "band")
gain = smooth(np.minimum(0.0, -np.maximum(0.0, level_db(det, 20) + 30) * 0.875).clip(-12, 0), 0.5, 60)
g = 10 ** (gain / 20)
band = lambda x: svf(x, 6200, 1.414, "band")
both_l = left + (g - 1) * band(left)
both_r = right + (g - 1) * band(right)
mid_only_m = mid + (g - 1) * band(mid)

side_both = 0.5 * (both_l - both_r)
ess_cut = db(svf(0.5 * (both_l + both_r), 5000, 0.707, "high")[s_mask]) - db(svf(mid, 5000, 0.707, "high")[s_mask])
ess_cut_mid = db(svf(mid_only_m, 5000, 0.707, "high")[s_mask]) - db(svf(mid, 5000, 0.707, "high")[s_mask])
cym_both = db(side_both[s_mask]) - db(side[s_mask])
print("32. A centre vocal's \"s\" sounds with wide cymbals, Split, Threshold -30, Range -12:")
print(f"    reduce both channels (today): \"s\" {ess_cut:5.1f} dB, cymbals dip {cym_both:5.1f} dB on each \"s\"")
print(f"    reduce the mid only:          \"s\" {ess_cut_mid:5.1f} dB, cymbals dip   0.0 dB (the side passes untouched)")

# 33 --------------------------------------------------------------------------------------------
n = FS * 4
t = np.arange(n) / FS
phrases = ((t % 1.0) < 0.6)
voice = np.zeros(n)
for h in range(1, 40):
    voice += np.sin(2 * np.pi * 160 * h * t + rng.uniform(0, 6)) / h
voice *= 0.2 * phrases
hiss = 10 ** (-54 / 20) * rng.standard_normal(n)
signal = voice + hiss
highs = lambda x: svf(x, 4000, 0.707, "high")
pauses = (~phrases) & (t % 1.0 > 0.7)
loud = phrases & (t % 1.0 > 0.1) & (t % 1.0 < 0.5)

# Hiss mode: the band above 4 kHz, turned down when its level is under Threshold, by up to Range
# (-12 dB), 1 dB per dB below. It opens fast (2 ms) and closes over DS's default Release (60 ms),
# with a 30 ms detector. Threshold -38 dBFS sits above the hiss's peaks (about -44) and under the
# voice's highs. (150 ms to close, with a 60 ms detector, took only 3.7 dB off the pauses.)
hl = level_db(highs(signal), 30)
want = np.maximum(-12.0, np.minimum(0.0, (hl + 38) * 1.0))
g_hiss = 10 ** (smooth(want, 2, 60) / 20)
out_hiss = signal + (g_hiss - 1) * highs(signal)
static = signal + (10 ** (-12 / 20) - 1) * highs(signal)


def above_8k_db(x, mask):
    # Energy above 8 kHz, where the shelf is fully in effect, over the masked samples (FFT frames).
    frames = []
    idx = np.flatnonzero(mask)
    for start in range(0, len(idx) - 4096, 4096):
        seg = x[idx[start:start + 4096]]
        spec = np.abs(np.fft.rfft(seg * np.hanning(4096))) ** 2
        frames.append(np.sum(spec[np.fft.rfftfreq(4096, 1 / FS) > 8000]))
    return 10 * math.log10(np.mean(frames) + 1e-30)


print("33. Hiss mode on a vocal with tape hiss at -54 dBFS (high shelf at 4 kHz, Threshold -38, Range -12):")
for name, out in (("Hiss mode", out_hiss), ("a static -12 dB high shelf", static)):
    print(f"    {name:<28} hiss in the pauses {above_8k_db(out, pauses) - above_8k_db(signal, pauses):6.1f} dB, "
          f"the voice's highs in loud parts {above_8k_db(out, loud) - above_8k_db(signal, loud):6.1f} dB")
