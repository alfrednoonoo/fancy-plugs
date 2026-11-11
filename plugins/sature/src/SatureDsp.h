#pragma once

#include <fancy/dsp/Saturation.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>
#include <fancy/dsp/SwitchedOversampler.h>

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <vector>

// Fancy Sature's signal path. See docs/sature.md.
//
// The plugin adds what saturating the band changes, `added = saturated(mid) - mid`, to the
// input, so every unsaturated path is the input itself through matched filters:
//   matched path:   x -> up -> half-sample delay -> down                     = m
//   saturated path: mid -> up -> Drive -> curve (ADAA) -> Tape loss -> Tone -> auto gain,
//                   minus the ADAA's own linear reference, -> down             = added
//   out = ((m + DC filter (added)) x Wet + m x Dry) x Output
class SatureDsp
{
public:
    enum Oversampling { off, x2, x4, x8 };

    struct Targets
    {
        int type = fancy::dsp::saturation::tape;
        double driveDb = 6.0;
        double character = -0.3;   // -1 even 100 %, +1 odd 100 %
        double toneDb = 0.0;
        bool lowOn = false;
        double lowHz = 20.0;
        bool highOn = false;
        double highHz = 20000.0;
        bool autoGain = true;
        double trimGain = 1.0;
        double wetGain = 1.0;
        double dryGain = 0.0;
        double outputGain = 1.0;
        int oversampling = x4;
        bool minimumPhase = true;
        bool dcFilter = true;
        bool listen = false;
        bool bypass = false;
    };

    SatureDsp();

    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets);

    // Jumps straight to the targets, for use before processing starts.
    void snapToTargets (const Targets& targets);

    void process (float* const* channels, int numChannels, int numSamples);

    // Latency of the setting in use. It changes during process() when the oversampling does.
    int getLatencySamples() const noexcept;
    int latencyFor (int oversampling, bool minimumPhase) const noexcept;

    // Auto gain in dB from the build-time table, for one Type, Drive and Character.
    static double tableGainDb (int type, double driveDb, double character) noexcept;
    static double toneGainDb (double toneDb) noexcept;

    // The 8th-order Butterworth band edges (Decision 1) and the octave High keeps above Low
    // (Decision 2).
    static double effectiveHighHz (const Targets& targets) noexcept;

    static constexpr int maxChunk = 64;

private:
    static constexpr int numSections = 4;
    static constexpr int maxTaps = 8;

    struct ChannelState
    {
        std::array<fancy::dsp::Svf, numSections> lowCut, highCut;
        fancy::dsp::saturation::Adaa adaa;
        fancy::dsp::saturation::TapeLoss tapeLoss;
        fancy::dsp::saturation::Tilt tilt;
        double previousMid = 0.0;                    // the ADAA's linear reference
        std::array<double, maxTaps> matchedHistory {}; // the half-sample delay's taps
        std::array<double, 4> addedDelay {};          // aligns the saturated path with it
        double dcIn = 0.0, dcOut = 0.0;
        std::vector<float> dryDelay;
    };

    void processChunk (float* const* channels, int numChannels, int offset, int numSamples);
    void processOversampled (int numSamples);
    void processBaseRate (int numSamples);
    void resetCore();
    void advanceTypeFade();
    void updateBandFilters (bool force);
    void updateRateDependent();

    double sampleRate = 48000.0;
    double processRate = 48000.0;

    fancy::dsp::SwitchedOversampler oversampling;
    juce::AudioBuffer<float> matchedBuffer, midBuffer;

    std::array<ChannelState, 2> channelStates;

    std::array<fancy::dsp::SvfCoefficients, numSections> lowCoefficients, highCoefficients;
    std::array<double, numSections> sectionQ {};

    // Per input sample of the chunk.
    std::array<fancy::dsp::saturation::Curve, maxChunk> chunkCurve;
    std::array<bool, maxChunk> chunkShapeChanged {};
    std::array<double, maxChunk> chunkDc {}, chunkDrive {}, chunkGain {}, chunkTape {};
    std::array<fancy::dsp::saturation::TiltCoefficients, maxChunk> chunkTilt;

    fancy::dsp::saturation::Curve lastCurve;
    fancy::dsp::saturation::TiltCoefficients tiltCoefficients;
    double lastToneDb = 1.0e9;
    double tapeCoefficient = 0.0;
    double dcCoefficient = 0.0;

    // The half-sample delay (Lagrange) of the active setting, and the matching delay on the
    // saturated path.
    std::array<double, maxTaps> halfDelayTaps {};
    int numTaps = 4;
    int addedDelaySamples = 1;

    int activeType = 0, previousType = 0;
    int requestedOversampling = x4;
    bool requestedMinimumPhase = true;
    Targets targets;

    int delayWritePosition = 0;
    int delayLength = 1;

    fancy::dsp::Smoother trim, drive, character, tone, logLow, logHigh, lowOn, highOn, autoGain,
        wet, dry, output, bypass, listen, dcOn, typeFade;
};
