#pragma once

#include "KickerProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

class KickerEditor final : public fancy::ui::ShellEditor
{
public:
    explicit KickerEditor (KickerProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: mockup levels, optionally with the EDIT drawer open, in Boost or
    // in Sub mode (docs/kicker-sub.md).
    void showPreview (bool drawerOpen, bool subMode = false);

private:
    void onTick (float seconds) override;
    void updateMode();

    fancy::ui::CaptionPanels panels;
    fancy::ui::ParamSegments modeSegments;

    // Boost.
    fancy::ui::Knob freqKnob, wetKnob;

    // Sub.
    fancy::ui::Knob cutoffKnob, subKnob;
    fancy::ui::ParamSegments octaveSegments;

    // Both.
    fancy::ui::Knob dryKnob;
    fancy::ui::EditLink editLink;
    fancy::ui::InOutMeter meter;

    fancy::ui::EditDrawer drawer;
    fancy::ui::Knob qKnob, drawerOutputKnob, sensitivityKnob, subLowpassKnob;
    fancy::ui::ParamCheckbox monoCheckbox, softClipCheckbox, subMonoCheckbox;

    // Phase 4 Wave 1: the Sub group, Auto gain and Ceiling in the drawer, and the sub meter.
    class SubMeter final : public juce::Component
    {
    public:
        void update (float peak, float seconds);
        void paint (juce::Graphics&) override;

    private:
        float shownDb = -100.0f;
    };

    fancy::ui::Knob subLengthKnob, roomFreqKnob, ceilingKnob;
    fancy::ui::ParamCheckbox roomCheckbox, polarityCheckbox, autoGainCheckbox;
    SubMeter subMeter;

    int shownMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickerEditor)
};
