#pragma once

#include <fancy/dsp/Compression.h>
#include <fancy/dsp/Detector.h>
#include <fancy/dsp/TempoTrigger.h>
#include <fancy/dsp/LevelHistory.h>
#include <fancy/dsp/MatchedFilters.h>
#include <fancy/dsp/Saturation.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>
#include <fancy/dsp/TruePeakLimiter.h>

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

// CompDsp as released in v1.7.0 (with Wave 2 items 3 and 6), before the six-band detector EQ, cut slopes and link modes (docs/comp.md, Phase 4 Wave 2 items 13 and 14), for the compatibility test.
class LegacyCompW2Dsp
{
public:
    // Host sync (Phase 4 Wave 2) is appended, so stored indices keep their meaning.
    enum Detector { mainLR, mainL, mainR, sideLR, sideL, sideR, mid, side, hostSync };
    enum Oversampling { off, x2, x4, x8 };

    // Phase 3 Q2.5.
    enum Character { characterOff, warm, bright };
    enum EqBand { lowShelf, bell, highShelf, numEqBands };

    static constexpr double infiniteRatio = 100.0;

    // Auto threshold's reference: Threshold is set as if the input sat here (Decision 3 A).
    static constexpr double autoReferenceDb = -18.0;
    static constexpr double maxAutoOffsetDb = 24.0;

    struct EqBandTargets
    {
        double frequencyHz = 1000.0;
        double gainDb = 0.0;
        double q = 1.0;
    };

    struct Targets
    {
        double thresholdDb = 0.0;
        double ratio = 4.0;
        double kneeDb = 0.5;
        double attackMs = 3.0;
        double releaseMs = 200.0;
        double preCompMs = 0.0;
        double rmsMs = 5.0;
        double lowpassHz = 24000.0;
        double highpassHz = 20.0;
        double wetGain = 1.0;
        double dryGain = 0.0;
        double outputGain = 1.0;
        double linkAmount = 1.0;
        int detector = mainLR;
        int oversampling = off;
        bool weirdKnee = false;
        bool classic = false;
        bool autoRelease = false;
        bool linearRelease = false;
        bool attack90 = false;
        bool optical = false;   // Phase 4 Wave 2: the optical follower, in place of the default and Classic
        bool minimumPhase = false;
        bool linkChannels = true;
        bool feedback = false;
        bool autoMakeup = false;
        bool previewFilter = false;
        bool limitOutput = false;
        bool bypass = false;

        // Phase 3 Q2.5. The defaults leave the path as it was, to the bit.
        int character = characterOff;
        bool characterPost = true;
        double driveDb = 6.0;
        bool upward = false;
        double upThresholdDb = -40.0;
        double upRatio = 2.0;
        double maxLiftDb = 12.0;
        double floorDb = -70.0;
        bool autoThreshold = false;
        std::array<EqBandTargets, numEqBands> eq { EqBandTargets { 100.0, 0.0, 0.70710678118654752 },
                                                   EqBandTargets { 1000.0, 0.0, 1.0 },
                                                   EqBandTargets { 8000.0, 0.0, 0.70710678118654752 } };

        // Phase 4 Wave 1 (docs/comp.md). Hold 0 and Range 0 (Off) leave the path as it was.
        double holdMs = 0.0;
        double rangeDb = 0.0;
        bool listenRemoved = false;

        // Phase 4 Wave 2: Host sync's grid step in beats (1 is a quarter note) and the virtual
        // kick's length as a share of the step.
        double syncStepBeats = 1.0;
        double syncShape = 0.25;
    };

    // The host's transport, for the history's beat and bar lines. Set once per block.
    struct Transport
    {
        bool playing = false;
        double ppq = 0.0, bpm = 0.0, barStartPpq = 0.0;
        int numerator = 4, denominator = 4;
    };

    LegacyCompW2Dsp();
    ~LegacyCompW2Dsp();

    void prepare (double sampleRate);
    void reset();

    void setTargets (const Targets& targets);

    // Jumps straight to the targets, for use before processing starts.
    void snapToTargets (const Targets& targets);

    void process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples);

    void setTransport (const Transport& newTransport) noexcept { transport = newTransport; }

    // Phase 4 Wave 2: whether Host sync is running on the host's tempo (or on 120 BPM).
    bool syncHasHostTempo() const noexcept { return trigger.hasHostTempo(); }

    // The level history's columns made since the last call (the editor's timer).
    int popHistory (fancy::dsp::LevelColumn* destination, int maxColumns) noexcept { return history.pop (destination, maxColumns); }

    int getLatencySamples() const noexcept { return activeLatency; }

    // Largest gain reduction since the last call, as a positive number of dB.
    float takeGainReduction (int channel) noexcept { return gainReduction[(size_t) channel].exchange (0.0f, std::memory_order_relaxed); }

    // Auto threshold's current offset in dB, added to Threshold and Up threshold (0 when off).
    float getAutoThresholdOffset() const noexcept { return autoOffsetDisplay.load (std::memory_order_relaxed); }

    // The detector level and the upward lift at the end of the last block, for the Upward
    // tab's transfer curve.
    float getDisplayLevel() const noexcept { return levelDisplay.load (std::memory_order_relaxed); }
    float getDisplayLift() const noexcept { return liftDisplay.load (std::memory_order_relaxed); }

    // The static curve, exposed for tests. Returns gain reduction as a positive number of dB.
    static double computeReduction (double levelDb, double thresholdDb, double slope, double kneeDb, bool weird) noexcept;

    // Character's shaper on its own at one sample rate, for tests: x in, x + added out.
    struct CharacterStage
    {
        void prepare (int type, double sampleRate) noexcept;
        void reset() noexcept;
        double process (double x, double drive, double amount) noexcept;

        fancy::dsp::saturation::Curve curve;
        fancy::dsp::saturation::TiltCoefficients emphasis, deEmphasis;
        fancy::dsp::saturation::Tilt pre, post;
        fancy::dsp::saturation::Adaa adaa;
        double previousU = 0.0;
        double dcCoefficient = 0.0, dcInput = 0.0, dcOutput = 0.0;
        bool blockDc = false;
    };

