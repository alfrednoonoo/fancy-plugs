#pragma once

#include "TsProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <vector>

// Fancy TS's window: reference/phase4/50__transient_mockup.png (docs/ts.md).
class TsEditor final : public fancy::ui::ShellEditor
{
public:
    explicit TsEditor (TsProcessor& processor);
    ~TsEditor() override;

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

    // The input (grey), the output (orange) and the gain applied (white, 0 dB dashed) over the
    // last 3 s, with each detected hit marked.
    class History final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit History (TsDsp& dsp);

        void update();
        void showColumns (std::vector<TsDsp::HistoryColumn> columns);
        void setWarning (bool shouldShow);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        TsDsp& tsDsp;
        std::vector<TsDsp::HistoryColumn> columns, incoming;
        int writeIndex = 0;
        bool warning = false, frozen = false;
    };

    void onTick (float seconds) override;
    void updateLocks();

    TsProcessor& tsProcessor() noexcept;

    Panels panels;
    juce::Label contourLabel, detectorLabel;
    fancy::ui::ParamSegments contourSegments;
    fancy::ui::ParamDropdown detectorDropdown;

    History history;

    fancy::ui::Knob attackKnob, speedKnob, sustainKnob, lengthKnob, pumpKnob, sensitivityKnob, highpassKnob;

    fancy::ui::ParamCheckbox clipCheckbox, linkCheckbox;
    fancy::ui::ListenCheckbox listenCheckbox;
    fancy::ui::EditLink editLink;

    fancy::ui::OutMeter outMeter;
    juce::Label wetLabel, dryLabel;
    fancy::ui::Fader wetFader, dryFader;

    fancy::ui::EditDrawer drawer;
    juce::Label lookaheadLabel, oversamplingLabel, channelsLabel, latencyNote, drawerNote;
    fancy::ui::ParamDropdown lookaheadDropdown, oversamplingDropdown, channelsDropdown;
    fancy::ui::Knob lowpassKnob, deadbandKnob, outputKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsEditor)
};
