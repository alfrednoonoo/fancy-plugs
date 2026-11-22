"""Model of Fancy Gate's Transient detector mode (docs/gate.md, Phase 4 Wave 2, item 11 of the order).

Gate today opens on level: peak, or RMS with RMS size. When the bleed on a drum mic changes level
through a song, no threshold works throughout: low enough for the soft hits of a quiet verse, it
stays open on the bleed of a loud chorus. A transient detector opens on a sudden rise instead.
Drum hits rise in about a millisecond; a bass note, a pad or a sustained guitar swells in over
tens of milliseconds, so it doesn't count, however loud it is.

The test, 6 s at 48 kHz: a tom mic with bass bleed. A verse (0-3 s) with the bass at -40 dBFS peak
and soft hits at -12, -18 and -24; a chorus (3-6 s) with the bass at -20 dBFS peak and hits at -6
and -9. Hits are a 3 ms click into a 110 Hz body decaying over 150 ms; bass notes are 55 to 82 Hz,
400 ms long, each swelling in over 30 ms. Both detectors listen through Gate's Lowpass at 1 kHz,
as a user would set it for a tom.

  Level:     today's Gate: open at Threshold, Hysteresis -6 dB, Hold 150 ms (peak detector).
  Transient: open when the detector level (1 ms attack, 20 ms release) is Rise dB above its own
             50 ms average, and above Threshold as a floor; closes Hold after the last rise,
             whatever the level, so a sustained sound can't keep it open.

For each setting: how many of the 12 hits open the gate within 5 ms, and how long the gate is open
more than 250 ms after any hit (opening on the bass).

Run: py -3 docs/research/gate-transient-model.py
"""
import math

import numpy as np

RATE = 48000
rng = np.random.default_rng(5)
n = RATE * 6
t = np.arange(n) / RATE

x = np.zeros(n)
hits = []
verse = [-12, -18, -24]
chorus = [-6, -9]
for k in range(12):
    start = int((0.2 + 0.5 * k) * RATE)
    hits.append(start)
    db = verse[k % 3] if start < 3 * RATE else chorus[k % 2]
    m = np.arange(int(0.4 * RATE))
    body = np.sin(2 * np.pi * 110 * m / RATE) * np.exp(-m / (0.15 * RATE))
    click = rng.standard_normal(len(m)) * np.exp(-m / (0.001 * RATE)) * (m < 0.003 * RATE)
    tom = body + 0.8 * click
    tom = 10 ** (db / 20) * tom / np.max(np.abs(tom))
    end = min(n, start + len(m))
    x[start:end] += tom[:end - start]

# Bass bleed: 400 ms notes, each swelling in over 30 ms, quiet in the verse and loud in the chorus.
for k in range(15):
    start = int(k * 0.4 * RATE)
    m = np.arange(min(int(0.4 * RATE), n - start))
    f = (55.0, 65.4, 73.4, 82.4)[k % 4]
    note = np.sin(2 * np.pi * f * m / RATE) * (1 - np.exp(-m / (0.03 * RATE))) * np.exp(-m / (0.8 * RATE))
    peak = 10 ** ((-40 if start < 3 * RATE else -20) / 20)
    x[start:start + len(m)] += peak * note / np.max(np.abs(note))
x += 10 ** (-60 / 20) * rng.standard_normal(n)


def lowpass(sig, hz):
    g = math.tan(math.pi * hz / RATE)
    k = math.sqrt(2)
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


def follower(sig, attack_ms, release_ms):
    a = 1 - math.exp(-1 / (attack_ms * 0.001 * RATE))
    r = 1 - math.exp(-1 / (release_ms * 0.001 * RATE))
    env, out = 0.0, np.empty(len(sig))
    for i, v in enumerate(np.abs(sig)):
        env += (v - env) * (a if v > env else r)
        out[i] = env
    return 20 * np.log10(np.maximum(out, 1e-9))


detected = lowpass(x, 1000.0)
peak = follower(detected, 0.01, 10.0)        # today's peak detector
level = follower(detected, 1.0, 20.0)        # Transient's level
average = follower(detected, 50.0, 50.0)     # and its own average


def level_gate(threshold, hysteresis=-6.0, hold_ms=150.0):
    hold = int(hold_ms * 0.001 * RATE)
    open_, armed, left, state = np.zeros(n, bool), True, 0, False
    for i in range(n):
        if peak[i] >= threshold and armed:
            state, armed, left = True, False, hold
        if peak[i] < threshold:
            armed = True
        if state:
            if peak[i] >= threshold + hysteresis:
                left = hold
            elif left > 0:
                left -= 1
            else:
                state = False
        open_[i] = state
    return open_


def transient_gate(floor, rise_db, hold_ms=150.0):
    hold = int(hold_ms * 0.001 * RATE)
    open_, left, armed = np.zeros(n, bool), 0, True
    for i in range(n):
        rise = level[i] - average[i]
        if rise >= rise_db and level[i] >= floor and armed:
            left, armed = hold, False
        if rise < 0.5 * rise_db:
            armed = True   # re-armed once the rise has passed
        if left > 0:
            left -= 1
        open_[i] = left > 0
    return open_


def score(open_):
    caught = sum(bool(open_[h:h + int(0.005 * RATE)].any()) for h in hits)
    near = np.zeros(n, bool)
    for h in hits:
        near[max(0, h - int(0.005 * RATE)):h + int(0.25 * RATE)] = True
    return caught, np.sum(open_ & ~near) / RATE


print("Fancy Gate: level against transient detection, tom mic with bass bleed that's louder in the chorus (12 hits):")
for threshold in (-30, -22, -16):
    c, f = score(level_gate(threshold))
    print(f"  Level, Threshold {threshold:4d} dB:                 {c:2d} hits caught, open {f:4.2f} s on the bass")
for rise in (6, 7, 8, 9, 12):
    c, f = score(transient_gate(-40, rise))
    print(f"  Transient, Rise {rise:2d} dB, Threshold -40 dB:   {c:2d} hits caught, open {f:4.2f} s on the bass")
