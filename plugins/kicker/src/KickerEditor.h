#pragma once

#include "KickerProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

class KickerEditor final : public fancy::ui::ShellEditor
{
public:
    explicit KickerEditor (KickerProcessor& processor);

    // For rendered screenshots: mockup levels, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    void onTick (float seconds) override;

    Panels panels;
    fancy::ui::Knob freqKnob, wetKnob, dryKnob;
    fancy::ui::EditLink editLink;
    fancy::ui::InOutMeter meter;

    fancy::ui::EditDrawer drawer;
    fancy::ui::Knob qKnob, drawerOutputKnob;
    fancy::ui::ParamCheckbox monoCheckbox, softClipCheckbox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickerEditor)
};
