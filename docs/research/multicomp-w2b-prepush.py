"""Pre-push check for Fancy MultiComp's Wave 2 items 23 to 26 (docs/multicomp.md, "Phase 4 Wave 2:
per-band lookahead, detector EQ, crossover slope and GR history", section 6), measuring what
tests/MultiCompW2bTests.cpp measures, with MultiCompDsp's own detector:

  mean square, one pole at RMS size (5 ms); level = 10 log10 (mean square); reduction from the
  hard-knee static curve; follower: attack and release one-poles, 1 - exp (-1 / samples); the
  band's audio times 10 ^ (-reduction / 20).

Test 3 (detector EQ). Two bands split at 150 Hz (LR 24), the model's kick (50 Hz) and bass
(100 Hz) in band 1: 4:1, threshold -24 dB, knee 0, attack 5 ms, release 120 ms, Auto release off.
The reduction during kicks and during bass notes is measured as the test does: each window's
level in a render at 1:1 minus its level in the compressed render, after the same crossover.

The tilt: the first-order design MultiCompDsp::designTilt uses, checked for 0 dB at the band's
centre and the asked-for slope there, and its gain 2 octaves either side, which test 3b uses.

Tests 2, 4 and 5 compare renders with each other (equal to 1e-5, or flat to 0.01 dB, which an LR
cascade with each crossover's own all-pass is by construction; see multicomp-w2-model.py, item
25), so they need no numbers from here.

Run: py -3 docs/research/multicomp-w2b-prepush.py
"""
import math

import numpy as np

FS = 48000.0


def svf_section(x, hz, q, kind):
    """fancy::dsp::Svf (Simper's TPT form), one second-order section."""
    g = math.tan(math.pi * hz / FS)
    k = 1.0 / q
    a1 = 1.0 / (1.0 + g * (g + k))
    a2 = g * a1
    a3 = g * a2
    ic1 = ic2 = 0.0
    out = np.empty(len(x))
    for i, v0 in enumerate(x):
        v3 = v0 - ic2
        v1 = a1 * ic1 + a2 * v3
        v2 = ic2 + a2 * ic1 + a3 * v3
        ic1 = 2.0 * v1 - ic1
        ic2 = 2.0 * v2 - ic2
        out[i] = v2 if kind == "low" else v0 - k * v1 - v2
    return out


def lr24(x, hz, kind):
    """Two Butterworth Q 0.707 sections, as MinPhaseCrossover's LR 24."""
    return svf_section(svf_section(x, hz, 0.70710678, kind), hz, 0.70710678, kind)


def butter24(x, hz, kind):
    """The detector EQ: Butterworth sections at Q 1.3066 and 0.5412."""
    return svf_section(svf_section(x, hz, 1.3065629648763766, kind), hz, 0.54119610014619698, kind)


def compress(audio, detector, threshold, ratio, attack_ms, release_ms, rms_ms=5.0):
    rms = 1.0 - math.exp(-1.0 / (rms_ms * 0.001 * FS))
    att = 1.0 - math.exp(-1.0 / (attack_ms * 0.001 * FS))
    rel = 1.0 - math.exp(-1.0 / (release_ms * 0.001 * FS))
    slope = 1.0 - 1.0 / ratio
    ms = env = 0.0
    out = np.empty(len(audio))
    for i in range(len(audio)):
        d = detector[i]
        ms += rms * (d * d - ms)
        level = 10.0 * math.log10(max(ms, 1e-30))
        target = max(0.0, level - threshold) * slope
        env += (att if target > env else rel) * (target - env)
        out[i] = audio[i] * 10.0 ** (-env / 20.0)
    return out


def block_db(x, mask):
    return 10.0 * math.log10(max(np.mean(x[mask] ** 2), 1e-30))


# Test 3 ----------------------------------------------------------------------------------------
n = int(2 * FS)
t = np.arange(n) / FS
kick = np.zeros(n)
for k in range(4):
    s = int(k * 0.5 * FS)
    m = np.arange(int(0.3 * FS))
    kick[s:s + len(m)] += 0.8 * np.sin(2 * np.pi * 50 * m / FS) * np.exp(-m / (0.08 * FS))
bassline = 0.25 * np.sin(2 * np.pi * 100 * t) * ((t % 0.5) >= 0.25)
signal = kick + bassline

