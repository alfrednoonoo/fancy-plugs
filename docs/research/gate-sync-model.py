"""Model of Fancy Gate's tempo-synced trigger (docs/gate.md, Phase 4 Wave 2, item 4 of the order).

Gate reuses Comp's fancy::dsp::TempoTrigger, but Gate's detector is a threshold and a state machine
(open while the key is over Threshold, then Hold, then Release), not a compressor curve. So what
matters is how long each hit holds the gate open:

  Decaying kick (Comp's): 0 dBFS falling to -60 dB over Shape of the step. The key is over
  Threshold T for Shape x step x (-T / 60): the open time depends on Threshold as much as on Shape.
  Flat pulse: 0 dBFS for exactly Shape of the step, then silence. Open for Shape x step at any
  Threshold below 0 dBFS, which is what "Shape 50 %" says.

Prints the open time per step (before Hold and Release) at 120 BPM, Rate 1/8 (250 ms).

Run: py -3 docs/research/gate-sync-model.py
"""
STEP_MS = 250.0

print(f"{'Threshold':>10} {'Shape':>6}   decaying kick   flat pulse")
for threshold in (-50.0, -30.0, -12.0):
    for shape in (0.25, 0.5, 0.9):
        decaying = shape * STEP_MS * (-threshold / 60.0)
        flat = shape * STEP_MS
        print(f"{threshold:8.0f} dB {shape:6.0%}   {decaying:10.1f} ms   {flat:7.1f} ms")

print("\nWith the decaying kick, moving Threshold from -50 to -12 dB shortens every gate from 83 % to 20 % of")
print("Shape; with the flat pulse, Threshold doesn't matter and Shape is the gate's length, the trance-gate")
print("behaviour. Comp keeps the decaying kick, where Threshold and Ratio shaping the duck is the point.")
