"""Model of Fancy Gate's multiband gating (docs/gate.md, Phase 4 Wave 3; layout A of mockup 75).

The audio and the detector are split by the same Linkwitz-Riley crossover (MultiComp's minimum-phase cascade, as
Limit's bands, so the bands sum to an all-pass); each band's detector drives its own state machine (Gate's: open at
Threshold, close at Threshold + Hysteresis, hold, linear attack and release of the position, the position mapped to
gain over Range), with the band's own Threshold offset and Range depth, and gates that band's audio.

The case it measures, a tom close mic: tom hits every 0.75 s (a 110 Hz body decaying over 180 ms and a 4 ms stick
click), cymbal bleed above 3 kHz at a steady level with two crashes, and a little snare bleed around 200 Hz to 2 kHz.

  1. tom tail kept: the tom body's energy in the output's 30-400 ms after each hit against the input's (low band);
  2. bleed in the gaps: the output's level above 2 kHz between hits, crashes included, against the input's;
  3. false opens: how often the gate opens with no tom hit (crashes and snare bleed);
  4. the crossover sums flat (the all-pass), as Limit's model checks.

for a wideband gate set two ways (low enough to keep the tail; high enough to reject the crashes), and 2 and 3 bands.

Run: python docs/research/gate-multiband-model.py
"""
import numpy as np

RATE = 48000
rng = np.random.default_rng(7)
n = int(RATE * 4.5)
t = np.arange(n) / RATE


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


def gate(detector, threshold_db, hysteresis_db=-6.0, attack_ms=0.5, hold_ms=40.0, release_ms=120.0, range_db=-40.0):
    """Fancy Gate's machine on a peak detector (RMS size 0): returns the gain per sample and the opening times."""
    level = 20 * np.log10(np.abs(detector) + 1e-12)
    up, down = 1.0 / max(1.0, attack_ms * 1e-3 * RATE), 1.0 / (release_ms * 1e-3 * RATE)
    hold_samples = int(hold_ms * 1e-3 * RATE)
    close_db = threshold_db + hysteresis_db
    floor = 10 ** (range_db / 20)
    state, position, hold_left, armed = "closed", 0.0, 0, True
    gains, opens = np.empty(len(detector)), []
    for i, lv in enumerate(level):
        if lv < threshold_db:
            armed = True
        if armed and lv >= threshold_db and state in ("closed", "closing", "hold"):
            if state in ("closed", "closing"):
                opens.append(i)
            state, armed = "opening", False
        if state == "opening":
            position = min(1.0, position + up)
            if position >= 1.0:
                state = "open"
        if state == "open" and lv < close_db:
            state, hold_left = "hold", hold_samples
        elif state == "hold":
            if lv >= close_db:
                state = "open"
            else:
                hold_left -= 1
                if hold_left <= 0:
                    state = "closing"
        if state == "closing":
            position = max(0.0, position - down)
            if position <= 0.0:
                state = "closed"
        gains[i] = floor + (1 - floor) * position
    return gains, opens


# The mic: tom hits, cymbal bleed with two crashes, and snare bleed.
hits = [int((0.25 + 0.75 * k) * RATE) for k in range(6)]
tom = np.zeros(n)
for h in hits:
    k = np.arange(min(int(0.8 * RATE), n - h))
    tom[h:h + len(k)] += 0.7 * np.exp(-k / (0.18 * RATE)) * np.sin(2 * np.pi * 110 * k / RATE * (1 + 0.15 * np.exp(-k / (0.02 * RATE))))
    tom[h:h + len(k)] += 0.25 * np.exp(-k / (0.004 * RATE)) * rng.standard_normal(len(k)) * (k < 0.004 * RATE)
cymbal_env = np.full(n, 0.02)
for c in (int(0.6 * RATE), int(2.9 * RATE)):  # two crashes, between hits
    k = np.arange(n - c)
    cymbal_env[c:] += 0.06 * np.exp(-k / (0.6 * RATE))
cymbal = lr24(rng.standard_normal(n), 3000, "high") * cymbal_env
snare = np.zeros(n)
for s in hits:
    s += int(0.375 * RATE)  # on the off-beats
    if s < n:
        k = np.arange(min(int(0.15 * RATE), n - s))
        burst = rng.standard_normal(len(k)) * 0.05 * np.exp(-k / (0.03 * RATE))
        snare[s:s + len(k)] += burst
snare = lr24(lr24(snare, 200, "high"), 2000, "low")
mic = tom + cymbal + snare

TOM_LOW = lr24(tom, 300, "low")  # the tom body, to measure its tail


