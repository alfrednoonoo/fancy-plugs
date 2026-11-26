"""Model for Fancy EQ's spectral dynamics (docs/eq.md, Phase 4 Wave 3, backlog item 8 of section 4).

A dynamic band today moves its whole bell (or shelf) when the level in its region passes
Threshold. A spectral band does the same arithmetic per FFT bin inside the band's shape: each bin
has its own level, gain computer, Attack and Release, and its change is weighted by the band's
shape (1 at the centre, 0 far away). A resonance inside a wide band is then cut on its own, and
the rest of the band is left alone.

Signal: pink noise at -24 dBFS RMS, plus a 2.5 kHz tone at -14 dBFS (a ringing resonance) that is
on for 0.5 s, off for 0.5 s. Band: a bell at 2.5 kHz, Q 1, DYN Above, Threshold -30 dB, Range
-9 dB, Reach 12, Attack 5 ms, Release 80 ms; detector: the EQ's input, as today.

1. While the tone sounds: how much the tone is cut, and how much the noise is cut at 1.5 kHz and
   4 kHz (inside the band, away from the tone); with the tone off, how much the noise alone is cut.
2. How fast: the tone's cut reaching 90 % after the tone starts.
3. Range 0: output against the input delayed by the latency (perfect reconstruction).
4. Pre-echo: clicks every 250 ms over the held tone; the energy in the 10 ms before each click,
   against the same window with the band off.

Spectral at 1024 and 2048 points (75 % overlap, Hann), per-bin level over the Hann main lobe
(three bins), calibrated so a sine reads its RMS, as the band's detector does.

Run: python docs/research/eq-spectral-model.py
"""
import math

import numpy as np

RATE = 48000
rng = np.random.default_rng(9)
THRESH, RANGE, REACH, ATTACK, RELEASE = -30.0, -9.0, 12.0, 5.0, 80.0
F0, Q = 2500.0, 1.0


def db(v):
    return 10 * math.log10(max(v, 1e-30))


def pink(n, rms_db):
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / RATE)
    spec[1:] /= np.sqrt(f[1:])
    spec[0] = 0
    x = np.fft.irfft(spec, n)
    return x * 10 ** (rms_db / 20) / np.sqrt(np.mean(x ** 2))


def peaking(f0, q, gain_db):
    a = 10 ** (gain_db / 40)
    w0 = 2 * math.pi * f0 / RATE
    alpha = math.sin(w0) / (2 * q)
    b = np.array([1 + alpha * a, -2 * math.cos(w0), 1 - alpha * a])
    aa = np.array([1 + alpha / a, -2 * math.cos(w0), 1 - alpha / a])
    return b / aa[0], aa / aa[0]


def bell_db(freqs, gain_db):
    b, a = peaking(F0, Q, gain_db)
    z = np.exp(-1j * 2 * np.pi * freqs / RATE)
    h = (b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z)
    return 20 * np.log10(np.abs(h))


def gain_change(level_db):
    """Today's gain computer (docs/phase2_1-D.md 4.1, with Reach): a 6 dB soft knee, then Range
    reached Reach dB past Threshold."""
    over = level_db - THRESH
    over = np.where(over < -3, 0.0, np.where(over > 3, over, (over + 3) ** 2 / 12))
    return RANGE * np.minimum(1.0, over / REACH)


def coeff(ms, step):
    return 1 - math.exp(-step / (ms * 0.001 * RATE))


# Today's dynamic band ------------------------------------------------------------------------------
def band_pass(x):
    """The detector's band pass at F0 and Q, used twice (4th order)."""
    w0 = 2 * math.pi * F0 / RATE
    alpha = math.sin(w0) / (2 * Q)
    b = np.array([alpha, 0, -alpha]) / (1 + alpha)
    a = np.array([1, -2 * math.cos(w0) / (1 + alpha), (1 - alpha) / (1 + alpha)])
    for _ in range(2):
        y = np.zeros_like(x)
        s1 = s2 = 0.0
        for i, v in enumerate(x):
            o = b[0] * v + s1
            s1 = b[1] * v - a[1] * o + s2
            s2 = b[2] * v - a[2] * o
            y[i] = o
        x = y
    return x


def normal_band(x):
    det = band_pass(x)
    ms = 0.0
    k5 = coeff(5, 1)
    ka, kr = coeff(ATTACK, 8), coeff(RELEASE, 8)
    change = 0.0
    y = np.zeros_like(x)
    s1 = s2 = 0.0
    trace = np.zeros(len(x))
    b, a = peaking(F0, Q, 0.0)
    for i, v in enumerate(x):
        ms += k5 * (det[i] * det[i] - ms)
        if i % 8 == 0:
            want = float(gain_change(np.array(db(ms))))
            change += (want - change) * (ka if abs(want) > abs(change) else kr)
            if i % 16 == 0:
                b, a = peaking(F0, Q, change)
        o = b[0] * v + s1
        s1 = b[1] * v - a[1] * o + s2
        s2 = b[2] * v - a[2] * o
        y[i] = o
        trace[i] = change
    return y, 0, trace


