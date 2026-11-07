#pragma once

#include "VerbChain.h"

#include <utility>
#include <vector>

namespace verb
{

inline constexpr double maxIrSeconds = 60.0;
inline constexpr int previewColumns = 1024;

struct IrPreview
{
    struct Column
    {
        float low = 0.0f, high = 0.0f, rms = 0.0f;
    };

    std::vector<Column> columns;      // the whole IR, before the display handles cut it
    std::vector<double> echoSeconds;  // where the echo generators put their echoes
    double fullSeconds = 0.0;
    double trimmedSeconds = 0.0;
    double sampleRate = 48000.0;
    int numChannels = 0;
    double peakDb = -200.0;
};

struct BuiltIr
{
    juce::AudioBuffer<float> audio; // always two channels; both are equal for a mono IR
    IrPreview preview;
};

// Windowed-sinc rate change, in double precision. Runs off the audio thread.
std::vector<double> resample (const double* source, int numSamples, double ratio);

// Runs the chain's modules in order and applies the display handles.
BuiltIr buildIr (const ChainSpec& chain, double sampleRate);

} // namespace verb
