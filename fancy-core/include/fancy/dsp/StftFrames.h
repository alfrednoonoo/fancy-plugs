#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

namespace fancy::dsp
{

// Short-time Fourier framing, shared by Fancy FIR's spectral engine and Fancy EQ's spectral
// dynamics (docs/eq.md, "Phase 4 Wave 3", decision 5 A). Moved out of FIR's SpectralEngine as it
// was, so FIR renders as before to the bit.
//
// It holds the input rings, the analysis and synthesis windows, the FFT and the output rings, and
// decides when a frame is due. The owner does the frame's work in stages: advance() calls back
// runStage (0 to numStages - 1), spread over `extra` samples after the frame's end (all at once
// when extra is 0), and startFrame when a new frame begins. A frame's stages always finish before
// the next frame starts.
//
// Windows (not convolving): square-root Hann both ways, overlap-adding to 1 at a hop of a quarter
// of the frame; or, for low latency, an asymmetric pair whose product is a Hann of half the frame
// over its last part. Convolving: no windows, frames of frameLength zero-padded to twice that,
// a hop of frameLength.
//
// prepare() allocates; everything else doesn't.
class StftFrames
{
public:
    static constexpr int maxInputs = 4, maxOutputs = 2;

    struct Layout
    {
        int frameLength = 1024;
        bool lowLatency = false;
        bool convolve = false;
        int extra = 0;     // samples the stages are spread over, added to the latency
        int numStages = 1;
    };

    static int latencyOf (const Layout& layout) noexcept
    {
        if (layout.convolve)
            return layout.frameLength + layout.extra;

        return (layout.lowLatency ? layout.frameLength / 2 : layout.frameLength) + layout.extra;
    }

    void prepare (const Layout& newLayout)
    {
        layout = newLayout;
        frameLength = layout.frameLength;
        fftLength = layout.convolve ? frameLength * 2 : frameLength;
        bins = fftLength / 2 + 1;
        hop = layout.convolve ? frameLength : frameLength / 4;
        synthesisLength = layout.convolve ? frameLength : (layout.lowLatency ? frameLength / 2 : frameLength);
        extra = layout.extra;
        latency = synthesisLength + extra;
        numStages = std::max (1, layout.numStages);
        stageInterval = extra / numStages;

        fft = std::make_unique<juce::dsp::FFT> (juce::roundToInt (std::log2 ((double) fftLength)));

        analysisWindow.assign ((size_t) frameLength, 1.0f);
        synthesisWindow.assign ((size_t) frameLength, 1.0f);

        if (! layout.convolve)
        {
            if (layout.lowLatency)
            {
                // An asymmetric pair: their product is a Hann of synthesisLength over the last
                // part of the frame, which overlap-adds to 1 at a hop of half that.
                const auto shortLength = synthesisLength;
                const auto rise = frameLength - shortLength / 2;

                for (int n = 0; n < frameLength; ++n)
                {
                    const auto analysis = n < rise ? std::sqrt (hann (n, 2 * rise))
                                                   : std::sqrt (hann (n - (frameLength - shortLength), shortLength));

                    const auto product = n < frameLength - shortLength ? 0.0 : hann (n - (frameLength - shortLength), shortLength);

                    analysisWindow[(size_t) n] = (float) analysis;
                    synthesisWindow[(size_t) n] = analysis > 1.0e-9 ? (float) (product / analysis) : 0.0f;
                }
            }
            else
            {
                // Square-root Hann both ways overlap-add to 2 at a quarter of the frame.
                for (int n = 0; n < frameLength; ++n)
                {
                    const auto root = std::sqrt (hann (n, frameLength));

                    analysisWindow[(size_t) n] = (float) root;
                    synthesisWindow[(size_t) n] = (float) (0.5 * root);
                }
            }
        }

        auto windowSum = 0.0, windowEnergy = 0.0;

        for (const auto value : analysisWindow)
        {
            windowSum += (double) value;
            windowEnergy += (double) value * (double) value;
        }

        // A sine at amplitude A reads A, whatever the window and size.
        levelScale = 2.0 / windowSum;
        powerScale = 1.0 / windowEnergy;

        inputSize = nextPowerOfTwo (frameLength + extra + 8);
        inputMask = inputSize - 1;
        outputSize = nextPowerOfTwo (fftLength + latency + hop + 8);
        outputMask = outputSize - 1;

        for (auto& ring : inputRings)
            ring.assign ((size_t) inputSize, 0.0f);

        for (auto& ring : outputRings)
            ring.assign ((size_t) outputSize, 0.0f);

        reset();
    }

