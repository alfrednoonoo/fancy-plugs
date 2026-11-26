"""Pre-push check for Fancy Room's Shimmer (docs/room.md, Phase 4 Wave 3), a port of what's built:
room-model.py's render with the loop added where RoomDsp adds it.

Per sample, the late network's output (lateL, lateR, after the output gain) goes through its own
copy of Low cut and High cut, a 200 Hz high-pass (12 dB/oct) and the shared pitch shifter (plain
mode), is scaled by g = Shimmer x 0.9 / sqrt(E) and by the guard, and is added to the next
sample's diffuser input. E is the energy one late channel passes, from Auto gain's per-line
energy (loop_energy; `energy` checks it against the network's impulse response). The guard holds the
feedback's 50 ms RMS under the peak 50 ms RMS of Room's input, held for Decay and released 60 dB
per Decay, through a gain with a 10 ms attack and a 100 ms release.

    python docs/research/room-w3-prepush.py [energy test2 test3 test4 test5]
"""
import importlib.util
import math
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, HERE / file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


rm = load("room_model", "room-model.py")
port = load("delay_w3_prepush", "delay-w3-prepush.py")
vp = load("verb_w3_prepush", "verb-w3-prepush.py")

FS = rm.FS
SEMITONES = {"+12": 12.0, "+7": 7.0, "+5": 5.0, "+19": 19.0, "+24": 24.0, "-12": -12.0}


def line_energy(size, decay, n_lines):
    """A: Auto gain's 1 / (1 - g^2), averaged over the network's lines."""
    lines_ms = rm.LINE_MS if n_lines == 16 else rm.LINE_MS[::2]
    scale = rm.size_scale(size) * 0.001
    return sum(1.0 / (1.0 - 10 ** (-6.0 * ms * scale / decay)) for ms in lines_ms) / n_lines


def loop_energy(size, decay, n_lines):
    """The energy one late channel passes, from A' = A - 1 (each line's g^2 / (1 - g^2): Auto gain's
    sum counts a direct pass the network doesn't have), times a fit for the lines' correlation,
    which grows with the decay. The fit is never under the measured impulse responses (Damping 0,
    Mod 0, Normal and Eco, Size 0 to 100 %, Decay 0.3 to 20 s), and at most 16 % over, so Shimmer
    100 % passes 0.70 to 0.81 of the energy round the loop."""
    a = max(1.0e-6, line_energy(size, decay, n_lines) - 1.0)
    return 2.0 * a / n_lines * (1.14 + 0.44 * a / (a + 2.25))


class BiquadState:
    """Direct form I on rm.biquad's coefficients (b0, b1, b2, a1, a2)."""

    def __init__(self, coef):
        self.c = coef
        self.x1 = self.x2 = self.y1 = self.y2 = 0.0

    def tick(self, x):
        b0, b1, b2, a1, a2 = self.c
        y = b0 * x + b1 * self.x1 + b2 * self.x2 - a1 * self.y1 - a2 * self.y2
        self.x2, self.x1, self.y2, self.y1 = self.x1, x, self.y1, y
        return y


class Loop:
    def __init__(self, p, shimmer, semitones, n_lines):
        self.g = shimmer * 0.9 / math.sqrt(loop_energy(p["size"], p["decay"], n_lines))
        self.ratio = 2.0 ** (semitones / 12.0)
        self.cuts = [(BiquadState(rm.biquad("hp", p["lowcut"])), BiquadState(rm.biquad("lp", p["highcut"]))) for _ in (0, 1)]
        self.hp = [vp.SvfHigh(), vp.SvfHigh()]
        self.hpc = vp.svf_coeffs(200.0, 0.70710678118654752)
        self.shifters = [port.PitchShifter(FS, aligned=False), port.PitchShifter(FS, aligned=False)]
        self.rms = 1 - math.exp(-1 / (0.05 * FS))
        self.att = 1 - math.exp(-1 / (0.01 * FS))
        self.rel = 1 - math.exp(-1 / (0.1 * FS))
        rt = max(0.05, p["decay"]) * FS
        self.hold = int(min(rt, 60 * FS))
        self.let_go = 10 ** (-6.0 / rt)
        self.in_env = self.fb_env = self.peak = 0.0
        self.hold_left = 0
        self.guard = 1.0
        self.pending = [0.0, 0.0]

    def follow_input(self, xl, xr):
        self.in_env += (0.5 * (xl * xl + xr * xr) - self.in_env) * self.rms
        if self.in_env >= self.peak:
            self.peak, self.hold_left = self.in_env, self.hold
        elif self.hold_left > 0:
            self.hold_left -= 1
        else:
            self.peak *= self.let_go

    def feed(self, late):
        out = []
        for ch in (0, 1):
            lo, hi = self.cuts[ch]
            x = hi.tick(lo.tick(late[ch]))
            x = self.hp[ch].high(self.hpc, x)
            out.append(self.g * self.shifters[ch].process(x, self.ratio))
        self.fb_env += (0.5 * (out[0] ** 2 + out[1] ** 2) - self.fb_env) * self.rms
        want = math.sqrt(self.peak / self.fb_env) if self.fb_env > self.peak and self.fb_env > 1e-30 else 1.0
        self.guard += (want - self.guard) * (self.att if want < self.guard else self.rel)
        self.pending = [out[0] * self.guard, out[1] * self.guard]


