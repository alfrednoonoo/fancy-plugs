#pragma once

#include "KickerDsp.h"

#include <fancy/dsp/OutputCeiling.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/plugin/FancyProcessor.h>

#include <array>
#include <atomic>

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

private:
    KickerDsp::Targets readTargets() const;

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
