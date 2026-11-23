"""Pre-push check for Fancy Gate's Transient detector and Upward mode (docs/gate.md, Phase 4 Wave 2,
items 11 and 12): the tests in tests/GateW2bTests.cpp, run through a model of GateDsp as built, sample
for sample, with several noise seeds so the margins don't depend on one draw.

  Transient: the detector's filtered signal (SVF lowpass, as GateDsp's) is followed with a 1 ms attack
  and 20 ms release, and with a 50 ms average, both on |x|. A hit is a rise of Rise dB over the average
  with the level over Threshold; it re-arms once the rise is under half of Rise. The machine opens over
  Attack, holds for Hold once fully open, and closes over Release; a hit during any of that re-opens it.

  Upward: (level - Threshold) x (Up ratio - 1) dB, at most Boost, smoothed with one-pole Attack and
  Release on the dB value, held for Hold after the level drops under Threshold.

Run: py -3 docs/research/gate-w2b-prepush.py
"""
import math

import numpy as np

RATE = 48000.0


def coeff(ms):
    return 1.0 - math.exp(-1.0 / (ms * 0.001 * RATE))


def lowpass(sig, hz):
    g = math.tan(math.pi * hz / RATE)
    k = 1 / 0.70710678118654752
    a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2
    ic1 = ic2 = 0.0
    out = np.empty(len(sig))
    for i, v in enumerate(sig):
        v3 = v - ic2
        v1 = a1 * ic1 + a2 * v3
        v2 = ic2 + a2 * ic1 + a3 * v3
        ic1, ic2 = 2 * v1 - ic1, 2 * v2 - ic2
        out[i] = v2
    return out


def transient(filtered, threshold, rise_db, attack_ms=0.5, hold_ms=150.0, release_ms=50.0):
    """Returns the machine position per sample and the indices of note-ons (opened or retriggered)."""
    a, r, c = coeff(1.0), coeff(20.0), coeff(50.0)
    att = 1.0 / max(24, attack_ms * 0.001 * RATE)
    rel = 1.0 / max(24, release_ms * 0.001 * RATE)
    hold = int(round(hold_ms * 0.001 * RATE))
    env = avg = 0.0
    armed = True
    state, pos, left = "closed", 0.0, 0
    out = np.empty(len(filtered)); events = []
    for i, v in enumerate(np.abs(filtered)):
        env += (v - env) * (a if v > env else r)
        avg += (v - avg) * c
        level = 20 * math.log10(max(env, 1e-15))
        rise = level - 20 * math.log10(max(avg, 1e-15))
        if rise < 0.5 * rise_db:
            armed = True
        trig = armed and rise >= rise_db and level >= threshold
        if trig:
            armed = False
            left = hold
            events.append(i)
            state = "opening"
        elif state == "opening":
            pos += att
            if pos >= 1.0:
                pos, state = 1.0, "hold"
        elif state == "hold":
            left -= 1
            if left <= 0:
                state = "closing"
        elif state == "closing":
            pos -= rel
            if pos <= 0.0:
                pos, state = 0.0, "closed"
        out[i] = pos
    return out, events


def level_gate(filtered, threshold, hysteresis=-6.0, hold_ms=150.0, release_ms=50.0, attack_ms=0.5):
    """GateDsp's Level machine on the peak detector (RMS size 0: the instantaneous |x|)."""
    att = 1.0 / max(24, attack_ms * 0.001 * RATE)
    rel = 1.0 / max(24, release_ms * 0.001 * RATE)
    hold = int(round(hold_ms * 0.001 * RATE))
    close = threshold + hysteresis
    state, pos, left, armed = "closed", 0.0, 0, True
    out = np.empty(len(filtered))
    for i, v in enumerate(filtered):
        lv = 10 * math.log10(max(v * v, 1e-30))
        if lv < threshold:
            armed = True
        trig = armed and lv >= threshold
        if state == "closed":
            if trig:
                state, armed = "opening", False
        elif state == "closing":
            if trig:
                state, armed = "opening", False
            else:
                pos -= rel
                if pos <= 0:
                    pos, state = 0.0, "closed"
        elif state == "opening":
            pos += att
            if pos >= 1:
                pos, state = 1.0, "open"
        elif state == "open":
            if lv < close:
                state, left = "hold", hold
        elif state == "hold":
            if trig:
                state, armed = "open", False
            elif lv >= close:
                state = "open"
            else:
                left -= 1
                if left <= 0:
                    state = "closing"
        out[i] = pos
    return out


