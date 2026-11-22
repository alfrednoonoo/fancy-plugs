"""Model of Fancy DS's dual-band detection (docs/ds.md, Phase 4 Wave 2, item 10 of the order).

A voice has two kinds of sibilance: "s" (and "z", "t") high up, about 5 to 10 kHz, and "sh" (and
"ch", "j") lower, about 2.5 to 5 kHz. One detection band can't treat both well: tuned to "s" it
misses "sh"; widened to cover both, it also takes the vowels' upper harmonics around 3 kHz down.

The test signal, 6 s at 48 kHz:
  - a voice: a 150 Hz tone (harmonics falling 6 dB/oct to 1 kHz, 12 above, to 8 kHz) shaped into
    syllables, plus a little breath noise;
  - "s" bursts: noise band-limited to 5.5-10 kHz, 90 ms, 8 dB above the voice's level there;
  - "sh" bursts: noise band-limited to 2.3-4.8 kHz, 130 ms, at about the vowels' level.

Each setting runs DS's engine as modelled here (docs/ds.md: SVF band-pass detector, 1 ms peak hold
and 20 dB/s fall, relative to a 50 ms full-band RMS, 8:1 with a 6 dB knee capped by Range, 0.5 ms
attack and 60 ms release in dB, the split by subtraction with the bell widening as it cuts):

  A. today's single band at its defaults (6.2 kHz, 1 octave, -28 dB relative, Range -12);
  B. the single band widened to catch both (4.6 kHz, 2.5 octaves);
  C. two bands: High as A, and Low at 3.5 kHz, 1 octave, its own Threshold -24 and Range -9,
     in series.

It prints how far each burst type is cut (the energy in its own band, during the bursts) and how
far the vowels' 2.5-4.5 kHz region is cut while no burst plays: the collateral dulling.

Run: py -3 docs/research/ds-dual-model.py
"""
import math

import numpy as np

RATE = 48000.0
rng = np.random.default_rng(7)
N = int(6 * RATE)
t = np.arange(N) / RATE


def svf_coefs(freq, q):
    g = math.tan(math.pi * min(freq, 0.45 * RATE) / RATE)
    k = 1 / q
    a1 = 1 / (1 + g * (g + k))
    return g, k, a1


def svf(x, freq, q, kind):
    g, k, a1 = svf_coefs(freq, q)
    a2, a3 = g * a1, g * g * a1
    ic1 = ic2 = 0.0
    y = np.empty_like(x)
    for i, v in enumerate(x):
        v3 = v - ic2
        v1 = a1 * ic1 + a2 * v3
        v2 = ic2 + a2 * ic1 + a3 * v3
        ic1, ic2 = 2 * v1 - ic1, 2 * v2 - ic2
        y[i] = v1 * k if kind == "band" else (v2 if kind == "low" else v - k * v1 - v2)
    return y


def band_limited_noise(lo, hi, n):
    noise = rng.standard_normal(n)
    return svf(svf(noise, hi, 0.7071, "low"), lo, 0.7071, "high")


def q_for_width(octaves):
    return math.sqrt(2 ** octaves) / (2 ** octaves - 1)


# The voice: syllables of a harmonic tone, 300 ms on, 100 ms gap.
voice = np.zeros(N)
for h in range(1, 54):
    f = 150.0 * h
    if f > 8000:
        break
    # Falling 6 dB/oct to 1 kHz and 18 dB/oct above, as a vowel's harmonics roughly do.
    voice += np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi)) / h / max(1.0, f / 1000.0) ** 2
envelope = ((t % 0.4) < 0.3).astype(float)
envelope = svf(envelope, 30.0, 0.7071, "low")
voice *= 0.1 * envelope
voice += 0.002 * rng.standard_normal(N)

def rms(sig):
    return math.sqrt(np.mean(sig ** 2))


# Each burst type set against the vowels in its own band: "s" 10 dB over, "sh" 8 dB over.
voiced = envelope > 0.5
vowel_s = rms(svf(svf(voice, 10000, 0.7071, "low"), 5500, 0.7071, "high")[voiced])
vowel_sh = rms(svf(svf(voice, 4800, 0.7071, "low"), 2300, 0.7071, "high")[voiced])

s_mask = np.zeros(N, bool)
sh_mask = np.zeros(N, bool)
x = voice.copy()
for k in range(7):   # a burst on every other syllable, alternately "s" and "sh"
    start = int((0.25 + 0.8 * k) * RATE)
    s_burst = k % 2 == 0
    length = int((0.09 if s_burst else 0.13) * RATE)
    burst = band_limited_noise(5500, 10000, length) if s_burst else band_limited_noise(2300, 4800, length)
    burst *= np.hanning(length) / rms(burst * np.hanning(length))
    x[start:start + length] += burst * (vowel_s * 10 ** (10 / 20) if s_burst else vowel_sh * 10 ** (8 / 20))
    (s_mask if s_burst else sh_mask)[start:start + length] = True

