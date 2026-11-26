"""Model for Fancy Delay's reverse and pitch-shifted taps (docs/delay.md, Phase 4 Wave 3,
backlog items 3 and 4 of section 6).

1. Reverse. A reverse tap plays the ring backwards in windows one tap time long: inside each
   window the read head runs backwards from the write head, so its delay grows from 0 to twice
   the tap time and averages the tap time. Each window is faded in and out (F ms) so the jumps
   between windows don't click. Measured on a 437.25 Hz tone: the level kept and the click at the
   worst window edge (the peak above 6 kHz, which the tone has none of); and where a pluck lands.

2. Pitch. The shared pitch shifter (fancy::dsp::PitchShifter, also for Verb's and Room's
   shimmer): two read heads on a delay line, half a grain apart, each reading at the new speed
   across a window of W ms and faded by a raised cosine so the two sum to 1.
   - Plain: each grain starts at a fixed delay, as the classic delay-line shifter does.
   - Aligned: each grain's start moves by up to 10 ms to where it best matches (correlates
     with) the grain fading out, as WSOLA does, so a steady tone's heads add in phase.
   Measured on 16 tones from 150 Hz to 1.5 kHz (+12 st): how far the level wavers (2 ms RMS,
   max over min), and the level change; and on pink noise, the level change.

3. Pitch inside the feedback loop: a 300 ms tap at 60 % feedback: each repeat's pitch and level.

Run: python docs/research/delay-reverse-pitch-model.py
"""
import math

import numpy as np

RATE = 48000
rng = np.random.default_rng(11)


def db(v):
    return 10 * math.log10(max(v, 1e-30))


def read(buf, pos):
    """Linear-interpolated read at fractional positions (the C++ uses 4-point Hermite)."""
    pos = np.clip(pos, 0, len(buf) - 2)
    i = np.floor(pos).astype(int)
    f = pos - i
    return buf[i] + f * (buf[i + 1] - buf[i])


