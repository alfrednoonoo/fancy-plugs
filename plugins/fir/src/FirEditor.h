#pragma once

#include "FirGraph.h"
#include "FirProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <memory>
#include <vector>

class FirEditor final : public fancy::ui::ShellEditor
{
public:
    explicit FirEditor (FirProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    enum class Preview { subtractLearning, eqCurve, drawer, subtractAdaptive, resonance, resonanceRange,
                         compressorThreshold, compressorRatioSide }; // Phase 4 Wave 2, mockup 59

    // For rendered screenshots.
    void showPreview (bool drawerOpen);
    void showPreview (Preview preview);

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    // Noise profile: Learn, its progress, and Clear and Re-learn once there's a profile.
    class LearnCard final : public juce::Component
    {
    public:
        explicit LearnCard (FirProcessor& processor);

        void update();
        void showLearning (float seconds, float total);

        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

        // For rendered screenshots: Adaptive, tracking at -52 dB, holding.
        void showTracking (float floorDb, bool held);

    private:
        juce::Rectangle<int> learnButtonBounds() const;
        void paintTracking (juce::Graphics&, int x, bool compact);

        FirProcessor& firProcessor;
        fancy::ui::PillButton clearButton { "Clear" }, relearnButton { "Re-learn" };

        // Phase 2.1 A: Learned | Adaptive | Both, Adapt speed, and Both's Learn button.
        fancy::ui::ParamSegments sourceSwitch;
        fancy::ui::Knob adaptKnob;
        fancy::ui::PillButton bothLearnButton { "Learn" };
        int source = fir::learnedSource;
        float floorDb = -200.0f;
        bool held = false, tracking = false, frozen = false;

        bool learning = false, hasProfile = false;
        float progress = 0.0f, seconds = 0.0f, totalSeconds = 2.0f, capturedSeconds = 0.0f;
    };

    void onTick (float seconds) override;
    juce::Rectangle<int> getSidechainNoticeBounds() override;
    int getShownInputs() override { return juce::jmax (getFancyProcessor().getTotalNumInputChannels(), getFancyProcessor().getMainBusNumInputChannels() + 2); }

    FirProcessor& firProcessor() noexcept;
    void updateMode();

    Panels panels;
    juce::Label latencyLabel, modeLabel, editLabel;
    fancy::ui::ParamSegments modeSwitch;
    fancy::ui::ParamDropdown editDropdown;

    // Phase 4 Wave 1 (reference/phase4/41__fir_w1_mockup.png): Learn for the Gate and Compressor
    // curves, and Resonance's Tilt.
    fancy::ui::PillButton curveLearnButton { "Learn" };
    fancy::ui::PillButton resetDepthButton { "Reset curve" }; // Phase 4 Wave 2: the depth curve
    fancy::ui::Knob resTiltKnob;

    // Phase 4 Wave 2 (reference/phase4/59__fir_w2_mockup.png): Mid | Side beside the Mode row
    // while Mid/Side is on, Reset ratio curve and its note in Compressor mode, and the Mid/Side
    // checkbox in the drawer.
    TwoWaySwitch midSideSwitch { "Mid", "Side" };
    fancy::ui::PillButton resetRatioButton { "Reset ratio curve" };
    juce::Label ratioHint, midSideText;

    FirGraph graph;

    // Left card, one per mode.
    LearnCard learnCard;
    juce::Label curveCaption, convolveText;
    fancy::ui::PillButton resetButton { "Reset curve" }, flatButton { "Flat" }, tiltButton { "Tilt" }, demudButton { "De-mud" };

    // Mode controls card.
    fancy::ui::Knob floorKnob, gateAttackKnob, gateReleaseKnob;
    fancy::ui::Knob ratioKnob, compAttackKnob, compReleaseKnob;
    fancy::ui::Knob reductionKnob, offsetKnob, smoothingKnob, subReleaseKnob;

    // Phase 3 Q2.7: Resonance mode's Range card and knobs.
    juce::Label rangeCaption, rangeLowLabel, rangeHighLabel;
    fancy::ui::Knob resLowKnob, resHighKnob;
    fancy::ui::Knob resDepthKnob, resSharpnessKnob, resAttackKnob, resReleaseKnob;
    juce::Label fftLabel, eqHint;
    fancy::ui::ParamDropdown fftDropdown;
    fancy::ui::ParamCheckbox reduceCheckbox;

    fancy::ui::ParamCheckbox analysisCheckbox, lowLatencyCheckbox, sidechainCheckbox;
    fancy::ui::ListenCheckbox listenNoiseCheckbox, listenReducedCheckbox, listenResonanceCheckbox;
    fancy::ui::EditLink editLink;

    fancy::ui::OutMeter outMeter;
    juce::Label gainLabel;
    fancy::ui::Fader gainFader;

    fancy::ui::EditDrawer drawer;
    fancy::ui::Knob learnLengthKnob, drawerOutputKnob;
    fancy::ui::ParamCheckbox midSideCheckbox;

    int shownMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirEditor)
};
