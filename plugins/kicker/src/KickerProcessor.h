#pragma once

#include "KickerDsp.h"
#include "KickerDucker.h"

#include <fancy/dsp/OutputCeiling.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/SwitchedOversampler.h>
#include <fancy/dsp/TempoTrigger.h>
#include <fancy/plugin/FancyProcessor.h>

#include <array>
#include <atomic>
#include <vector>

class KickerProcessor final : public fancy::FancyProcessor
{
public:
    KickerProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    ~KickerProcessor() override;

    // Phase 4 Wave 1 (docs/kicker.md).
    float takeSubPeak() noexcept { return dsp.takeSubPeak(); }

    // The gain these settings give a reference kick, in dB, which Auto gain cancels. Deterministic.
    static float measureGainDb (const KickerDsp::Targets& targets);

    // Phase 4 (docs/kicker.md): Duck from's choices, and Host sync's grid (Gate's list).
    enum DuckFrom { duckFromSidechain, duckFromHostSync };
    static const juce::StringArray& syncRateNames();
    static double syncStepBeats (int index) noexcept;
    static constexpr int defaultSyncRate = 6; // 1/4

    // The duck applied in the last block, in dB, for the UI and tests.
    float getDuckDb() const noexcept { return duckDb.load (std::memory_order_relaxed); }

private:
    KickerDsp::Targets readTargets() const;
    float read (const char* parameterId) const;

    // Phase 4: the Duck's gain and the oversampled clipper, a chunk at a time.
    static constexpr int maxChunk = 512;
    void processChunk (float* const* channels, int numChannels, int numSamples, const float* const* sidechain, int numSidechain,
                       bool ducking, bool fromHostSync, int clipFactor);
    fancy::dsp::TempoTrigger::Position readPosition();

    // Phase 4 Wave 1: Auto gain's measure, on a background thread (synchronous offline).
    using AnalysisKey = std::array<double, 14>;
    static AnalysisKey keyFor (const KickerDsp::Targets& targets) noexcept;
    void runAnalysis();

    class AnalysisThread final : public juce::Thread
    {
    public:
        explicit AnalysisThread (KickerProcessor& owner) : Thread ("Fancy Kicker analysis"), processor (owner) {}
        void run() override;

    private:
        KickerProcessor& processor;
    };

    AnalysisThread analysisThread { *this };
    juce::SpinLock analysisLock;
    AnalysisKey analysedKey {};
    bool analysed = false;
    std::atomic<float> autoGainDb { 0.0f };
    fancy::dsp::Smoother autoTrim;
    fancy::dsp::OutputCeiling ceiling;

    KickerDsp dsp;

    // Phase 4: Duck, Host sync and Clip oversampling.
    KickerDucker ducker;
    fancy::dsp::TempoTrigger trigger;
    fancy::dsp::SwitchedOversampler clipOversampler;
    std::vector<float> keyBuffer, gainBuffer, clipBuffer, bypassBuffer;
    juce::AudioBuffer<float> dryBuffer;
    std::array<std::vector<float>, 2> dryDelay;
    std::vector<float> bypassDelay;
    int delayWrite = 0;
    bool clipStageIdle = true;
    std::atomic<float> duckDb { 0.0f };

    std::atomic<float>& freq;
    std::atomic<float>& wet;
    std::atomic<float>& dry;
    std::atomic<float>& q;
    std::atomic<float>& output;
    std::atomic<float>& monoBelow;
    std::atomic<float>& softClip;
    std::atomic<float>& bypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickerProcessor)
};
