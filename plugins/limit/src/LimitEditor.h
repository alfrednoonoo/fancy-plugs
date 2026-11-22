#pragma once

#include "LimitProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <vector>

class LimitEditor final : public fancy::ui::ShellEditor
{
public:
    explicit LimitEditor (LimitProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: the mockup's values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

    // The read-out panel's values, for the screenshots and the tests.
    struct Readings
    {
        double shortTerm = -200.0, integrated = -200.0, truePeak = -200.0, range = 0.0;
        int overs = 0; // Phase 4 Wave 1
    };

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    // Input, output and reduction over the last 4 s, and the loudness read-outs. A click on the
    // read-outs resets the integration.
    class Display final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Display (LimitProcessor& processor);

        void update();
        void showColumns (std::vector<LimitDsp::HistoryColumn> columns, const Readings& readings);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent&) override;

        static constexpr int panelWidth = 150;

    private:
        float yForDb (float db) const;
        juce::Rectangle<float> panelBounds() const;

        LimitProcessor& limitProcessor;
        std::vector<LimitDsp::HistoryColumn> columns, incoming;
        int writeIndex = 0;
        Readings readings;
        float ceilingDb = -1.0f;
        bool frozen = false;
    };

    // "Streaming · -14 LUFS". The last choice, Custom…, asks for a value.
    class TargetDropdown final : public juce::Component, public juce::SettableTooltipClient, public fancy::ui::MenuTarget
    {
    public:
        explicit TargetDropdown (LimitProcessor& processor);

        void refresh();
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void showMenu();
        void askForCustom();

    private:
        LimitProcessor& limitProcessor;
        juce::String text;
    };

    // Phase 4 Wave 1: measures 10 s of playback and moves Threshold once so the integrated
    // loudness lands on the Target. A ring shows the progress.
    class AutoTargetButton final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void()> onClick;
        float progress = -1.0f; // -1 idle, 0 to 1 measuring
    };

    void onTick (float seconds) override;
    void updateAutoTarget (float seconds);

    LimitProcessor& limitProcessor() noexcept;

    Panels panels;

    juce::Label styleLabel, targetLabel;
    fancy::ui::ParamSegments styleSegments;
    TargetDropdown targetDropdown;

    fancy::ui::ThresholdMeter thresholdMeter;
    fancy::ui::GainReductionMeter gainMeter;

    Display display;

    fancy::ui::Knob ceilingKnob, releaseKnob, lookaheadKnob, linkKnob;

    fancy::ui::ParamCheckbox truePeakCheckbox, autoReleaseCheckbox, listenCheckbox;
    fancy::ui::EditLink editLink;

    fancy::ui::OutMeter outMeter;
    fancy::ui::LoudnessBar loudnessBar;

    fancy::ui::EditDrawer drawer;
    juce::Label oversamplingLabel, phaseLabel, ditherLabel, ballisticsLabel, resetLabel;
    fancy::ui::ParamDropdown oversamplingDropdown, phaseDropdown, ditherDropdown, ballisticsDropdown;
    fancy::ui::PillButton resetButton { "Reset loudness" };

    // Phase 4 Wave 1.
    fancy::ui::ToggleCheckbox gainLockCheckbox { "Gain lock", juce::Colour (0xffe74d3d) };
    AutoTargetButton autoTargetButton;
    float autoTargetSeconds = 0.0f;
    juce::Label releaseCurveLabel, dcLabel;
    fancy::ui::ParamDropdown releaseCurveDropdown;
    fancy::ui::ParamCheckbox dcCheckbox;

    // Phase 4 Wave 2 (reference/phase4/49__limit_w2_mockup.png): the clipper.
    fancy::ui::ParamCheckbox clipCheckbox;
    fancy::ui::Knob clipKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimitEditor)
};
