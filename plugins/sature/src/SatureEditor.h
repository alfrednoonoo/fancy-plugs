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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SatureEditor)
};
