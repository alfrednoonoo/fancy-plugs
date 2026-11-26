"""Model for Fancy Verb's Freeze (docs/verb.md, Phase 4 Wave 3, backlog item 9 of section 5).

Verb is a convolver: its tail is the input convolved with a fixed IR, so there is no loop to hold
as Room and Delay hold theirs. Three ways to freeze it, on the wet signal of a chord through a
generated hall (stereo, Decay 2.5 s, highs shorter), frozen 1.5 s after the chord starts:

A. Spectral freeze: the wet signal's magnitude spectrum over the last 250 ms (STFT 4096 points,
   hop 1024), resynthesised with new random phases every hop, overlap-added. Two stereo options:
   independent phases per channel, or the same random phase step on both, so the captured
   difference between left and right is kept.
B. Loop: the last 1 s of the wet signal looped with a 50 ms crossfade.
C. Recirculation: the wet signal fed back into the convolver, input muted, with a loop gain set
   so the IR passes energy at 1 (0 dB a pass).

Measured over 10 s frozen: how far the level wanders (50 ms RMS, 5th to 95th percentile), how far
each third octave is from the captured spectrum, the strongest repetition (normalised
autocorrelation of the 50 ms envelope at lags 0.2 to 5 s), and left/right correlation.

Run: python docs/research/verb-freeze-model.py
"""
import math

import numpy as np

RATE = 48000
rng = np.random.default_rng(23)


def db(v):
    return 10 * math.log10(max(v, 1e-30))


def band_decay_ir(seconds, rts, edges, seed):
    """Noise with a different RT60 in each band (Verb's generator, simplified)."""
    r = np.random.default_rng(seed)
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    out = np.zeros(n)
    spec = np.fft.rfft(r.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / RATE)
    lo = 0.0
    for rt, hi in zip(rts, edges + [RATE]):
        part = spec * ((f >= lo) & (f < hi))
        out += np.fft.irfft(part, n) * np.exp(-6.91 * t / rt)
        lo = hi
    return out / math.sqrt(np.sum(out ** 2))


def third_octaves(x):
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / RATE)
    centres = 1000 * 2 ** (np.arange(-15, 13) / 3)
    out = []
    for c in centres:
        sel = (f >= c * 2 ** (-1 / 6)) & (f < c * 2 ** (1 / 6))
        out.append(db(np.sum(spec[sel])))
    return np.array(out)


def envelope(x, ms=50):
    frame = int(ms * 0.001 * RATE)
    k = len(x) // frame
    return np.sqrt(np.mean(x[:k * frame].reshape(k, frame) ** 2, axis=1))


def measure(name, frozen, captured):
    env = envelope(frozen[0])
    env_db = 20 * np.log10(np.maximum(env, 1e-12))
    wander = np.percentile(env_db, 95) - np.percentile(env_db, 5)
    a = third_octaves(frozen[0])
    b = third_octaves(captured[0])
    a -= db(np.sum(10 ** (a / 10)))           # each as a share of its own total
    b -= db(np.sum(10 ** (b / 10)))
    used = b > np.max(b) - 30                 # bands within 30 dB of the strongest
    spec_err = np.max(np.abs(a[used] - b[used]))
    e = env - env.mean()
    ac = np.correlate(e, e, mode="full")[len(e) - 1:]
    ac /= ac[0]
    lags = np.arange(len(ac)) * 0.05
    sel = (lags >= 0.2) & (lags <= 5.0)
    rep = np.max(ac[sel])
    rep_at = lags[sel][np.argmax(ac[sel])]
    corr = np.corrcoef(frozen[0], frozen[1])[0, 1]
    level = db(np.mean(frozen[0] ** 2)) - db(np.mean(captured[0] ** 2))
    print(f"   {name:34s} level {level:+5.1f} dB, wanders {wander:4.1f} dB, spectrum within {spec_err:4.1f} dB, "
          f"repeats {rep:4.2f} at {rep_at:.2f} s, L/R correlation {corr:+.2f}")


# The source and the hall --------------------------------------------------------------------------
ir_a, ir_b = (band_decay_ir(3.5, [3.0, 2.5, 1.5], [300.0, 3000.0], seed) for seed in (1, 2))
irs = [ir_a, 0.5 * ir_a + math.sqrt(0.75) * ir_b]    # left and right correlated about 0.5, as halls are
t = np.arange(int(1.2 * RATE)) / RATE
chord = sum(np.sin(2 * np.pi * f0 * k * t) / k for f0 in (196.0, 246.9, 293.7) for k in range(1, 6))
chord *= np.minimum(1, t / 0.01) * np.exp(-t / 0.8) * 0.1
n_total = int(4.0 * RATE)
src = np.zeros(n_total)
src[:len(chord)] = chord
wet = [np.fft.irfft(np.fft.rfft(src, 2 * n_total) * np.fft.rfft(h, 2 * n_total))[:n_total] for h in irs]
freeze_at = int(1.5 * RATE)
captured = [w[freeze_at - int(0.25 * RATE):freeze_at] for w in wet]
print(f"Captured: the hall's wet signal 1.25 to 1.5 s after a chord; its L/R correlation {np.corrcoef(captured[0], captured[1])[0, 1]:+.2f}")

