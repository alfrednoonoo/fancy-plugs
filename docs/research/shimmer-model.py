"""Model for the shimmer in Fancy Verb and Fancy Room (docs/verb.md and docs/room.md, Phase 4
Wave 3: Verb backlog item 10 of section 5, Room item 8 of section 14).

Shimmer feeds the reverb's own output back into its input through a pitch shifter, so each pass
through the reverb comes back an interval higher (or lower) and the tail grows a choir of
octaves. Both plugins use the shared fancy::dsp::PitchShifter (docs/delay.md, Wave 3) in its
plain mode: on a dense tail the heads are uncorrelated, so alignment buys nothing.

The loop:  wet = R(input + f),  f = g x Shift(HP(wet)),  where R is the reverb (Verb: the
convolver's IR; Room: its late network, taken here as its impulse response, which is exact for a
linear network), HP a 200 Hz high-pass so falling shifts don't pile up mud, and
g = Shimmer x 0.9 / sqrt(E), E the energy the reverb passes (the IR's sum of squares; for Room,
the network's per-line energy, as its Auto gain already computes). Shimmer 100 % passes 0.81 of
the energy round the loop, so the tail always dies away: never infinite, never a runaway.

Computed as a sum of passes (wet_0 = R x input, wet_n = R(g Shift(HP(wet_(n-1))))), which equals
the loop for a linear reverb. Measured for a 220 Hz note (0.4 s) at Shimmer 0, 50, 75 and 100 %,
+12 and +7 st: how long the tail takes to fall 60 dB after the note, the level at 2 s and 5 s,
and which intervals sound at 2 s and 5 s. Then the loop gain at 100 % with a bright IR (no
damping), and how many passes the 0.9 keeps the loop under 1 at any frequency band.

Run: python docs/research/shimmer-model.py [room]   ("room" renders Room's late response with
docs/research/room-model.py first, about a minute)
"""
import math
import sys

import numpy as np

RATE = 48000
rng = np.random.default_rng(5)


def db(v):
    return 10 * math.log10(max(v, 1e-30))


def band_decay_ir(seconds, rts, edges, seed):
    r = np.random.default_rng(seed)
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    out = np.zeros(n)
    spec = np.fft.rfft(r.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / RATE)
    lo = 0.0
    for rt, hi in zip(rts, edges + [RATE]):
        out += np.fft.irfft(spec * ((f >= lo) & (f < hi)), n) * np.exp(-6.91 * t / rt)
        lo = hi
    return out


def read(buf, pos):
    pos = np.clip(pos, 0, len(buf) - 2)
    i = np.floor(pos).astype(int)
    fr = pos - i
    return buf[i] + fr * (buf[i + 1] - buf[i])


def lowpass(x, hz):
    spec = np.fft.rfft(x)
    spec[np.fft.rfftfreq(len(x), 1 / RATE) > hz] = 0
    return np.fft.irfft(spec, len(x))


def shift(x, semitones, window_ms=40.0, prefilter=True):
    """The plain two-head shifter. A rising shift reads faster than the rate, so anything above
    0.45 x rate / ratio would pass Nyquist and fold back down: prefilter takes it out first.
    Without it, only the output is low-passed at 0.45 x the rate, which can't remove the fold."""
    r = 2 ** (semitones / 12)
    if prefilter and r > 1:
        x = lowpass(x, 0.45 * RATE / r)
    w = window_ms * 0.001 * RATE
    n = np.arange(len(x))
    pad = int(w + 60)
    buf = np.concatenate([np.zeros(pad), x, [0, 0]])
    out = np.zeros(len(x))
    p1 = np.mod(n * (1 - r) / w, 1.0)
    for p in (p1, np.mod(p1 + 0.5, 1.0)):
        out += (0.5 - 0.5 * np.cos(2 * np.pi * p)) * read(buf, n + pad - 48 - w * p)
    return lowpass(out, 0.45 * RATE)


def highpass(x, hz=200.0):
    spec = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / RATE)
    spec /= np.sqrt(1 + (hz / np.maximum(f, 1e-3)) ** 4)       # 12 dB/oct magnitude
    return np.fft.irfft(spec, len(x))


def convolve(x, h, n):
    size = 1 << int(math.ceil(math.log2(len(x) + len(h))))
    return np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(h, size), size)[:n]


def peak_rms(x, ms=50):
    frame = int(ms * 0.001 * RATE)
    k = len(x) // frame
    return math.sqrt(np.max(np.mean(x[:k * frame].reshape(k, frame) ** 2, axis=1)))


def run(ir, amount, semitones, seconds=14.0, prefilter=True, guard=False, hz=220.0):
    n = int(seconds * RATE)
    t = np.arange(int(0.4 * RATE)) / RATE
    note = np.zeros(n)
    note[:len(t)] = np.sin(2 * np.pi * hz * t) * np.minimum(1, t / 0.01) * np.minimum(1, (0.4 - t) / 0.02)
    g = amount * 0.9 / math.sqrt(np.sum(ir ** 2))
    wet = convolve(note, ir, n)
    total = wet.copy()
    passes = 0
    while amount > 0 and passes < 80:
        feed = g * shift(highpass(wet), semitones, prefilter=prefilter)
        if guard:
            # The loop guard, per pass here (a limiter in time in the plugin): what's fed back is
            # never louder (50 ms RMS) than the plugin's own input was at its loudest.
            peak_in, peak_feed = peak_rms(note), peak_rms(feed)
            feed *= min(1.0, peak_in / max(peak_feed, 1e-30))
        wet = convolve(feed, ir, n)
        total += wet
        passes += 1
        if np.max(np.abs(wet)) < 1e-6 * np.max(np.abs(total)):
            break
    return total, passes


