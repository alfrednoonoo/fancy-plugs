#pragma once

#include <fancy/dsp/DelayTime.h>

#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <array>
#include <cstdint>
#include <vector>

// See docs/delay.md.
class DelayDsp
{
public:
    enum Character { clean, tape, bbd };

    struct TapSpec
    {
        bool used = false, on = true, solo = false;
        bool noteMode = true, pingPong = false, filtersInLoop = true, saturate = false;
        double lengthMs = 375.0;
        delay::NoteLength noteLength;
        double feedback = 0.0;          // 1 is 100 %
        double lowpassHz = 22000.0, highpassHz = 20.0;
        double bits = 24.0, width = 1.0, volumeGain = 1.0, pan = 0.0;

        // Phase 3 Q2.3. The defaults leave the tap as it was, to the bit.
        int character = clean;
        double characterAmount = 0.5;   // 0 to 1
        double modRateHz = 0.8, modDepthCents = 0.0, modRandom = 0.3;
        double diffusion = 0.0;         // 0 to 1
    };

    struct Targets
    {
        std::array<TapSpec, delay::maxTaps> taps {};
        double bpm = 120.0;
        double wetGain = 1.0, dryGain = 1.0, outputGain = 1.0;
        bool smoothTime = true, bypass = false;

        // Phase 3 Q2.3: ducking, swing, and the fast tanh (off for states saved before it).
        double duckAmountDb = 0.0, duckThresholdDb = -24.0, duckReleaseMs = 250.0;
        double swing = 0.5;
        bool fastTanh = false;
    };

    // tanh within 5e-4, never above 1 in size (Phase 3 Q2.3).
    static double fastTanh (double x) noexcept;

    static constexpr double lowpassOffHz = 22000.0, highpassOffHz = 20.0;
    static constexpr double fullBits = 24.0;

    static double tapMs (const TapSpec& spec, double bpm, double swing = 0.5) noexcept;

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

        // Phase 3 Q2.3.
        Smoother modDepth, tapeMix, bbdMix, diffusion;
        double modRate = 0.8, modRandom = 0.3, characterAmount = 0.5, bbdCutoffHz = 15000.0;
        double modPhase = 0.0, walk = 0.0, walkTarget = 0.0;
        int walkCountdown = 0;
        std::uint32_t noiseState = 1;

        struct ToneState
        {
            double bump1 = 0.0, bump2 = 0.0, loss = 0.0;
            fancy::dsp::Svf bbdA, bbdB;
            double envelope = 0.0;
        };

        ToneState tone[2];
        fancy::dsp::SvfCoefficients bbdCoefficients;
        double bumpB0 = 1.0, bumpB1 = 0.0, bumpB2 = 0.0, bumpA1 = 0.0, bumpA2 = 0.0, lossCoefficient = 1.0;
        double lastCharacterAmount = -1.0, lastCutoff = -1.0;
        bool characterActive = false, diffusionActive = false;

        std::vector<float> diffusers[2][4];
        int diffuserLength[2][4] {};
        int diffuserPosition = 0;
    };

    void updateCoefficients (Tap&) noexcept;
    void processTap (Tap&, const float* inLeft, const float* inRight, double* wetLeft, double* wetRight, int numSamples) noexcept;
    float readHead (const Tap&, int channel, int writeAt, int head) const noexcept;
    float readGlide (const Tap&, int channel, int writeAt) const noexcept;
    float readAt (const Tap&, int channel, int writeAt, double position) const noexcept;
    double applyCharacter (Tap&, int channel, double value, double tapeMix, double bbdMix) noexcept;
    void updateCharacter (Tap&) noexcept;
    double diffuse (Tap&, int channel, double value, double coefficient) noexcept;
    void setTapTargets (Tap&, const TapSpec&, bool anySolo, double bpm, double swing, bool smoothTime, bool isMono) noexcept;
    void snapTap (Tap&) noexcept;

    double rate = 44100.0;
    int ringSize = 0, ringMask = 0, maxDelaySamples = 1, fadeSamples = 1;
    int writeIndex = 0;
    double dcCoefficient = 0.999, glideCoefficient = 0.001;

    std::array<Tap, delay::maxTaps> taps;

    Smoother wetGain, dryGain, outputGain, bypassMix;

    // Phase 3 Q2.3.
    Smoother duckAmount;
    double duckThresholdDb = -24.0, duckEnvelope = 0.0, duckAttack = 1.0, duckRelease = 1.0;
    bool useFastTanh = false;
};
