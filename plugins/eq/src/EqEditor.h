#pragma once

#include "EqGraph.h"
#include "EqProcessor.h"

#include <fancy/ui/ShellEditor.h>

#include <memory>
#include <vector>

class EqEditor final : public fancy::ui::ShellEditor
{
public:
    explicit EqEditor (EqProcessor& processor);
    ~EqEditor() override;

    juce::Colour getAccentColour() override { return bandColour (selectedBand); }
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: mockup values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);
    void showCompactPreview();

    // Panel 3 of the Phase 2.1 mockup: band 2 dynamic, 3.6 dB into its -6 dB reach.
    void showDynamicPreview (bool compact);

    // Panel 2 of the Q2.4 mockup: band 3 on Mid and selected, band 5 on Side.
    void showChannelPreview();

    // Panel 3 of the Q2.4 mockup: the Match card open over a learned reference and current.
    void showMatchPreview();

    // Opens or closes the Match card. While it's open the processor's Match engine runs.
    void setMatchOpen (bool shouldBeOpen);
    bool isMatchOpen() const noexcept { return matchOpen; }

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;

        bool compact = false;
    };

    class TabStrip final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void (int)> onSelect;
        std::vector<int> bands;
        int selected = 0;

    private:
        juce::Rectangle<int> tabBounds (int index) const;
    };

    // The controls for one band. All 24 exist, and only the selected one is shown.
    class BandEditor final : public juce::Component
    {
    public:
        BandEditor (EqProcessor& processor, int bandIndex, EqEditor& owner);

        void layoutFor (bool compact);
        void update();

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
    public:
        class TypeDropdown final : public fancy::ui::ParamClickControl, public fancy::ui::MenuTarget
        {
        public:
            TypeDropdown (juce::RangedAudioParameter& type, juce::RangedAudioParameter& slope);

            void paint (juce::Graphics&) override;

            bool compact = false;

        private:
            void clicked() override;

            juce::RangedAudioParameter& slopeParameter;
        };

        // Phase 3 Q2.4: which channel the band filters. Reads ST, L, R, M or S.
        class ChannelChip final : public fancy::ui::ParamClickControl, public fancy::ui::MenuTarget
        {
        public:
            ChannelChip (juce::RangedAudioParameter& channel, juce::Colour accentColour);

            void paint (juce::Graphics&) override;

        private:
            void clicked() override;

            juce::Colour accent;
        };

        ChannelChip& getChannelChip() noexcept { return channelChip; }
        TypeDropdown& getTypeDropdown() noexcept { return typeDropdown; }
        fancy::ui::Knob& getFrequencyKnob() noexcept { return frequencyKnob; }
        fancy::ui::Knob& getGainKnob() noexcept { return gainKnob; }
        fancy::ui::ParamTextToggle& getDynamicChip() noexcept { return dynamicChip; }

        // Whether the dynamic half is showing, which widens the Compact-mode card (Phase 2.1 D).
        bool showsDynamics() const noexcept { return dynamicShown; }

        static constexpr const char* linearTooltip = "Dynamic bands need Minimum phase";

    private:

        void layoutDynamic (int dx, int dy, int buttonsX, int buttonsDy);

        EqProcessor& eqProcessor;
        EqEditor& owner;
        const int band;
        bool isCompact = false;
        bool dynamicShown = false, chipShown = false, linearPhase = false;

        TypeDropdown typeDropdown;
        ChannelChip channelChip;
        fancy::ui::Knob frequencyKnob, gainKnob, qKnob;
        fancy::ui::ParamPowerIcon powerIcon;
        fancy::ui::IconButton plusButton, closeButton;

        // Phase 2.1 D: the DYN chip and the dynamic half.
        fancy::ui::ParamTextToggle dynamicChip;
        fancy::ui::Knob thresholdKnob, rangeKnob, attackKnob, releaseKnob;
        fancy::ui::ParamSegments modeSwitch;
        fancy::ui::ParamCheckbox sidechainCheckbox;
        int separatorX = -1, separatorTop = 0;
    };

    // The header's MATCH link, left of EDIT: drawn as EditLink is, with a target for its icon.
    // White while the card is open. In a narrow window it's the icon alone.
    class MatchLink final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        MatchLink();

        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void()> onClick;
        bool lit = false;
    };

    // The Match card, which floats over the graph as the Compact-mode card does. Its settings
    // aren't the plugin's parameters, so each control has a parameter of its own that writes to
    // the Match engine: the shared controls, with typed values, and nothing for a host to see.
    class MatchCard final : public juce::Component
    {
    public:
        static constexpr int cardWidth = 548, cardHeight = 152;

        explicit MatchCard (EqProcessor& processor);
        ~MatchCard() override;

        // 30 Hz, from the editor: button texts, the times learned, and a finished fit.
        void update();

        // Puts the controls back on the engine's settings, after a project loads.
        void readSettings();

        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        void resized() override;

        std::function<void()> onClose;

    private:
        struct Link final : public juce::AudioProcessorParameter::Listener
        {
            Link (juce::RangedAudioParameter& parameterToUse, std::function<void (float)> changed);
            ~Link() override;

            void parameterValueChanged (int, float) override;
            void parameterGestureChanged (int, bool) override {}

            juce::RangedAudioParameter& parameter;
            std::function<void (float)> onChange;
        };

        void toggle (eq::match::Engine::Learning mode);
        void startFit();

        EqProcessor& eqProcessor;
        eq::match::Engine& engine;

        juce::AudioParameterChoice sourceParameter, smoothingParameter;
        std::unique_ptr<juce::AudioParameterFloat> amountParameter, bandsParameter, lowParameter, highParameter;
        std::vector<std::unique_ptr<Link>> links;

        fancy::ui::ParamSegments sourceSwitch;
        fancy::ui::Knob smoothingKnob, amountKnob, bandsKnob, lowKnob, highKnob;
        fancy::ui::PillButton learnButton, learnCurrentButton, clearButton, matchButton;
        fancy::ui::IconButton closeButton;

        juce::String statusText, captionText;
        juce::Rectangle<int> learningOutline;
    };

    void bodyResized() override;
    void onTick (float seconds) override;
    void updateMatchOverlay();

    // The sidechain counts once the host connects it (Phase 2.1 D), as in Comp: "4 in 2 out".
    int getShownInputs() override { return juce::jmax (getFancyProcessor().getTotalNumInputChannels(), getFancyProcessor().getMainBusNumInputChannels() + 2); }
    bool listsParameter (const juce::AudioProcessorParameter&) override;

    EqProcessor& eqProcessor() noexcept;

    void selectBand (int band);
    void addBand();
    void removeSelectedBand();
    void removeBandNow();
    void updateSelection();
    void layoutEditors();
    void applyPreviewValues();

    Panels panels;
    EqGraph graph;

    juce::Label gainLabel;
    fancy::ui::Fader gainFader;

    fancy::ui::ParamCheckbox logCheckbox, gridCheckbox, phaseCheckbox, compactCheckbox;

    TabStrip tabs;
    std::vector<std::unique_ptr<BandEditor>> bandEditors;

    fancy::ui::EditLink editLink;
    MatchLink matchLink;
    MatchCard matchCard;
    bool matchOpen = false;
    int matchTick = 0;
    fancy::ui::EditDrawer drawer;
    juce::Label phaseLabel, firLabel, oversamplingLabel, slopeLabel, rangeLabel, analyzerLabel, fftLabel, windowLabel, overlapLabel, analyzerChannelLabel;
    fancy::ui::ParamDropdown phaseDropdown, firDropdown, oversamplingDropdown, rangeDropdown, analyzerDropdown, fftDropdown, windowDropdown, overlapDropdown, analyzerChannelDropdown;
    std::vector<std::unique_ptr<fancy::ui::ParamDropdown>> slopeDropdowns;
    fancy::ui::ParamCheckbox tiltCheckbox, peakCheckbox;
    fancy::ui::Knob smoothingKnob;

    bool compactMode = false;
    float lastRange = -1.0f;
    int selectedBand = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqEditor)
};
