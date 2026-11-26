"""Pre-push check for Fancy Gate's multiband gating (docs/gate.md, Phase 4 Wave 3), modelling GateDsp as built.

docs/research/gate-multiband-model.py chose the design with a simple gate (a linear gain fade). This runs the
numbers tests/GateW3Tests.cpp checks through GateDsp's own machine and curve: the position mapped to gain in dB
along a raised cosine (Range x (1 - shaped)), Attack and Release as linear steps of the position with a 0.5 ms
floor, a peak detector (RMS size 0), linked channels, and the band's Threshold offset and Range depth.

  3. the tom mic at 2 bands (250 Hz, offsets 0 / +6 dB, Threshold -12 dB, Range -40 dB): tom tail kept, cymbal with
     toms, bleed in gaps; and a wideband gate at -6 dB;
  4. band independence: a 100 Hz burst over Threshold and a steady 5 kHz tone under band 2's threshold;
  5. depth: band 2 at 50 % with Range -40 dB, and with Range -inf;
  6. offsets: the level at which a band at +6 dB opens on a rising tone, against one at 0 dB.

Run: python docs/research/gate-w3-prepush.py
"""
import numpy as np

RATE = 48000.0


def biquad(x, b, a):
    y = np.empty_like(x)
    x1 = x2 = y1 = y2 = 0.0
    b0, b1, b2 = b
    _, a1, a2 = a
    for i, v in enumerate(x):
        out = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, v, y1, out
        y[i] = out
    return y


def butter2(hz, kind):
    w = 2 * np.pi * hz / RATE
    c, s = np.cos(w), np.sin(w)
    alpha = s / (2 * 0.7071067811865476)
    a0 = 1 + alpha
    b = [(1 - c) / 2, 1 - c, (1 - c) / 2] if kind == "low" else [(1 + c) / 2, -(1 + c), (1 + c) / 2]
    return [v / a0 for v in b], [1.0, -2 * c / a0, (1 - alpha) / a0]


def lr24(x, hz, kind):
    b, a = butter2(hz, kind)
    return biquad(biquad(x, b, a), b, a)


def split(x, crossovers):
    bands, rest = [], x
    for i, hz in enumerate(crossovers):
        low = lr24(rest, hz, "low")
        rest = lr24(rest, hz, "high")
        for above in crossovers[i + 1:]:
            low = lr24(low, above, "low") + lr24(low, above, "high")
        bands.append(low)
    bands.append(rest)
    return bands


def gate_dsp(detector, open_db, hysteresis_db=-6.0, attack_ms=0.5, hold_ms=40.0, release_ms=120.0, range_db=-40.0):
    """GateDsp::Machine::step and gainFor, Gate mode, as built. range_db is the band's (Range x depth)."""
    level = 20 * np.log10(np.maximum(np.abs(detector), 1e-15))
    minimum_fade = max(1, int(min(32.0, round(0.0005 * RATE))))
    up = 1.0 / max(minimum_fade, attack_ms * 1e-3 * RATE)
    down = 1.0 / max(minimum_fade, release_ms * 1e-3 * RATE)
    hold_samples = int(round(hold_ms * 1e-3 * RATE))
    close_db = open_db + hysteresis_db
    state, position, hold_left, armed = "closed", 0.0, 0, True
    gains, opens = np.empty(len(detector)), []
    for i, lv in enumerate(level):
        if lv < open_db:
            armed = True
        trigger = armed and lv >= open_db
        if state == "closed":
            if trigger:
                state, armed = "opening", False
                opens.append(i)
        elif state == "closing":
            if trigger:
                state, armed = "opening", False
                opens.append(i)
            else:
                position -= down
                if position <= 0.0:
                    position, state = 0.0, "closed"
        elif state == "opening":
            position += up
            if position >= 1.0:
                position, state = 1.0, "open"
        elif state == "open":
            if lv < close_db:
                state, hold_left = "hold", hold_samples
        elif state == "hold":
            if trigger:
                state, armed = "open", False
            elif lv >= close_db:
                state = "open"
            else:
                hold_left -= 1
                if hold_left <= 0:
                    state = "closing"
        shaped = 0.5 - 0.5 * np.cos(np.pi * position)
        gains[i] = 10 ** (range_db * (1 - shaped) / 20)
    return gains, opens


# 3. The model's tom mic, as tests/GateW3Tests.cpp builds it (its own random numbers, so the figures differ a little).
rng = np.random.default_rng(7)
n = int(RATE * 4.5)
hits = [int((0.25 + 0.75 * k) * RATE) for k in range(6)]
tom = np.zeros(n)
for h in hits:
    k = np.arange(min(int(0.8 * RATE), n - h))
    tom[h:h + len(k)] += 0.7 * np.exp(-k / (0.18 * RATE)) * np.sin(2 * np.pi * 110 * k / RATE * (1 + 0.15 * np.exp(-k / (0.02 * RATE))))
    tom[h:h + len(k)] += 0.25 * np.exp(-k / (0.004 * RATE)) * rng.standard_normal(len(k)) * (k < 0.004 * RATE)
