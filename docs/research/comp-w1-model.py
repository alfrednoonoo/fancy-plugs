"""Model of Fancy Comp's Hold (docs/comp.md, Phase 4 Wave 1, section 7, test 4).

A 50 Hz sine 12 dB over the threshold, peak detection (RMS size 0), 4:1, hard knee, Attack 1 ms,
Release 20 ms, through the shared branching follower (fancy/dsp/Compression.h) with and without
Hold. Prints the THD of the output after it settles, to set the test's margin.

Run: py -3 docs/research/comp-w1-model.py
"""
import math

RATE = 48000.0


def coefficient(ms):
    samples = ms * 0.001 * RATE
    return 1.0 - math.exp(-1.0 / samples) if samples > 0 else 1.0


def render(hold_ms, hz=50.0, seconds=1.0, threshold=-24.0, ratio=4.0):
    attack, release = coefficient(1.0), coefficient(20.0)
    hold = max(1, round(hold_ms * 0.001 * RATE)) if hold_ms > 0 else 0
    amplitude = 10 ** ((threshold + 12.0) / 20.0)
    slope = 1.0 - 1.0 / ratio
    envelope, count, out = 0.0, 0, []
    for i in range(int(seconds * RATE)):
        x = amplitude * math.sin(2 * math.pi * hz * i / RATE)
        level = 20 * math.log10(max(abs(x), 1e-30))
        target = max(0.0, level - threshold) * slope
        held = False
        if hold > 0:
            if target >= envelope:
                count = hold
            elif count > 0:
                count -= 1
                held = True
        if not held:
            if target > envelope:
                envelope += attack * (target - envelope)
            else:
                envelope += release * (target - envelope)
        out.append(x * 10 ** (-envelope / 20.0))
    return out


def thd_db(samples, hz=50.0, harmonics=10):
    # The last 0.5 s, a whole number of cycles, so no window is needed.
    n = int(0.5 * RATE)
    tail = samples[-n:]

    def level(k):
        re = sum(s * math.cos(2 * math.pi * k * hz * i / RATE) for i, s in enumerate(tail))
        im = sum(s * math.sin(2 * math.pi * k * hz * i / RATE) for i, s in enumerate(tail))
        return math.hypot(re, im)

    fundamental = level(1)
    rest = math.sqrt(sum(level(k) ** 2 for k in range(2, harmonics + 1)))
    return 20 * math.log10(rest / fundamental)


if __name__ == "__main__":
    results = {hold: thd_db(render(hold)) for hold in (0.0, 10.0, 30.0)}
    for hold, thd in results.items():
        print(f"Hold {hold:5.1f} ms: THD {thd:7.2f} dB")
    print(f"Hold 30 ms against 0: {results[0.0] - results[30.0]:.1f} dB less distortion")
