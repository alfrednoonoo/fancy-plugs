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

    // Phase 3 Q2.6: band 2 on Mid, for the channel chip and the graph's letter.
    void showChannelPreview();

    // Phase 4 Wave 2 (item 26): the graph in GR history, with mockup 58's lanes.
    void showHistoryPreview();

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
        // Phase 2.1 U: the band's static curve, input level against output level, with guides
        // at Floor, Up threshold and Threshold.
        class TransferCurve final : public juce::Component
        {
        public:
            TransferCurve (MultiCompProcessor& processor, int bandIndex);

            void update();
            void paint (juce::Graphics&) override;

        private:
            MultiCompProcessor& processor;
            const int band;
            MultiCompDsp::BandSpec shown;
            double shownDepth = -1.0;
        };

        MultiCompProcessor& multiProcessor;
        const int band;
        int titleLeft = 0;

        fancy::ui::OutMeter inputMeter;
        juce::Label thresholdLabel;
        fancy::ui::Fader thresholdFader;
        fancy::ui::GainReductionMeter reductionMeter;

        fancy::ui::Knob ratioKnob, kneeKnob, attackKnob, releaseKnob, rmsKnob, gainKnob;
        fancy::ui::Knob lookKnob; // Phase 4 Wave 2 (item 23)
        fancy::ui::ParamCheckbox makeupCheckbox, autoReleaseCheckbox, feedbackCheckbox, sidechainCheckbox;

        fancy::ui::ChannelChip channelChip; // Phase 3 Q2.6
        fancy::ui::ParamTextToggle soloButton;
        fancy::ui::ParamTextToggle muteButton; // Phase 4 Wave 1
        fancy::ui::ParamPowerIcon powerIcon;
        fancy::ui::IconButton closeButton;

        // Phase 2.1 U: the UPWARD row.
        juce::Label upwardLabel;
        fancy::ui::Knob upThresholdKnob, upRatioKnob, maxLiftKnob, floorKnob;
        fancy::ui::ParamCheckbox upwardCheckbox;
        TransferCurve transferCurve;
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
    fancy::ui::PillButton learnButton { "Learn" }; // Phase 4 Wave 1: Learn crossovers
    fancy::ui::ColourTabs tabs;
    std::vector<std::unique_ptr<BandEditor>> bandEditors;

    fancy::ui::ParamDropdown detectorDropdown;

    fancy::ui::ParamCheckbox spectrumCheckbox, linkCheckbox, gridCheckbox, limitCheckbox;
    juce::Label crossoverLabel;
    fancy::ui::ParamDropdown crossoverDropdown;

    fancy::ui::OutMeter outMeter;
    fancy::ui::Knob depthKnob, timeKnob;
    juce::Label wetLabel, dryLabel;
    fancy::ui::Fader wetFader, dryFader;
    fancy::ui::EditLink editLink;

    fancy::ui::EditDrawer drawer;
    fancy::ui::ParamCheckbox attack90Checkbox, logCheckbox;
    fancy::ui::Knob drawerOutputKnob;

    // Phase 3 Q2.6. Phase 4 Wave 2 makes it Set all bands.
    fancy::ui::Knob lookaheadKnob;
    juce::Label latencyLabel, setAllCaption;

    // Phase 4 Wave 2 (items 24 and 25; reference/phase4/58__multicomp_w2_mockup.png, panel 3):
    // the selected band's detector EQ and a row per crossover for its slope, in the drawer.
    class DrawerNotes final : public juce::Component
    {
    public:
        explicit DrawerNotes (MultiCompEditor& owner) : editor (owner) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;

    private:
        MultiCompEditor& editor;
    };

    void layoutSlopes();

    DrawerNotes drawerNotes { *this };
    std::vector<std::unique_ptr<fancy::ui::Knob>> detectorKnobs; // band * 3 + (HP, LP, Tilt)
    std::vector<std::unique_ptr<fancy::ui::ParamDropdown>> slopeDropdowns;
    int shownCrossovers = -1;
    bool slopesEnabled = true;

    std::vector<MultiCompDsp::HistoryColumn> historyScratch;

    int selectedBand = 0;
    std::array<float, multicomp::maxBands> displayed {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiCompEditor)
};
