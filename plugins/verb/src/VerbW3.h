#pragma once

#include <fancy/dsp/PitchShifter.h>
#include <fancy/dsp/Svf.h>

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace verb
{

// Phase 4 Wave 3 (docs/verb.md, "Freeze and shimmer", decision 1 A; modelled in
// docs/research/verb-freeze-model.py and checked in verb-w3-prepush.py).
//
// The wet signal is always written into a history ring. capture() takes the RMS magnitude of eight
// Hann frames (4096 points at 48 kHz, scaled with the rate, hop a quarter of that) per channel, and
// the last frame's phases. From then the spectrum is resynthesised one frame per hop: each bin turns
// by a new random step, the same for both channels, so the difference between them (the image)
// stays as it was captured. The steps come from a seeded generator, so offline renders repeat:
// one of stepCount equally spaced angles, from a table. Both channels go through one complex
// inverse FFT (left real, right imaginary), half the work of two real ones.
//
// prepare() allocates; push(), capture() and next() don't.
class SpectralFreeze
{
public:
    static constexpr int numFrames = 8;
    static constexpr int baseSize = 4096;
    static constexpr int stepBits = 10, stepCount = 1 << stepBits;

    void prepare (double sampleRate);
    void reset() noexcept;

    // The wet signal, every sample, frozen or not.
    void push (double left, double right) noexcept;

    // Analyses the last numFrames frames. The output starts at full level, as if it had been
    // playing for a whole frame already.
    void capture() noexcept;

    // The frozen sound, one sample per channel.
    void next (double& left, double& right) noexcept;

    int getSize() const noexcept { return size; }
    int getHop() const noexcept { return hop; }

private:
    void addFrame (int offset) noexcept; // a new frame, starting `offset` hops before now
    int nextStep() noexcept;

    int size = baseSize, hop = baseSize / 4, bins = baseSize / 2 + 1;
    int historySize = 1, historyMask = 0, historyWrite = 0;
    int position = 0;
    double scale = 1.0, inverseCorrection = 1.0, complexCorrection = 1.0;
    std::uint32_t seed = 0x9e3779b9u;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, buffer;
    std::vector<juce::dsp::Complex<float>> packed, unpacked;
    std::vector<double> stepCos, stepSin;
    std::array<std::vector<float>, 2> history, accumulator;
    std::array<std::vector<float>, 2> magnitude;
    std::array<std::vector<double>, 2> phaseRe, phaseIm;
};

// Phase 4 Wave 3: Shimmer's loop, wet = IR * (input + f), f = g x Shift(HP(wet)) (docs/verb.md,
// section 3). What the loop makes from the wet at sample t is added to the convolver's input at
// t + getLoopDelay(), so the convolver can run in pieces of up to that length and the loop never
// depends on the host's block size.
//
// The guard keeps the feedback's 50 ms RMS under the convolver input's loudest 50 ms RMS, held for
// the IR's RT60 and then let go at 60 dB per RT60, with a gain that pulls down in 10 ms.
class ShimmerLoop
{
public:
    static constexpr double baseDelay = 1024.0; // samples at 48 kHz
    static constexpr double highpassHz = 200.0;
    static constexpr double rmsMs = 50.0, attackMs = 10.0, releaseMs = 100.0;

    void prepare (double sampleRate, int maxPiece);
    void reset() noexcept;

    int getLoopDelay() const noexcept { return loopDelay; }

    // Before the convolver: follows the input's level (for the guard) and adds the feedback that
    // falls due in these samples. numSamples is at most the loop delay.
    void addFeedback (float* left, float* right, int numSamples, double rt60Seconds) noexcept;

    // After the convolver, one wet sample at a time, in the order addFeedback() took them: index
    // is the sample's place in that piece. gain is the loop gain now.
    void feed (double left, double right, int index, double ratio, double gain) noexcept;

    // How much the guard is pulling down now (1: not at all), for tests.
    double getGuardGain() const noexcept { return guardGain; }

private:
    double rate = 48000.0;
    int loopDelay = 1024, ringSize = 1, ringMask = 0, position = 0;

    std::array<std::vector<float>, 2> ring;
    std::vector<float> inputPeak; // the guard's reference, per sample of the piece

    fancy::dsp::SvfCoefficients highpass;
    std::array<fancy::dsp::Svf, 2> highpassFilters;
    fancy::dsp::PitchShifter shifter;

    double rmsCoefficient = 0.0, attackCoefficient = 0.0, releaseCoefficient = 0.0;
    double inputEnvelope = 0.0, feedbackEnvelope = 0.0, peak = 0.0, guardGain = 1.0;
    int holdLeft = 0;
};

} // namespace verb