private:
    static constexpr int maxChunk = 64;
    static constexpr int maxFactor = 8;
    static constexpr int maxStages = 3;
    static constexpr int numOversamplers = maxStages * 2;
    static constexpr double maxPreCompMs = 250.0;

    struct Config
    {
        int oversampling = off;
        int preCompSamples = 0;
        bool minimumPhase = false;
        bool limiter = false;

        bool operator== (const Config&) const = default;
    };

    struct EqState
    {
        double z1 = 0.0, z2 = 0.0;
    };

    struct ChannelState
    {
        fancy::dsp::DetectorFilter detector;
        std::vector<float> dryDelay, preCompDelay;
        fancy::dsp::EnvelopeFollower follower, upFollower;
        fancy::dsp::OpticalFollower optical; // Phase 4 Wave 2
        double previousOutput = 0.0;
        CharacterStage character;
        std::array<EqState, numEqBands> eq {};
    };

    struct EqSmoothers
    {
        fancy::dsp::Smoother logFrequency, gain, q;
    };

    using Oversampler = juce::dsp::Oversampling<float>;

    void processChunk (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int offset, int numSamples);
    void compress (float* const* signal, const float* const* key, int numSamples);
    void updateConfig (int numSamples);
    void applyConfig (const Config& config);
    void updateBallistics() noexcept;
    Config configFor (const Targets& targets) const noexcept;
    int latencyOf (const Config& config) const noexcept;
    int factorOf (const Config& config) const noexcept;
    Oversampler* oversamplerFor (const Config& config, bool sidechainPath) const noexcept;
    void resetCore();

    void setNewSmootherTimes() noexcept;
    void setNewTargets (const Targets& targets) noexcept;
    void updateEqCoefficients() noexcept;
    double filterEq (ChannelState& state, double x) noexcept;
    void activateCharacter() noexcept;

    double sampleRate = 48000.0, internalRate = 48000.0;

    std::array<std::unique_ptr<Oversampler>, numOversamplers> mainOversamplers, keyOversamplers;
    std::array<int, numOversamplers> oversamplerLatencies {};

    juce::AudioBuffer<float> mainBuffer, keyBuffer;
    std::array<ChannelState, 2> channels;
    fancy::dsp::TruePeakLimiter limiter;

    fancy::dsp::SvfCoefficients highPassCoefficients, lowPassCoefficients;

    Targets current;
    Config activeConfig, wantedConfig;
    int wantedStableSamples = 0;
    int holdSamples = 0;
    int activeLatency = 0;
    int preCompLength = 1, dryLength = 1;
    int preCompPosition = 0, dryPosition = 0;

    fancy::dsp::Ballistics ballistics;
    fancy::dsp::OpticalFollower opticalTimes; // Phase 4 Wave 2: its times (each channel keeps its own state)
    fancy::dsp::Ballistics upBallistics; // the lift's: the same times without Hold
    double rmsCoefficient = 1.0;
    double maxDetectorCoefficient = 1.0;

    fancy::dsp::Smoother threshold, slope, knee, makeup, rms, previewAmount, lowPassAmount, highPassAmount;
    fancy::dsp::Smoother logLowPass, logHighPass;
    fancy::dsp::Smoother wet, dry, output, bypass, limiterAmount, switchGain;

    // Phase 3 Q2.5.
    fancy::dsp::Smoother upThreshold, upSlope, maxLift, upFloor;
    fancy::dsp::Smoother drive, characterAmount, autoOffset;
    std::array<EqSmoothers, numEqBands> eqSmoothers;
    std::array<fancy::dsp::matched::Section, numEqBands> eqSections {};
    std::array<bool, numEqBands> eqActive {};
    int eqCountdown = 0;
    bool eqNeedsUpdate = false;

    int activeCharacter = characterOff;
    bool activeCharacterPost = true;
    double driveGain = 1.0;

    double longTermPower = 0.0, gatePower = 0.0;
    double longTermCoefficient = 0.0, gateCoefficient = 0.0;
    int autoCountdown = 0;

    // Phase 4 Wave 1.
    std::uint8_t markFor (double blockSample) noexcept;

    fancy::dsp::Smoother rangeLimit, removedAmount, removedMix;
    bool rangeOn = false;
    fancy::dsp::LevelHistoryRecorder history;
    Transport transport;
    fancy::dsp::TempoTrigger trigger; // Phase 4 Wave 2: Host sync
    int chunkOffset = 0;
    std::int64_t lastBeat = 0;
    bool haveLastBeat = false;

    std::array<std::atomic<float>, 2> gainReduction {};
    std::atomic<float> autoOffsetDisplay { 0.0f }, levelDisplay { -100.0f }, liftDisplay { 0.0f };
};
