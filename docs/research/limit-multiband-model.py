"""Model of Fancy Limit's multiband limiting (docs/limit.md, Phase 4 Wave 3; layout A of mockup 74).

The bands are split by a Linkwitz-Riley crossover (MultiComp's minimum-phase cascade, each lower band
through the all-passes of the crossovers above it, so the bands sum to one all-pass), each band is
limited on its own with its own release, the bands are summed, and today's limiter (the final stage)
then holds the ceiling across the whole signal.

What it measures, on a mix pushed 8 dB into a 0 dBFS ceiling (60 Hz kicks with a click every 0.5 s,
a steady 1 kHz tone, steady hi-hat noise above 6 kHz):

  1. pumping: how far the steady 1 kHz tone and the hats dip in the 150 ms after each kick, against
     their level just before it (the wideband limiter turns everything down when the kick hits);
  2. the final stage's gain reduction once the bands have been limited (how much is left for it);
  3. loudness (mean square) and the output's peak;
  4. the crossover: the bands' sum against the input, magnitude, with no limiting.

for four band ceilings (each band at Ceiling, and 1, 3 and 6 dB under it).

Run: python docs/research/limit-multiband-model.py
"""
import numpy as np

RATE = 48000
rng = np.random.default_rng(5)
n = RATE * 4
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
    """Second-order Butterworth (RBJ cookbook, Q 0.707)."""
    w = 2 * np.pi * hz / RATE
    c, s = np.cos(w), np.sin(w)
    alpha = s / (2 * 0.7071067811865476)
    a0 = 1 + alpha
    if kind == "low":
        b = [(1 - c) / 2, 1 - c, (1 - c) / 2]
    else:
        b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]
    return [v / a0 for v in b], [1.0, -2 * c / a0, (1 - alpha) / a0]


def lr24(x, hz, kind):
    b, a = butter2(hz, kind)
    return biquad(biquad(x, b, a), b, a)


def allpass_lr24(x, hz):
    """An LR24 low plus high is an all-pass: the phase every band below this crossover must get."""
    return lr24(x, hz, "low") + lr24(x, hz, "high")


def split(x, crossovers):
    """Bands, low to high; each lower band through the all-passes of every crossover above it."""
    bands, rest = [], x
    for i, hz in enumerate(crossovers):
        low = lr24(rest, hz, "low")
        rest = lr24(rest, hz, "high")
        for above in crossovers[i + 1:]:
            low = allpass_lr24(low, above)
        bands.append(low)
    bands.append(rest)
    return bands


def limiter(v, ceiling=1.0, lookahead_ms=2.0, release_ms=80.0):
    """Lookahead peak limiter on the mono sum's peaks (the plugin links the channels at Link 100 %)."""
    look = int(lookahead_ms * 0.001 * RATE)
    need = np.maximum(np.abs(v) / ceiling, 1.0)
    window = np.lib.stride_tricks.sliding_window_view(np.pad(need, (0, look)), look + 1).max(axis=1)
    rel = 1 - np.exp(-1 / (release_ms * 0.001 * RATE))
    g, gains = 1.0, np.empty(len(v))
    for i, w in enumerate(window):
        target = 1.0 / w
        g = target if target < g else g + (target - g) * rel
        gains[i] = g
    # The lookahead: the gain leads the signal by `look` samples, so delay the signal.
    delayed = np.concatenate([np.zeros(look), v[:-look]]) if look else v
    gains = np.concatenate([np.ones(look), gains[:-look]]) if look else gains
    return delayed * gains, gains


def db(v):
    return 10 * np.log10(np.mean(v ** 2) + 1e-30)


# The mix, in two versions: the kick with and without its 3 ms click (the click is broadband, so it reaches
# every band; the body is all under 150 Hz).
body, click = np.zeros(n), np.zeros(n)
hits = [int(k * 0.5 * RATE) for k in range(8)]
for start in hits:
    k = np.arange(min(int(0.5 * RATE), n - start))
    body[start:start + len(k)] += 0.9 * np.exp(-k / (0.09 * RATE)) * np.sin(2 * np.pi * 60 * k / RATE)
    click[start:start + len(k)] += 0.5 * np.exp(-k / (0.002 * RATE)) * rng.standard_normal(len(k)) * (k < 0.003 * RATE)
tone = 0.12 * np.sin(2 * np.pi * 1000 * t)
hats = 0.5 * lr24(lr24(rng.standard_normal(n), 6000, "high"), 6000, "high") * 0.15
MIXES = {"kick body only": (body + tone + hats) * 10 ** (8 / 20),
         "kick with its click": (body + click + tone + hats) * 10 ** (8 / 20)}
