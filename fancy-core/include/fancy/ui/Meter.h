#pragma once

#include "Style.h"

#include <fancy/dsp/MeterTap.h>

namespace fancy::ui
{

// Display ballistics for one channel: instant rise, steady fall, timed peak hold.
struct MeterChannel
{
    void update (float peakGain, float seconds);
    void show (float db) { level = hold = db; }

    float level = -120.0f;
    float hold = -120.0f;

private:
    float holdAge = 0.0f;
};

// The In and Out meter column used by Kicker and Exciter. Each side is a stereo pair of
// bars with one peak readout.
class InOutMeter final : public juce::Component
{
public:
    void update (dsp::MeterTap& input, dsp::MeterTap& output, float seconds);

    // Sets levels directly, for rendered screenshots.
    void show (float inLeft, float inRight, float outLeft, float outRight);

    void paint (juce::Graphics&) override;

private:
    MeterChannel channels[4];
};

} // namespace fancy::ui
