"""Model of Fancy FIR's Resonance mode (Phase 3 Q2.7), to check the design before any C++.

Mirrors the engine: 4096-point STFT, hop 1024, square-root Hann windows, 48 kHz, and the
Reduce artifacts smoothing. Checks the brief's two tests:
  1. a 3 kHz resonance 12 dB above a noise bed is reduced by the set Depth;
  2. broadband material (white and pink noise, no resonance) changes by under 0.5 dB.
Run with: py -3.14 docs/research/fir-resonance-model.py
"""

import numpy as np

RATE = 48000
N = 4096
HOP = N // 4
BINS = N // 2 + 1
SECONDS = 8
SKIP = RATE  # the first second is ignored while the detector settles

window = np.sqrt(np.hanning(N + 1)[:N])  # periodic square-root Hann
freqs = np.arange(BINS) * RATE / N

# Fixed design constants (docs/fir.md, Phase 3 Q2.7, section 2).
TOLERANCE_DB = 4.0  # a bin must stand this far above the envelope before it's turned down
DETECT_MS = 80.0  # the detector's own smoothing, separate from Attack and Release
CORE_BINS = 2  # ±2 bins round each bin are left out of its envelope (a sine's main lobe)
PASSES = 2  # extra envelope passes with levels clipped at envelope + tolerance
WIDEST_OCT, NARROWEST_OCT = 2.0, 1 / 3  # envelope width at Sharpness 0 and 100 %


def noise(kind, seed, seconds=SECONDS):
    """White or pink noise, band-limited to 20 Hz to 20 kHz."""
    rng = np.random.default_rng(seed)
    spectrum = np.fft.rfft(rng.standard_normal(int(RATE * seconds)))
    f = np.fft.rfftfreq(int(RATE * seconds), 1 / RATE)
    shape = np.where((f >= 20) & (f <= 20000), 1.0, 0.0)
    if kind == "pink":
        shape = shape / np.sqrt(np.maximum(f, 20) / 1000.0)
    x = np.fft.irfft(spectrum * shape, int(RATE * seconds))
    return x * 0.1 / np.sqrt(np.mean(x ** 2))


def peaking(x, f0, gain_db, q):
    """RBJ peaking EQ: the resonance."""
    a = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f0 / RATE
    alpha = np.sin(w0) / (2 * q)
    b = np.array([1 + alpha * a, -2 * np.cos(w0), 1 - alpha * a])
    den = np.array([1 + alpha / a, -2 * np.cos(w0), 1 - alpha / a])
    b, den = b / den[0], den / den[0]
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        out = b[0] * v + b[1] * x1 + b[2] * x2 - den[1] * y1 - den[2] * y2
        x2, x1, y2, y1 = x1, v, y1, out
        y[i] = out
    return y


def envelope_half_widths(sharpness):
    """Half-width in bins of each bin's envelope window. The window spans WIDEST_OCT octaves
    at Sharpness 0 and NARROWEST_OCT at 1, on a log scale, and never less than ±(core + 4)."""
    width = WIDEST_OCT * (NARROWEST_OCT / WIDEST_OCT) ** sharpness
    half = freqs * (2 ** (width / 2) - 1)
    return np.maximum(CORE_BINS + 4, np.round(half / (RATE / N))).astype(int)


