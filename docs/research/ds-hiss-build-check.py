"""Build check for Fancy DS's Hiss mode as decided on 24 November 2026 (docs/ds.md, Phase 4 Wave 2,
section 7): the detector is a 15 ms envelope of the shelf band (instant rise), absolute; the cut is
1 dB per dB under Threshold, up to Range; it closes over Release and opens over Attack (DS's
Ballistics with the two times swapped). Measures what tests/DsW2bTests.cpp test 3 measures, on the
pre-push signal, and DS-Mini's Hiss profile (shelf at 5 kHz, Threshold -56 + 4 x Amount, Range
-1.8 x Amount) at Amount 5.

Run: python docs/research/ds-hiss-build-check.py
"""
import importlib.util
import math
import pathlib

import numpy as np

spec = importlib.util.spec_from_file_location("pre", pathlib.Path(__file__).with_name("ds-hiss-prepush.py"))
pre = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pre)
FS = pre.FS


def envelope_db(x, release_ms):
    r = 1 - math.exp(-1 / (release_ms * 0.001 * FS))
    env, out = 0.0, np.empty(len(x))
    for i, v in enumerate(np.abs(x)):
        env = v if v > env else env + (v - env) * r
        out[i] = 20 * math.log10(max(env, 1e-10))
    return out


def follow(want, grow_ms, shrink_ms):
    g = 1 - math.exp(-1 / (grow_ms * 0.001 * FS))
    s = 1 - math.exp(-1 / (shrink_ms * 0.001 * FS))
    env, out = 0.0, np.empty(len(want))
    for i, w in enumerate(want):
        env += (g if w > env else s) * (w - env)
        out[i] = env
    return out


def run(freq, threshold, range_db, attack=0.5, release=60.0):
    highs = pre.svf_high(pre.signal, freq)
    level = envelope_db(highs, 15.0)
    want = np.clip(threshold - level, 0.0, range_db)
    red = follow(want, release, attack)
    out = pre.signal + (10 ** (-red / 20) - 1) * highs
    return (pre.above_8k_db(out, pre.pauses) - pre.above_8k_db(pre.signal, pre.pauses),
            pre.above_8k_db(out, pre.loud) - pre.above_8k_db(pre.signal, pre.loud))


for name, args in (("DS Hiss, 4 kHz, Threshold -38, Range -12 (test 3)", (4000, -38.0, 12.0)),
                   ("DS-Mini Hiss, Amount 5 (5 kHz, -36, -9)", (5000, -36.0, 9.0)),
                   ("DS-Mini Hiss, Amount 8 (5 kHz, -24, -14.4)", (5000, -24.0, 14.4))):
    p, l = run(*args)
    print(f"{name}: pauses {p:6.2f} dB, loud parts {l:6.2f} dB")


# What tests/DsW2bTests.cpp test 3 measures: the energy after a 4th-order Butterworth high-pass at
# 8 kHz (two SVF stages, Q 0.541 and 1.307), over the same pauses and loud parts, against the input.
def above_8k_hp_db(x, mask):
    y = pre.svf_high(pre.svf_high(x, 8000, 0.5412), 8000, 1.3066)
    return 10 * math.log10(np.mean(y[mask] ** 2) + 1e-30)


def run_hp(freq, threshold, range_db, attack=0.5, release=60.0):
    highs = pre.svf_high(pre.signal, freq)
    red = follow(np.clip(threshold - envelope_db(highs, 15.0), 0.0, range_db), release, attack)
    out = pre.signal + (10 ** (-red / 20) - 1) * highs
    return (above_8k_hp_db(out, pre.pauses) - above_8k_hp_db(pre.signal, pre.pauses),
            above_8k_hp_db(out, pre.loud) - above_8k_hp_db(pre.signal, pre.loud))


print("As the C++ test measures it (8 kHz high-pass), limits: pauses <= -4.5 dB, loud parts within 0.5 dB:")
for name, args in (("DS Hiss, 4 kHz, -38, -12", (4000, -38.0, 12.0)),
                   ("DS-Mini Hiss, Amount 5 (5 kHz, -36, -9)", (5000, -36.0, 9.0))):
    p, l = run_hp(*args)
    print(f"    {name}: pauses {p:6.2f} dB, loud parts {l:6.2f} dB")