# Test 2: the design note's song, built as the C++ test builds it (noise from a seeded generator).
def song(seed):
    rng = np.random.default_rng(seed)
    n = int(RATE * 6)
    x = np.zeros(n)
    hits = []
    verse, chorus = [-12, -18, -24], [-6, -9]
    for k in range(12):
        start = int((0.2 + 0.5 * k) * RATE)
        hits.append(start)
        db = verse[k % 3] if start < 3 * RATE else chorus[k % 2]
        m = np.arange(int(0.4 * RATE))
        body = np.sin(2 * np.pi * 110 * m / RATE) * np.exp(-m / (0.15 * RATE))
        click = rng.uniform(-1.7, 1.7, len(m)) * np.exp(-m / (0.001 * RATE)) * (m < 0.003 * RATE)
        tom = body + 0.8 * click
        tom = 10 ** (db / 20) * tom / np.max(np.abs(tom))
        x[start:start + len(m)] += tom[:n - start]
    for k in range(15):
        start = int(k * 0.4 * RATE)
        m = np.arange(min(int(0.4 * RATE), n - start))
        f = (55.0, 65.4, 73.4, 82.4)[k % 4]
        note = np.sin(2 * np.pi * f * m / RATE) * (1 - np.exp(-m / (0.03 * RATE))) * np.exp(-m / (0.8 * RATE))
        peak = 10 ** ((-40 if start < 3 * RATE else -20) / 20)
        x[start:start + len(m)] += peak * note / np.max(np.abs(note))
    x += 10 ** (-60 / 20) * rng.uniform(-1.7, 1.7, n)
    return x, hits


def score(pos, hits):
    n = len(pos)
    is_open = pos > 0
    caught = sum(bool(is_open[h:h + int(0.005 * RATE)].any()) for h in hits)
    near = np.zeros(n, bool)
    for h in hits:
        near[max(0, h - int(0.005 * RATE)):h + int(0.25 * RATE)] = True
    return caught, np.sum(is_open & ~near) / RATE * 1000.0


print("Test 2, the song (Lowpass 1 kHz, Hold 150 ms, Release 50 ms):")
for seed in (1, 2, 3, 4, 5):
    x, hits = song(seed)
    f = lowpass(x, 1000.0)
    tpos, _ = transient(f, -40.0, 8.0)
    lpos = level_gate(f, -30.0)
    c, ms = score(tpos, hits)
    lc, lms = score(lpos, hits)
    print(f"  seed {seed}: Transient {c} hits caught, {ms:5.1f} ms open on the bass (need 12, < 50); "
          f"Level -30: {lms:6.0f} ms (need > 1000)")

# Test 3: a held 1 kHz tone at -6 dBFS from 0.1 s for 1 s; Hold 50 ms, Release 50 ms.
n = int(RATE * 1.3)
t = np.arange(n) / RATE
tone = np.where((t >= 0.1) & (t < 1.1), 0.5 * np.sin(2 * np.pi * 1000 * t), 0.0)
pos, ev = transient(tone, -40.0, 8.0, hold_ms=50.0, release_ms=50.0)
lpos = level_gate(tone, -40.0, hold_ms=50.0, release_ms=50.0)
print(f"Test 3, held tone: Transient opens {len(ev)} time(s), open from 0.3 to 1.05 s: {bool((pos[int(0.3*RATE):int(1.05*RATE)] > 0).any())}; "
      f"Level open throughout 0.3 to 1.05 s: {bool((lpos[int(0.3*RATE):int(1.05*RATE)] >= 1).all())}")