quiet = ~(s_mask | sh_mask)
# Voice only, away from bursts (200 ms either side, past the release), where the vowels sound.
for mask in (s_mask, sh_mask):
    idx = np.flatnonzero(mask)
    for i in idx[::200]:
        quiet[max(0, i - 9600):i + 9600] = False
quiet &= envelope > 0.5


def detector_level(x, freq, octaves):
    band = svf(x, freq, q_for_width(octaves), "band")
    hold, fall = int(0.001 * RATE), 20.0 / RATE
    peak, left = -200.0, 0
    full, ms = 0.0, 0.0
    rms_c = 1 - math.exp(-1 / (0.05 * RATE))
    rel = np.empty(len(x))
    for i in range(len(x)):
        db = 20 * math.log10(max(abs(band[i]), 1e-10))
        if db >= peak:
            peak, left = db, hold
        elif left > 0:
            left -= 1
        else:
            peak -= fall
        ms += (x[i] * x[i] - ms) * rms_c
        full_db = max(10 * math.log10(max(ms, 1e-20)), -60.0)
        rel[i] = peak - full_db
    return rel


def gain_db(level, threshold, range_db):
    slope, knee = 0.875, 6.0
    over = level - threshold
    red = np.where(over <= -knee / 2, 0.0,
                   np.where(over >= knee / 2, slope * over, slope * (over + knee / 2) ** 2 / (2 * knee)))
    red = np.minimum(red, -range_db)
    att, rel = 1 - math.exp(-1 / (0.0005 * RATE)), 1 - math.exp(-1 / (0.06 * RATE))
    env, out = 0.0, np.empty(len(red))
    for i, r in enumerate(red):
        env += (r - env) * (att if r > env else rel)
        out[i] = -env
    return out


def split(x, g_db, freq, octaves):
    # out = x + (g - 1) * band, the bell widening as it cuts (Q times sqrt of the gain).
    g = 10 ** (g_db / 20)
    q0 = q_for_width(octaves)
    out = np.empty_like(x)
    ic1 = ic2 = 0.0
    gg = math.tan(math.pi * freq / RATE)
    for i in range(len(x)):
        q = q0 * math.sqrt(g[i])
        k = 1 / q
        a1 = 1 / (1 + gg * (gg + k)); a2 = gg * a1; a3 = gg * a2
        v3 = x[i] - ic2
        v1 = a1 * ic1 + a2 * v3
        v2 = ic2 + a2 * ic1 + a3 * v3
        ic1, ic2 = 2 * v1 - ic1, 2 * v2 - ic2
        out[i] = x[i] + (g[i] - 1) * v1 * k
    return out


def energy_db(sig, lo, hi, mask):
    b = band_limited_noise_filter(sig, lo, hi)
    return 10 * math.log10(np.mean(b[mask] ** 2) + 1e-20)


def band_limited_noise_filter(sig, lo, hi):
    return svf(svf(sig, hi, 0.7071, "low"), lo, 0.7071, "high")


def one_band(sig, freq, octaves, threshold, range_db):
    return split(sig, gain_db(detector_level(sig, freq, octaves), threshold, range_db), freq, octaves)


reference = {
    "s": energy_db(x, 5500, 10000, s_mask),
    "sh": energy_db(x, 2300, 4800, sh_mask),
    "vowels": energy_db(x, 2500, 4500, quiet),
}


def report(name, y):
    cut = {
        "s": energy_db(y, 5500, 10000, s_mask) - reference["s"],
        "sh": energy_db(y, 2300, 4800, sh_mask) - reference["sh"],
        "vowels": energy_db(y, 2500, 4500, quiet) - reference["vowels"],
    }
    print(f"  {name:<44} s {cut['s']:+6.1f} dB   sh {cut['sh']:+6.1f} dB   vowels 2.5-4.5 kHz {cut['vowels']:+6.1f} dB")


print(f"Fancy DS dual band (cuts in each sound's own band; vowels over {quiet.sum() / RATE:.2f} s with no burst near):")
report("A. one band, 6.2 kHz, 1 oct (today)", one_band(x, 6200, 1.0, -28, -12))
report("B. one band widened, 4.6 kHz, 2.5 oct", one_band(x, 4600, 2.5, -28, -12))
high = one_band(x, 6200, 1.0, -28, -12)
report("C. two bands: High 6.2k + Low 3.5k (-24, -9)", one_band(high, 3500, 1.0, -24, -9))

for low_threshold in (-20, -16):
    report(f"C. two bands, Low's Threshold {low_threshold}", one_band(high, 3500, 1.0, low_threshold, -9))

# Detecting both bands from the input, rather than Low from High's output (decision 2's check).
low_from_input = gain_db(detector_level(x, 3500, 1.0), -24, -9)
report("C'. two bands, both detected from the input", split(high, low_from_input, 3500, 1.0))
