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

    enum class Preview { subtractLearning, eqCurve, drawer };

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

    private:
        juce::Rectangle<int> learnButtonBounds() const;

        FirProcessor& firProcessor;
        fancy::ui::PillButton clearButton { "Clear" }, relearnButton { "Re-learn" };

        bool learning = false, hasProfile = false;
        float progress = 0.0f, seconds = 0.0f, totalSeconds = 2.0f, capturedSeconds = 0.0f;
    };

    void onTick (float seconds) override;
    int getShownInputs() override { return juce::jmax (getFancyProcessor().getTotalNumInputChannels(), getFancyProcessor().getMainBusNumInputChannels() + 2); }

    FirProcessor& firProcessor() noexcept;
    void updateMode();

    Panels panels;
    juce::Label latencyLabel, modeLabel, editLabel;
    fancy::ui::ParamSegments modeSwitch;
    fancy::ui::ParamDropdown editDropdown;

    FirGraph graph;

    // Left card, one per mode.
    LearnCard learnCard;
    juce::Label curveCaption, convolveText;
    fancy::ui::PillButton resetButton { "Reset curve" }, flatButton { "Flat" }, tiltButton { "Tilt" }, demudButton { "De-mud" };

    // Mode controls card.
    fancy::ui::Knob floorKnob, gateAttackKnob, gateReleaseKnob;
    fancy::ui::Knob ratioKnob, compAttackKnob, compReleaseKnob;
    fancy::ui::Knob reductionKnob, offsetKnob, smoothingKnob, subReleaseKnob;
    juce::Label fftLabel, eqHint;
    fancy::ui::ParamDropdown fftDropdown;
    fancy::ui::ParamCheckbox reduceCheckbox;

    fancy::ui::ParamCheckbox analysisCheckbox, lowLatencyCheckbox, sidechainCheckbox, listenNoiseCheckbox, listenReducedCheckbox;
    fancy::ui::EditLink editLink;

    fancy::ui::OutMeter outMeter;
    juce::Label gainLabel;
    fancy::ui::Fader gainFader;

    fancy::ui::EditDrawer drawer;
    fancy::ui::Knob learnLengthKnob, drawerOutputKnob;

    int shownMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirEditor)
};
