#pragma once

#include "IrDisplay.h"
#include "ModuleRack.h"

#include <fancy/ui/ShellEditor.h>

class VerbEditor final : public fancy::ui::ShellEditor
{
public:
    static constexpr int baseWidth = 866, baseHeight = 677;

    enum class Preview { rack, addPanel, setup, scrolled };

    explicit VerbEditor (VerbProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // Set the Figma values, for CI screenshots.
    void showPreview (Preview preview);
    void showEmptyPreview();

private:
    void bodyResized() override;
    void onTick (float seconds) override;
    bool listsParameter (const juce::AudioProcessorParameter&) override;

    void updateDisplay();
    void setValue (const char* id, float value);
    void chooseIrFolder();

    VerbProcessor& processor;

    class ColumnPanel final : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override { g.fillAll (fancy::ui::colours::bgSection); }
    };

    ColumnPanel columnPanel;
    IrDisplay display;
    ModuleRack rack;
    ChainStrip strip;

    fancy::ui::Knob predelayKnob, centerKnob, widthKnob;
    fancy::ui::Fader wetFader, dryFader;
    juce::Label centerLabel, widthLabel, wetLabel, dryLabel;
    fancy::ui::EditLink setupButton { fancy::ui::colours::textSecondary, "SETUP" };

    fancy::ui::EditDrawer drawer { "SETUP" };
    juce::Label latencyLabel, fftLabel, storageLabel, folderLabel, folderPath, lateLabel;
    fancy::ui::ParamDropdown latencyDropdown, fftDropdown, storageDropdown;
    fancy::ui::PillButton folderButton { "Choose..." };
    std::unique_ptr<juce::FileChooser> chooser;

    int seenChainRevision = -1;
    int seenBuildRevision = -1;
    bool previewFrozen = false;
};
