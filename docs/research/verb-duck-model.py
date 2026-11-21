"""Model of Fancy Verb's ducking and level gate (docs/verb.md, Phase 4 Wave 2, item 8 of the order).

Both are live stages on the wet signal after the convolver, with Fancy Room's detector (a peak
detector on the dry input, docs/room.md decision 7 and its Phase 4 gate). What's new in Verb is
latency: in Low CPU mode the convolver reports 4096 samples, and the dry path is delayed to match.
A detector reading the input as it arrives would act that much before the wet signal it should
act on. This prints, for a drum hit at t = 0 and the hit's reverb, when the duck and the gate act
against when the wet signal they act on is heard, with the detector undelayed and delayed by the
reported latency.

Run: py -3 docs/research/verb-duck-model.py
"""
RATES = (44100.0, 48000.0, 96000.0)
LOW_CPU_LATENCY = 4096          # samples, at any rate (docs/verb.md)
DUCK_ATTACK_MS, DUCK_RELEASE_MS = 10.0, 300.0
GATE_HOLD_MS, GATE_DECAY_MS = 80.0, 40.0
HIT_MS = 50.0                   # the hit lasts 50 ms

for rate in RATES:
    latency_ms = LOW_CPU_LATENCY / rate * 1000
    heard_from = latency_ms                     # the wet signal of the hit starts here
    for name, delay in (("undelayed", 0.0), ("delayed by the latency", latency_ms)):
        duck_on = delay + DUCK_ATTACK_MS
        gate_shut = delay + HIT_MS + GATE_HOLD_MS + GATE_DECAY_MS
        print(f"{rate / 1000:5.1f} kHz, detector {name:23}: duck down at {duck_on:6.1f} ms, gate shut at {gate_shut:6.1f} ms; "
              f"the hit's reverb is heard from {heard_from:5.1f} ms")
    print()

print("Undelayed, at 48 kHz the gate shuts 85 ms early: a 170 ms gated reverb would lose half its length,")
print("and the duck lets go 85 ms before the dry hit is even heard. Delayed by the latency, both act on")
print("the wet signal as Room's do. Zero latency mode has none to delay.")
