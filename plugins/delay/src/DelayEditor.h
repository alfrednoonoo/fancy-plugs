#pragma once

#include "DelayProcessor.h"
#include "TapMap.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <functional>
#include <memory>
#include <vector>

class DelayEditor final : public fancy::ui::ShellEditor
{
public:
    explicit DelayEditor (DelayProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: mockup values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

    // Phase 3 Q2.3: tap 1 on Tape, its Motion page showing.
    void showMotionPreview();

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    // Straight, dotted and triplet, drawn as notes.
    class FeelSwitch final : public fancy::ui::ParamSegments
    {
    public:
        using ParamSegments::ParamSegments;

    private:
        void paintSegment (juce::Graphics&, int index, juce::Rectangle<float> area, juce::Colour ink) override;
    };

    // The controls for one tap. All 16 exist, and only the selected one is shown.
    class TapEditor final : public juce::Component
    {
    public:
        TapEditor (DelayProcessor& processor, int tapIndex, DelayEditor& owner);

        void update (int titleX);
        void paint (juce::Graphics&) override;

        fancy::ui::ParamDropdown& getNoteDropdown() noexcept { return noteDropdown; }
        fancy::ui::Knob& getNoteKnob() noexcept { return noteKnob; }
        fancy::ui::Knob& getFeedbackKnob() noexcept { return feedbackKnob; }

    private:
        juce::String detailText() const;

        DelayProcessor& delayProcessor;
        const int tap;
        int titleLeft = 0;

        fancy::ui::ParamSegments modeSwitch;
        fancy::ui::Knob msKnob, noteKnob;
        fancy::ui::ParamDropdown noteDropdown;
        FeelSwitch feelSwitch;
        juce::Label resultLabel;

        fancy::ui::Knob feedbackKnob, lowpassKnob, highpassKnob, resolutionKnob, widthKnob, panKnob;
        juce::Label volumeLabel;
        fancy::ui::Fader volumeFader;

        fancy::ui::ParamTextToggle soloButton;
        fancy::ui::ParamPowerIcon powerIcon;
        fancy::ui::IconButton closeButton;

        fancy::ui::ParamCheckbox loopCheckbox, pingCheckbox, saturateCheckbox;

        // Phase 3 Q2.3: the Tone | Motion view switch and the Motion page.
        class PageSwitch final : public juce::Component
        {
        public:
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;

            std::function<void (bool)> onChange;
            bool motion = false;
        };

        void showPage (bool motion);

        PageSwitch pageSwitch;
        juce::Label characterLabel;
        fancy::ui::ParamSegments characterSegments;
        fancy::ui::Knob amountKnob, modRateKnob, modDepthKnob, randomKnob, diffusionKnob;

    public:
        void showMotion (bool motion) { showPage (motion); }
    };

    void onTick (float seconds) override;
    bool listsParameter (const juce::AudioProcessorParameter&) override;

    DelayProcessor& delayProcessor() noexcept;

    void selectTap (int tap);
    void addTap();
    void removeSelectedTap();
    void removeTapNow();
    void updateSelection();

    Panels panels;
    TapMap tapMap;
    fancy::ui::ColourTabs tabs;
    std::vector<std::unique_ptr<TapEditor>> tapEditors;

    fancy::ui::ParamCheckbox smoothCheckbox;
    fancy::ui::EditLink editLink;

    fancy::ui::OutMeter outMeter;
    juce::Label wetLabel, dryLabel, tempoLabel;
    fancy::ui::Fader wetFader, dryFader;

    fancy::ui::EditDrawer drawer;
    juce::Label multiplierLabel;
    std::vector<std::unique_ptr<fancy::ui::ParamDropdown>> multiplierDropdowns;
    fancy::ui::Knob drawerOutputKnob;

    // Phase 3 Q2.3.
    juce::Label duckingLabel;
    fancy::ui::Knob swingKnob, duckAmountKnob, duckThresholdKnob, duckReleaseKnob;

    int selectedTap = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayEditor)
};
