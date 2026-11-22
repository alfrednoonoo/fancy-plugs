#pragma once

#include "ChorusProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <vector>

// Fancy Chorus's window: reference/phase4/51__chorus_mockup.png (docs/chorus.md).
class ChorusEditor final : public fancy::ui::ShellEditor
{
public:
    explicit ChorusEditor (ChorusProcessor& processor);
    ~ChorusEditor() override;

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: the mockup's values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    // Each voice's delay over the last 2 s, and on the right where each voice sits in stereo.
    class VoiceMap final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit VoiceMap (ChorusProcessor& processor);

        void update();
        void showColumns (std::vector<ChorusDsp::HistoryColumn> columns);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        int traceWidth() const noexcept;

        ChorusProcessor& chorus;
        std::vector<ChorusDsp::HistoryColumn> columns, incoming;
        int writeIndex = 0;
        bool frozen = false;
        juce::String lastCaption;
    };

    void onTick (float seconds) override;
    void updateLocks();

    ChorusProcessor& chorusProcessor() noexcept;

    Panels panels;
    juce::Label modeLabel;
    fancy::ui::ParamSegments modeSegments;

    VoiceMap voiceMap;

    fancy::ui::Knob rateKnob, syncRateKnob, depthKnob, delayKnob, feedbackKnob;
    fancy::ui::Knob voicesKnob, spreadKnob, widthKnob;
    fancy::ui::Knob lowCutKnob, highCutKnob, ageKnob;

    fancy::ui::ParamCheckbox syncCheckbox, bassMonoCheckbox;
    fancy::ui::ListenCheckbox listenCheckbox;
    fancy::ui::EditLink editLink;

    fancy::ui::OutMeter outMeter;
    juce::Label wetLabel, dryLabel;
    fancy::ui::Fader wetFader, dryFader;

    fancy::ui::EditDrawer drawer;
    juce::Label shapeLabel, inputLabel, drawerNote;
    fancy::ui::ParamDropdown shapeDropdown, inputDropdown;
    fancy::ui::Knob lfoPhaseKnob, duckingKnob, outputKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusEditor)
};
