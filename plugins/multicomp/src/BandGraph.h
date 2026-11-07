#pragma once

#include "MultiCompProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <vector>

juce::Colour multiBandColour (int band);

// The graph: band regions up to their thresholds, crossover lines, gain reduction hanging from
// the top and a live spectrum.
class BandGraph final : public juce::Component
{
public:
    explicit BandGraph (MultiCompProcessor& processor);
    ~BandGraph() override;

    // 30 Hz, from the editor.
    void update (const std::array<float, multicomp::maxBands>& gainReduction);

    void setSelectedBand (int band);

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

    // Exposed for tests.
    static float xForHz (double hz, float width) noexcept;
    static double hzForX (float x, float width) noexcept;
    static float yForDb (float db, float height) noexcept;
    static float dbForY (float y, float height) noexcept;

private:
    class AnalyzerThread final : public juce::Thread
    {
    public:
        explicit AnalyzerThread (BandGraph& owner) : Thread ("Fancy MultiComp analyzer"), graph (owner) {}

        void run() override;

    private:
        BandGraph& graph;
    };

    enum class Drag { none, threshold, ratio, crossover };

    void analyse (fancy::dsp::SpectrumAnalyzer& analyzer, std::vector<float>& scratch);

    juce::Point<float> handleOf (int band) const;
    int bandAt (juce::Point<float> position) const;
    int crossoverAt (juce::Point<float> position) const;
    int bandUnder (float x) const;
    void select (int band);
    void changeRatio (int band, float distance);

    void paintGrid (juce::Graphics&);
    void paintSpectrum (juce::Graphics&);

    MultiCompProcessor& processor;
    AnalyzerThread analyzerThread;

    std::array<float, multicomp::maxBands> reduction {};
    int selectedBand = 0;
    bool showGrid = true, showSpectrum = true;

    Drag drag = Drag::none;
    int dragTarget = -1;
    float lastDragY = 0.0f;

    juce::SpinLock spectrumLock;
    std::vector<float> spectrum;
    std::atomic<int> columns { 0 };
    bool frozenSpectrum = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandGraph)
};
