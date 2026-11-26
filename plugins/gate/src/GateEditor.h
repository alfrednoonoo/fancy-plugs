#pragma once

#include "GateProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <array>
#include <memory>
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

    // Phase 4 Wave 2: the EDIT drawer open, with Host sync chosen.
    void showDrawerPreview();

    // Phase 4 Wave 2, v1.7.1: the main window with Transient chosen, or in Upward; and the drawer
    // in Upward with a ceiling.
    void showModePreview (bool upward);
    void showUpwardDrawerPreview();

    // Phase 4 Wave 3: mockup 75's panel 1, three bands gating a drum overhead.
    void showBandsPreview();

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
        void setUpward (bool isUpward);   // reads "boost" for "open"
        void paint (juce::Graphics&) override;

    private:
        int state = GateDsp::lightClosed;
        bool upward = false;
    };

    // Phase 4 Wave 3 (mockup 75 A; docs/gate.md, "multiband gating"): the crossover strip, each
    // band's attenuation falling from the top in its colour. Drag a crossover's blue line to move
    // it, or click it to type a frequency.
    class BandStrip final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit BandStrip (GateProcessor& processor);

        void setAttenuations (const std::array<float, GateDsp::maxBands>& attenuationsDb);
        void askForCrossover (int index);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        static constexpr float stripHeight = 44.0f, scaleDb = 40.0f;

    private:
        float xForHz (double hz) const noexcept;
        double hzForX (float x) const noexcept;
        int crossoverAt (juce::Point<float> position) const;

        GateProcessor& gateProcessor;
        std::array<float, GateDsp::maxBands> attenuations {};
        int dragging = -1;
        bool dragged = false;
    };

    // The BANDS card: the band count, the crossover type, the strip, and each band's Threshold
    // offset, Range depth and open or shut state.
    class BandsCard final : public juce::Component
    {
    public:
        explicit BandsCard (GateProcessor& processor);

        void update (float seconds);

        // For screenshots: fixed attenuations and states, no updates.
        void showValues (const std::array<float, GateDsp::maxBands>& attenuationsDb, const std::array<bool, GateDsp::maxBands>& open);

        void paint (juce::Graphics&) override;

        BandStrip strip;

    private:
        void refreshBands();
        static float cellX (int band) noexcept;

        GateProcessor& gateProcessor;
        juce::Label crossoverLabel;
        fancy::ui::ParamSegments bandsSegments;
        fancy::ui::ParamDropdown crossoverDropdown;
        std::array<std::unique_ptr<fancy::ui::Knob>, GateDsp::maxBands> thresholdKnobs, rangeKnobs;
        std::array<float, GateDsp::maxBands> shown {};
        std::array<bool, GateDsp::maxBands> opened {};
        int bands = 1;
        bool frozen = false;
    };

    // Phase 4 Wave 2 (mockup 54): Ceiling as a dropdown of common values. A ctrl-click, or
    // "Type a value...", opens the typed-value box, as a knob's value does.
    class CeilingDropdown final : public fancy::ui::ParamClickControl, public fancy::ui::MenuTarget
    {
    public:
        using ParamClickControl::ParamClickControl;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void clicked() override;
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
    void updateSyncGroup();

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
    BandsCard bandsCard; // Phase 4 Wave 3

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
    // Phase 4 Wave 2 (reference/phase4/44__gate_w2_mockup.png): Host sync's group, in place of
    // the De-bleed note while Host sync is the Detector input.
    juce::Label syncCaption, syncRateLabel;
    fancy::ui::ParamDropdown syncRateDropdown;
    fancy::ui::Knob syncShapeKnob;
    // Phase 4 Wave 2, v1.7.1 (mockups 53 and 54): the Detector switch in the Range card's title
    // row, Rise in RMS size's place in Transient, Boost in Range's place and Up ratio in Ratio's
    // in Upward, and Ceiling under Learn.
    juce::Label detModeLabel;
    fancy::ui::ParamSegments detModeSegments;
    fancy::ui::Knob riseKnob, boostKnob, upRatioKnob;
    juce::Label ceilingLabel;
    CeilingDropdown ceilingDropdown;

    std::vector<float> learnedLevels;
    float learnSeconds = -1.0f;   // -1 while not learning

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateEditor)
};
