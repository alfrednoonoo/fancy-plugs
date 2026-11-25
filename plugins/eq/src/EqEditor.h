#pragma once

#include "EqGraph.h"
#include "EqProcessor.h"

#include <fancy/ui/ShellEditor.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

// A parameter's value as a small field with a label, for a row with no room for a knob (Phase 4
// Wave 1: EQ's Reach). It drags, scrolls and types as a knob's value does.
class ValueField final : public fancy::ui::ParamControl
{
public:
    // With no label (Phase 4 Wave 2: the band list), the value alone, with no box behind it.
    ValueField (juce::RangedAudioParameter& parameter, const juce::String& label, juce::Colour colour);

    void paint (juce::Graphics&) override;
    juce::Colour getAccentColour() const override { return colour; }

    bool showDash = false; // bare only: a dash in place of the value

protected:
    juce::Rectangle<int> getValueArea() const override;

private:
    juce::String labelText;
    juce::Colour colour;
};

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

    // Phase 4 Wave 2: band 1 a Brickwall high-pass, selected, with the drawer open and Character Warm.
    void showBrickwallPreview();

    // Panel 3 of the Q2.4 mockup: the Match card open over a learned reference and current.
    void showMatchPreview();

    // Phase 4 Wave 2 item 46, mockup 66 panel 1: the band list, bands 2 and 3 in group A; and
    // panel 2, the same in Bands, with the group tags.
    void showListPreview();
    void showGroupsPreview();

    // v1.7.2, mockup 68: the list with twelve bands, for the largest window.
    void showLargeListPreview();

    // v1.7.2 (docs/eq.md, "the band list in a large window"): the list's height for a body of this
    // height, and where a column drawn at this x in the default window goes. Static for tests.
    static int listHeight (int bodyHeight, int bandsUsed);
    static int listColumnX (int x, int contentWidth);
    static juce::Range<int> listSpan (int contentWidth);

    int getGraphHeight() const { return graph.getHeight(); }
    int getListHeight() const;

    // Opens or closes the Match card. While it's open the processor's Match engine runs.
    void setMatchOpen (bool shouldBeOpen);
    bool isMatchOpen() const noexcept { return matchOpen; }

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;

        bool compact = false;
        int stripHeight = 0;
    };

    class TabStrip final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void (int)> onSelect;
        std::vector<int> bands;
        std::vector<int> groups; // Phase 4 Wave 2 item 46: each tab's group, 0 for none
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
            bool listStyle = false; // Phase 4 Wave 2: the band list's small menu box, no icon

        private:
            void clicked() override;

            juce::RangedAudioParameter& slopeParameter;
        };

        // Phase 3 Q2.4: which channel the band filters. Reads ST, L, R, M or S (in fancy-core
        // since Q2.6, shared with Fancy MultiComp).
        using ChannelChip = fancy::ui::ChannelChip;

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
        ValueField reachField; // Phase 4 Wave 1
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

    // Phase 4 Wave 2 item 46 (docs/eq.md, "band list and groups"): moves a group with the band
    // being edited. Only edits made in this window start gestures, never a host, so automation
    // moves its own band only (Decision 1 A). At an edit's start it takes every member's value
    // and opens a gesture on each, so automation records them and undo makes one step; each
    // change then moves them from those values (eq::groups), and the edit's end closes them.
    class GroupLink final : private juce::AudioProcessorParameter::Listener
    {
    public:
        explicit GroupLink (EqProcessor& processor);
        ~GroupLink() override;

    private:
        enum Suffix { freq, gain, q };

        struct Session
        {
            int band = 0, suffix = freq;
            std::vector<int> members;   // the other bands of the group
            std::vector<double> starts; // the edited band's value, then each member's
        };

        void parameterValueChanged (int parameterIndex, float newValue) override;
        void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;

        juce::RangedAudioParameter& parameter (int band, int suffix);
        int groupOf (int band);

        EqProcessor& eqProcessor;
        std::unordered_map<int, int> watched; // parameter index to band * 3 + suffix
        std::map<int, Session> sessions;      // by the edited parameter's index
        bool applying = false;
    };

    // Phase 4 Wave 2 item 46: every band in use as a row of a table, in the band strip's place
    // (mockup 66 panel 1). Rows for all 24 bands are made once and shown as bands come into use.
    class BandList final : public juce::Component
    {
    public:
        BandList (EqProcessor& processor, EqEditor& owner);
        ~BandList() override;

        // 30 Hz, from the editor: which rows show, and the selection.
        void update();

        void paint (juce::Graphics&) override;
        void resized() override;

        static constexpr int rowHeight = 22, headerY = 29, rowsTop = 44;

    private:
        class Row;

        class AddRow final : public juce::Component
        {
        public:
            void paint (juce::Graphics&) override;
            void mouseUp (const juce::MouseEvent&) override;

            std::function<void()> onClick;
        };

        EqProcessor& eqProcessor;
        EqEditor& owner;
        juce::Component content; // declared first, so the viewport lets go of it before it goes
        juce::Viewport viewport;
        std::vector<std::unique_ptr<Row>> rows;
        AddRow addRow;
        std::vector<int> shown;
        int shownSelected = -1;
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
    int wantedStripHeight();
    void applyPreviewValues();

    // Phase 4 Wave 2 item 46: Bands or List, and opening a band in Bands (from the list).
    bool isListShown();
    void openInBands (int band);

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

    // Phase 4 Wave 1: Auto gain in the drawer, and its trim under the Gain fader.
    fancy::ui::ParamCheckbox autoGainCheckbox;
    juce::Label autoGainLabel;

    // Phase 4 Wave 2 (docs/eq.md): the Character output stage, under Auto gain.
    fancy::ui::ParamDropdown characterDropdown;
    juce::Label characterLabel;

    // Phase 4 Wave 2 item 46: the Bands | List switch, the list and the group link.
    fancy::ui::ParamSegments viewSwitch;
    std::unique_ptr<BandList> bandList;
    std::unique_ptr<GroupLink> groupLink;
    bool listShown = false;

    int stripShown = 0; // the band strip's height as laid out
    bool compactMode = false;
    float lastRange = -1.0f;
    int selectedBand = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqEditor)
};
