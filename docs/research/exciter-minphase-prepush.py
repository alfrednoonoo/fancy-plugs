"""Pre-push check for Fancy Exciter's item 44, minimum-phase oversampling (docs/exciter.md, "Phase 4
Wave 2", test 5), written before any code (24 November 2026).

The design model (docs/research/exciter-kicker-w2-model.py, part 44) stood in an 8th-order
Butterworth for the minimum-phase filters and found the dry + excited sum within 0.71 dB of linear
phase above Freq; test 5 allows 1 dB. The plugin will use SwitchedOversampler's minimum-phase
setting: JUCE 8.0.17's polyphase IIR half-bands (FilterDesign::designIIRLowpassHalfBandPolyphase-
AllpassMethod), transition 0.06 and -100 dB for the first stage, 0.12 and -90 dB for the second,
with integer latency (a Thiran fractional delay after the last stage). Those are much steeper than
the stand-in, so their phase near the top of the band is the question.

This reproduces JUCE's design and processing exactly (both stages up, both down, the Thiran delay),
measures the chain's small-signal response at the base rate, delays the dry by the reported integer
latency, and sums them as the design model did (dry 1, excited path at Mix Back -6 dB), from Freq
(3 kHz) to 20 kHz.

Run: python docs/research/exciter-minphase-prepush.py
"""
import math

import numpy as np

FS = 48000


def design(transition, stopband_db):
    # JUCE's FilterDesign::designIIRLowpassHalfBandPolyphaseAllpassMethod, line for line.
    wt = 2 * math.pi * transition
    ds = 10 ** (stopband_db / 20)
    k = math.tan((math.pi - wt) / 4) ** 2
    kp = math.sqrt(1 - k * k)
    e = (1 - math.sqrt(kp)) / (1 + math.sqrt(kp)) * 0.5
    q = e + 2 * e ** 5 + 15 * e ** 9 + 150 * e ** 13
    k1 = ds * ds / (1 - ds * ds)
    n = int(math.ceil(math.log(k1 * k1 / 16) / math.log(q)))
    if n % 2 == 0:
        n += 1
    if n == 1:
        n = 3
    count = (n - 1) // 2
    ai = []
    for i in range(1, count + 1):
        num, m = 0.0, 0
        while True:
            delta = (-1) ** m * q ** (m * (m + 1)) * math.sin((2 * m + 1) * math.pi * i / n)
            num += delta
            m += 1
            if abs(delta) <= 1e-100:
                break
        num *= 2 * q ** 0.25
        den, m = 0.0, 1
        while True:
            delta = (-1) ** m * q ** (m * m) * math.cos(m * 2 * math.pi * i / n)
            den += delta
            m += 1
            if abs(delta) <= 1e-100:
                break
        den = 1 + 2 * den
        wi = num / den
        api = math.sqrt((1 - wi * wi * k) * (1 - wi * wi / k)) / (1 + wi * wi)
        ai.append((1 - api) / (1 + api))
    direct = [ai[i] for i in range(0, count, 2)]
    delayed = [ai[i] for i in range(1, count, 2)]
    return direct, delayed


def allpass_chain(x, coeffs):
    y = np.array(x, dtype=float)
    for a in coeffs:
        out = np.empty(len(y))
        v = 0.0
        for i, s in enumerate(y):
            o = a * s + v
            v = s - a * o
            out[i] = o
        y = out
    return y


def stage_up(x, design_):
    direct, delayed = design_
    y = np.empty(2 * len(x))
    y[0::2] = allpass_chain(x, direct)
    y[1::2] = allpass_chain(x, delayed)
    return y


def stage_down(x, design_):
    direct, delayed = design_
    d = allpass_chain(x[0::2], direct)
    late = allpass_chain(x[1::2], delayed)
    previous = np.concatenate([[0.0], late[:-1]])   # JUCE keeps the delayed path's last output
    return (previous + d) * 0.5


