#pragma once

#include "Convolver.h"
#include "VerbW3.h"

#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/WetDynamics.h>

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
        double width = 1.0;  // 0 to 1, or to 2 with Extra width (Phase 4 Wave 2)
        double wet = 1.0, dry = 1.0, output = 1.0;
        bool bypass = false;

        // Modulate's live settings (Phase 3 Q2.8), for a modulated convolver.
        double modRate = 0.3, modDepth = 0.5, modSpread = 1.0;

        // Phase 4 Wave 2 (docs/verb.md): Ducking and the Gate on the wet signal, from the main
        // input or the sidechain, delayed by the latency. At these defaults neither runs.
        double ducking = 0.0;   // 0 to 1
        bool duckSidechain = false;
        bool gate = false;
        double gateThresholdDb = -30.0, gateHoldMs = 80.0, gateDecayMs = 40.0;

        // Phase 4 Wave 3 (docs/verb.md): Freeze, and Shimmer's amount (0 to 1) and pitch ratio.
        // Off and 0 run neither, so the output is as before, to the bit.
        bool freeze = false;
        double shimmer = 0.0, shimmerRatio = 2.0;
    };

    ~VerbDsp();

    void prepare (double sampleRate, int latency);
    void reset() noexcept;

    void setTargets (const Targets& targets) noexcept { current = targets; }
    void snapToTargets (const Targets& targets) noexcept;

    void process (float* const* channels, int numChannels, int numSamples, bool offline) noexcept
    {
        process (channels, numChannels, nullptr, 0, numSamples, offline);
    }

    // Phase 4 Wave 2: with the sidechain, for Ducking and the Gate. numSidechain may be 0.
    void process (float* const* channels, int numChannels, const float* const* sidechain, int numSidechain, int numSamples, bool offline) noexcept;

    // The gate's gain now, for tests (1 open, 0 closed).
    double getGateGain() const noexcept { return wetGate.getGain(); }

    // Phase 4 Wave 3, for tests: the shimmer loop's guard gain, and whether Freeze is sounding.
    double getGuardGain() const noexcept { return loop.getGuardGain(); }
    bool isFreezeSounding() const noexcept { return freezeState != FreezeState::idle; }
    int getLoopDelay() const noexcept { return loop.getLoopDelay(); }

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
    bool loopWanted() const noexcept;
    void startFreeze() noexcept;
    void processChunk (float* const* channels, int numChannels, const float* const* sidechain, int numSidechain, int offset, int numSamples,
                       bool offline) noexcept;

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

    // Phase 4 Wave 2: the sidechain, delayed as the dry path is, and the two stages.
    std::array<std::vector<float>, maxChannels> keyRing;
    std::array<std::array<float, maxChunk>, maxChannels> delayedKey {};
    fancy::dsp::WetDucker ducker;
    fancy::dsp::WetGate wetGate;
    fancy::dsp::Smoother duckAmount;

    // Phase 4 Wave 3: Freeze and Shimmer (VerbW3.h).
    enum class FreezeState { idle, holding, releasing };
    static constexpr double freezeFadeMs = 200.0, inputFadeMs = 20.0;

    verb::SpectralFreeze freezer;
    verb::ShimmerLoop loop;
    FreezeState freezeState = FreezeState::idle;
    double freezeIn = 0.0;          // 0 to 1 over freezeFadeMs: the equal-power crossfade into the frozen sound
    double freezeLevel = 1.0;       // falls at the IR's RT60 once Freeze is off
    double liveGain = 1.0;          // the live wet, faded out under the frozen sound and back in on release
    double inputGain = 1.0;         // the convolver's input, muted while frozen
    bool loopRunning = false, snapLoopGain = true;
    int loopLeft = 0;               // samples of feedback still in the loop's ring
    fancy::dsp::Smoother loopGain;
};