# The spectral band ---------------------------------------------------------------------------------
def spectral_band(x, n_fft, range_db=RANGE):
    hop = n_fft // 4
    win = np.hanning(n_fft + 1)[:n_fft]
    freqs = np.fft.rfftfreq(n_fft, 1 / RATE)
    weight = bell_db(freqs, -1.0) / -1.0                 # the band's shape: 1 at F0, 0 far away
    weight = np.clip(weight / weight.max(), 0, 1)
    cal = 1.5 * (n_fft / 4) ** 2 * 2                      # three-bin power of a unit sine, x 2 for its RMS
    pad = np.concatenate([np.zeros(n_fft), x, np.zeros(n_fft)])
    out = np.zeros(len(pad))
    change = np.zeros(len(freqs))
    ka, kr = coeff(ATTACK, hop), coeff(RELEASE, hop)
    centre_trace = []
    centre = np.argmin(np.abs(freqs - F0))
    for start in range(0, len(pad) - n_fft, hop):
        spec = np.fft.rfft(pad[start:start + n_fft] * win)
        p = np.abs(spec) ** 2
        p3 = p + np.concatenate([[0], p[:-1]]) + np.concatenate([p[1:], [0]])
        level = 10 * np.log10(np.maximum(p3 / cal, 1e-30))
        want = gain_change(level) * (range_db / RANGE) * weight
        moving_away = np.abs(want) > np.abs(change)
        change += (want - change) * np.where(moving_away, ka, kr)
        centre_trace.append(change[centre])
        out[start:start + n_fft] += np.fft.irfft(spec * 10 ** (change / 20), n_fft) * win / 1.5
    # Latency n_fft: the frame ending now is heard from now on.
    return out[n_fft:n_fft + len(x)], n_fft, np.repeat(centre_trace, hop)[:len(x)]


def tone_and_noise(n):
    t = np.arange(n) / RATE
    gate = ((t % 1.0) < 0.5).astype(float)
    ramp = np.convolve(gate, np.ones(96) / 96, mode="same")
    return 10 ** (-14 / 20) * math.sqrt(2) * np.sin(2 * np.pi * F0 * t) * ramp, pink(n, -24), gate


def band_level(y, lo, hi):
    spec = np.abs(np.fft.rfft(y * np.hanning(len(y)))) ** 2
    f = np.fft.rfftfreq(len(y), 1 / RATE)
    return db(np.sum(spec[(f >= lo) & (f < hi)]))


n = 3 * RATE
tone, noise, gate = tone_and_noise(n)
x = tone + noise
print("1 and 2. Pink noise -24 dBFS and a 2.5 kHz tone at -14 dBFS, on 0.5 s, off 0.5 s; bell 2.5 kHz Q 1, Range -9:")
for name, proc in (("today's band", normal_band),
                   ("spectral 1024", lambda s: spectral_band(s, 1024)),
                   ("spectral 2048", lambda s: spectral_band(s, 2048))):
    y, lat, trace = proc(x)
    # Measure the second cycle: tone on 1.0-1.5 s (from 1.15 s, settled), off 1.5-2.0 s (from 1.75 s).
    # spectral_band returns its output aligned with the input (its latency taken out), so the
    # same stretch of input and output is compared.
    on = slice(int(1.15 * RATE), int(1.5 * RATE))
    off = slice(int(1.75 * RATE), int(2.0 * RATE))
    on_in = slice(int(1.15 * RATE), int(1.5 * RATE))
    off_in = slice(int(1.75 * RATE), int(2.0 * RATE))
    # Each part's change, read from the mixture's output against its input, in its own band.
    tone_cut = band_level(y[on], F0 * 0.97, F0 * 1.03) - band_level(x[on_in], F0 * 0.97, F0 * 1.03)
    n15 = band_level(y[on], 1400, 1600) - band_level(x[on_in], 1400, 1600)
    n40 = band_level(y[on], 3800, 4200) - band_level(x[on_in], 3800, 4200)
    alone = band_level(y[off], 1200, 5000) - band_level(x[off_in], 1200, 5000)
    tr = trace[int(1.0 * RATE):int(1.5 * RATE)]
    final = np.median(tr[-int(0.1 * RATE):])
    reach90 = np.argmax(tr <= 0.9 * final) / RATE * 1000 if final < -0.5 else float("nan")
    print(f"   {name:14s}: tone {tone_cut:+5.1f} dB; noise at 1.5 kHz {n15:+5.1f} dB, at 4 kHz {n40:+5.1f} dB while the tone sounds;"
          f" noise alone {alone:+5.1f} dB; 90 % of the cut {reach90:4.0f} ms after the tone; latency {lat} samples ({lat / RATE * 1000:.1f} ms)")

print("3. Range 0: the output against the input delayed by the latency:")
for n_fft in (1024, 2048):
    y, lat, _ = spectral_band(noise, n_fft, range_db=0.0)
    err = np.max(np.abs(y[n_fft:-n_fft] - noise[n_fft:-n_fft]))
    print(f"   {n_fft}: largest difference {20 * math.log10(max(err, 1e-300)):.0f} dBFS")

print("4. Pre-echo: clicks every 250 ms over the held tone (cut active), 10 ms before each click:")
clicks = np.zeros(n)
clicks[int(0.5 * RATE)::int(0.25 * RATE)] = 0.5
held = 10 ** (-14 / 20) * math.sqrt(2) * np.sin(2 * np.pi * F0 * np.arange(n) / RATE)
xc = held + clicks
for n_fft in (1024, 2048):
    y, lat, _ = spectral_band(xc, n_fft)
    y0, _, _ = spectral_band(held, n_fft)            # the same, without clicks: what the tone alone gives
    diff = y - y0                                     # the clicks' own contribution
    pre, post = [], []
    for c in range(int(0.75 * RATE), n - int(0.3 * RATE), int(0.25 * RATE)):
        pre.append(np.sum(diff[c - int(0.010 * RATE):c - 8] ** 2))
        post.append(np.sum(diff[c - 8:c + int(0.010 * RATE)] ** 2))
    print(f"   {n_fft}: the click's energy before it {db(np.mean(pre)) - db(np.mean(post)):+.0f} dB under its energy after it"
          f" (the frames spread a click over {n_fft / RATE * 1000:.0f} ms)")
