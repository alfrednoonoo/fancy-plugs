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

    IrDisplay display;
    ModuleRack rack;
    ChainStrip strip;

    fancy::ui::Knob predelayKnob, centerKnob, widthKnob;
    fancy::ui::Fader wetFader, dryFader;
    juce::Label centerLabel, widthLabel, wetLabel, dryLabel;
    fancy::ui::PillButton setupButton { "SETUP" };

    fancy::ui::EditDrawer drawer { "SETUP" };
    juce::Label latencyLabel, fftLabel, storageLabel, folderLabel, folderPath, lateLabel;
    fancy::ui::ParamDropdown latencyDropdown, fftDropdown, storageDropdown;
    fancy::ui::PillButton folderButton { "Choose..." };
    std::unique_ptr<juce::FileChooser> chooser;

    int seenChainRevision = -1;
    int seenBuildRevision = -1;
    bool previewFrozen = false;
};