# Test 4: back-to-back 400 ms bass notes at -6 dBFS peak, each swelling in over 30 ms (the model's bass);
# after the first note none opens. Then a 1 ms click 10 dB over Threshold, on silence, does.
for f0 in (55.0, 82.4, 110.0):
    n = int(RATE * 2.4)
    x = np.zeros(n)
    for k in range(6):
        start = int(k * 0.4 * RATE)
        m = np.arange(int(0.4 * RATE))
        note = np.sin(2 * np.pi * f0 * m / RATE) * (1 - np.exp(-m / (0.03 * RATE))) * np.exp(-m / (0.8 * RATE))
        x[start:start + len(m)] = 0.5 * note / np.max(np.abs(note))
    pos, ev = transient(x, -40.0, 8.0, hold_ms=20.0, release_ms=20.0)
    late = [e for e in ev if e >= int(0.4 * RATE)]
    worst = 0.0
    a, r, c = coeff(1.0), coeff(20.0), coeff(50.0)
    env = avg = 0.0
    for i, v in enumerate(np.abs(x)):
        env += (v - env) * (a if v > env else r); avg += (v - avg) * c
        if i >= int(0.4 * RATE):
            worst = max(worst, 20 * math.log10(max(env, 1e-15) / max(avg, 1e-15)))
    print(f"Test 4, {f0:5.1f} Hz notes: openings after the first note {len(late)} (need 0); largest rise {worst:.1f} dB")

click = np.zeros(int(RATE * 0.3))
click[int(0.1 * RATE):int(0.1 * RATE) + 48] = 10 ** ((-40 + 10) / 20) * np.where(np.arange(48) % 2 == 0, 1.0, -1.0)
pos, ev = transient(click, -40.0, 8.0, hold_ms=20.0, release_ms=20.0)
print(f"Test 4, click at -30 dBFS (Threshold -40): opens {len(ev)} time(s), at {(ev[0] - int(0.1 * RATE)) if ev else None} samples")

# Test 5 (MIDI out, one note per hit): hits every 250 ms with Hold 20, Release 20, on silence.
x = np.zeros(int(RATE * 1.0))
for k in range(4):
    s = int((0.05 + 0.25 * k) * RATE)
    m = np.arange(int(0.2 * RATE))
    x[s:s + len(m)] = 0.5 * np.sin(2 * np.pi * 180 * m / RATE) * np.exp(-m / (0.03 * RATE))
pos, ev = transient(x, -40.0, 8.0, hold_ms=20.0, release_ms=20.0)
print(f"Test 5, four decaying hits: {len(ev)} note-ons (need 4), at {[round(e / RATE * 1000) for e in ev]} ms")


# Upward, tests 3 and 4.
def upward(level_db, threshold, ratio, boost, attack_ms, release_ms, hold_ms=0.0):
    a = 1.0 - math.exp(-1.0 / max(24, attack_ms * 0.001 * RATE))
    r = 1.0 - math.exp(-1.0 / max(24, release_ms * 0.001 * RATE))
    hold = int(round(hold_ms * 0.001 * RATE))
    g, left, held = 0.0, 0, 0.0
    out = np.empty(len(level_db))
    for i, lv in enumerate(level_db):
        if lv >= threshold:
            want = min(boost, (lv - threshold) * (ratio - 1.0))
            held, left = want, hold
        elif left > 0:
            left -= 1
            want = held
        else:
            want = 0.0
        g += (want - g) * (a if want > g else r)
        out[i] = g
    return out


def rms_db(sig, ms):
    c = coeff(ms)
    s, out = 0.0, np.empty(len(sig))
    for i, v in enumerate(sig):
        s += c * (v * v - s)
        out[i] = 10 * math.log10(max(s, 1e-30))
    return out


n = int(RATE * 1.0)
t = np.arange(n) / RATE
tone = math.sqrt(2) * 10 ** (-20 / 20) * np.sin(2 * np.pi * 1000 * t)   # RMS -20 dBFS, Threshold -30
lv = rms_db(tone, 50.0)
for boost in (12.0, 4.0):
    g = upward(lv, -30.0, 1.5, boost, 10.0, 200.0)
    print(f"Upward test 3, tone 10 dB over, Up ratio 1.5, Boost {boost:4.1f}: gain {np.mean(g[int(0.6 * RATE):]):.3f} dB")

rng = np.random.default_rng(9)
n = int(RATE * 8)
half = n // 2
bed = rng.uniform(-1.7, 1.7, n)
s = np.concatenate([bed[:half] * 10 ** (-24 / 20), bed[half:] * 10 ** (-16 / 20)])
lv = rms_db(s, 50.0)
g = upward(lv, -20.0, 1.5, 6.0, 10.0, 200.0)
print(f"Upward test 4, song (noise bed only): verse +{np.max(g[int(RATE):half]):.3f} dB max, chorus +{np.mean(g[half + int(RATE):]):.2f} dB mean "
      "(need verse < 0.05, chorus 2.5 to 3.5)")
