#pragma once

#include "DsProcessor.h"

#include <fancy/ui/DeEsserHistory.h>
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

    // Phase 4 Wave 2: the High | Low band tabs. A click on a name selects its band (the window's
    // state, not a parameter); the power switch on Low turns Dual band on and off.
    class BandTabs final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<void (bool low)> onSelect;
        std::function<void()> onPower;

        void setState (bool lowSelected, bool dualOn);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::Rectangle<float> powerArea() const;

        bool low = false, dual = false;
    };

    // The detector level and reduction over the last 3 s, and a frequency ruler with the band.
    // Drag on the history for Threshold, the handle for Freq, the band's edges for Width.
    class Display final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Display (DsProcessor& processor);

        void update();
        void showColumns (std::vector<DsDsp::HistoryColumn> columns);

        // Phase 4 Wave 1: every new history column, for Learn.
        void setColumnListener (std::function<void (const DsDsp::HistoryColumn*, int)> listener) { history.onColumns = std::move (listener); }

        // Phase 4 Wave 2: which band the history's level and threshold and the threshold drag are
        // for, and a click on a band's bar selects it.
        void setLowSelected (bool shouldBeLow);
        std::function<void (bool low)> onSelectBand;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        static constexpr int rulerHeight = 42;

    private:
        enum class Drag { none, threshold, freq, lowEdge, highEdge, lowBandFreq, lowBandLowEdge, lowBandHighEdge };

        float historyHeight() const noexcept { return (float) (getHeight() - rulerHeight); }
        float yForDb (float db) const noexcept;
        float dbForY (float y) const noexcept;
        float xForHz (double hz) const noexcept;
        double hzForX (float x) const noexcept;
        Drag dragAt (juce::Point<float> position) const;
        juce::RangedAudioParameter& parameterFor (Drag drag) const;
        void dragTo (juce::Point<float> position);

        static bool isLowBandDrag (Drag drag) noexcept { return drag == Drag::lowBandFreq || drag == Drag::lowBandLowEdge || drag == Drag::lowBandHighEdge; }

        DsProcessor& dsProcessor;
        fancy::ui::DeEsserHistory history;
        Drag dragging = Drag::none;
        bool lowSelected = false;
    };

    void onTick (float seconds) override;
    void updateLocks();

    // Phase 4 Wave 2: shows High's or Low's Threshold, Freq, Width and Range.
    void selectBand (bool low);

    // Phase 4 Wave 1: Learn listens to 2 s of the detector level and sets Threshold at its 90th
    // percentile, so only the loudest "s" sounds are turned down (Decision 1 A).
    void finishLearning();

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

    // Phase 4 Wave 1.
    fancy::ui::PillButton learnButton { "Learn" };
    juce::Label learnNote, slopeLabel, ceilingLabel;
    fancy::ui::ParamDropdown slopeDropdown;

    // Phase 4 Wave 2, items 32 and 33.
    juce::Label reduceLabel;
    fancy::ui::ParamDropdown reduceDropdown;
    bool hissShown = false;
    fancy::ui::Knob ceilingKnob;
    std::vector<float> learnedLevels, learnedLowLevels;
    float learnSeconds = -1.0f;

    // Phase 4 Wave 2: the Low band's controls, in the High band's places while Low is selected.
    juce::Label bandLabel;
    BandTabs bandTabs;
    fancy::ui::ThresholdMeter lowThresholdMeter;
    fancy::ui::Knob lowFreqKnob, lowWidthKnob, lowRangeKnob;
    bool lowSelected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DsEditor)
};
