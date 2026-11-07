#pragma once

#include "EqProcessor.h"

#include <array>
#include <functional>
#include <vector>

// Figma's axis fits log2 (f + 40), running from 20 Hz to 24 kHz.
double xForHz (double hz, double width) noexcept;
double hzForX (double x, double width) noexcept;

juce::Colour bandColour (int band);

// The graph, its band handles and the analyzer line.
class EqGraph final : public juce::Component
{
public:
    explicit EqGraph (EqProcessor& processor);
    ~EqGraph() override;

    // 30 Hz, from the editor. Recalculates the curves if a parameter has changed.
    void update();

    void setDisplayRange (float rangeDb);
    void setSelectedBand (int band);
    int getSelectedBand() const noexcept { return selectedBand; }

    // For rendered screenshots: stops the analyzer and draws a fixed spectrum instead.
    void showPreviewSpectrum();

    std::function<void (int)> onBandSelected;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    class AnalyzerThread final : public juce::Thread
    {
    public:
        explicit AnalyzerThread (EqGraph& owner) : Thread ("Fancy EQ analyzer"), graph (owner) {}

        void run() override;

    private:
        EqGraph& graph;
    };

    struct BandDisplay
    {
        eq::BandSpec spec;
        std::vector<float> db;
        juce::Point<float> handle;
        bool used = false;
    };

    void recalculate();
    void analyse (fancy::dsp::SpectrumAnalyzer& analyzer, std::vector<float>& scratch);

    float yForDb (float db) const noexcept;
    float dbForY (float y) const noexcept;
    float yForSpectrum (float db) const noexcept;
    float spectrumForDb (float db) const noexcept;

    void paintGrid (juce::Graphics&);
    void paintLabels (juce::Graphics&);
    void paintSpectrum (juce::Graphics&);
    void paintBands (juce::Graphics&);
    void paintHandles (juce::Graphics&);

    int bandAt (juce::Point<float> position) const noexcept;
    void select (int band);
    void showBandMenu (int band);
    void setValue (int band, const char* suffix, float value);
    juce::RangedAudioParameter& bandParameter (int band, const char* suffix);

    EqProcessor& processor;
    AnalyzerThread analyzerThread;

    std::array<BandDisplay, eq::maxBands> bands;
    std::vector<float> composite, phase;
    std::array<eq::BandSpec, eq::maxBands> lastSpecs;
    std::array<bool, eq::maxBands> lastUsed {};
    double lastRate = 0.0;
    float rangeDb = 12.0f;
    bool needsRecalculation = true;
    bool showGrid = true, showPhase = false, showPeak = false;
    int analyzerMode = 2;

    juce::SpinLock spectrumLock;
    std::vector<float> spectrum, peaks;
    std::atomic<int> columns { 0 };
    bool frozenSpectrum = false;

    int selectedBand = 0;
    int dragBand = -1;
    bool draggingQ = false;
    juce::Point<float> dragPosition, lastMouse;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqGraph)
};
