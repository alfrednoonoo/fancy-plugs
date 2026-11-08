#pragma once

#include "BandGraph.h"
#include "MultiCompProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <memory>
#include <vector>

class MultiCompEditor final : public fancy::ui::ShellEditor
{
public:
    explicit MultiCompEditor (MultiCompProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: mockup values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    // The controls for one band. All 8 exist, and only the selected one is shown.
    class BandEditor final : public juce::Component
    {
    public:
        BandEditor (MultiCompProcessor& processor, int bandIndex, MultiCompEditor& owner);

        void update (int titleX, float reductionDb, float seconds);
        void showLevels (float left, float right, float reductionDb);

        void paint (juce::Graphics&) override;

        fancy::ui::Fader& getThresholdFader() noexcept { return thresholdFader; }
        fancy::ui::Knob& getRatioKnob() noexcept { return ratioKnob; }

    private:
        MultiCompProcessor& multiProcessor;
        const int band;
        int titleLeft = 0;

        fancy::ui::OutMeter inputMeter;
        juce::Label thresholdLabel;
        fancy::ui::Fader thresholdFader;
        fancy::ui::GainReductionMeter reductionMeter;

        fancy::ui::Knob ratioKnob, kneeKnob, attackKnob, releaseKnob, rmsKnob, gainKnob;
        fancy::ui::ParamCheckbox makeupCheckbox, autoReleaseCheckbox, feedbackCheckbox, sidechainCheckbox;

        fancy::ui::ParamTextToggle soloButton;
        fancy::ui::ParamPowerIcon powerIcon;
        fancy::ui::IconButton closeButton;
    };

    void onTick (float seconds) override;
    bool listsParameter (const juce::AudioProcessorParameter&) override;

    // The stereo sidechain counts even while the host leaves it off, as the plugin has it.
    int getShownInputs() override { return juce::jmax (getFancyProcessor().getTotalNumInputChannels(), getFancyProcessor().getMainBusNumInputChannels() + 2); }


    MultiCompProcessor& multiProcessor() noexcept;

    void selectBand (int band);
    void addBand();
    void removeSelectedBand();
    void removeBandNow();
    void updateSelection();

    Panels panels;
    BandGraph graph;
    fancy::ui::ColourTabs tabs;
    std::vector<std::unique_ptr<BandEditor>> bandEditors;

    fancy::ui::ParamDropdown detectorDropdown;

    fancy::ui::ParamCheckbox spectrumCheckbox, linkCheckbox, gridCheckbox, limitCheckbox;
    juce::Label crossoverLabel;
    fancy::ui::ParamDropdown crossoverDropdown;

    fancy::ui::OutMeter outMeter;
    juce::Label wetLabel, dryLabel;
    fancy::ui::Fader wetFader, dryFader;
    fancy::ui::EditLink editLink;

    fancy::ui::EditDrawer drawer;
    fancy::ui::ParamCheckbox attack90Checkbox, logCheckbox;
    fancy::ui::Knob drawerOutputKnob;

    int selectedBand = 0;
    std::array<float, multicomp::maxBands> displayed {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiCompEditor)
};
