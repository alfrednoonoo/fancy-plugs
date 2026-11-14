#pragma once

#include "EqProcessor.h"

#include <array>
#include <complex>
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

    // For rendered screenshots: a dynamic band's change, as if the detector had moved it.
    void showPreviewChange (int band, float changeDb) { previewBand = band; previewChangeDb = changeDb; }

    std::function<void (int)> onBandSelected;

    // From the band's right-click menu: type a value ("freq" or "gain"), or delete the band.
    std::function<void (int band, const juce::String& suffix)> onTypeValue;
    std::function<void (int band)> onDeleteBand;

    void showBandMenu (int band, juce::Point<float> position);
    juce::Point<float> getHandlePosition (int band) const { return bands[(size_t) band].handle; }

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

        // Phase 2.1 D: a dynamic band's reach (its curve at Gain + Range) and current change.
        bool dynamic = false;
        std::vector<float> reach;
        float reachDb = 0.0f, changeDb = 0.0f;
    };

    // What the graph last drew for a band's dynamics, to know when to draw again.
    struct DynamicState
    {
        bool dynamic = false;
        float rangeDb = 0.0f, changeDb = 0.0f;

        bool operator== (const DynamicState&) const = default;
    };

    void recalculate();
    static juce::String formatDbText (float db);
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
    void setValue (int band, const char* suffix, float value);
    juce::RangedAudioParameter& bandParameter (int band, const char* suffix);

    EqProcessor& processor;
    AnalyzerThread analyzerThread;

    std::array<BandDisplay, eq::maxBands> bands;
    std::vector<float> composite, phase;

    // Phase 3 Q2.4: with any band off Stereo, the total is drawn for one channel: the selected
    // band's, or Mid for a Stereo band.
    bool mixedChannels = false;
    eq::Channel totalChannel = eq::Channel::mid;
    bool lastStereoBus = true;
    std::array<eq::BandSpec, eq::maxBands> lastSpecs;
    std::array<bool, eq::maxBands> lastUsed {};
    std::array<DynamicState, eq::maxBands> lastDynamics {};
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
    int previewBand = -1;
    float previewChangeDb = 0.0f;
    bool draggingQ = false;
    juce::Point<float> dragPosition, lastMouse;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqGraph)
};
