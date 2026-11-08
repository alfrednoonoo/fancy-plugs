#pragma once

#include "EqGraph.h"
#include "EqProcessor.h"

#include <fancy/ui/ShellEditor.h>

#include <memory>
#include <vector>

class EqEditor final : public fancy::ui::ShellEditor
{
public:
    explicit EqEditor (EqProcessor& processor);

    juce::Colour getAccentColour() override { return bandColour (selectedBand); }
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: mockup values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);
    void showCompactPreview();

    // Panel 3 of the Phase 2.1 mockup: band 2 dynamic, 3.6 dB into its -6 dB reach.
    void showDynamicPreview (bool compact);

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;

        bool compact = false;
    };

    class TabStrip final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void (int)> onSelect;
        std::vector<int> bands;
        int selected = 0;

    private:
        juce::Rectangle<int> tabBounds (int index) const;
    };

    // The controls for one band. All 16 exist, and only the selected one is shown.
    class BandEditor final : public juce::Component
    {
    public:
        BandEditor (EqProcessor& processor, int bandIndex, EqEditor& owner);

        void layoutFor (bool compact);
        void update();

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
    public:
        class TypeDropdown final : public fancy::ui::ParamClickControl, public fancy::ui::MenuTarget
        {
        public:
            TypeDropdown (juce::RangedAudioParameter& type, juce::RangedAudioParameter& slope);

            void paint (juce::Graphics&) override;

            bool compact = false;

        private:
            void clicked() override;

            juce::RangedAudioParameter& slopeParameter;
        };

        TypeDropdown& getTypeDropdown() noexcept { return typeDropdown; }
        fancy::ui::Knob& getFrequencyKnob() noexcept { return frequencyKnob; }
        fancy::ui::Knob& getGainKnob() noexcept { return gainKnob; }
        fancy::ui::ParamTextToggle& getDynamicChip() noexcept { return dynamicChip; }

        // Whether the dynamic half is showing, which widens the Compact-mode card (Phase 2.1 D).
        bool showsDynamics() const noexcept { return dynamicShown; }

        static constexpr const char* linearTooltip = "Dynamic bands need Minimum phase";

    private:

        void layoutDynamic (int dx, int dy, int buttonsX, int buttonsDy);

        EqProcessor& eqProcessor;
        EqEditor& owner;
        const int band;
        bool isCompact = false;
        bool dynamicShown = false, chipShown = false, linearPhase = false;

        TypeDropdown typeDropdown;
        fancy::ui::Knob frequencyKnob, gainKnob, qKnob;
        fancy::ui::ParamPowerIcon powerIcon;
        fancy::ui::IconButton plusButton, closeButton;

        // Phase 2.1 D: the DYN chip and the dynamic half.
        fancy::ui::ParamTextToggle dynamicChip;
        fancy::ui::Knob thresholdKnob, rangeKnob, attackKnob, releaseKnob;
        fancy::ui::ParamSegments modeSwitch;
        fancy::ui::ParamCheckbox sidechainCheckbox;
        int separatorX = -1, separatorTop = 0;
    };

    void bodyResized() override;
    void onTick (float seconds) override;

    // The sidechain counts once the host connects it (Phase 2.1 D), as in Comp: "4 in 2 out".
    int getShownInputs() override { return juce::jmax (getFancyProcessor().getTotalNumInputChannels(), getFancyProcessor().getMainBusNumInputChannels() + 2); }
    bool listsParameter (const juce::AudioProcessorParameter&) override;

    EqProcessor& eqProcessor() noexcept;

    void selectBand (int band);
    void addBand();
    void removeSelectedBand();
    void removeBandNow();
    void updateSelection();
    void layoutEditors();
    void applyPreviewValues();

    Panels panels;
    EqGraph graph;

    juce::Label gainLabel;
    fancy::ui::Fader gainFader;

    fancy::ui::ParamCheckbox logCheckbox, gridCheckbox, phaseCheckbox, compactCheckbox;

    TabStrip tabs;
    std::vector<std::unique_ptr<BandEditor>> bandEditors;

    fancy::ui::EditLink editLink;
    fancy::ui::EditDrawer drawer;
    juce::Label phaseLabel, firLabel, oversamplingLabel, slopeLabel, rangeLabel, analyzerLabel, fftLabel, windowLabel, overlapLabel;
    fancy::ui::ParamDropdown phaseDropdown, firDropdown, oversamplingDropdown, rangeDropdown, analyzerDropdown, fftDropdown, windowDropdown, overlapDropdown;
    std::vector<std::unique_ptr<fancy::ui::ParamDropdown>> slopeDropdowns;
    fancy::ui::ParamCheckbox tiltCheckbox, peakCheckbox;
    fancy::ui::Knob smoothingKnob;

    bool compactMode = false;
    float lastRange = -1.0f;
    int selectedBand = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqEditor)
};
