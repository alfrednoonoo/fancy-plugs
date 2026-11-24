#pragma once

#include "SatureProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <array>
#include <atomic>

class SatureEditor final : public fancy::ui::ShellEditor
{
public:
    explicit SatureEditor (SatureProcessor& processor);
    ~SatureEditor() override;

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: the mockup's values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

    // Phase 4 Wave 2: the drawer's pages, Shape and Tone and stereo (mockup 61).
    enum DrawerPage { shapePage, toneStereoPage };
    void showDrawerPage (int page);

    // For rendered screenshots, after showPreview (false) and in this order, as each keeps the
    // settings of the one before (mockup 61): the Shape page with Offset at 0.10 (panel 2), the
    // Tone and stereo page at the mockup's values (panel 3), and Fold chosen with the drawer shut
    // (panel 1).
    void showShapePagePreview();
    void showToneStereoPagePreview();
    void showFoldPreview();

    // What the harmonics card shows: H1 to H10 relative to H1, in dB, and THD from H2 to H10.
    struct Harmonics
    {
        std::array<float, 10> levelsDb {};
        float thdPercent = 0.0f;
        bool outsideBand = false;
    };

    // Renders a 1 kHz tone at -12 dBFS through a copy of the saturated path at these settings
    // (wet only, 48 kHz, 4x) and reads its harmonics. Any thread; allocates.
    static Harmonics analyse (SatureDsp& dsp, SatureDsp::Targets targets);

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;

        // Phase 4 Wave 2: names the driven outer bands on the BAND card, which tints while any is.
        void setOuterText (const juce::String& text);

    private:
        juce::String outerText;
    };

    // Input to output of the current Type, Drive and Character.
    class TransferCurve final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit TransferCurve (SatureProcessor& processor);
        void update();
        void paint (juce::Graphics&) override;

    private:
        SatureProcessor& satureProcessor;
        int lastType = -1;
        double lastDrive = -1.0, lastCharacter = -2.0;
    };

    // The harmonics the settings add to a 1 kHz tone, worked out on a background thread.
    class HarmonicsCard final : public juce::Component, public juce::SettableTooltipClient, private juce::Thread
    {
    public:
        explicit HarmonicsCard (SatureProcessor& processor);
        ~HarmonicsCard() override;

        void update();
        void show (const Harmonics& harmonics);
        void paint (juce::Graphics&) override;

    private:
        void run() override;
        static bool sameSound (const SatureDsp::Targets& a, const SatureDsp::Targets& b);

        SatureProcessor& satureProcessor;
        SatureDsp::Targets requested, shownFor;
        bool hasRequest = false, frozen = false;

        juce::SpinLock lock;
        SatureDsp::Targets pending;
        Harmonics result, shown;
        std::atomic<int> resultVersion { 0 };
        int shownVersion = 0;
    };

    void onTick (float seconds) override;
    void updateOuter();

    SatureProcessor& satureProcessor() noexcept;

    Panels panels;
    juce::Label typeLabel;
    fancy::ui::ParamSegments typeSegments;

    TransferCurve transferCurve;
    HarmonicsCard harmonicsCard;

    fancy::ui::Knob driveKnob, characterKnob, toneKnob, lowKnob, highKnob;

    fancy::ui::ParamCheckbox autoGainCheckbox;
    fancy::ui::ListenCheckbox listenCheckbox;
    fancy::ui::EditLink editLink;

    fancy::ui::OutMeter outMeter;
    juce::Label wetLabel, dryLabel;
    fancy::ui::Fader wetFader, dryFader;

    fancy::ui::EditDrawer drawer;
    juce::Label oversamplingLabel, filterLabel, aliasNote;
    fancy::ui::ParamDropdown oversamplingDropdown, filterDropdown;
    fancy::ui::Knob trimKnob;
    fancy::ui::ParamCheckbox dcCheckbox;

    // Phase 4 Wave 2 (reference/phase4/45__sature_w2_mockup.png): the OUTER BANDS group.
    juce::Label outerCaption, belowLowLabel, aboveHighLabel, outerNote;
    fancy::ui::Knob lowDriveKnob, lowMixKnob, highDriveKnob, highMixKnob;

    // Phase 4 Wave 2, items 35 to 39 (reference/phase4/61__sature_w2b_mockup.png): the page switch,
    // Offset on Shape, and the cuts, Spread and Channels on Tone and stereo.
    fancy::ui::ViewTabs drawerTabs { juce::StringArray { "Shape", "Tone and stereo" } };
    fancy::ui::Knob offsetKnob;
    juce::Label offsetNote;
    juce::Label lowCutTitle, highCutTitle, lowSlopeLabel, highSlopeLabel, cutNote, stereoTitle, spreadNote, channelsLabel, channelsNote;
    fancy::ui::Knob lowCutKnob, highCutKnob, spreadKnob;
    fancy::ui::ParamDropdown lowSlopeDropdown, highSlopeDropdown, channelsDropdown;
    fancy::ui::ParamSegments lowPosSegments, highPosSegments;
    std::vector<juce::Component*> shapeItems, toneStereoItems;
    int drawerPage = shapePage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SatureEditor)
};