band = lr24(signal, 150.0, "low")
upper = lr24(signal, 150.0, "high")  # band 2, at 1:1: the test measures the summed output
kicks = (t % 0.5) < 0.15
basses = ((t % 0.5) >= 0.3) & ((t % 0.5) < 0.45)
# The first beat warms the detector up, so the test measures beats 2 to 4.
kicks &= t >= 0.5
basses &= t >= 0.5

print("Test 3: detector EQ (band 1, 20-150 Hz, 4:1, -24 dB, attack 5 ms, release 120 ms)")
results = {}
for name, detector in (("whole band", band), ("HP 80 Hz", butter24(band, 80.0, "high"))):
    out = compress(band, detector, -24.0, 4.0, 5.0, 120.0) + upper
    reference = band + upper
    k_red = block_db(reference, kicks) - block_db(out, kicks)
    b_red = block_db(reference, basses) - block_db(out, basses)
    results[name] = (k_red, b_red)
    print(f"    detector {name:<11} reduction during kicks {k_red:5.2f} dB, during bass notes {b_red:5.2f} dB")

whole, hp = results["whole band"], results["HP 80 Hz"]
print(f"    kicks with HP / without: {hp[0] / whole[0]:.3f} (test: under 0.5)")
print(f"    bass notes, change: {abs(hp[1] - whole[1]):.2f} dB (test: under 3 dB)")
assert hp[0] / whole[0] < 0.5
assert abs(hp[1] - whole[1]) < 3.0


# The tilt --------------------------------------------------------------------------------------
def tilt_response(tilt, bottom, top, hz):
    octave_db = 6.020599913279624
    size = min(abs(tilt), 6.0)
    root = math.sqrt((octave_db + size) / (octave_db - size))
    fc = min(math.sqrt(bottom * top), 0.45 * FS)
    wc = 2 * math.pi * fc
    k = wc / math.tan(math.pi * fc / FS)
    zero, pole = (wc / root, wc * root) if tilt >= 0 else (wc * root, wc / root)
    level = root if tilt >= 0 else 1.0 / root
    b0 = level * (k + zero) / (k + pole)
    b1 = level * (zero - k) / (k + pole)
    a1 = (pole - k) / (k + pole)
    z = np.exp(-1j * 2 * np.pi * np.asarray(hz) / FS)
    return 20 * np.log10(np.abs((b0 + b1 * z) / (1 + a1 * z)))


print("Tilt: the first-order design, band 150 Hz to 1.5 kHz (centre 474 Hz)")
for tilt in (-6.0, -3.0, 1.5, 3.0, 6.0):
    fc = math.sqrt(150 * 1500)
    at = tilt_response(tilt, 150, 1500, [fc, fc * 2 ** 0.05, fc * 2 ** -0.05, fc / 4, fc * 4])
    slope = (at[1] - at[2]) / 0.1
    print(f"    tilt {tilt:+.1f}: at fc {at[0]:+.3f} dB, slope there {slope:+.2f} dB/oct, "
          f"2 oct below {at[3]:+.2f} dB, 2 oct above {at[4]:+.2f} dB")
    assert abs(at[0]) < 0.05 and abs(slope - tilt) < 0.1

# Test 3b: tilt +6 on a band 150 Hz to 1.5 kHz; tones 2 octaves either side of the centre are
# heard by the detector at these gains, so with a tone 2 octaves up (1.9 kHz, outside the band,
# which is why the test uses the 1-band case: band 1 covers everything) the reduction rises by
# about the gain there times the slope 0.75 (4:1, well over the threshold).
one_band = tilt_response(6.0, 20.0, 20000.0, [632.0 * 4, 632.0 / 4])
print(f"Test 3b: one band (centre 632 Hz), tilt +6: detector gain 2 oct up {one_band[0]:+.2f} dB, 2 oct down {one_band[1]:+.2f} dB; "
      f"at 4:1 over the threshold the reduction changes by {0.75 * one_band[0]:+.2f} and {0.75 * one_band[1]:+.2f} dB (test: over +5 and under -5)")
assert 0.75 * one_band[0] > 5.0 and 0.75 * one_band[1] < -5.0
print("all pre-push checks pass")