    void reset() noexcept
    {
        for (auto& ring : inputRings)
            std::fill (ring.begin(), ring.end(), 0.0f);

        for (auto& ring : outputRings)
            std::fill (ring.begin(), ring.end(), 0.0f);

        time = 0;
        frameEnd = -1;
        nextStage = numStages;
    }

    int getFrameLength() const noexcept { return frameLength; }
    int getFftLength() const noexcept { return fftLength; }
    int getBins() const noexcept { return bins; }
    int getHop() const noexcept { return hop; }
    int getLatency() const noexcept { return latency; }
    double getLevelScale() const noexcept { return levelScale; }
    double getPowerScale() const noexcept { return powerScale; }
    long long getFrameEnd() const noexcept { return frameEnd; }

    // A spectrum buffer the size the FFT needs: 2 x fftLength floats.
    int getSpectrumSize() const noexcept { return 2 * fftLength; }

    // This sample of one input (0 to 3), before advance().
    void write (int input, float value) noexcept
    {
        inputRings[(size_t) input][(size_t) (time & inputMask)] = value;
    }

    // One sample on. Starts a frame when one is due (finishing the last one's stages first), and
    // runs the stages whose time has come.
    template <typename RunStage, typename StartFrame>
    void advance (RunStage&& runStage, StartFrame&& startFrame) noexcept
    {
        ++time;

        // A frame is ready every hop, once the first frame has filled.
        if (time >= frameLength && (time - frameLength) % hop == 0)
        {
            while (nextStage < numStages)
                runStage (nextStage++);

            frameEnd = time;
            nextStage = 0;
            startFrame();
        }

        while (nextStage < numStages && time >= frameEnd + (long long) (nextStage * stageInterval))
            runStage (nextStage++);
    }

    // The output sample that's now a full latency old (after advance()), and clearing it for
    // later frames.
    float read (int output) const noexcept
    {
        return outputRings[(size_t) output][(size_t) ((time - 1 - latency) & outputMask)];
    }

    void clear (int output) noexcept
    {
        outputRings[(size_t) output][(size_t) ((time - 1 - latency) & outputMask)] = 0.0f;
    }

    // The current frame of one input, windowed, into spectrum (getSpectrumSize() floats), and its
    // forward transform: bins complex values, interleaved.
    void analyse (int input, std::vector<float>& spectrum) noexcept
    {
        const auto& ring = inputRings[(size_t) input];

        // The frame ends at frameEnd, exclusive.
        for (int n = 0; n < frameLength; ++n)
            spectrum[(size_t) n] = ring[(size_t) ((frameEnd - frameLength + n) & inputMask)] * analysisWindow[(size_t) n];

        std::fill (spectrum.begin() + frameLength, spectrum.end(), 0.0f);

        fft->performRealOnlyForwardTransform (spectrum.data(), true);
    }

    // The inverse transform of spectrum, windowed and overlap-added into one output.
    void synthesise (int output, std::vector<float>& spectrum) noexcept
    {
        fft->performRealOnlyInverseTransform (spectrum.data());

        auto& ring = outputRings[(size_t) output];

        for (int n = 0; n < frameLength; ++n)
            ring[(size_t) ((frameEnd - frameLength + n) & outputMask)] += spectrum[(size_t) n] * synthesisWindow[(size_t) n];
    }

    // Convolving: the inverse transform of spectrum, all fftLength samples added unwindowed to
    // every output given.
    void synthesiseUnwindowed (std::vector<float>& spectrum, int firstOutput, int lastOutput) noexcept
    {
        fft->performRealOnlyInverseTransform (spectrum.data());

        for (int n = 0; n < fftLength; ++n)
        {
            const auto position = (size_t) ((frameEnd - frameLength + n) & outputMask);

            for (int output = firstOutput; output <= lastOutput; ++output)
                outputRings[(size_t) output][position] += spectrum[(size_t) n];
        }
    }

private:
    static int nextPowerOfTwo (int value) noexcept
    {
        auto size = 1;

        while (size < value)
            size <<= 1;

        return size;
    }

    static double hann (int n, int length) noexcept
    {
        return 0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * (double) n / (double) length);
    }

    Layout layout;
    int frameLength = 1024, fftLength = 1024, bins = 513, hop = 256, synthesisLength = 1024, extra = 0, latency = 1024;
    int numStages = 1, stageInterval = 0;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> analysisWindow, synthesisWindow;
    double levelScale = 1.0, powerScale = 1.0;

    int inputSize = 1, inputMask = 0, outputSize = 1, outputMask = 0;
    std::array<std::vector<float>, maxInputs> inputRings;
    std::array<std::vector<float>, maxOutputs> outputRings;
    long long time = 0;
    long long frameEnd = -1;
    int nextStage = 1;
};

} // namespace fancy::dsp
