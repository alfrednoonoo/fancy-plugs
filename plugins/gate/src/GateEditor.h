#pragma once

#include "GateProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <vector>

class GateEditor final : public fancy::ui::ShellEditor
{
public:
    explicit GateEditor (GateProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: mockup values. The second view has Lookahead and MIDI on, and
    // shows the sidechain warning.
    void showPreview (bool secondView);

    // The hint the locked Pre-open knob shows.
    static const char* const preOpenHint;

    // The Velocity from level tooltip for these settings.
    static juce::String velocityTooltip (const GateDsp::Targets& targets);

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    // Input level and the gate's gain over the last 3 s. A click or drag sets Threshold.
    class History final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        History (GateDsp& dsp, juce::RangedAudioParameter& threshold, juce::RangedAudioParameter& hysteresis);

        void update();
        void showColumns (std::vector<GateDsp::HistoryColumn> columns);

        // Phase 4 Wave 1: every new column, for Learn.
        std::function<void (const GateDsp::HistoryColumn*, int)> onColumns;
        void setWarning (bool shouldShow);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        float yForDb (float db) const;
        float dbForY (float y) const;
        void setThresholdAt (float y);

        GateDsp& gateDsp;
        juce::RangedAudioParameter &thresholdParam, &hysteresisParam;
        std::vector<GateDsp::HistoryColumn> columns, incoming;
        int writeIndex = 0;
        bool warning = false;
    };

    // The state light under the Gain meter.
    class Light final : public juce::Component
    {
    public:
        void setState (int newState);
        void paint (juce::Graphics&) override;

    private:
        int state = GateDsp::lightClosed;
    };

    // "A4 · ch 1": opens the note, channel and velocity window menus.
    class MidiDropdown final : public juce::Component, public juce::SettableTooltipClient, public fancy::ui::MenuTarget
    {
    public:
        explicit MidiDropdown (GateProcessor& processor);

        void refresh();
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void showMenu();

    private:
        GateProcessor& gateProcessor;
        juce::String text;
    };

    void onTick (float seconds) override;
    void updateLocks();

    // Phase 4 Wave 1: Learn listens to 3 s of the detector level and sets Threshold a third of
    // the way from the noise floor (10th percentile) to the hits (95th), Decision 2 A.
    void learnFrom (const GateDsp::HistoryColumn* columns, int count);
    void finishLearning();

    // The stereo sidechain counts even while the host leaves it off, as the plugin has it.
    int getShownInputs() override { return juce::jmax (getFancyProcessor().getTotalNumInputChannels(), getFancyProcessor().getMainBusNumInputChannels() + 2); }

    GateProcessor& gateProcessor() noexcept;

    Panels panels;

    juce::Label detectorLabel, latencyLabel, modeLabel;
    fancy::ui::ParamDropdown detectorDropdown;
    fancy::ui::ParamCheckbox lookaheadCheckbox;
    fancy::ui::ParamSegments modeSegments;

    fancy::ui::ThresholdMeter thresholdMeter;
    fancy::ui::GainReductionMeter gainMeter;
    Light light;

    History history;

    fancy::ui::Knob preOpenKnob, attackKnob, holdKnob, releaseKnob, hysteresisKnob;
    fancy::ui::Knob rangeKnob, rmsKnob, lowpassKnob, highpassKnob;

    fancy::ui::ListenCheckbox previewCheckbox;
    fancy::ui::ParamCheckbox linkCheckbox, midiCheckbox, velocityCheckbox;
    MidiDropdown midiDropdown;

    fancy::ui::OutMeter outMeter;
    juce::Label wetLabel, dryLabel, noiseLabel;
    fancy::ui::Fader wetFader, dryFader, noiseFader;

    // Phase 4 Wave 1: the EDIT drawer (Decision 4 A).
    fancy::ui::EditLink editLink;
    fancy::ui::EditDrawer drawer;
    fancy::ui::ParamCheckbox midiInCheckbox;
    juce::Label midiInNote, learnNote;
    fancy::ui::PillButton learnButton { "Learn" };
    fancy::ui::Knob ratioKnob, deBleedKnob;
    juce::Label drawerNote;
    std::vector<float> learnedLevels;
    float learnSeconds = -1.0f;   // -1 while not learning

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateEditor)
};
