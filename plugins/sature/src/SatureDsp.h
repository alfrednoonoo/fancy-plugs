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
    enum Channels { stereo, midOnly, sideOnly };   // Phase 4 Wave 2, item 39

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

        // Phase 4 Wave 2 (docs/sature.md): the bands below Low and above High, saturated with
        // the mid band's Type, Character, Tape HF loss and Tone. Drive 0 runs nothing.
        double lowDriveDb = 0.0, highDriveDb = 0.0;
        double lowMix = 1.0, highMix = 1.0;

        // Phase 4 Wave 2, items 35 to 39 (docs/sature.md). Every default is the old sound, bit
        // for bit. Offset: a bias before the curve, taken off after it (item 35). The cuts: a
        // low cut (high-pass) and a high cut (low-pass), 12 or 24 dB/oct, Pre (on what's driven)
        // or Post (on the added part) (item 37). Spread: the added harmonics turned +/- Spread x
        // 90 degrees on each channel, 0 to 0.5 (item 38). Channels: Stereo, or only the mid or
        // the side driven (item 39).
        double offset = 0.0;
        bool lowCutOn = false, lowCut24 = false, lowCutPost = false;
        double lowCutHz = 20.0;
        bool highCutOn = false, highCut24 = false, highCutPost = true;
        double highCutHz = 20000.0;
        double spread = 0.0;
        int channels = stereo;
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

    // Phase 4 Wave 2: one outer band's saturator state, per channel.
    struct OuterState
    {
        std::array<fancy::dsp::Svf, numSections> split;
        fancy::dsp::saturation::Adaa adaa;
        fancy::dsp::saturation::TapeLoss tapeLoss;
        fancy::dsp::saturation::Tilt tilt;
        double previousIn = 0.0;
        std::array<double, 4> addedDelay {};
    };

    enum Outer { lowBand, highBand };

    // Phase 4 Wave 2, item 37: one cut on one signal, its 12 dB/oct section and its 24 dB/oct
    // pair run side by side so the slope can crossfade.
    struct CutFilter
    {
        fancy::dsp::Svf one;
        std::array<fancy::dsp::Svf, 2> two;

        void reset() noexcept;
        double process (const std::array<fancy::dsp::SvfCoefficients, 3>& c, double x, double steep, bool lowCut) noexcept;
    };

    enum Cut { lowCutIndex, highCutIndex };

    struct CutState
    {
        std::array<CutFilter, 2> pre, post;   // per channel
        std::array<fancy::dsp::SvfCoefficients, 3> coefficients;   // Q 0.7071; then 0.5412 and 1.3066
        fancy::dsp::Smoother logHz, on, steep, postAmount;
        bool running = false;
    };

    void updateCutCoefficients (int cut, bool force);

    // Phase 4 Wave 2, item 38: Olli Niemitalo's 90-degree allpass pair, and the added part's best
    // linear fit to the clean signal, which Spread leaves where it is.
    struct HilbertChain
    {
        std::array<double, 4> x1 {}, x2 {}, y1 {}, y2 {};
        double process (const std::array<double, 4>& a2, double x) noexcept;
        void reset() noexcept { x1.fill (0.0); x2.fill (0.0); y1.fill (0.0); y2.fill (0.0); }
    };

    struct SpreadState
    {
        HilbertChain a, b;
        double aDelay = 0.0;
        double addedTimesClean = 0.0, cleanSquared = 0.0;
        void reset() noexcept { a.reset(); b.reset(); aDelay = addedTimesClean = cleanSquared = 0.0; }
    };

    void resetOuter (int band);
    void saturateOuterBaseRate (int band, int numSamples);
    void saturateOuterOversampled (int band, int numSamples);

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

    // Phase 4 Wave 2: the outer bands, [band][channel], their buffers (input, then added) and
    // per-sample settings, and whether each runs.
    std::array<std::array<OuterState, 2>, 2> outerStates;
    std::array<juce::AudioBuffer<float>, 2> outerBuffers;
    std::array<std::array<double, maxChunk>, 2> outerDrive {}, outerGain {}, outerTape {};
    std::array<bool, 2> outerWanted { false, false }, outerRunning { false, false };
    double lastT = 1.0, lastCharacter = 0.0, lastTone = 0.0, lastAuto = 1.0;
    bool prepared = false;   // the oversamplers exist (setTargets can come before prepare)

    std::array<fancy::dsp::SvfCoefficients, numSections> lowCoefficients, highCoefficients;
    std::array<double, numSections> sectionQ {};

    // Per input sample of the chunk.
    std::array<fancy::dsp::saturation::Curve, maxChunk> chunkCurve;
    std::array<bool, maxChunk> chunkShapeChanged {};
    std::array<double, maxChunk> chunkDc {}, chunkDrive {}, chunkGain {}, chunkTape {};
    std::array<fancy::dsp::saturation::TiltCoefficients, maxChunk> chunkTilt;

    fancy::dsp::saturation::Curve lastCurve;

    // The last sample's settings, reused while Type, Drive, Character, Tone and Auto gain are
    // all settled.
    bool settingsCached = false;
    fancy::dsp::saturation::Curve cachedCurve;
    double cachedDc = 0.0, cachedDrive = 1.0, cachedGain = 1.0, cachedTape = 0.0;
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

    // Phase 4 Wave 2.
    std::array<fancy::dsp::Smoother, 2> outerDriveDb, outerMix, outerOn;

    // Phase 4 Wave 2, items 35 to 39.
    fancy::dsp::Smoother biasOffset;
    std::array<CutState, 2> cuts;
    std::array<std::array<std::array<fancy::dsp::SvfCoefficients, 3>, maxChunk>, 2> chunkCutCoefficients {};
    std::array<std::array<double, maxChunk>, 2> chunkCutSteep {}, chunkCutPost {};
    std::array<SpreadState, 2> spreadStates;
    fancy::dsp::Smoother spreadAmount, spreadOn;
    bool spreadRunning = false;
    double spreadCoefficient = 0.0;

    // Channels: a change fades the added part out, switches once it's silent and the old setting
    // has left the oversampler, then fades back in.
    int activeChannels = stereo;
    fancy::dsp::Smoother channelsFade;
    int channelsHold = 0;
};
