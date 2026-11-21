"""Model of Fancy Comp's tempo-synced trigger (docs/comp.md, Phase 4 Wave 2, item 3 of the order).

Comp gets a new Detector input, "Host sync": instead of audio, the detector hears a virtual kick,
a 0 dBFS hit at every step of the host's grid (Rate) that decays over Shape's share of the step.
Threshold, Ratio, Attack, Release and Knee then act as on a real sidechain kick.

Checks, at 120 BPM, 48 kHz, quarter notes, Threshold -20, Ratio 8, Attack 1 ms, Release 120 ms:
  1. Where the hits land: computed per sample from the block's start position (ppq), the hit
     lands on the same sample for every block size, and on the grid to the sample.
  2. The ducking curve for Shape 10 %, 25 % and 50 %: reduction at the hit, at a quarter and at
     half the step, and how long it's over 1 dB.
  3. The alternative, Decision 1 B: a gain envelope drawn directly (no detector): its depth is
     a new control, and Threshold, Ratio, Attack and Release no longer mean anything.

Run: py -3 docs/research/comp-sync-model.py
"""
import math

RATE, BPM = 48000.0, 120.0
STEP_BEATS = 1.0                     # quarter notes
SAMPLES_PER_BEAT = RATE * 60.0 / BPM


def hits(total, block, start_ppq=0.37):
    """Sample indices of the hits, found per sample from each block's start ppq, as a host gives it."""
    found = []
    for start in range(0, total, block):
        ppq = start_ppq + start / SAMPLES_PER_BEAT  # what the host reports for this block
        for i in range(min(block, total - start)):
            p = ppq + i / SAMPLES_PER_BEAT
            previous = p - 1.0 / SAMPLES_PER_BEAT
            if math.floor(p / STEP_BEATS) != math.floor(previous / STEP_BEATS):
                found.append(start + i)
    return found


def key(total, shape, start_ppq=0.0):
    """The virtual kick: 1.0 at each hit, decaying exponentially to -60 dB over shape x step."""
    step = STEP_BEATS * SAMPLES_PER_BEAT
    tau = max(1.0, shape * step) / math.log(1000.0)
    out, since = [], None
    for n in range(total):
        p = start_ppq + n / SAMPLES_PER_BEAT
        if n == 0 and p % STEP_BEATS == 0 or n > 0 and math.floor(p / STEP_BEATS) != math.floor((p - 1 / SAMPLES_PER_BEAT) / STEP_BEATS):
            since = 0
        out.append(0.0 if since is None else math.exp(-since / tau))
        if since is not None:
            since += 1
    return out


def compress(signal, threshold=-20.0, ratio=8.0, attack_ms=1.0, release_ms=120.0):
    a = 1 - math.exp(-1 / (attack_ms * 0.001 * RATE))
    r = 1 - math.exp(-1 / (release_ms * 0.001 * RATE))
    slope = 1 - 1 / ratio
    env, gr = 0.0, []
    for x in signal:
        level = 20 * math.log10(max(abs(x), 1e-9))
        target = max(0.0, level - threshold) * slope
        env += (target - env) * (a if target > env else r)
        gr.append(env)
    return gr


def hits_robust(total, block, start_ppq=0.37):
    """The fix: remember the last step index fired and fire only when it goes up, so a rounding
    wobble at a boundary can't fire twice; and take each block's position from the host."""
    found, last = [], None
    for start in range(0, total, block):
        ppq = start_ppq + start / SAMPLES_PER_BEAT
        for i in range(min(block, total - start)):
            index = math.floor((ppq + i / SAMPLES_PER_BEAT) / STEP_BEATS + 1e-9)
            if last is not None and index > last:
                found.append(start + i)
            last = index if last is None else max(last, index)
    return found


total = int(SAMPLES_PER_BEAT * 4)
print("1. Hits by block size (start ppq 0.37):")
reference = hits(total, 1)
for block in (1, 64, 441, 512, 4096):
    print(f"   block {block:4}: {hits(total, block)}  same as per-sample: {hits(total, block) == reference}")
print(f"   grid: {[round((k - 0.37) * SAMPLES_PER_BEAT) for k in range(1, 5)]}")
print("   The naive per-sample comparison can fire twice at one step (87120 and 87121), depending on the block. With the last step index kept:")
for block in (1, 64, 441, 512, 4096):
    print(f"   block {block:4}: {hits_robust(total, block)}")

print("\n2. Gain reduction through Comp's detector model (dB):")
step = int(SAMPLES_PER_BEAT)
for shape in (0.10, 0.25, 0.50):
    gr = compress(key(step * 3, shape))
    beat = step  # look at the second step
    over = sum(1 for g in gr[beat:beat + step] if g > 1.0) / RATE * 1000
    print(f"   Shape {shape:4.0%}: at the hit + 2 ms {gr[beat + 96]:5.1f}, quarter step {gr[beat + step // 4]:5.1f}, "
          f"half step {gr[beat + step // 2]:5.1f}, end {gr[beat + step - 1]:5.1f}; over 1 dB for {over:4.0f} ms of {step / RATE * 1000:.0f}")

print("\n3. Decision 1 B, a drawn envelope: Threshold, Ratio, Attack and Release would be bypassed;")
print("   the curve's depth would be one more knob. Model A keeps every Comp control meaningful.")
