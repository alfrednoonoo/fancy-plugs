#pragma once

#include "SpectralEngine.h"

#include <fancy/dsp/Smoother.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

// Runs one spectral engine, and swaps to a new one when the size, Low latency or Convolve
// L/R changes. See docs/fir.md.
class FirDsp
{
public:
    using Config = fir::SpectralEngine::Config;

    struct Targets
    {
        fir::FrameSettings frame;
        Config engine;
        double gain = 1.0, outputGain = 1.0;
        bool bypass = false;
    };

    FirDsp();
    ~FirDsp();

    // Builds the first engine inline.
    void prepare (double sampleRate, const Targets& targets);
    void reset();

    void setTargets (const Targets& targets) noexcept;
    void process (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return active != nullptr ? active->getLatency() : 0; }

    // Worker thread: builds a requested engine and frees retired ones.
    void serviceBuilds();

    // Builds engines inline, so an offline render is repeatable.
    void setSynchronousBuilds (bool shouldBeSynchronous) noexcept { synchronous = shouldBeSynchronous; }

    // True while a new engine is being built, warmed up or faded in.
    bool isChanging() const noexcept { return incoming != nullptr || requested.load (std::memory_order_relaxed) >= 0; }

    fir::Shared shared;

private:
    static constexpr int maxChunk = 512;
    static constexpr int retireSlots = 4;

    static Config normalised (Config config) noexcept;
    static int pack (const Config& config) noexcept;
    static Config unpack (int packed) noexcept;

    void processChunk (float* const* main, int numMain, const float* const* sidechain, int numSidechain, int offset, int numSamples) noexcept;
    void updateEngines() noexcept;
    void retire (fir::SpectralEngine* engine) noexcept;
    fir::SpectralEngine* build (const Config& config);

    double rate = 48000.0;
    Targets current;
    bool synchronous = false;

    fir::SpectralEngine* active = nullptr;
    fir::SpectralEngine* incoming = nullptr;
    int warmupLeft = 0;
    bool fading = false;
    fancy::dsp::Smoother fade;

    std::atomic<int> requested { -1 };
    std::atomic<fir::SpectralEngine*> built { nullptr };
    std::array<std::atomic<fir::SpectralEngine*>, retireSlots> retired {};
    fir::SpectralEngine* waitingToRetire = nullptr;

    std::vector<float> inputs[4], activeOut[2], incomingOut[2];
    std::vector<float> bypassDelay[2];
    int bypassLength = 1, bypassPosition = 0;

    fancy::dsp::Smoother gain, output, bypass;
};
