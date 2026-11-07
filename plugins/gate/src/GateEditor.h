#pragma once

#include "GateProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <vector>

class GateEditor final : public fancy::ui::ShellEditor
{
public:
    explicit GateEditor (GateProcessor& processor);

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
    class History final : public juce::Component
    {
    public:
        History (GateDsp& dsp, juce::RangedAudioParameter& threshold, juce::RangedAudioParameter& hysteresis);

        void update();
        void showColumns (std::vector<GateDsp::HistoryColumn> columns);
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
    class MidiDropdown final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit MidiDropdown (GateProcessor& processor);

        void refresh();
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        GateProcessor& gateProcessor;
        juce::String text;
    };

    void onTick (float seconds) override;
    void updateLocks();

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

    fancy::ui::ParamCheckbox previewCheckbox, linkCheckbox, midiCheckbox, velocityCheckbox;
    MidiDropdown midiDropdown;

    fancy::ui::OutMeter outMeter;
    juce::Label wetLabel, dryLabel, noiseLabel;
    fancy::ui::Fader wetFader, dryFader, noiseFader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateEditor)
};