cymbal_env = np.full(n, 0.02)
for c in (int(0.6 * RATE), int(2.9 * RATE)):
    k = np.arange(n - c)
    cymbal_env[c:] += 0.06 * np.exp(-k / (0.6 * RATE))
cymbal = lr24(rng.standard_normal(n), 3000, "high") * cymbal_env
snare = np.zeros(n)
for s in hits:
    s += int(0.375 * RATE)
    if s < n:
        k = np.arange(min(int(0.15 * RATE), n - s))
        snare[s:s + len(k)] += rng.standard_normal(len(k)) * 0.05 * np.exp(-k / (0.03 * RATE))
snare = lr24(lr24(snare, 200, "high"), 2000, "low")
mic = tom + cymbal + snare
TOM_LOW = lr24(tom, 300, "low")


def tail_kept(out):
    low = lr24(out, 300, "low")
    kept = []
    for h in hits:
        a, b = h + int(0.03 * RATE), h + int(0.4 * RATE)
        kept.append(np.sum(low[a:b] ** 2) / np.sum(TOM_LOW[a:b] ** 2))
    return 10 * np.log10(np.mean(kept))


def high_ratio(out, inside):
    hi_in, hi_out = lr24(mic, 2000, "high"), lr24(out, 2000, "high")
    mask = np.zeros(n, bool)
    for h in hits:
        if inside:
            mask[h + int(0.03 * RATE):h + int(0.4 * RATE)] = True
        else:
            mask[max(0, h - int(0.01 * RATE)):h + int(0.4 * RATE)] = True
    if not inside:
        mask = ~mask
    return 10 * np.log10(np.mean(hi_out[mask] ** 2) / np.mean(hi_in[mask] ** 2))


bands = split(mic, [250.0])
out = np.zeros(n)
for band, offset in zip(bands, [0.0, 6.0]):
    g, _ = gate_dsp(band, -12.0 + offset)
    out += band * g
print("3. tom mic, 2 bands at 250 Hz, 0 / +6 dB, Threshold -12, Range -40:")
print(f"   tom tail kept {tail_kept(out):+.2f} dB (test: within 1 dB), cymbal with toms {high_ratio(out, True):+.1f} dB (test: <= -8), "
      f"bleed in gaps {high_ratio(out, False):+.1f} dB (test: <= -30)")
g, _ = gate_dsp(mic, -6.0)
print(f"   wideband at -6 dB: cymbal with toms {high_ratio(mic * g, True):+.1f} dB (test: >= -3)")

# 4. Band independence: 100 Hz bursts at -6 dBFS peak (0.5 s on, 0.5 s off) and a steady 5 kHz tone at -30 dBFS peak;
# Threshold -20 dB, band 2 at +6 dB (so -14 dB); 2 bands at 1 kHz. The 5 kHz tone's band stays shut.
m = int(2.0 * RATE)
t = np.arange(m) / RATE
burst = 0.5 * np.sin(2 * np.pi * 100 * t) * ((t % 1.0) < 0.5)
tone = 0.0316 * np.sin(2 * np.pi * 5000 * t)
low, high = split(burst + tone, [1000.0])
g1, _ = gate_dsp(low, -20.0)
g2, _ = gate_dsp(high, -14.0)
on = (t % 1.0 > 0.1) & (t % 1.0 < 0.4)
print(f"\n4. independence: band 1's gain in the bursts {20 * np.log10(np.min(g1[on])):+.2f} dB; band 2's over the whole "
      f"{20 * np.log10(np.max(g2)):+.2f} dB (test: band 2 at Range, -40 dB)")

# 5. Depth: band 2 at 50 % with Range -40 dB is -20 dB shut; with Range -inf (the curve's -120 dB floor), -60 dB.
for range_db, want in ((-40.0, -20.0), (-120.0, -60.0)):
    g, _ = gate_dsp(np.full(1000, 1e-4), -12.0, range_db=range_db * 0.5)
    print(f"5. depth 50 %, Range {range_db:+.0f}: shut gain {20 * np.log10(g[-1]):+.2f} dB (test {want:+.0f} +- 0.5)")

# 6. Offsets: a 2 kHz tone (band 2 of 2 at 250 Hz) rising 1 dB per 10 ms from -60 dBFS; the level at the opening for
# offsets 0 and +6 dB, Threshold -30 dB.
k = int(0.8 * RATE)
tt = np.arange(k) / RATE
ramp_db = -60.0 + 100.0 * tt
rise = 10 ** (ramp_db / 20) * np.sin(2 * np.pi * 2000 * tt)
_, high = split(rise, [250.0])
levels = {}
for offset in (0.0, 6.0):
    _, opens = gate_dsp(high, -30.0 + offset)
    levels[offset] = ramp_db[opens[0]]
print(f"6. offsets: opens at {levels[0.0]:+.2f} dB (offset 0) and {levels[6.0]:+.2f} dB (offset +6): "
      f"{levels[6.0] - levels[0.0]:+.2f} dB later (test 6 +- 0.5)")
