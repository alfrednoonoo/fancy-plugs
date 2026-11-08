#pragma once

#include "Curve.h"
#include "FirDsp.h"

#include <fancy/dsp/Analyzer.h>
#include <fancy/plugin/FancyProcessor.h>

#include <array>
#include <atomic>
#include <vector>

class FirProcessor final : public fancy::FancyProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    FirProcessor();
    ~FirProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    bool waitForBackgroundWork (int timeoutMs) override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    void stateApplied() override;
    void factoryPresetLoaded (const juce::String& data) override;
    void addToState (juce::ValueTree& state) override;

    FirDsp::Targets readTargets() const;
    int getMode() const;
    int getEditMode() const;
    int getFftSize() const;
    int getLatency() const noexcept { return dsp.getLatencySamples(); }

    fir::Shared& getShared() noexcept { return dsp.shared; }

    // Message thread. Curves exist for EQ, Gate and Compressor.
    const std::vector<fir::CurvePoint>& getPoints (int curve) const { return points[(size_t) curve]; }
    const std::array<float, fir::curvePoints>& getFreehand (int curve) const { return freehand[(size_t) curve]; }
    void setPoints (int curve, std::vector<fir::CurvePoint> newPoints);
    void setFreehand (int curve, const std::array<float, fir::curvePoints>& table);
    void resetCurve (int curve);
    double curveDbAt (int curve, double hz) const;

    // Message thread.
    void startLearning();
    void clearProfile();
    bool isLearning() const noexcept { return dsp.shared.learning.load() || dsp.shared.learnRequested.load(); }
    float getLearnProgress() const noexcept;

    // The analysis feeds. The editor turns them on only while it's open.
    fancy::dsp::SampleFifo inputFifo, outputFifo;
    std::atomic<bool> analyzerActive { false };

private:
    class BuildThread final : public juce::Thread
    {
    public:
        explicit BuildThread (FirProcessor& processorToUse)
            : Thread ("Fancy FIR engine builder"), processor (processorToUse) {}

        void run() override;

    private:
        FirProcessor& processor;
    };

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    float read (const char* parameterId) const;
    bool isOn (const char* parameterId) const;

    void publishCurve (int curve);
    void storeCurves();
    void loadCurves (const juce::ValueTree& tree);
    void loadProfile (const juce::ValueTree& tree);

    std::array<std::vector<fir::CurvePoint>, fir::numCurves> points;
    std::array<std::array<float, fir::curvePoints>, fir::numCurves> freehand {};

    FirDsp dsp;
    BuildThread buildThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirProcessor)
};
