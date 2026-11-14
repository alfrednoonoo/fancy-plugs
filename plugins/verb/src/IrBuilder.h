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
    // Two channels, equal for a mono IR. A true-stereo IR (Phase 3 Q2.8) has four: left in to
    // left out, right in to right out, then right in to left out (RL) and left in to right out (LR).
    juce::AudioBuffer<float> audio;
    IrPreview preview;
};

// One early reflection of the Reverb generator's image-source box (Phase 3 Q2.8): its delay
// after the direct sound, and its gain relative to the direct sound.
struct Reflection
{
    double seconds = 0.0, gain = 0.0;
};

// The reflections from one input side to one output side (0 left, 1 right) for Early Room or
// Hall, in order of arrival. Empty for Off and Plate.
std::vector<Reflection> earlyPattern (EarlyType type, double sizeMetres, int sourceSide, int earSide);

// Windowed-sinc rate change, in double precision. Runs off the audio thread.
std::vector<double> resample (const double* source, int numSamples, double ratio);

// Runs the chain's modules in order and applies the display handles.
BuiltIr buildIr (const ChainSpec& chain, double sampleRate);

} // namespace verb
