#pragma once

#include <fancy/dsp/DelayTime.h>

#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <array>
#include <vector>

// See docs/delay.md.
class DelayDsp
{
public:
    struct TapSpec
    {
        bool used = false, on = true, solo = false;
        bool noteMode = true, pingPong = false, filtersInLoop = true, saturate = false;
        double lengthMs = 375.0;
        delay::NoteLength noteLength;
        double feedback = 0.0;          // 1 is 100 %
        double lowpassHz = 22000.0, highpassHz = 20.0;
        double bits = 24.0, width = 1.0, volumeGain = 1.0, pan = 0.0;
    };

    struct Targets
    {
        std::array<TapSpec, delay::maxTaps> taps {};
        double bpm = 120.0;
        double wetGain = 1.0, dryGain = 1.0, outputGain = 1.0;
        bool smoothTime = true, bypass = false;
    };

    static constexpr double lowpassOffHz = 22000.0, highpassOffHz = 20.0;
    static constexpr double fullBits = 24.0;

    static double tapMs (const TapSpec& spec, double bpm) noexcept;

    void prepare (double sampleRate);
    void reset();

    // Call once per block before process().
    void setTargets (const Targets& targets, bool isMono = false) noexcept;

    // Jumps to the targets with no smoothing, for prepare and offline starts.
    void snapToTargets (const Targets& targets, bool isMono = false) noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // The tap's current delay. Exposed for tests.
    int getDelaySamples (int tap) const noexcept { return taps[(size_t) tap].targetDelay; }
    bool isTapAsleep (int tap) const noexcept { return taps[(size_t) tap].asleep; }

private:
    using Smoother = fancy::dsp::Smoother;

    struct Tap
    {
        std::vector<float> ring[2];

        Smoother feedback, width, volume, panLeft, panRight, inGate, outGate;
        Smoother lowpassMix, highpassMix, crushMix, loopMix, saturateMix, pingMix, startLeft;
        Smoother lowpassOctaves, highpassOctaves, bits;

        fancy::dsp::SvfCoefficients lowpassCoefficients, highpassCoefficients;
        fancy::dsp::Svf lowpass[2], highpass[2];

        double crushScale = 8388608.0;
        double dcInput[2] {}, dcOutput[2] {};

        int targetDelay = 1;
        int headA = 1, headB = 1, fadeLeft = 0;
        double glidePosition = 1.0;
        bool smoothTime = true;

        int quietSamples = 0;
        bool asleep = true;
    };

    void updateCoefficients (Tap&) noexcept;
    void processTap (Tap&, const float* inLeft, const float* inRight, double* wetLeft, double* wetRight, int numSamples) noexcept;
    float readHead (const Tap&, int channel, int writeAt, int head) const noexcept;
    float readGlide (const Tap&, int channel, int writeAt) const noexcept;
    void setTapTargets (Tap&, const TapSpec&, bool anySolo, double bpm, bool smoothTime, bool isMono) noexcept;
    void snapTap (Tap&) noexcept;

    double rate = 44100.0;
    int ringSize = 0, ringMask = 0, maxDelaySamples = 1, fadeSamples = 1;
    int writeIndex = 0;
    double dcCoefficient = 0.999, glideCoefficient = 0.001;

    std::array<Tap, delay::maxTaps> taps;

    Smoother wetGain, dryGain, outputGain, bypassMix;
};
