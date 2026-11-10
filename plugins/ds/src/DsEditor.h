#pragma once

#include "DsProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <vector>

class DsEditor final : public fancy::ui::ShellEditor
{
public:
    explicit DsEditor (DsProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: the mockup's values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

    // The hint the locked Width knob shows in Shelf.
    static const char* const widthHint;

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    // The detector level and reduction over the last 3 s, and a frequency ruler with the band.
    // Drag on the history for Threshold, the handle for Freq, the band's edges for Width.
    class Display final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Display (DsProcessor& processor);

        void update();
        void showColumns (std::vector<DsDsp::HistoryColumn> columns);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        static constexpr int rulerHeight = 42;

    private:
        enum class Drag { none, threshold, freq, lowEdge, highEdge };

        float historyHeight() const noexcept { return (float) (getHeight() - rulerHeight); }
        float yForDb (float db) const noexcept;
        float dbForY (float y) const noexcept;
        float xForHz (double hz) const noexcept;
        double hzForX (float x) const noexcept;
        Drag dragAt (juce::Point<float> position) const;
        juce::RangedAudioParameter& parameterFor (Drag drag) const;
        void dragTo (juce::Point<float> position);

        DsProcessor& dsProcessor;
        std::vector<DsDsp::HistoryColumn> columns, incoming;
        int writeIndex = 0;
        bool frozen = false;
        Drag dragging = Drag::none;
    };

    void onTick (float seconds) override;
    void updateLocks();

    // The stereo sidechain counts even while the host leaves it off, as the plugin has it.
    int getShownInputs() override { return juce::jmax (getFancyProcessor().getTotalNumInputChannels(), getFancyProcessor().getMainBusNumInputChannels() + 2); }

    DsProcessor& dsProcessor() noexcept;

    Panels panels;

    juce::Label detectorLabel, modeLabel;
    fancy::ui::ParamDropdown detectorDropdown;
    fancy::ui::ParamSegments modeSegments;

    fancy::ui::ThresholdMeter thresholdMeter;
    fancy::ui::GainReductionMeter gainMeter;

    Display display;

    fancy::ui::ParamSegments shapeSegments;
    fancy::ui::Knob freqKnob, widthKnob, rangeKnob, attackKnob, releaseKnob;

    fancy::ui::ParamCheckbox relativeCheckbox;
    fancy::ui::ListenCheckbox listenCheckbox;
    fancy::ui::ParamCheckbox linkCheckbox;
    fancy::ui::EditLink editLink;

    fancy::ui::OutMeter outMeter;
    juce::Label gainLabel;
    fancy::ui::Fader gainFader;

    fancy::ui::EditDrawer drawer;
    juce::Label lookaheadLabel, phaseLabel;
    fancy::ui::ParamDropdown lookaheadDropdown, phaseDropdown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DsEditor)
};
