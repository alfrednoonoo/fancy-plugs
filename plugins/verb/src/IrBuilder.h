#pragma once

#include "VerbChain.h"

#include <array>
#include <utility>
#include <vector>

namespace verb
{

inline constexpr double maxIrSeconds = 60.0;
inline constexpr int previewColumns = 1024;

// Phase 4 Wave 2: what a Decay module measured, for its card. Bands are low, mid and high.
struct DecayReadout
{
    int uid = 0;
    std::array<double, 3> beforeSeconds {}, afterSeconds {};
    std::array<bool, 3> measured {};   // false: "can't measure", and the band is left as it is
    std::array<bool, 3> limited {};    // lengthening stopped where the band reached the IR's floor
};

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

    // Phase 4 Wave 1: the gain Equal loudness applied, in dB, or 0 with it off.
    double loudnessTrimDb = 0.0;

    // Phase 4 Wave 2: one per Decay module that's on, in chain order.
    std::vector<DecayReadout> decays;
};

// Modulate (Phase 3 Q2.8): the tail after Start in two versions with the same decay, which the
// convolver crossfades slowly.
struct Modulation
{
    bool on = false;
    double rateHz = 0.3, depth = 0.5, spread = 1.0;
    int seed = 1;
};

struct BuiltIr
{
    // Two channels, equal for a mono IR. A true-stereo IR (Phase 3 Q2.8) has four: left in to
    // left out, right in to right out, then right in to left out (RL) and left in to right out (LR).
    juce::AudioBuffer<float> audio;
    IrPreview preview;

    // With Modulate on, audio holds the early part only, and the tail after Start is here as it
    // is (A) and decorrelated (B). early + A is the whole IR.
    Modulation modulation;
    juce::AudioBuffer<float> tailA, tailB;
};

// The same spectrum and envelope with a different fine structure: short-time FFT frames (2048,
// hop 512, root-Hann) with their phases randomised from the seed. Off the audio thread.
std::vector<double> decorrelate (const std::vector<double>& signal, int seed);

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

// Phase 4 Wave 2 (docs/verb.md, "decay module"; modelled in docs/research/verb-decay-model.py):
// splits the channels into three bands (an 8192-point short-time FFT with complementary
// half-octave crossfades, so the bands sum to the input), measures each band's RT60 on all
// channels' energy together, and reshapes each band's envelope so its RT becomes the multiplier
// times what it was. A band at 1 x, or without a measurable decay, is left untouched; a band
// lengthened stops growing within 10 dB of its noise floor and fades out over 100 ms.
DecayReadout reshapeDecay (std::vector<std::vector<double>*>& channels, const std::array<double, 3>& multipliers,
                           double lowCrossHz, double highCrossHz, double sampleRate);

// Schroeder RT60 of an energy envelope (per-sample energy), from -5 to -35 dB, or -5 to -25 dB
// if it never falls 35 dB. Returns 0 if it can't be measured.
double schroederRt60 (const std::vector<double>& energy, double sampleRate);

// Runs the chain's modules in order and applies the display handles.
BuiltIr buildIr (const ChainSpec& chain, double sampleRate);

// Phase 4 Wave 1 (docs/verb.md): Equal loudness. The pink-weighted energy of one IR channel
// (1/f weights from 20 Hz to 20 kHz over frames of about 1/6 s), equal to the plain energy for
// white noise. The trim brings an IR's energy to the reference: where a library IR at the
// default gain mode (-18 dB) lands, so those get about 0 dB.
double pinkEnergy (const std::vector<double>& samples, double sampleRate);
double referenceLoudnessEnergy (double sampleRate) noexcept;
inline constexpr double maxLoudnessTrimDb = 24.0;

// The finished IR as a file holds it (Phase 4 Wave 1, Export IR): Modulate's tail added back,
// and true stereo in a four-channel file's order, LL, LR, RL, RR.
juce::AudioBuffer<float> exportableIr (const BuiltIr& built);

} // namespace verb