def tail_kept(out):
    low = lr24(out, 300, "low")
    kept = []
    for h in hits:
        a, b = h + int(0.03 * RATE), h + int(0.4 * RATE)
        kept.append(np.sum(low[a:b] ** 2) / np.sum(TOM_LOW[a:b] ** 2))
    return 10 * np.log10(np.mean(kept))


def cymbal_with_toms(out):
    """The cymbal let through while the gate is open on a tom: above 2 kHz, 30-400 ms after each hit."""
    hi_in, hi_out = lr24(mic, 2000, "high"), lr24(out, 2000, "high")
    mask = np.zeros(n, bool)
    for h in hits:
        mask[h + int(0.03 * RATE):h + int(0.4 * RATE)] = True
    return 10 * np.log10(np.mean(hi_out[mask] ** 2) / np.mean(hi_in[mask] ** 2))


def bleed_in_gaps(out):
    hi_in, hi_out = lr24(mic, 2000, "high"), lr24(out, 2000, "high")
    mask = np.ones(n, bool)
    for h in hits:
        mask[max(0, h - int(0.01 * RATE)):h + int(0.4 * RATE)] = False
    return 10 * np.log10(np.mean(hi_out[mask] ** 2) / np.mean(hi_in[mask] ** 2))


def false_opens(opens):
    return sum(1 for o in opens if not any(h - int(0.005 * RATE) <= o <= h + int(0.05 * RATE) for h in hits))


def run_wideband(threshold_db):
    g, opens = gate(mic, threshold_db)
    return mic * g, len(opens), false_opens(opens)


def run_bands(crossovers, offsets_db, depths, threshold_db=-12.0, range_db=-40.0):
    audio = split(mic, crossovers)
    out = np.zeros(n)
    total, false = 0, 0
    for band, offset, depth in zip(audio, offsets_db, depths):
        g, opens = gate(band, threshold_db + offset, range_db=range_db * depth)
        out += band * g
        total += len(opens)
        false += false_opens(opens)
    return out, total, false


print("A tom close mic: 6 hits, cymbal bleed above 3 kHz with two crashes, snare bleed 200 Hz to 2 kHz.")
print("Gate: Hysteresis -6 dB, Attack 0.5 ms, Hold 40 ms, Release 120 ms, Range -40 dB, peak detector.\n")

impulse = np.zeros(8192)
impulse[0] = 1.0
for xs in ([250.0], [250.0, 2500.0]):
    total = np.sum(split(impulse, xs), axis=0)
    mag = 20 * np.log10(np.abs(np.fft.rfft(total)) + 1e-30)
    print(f"crossover {xs}: the sum's magnitude {mag[1:].min():+.4f} to {mag[1:].max():+.4f} dB (all-pass)")
print()

def peak_db(x):
    return 20 * np.log10(np.max(np.abs(x)))


print(f"levels (peak): tom {peak_db(tom):+.1f} dBFS, steady cymbal {peak_db(cymbal[:int(0.5 * RATE)]):+.1f}, crash {peak_db(cymbal):+.1f}, "
      f"snare bleed {peak_db(snare):+.1f}\n")
print("setting                                                 tom tail kept  cymbal with toms  bleed in gaps  opens (false)")


def row(name, out, opens, false):
    print(f"{name:55s} {tail_kept(out):+6.1f} dB      {cymbal_with_toms(out):+6.1f} dB        {bleed_in_gaps(out):+6.1f} dB      {opens} ({false})")


for th in (-18.0, -12.0, -6.0):
    row(f"wideband, Threshold {th:+.0f} dB", *run_wideband(th))
configs = [
    ("2 bands at 250 Hz, offsets 0 / +6 dB", [250.0], [0.0, 6.0], [1.0, 1.0]),
    ("3 bands at 250 Hz, 2.5 kHz, offsets 0 / 0 / +6 dB", [250.0, 2500.0], [0.0, 0.0, 6.0], [1.0, 1.0, 1.0]),
    ("3 bands, as above, the top band's depth 50 % (-20 dB)", [250.0, 2500.0], [0.0, 0.0, 6.0], [1.0, 1.0, 0.5]),
    ("3 bands, all offsets 0 (the Threshold fader alone)", [250.0, 2500.0], [0.0, 0.0, 0.0], [1.0, 1.0, 1.0]),
    ("the proposed defaults, 2 bands: 250 Hz, offsets 0 / +6 dB", [250.0], [0.0, 6.0], [1.0, 1.0]),
    ("the proposed defaults, 3 bands: 250 Hz, 2.5 kHz, 0 / +6 / +6", [250.0, 2500.0], [0.0, 6.0, 6.0], [1.0, 1.0, 1.0]),
]
for name, xs, offsets, depths in configs:
    row(name, *run_bands(xs, offsets, depths))