mix = None

CROSSOVERS = {2: [150.0], 3: [150.0, 2500.0], 4: [150.0, 1000.0, 6000.0]}
RELEASES = {1: [80.0], 2: [120.0, 60.0], 3: [120.0, 80.0, 50.0], 4: [120.0, 90.0, 60.0, 40.0]}


def dips(gain_on_part, start_ms=5, end_ms=150):
    """How far a steady part's gain falls after each kick, against just before it, in dB (the mean of the
    deepest point after each hit but the first)."""
    out = []
    for start in hits[1:]:
        before = gain_on_part[start - int(0.03 * RATE):start - int(0.005 * RATE)].mean()
        window = gain_on_part[start + int(start_ms * 0.001 * RATE):start + int(end_ms * 0.001 * RATE)]
        out.append((20 * np.log10(window.min() / before), 20 * np.log10(window.mean() / before)))
    return np.mean(out, axis=0)


def dip_text(gain_on_part):
    deepest, mean = dips(gain_on_part)
    return f"{deepest:6.1f} / {mean:5.1f}"


def run(bands, under_db=0.0):
    """The whole chain; returns the output and the gain each steady part (1 kHz, hats) got overall."""
    if bands == 1:
        out, g = limiter(mix)
        return out, g, g, np.ones(n), g
    parts = split(mix, CROSSOVERS[bands])
    ceiling = 10 ** (-under_db / 20)
    limited, gains = [], []
    for part, release in zip(parts, RELEASES[bands]):
        y, g = limiter(part, ceiling, release_ms=release)
        limited.append(y)
        gains.append(g)
    summed = np.sum(limited, axis=0)
    out, final = limiter(summed)
    look = int(0.002 * RATE)
    final_aligned = final  # the final stage's own gain, on the summed (already delayed) signal
    # The band that holds 1 kHz and the one that holds the hats, through both stages (band gains delayed by
    # the final stage's lookahead to line up with it).
    def through(i):
        return np.concatenate([np.ones(look), gains[i][:-look]]) * final_aligned
    tone_band = 1 if bands == 2 else (1 if bands == 3 else 1)
    hats_band = bands - 1
    if bands == 2:
        tone_band = 1
    return out, through(tone_band), through(hats_band), final, gains


print("Mix: 60 Hz kicks with a click every 0.5 s, a steady 1 kHz tone, hats above 6 kHz; pushed 8 dB into a 0 dBFS ceiling.")
print("Crossovers: 2 bands 150 Hz; 3 bands 150 Hz, 2.5 kHz; 4 bands 150 Hz, 1 kHz, 6 kHz. Releases low to high: "
      "2 bands 120, 60; 3 bands 120, 80, 50; 4 bands 120, 90, 60, 40 ms. The final stage: 2 ms, 80 ms.\n")

# 4. The crossover sums flat (an all-pass): the magnitude of the sum against the input, by FFT of an impulse.
impulse = np.zeros(8192)
impulse[0] = 1.0
for bands in (2, 3, 4):
    total = np.sum(split(impulse, CROSSOVERS[bands]), axis=0)
    mag = 20 * np.log10(np.abs(np.fft.rfft(total)) + 1e-30)
    print(f"crossover, {bands} bands: the sum's magnitude {mag[1:].min():+.4f} to {mag[1:].max():+.4f} dB (all-pass)")
print()

for name, m in MIXES.items():
  mix = m
  print(f"{name}:")
  print("bands  band ceiling   1 kHz dip, deepest / mean   hats dip, deepest / mean   final stage GR mean / max   loudness   output peak")
  for bands, under in [(1, 0.0), (2, 0.0), (3, 0.0), (4, 0.0), (3, 1.0), (3, 3.0), (3, 6.0)]:
    out, g_tone, g_hats, final, _ = run(bands, under)
    gr = -20 * np.log10(final)
    label = "(wideband)" if bands == 1 else f"Ceiling -{under:.0f} dB" if under else "Ceiling"
    final_text = "  (it is the limiter)  " if bands == 1 else f"{gr.mean():5.2f} / {gr.max():5.2f} dB     "
    print(f"  {bands}    {label:13s}    {dip_text(g_tone)} dB            {dip_text(g_hats)} dB          {final_text}  {db(out):6.2f} dB  "
          f"{20 * np.log10(np.max(np.abs(out))):+6.2f} dBFS")
  print()
