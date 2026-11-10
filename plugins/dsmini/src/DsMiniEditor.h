#pragma once

#include "DsMiniProcessor.h"

#include <fancy/ui/DeEsserHistory.h>
#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <vector>

// reference/phase3/22__ds_mini_mockup.png: the profile, its band, a small history and Amount.
class DsMiniEditor final : public fancy::ui::ShellEditor
{
public:
    explicit DsMiniEditor (DsMiniProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: the mockup's values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

    // What the caption says for a profile: what it catches, then its band.
    static juce::String captionFor (int profile);

    // Asks for a name, then writes the Fancy DS preset.
    void showSaveAsDsPreset();

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    // The history, from the engine that's playing. It only shows: Amount is the one control.
    class Display final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Display (DsMiniProcessor& processor);

        void update();
        void showColumns (std::vector<fancy::dsp::DeEsser::HistoryColumn> columns);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        DsMiniProcessor& miniProcessor;
        fancy::ui::DeEsserHistory history;
    };

    void onTick (float seconds) override;
    DsMiniProcessor& miniProcessor() noexcept;

    Panels panels;

    fancy::ui::ParamDropdown profileDropdown;
    juce::Label captionLabel;
    Display display;
    fancy::ui::Knob amountKnob;

    fancy::ui::ListenCheckbox listenCheckbox;
    fancy::ui::EditLink editLink;

    fancy::ui::GainReductionMeter gainMeter;
    fancy::ui::OutMeter outMeter;

    fancy::ui::EditDrawer drawer;
    juce::Label modeLabel, lookaheadLabel;
    fancy::ui::ParamSegments modeSegments;
    fancy::ui::ParamDropdown lookaheadDropdown;
    fancy::ui::ParamCheckbox linkCheckbox;
    fancy::ui::PillButton saveAsDsButton;
    juce::Label saveAsDsNote;

    int shownProfile = -1;
};
