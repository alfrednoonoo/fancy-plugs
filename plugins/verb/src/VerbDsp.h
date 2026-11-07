#pragma once

#include "Convolver.h"

#include <fancy/dsp/Smoother.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

// The audio path: pre-delay, one or two convolvers, stereo image, Wet/Dry, Output and bypass.
// process() never allocates or locks. New convolvers arrive and old ones leave through two
// atomic pointers, so they're built and freed on the worker thread.
class VerbDsp
{
public:
    static constexpr int maxChannels = 2;

    struct Targets
    {
        double predelayMs = 0.0;
        double center = 0.0; // -1 to 1
        double width = 1.0;  // 0 to 1
        double wet = 1.0, dry = 1.0, output = 1.0;
        bool bypass = false;
    };

    ~VerbDsp();

    void prepare (double sampleRate, int latency);
    void reset() noexcept;

    void setTargets (const Targets& targets) noexcept { current = targets; }
    void snapToTargets (const Targets& targets) noexcept;

    void process (float* const* channels, int numChannels, int numSamples, bool offline) noexcept;

    // Worker thread. A convolver that was never picked up is replaced and freed.
    void offer (std::unique_ptr<verb::Convolver> convolver);
    std::unique_ptr<verb::Convolver> takeRetired() noexcept;

    bool hasConvolver() const noexcept { return active != nullptr || incoming.load() != nullptr; }
    int getLatency() const noexcept { return latency.load (std::memory_order_relaxed); }
    int getLateBlocks() const noexcept { return lateBlocks.load (std::memory_order_relaxed); }

private:
    static constexpr int maxChunk = 2048;
    static constexpr double crossfadeMs = 64.0;
    static constexpr double fadeInMs = 20.0;

    void dropConvolvers() noexcept;
    void acceptIncoming() noexcept;
    void startFade (double milliseconds) noexcept;
    void processChunk (float* const* channels, int numChannels, int offset, int numSamples, bool offline) noexcept;

    double sampleRate = 48000.0;
    Targets current;

    std::atomic<verb::Convolver*> incoming { nullptr };
    std::atomic<verb::Convolver*> retired { nullptr };
    verb::Convolver* active = nullptr;
    verb::Convolver* outgoing = nullptr;

    int fadeTotal = 1, fadeLeft = 0;

    fancy::dsp::Smoother predelay, center, width, wet, dry, output, bypass, switchGain;

    std::array<std::vector<float>, maxChannels> predelayRing, dryRing;
    int predelayLength = 1, predelayWrite = 0;
    int dryLength = 1, dryWrite = 0;

    std::atomic<int> latency { 0 };
    std::atomic<int> lateBlocks { 0 };

    std::array<std::array<float, maxChunk>, maxChannels> wetInput {}, wetNew {}, wetOld {}, delayedDry {};
};
