#pragma once

#include "ExciterDsp.h"

#include <fancy/dsp/OutputCeiling.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/plugin/FancyProcessor.h>

#include <array>
#include <atomic>

class ExciterProcessor final : public fancy::FancyProcessor
{
public:
    ExciterProcessor();

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    // Phase 4 Wave 1 (docs/exciter.md): what a private copy of the path does at these settings.
    // THD of a -12 dBFS tone an octave above Freq (Decision 1 A), and the gain it gives pink
    // noise at -18 dBFS RMS, which Auto gain cancels. Deterministic.
    struct Analysis
    {
        float thdPercent = 0.0f, gainDb = 0.0f;
    };

    static Analysis analyse (const ExciterDsp::Targets& targets);

    // Phase 4 Wave 2: Freq 2's floor, and the High cut's range, Off past its top.
    static constexpr float freq2MinHz = 100.0f;
    static constexpr float highCutMinHz = 4000.0f, highCutMaxHz = 20000.0f, highCutOffHz = 25000.0f;

    float getThdPercent() const noexcept { return thdPercent.load (std::memory_order_relaxed); }

private:
    ExciterDsp::Targets readTargets() const;

    // The settings the analysis depends on, to know when to run it again.
    using AnalysisKey = std::array<double, 10>;
    static AnalysisKey keyFor (const ExciterDsp::Targets& targets) noexcept;
    void runAnalysis();

    class AnalysisThread final : public juce::Thread
    {
    public:
        explicit AnalysisThread (ExciterProcessor& owner) : Thread ("Fancy Exciter analysis"), processor (owner) {}
        void run() override;

    private:
        ExciterProcessor& processor;
    };

    AnalysisThread analysisThread { *this };
    juce::SpinLock analysisLock;
    AnalysisKey analysedKey {};
    bool analysed = false;
    std::atomic<float> thdPercent { 0.0f }, autoGainDb { 0.0f };
    fancy::dsp::Smoother autoTrim;
    fancy::dsp::OutputCeiling ceiling;

    ExciterDsp dsp;

public:
    ~ExciterProcessor() override;

private:

    std::atomic<float>& freq;
    std::atomic<float>& clipBoost;
    std::atomic<float>& harmonics;
    std::atomic<float>& mixBack;
    std::atomic<float>& oversampling;
    std::atomic<float>& character;
    std::atomic<float>& output;
    std::atomic<float>& bypass;

    // Phase 4 Wave 2.
    std::atomic<float>& freq2;
    std::atomic<float>& harmonics2;
    std::atomic<float>& punch;
    std::atomic<float>& highCut;
    std::atomic<float>& width;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExciterProcessor)
};