def intervals(seg, semitones):
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / RATE)
    out = []
    for k in range(0, 7):
        hz = 220 * 2 ** (k * semitones / 12)
        if hz > 20000 or hz < 30:
            break
        sel = np.abs(f - hz) < hz * 0.03
        out.append((hz, db(np.sum(spec[sel]))))
    top = max(v for _, v in out)
    return " ".join(f"{hz:.0f}:{v - top:+.0f}" for hz, v in out if v - top > -40)


def report(name, ir, prefilter=True, guard=False, amounts=(0.0, 0.5, 0.75, 1.0), steps=(12, 7)):
    print(f"{name}: the 220 Hz note's tail (time to -60 dB after the note; levels against Shimmer 0 at 0.5 s)")
    ref = None
    for semitones in steps:
        for amount in amounts:
            total, passes = run(ir, amount, semitones, prefilter=prefilter, guard=guard)
            frame = int(0.05 * RATE)
            k = len(total) // frame
            env = 10 * np.log10(np.maximum(np.mean(total[:k * frame].reshape(k, frame) ** 2, axis=1), 1e-30))
            if ref is None:
                ref = env[10]
            after = env[8:]
            below = np.where(after < np.max(after) - 60)[0]
            t60 = (8 + below[0]) * 0.05 - 0.4 if len(below) else float("inf")
            l2, l5 = env[40] - ref, env[100] - ref
            iv2 = intervals(total[int(2 * RATE):int(2.5 * RATE)], semitones)
            iv5 = intervals(total[int(5 * RATE):int(5.5 * RATE)], semitones) if l5 > -100 else "silent"
            print(f"   {semitones:+d} st, Shimmer {int(amount * 100):3d} %: -60 dB at {t60:5.1f} s, level at 2 s {l2:+6.1f} dB, at 5 s {l5:+6.1f} dB,"
                  f" {passes:2d} passes; at 2 s (Hz:dB) {iv2}; at 5 s {iv5}")


# Verb: a generated hall, highs shorter (Decay 2.5 s) --------------------------------------------
hall = band_decay_ir(3.5, [3.0, 2.5, 1.5], [300.0, 3000.0], 1)
report("Verb, generated hall", hall)
print("The same, +12 st only, without the shifter's input filter (output low-passed only):")
total, passes = run(hall, 1.0, 12, prefilter=False)
frame = int(0.05 * RATE)
k = len(total) // frame
env = 10 * np.log10(np.maximum(np.mean(total[:k * frame].reshape(k, frame) ** 2, axis=1), 1e-30))
print(f"   Shimmer 100 %: level at 5 s {env[100] - env[10]:+.1f} dB against 0.5 s, at 10 s {env[200] - env[10]:+.1f} dB: it runs away, folded partials feeding themselves")

# The loop's worst band: energy passed per pass, per octave, at Shimmer 100 % --------------------
print("Loop gain per pass at Shimmer 100 % (+12 st), per octave of the IR's response, bright IR (no damping):")
bright = band_decay_ir(3.5, [2.5, 2.5, 2.5], [300.0, 3000.0], 3)
g = 0.9 / math.sqrt(np.sum(bright ** 2))
H = np.abs(np.fft.rfft(bright, 1 << 18)) ** 2
f = np.fft.rfftfreq(1 << 18, 1 / RATE)
worst = -100.0
for lo in 125 * 2 ** np.arange(0, 7):
    sel = (f >= lo) & (f < 2 * lo)
    gain = db(g * g * np.mean(H[sel]))
    worst = max(worst, gain)
    print(f"   {lo:5.0f} to {2 * lo:5.0f} Hz: {gain:+5.2f} dB a pass")
print(f"   worst octave {worst:+.2f} dB: under 0 dB, so even a bright IR at 100 % dies away")

if len(sys.argv) > 1 and sys.argv[1] == "room":
    sys.path.insert(0, "docs/research")
    import importlib
    room = importlib.import_module("room-model")
    n = int(3.5 * RATE)
    left, _ = room.render(room.impulse(), room.impulse(), n, predelay_ms=0.0, er_db=-60.0)
    room_ir = np.array(left)
    report("Room, defaults (late network only, Decay 1.8 s)", room_ir)
    report("Room, with the loop guard", room_ir, guard=True, amounts=(0.0, 0.75, 1.0))
    print("Room, +7 st, the loudest 50 ms after the note against the loudest 50 ms of Shimmer 0, six notes:")
    for guard in (False, True):
        rises = []
        for hz in (196.0, 220.0, 246.9, 261.6, 329.6, 392.0):
            base, _ = run(room_ir, 0.0, 7, seconds=8.0, hz=hz)
            total, _ = run(room_ir, 1.0, 7, seconds=8.0, guard=guard, hz=hz)
            rises.append(20 * math.log10(peak_rms(total) / peak_rms(base)))
        print(f"   Shimmer 100 %, guard {'on ' if guard else 'off'}: " + ", ".join(f"{r:+.1f}" for r in rises) + " dB")
report("Verb, with the loop guard", hall, guard=True, amounts=(0.0, 1.0))
