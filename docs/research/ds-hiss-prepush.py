"""Pre-push check for Fancy DS's Hiss mode (docs/ds.md, "Phase 4 Wave 2", item 33, test 3),
written before any code (24 November 2026).

The design model (docs/research/ds-w2b-model.py, part 33) found -6.8 dB of hiss in the pauses and
-0.2 dB on the voice's highs. Two things in it differ from what the plugin would do:

1. Its ballistics are the wrong way round for the note. The note says "Attack is how fast it opens
   and Release how fast it closes", and the model's comment says it "opens fast (2 ms) and closes
   over 60 ms", but its smooth() moves at the attack time while the reduction grows: it closes in
   2 ms and opens over 60 ms.
2. Its detector is an envelope with a 30 ms release. DS's detector (fancy-core DeEsser) holds the
   peak for 1 ms and then lets it fall at 20 dB/s. After a phrase, it takes 13 dB / 20 dB/s, about
   0.65 s, to fall from the voice's highs to Threshold, longer than the 0.4 s pauses.

This runs the model's signal through four versions and measures what test 3 measures (the energy
above 8 kHz in the pauses and in the loud parts, against the input):
  A. the design model as written;
  B. the note's ballistics (opens at Attack 0.5 ms, closes at Release 60 ms, DS's defaults) with
     the model's 30 ms detector;
  C. the note's ballistics with DS's own detector (1 ms hold, then 20 dB/s);
  D. the note's ballistics with DS's detector falling faster in Hiss mode (200 dB/s).

Run: python docs/research/ds-hiss-prepush.py
"""
import math

import numpy as np

FS = 48000
rng = np.random.default_rng(33)


def svf_high(x, hz, q=0.707):
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
        out[i] = v - k * v1 - v2
    return out


def envelope_db(x, release_ms):
    r = 1 - math.exp(-1 / (release_ms * 0.001 * FS))
    env, out = 0.0, np.empty(len(x))
    for i, v in enumerate(np.abs(x)):
        env = v if v > env else env + (v - env) * r
        out[i] = 20 * math.log10(max(env, 1e-9))
    return out


def ds_peak_db(x, fall_db_per_s):
    hold_samples, fall = int(0.001 * FS), fall_db_per_s / FS
    peak, hold, out = -200.0, 0, np.empty(len(x))
    for i, v in enumerate(np.abs(x)):
        db = 20 * math.log10(max(v, 1e-10))
        if db >= peak:
            peak, hold = db, hold_samples
        elif hold > 0:
            hold -= 1
        else:
            peak = max(db, peak - fall)
        out[i] = peak
    return out


def follow(reduction_db, grow_ms, shrink_ms):
    """A reduction in dB (positive), moving at grow_ms while it grows (closing) and shrink_ms
    while it shrinks (opening)."""
    g = 1 - math.exp(-1 / (grow_ms * 0.001 * FS))
    s = 1 - math.exp(-1 / (shrink_ms * 0.001 * FS))
    env, out = 0.0, np.empty(len(reduction_db))
    for i, want in enumerate(reduction_db):
        env += (want - env) * (g if want > env else s)
        out[i] = env
    return out


n = FS * 4
t = np.arange(n) / FS
phrases = (t % 1.0) < 0.6
voice = np.zeros(n)
for h in range(1, 40):
    voice += np.sin(2 * np.pi * 160 * h * t + rng.uniform(0, 6)) / h
voice *= 0.2 * phrases
signal = voice + 10 ** (-54 / 20) * rng.standard_normal(n)
highs = svf_high(signal, 4000)
pauses = (~phrases) & (t % 1.0 > 0.7)
loud = phrases & (t % 1.0 > 0.1) & (t % 1.0 < 0.5)


def above_8k_db(x, mask):
    frames = []
    idx = np.flatnonzero(mask)
    for start in range(0, len(idx) - 4096, 4096):
        seg = x[idx[start:start + 4096]]
        spec = np.abs(np.fft.rfft(seg * np.hanning(4096))) ** 2
        frames.append(np.sum(spec[np.fft.rfftfreq(4096, 1 / FS) > 8000]))
    return 10 * math.log10(np.mean(frames) + 1e-30)


THRESHOLD, RANGE = -38.0, 12.0


def run(level_db, grow_ms, shrink_ms):
    want = np.clip(THRESHOLD - level_db, 0.0, RANGE)          # 1 dB per dB below Threshold
    reduction = follow(want, grow_ms, shrink_ms)
    out = signal + (10 ** (-reduction / 20) - 1) * highs
    return above_8k_db(out, pauses) - above_8k_db(signal, pauses), above_8k_db(out, loud) - above_8k_db(signal, loud)


cases = [
    ("A. the design model (closes in 2 ms, opens over 60 ms; 30 ms detector)", envelope_db(highs, 30), 2.0, 60.0),
    ("B. the note's ballistics (closes at Release 60 ms, opens at Attack 0.5 ms); 30 ms detector", envelope_db(highs, 30), 60.0, 0.5),
    ("C. the note's ballistics; DS's detector (1 ms hold, 20 dB/s)", ds_peak_db(highs, 20.0), 60.0, 0.5),
    ("D. the note's ballistics; DS's detector falling at 200 dB/s in Hiss mode", ds_peak_db(highs, 200.0), 60.0, 0.5),
]

print("Item 33, Hiss mode on the model's vocal with tape hiss at -54 dBFS (shelf at 4 kHz, Threshold -38 dB, Range -12 dB),")
print("energy above 8 kHz against the input; test 3 asks for at least 5 dB off in the pauses and under 0.5 dB in loud parts:")
for name, level, grow, shrink in cases:
    hiss, highs_loud = run(level, grow, shrink)
    verdict = "passes" if hiss <= -5.0 and abs(highs_loud) < 0.5 else "fails"
    print(f"    {name}\n        pauses {hiss:6.1f} dB, loud parts {highs_loud:5.2f} dB: {verdict}")
