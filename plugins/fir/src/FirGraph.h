#pragma once

#include "FirLearn.h"
#include "FirProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

juce::Colour modeColour (int mode);

// The graph: input spectrum, output spectrum, and the curve, profile or gain reduction of
// the mode, with the curve's points to edit.
class FirGraph final : public juce::Component
{
public:
    // Prompts for a point's frequency and gain, or for the graph's top or analysis floor.
    void typePoint (int index);
    void typeAxis (bool top);

    explicit FirGraph (FirProcessor& processor);
    ~FirGraph() override;

    // 30 Hz, from the editor.
    void update();

    // Phase 4 Wave 1: Learn for the Gate (curve 1) and Compressor (curve 2) curves. The analyzer
    // thread listens for 3 s of input; update() then sets the curve, as one undo step.
    void startLearn (int curve);
    void cancelLearn();
    bool isLearningCurve() const noexcept { return learnState.load() != learnIdle; }
    float getCurveLearnProgress() const noexcept { return learnProgress.load(); }

    // For rendered screenshots: stops the analyzers and draws fixed spectra instead.
    // resonances: Phase 3 Q2.7's mockup, two resonances and their reduction.
    void showPreviewSpectra (bool resonances = false);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // Exposed for tests.
    static float xForHz (double hz, float width) noexcept;
    static double hzForX (float x, float width) noexcept;
    static float yForDb (float db, float height, float topDb, float floorDb) noexcept;
    static float dbForY (float y, float height, float topDb, float floorDb) noexcept;

private:
    class AnalyzerThread final : public juce::Thread
    {
    public:
        explicit AnalyzerThread (FirGraph& owner) : Thread ("Fancy FIR analyzer"), graph (owner) {}

        void run() override;

    private:
        FirGraph& graph;
    };

    void analyse (fancy::dsp::SpectrumAnalyzer& before, fancy::dsp::SpectrumAnalyzer& after, std::vector<float>& scratch);

    bool editsCurve() const noexcept;
    float yFor (float db) const noexcept;
    float dbAt (float y) const noexcept;
    int pointAt (juce::Point<float> position) const;
    double profileDbAt (double hz) const;

    void paintGrid (juce::Graphics&);
    void paintAnalysis (juce::Graphics&);
    void paintModeLayer (juce::Graphics&);
    void paintAdaptive (juce::Graphics&);
    bool showsAdaptive() const;
    void paintLegend (juce::Graphics&);


    void drawFreehand (juce::Point<float> from, juce::Point<float> to);

    FirProcessor& processor;
    AnalyzerThread analyzerThread;

    int mode = 0, editMode = 0;
    float topDb = 24.0f, floorDb = -90.0f;
    bool showAnalysis = true;

    int dragPoint = -1;
    bool draggingCurve = false, drawing = false;
    juce::Point<float> lastMouse;
    double dragStartHz = 0.0;
    std::vector<fir::CurvePoint> dragStartPoints;
    std::array<float, fir::curvePoints> dragStartTable {};

    juce::SpinLock spectrumLock;
    std::vector<float> inputSpectrum, outputSpectrum;
    std::atomic<int> columns { 0 };
    bool frozenSpectra = false;

    // Phase 4 Wave 1: Learn's state, shared with the analyzer thread.
    enum { learnIdle, learnRequested, learnRunning, learnDone };
    fir::CurveLearner learner;
    std::vector<fir::CurvePoint> learnedPoints;
    std::atomic<int> learnState { learnIdle }, learnCurve { 1 };
    std::atomic<float> learnProgress { 0.0f };

    // Phase 3 Q2.7: Resonance's range, and the preview's reduction per display column.
    float resLowHz = 100.0f, resHighHz = 20000.0f;
    std::vector<float> previewReduction;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirGraph)
};
