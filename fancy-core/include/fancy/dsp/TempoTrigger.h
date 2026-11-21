#pragma once

#include <algorithm>
#include <cmath>

// Phase 4 Wave 2 (docs/comp.md, "tempo-synced trigger"): a virtual kick on the host's beat grid,
// for a detector to hear instead of audio. A hit at every step (in beats), 1.0 at the hit and
// decaying exponentially to -60 dB over Shape's share of the step. Modelled in
// docs/research/comp-sync-model.py.
//
// Hits are found by step index: the last index fired is kept, and a hit fires only when the
// index goes up, so rounding at a boundary can't fire twice and every block size gives the
// same hits. The host's position is taken at each block's start; while the transport is stopped
// the trigger runs on from where it was at the last tempo (Decision 2 A), and a jump (play, a
// loop, a seek) re-aligns it. Shared, so Gate and Kicker's Duck can use it.
namespace fancy::dsp
{

class TempoTrigger
{
public:
    struct Position
    {
        bool hasTempo = false;  // the host sent a tempo
        bool playing = false;   // and its transport is running
        double ppq = 0.0;       // quarter notes at the block's first sample
        double bpm = 120.0;
    };

    static constexpr double defaultBpm = 120.0;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        freePpq = 0.0;
        lastIndex = 0;
        aligned = false;
        sinceHit = -1;
        pending = false;
    }

    void setStep (double beats) noexcept { stepBeats = std::max (1.0e-3, beats); }
    void setShape (double fraction) noexcept { shape = std::clamp (fraction, 0.01, 1.0); }

    // The host's position at the start of the next block. Call once per host block.
    void setPosition (const Position& position) noexcept
    {
        next = position;
        pending = true;
    }

    // Whether the last block ran on the host's tempo, for the "No host tempo" caption.
    bool hasHostTempo() const noexcept { return hostTempo; }

    void process (float* out, int numSamples) noexcept
    {
        auto ppq = freePpq;

        if (pending)
        {
            pending = false;
            hostTempo = next.hasTempo;

            if (next.hasTempo)
                bpm = std::clamp (next.bpm, 20.0, 999.0);

            if (next.hasTempo && next.playing)
            {
                // A jump of more than a couple of samples re-aligns to the host's grid.
                const auto perSample = bpm / (60.0 * sampleRate);

                if (! aligned || std::abs (next.ppq - freePpq) > 2.0 * perSample)
                    align (next.ppq);

                ppq = next.ppq;
            }
        }

        if (! aligned)
            align (ppq);

        const auto perSample = bpm / (60.0 * sampleRate);
        const auto stepSamples = stepBeats / perSample;
        const auto decay = std::exp (-std::log (1000.0) / std::max (1.0, shape * stepSamples));

        for (int i = 0; i < numSamples; ++i)
        {
            const auto position = ppq + (double) i * perSample;
            const auto index = (long long) std::floor (position / stepBeats + 1.0e-9);

            if (index > lastIndex)
            {
                lastIndex = index;
                sinceHit = 0;
                level = 1.0;
            }

            if (sinceHit < 0)
            {
                out[i] = 0.0f;
                continue;
            }

            out[i] = (float) level;
            level *= decay;
            ++sinceHit;

            if (level < 1.0e-6)
            {
                level = 0.0;
                sinceHit = -1;
            }
        }

        freePpq = ppq + (double) numSamples * perSample;
    }

private:
    // From here, the next step boundary fires, or this one if the position is on it.
    void align (double ppq) noexcept
    {
        lastIndex = (long long) std::ceil (ppq / stepBeats - 1.0e-9) - 1;
        aligned = true;
    }

    double sampleRate = 48000.0, bpm = defaultBpm, stepBeats = 1.0, shape = 0.25;
    double freePpq = 0.0, level = 0.0;
    long long lastIndex = 0;
    long long sinceHit = -1;
    bool aligned = false, pending = false, hostTempo = false;
    Position next;
};

} // namespace fancy::dsp
