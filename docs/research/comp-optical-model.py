"""Model of Fancy Comp's Optical mode (docs/comp.md, Phase 4 Wave 2, item 6 of the order).

An optical cell (a lamp lighting a photoresistor) behaves in three ways the default follower
doesn't, and this follower models each, in dB of gain reduction:

  1. Attack depends on the signal: the harder the lamp is driven, the faster it lights.
     attack rate = Attack's rate x (over / 6 dB), held between 0.5x and 4x: at 6 dB over the
     threshold the attack is the knob's time.
  2. Two-stage release: half the reduction lets go fast (Release / 10), the rest slowly (Release).
  3. Memory: the cell "remembers" long reduction. A memory value charges towards 1 while more than
     3 dB is taken off (time constant 2 s) and drains back (6 s); the slow stage's time is
     Release x (1 + 3 x memory), so after long heavy compression recovery takes up to 4 x longer.

Prints, for Attack 10 ms and Release 500 ms at 48 kHz: the attack's 63 % time for 3, 6 and 12 dB
over; the release after a 0.3 s and a 6 s burst of 12 dB reduction (time to come back to 1 dB);
and the same releases with the default follower for comparison.

Run: py -3 docs/research/comp-optical-model.py
"""
import math

RATE = 48000.0
ATTACK_MS, RELEASE_MS = 10.0, 500.0


def coefficient(ms):
    return 1.0 - math.exp(-1.0 / max(1e-9, ms * 0.001 * RATE))


class Optical:
    def __init__(self):
        self.fast = self.slow = self.memory = 0.0

    def follow(self, target):
        total = self.fast + self.slow
        if target > total:
            over = target - total
            speed = min(4.0, max(0.5, over / 6.0))
            step = coefficient(ATTACK_MS / speed) * over
            self.fast += 0.5 * step
            self.slow += 0.5 * step
        else:
            self.fast += coefficient(RELEASE_MS / 10.0) * (0.5 * target - self.fast)
            slow_time = RELEASE_MS * (1.0 + 3.0 * self.memory)
            self.slow += coefficient(slow_time) * (0.5 * target - self.slow)
        charging = total > 3.0
        self.memory += (coefficient(2000.0) if charging else coefficient(6000.0)) * ((1.0 if charging else 0.0) - self.memory)
        return self.fast + self.slow


class Default:
    def __init__(self):
        self.env = 0.0

    def follow(self, target):
        self.env += (coefficient(ATTACK_MS) if target > self.env else coefficient(RELEASE_MS)) * (target - self.env)
        return self.env


def attack_time(over_db):
    follower = Optical()
    for n in range(int(RATE)):
        if follower.follow(over_db) >= 0.632 * over_db:
            return n / RATE * 1000
    return float("nan")


def recovery(follower_class, burst_s, depth=12.0):
    follower = follower_class()
    for _ in range(int(burst_s * RATE)):
        follower.follow(depth)
    for n in range(int(30 * RATE)):
        if follower.follow(0.0) <= 1.0:
            return n / RATE * 1000
    return float("nan")


print(f"Attack {ATTACK_MS:.0f} ms, Release {RELEASE_MS:.0f} ms")
for over in (3.0, 6.0, 12.0):
    print(f"  attack to 63 %, {over:4.0f} dB over: {attack_time(over):6.1f} ms")
for burst in (0.3, 6.0):
    print(f"  back to 1 dB after a {burst:3.1f} s burst of 12 dB: optical {recovery(Optical, burst):7.0f} ms, "
          f"default {recovery(Default, burst):7.0f} ms")
