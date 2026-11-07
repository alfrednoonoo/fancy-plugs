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

    // For rendered screenshots: mockup values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);
    void showCompactPreview();

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
        class TypeDropdown final : public fancy::ui::ParamClickControl
        {
        public:
            using ParamClickControl::ParamClickControl;

            void paint (juce::Graphics&) override;

            bool compact = false;

        private:
            void clicked() override;
        };

        EqProcessor& eqProcessor;
        const int band;
        bool isCompact = false;

        TypeDropdown typeDropdown;
        fancy::ui::Knob frequencyKnob, gainKnob, qKnob;
        fancy::ui::ParamPowerIcon powerIcon;
        fancy::ui::IconButton plusButton, closeButton;
    };

    void bodyResized() override;
    void onTick (float seconds) override;
    bool listsParameter (const juce::AudioProcessorParameter&) override;

    EqProcessor& eqProcessor() noexcept;

    void selectBand (int band);
    void addBand();
    void removeSelectedBand();
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