def process(x, depth_db, sharpness=0.5, attack_ms=5.0, release_ms=80.0, low_hz=100.0, high_hz=20000.0):
    """Returns the output, aligned with x (an offline STFT has no delay)."""
    frames = (len(x) - N) // HOP
    out = np.zeros(len(x))
    k = np.arange(BINS)
    half = envelope_half_widths(sharpness)
    lo, hi = np.maximum(0, k - half), np.minimum(BINS - 1, k + half)
    # Each side of the window, without the core: left lo .. k-core-1, right k+core+1 .. hi.
    left_hi, right_lo = k - CORE_BINS - 1, k + CORE_BINS + 1
    left_count = np.maximum(0, left_hi - lo + 1)
    right_count = np.maximum(0, hi - right_lo + 1)

    hop_s = HOP / RATE
    att = 1 - np.exp(-hop_s / (attack_ms / 1000)) if attack_ms > 0 else 1.0
    rel = 1 - np.exp(-hop_s / (release_ms / 1000)) if release_ms > 0 else 1.0
    det = 1 - np.exp(-hop_s / (DETECT_MS / 1000))
    calibration = 4 / np.sum(window) ** 2  # a sine at amplitude A reads 20 log10 A

    # Range: full inside Low to High, fading to nothing over half an octave outside.
    octaves_out = np.maximum(np.log2(low_hz / np.maximum(freqs, 1.0)), np.log2(np.maximum(freqs, 1.0) / high_hz))
    range_weight = np.clip(1.0 - 2.0 * octaves_out, 0.0, 1.0)

    detector = None
    reduction = np.zeros(BINS)

    for f in range(frames):
        start = f * HOP
        spec = np.fft.rfft(x[start:start + N] * window)
        power = np.abs(spec) ** 2 * calibration

        # Detector: power averaged over ±1 bin, then smoothed over DETECT_MS.
        reach = np.convolve(power, [1 / 3, 1 / 3, 1 / 3], mode="same")
        detector = reach if detector is None else detector + (reach - detector) * det
        level = 10 * np.log10(np.maximum(detector, 1e-20))

        # Envelope: the louder of the two sides' mean levels (dB), without the core. A peak is
        # below its envelope on both sides; a step or a band edge has one side as loud as
        # itself, so it's never a peak. Then again with each level clipped at envelope +
        # tolerance, so a peak doesn't lift its own envelope.
        clipped = level
        for _ in range(1 + PASSES):
            cumulative = np.concatenate([[0.0], np.cumsum(clipped)])
            left = (cumulative[np.maximum(left_hi, lo - 1) + 1] - cumulative[lo]) / np.maximum(left_count, 1)
            right = (cumulative[hi + 1] - cumulative[np.minimum(right_lo, hi + 1)]) / np.maximum(right_count, 1)
            left = np.where(left_count > 0, left, -np.inf)
            right = np.where(right_count > 0, right, -np.inf)
            envelope = np.maximum(left, right)
            envelope = np.where(np.isfinite(envelope), envelope, level)
            clipped = np.minimum(level, envelope + TOLERANCE_DB)

        target = np.clip(level - envelope - TOLERANCE_DB, 0.0, depth_db)
        target *= range_weight
        reduction += (target - reduction) * np.where(target > reduction, att, rel)

        gain = 10 ** (-reduction / 20)
        smoothed = np.convolve(gain, [0.25, 0.5, 0.25], mode="same")  # Reduce artifacts
        smoothed[0], smoothed[-1] = gain[0], gain[-1]
        out[start:start + N] += np.fft.irfft(spec * smoothed, N) * window * 0.5  # sqrt-Hann at 75 % sums to 2
    end = frames * HOP
    return out[N:end], x[N:end]  # the first frame's samples aren't fully overlapped


def transfer_db(y, x, f0):
    """The gain at f0 from the averaged cross-spectrum, in dB."""
    seg = 8192
    w = np.hanning(seg)
    sxy = sxx = 0
    for s in range(SKIP, len(y) - seg, seg // 2):
        xs, ys = np.fft.rfft(x[s:s + seg] * w), np.fft.rfft(y[s:s + seg] * w)
        sxy, sxx = sxy + ys * np.conj(xs), sxx + np.abs(xs) ** 2
    f = np.fft.rfftfreq(seg, 1 / RATE)
    kk = np.argmin(np.abs(f - f0))
    return 20 * np.log10(np.abs(sxy[kk]) / sxx[kk])


def octave_change(y, x, fc):
    def band(z):
        s = np.abs(np.fft.rfft(z[SKIP:] * np.hanning(len(z) - SKIP))) ** 2
        f = np.fft.rfftfreq(len(z) - SKIP, 1 / RATE)
        return np.sum(s[(f > fc / np.sqrt(2)) & (f < fc * np.sqrt(2))])
    return 10 * np.log10(band(y) / band(x))


OCTAVES = (31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000)


def main():
    print("1. 3 kHz resonance, 12 dB above the bed (peaking +12 dB, Q 8): gain at 3 kHz and 1.5 kHz")
    for kind in ("white", "pink"):
        resonant = peaking(noise(kind, 1), 3000.0, 12.0, 8.0)
        for depth, sharp in ((3, 0.5), (6, 0.0), (6, 0.5), (6, 1.0), (8, 0.5), (12, 0.5)):
            y, x = process(resonant, float(depth), sharpness=sharp)
            print(f"   {kind:5s} Depth {depth:2d} dB, Sharpness {sharp * 100:3.0f} %: "
                  f"3 kHz {transfer_db(y, x, 3000):+6.2f} dB, 1.5 kHz {transfer_db(y, x, 1500):+5.2f} dB")

    print("2. Broadband, no resonance: change per octave, 31.5 Hz to 16 kHz (limit 0.5 dB)")
    for kind in ("white", "pink"):
        flat = noise(kind, 2)
        y, x = process(flat, 24.0, low_hz=20.0)
        changes = [octave_change(y, x, fc) for fc in OCTAVES]
        print(f"   {kind:5s} Low 20 Hz (not the default), Depth 24: {' '.join(f'{c:+.2f}' for c in changes)}")
        for sharp in (0.0, 0.5, 1.0):
            for depth in (6.0, 24.0):
                y, x = process(flat, depth, sharpness=sharp)
                changes = [octave_change(y, x, fc) for fc in OCTAVES]
                print(f"   {kind:5s} Sharpness {sharp * 100:3.0f} %, Depth {depth:4.1f}: worst {min(changes):+.2f} dB "
                      f"({' '.join(f'{c:+.2f}' for c in changes)})")


if __name__ == "__main__":
    main()
