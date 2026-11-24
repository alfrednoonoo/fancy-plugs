#pragma once

#include "FirLearn.h"
#include "FirProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

juce::Colour modeColour (int mode);

// Phase 4 Wave 2 (reference/phase4/59__fir_w2_mockup.png): two choices, the selected one filled in
// its colour. A view switch, not a parameter: Threshold | Ratio over the graph, Mid | Side beside
// the Mode row.
class TwoWaySwitch final : public juce::Component, public juce::SettableTooltipClient
{
public:
    TwoWaySwitch (const juce::String& first, const juce::String& second);

    void setColours (juce::Colour first, juce::Colour second);
    void setTextHeight (float height) { textHeight = height; }
    void setBackground (juce::Colour colour) { background = colour; }
    void setSelected (int index);
    int getSelected() const noexcept { return selected; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void (int)> onChange;

private:
    juce::Rectangle<float> segmentBounds (int index) const;

    juce::String names[2];
    juce::Colour fills[2];
    juce::Colour background;
    float textHeight = 12.0f;
    int selected = 0;
};

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

    // Phase 4 Wave 2: for rendered screenshots, a harsh band at 3 kHz and what Compressor mode's
    // curves take from it; and the point whose frequency and ratio are shown.
    void showHarshPreview();
    void selectPoint (int index) { selectedPoint = index; repaint(); }

    // Phase 4 Wave 2 (item 27): which of Compressor mode's curves the points edit.
    void setEditsRatio (bool ratio);

    // Phase 4 Wave 2: the threshold (Gate, Compressor) or EQ curve of the shown domain.
    int thresholdCurve() const noexcept;

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

    // Phase 4 Wave 2: the ratio curve's own scale, log from 0.5:1 at the bottom to 20:1 at the
    // top. Exposed for tests.
    static float yForRatio (double ratio, float height) noexcept;
    static double ratioForY (float y, float height) noexcept;

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

    // Phase 4 Wave 2: which curve the mode edits (Resonance: the depth curve; Compressor: the
    // threshold or the ratio curve; the side's in Mid/Side), and a curve's values on the graph,
    // which for the depth curve depend on Depth, Tilt and the frequency, and for the ratio curve
    // on the Ratio knob.
    int curve() const noexcept;
    double ceilingScale (double hz) const noexcept;
    float yForCurve (int curveIndex, double value, double hz) const noexcept;
    double valueForCurve (int curveIndex, float y, double hz) const noexcept;
    float yForValue (double value, double hz) const noexcept { return yForCurve (curve(), value, hz); }
    double valueAt (float y, double hz) const noexcept { return valueForCurve (curve(), y, hz); }
    juce::String formatValue (int curveIndex, double value) const;
    float yFor (float db) const noexcept;
    float dbAt (float y) const noexcept;
    int pointAt (juce::Point<float> position) const;
    double profileDbAt (double hz) const;

    void paintGrid (juce::Graphics&);
    void paintAnalysis (juce::Graphics&);
    void paintModeLayer (juce::Graphics&);
    void paintCurveLine (juce::Graphics&, int curveIndex, juce::Colour colour, bool edited, bool filled);
    void paintPoints (juce::Graphics&, int curveIndex, juce::Colour colour);
    void paintRatioScale (juce::Graphics&);
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
    float resDepthDb = 6.0f, resTilt = 0.0f; // Phase 4 Wave 2
    std::vector<float> previewReduction;

    // Phase 4 Wave 2 (items 27 and 28).
    TwoWaySwitch curveSwitch { "Threshold", "Ratio" };
    bool editsRatio = false, sideShown = false;
    double knobRatio = 2.0;
    int selectedPoint = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirGraph)
};
