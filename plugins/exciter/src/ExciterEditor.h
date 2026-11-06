#pragma once

#include "ExciterProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

class ExciterEditor final : public fancy::ui::ShellEditor
{
public:
    explicit ExciterEditor (ExciterProcessor& processor);

    // For rendered screenshots: mockup values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

private:
    void onTick (float seconds) override;

    fancy::ui::CaptionPanels panels;
    fancy::ui::Knob freqKnob, clipBoostKnob, harmonicsKnob, mixBackKnob;
    fancy::ui::EditLink editLink;
    fancy::ui::InOutMeter meter;

    fancy::ui::EditDrawer drawer;
    fancy::ui::Knob characterKnob, drawerOutputKnob;
    fancy::ui::ParamDropdown oversamplingDropdown;
    juce::Label oversamplingLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExciterEditor)
};