# A. Spectral freeze -------------------------------------------------------------------------------
N, HOP = 4096, 1024
win = np.hanning(N + 1)[:N]
frames = []
for ch in range(2):
    w = wet[ch]
    mags = []
    for start in range(freeze_at - N - 7 * HOP, freeze_at - N + 1, HOP):
        mags.append(np.abs(np.fft.rfft(w[start:start + N] * win)))
    frames.append(np.sqrt(np.mean(np.array(mags) ** 2, axis=0)))

seconds = 10.0
n_out = int(seconds * RATE)
bins = len(frames[0])
bin_advance = 2 * np.pi * np.arange(bins) * HOP / N
# The phase vocoder's measured advance per hop, from the last two captured frames.
measured = []
for ch in range(2):
    a = np.angle(np.fft.rfft(wet[ch][freeze_at - N - HOP:freeze_at - HOP] * win))
    b = np.angle(np.fft.rfft(wet[ch][freeze_at - N:freeze_at] * win))
    dev = np.angle(np.exp(1j * (b - a - bin_advance)))
    measured.append(bin_advance + dev)
for mode in ("random, independent", "random, shared step", "vocoder, measured frequency"):
    out = [np.zeros(n_out + N), np.zeros(n_out + N)]
    norm = np.zeros(n_out + N)
    phase = [np.angle(np.fft.rfft(wet[ch][freeze_at - N:freeze_at] * win)) for ch in range(2)]
    for start in range(0, n_out, HOP):
        step = rng.uniform(-np.pi, np.pi, bins)
        for ch in range(2):
            if mode.startswith("random, independent"):
                phase[ch] = rng.uniform(-np.pi, np.pi, bins)
            elif mode.startswith("random, shared"):
                phase[ch] = phase[ch] + step          # the same random step: L-R difference kept
            else:
                phase[ch] = phase[ch] + measured[ch]
            out[ch][start:start + N] += np.fft.irfft(frames[ch] * np.exp(1j * phase[ch]), N) * win
        norm[start:start + N] += win ** 2
    if mode.startswith("random"):
        # Random phases add frames by power, not amplitude: scale by the power of the overlap.
        frozen = [o[N:n_out] / np.sqrt(np.maximum(norm[N:n_out] * np.mean(win ** 2), 1e-9)) for o in out]
    else:
        frozen = [o[N:n_out] / np.maximum(norm[N:n_out], 1e-9) for o in out]
    measure(f"A. spectral, {mode}", frozen, captured)

# B. Loop -------------------------------------------------------------------------------------------
loop_len, xf = int(1.0 * RATE), int(0.05 * RATE)
frozen = []
for ch in range(2):
    seg = wet[ch][freeze_at - loop_len - xf:freeze_at]
    body = seg[xf:].copy()
    ramp = 0.5 - 0.5 * np.cos(np.pi * np.arange(xf) / xf)
    body[-xf:] = body[-xf:] * (1 - ramp) + seg[:xf] * ramp   # the end fades into the loop's start
    frozen.append(np.tile(body, int(seconds) + 1)[:n_out])
measure("B. 1 s loop, 50 ms crossfade", frozen, captured)

# C. Recirculation through the IR -------------------------------------------------------------------
H = np.fft.rfft(irs[0], 2 * len(irs[0]))
mag = np.abs(H)
rms = math.sqrt(np.mean(mag[1:] ** 2))
peak = np.max(mag[1:])
print(f"   C. recirculation: the IR's strongest bin is {20 * math.log10(peak / rms):.1f} dB over its average.")
print("      A loop gain of 0 dB on average runs away at those bins; held under them, the average")
print(f"      tail falls by {20 * math.log10(peak / rms):.1f} dB a pass ({3.5:.1f} s), so it decays instead of holding,")
print("      and each pass colours it by the IR again, narrowing it onto the IR's strongest modes.")
for passes in (1, 3, 10):
    p = (mag[1:] / peak) ** (2 * passes)
    share = np.sort(p)[::-1]
    top = np.sum(share[:len(share) // 100]) / np.sum(share)
    print(f"      after {passes:2d} passes at the safe gain: {db(np.mean(p)):6.1f} dB, the top 1 % of bins hold {100 * top:4.1f} % of the energy")
