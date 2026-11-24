"""Pre-push model for Fancy EQ's band groups (docs/eq.md, "Phase 4 Wave 2: band list and groups",
section 2 and test 2), written before the push (24 November 2026). It works out what
tests/EqGroupsTests.cpp checks, with the rules plugins/eq/src/EqGroups.h follows: Freq and Q by a
ratio, Gain by an offset, from the values at the start of the edit; the move stops where the
first member would leave its range.

Ranges as the parameters have them: Freq 20 Hz to 20 kHz, Gain -24 to +24 dB, Q 0.1 to 30.

Run: python docs/research/eq-groups-prepush.py
"""


def limit(kind, move, starts, low, high):
    limited = move
    for start in starts:
        if kind == "ratio":
            if start * limited > high:
                limited = high / start
            if start * limited < low:
                limited = low / start
        else:
            if start + limited > high:
                limited = high - start
            if start + limited < low:
                limited = low - start
    return limited


def move(kind, starts, leader, now, low, high):
    wanted = now / starts[leader] if kind == "ratio" else now - starts[leader]
    m = limit(kind, wanted, starts, low, high)
    return [s * m if kind == "ratio" else s + m for s in starts]


print("Freq: 200 Hz and 2 kHz, the first to 400 Hz ->", move("ratio", [200.0, 2000.0], 0, 400.0, 20.0, 20000.0))
print("Freq: 200 Hz and 2 kHz, the first to 4 kHz (the second would pass 20 kHz) ->", move("ratio", [200.0, 2000.0], 0, 4000.0, 20.0, 20000.0))
print("Freq: 200 Hz and 2 kHz, the second to 100 Hz (the first would pass 20 Hz) ->", move("ratio", [200.0, 2000.0], 1, 100.0, 20.0, 20000.0))
print("Gain: -5.1 and +5.4 dB, the first to -2.1 ->", move("offset", [-5.1, 5.4], 0, -2.1, -24.0, 24.0))
print("Gain: -5.1 and +5.4 dB, the first to +20 (the second would pass +24) ->", move("offset", [-5.1, 5.4], 0, 20.0, -24.0, 24.0))
print("Q: 2.0 and 1.0, the first to 4.0 ->", move("ratio", [2.0, 1.0], 0, 4.0, 0.1, 30.0))