def phase_delay(coeffs_direct, coeffs_delayed, w):
    # The 2x-rate transfer 0.5 (A0(z^2) + z^-1 A1(z^2)), each section (a + z^-2)/(1 + a z^-2).
    z2 = np.exp(-2j * w)
    a0 = np.prod([(a + z2) / (1 + a * z2) for a in coeffs_direct], axis=0) if coeffs_direct else 1
    a1 = np.prod([(a + z2) / (1 + a * z2) for a in coeffs_delayed], axis=0) if coeffs_delayed else 1
    h = 0.5 * (a0 + np.exp(-1j * w) * a1)
    return -np.angle(h) / w


STAGES = [design(0.06, -100.0), design(0.12, -90.0)]

# JUCE's latency: each stage's up and down phase delay at a very low frequency, in its own
# rate's samples, scaled to the base rate (the first stage's by 1/2, the second's by 1/4).
w0 = 2 * math.pi * 0.0001
latency = 0.0
for index, d in enumerate(STAGES):
    stage_latency = 2 * float(phase_delay(d[0], d[1], np.array([w0]))[0])
    latency += stage_latency / 2 ** (index + 1)
fractional = 1.0 - (latency - math.floor(latency))
if abs(fractional - 1.0) < 1e-9:
    fractional = 0.0
elif fractional < 0.618:
    fractional += 1.0
integer_latency = round(latency + fractional)

# The chain's impulse response at the base rate.
n = 8192
impulse = np.zeros(n)
impulse[64] = 1.0
y = impulse
for d in STAGES:
    y = stage_up(y, d)
for d in reversed(STAGES):
    y = stage_down(y, d)
if fractional > 0:
    delay_int = int(math.floor(fractional))
    frac = fractional - delay_int
    if frac < 0.618 and delay_int >= 1:
        frac += 1
        delay_int -= 1
    alpha = (1 - frac) / (1 + frac)
    shifted = np.concatenate([np.zeros(delay_int), y[: len(y) - delay_int]])
    out = np.empty(len(shifted))
    v, previous = 0.0, 0.0
    for i, s in enumerate(shifted):
        o = previous + alpha * (s - v)      # value2 + alpha (value1 - v), value2 the sample before
        v = o
        previous = s
        out[i] = o
    y = out

f = np.linspace(3000, 20000, 500)
spectrum = np.fft.rfft(y, 1 << 16)
bins = np.fft.rfftfreq(1 << 16, 1 / FS)
h = np.interp(f, bins, spectrum.real) + 1j * np.interp(f, bins, spectrum.imag)
h *= np.exp(2j * np.pi * f * 64 / FS)                   # the impulse sat at 64
dry = np.exp(-2j * np.pi * f * integer_latency / FS)
wet = 10 ** (-6 / 20)
linear = 20 * math.log10(1 + wet)
summed = 20 * np.log10(np.abs(dry + wet * h))

print("Item 44, Exciter's dry + excited path with JUCE 8.0.17's minimum-phase 4x (small signals, Mix Back -6 dB):")
print(f"    stages: {len(STAGES[0][0]) + len(STAGES[0][1])} and {len(STAGES[1][0]) + len(STAGES[1][1])} allpass sections; "
      f"latency {latency:.3f} samples, reported {integer_latency} (test 5 asks for under 8)")
print(f"    the chain's own magnitude from 3 to 20 kHz: {20 * np.log10(np.abs(h)).min():+.3f} to {20 * np.log10(np.abs(h)).max():+.3f} dB")
print(f"    linear phase (today): flat at {linear:+.2f} dB")
for lo, hi in ((3000, 10000), (10000, 16000), (16000, 20000)):
    sel = (f >= lo) & (f <= hi)
    print(f"    minimum phase, {lo / 1000:4.0f}-{hi / 1000:2.0f} kHz: {summed[sel].min():+.2f} to {summed[sel].max():+.2f} dB "
          f"(furthest from linear {np.max(np.abs(summed[sel] - linear)):.2f} dB)")
worst = np.max(np.abs(summed - linear))
print(f"    test 5 (within 1 dB of linear phase from Freq to 20 kHz): furthest {worst:.2f} dB, "
      f"{'passes' if worst <= 1.0 else 'fails'}")