def render(inL, inR, n_total, shimmer=0.0, semitones=12.0, filters=True, **kw):
    """rm.render with the loop. Wet only; late network plus reflections."""
    p = dict(rm.DEFAULTS, **kw)
    n_lines, modulated, cubic = rm.QUALITY[p["quality"]]
    lines_ms = rm.LINE_MS if n_lines == 16 else rm.LINE_MS[::2]
    scale = rm.size_scale(p["size"])
    T, D = p["decay"], p["damping"]

    lengths = [round(ms * scale * FS / 1000.0) for ms in lines_ms]
    depth = p["mod"] * rm.MOD_MAX_MS * FS / 1000.0
    is_mod = [i in modulated for i in range(n_lines)]
    rate = [2 * math.pi * (0.31 + 0.97 * rm.frac(i * 0.618034 + 0.17)) / FS for i in range(n_lines)]
    phase = [2 * math.pi * rm.frac(i * 0.4142 + 0.29) for i in range(n_lines)]
    g = [10 ** (-3.0 * d / (FS * T)) for d in lengths]
    ghf = [10 ** (-3.0 * d / (FS * T * (1.0 - 0.7 * D))) for d in lengths]
    bco = [(1 - gh / gg) / (1 + gh / gg) for gg, gh in zip(g, ghf)]
    sign = [1.0 if (i // 2) % 2 == 0 else -1.0 for i in range(n_lines)]
    in_gain = out_gain = 1.0 / math.sqrt(n_lines / 2)
    hscale = 1.0 / math.sqrt(n_lines)

    size_pow2 = 1 << (max(lengths) + int(depth) + 8).bit_length()
    mask = size_pow2 - 1
    bufs = [[0.0] * size_pow2 for _ in range(n_lines)]
    lp_state = [0.0] * n_lines

    pre = round(p["predelay_ms"] * FS / 1000.0)
    er = [rm.er_table(0, scale), rm.er_table(1, scale)]
    er_gain = 10 ** (p["er_db"] / 20.0)
    hist_len = 1 << (max(d for t in er for d, _ in t) + pre + 2).bit_length()
    hmask = hist_len - 1
    histL = [0.0] * hist_len
    histR = [0.0] * hist_len

    ap_len = [[round(ms * scale * FS / 1000.0) for ms in rm.DIFF_MS[ch]] for ch in (0, 1)]
    ap_buf = [[[0.0] * (d + 1) for d in ap_len[ch]] for ch in (0, 1)]
    ap_pos = [[0] * 4 for _ in (0, 1)]
    apg = 0.75 * p["diffusion"]

    loop = Loop(p, shimmer, semitones, n_lines) if shimmer > 0 else None
    freeze_at = p["freeze_at"]
    outL, outR = [], []
    w = 0
    nL, nR = len(inL), len(inR)
    v = [0.0] * n_lines
    m0 = [0.0] * n_lines
    m1 = [0.0] * n_lines
    k0 = 0
    for n in range(n_total):
        xL = inL[n] if n < nL else 0.0
        xR = inR[n] if n < nR else 0.0
        hw = n & hmask
        histL[hw] = xL
        histR[hw] = xR
        if loop:
            loop.follow_input(xL, xR)

        eL = 0.0
        for d, gk in er[0]:
            i = (n - pre - d) & hmask
            eL += gk * (0.75 * histL[i] + 0.25 * histR[i])
        eR = 0.0
        for d, gk in er[1]:
            i = (n - pre - d) & hmask
            eR += gk * (0.25 * histL[i] + 0.75 * histR[i])

        # The diffusers' input: the pre-delayed input, plus the loop's feedback from the last sample.
        i0 = (n - pre) & hmask
        dl = [histL[i0], histR[i0]]
        if loop:
            dl[0] += loop.pending[0]
            dl[1] += loop.pending[1]
        diffused = []
        for ch in (0, 1):
            s = dl[ch]
            for k in range(4):
                buf = ap_buf[ch][k]
                pos = ap_pos[ch][k]
                delayed = buf[pos]
                y = -apg * s + delayed
                buf[pos] = s + apg * y
                ap_pos[ch][k] = pos + 1 if pos + 1 < len(buf) - 1 else 0
                s = y
            diffused.append(s)

        f = 0.0
        if freeze_at is not None and n >= freeze_at:
            f = min(1.0, (n - freeze_at) / rm.FREEZE_RAMP)

        if n % rm.MOD_STEP == 0:
            k0 = n
            for i in range(n_lines):
                m0[i] = math.sin(phase[i] + rate[i] * n)
                m1[i] = math.sin(phase[i] + rate[i] * (n + rm.MOD_STEP))
        t = (n - k0) / rm.MOD_STEP

        lateL = lateR = 0.0
        for i in range(n_lines):
            if is_mod[i]:
                dmod = lengths[i] + depth * (m0[i] + t * (m1[i] - m0[i]))
                dd = (1.0 - f) * dmod + f * lengths[i]
                ip = math.floor(dd)
                fr = dd - ip
                b = bufs[i]
                a0 = b[(w - ip) & mask]
                rd = a0 + fr * (b[(w - ip - 1) & mask] - a0)
            else:
                rd = bufs[i][(w - lengths[i]) & mask]
            gi = g[i] + f * (1.0 - g[i])
            bi = bco[i] * (1.0 - f)
            y = gi * (1.0 - bi) * rd + bi * lp_state[i]
            lp_state[i] = y
            v[i] = y
            if i & 1:
                lateR += sign[i] * y
            else:
                lateL += sign[i] * y

        h = 1
        while h < n_lines:
            for s0 in range(0, n_lines, 2 * h):
                for j in range(s0, s0 + h):
                    a, bb = v[j], v[j + h]
                    v[j] = a + bb
                    v[j + h] = a - bb
            h *= 2
        gin = in_gain * (1.0 - f)
        for i in range(n_lines):
            bufs[i][w] = v[i] * hscale + gin * sign[i] * diffused[i & 1]
        w = (w + 1) & mask

        lateL *= out_gain
        lateR *= out_gain
        if loop:
            loop.feed((lateL, lateR))
        outL.append(lateL + er_gain * eL)
        outR.append(lateR + er_gain * eR)

    if not filters:
        return np.array(outL), np.array(outR)
    hp, lp = rm.biquad("hp", p["lowcut"]), rm.biquad("lp", p["highcut"])
    outL = rm.filt(lp, rm.filt(hp, outL))
    outR = rm.filt(lp, rm.filt(hp, outR))
    return np.array(outL), np.array(outR)


def note(hz, seconds=0.4, amp=0.5):
    n = int(seconds * FS)
    fade = int(0.005 * FS)
    x = amp * np.sin(2 * np.pi * hz * np.arange(n) / FS)
    x[:fade] *= np.linspace(0, 1, fade)
    x[-fade:] *= np.linspace(1, 0, fade)
    return list(x)


def env_db(x, ms=50):
    w = int(ms * 0.001 * FS)
    k = len(x) // w
    p = (np.asarray(x[:k * w]) ** 2).reshape(k, w).mean(axis=1)
    return 10 * np.log10(p + 1e-30)


def band_db(x, hz, width=0.25):
    """Power in the band hz x 2^(+-width/2 oct), in dB, from one FFT of the segment."""
    spectrum = np.abs(np.fft.rfft(np.asarray(x) * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / FS)
    lo, hi = hz * 2 ** (-width / 2), hz * 2 ** (width / 2)
    return 10 * math.log10(spectrum[(f >= lo) & (f < hi)].sum() + 1e-30)


def energy():
    """E = 2 A / n against the late network's impulse response at Damping 0, without cuts or reflections."""
    for quality, n_lines in (("normal", 16), ("eco", 8)):
        for size, decay in ((0.55, 1.8), (0.2, 0.6), (1.0, 6.0), (0.0, 20.0)):
            n = int((1.5 * decay + 0.5) * FS)
            L, R = render(rm.impulse(), rm.impulse(), n, filters=False, predelay_ms=0.0, er_db=-200.0, damping=0.0,
                          size=size, decay=decay, quality=quality, mod=0.0)
            measured = max(float(np.sum(L ** 2)), float(np.sum(R ** 2)))
            formula = loop_energy(size, decay, n_lines)
            print(f"energy {quality:6} size {size:.2f} decay {decay:.1f}: IR {measured:.4f}, E {formula:.4f}, "
                  f"Shimmer 100 % passes {0.81 * measured / formula:.3f}")


def test2():
    x = note(220.0)
    seconds = 9
    L, R = render(x, x, seconds * FS, shimmer=1.0, semitones=12.0)
    at = int(2.4 * FS)
    seg = L[at:at + int(0.5 * FS)]
    bands = {hz: band_db(seg, hz) for hz in (220, 440, 880, 1760, 3520, 7040, 14080)}
    strongest = max(bands.values())
    print("test 2: 2 s after the note, against the strongest: " + ", ".join(f"{hz} {b - strongest:+.1f}" for hz, b in bands.items()))
    e = env_db(np.concatenate([L, R]) if False else (L ** 2 + R ** 2) ** 0.5)
    peak = e.max()
    below = np.nonzero(e < peak - 60)[0]
    after = [i for i in below if i * 0.05 > 0.4 and all(e[j] < peak - 60 for j in range(i, len(e)))]
    t60 = after[0] * 0.05 - 0.4 if after else float("inf")
    print(f"test 2: falls 60 dB {t60:.2f} s after the note ends (limit 6); finite {np.all(np.isfinite(L))}")


def test3():
    rises = []
    for hz in (196.0, 220.0, 246.9, 261.6, 329.6, 392.0):
        x = note(hz)
        b = render(x, x, 5 * FS, shimmer=0.0)
        s = render(x, x, 5 * FS, shimmer=1.0, semitones=7.0)
        base = env_db(b[0] ** 2 + b[1] ** 2, 50)
        sh = env_db(s[0] ** 2 + s[1] ** 2, 50)
        start = int(0.4 / 0.05)
        rises.append(0.5 * (sh[start:].max() - base[start:].max()))
    print("test 3: +7 st at 100 %, loudest 50 ms after the note against Shimmer 0: "
          + ", ".join(f"{r:+.1f}" for r in rises) + " dB (limit +10)")


def third_octave_levels(x, nfft=4096):
    """Welch: Hann segments of nfft, half overlapped, power averaged, then summed per third octave."""
    x = np.asarray(x)
    win = np.hanning(nfft)
    segments = [x[i:i + nfft] * win for i in range(0, len(x) - nfft + 1, nfft // 2)]
    spectrum = np.mean([np.abs(np.fft.rfft(seg)) ** 2 for seg in segments], axis=0)
    f = np.fft.rfftfreq(nfft, 1 / FS)
    out = []
    for k in range(-17, 13):
        c = 1000 * 2 ** (k / 3)
        sel = (f >= c * 2 ** (-1 / 6)) & (f < c * 2 ** (1 / 6))
        out.append(10 * math.log10(spectrum[sel].sum() + 1e-30))
    return np.array(out)


def test4():
    """Frozen with Shimmer 100 %: the 1 s levels hold, and no third octave of 125 Hz to 8 kHz (those within
    30 dB of the strongest) climbs from seconds 3-5 to seconds 11-13, Welch-averaged. Shimmer 0 alongside,
    for how much a frozen tail's spectrum estimate moves on its own."""
    rng = np.random.default_rng(3)
    x = list(0.25 * rng.standard_normal(2 * FS))
    y = list(0.25 * rng.standard_normal(2 * FS))
    seconds = 13
    centres = np.array([1000 * 2 ** (k / 3) for k in range(-17, 13)])
    for shimmer in (0.0, 1.0):
        L, R = render(x, y, seconds * FS, shimmer=shimmer, semitones=12.0, freeze_at=int(2.5 * FS))
        first = third_octave_levels(L[3 * FS:5 * FS])
        last = third_octave_levels(L[11 * FS:13 * FS])
        use = (first > first.max() - 30) & (centres >= 125) & (centres <= 8000)
        climb = (last - first)[use]
        levels = [10 * math.log10(np.mean(L[s * FS:(s + 1) * FS] ** 2 + R[s * FS:(s + 1) * FS] ** 2) + 1e-30) for s in range(3, 13)]
        print(f"test 4: frozen, Shimmer {shimmer:.0%}: 1 s levels span {max(levels) - min(levels):.2f} dB; "
              f"third octaves move {climb.min():+.2f} to {climb.max():+.2f} dB ({use.sum()} bands; limit 1)")


def test5():
    x = list(0.5 * np.sin(2 * np.pi * 6000 * np.arange(int(0.5 * FS)) / FS))
    L, R = render(x, x, 4 * FS, shimmer=1.0, semitones=24.0)
    seg = L[int(2.5 * FS):int(3.5 * FS)]
    spectrum = np.abs(np.fft.rfft(seg * np.hanning(len(seg))))
    f = np.fft.rfftfreq(len(seg), 1 / FS)
    tone = 0.5 * np.sin(2 * np.pi * 6000 * np.arange(len(seg)) / FS)
    ref = np.abs(np.fft.rfft(tone * np.hanning(len(seg)))).max()
    below = 20 * math.log10(spectrum[f < 5000].max() / ref + 1e-30)
    print(f"test 5: Two octaves up on 6 kHz, 2 s after: strongest below 5 kHz {below:.1f} dB against the tone (limit -80)")


if __name__ == "__main__":
    wanted = sys.argv[1:] or ["energy", "test2", "test3", "test4", "test5"]
    for name in wanted:
        globals()[name]()