# 1. Reverse -------------------------------------------------------------------------------------
def reverse_tap(x, tap_s, fade_ms):
    n = np.arange(len(x))
    w = int(tap_s * RATE)
    start = (n // w) * w                    # the window this output sample is in
    pos = 2 * start - n                     # the read head runs backwards through the last window
    y = np.where(pos >= 0, read(np.concatenate([x, [0, 0]]), np.maximum(pos, 0)), 0.0)
    k = n - start
    f = max(1e-9, fade_ms * 0.001 * RATE)
    ramp = np.clip(np.minimum(k / f, (w - k) / f), 0, 1)
    return y * (0.5 - 0.5 * np.cos(np.pi * ramp))


def edge_click(y, tap_s, amplitude):
    """The worst peak above 6 kHz within 10 ms of a window edge, in dB under the tone's peak."""
    spec = np.fft.rfft(y)
    spec[np.fft.rfftfreq(len(y), 1 / RATE) < 6000] = 0
    high = np.fft.irfft(spec, len(y))
    w = int(tap_s * RATE)
    half = int(0.01 * RATE)
    worst = max(np.max(np.abs(high[e - half:e + half])) for e in range(2 * w, len(y) - half, w))
    return 20 * math.log10(max(worst, 1e-15) / amplitude)


print("1. Reverse tap, 500 ms, on a 437.25 Hz tone at 0.5 (4 s):")
t = np.arange(4 * RATE) / RATE
tone = 0.5 * np.sin(2 * np.pi * 437.25 * t)
for fade in (0, 2, 5, 10, 30):
    y = reverse_tap(tone, 0.5, fade)
    seg = y[RATE:]                            # past the first, empty window
    level = db(np.mean(seg ** 2)) - db(np.mean(tone[RATE:] ** 2))
    print(f"   fade {fade:2d} ms: level {level:+5.2f} dB, click at the worst edge {edge_click(y, 0.5, 0.5):6.1f} dB under the tone")

print("   Where a note lands: a pluck in a 500 ms reverse tap whose windows start at 0, 0.5, 1.0 s:")
for start_s in (0.55, 0.70, 0.95):
    pluck = np.zeros(3 * RATE)
    p0 = int(start_s * RATE)
    pk = np.arange(int(0.3 * RATE))
    pluck[p0:p0 + len(pk)] = np.sin(2 * np.pi * 220 * pk / RATE) * np.exp(-pk / (0.06 * RATE))
    y = reverse_tap(pluck, 0.5, 5)
    peak = np.argmax(np.abs(y)) / RATE
    print(f"   played at {start_s:.2f} s: its attack is heard at {peak:.3f} s, {peak - start_s:.3f} s later")

# 2. The pitch shifter ---------------------------------------------------------------------------
def pitch_shift(x, semitones, window_ms, search_ms=0.0, min_ms=1.0):
    """Two heads, grains of T output samples each, half a grain apart, raised-cosine faded.
    search_ms > 0 aligns each grain's start to the grain fading out (WSOLA-like)."""
    r = 2 ** (semitones / 12)
    w = window_ms * 0.001 * RATE
    s_max = int(search_ms * 0.001 * RATE)
    grain = int(round(w / abs(1 - r)))
    pad = int(w + s_max + min_ms * 0.001 * RATE + 8)
    buf = np.concatenate([np.zeros(pad), x, np.zeros(grain + 8)])
    n_out = len(x)
    out = np.zeros(n_out + grain)
    raw_of = {}                                  # head -> (start, raw read over its grain)
    k = np.arange(grain)
    gain = 0.5 - 0.5 * np.cos(2 * np.pi * k / grain)
    m = min_ms * 0.001 * RATE

    def raw(start, delay0):
        return read(buf, start + pad - delay0 + r * k)

    for g, start in enumerate(range(-grain // 2, n_out, grain // 2)):
        head = g % 2
        # The grain's starting delay, before alignment: it ends at the small end of the window.
        base = (m + s_max + (r - 1) * grain) if r > 1 else m
        best, best_score = 0, -2.0
        other = raw_of.get(1 - head)
        if s_max > 0 and other is not None:
            o_start, o_raw = other
            half = grain // 2
            target = o_raw[start - o_start:start - o_start + half]
            if len(target) == half and np.any(target):
                for delta in range(0, s_max + 1, 2):
                    cand = read(buf, start + pad - (base - delta if r > 1 else base + delta) + r * k[:half])
                    denom = math.sqrt(np.dot(cand, cand) * np.dot(target, target)) + 1e-12
                    score = np.dot(cand, target) / denom
                    if score > best_score:
                        best, best_score = delta, score
        delay0 = base - best if r > 1 else base + best
        rr = raw(start, delay0)
        raw_of[head] = (start, rr)
        lo = max(0, start)
        out[lo:start + grain] += (gain * rr)[lo - start:]
    return out[:n_out], (m + s_max / 2 + 0.5 * w) / RATE * 1000


def ripple_and_level(y, x, frame_ms):
    frame = int(frame_ms * 0.001 * RATE)
    seg = y[RATE // 2:]
    rms = np.sqrt(np.convolve(seg ** 2, np.ones(frame) / frame, mode="valid"))
    return 20 * math.log10(rms.max() / max(rms.min(), 1e-12)), db(np.mean(seg ** 2)) - db(np.mean(x[RATE // 2:] ** 2))


print("\n2. Pitch shifter, +12 st, 16 tones from 150 Hz to 1.5 kHz, and pink noise:")
t = np.arange(2 * RATE) / RATE
tones = np.exp(rng.uniform(math.log(150), math.log(1500), 16))
spec = np.fft.rfft(rng.standard_normal(len(t)))
f = np.fft.rfftfreq(len(t), 1 / RATE)
spec[1:] /= np.sqrt(f[1:])
spec[0] = 0
noise = np.fft.irfft(spec, len(t))
noise *= 0.2 / np.sqrt(np.mean(noise ** 2))
for search in (0.0, 10.0):
    for window in (20, 40, 80):
        ripples, levels = [], []
        for hz in tones:
            x = 0.5 * np.sin(2 * np.pi * hz * t)
            y, avg_ms = pitch_shift(x, 12, window, search)
            r_db, l_db = ripple_and_level(y, x, 2)
            ripples.append(r_db)
            levels.append(l_db)
        y, _ = pitch_shift(noise, 12, window, search)
        _, noise_level = ripple_and_level(y, noise, 2)
        name = "aligned" if search else "plain  "
        print(f"   {name} window {window:2d} ms: tone wavers median {np.median(ripples):4.1f} dB, worst {max(ripples):4.1f} dB;"
              f" tone level median {np.median(levels):+4.1f} dB; noise level {noise_level:+4.1f} dB; average delay {avg_ms:.0f} ms")

# 3. Pitch in the loop ----------------------------------------------------------------------------
def repeats(semitones, hz, fb=0.6, window_ms=40, search_ms=10.0):
    k = np.arange(int(0.15 * RATE))
    current = np.sin(2 * np.pi * hz * k / RATE) * np.hanning(len(k))
    first = None
    line = []
    for i in range(5):
        y, _ = pitch_shift(np.concatenate([current, np.zeros(int(0.1 * RATE))]), semitones, window_ms, search_ms)
        current = (fb if i else 1.0) * y
        sp = np.abs(np.fft.rfft(current * np.hanning(len(current)), 8 * len(current))) ** 2
        f0 = np.fft.rfftfreq(8 * len(current), 1 / RATE)[np.argmax(sp)]
        energy = db(np.sum(current ** 2))
        first = energy if first is None else first
        line.append(f"{f0:5.0f} Hz {energy - first:+5.1f} dB")
    return line


print("\n3. Pitch inside the loop, 300 ms tap, 60 % feedback (-4.4 dB a repeat without pitch), aligned, 40 ms:")
for st, hz in ((12, 220), (7, 220), (-12, 1760)):
    print(f"   {st:+3d} st from {hz} Hz, repeats 1-5: " + " | ".join(repeats(st, hz)))
